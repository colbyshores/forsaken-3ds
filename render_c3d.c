/*
 * render_c3d.c — Native citro3d renderer for Nintendo 3DS
 *
 * Replaces the picaGL (GL1 immediate mode) path with direct citro3d
 * calls, enabling:
 *   - GPU-side modelview+projection transforms via PICA200 vertex shader
 *   - Cached vertex/index buffers in linear VRAM (linearAlloc)
 *   - Single-pass stereo (same geometry, swap projection uniform)
 *
 * Build:
 *   make -f Makefile.3ds RENDERER=citro3d   # this renderer
 *   make -f Makefile.3ds                    # picaGL fallback (GL=1)
 *
 * Architecture:
 *   - picaGL is NOT linked. We provide our own pglInit/pglSwapBuffers/pglExit
 *     stubs that use citro3d directly for GPU init and frame presentation.
 *   - pglInit() calls C3D_Init(). c3d_renderer_init() creates the render
 *     target, shader, and scratch buffer.
 *   - FSBeginScene() calls C3D_FrameBegin + C3D_FrameDrawOn + binds shader
 *     + initializes GPU state (AttrInfo, TexEnv, depth, cull, uniforms).
 *   - draw_render_object() converts LVERTEX/TLVERTEX to gpu_vertex_t (36 bytes,
 *     all floats) in a linearAlloc scratch buffer, then C3D_DrawArrays.
 *   - pglSwapBuffers() calls C3D_FrameEnd which triggers the auto display
 *     transfer via C3D_RenderTargetSetOutput. DO NOT call picaGL's real
 *     pglSwapBuffers — its GX_DisplayTransfer conflicts with citro3d's
 *     auto-transfer and produces a black screen.
 *
 * Key lessons learned during development:
 *   1. Mandarine emulator DOES support citro3d render targets + auto-transfer.
 *      The earlier black screen was caused by pglSwapBuffers conflicting with
 *      C3D_FrameEnd's display transfer, NOT emulator limitations.
 *   2. GPU state (AttrInfo, BufInfo, TexEnv, DepthTest, CullFace) must be
 *      explicitly initialized in FSBeginScene. Uninitialized GPU registers
 *      cause invisible geometry (no crash, just no output).
 *   3. The shader has two uniform matrix slots: projection (reg 0-3) and
 *      modelView (reg 4-7). upload_matrices() writes combined MVP to
 *      projection and FSBeginScene writes identity to modelView. Both
 *      must be initialized or the shader reads garbage.
 *   4. Engine matrices are column-major (despite [4][4] declaration).
 *      matrix_to_c3d() transposes using the picaGL glLoadMatrixf pattern:
 *      dst->r[i].x = m[0+i], .y = m[4+i], .z = m[8+i], .w = m[12+i].
 *   5. Textures use GPU_RGBA4 (16-bit) to avoid VRAM exhaustion. GPU_RGBA8
 *      runs out after ~7 1024x1024 textures.
 *   6. Vertex colors are packed u32 ARGB in LVERTEX but the shader needs
 *      normalized floats. gpu_vertex_t stores pre-normalized float[4] colors.
 */

#ifdef RENDERER_C3D

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "main.h"
#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "networking.h"
#include "render.h"
#include "texture.h"
#include "file.h"
#include "main_3ds.h"
#include "lights.h"
#include "platform.h"

/* ---- shader binary (compiled from shaders/render_c3d.v.pica) ---- */
#include "render_c3d_shbin.h"

/* ---- uniform register locations (must match .v.pica) ---- */
#define UNIFORM_PROJECTION  0
#define UNIFORM_MODELVIEW   4

/* ---- citro3d state ---- */

static DVLB_s          *s_shaderDVLB    = NULL;
static shaderProgram_s  s_shaderProgram;
static bool             s_shaderReady   = false;
static C3D_Mtx          s_projection;
static C3D_Mtx          s_modelview;

/* Render targets for stereo */
static C3D_RenderTarget *s_targetLeft  = NULL;
static C3D_RenderTarget *s_targetRight = NULL;
static C3D_RenderTarget *s_targetBot   = NULL;

/* ---- GPU vertex format (all floats, matches shader inputs) ---- */
typedef struct {
	float pos[3];
	float color[4];
	float texcoord[2];
} gpu_vertex_t;  /* 36 bytes */

/* Scratch buffer for vertex conversion (256KB = ~7000 verts) */
#define GPU_SCRATCH_SIZE  (256 * 1024)
static gpu_vertex_t *s_scratch = NULL;
static int s_scratchUsed = 0;
static int s_scratchMax = 0;

/* ---- vertex attribute layout ---- */
static C3D_AttrInfo s_attrInfo3D;

/* ---- globals expected by the engine ---- */

render_info_t render_info;

typedef struct { float anisotropic; } gl_caps_t;
gl_caps_t caps;

MATRIX proj_matrix;
MATRIX view_matrix;
MATRIX world_matrix;

/* Texture handle — wraps a C3D_Tex for the engine's LPTEXTURE (void*) */
typedef struct {
	C3D_Tex tex;
	bool    initialized;
} texture_t;

/* debug trace to SD card */
static void c3d_trace(const char *msg)
{
	FILE *f = fopen("sdmc:/forsaken_c3d.log", "a");
	if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

/* bSquareOnly — shared global, always true on 3DS */
bool bSquareOnly = true;

/* Additive blend flag (matches GL1 behavior) */
bool _additive_blend_active = false;

/* Frame state tracking */
static bool s_inFrame = false;


/* picaGL is NOT linked for the citro3d renderer. We provide our own
 * pglInit/pglExit/pglSwapBuffers/pglTransferEye implementations. */
void pglInit(void)
{
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	{
		FILE *f = fopen("sdmc:/forsaken_c3d.log", "w");
		if (f) { fputs("pglInit: C3D_Init done\n", f); fclose(f); }
	}
}

void pglExit(void)
{
	c3d_renderer_cleanup();
	C3D_Fini();
}

void pglSwapBuffers(void)
{
	/* End the citro3d frame if one is active.
	 * C3D_FrameEnd triggers the auto display transfer via
	 * C3D_RenderTargetSetOutput. If no frame was started
	 * (e.g. during loading screens before FSBeginScene is called),
	 * this is safely a no-op. */
	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}
}

void pglTransferEye(unsigned int eye) { (void)eye; }

/* Gamma table for texture loading */
u_int8_t gamma_table[256];

/*===================================================================
	Gamma table (shared with texture loader)
===================================================================*/

void build_gamma_table(double gamma)
{
	int i;
	double k;
	if (gamma <= 0.0) gamma = 1.0;
	k = 255.0 / pow(255.0, 1.0 / gamma);
	for (i = 0; i < 256; i++)
	{
		gamma_table[i] = (u_int8_t)(k * (pow((double)i, 1.0 / gamma)));
		if (gamma_table[i] > 255)
			gamma_table[i] = 255;
		if (gamma_table[i] < 1)
			gamma_table[i] = 1;
	}
}

/*===================================================================
	Shader init / cleanup
===================================================================*/

bool c3d_renderer_init(void)
{
	c3d_trace("c3d_renderer_init: start");
	s_shaderDVLB = DVLB_ParseFile((u32*)render_c3d_shbin, render_c3d_shbin_len);
	if (!s_shaderDVLB)
	{
		c3d_trace("c3d_renderer_init: DVLB_ParseFile FAILED");
		return false;
	}
	c3d_trace("c3d_renderer_init: shader parsed");

	shaderProgramInit(&s_shaderProgram);
	if (shaderProgramSetVsh(&s_shaderProgram, &s_shaderDVLB->DVLE[0]) < 0)
		return false;

	/* GPU vertex layout: pos(3f) + color(4f) + texcoord(2f) = 36 bytes
	 * We convert LVERTEX/TLVERTEX → gpu_vertex_t on CPU before drawing */
	AttrInfo_Init(&s_attrInfo3D);
	AttrInfo_AddLoader(&s_attrInfo3D, 0, GPU_FLOAT, 3);  /* v0: position */
	AttrInfo_AddLoader(&s_attrInfo3D, 1, GPU_FLOAT, 4);  /* v1: color (normalized) */
	AttrInfo_AddLoader(&s_attrInfo3D, 2, GPU_FLOAT, 2);  /* v2: texcoord */

	/* Allocate scratch buffer for vertex conversion */
	s_scratch = (gpu_vertex_t*)linearAlloc(GPU_SCRATCH_SIZE);
	s_scratchMax = GPU_SCRATCH_SIZE / sizeof(gpu_vertex_t);
	s_scratchUsed = 0;

	/* Create render target and link to display output */
	s_targetLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	if (!s_targetLeft) {
		c3d_trace("FAILED to create render target");
		return false;
	}
	C3D_RenderTargetSetOutput(s_targetLeft, GFX_TOP, GFX_LEFT,
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
		GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
	c3d_trace("render target created and linked to display");

	Mtx_Identity(&s_projection);
	Mtx_Identity(&s_modelview);

	/* Match picaGL depth map: scale=1.0, offset=1.0 */
	C3D_DepthMap(true, 1.0f, 1.0f);

	s_shaderReady = true;
	c3d_trace("c3d_renderer_init: OK — shader ready, render targets created");
	return true;
}

void c3d_renderer_cleanup(void)
{
	if (s_shaderReady)
	{
		shaderProgramFree(&s_shaderProgram);
		s_shaderReady = false;
	}
	if (s_shaderDVLB)
	{
		DVLB_Free(s_shaderDVLB);
		s_shaderDVLB = NULL;
	}
}

/*===================================================================
	Matrix conversion: engine RENDERMATRIX → C3D_Mtx
===================================================================*/

static void matrix_to_c3d(const RENDERMATRIX *src, C3D_Mtx *dst)
{
	/* Engine matrices are column-major in memory (see render_gl_shared.c
	 * line 929-937). The raw float[16] IS the GL column-major data.
	 * Load it the same way picaGL's glLoadMatrixf does:
	 *   row[i].x = m[0+i]   (column 0, row i)
	 *   row[i].y = m[4+i]   (column 1, row i)
	 *   row[i].z = m[8+i]   (column 2, row i)
	 *   row[i].w = m[12+i]  (column 3, row i)
	 * C3D_FVec x/y/z/w accessors handle the WZYX storage internally. */
	const float *m = (const float *)src;
	int i;
	for (i = 0; i < 4; i++)
	{
		dst->r[i].x = m[0 + i];
		dst->r[i].y = m[4 + i];
		dst->r[i].z = m[8 + i];
		dst->r[i].w = m[12 + i];
	}
}

static void upload_matrices(void)
{
	/* Compute combined MVP on CPU, apply PICA fix, upload to uniform 0
	 * (picaGL's projection slot). picaGL's shader does: outpos = projection * pos.
	 * We pre-combine world*view*proj so the shader just needs one multiply. */
	MATRIX mvp;
	C3D_Mtx c3d_mvp;

	MatrixMultiply(&world_matrix, &view_matrix, &mvp);
	MatrixMultiply(&mvp, &proj_matrix, &mvp);

	matrix_to_c3d((const RENDERMATRIX*)&mvp, &c3d_mvp);

	/* PICA depth remap: D3D [0,1] → PICA [-1,0]
	 * z_pica = z_d3d - w, in matrix form: row[2] -= row[3] */
	c3d_mvp.r[2].x -= c3d_mvp.r[3].x;
	c3d_mvp.r[2].y -= c3d_mvp.r[3].y;
	c3d_mvp.r[2].z -= c3d_mvp.r[3].z;
	c3d_mvp.r[2].w -= c3d_mvp.r[3].w;

	/* 90° screen rotation: swap rows 0/1, negate new row 1 */
	{
		C3D_FVec row0 = c3d_mvp.r[0];
		c3d_mvp.r[0] = c3d_mvp.r[1];
		c3d_mvp.r[1].x = -row0.x;
		c3d_mvp.r[1].y = -row0.y;
		c3d_mvp.r[1].z = -row0.z;
		c3d_mvp.r[1].w = -row0.w;
	}

	/* Upload to projection uniform (register 0) */
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, 0, &c3d_mvp);
}

/*===================================================================
	Render init / cleanup / flip
===================================================================*/

static void detect_caps(void)
{
	caps.anisotropic = 0.0f;
}

bool render_init(render_info_t *info)
{
	detect_caps();
	build_gamma_table(1.0);
	/* C3D_Init already called by pglInit() stub */
	if (!c3d_renderer_init())
		return false;
	info->ok_to_render = true;
	return true;
}

void render_cleanup(render_info_t *info)
{
	/* Cleanup handled by pglExit() stub */
	info->ok_to_render = false;
}

bool render_mode_select(render_info_t *info)
{
	render_cleanup(info);
	if (!platform_init_video())
		return false;
	return render_init(info);
}

bool render_reset(render_info_t *info)
{
	(void)info;
	return false;
}

void render_set_filter(bool red, bool green, bool blue)
{
	/* citro3d color mask for anaglyph stereo */
	u8 mask = 0;
	if (red)   mask |= GPU_WRITE_RED;
	if (green) mask |= GPU_WRITE_GREEN;
	if (blue)  mask |= GPU_WRITE_BLUE;
	mask |= GPU_WRITE_ALPHA;
	C3D_ColorLogicOp(GPU_LOGICOP_COPY);
	/* TODO: implement proper per-channel write mask via C3D_FrameBuf */
}

bool render_flip(render_info_t *info)
{
	/* platform_render_present calls our pglSwapBuffers which calls
	 * C3D_FrameEnd if a frame is active */
	platform_render_present(info);
	return true;
}

const char *render_error_description(int e)
{
	(void)e;
	return NULL;
}

/*===================================================================
	Render mode
===================================================================*/

void render_mode_wireframe(void) { /* no wireframe on PICA200 */ }
void render_mode_points(void) { /* no point mode on PICA200 */ }
void render_mode_fill(void) { /* always fill on PICA200 */ }

/*===================================================================
	Scene begin / end
===================================================================*/


bool FSBeginScene(void)
{
	s_scratchUsed = 0;
	if (!s_inFrame)
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetLeft);
		C3D_BindProgram(&s_shaderProgram);

		/* Initialize GPU state for the frame — without this, the first
		 * draw call operates on uninitialized GPU registers */
		{
			C3D_AttrInfo *ai = C3D_GetAttrInfo();
			AttrInfo_Init(ai);
			AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);  /* v0: pos */
			AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);  /* v1: color */
			AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);  /* v2: texcoord */
		}
		{
			C3D_TexEnv *env = C3D_GetTexEnv(0);
			C3D_TexEnvInit(env);
			C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
		}
		C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
		C3D_DepthMap(true, 1.0f, 1.0f);

		/* Initialize modelView uniform (register 4) to identity.
		 * upload_matrices writes combined MVP to register 0 (projection),
		 * so the shader's modelView * pos must be identity to avoid
		 * double-transforming the already-combined MVP result. */
		{
			C3D_Mtx identity;
			Mtx_Identity(&identity);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
		}

		s_inFrame = true;
	}
	return true;
}

bool FSEndScene(void) { return true; }

/*===================================================================
	GPU state management
===================================================================*/

void reset_trans(void)
{
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	_additive_blend_active = false;
}

void reset_zbuff(void)
{
	C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);
}

void disable_zbuff_write(void)
{
	C3D_DepthTest(true, GPU_LESS, GPU_WRITE_COLOR);
}

void disable_zbuff(void)
{
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
}

void cull_none(void)
{
	C3D_CullFace(GPU_CULL_NONE);
}

void cull_cw(void)
{
	/* Game uses CW front faces (D3D convention).
	 * PICA200 has CCW-front only. CW = "our front" = PICA200's back.
	 * Culling CW (game's request) means cull PICA200's back faces. */
	C3D_CullFace(GPU_CULL_BACK_CCW);
}

void reset_cull(void)
{
	/* Default: cull back faces. Game's CW-front = PICA200's CCW-back.
	 * So cull PICA200's front (= game's back). */
	C3D_CullFace(GPU_CULL_FRONT_CCW);
}

void set_normal_states(void)
{
	reset_zbuff();
	reset_trans();
	_additive_blend_active = false;
}

void set_alpha_states(void)
{
	disable_zbuff_write();
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
	_additive_blend_active = true;
}

void set_whiteout_state(void)
{
	disable_zbuff_write();
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
}

/*===================================================================
	Clear
===================================================================*/


bool FSClear(XYRECT *rect)
{
	(void)rect;
	if (s_targetLeft)
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	return true;
}

bool FSClearBlack(void)
{
	if (s_targetLeft)
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_COLOR, 0x000000FF, 0);
	return true;
}

bool FSClearDepth(XYRECT *rect)
{
	(void)rect;
	if (s_targetLeft)
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_DEPTH, 0, 0xFFFFFF);
	return true;
}

/*===================================================================
	Viewport
===================================================================*/

static render_viewport_t s_viewport = { 0, 0, 400, 240, 1.0f, 1.0f, 0.0f, 1.0f };

bool FSGetViewPort(render_viewport_t *view)
{
	*view = s_viewport;
	return true;
}

bool FSSetViewPort(render_viewport_t *view)
{
	s_viewport = *view;

	/* Map game viewport (landscape, top-left origin) to PICA200 viewport
	 * (portrait framebuffer). Must match picaGL's glViewport → _picaViewport
	 * chain exactly:
	 *   GL: bottom = screenH - Y - Height; glViewport(X, bottom, W, H)
	 *   picaGL: viewportX = (screenW - W) - X; viewportY = bottom
	 *   PICA: _picaViewport(viewportY, viewportX, H, W)
	 * So: PICA x = 240 - Y - H, y = (400 - W) - X, w = H, h = W */
	{
		int bottom = (int)render_info.ThisMode.h - (int)(view->Y + view->Height);
		int gl_x = (int)view->X;
		/* picaGL transform: viewportX = (400 - W) - X, viewportY = bottom */
		int pica_x = bottom;
		int pica_y = (400 - (int)view->Width) - gl_x;
		int pica_w = (int)view->Height;
		int pica_h = (int)view->Width;

		if (pica_x < 0) pica_x = 0;
		if (pica_y < 0) pica_y = 0;

		C3D_SetViewport((u32)pica_x, (u32)pica_y, (u32)pica_w, (u32)pica_h);
	}

	return true;
}

/*===================================================================
	Matrix state
===================================================================*/

static void reset_modelview(void)
{
	MATRIX mv;
	MatrixMultiply(&world_matrix, &view_matrix, &mv);
	matrix_to_c3d((const RENDERMATRIX*)&mv, &s_modelview);
}

bool FSSetView(RENDERMATRIX *matrix)
{
	memmove(&view_matrix, &matrix->m, sizeof(view_matrix));
	reset_modelview();
	return true;
}

bool FSSetWorld(RENDERMATRIX *matrix)
{
	memmove(&world_matrix, &matrix->m, sizeof(world_matrix));
	reset_modelview();
	return true;
}

bool FSGetWorld(RENDERMATRIX *matrix)
{
	memmove(&matrix->m, &world_matrix, sizeof(matrix->m));
	return true;
}

bool FSSetProjection(RENDERMATRIX *matrix)
{
	/* Just store the raw projection matrix — PICA fix is applied in
	 * upload_matrices() to the combined MVP, matching render_3ds.c */
	memmove(&proj_matrix, &matrix->m, sizeof(proj_matrix));
	return true;
}

/*===================================================================
	Vertex / Index buffer management
	Phase 2: use linearAlloc for GPU-visible memory
===================================================================*/

bool FSCreateVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = linearAlloc(numVertices * sizeof(LVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSCreateDynamicVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	return FSCreateVertexBuffer(renderObject, numVertices);
}

bool FSCreateNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	renderObject->lpNormalBuffer = linearAlloc(numNormals * sizeof(NORMAL));
	return renderObject->lpNormalBuffer != NULL;
}

bool FSCreateDynamicNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	return FSCreateNormalBuffer(renderObject, numNormals);
}

bool FSCreateIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	renderObject->lpIndexBuffer = linearAlloc(numIndices * 3 * sizeof(WORD));
	return renderObject->lpIndexBuffer != NULL;
}

bool FSCreateDynamicIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	return FSCreateIndexBuffer(renderObject, numIndices);
}

bool FSLockIndexBuffer(RENDEROBJECT *renderObject, WORD **indices)
{
	*indices = renderObject->lpIndexBuffer;
	return true;
}

bool FSLockVertexBuffer(RENDEROBJECT *renderObject, LVERTEX **verts)
{
	*verts = renderObject->lpVertexBuffer;
	return true;
}

bool FSUnlockIndexBuffer(RENDEROBJECT *renderObject)  { (void)renderObject; return true; }
bool FSUnlockVertexBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSLockNormalBuffer(RENDEROBJECT *renderObject, NORMAL **normals)
{
	*normals = renderObject->lpNormalBuffer;
	return true;
}

bool FSUnlockNormalBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = linearAlloc(numVertices * sizeof(TLVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSLockPretransformedVertexBuffer(RENDEROBJECT *renderObject, TLVERTEX **verts)
{
	*verts = (TLVERTEX*)renderObject->lpVertexBuffer;
	return true;
}

void FSReleaseRenderObject(RENDEROBJECT *renderObject)
{
	int i;
	if (renderObject->lpVertexBuffer)
	{
		linearFree(renderObject->lpVertexBuffer);
		renderObject->lpVertexBuffer = NULL;
	}
	if (renderObject->lpNormalBuffer)
	{
		linearFree(renderObject->lpNormalBuffer);
		renderObject->lpNormalBuffer = NULL;
	}
	if (renderObject->lpIndexBuffer)
	{
		linearFree(renderObject->lpIndexBuffer);
		renderObject->lpIndexBuffer = NULL;
	}
	for (i = 0; i < renderObject->numTextureGroups; i++)
		renderObject->textureGroups[i].texture = NULL;
	renderObject->numTextureGroups = 0;
}

/*===================================================================
	Texture management
===================================================================*/

void release_texture(LPTEXTURE texture)
{
	if (!texture) return;
	texture_t *texdata = (texture_t*)texture;
	if (texdata->initialized)
		C3D_TexDelete(&texdata->tex);
	free(texdata);
}

/*===================================================================
	Morton tiling for PICA200 textures
	PICA200 requires textures in 8x8-block Morton/Z-order layout.
	Input: linear RGBA8 (R,G,B,A bytes). Output: tiled ABGR8 (A,B,G,R).
===================================================================*/

static inline u32 _mortonInterleave(u32 x, u32 y)
{
	static const u32 xlut[] = {0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15};
	static const u32 ylut[] = {0x00,0x02,0x08,0x0a,0x20,0x22,0x28,0x2a};
	return xlut[x & 7] + ylut[y & 7];
}

static void tile_rgba4(const u_int8_t *src, u_int16_t *dst, int w, int h)
{
	int x, y;
	for (y = 0; y < h; y++)
	{
		int out_y = h - 1 - y;  /* flip Y for PICA200 */
		u32 coarse_y = out_y & ~7;
		for (x = 0; x < w; x++)
		{
			u32 morton = _mortonInterleave(x, out_y);
			u32 offset = morton + coarse_y * w + (x & ~7) * 8;
			int si = (y * w + x) * 4;
			/* RGBA8 → RGBA4444 packed u16 for GPU_RGBA4 */
			u_int8_t r = src[si + 0] >> 4;
			u_int8_t g = src[si + 1] >> 4;
			u_int8_t b = src[si + 2] >> 4;
			u_int8_t a = src[si + 3] >> 4;
			dst[offset] = (r << 12) | (g << 8) | (b << 4) | a;
		}
	}
}

static bool create_texture(LPTEXTURE *t, const char *path,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	texture_image_t image;

	Change_Ext(path, image.path, ".PNG");
	if (!File_Exists((char*)image.path))
	{
		DebugPrintf("Could not find texture file: %s\n", path);
		return true;
	}

	if (load_image(&image, numMips) != 0)
		return false;

	/* PICA200 hard limit: 1024x1024 */
	while (image.w > 1024 || image.h > 1024)
	{
		int nw = image.w / 2, nh = image.h / 2;
		int x, y, c;
		char *nd = malloc(nw * nh * 4);
		if (!nd) break;
		for (y = 0; y < nh; y++)
			for (x = 0; x < nw; x++)
			{
				int si = (y * 2 * image.w + x * 2) * 4;
				for (c = 0; c < 4; c++)
					nd[(y * nw + x) * 4 + c] = (
						(u_int8_t)image.data[si + c] +
						(u_int8_t)image.data[si + 4 + c] +
						(u_int8_t)image.data[si + image.w * 4 + c] +
						(u_int8_t)image.data[si + image.w * 4 + 4 + c]
					) / 4;
			}
		free(image.data);
		image.data = nd;
		image.w = nw;
		image.h = nh;
	}

	*width = (u_int16_t)image.w;
	*height = (u_int16_t)image.h;
	*colorkey = (bool)image.colorkey;

	/* Gamma correction + colour key */
	{
		int y, x;
		int pitch = 4 * image.w;
		for (y = 0; y < image.h; y++)
			for (x = 0; x < image.w; x++)
			{
				DWORD idx = y * pitch + x * 4;
				image.data[idx]   = (char)gamma_table[(u_int8_t)image.data[idx]];
				image.data[idx+1] = (char)gamma_table[(u_int8_t)image.data[idx+1]];
				image.data[idx+2] = (char)gamma_table[(u_int8_t)image.data[idx+2]];
				image.data[idx+3] = (char)gamma_table[(u_int8_t)image.data[idx+3]];
				if ((image.data[idx] + image.data[idx+1] + image.data[idx+2]) == 0)
					image.data[idx+3] = 0;
			}
	}

	{
		texture_t *texdata;
		u_int8_t *tiled;
		bool is_new = (*t == NULL);

		if (is_new)
		{
			texdata = malloc(sizeof(texture_t));
			if (!texdata) { destroy_image(&image); return false; }
			memset(texdata, 0, sizeof(texture_t));
		}
		else
		{
			texdata = (texture_t*)*t;
			if (texdata->initialized)
			{
				C3D_TexDelete(&texdata->tex);
				texdata->initialized = false;
			}
		}

		{
			char buf[128];
			snprintf(buf, sizeof(buf), "create_texture: C3D_TexInit %dx%d", image.w, image.h);
			c3d_trace(buf);
		}
		if (!C3D_TexInit(&texdata->tex, image.w, image.h, GPU_RGBA4))
		{
			c3d_trace("create_texture: C3D_TexInit FAILED (VRAM full?)");
			texdata->initialized = false;
			*t = (LPTEXTURE)texdata;
			destroy_image(&image);
			return true; /* non-fatal — draw path falls back to vertex color */
		}

		/* Morton-tile RGBA8 → RGBA4444 into the texture's linear buffer */
		tiled = linearAlloc(image.w * image.h * 2); /* 2 bytes per pixel for RGBA4 */
		if (tiled)
		{
			tile_rgba4((u_int8_t*)image.data, (u_int16_t*)tiled, image.w, image.h);
			C3D_TexUpload(&texdata->tex, tiled);
			linearFree(tiled);
		}
		else
		{
			c3d_trace("create_texture: linearAlloc for tiled data FAILED");
		}

		C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);
		C3D_TexSetWrap(&texdata->tex, GPU_REPEAT, GPU_REPEAT);

		texdata->initialized = true;
		*t = (LPTEXTURE)texdata;
	}

	DebugPrintf("Created texture: file=%s, width=%d, height=%d, colorkey=%s\n",
		image.path, image.w, image.h, image.colorkey ? "true" : "false");

	destroy_image(&image);
	return true;
}

bool FSCreateTexture(LPTEXTURE *texture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colourkey)
{
	return create_texture(texture, fileName, width, height, numMips, colourkey);
}

bool update_texture_from_file(LPTEXTURE dstTexture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	create_texture(&dstTexture, fileName, width, height, numMips, colorkey);
	return true;
}

/*===================================================================
	Lighting (Phase 2: CPU-side same as GL1)
===================================================================*/

void render_reset_lighting_variables(void)
{
	/* Reset per-frame light list — implementation shared with GL1 */
}

void do_water_effect(VECTOR *pos, COLOR *color)
{
	/* TODO: port from render_gl1.c */
	(void)pos; (void)color;
}

void do_whiteout_effect(VECTOR *pos, COLOR *color)
{
	/* TODO: port from render_gl1.c */
	(void)pos; (void)color;
}

void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A)
{
	/* TODO: port from render_gl1.c */
	*R = *G = *B = 1.0f;
	*A = 1.0f;
}

void light_vert(LVERTEX *vert, u_int8_t *color)
{
	/* No per-vertex lighting yet — set full-bright white so geometry
	 * is visible. Without this, vertices stay at black (0xFF000000)
	 * and GPU_MODULATE (texture × vertex color) produces all black. */
	vert->color = 0xFFFFFFFF;  /* ARGB: opaque white */
	(void)color;
}

/*===================================================================
	Draw functions — GL-based via picaGL (proves pipeline correctness)
	TODO: replace with raw citro3d C3D_DrawArrays once visual output confirmed
===================================================================*/

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;
	C3D_BufInfo *bufInfo;

	static int _dc = 0;

	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer || !s_scratch)
		return false;

	if (_dc < 5) {
		char b[128];
		snprintf(b,sizeof(b),"draw[%d] groups=%d ortho=%d scratch=%d/%d",
			_dc, renderObject->numTextureGroups, orthographic, s_scratchUsed, s_scratchMax);
		c3d_trace(b);
	}
	_dc++;

	/* Using picaGL's shader — no C3D_BindProgram */

	if (!orthographic)
		upload_matrices();
	else
	{
		/* Orthographic — upload to picaGL's uniform 0 (projection slot) */
		C3D_Mtx ortho;
		Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
			render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, 0, &ortho);
	}

	for (group = 0; group < renderObject->numTextureGroups; group++)
	{
		TEXTUREGROUP *tg = &renderObject->textureGroups[group];
		int numIndices = tg->numTriangles * 3;
		int startVert = tg->startVert;
		bool has_texture = false;
		int totalVerts, i;
		gpu_vertex_t *dst;

		if (numIndices <= 0) continue;
		if (s_scratchUsed + numIndices > s_scratchMax) continue; /* overflow guard */

		dst = s_scratch + s_scratchUsed;

		/* Convert LVERTEX/TLVERTEX → gpu_vertex_t, expanding indices */
		if (renderObject->lpIndexBuffer)
		{
			WORD *indices = (WORD*)renderObject->lpIndexBuffer + tg->startIndex;

			if (orthographic)
			{
				TLVERTEX *verts = (TLVERTEX*)renderObject->lpVertexBuffer;
				for (i = 0; i < numIndices; i++)
				{
					TLVERTEX *v = &verts[startVert + indices[i]];
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = ((v->color >> 16) & 0xFF) / 255.0f;
					dst[i].color[1] = ((v->color >> 8)  & 0xFF) / 255.0f;
					dst[i].color[2] = ((v->color)       & 0xFF) / 255.0f;
					dst[i].color[3] = ((v->color >> 24) & 0xFF) / 255.0f;
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
			}
			else
			{
				LVERTEX *verts = (LVERTEX*)renderObject->lpVertexBuffer;
				for (i = 0; i < numIndices; i++)
				{
					LVERTEX *v = &verts[startVert + indices[i]];
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = ((v->color >> 16) & 0xFF) / 255.0f;
					dst[i].color[1] = ((v->color >> 8)  & 0xFF) / 255.0f;
					dst[i].color[2] = ((v->color)       & 0xFF) / 255.0f;
					dst[i].color[3] = ((v->color >> 24) & 0xFF) / 255.0f;
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
			}
			totalVerts = numIndices;
		}
		else
		{
			/* Non-indexed: use vertex list directly */
			LVERTEX *verts = (LVERTEX*)renderObject->lpVertexBuffer;
			int numVerts = tg->numVerts;
			if (s_scratchUsed + numVerts > s_scratchMax) continue;
			for (i = 0; i < numVerts; i++)
			{
				LVERTEX *v = &verts[startVert + i];
				dst[i].pos[0] = v->x;
				dst[i].pos[1] = v->y;
				dst[i].pos[2] = v->z;
				dst[i].color[0] = ((v->color >> 16) & 0xFF) / 255.0f;
				dst[i].color[1] = ((v->color >> 8)  & 0xFF) / 255.0f;
				dst[i].color[2] = ((v->color)       & 0xFF) / 255.0f;
				dst[i].color[3] = ((v->color >> 24) & 0xFF) / 255.0f;
				dst[i].texcoord[0] = v->tu;
				dst[i].texcoord[1] = v->tv;
			}
			totalVerts = numVerts;
		}

		/* GPU state */
		if (tg->colourkey)
		{
			C3D_AlphaTest(true, GPU_GREATER, 0x64);
			if (!_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
		}

		/* Texture binding */
		{
			C3D_TexEnv *env = C3D_GetTexEnv(0);
			C3D_TexEnvInit(env);

			if (tg->texture)
			{
				texture_t *texdata = (texture_t*)tg->texture;
				if (texdata->initialized)
				{
					C3D_TexBind(0, &texdata->tex);
					C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
					C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
					has_texture = true;
				}
			}
			if (!has_texture)
			{
				C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
			}
		}

		/* Configure vertex attributes and buffer — use global citro3d state
		 * getters (C3D_GetAttrInfo/C3D_GetBufInfo) as the working renderer does */
		{
			C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
			C3D_BufInfo *bufInfo = C3D_GetBufInfo();
			AttrInfo_Init(attrInfo);
			AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);  /* v0: pos */
			AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);  /* v1: color */
			AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2);  /* v2: texcoord */
			BufInfo_Init(bufInfo);
			BufInfo_Add(bufInfo, dst, sizeof(gpu_vertex_t), 3, 0x210);
		}

		C3D_DrawArrays(GPU_TRIANGLES, 0, totalVerts);

		s_scratchUsed += totalVerts;

		/* Restore state */
		if (tg->colourkey)
		{
			C3D_AlphaTest(false, GPU_ALWAYS, 0);
			if (!_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
		}
	}

	return true;
}

bool draw_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, false);
}

bool draw_2d_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, true);
}

bool draw_line_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 1, false);
}

#endif /* RENDERER_C3D */
