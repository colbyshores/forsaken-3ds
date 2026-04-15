/*
 * render_c3d.c — Native citro3d renderer for Nintendo 3DS
 *
 * Replaces the picaGL (GL1 immediate mode) path with direct citro3d
 * calls, enabling:
 *   - GPU-side modelview+projection transforms via vertex shader
 *   - Cached vertex/index buffers in linear VRAM
 *   - Single-pass stereo (same geometry, swap projection uniform)
 *
 * Build with RENDERER_C3D defined (and GL != 1) to use this renderer.
 * The picaGL path (render_gl1.c + GL=1) remains as fallback.
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

/* ---- vertex attribute layout ---- */
static C3D_AttrInfo s_attrInfo3D;
static C3D_AttrInfo s_attrInfo2D;

/* ---- globals expected by the engine ---- */

render_info_t render_info;

typedef struct { float anisotropic; } gl_caps_t;
gl_caps_t caps;

MATRIX proj_matrix;
MATRIX view_matrix;
MATRIX world_matrix;

/* Texture handle — stores citro3d texture ID (stub for now) */
typedef struct { u32 id; } texture_t;

/* bSquareOnly — shared global, always true on 3DS */
bool bSquareOnly = true;

/* Additive blend flag (matches GL1 behavior) */
bool _additive_blend_active = false;

/* picaGL replacement functions — initialize/teardown citro3d directly */
void pglInit(void)
{
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
}

void pglExit(void)
{
	c3d_renderer_cleanup();
	C3D_Fini();
}

void pglSwapBuffers(void)
{
	C3D_FrameEnd(0);
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
	s_shaderDVLB = DVLB_ParseFile((u32*)render_c3d_shbin, render_c3d_shbin_len);
	if (!s_shaderDVLB)
		return false;

	shaderProgramInit(&s_shaderProgram);
	if (shaderProgramSetVsh(&s_shaderProgram, &s_shaderDVLB->DVLE[0]) < 0)
		return false;

	/* 3D vertex: pos(3f) + color(4ub) + texcoord(2f) = 20 bytes */
	AttrInfo_Init(&s_attrInfo3D);
	AttrInfo_AddLoader(&s_attrInfo3D, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(&s_attrInfo3D, 1, GPU_UNSIGNED_BYTE, 4);
	AttrInfo_AddLoader(&s_attrInfo3D, 2, GPU_FLOAT, 2);

	/* 2D vertex: pos(4f) + color(4ub) + texcoord(2f) = 24 bytes */
	AttrInfo_Init(&s_attrInfo2D);
	AttrInfo_AddLoader(&s_attrInfo2D, 0, GPU_FLOAT, 4);
	AttrInfo_AddLoader(&s_attrInfo2D, 1, GPU_UNSIGNED_BYTE, 4);
	AttrInfo_AddLoader(&s_attrInfo2D, 2, GPU_FLOAT, 2);

	/* Render targets */
	s_targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	s_targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	s_targetBot   = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);

	u32 transfer_flags =
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
		GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

	C3D_RenderTargetSetOutput(s_targetLeft,  GFX_TOP,    GFX_LEFT,  transfer_flags);
	C3D_RenderTargetSetOutput(s_targetRight, GFX_TOP,    GFX_RIGHT, transfer_flags);
	C3D_RenderTargetSetOutput(s_targetBot,   GFX_BOTTOM, GFX_LEFT,  transfer_flags);

	Mtx_Identity(&s_projection);
	Mtx_Identity(&s_modelview);

	s_shaderReady = true;
	DebugPrintf("c3d_renderer_init: shader loaded, render targets created\n");
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
	/* RENDERMATRIX.m is [4][4] row-major.
	 * C3D_Mtx.r[row] is C3D_FVec with WZYX internal layout:
	 *   r[row].x = col3, r[row].y = col2, r[row].z = col1, r[row].w = col0
	 * We use the flat m[16] accessor: m[row*4 + col] maps to
	 * the WZYX-reversed storage so col 0→index 3, col 1→index 2, etc. */
	int r, c;
	for (r = 0; r < 4; r++)
		for (c = 0; c < 4; c++)
			dst->m[r * 4 + (3 - c)] = src->m[r][c];
}

static void upload_matrices(void)
{
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &s_projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &s_modelview);
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
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0);
	C3D_FrameDrawOn(s_targetLeft);
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
	C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0);
	return true;
}

bool FSClearBlack(void)
{
	C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_COLOR, 0x000000FF, 0);
	return true;
}

bool FSClearDepth(XYRECT *rect)
{
	(void)rect;
	C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_DEPTH, 0, 0);
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
	/* TODO: C3D_SetViewport when rendering */
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
	RENDERMATRIX gl_proj;
	memmove(&proj_matrix, &matrix->m, sizeof(proj_matrix));

	/* D3D LH [0,1] depth → GL RH [-1,1] depth conversion */
	memmove(&gl_proj, matrix, sizeof(RENDERMATRIX));
	gl_proj._33 = 2.0f * matrix->_33 - 1.0f;
	gl_proj._43 = 2.0f * matrix->_43;

	matrix_to_c3d(&gl_proj, &s_projection);
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
	/* TODO Phase 2: C3D_TexDelete */
	free(texture);
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

	/* TODO Phase 2: create C3D_Tex, tile with C3D_TexInitVRAM, upload.
	 * For now, allocate a stub texture_t so the pointer is non-NULL. */
	if (!*t)
	{
		texture_t *texdata = malloc(sizeof(texture_t));
		texdata->id = 0;
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
	/* Phase 2: no per-vertex lighting yet. Phase 6 moves to GPU. */
	(void)vert; (void)color;
}

/*===================================================================
	Draw functions — Phase 2 citro3d draw path
===================================================================*/

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;
	C3D_BufInfo bufInfo;

	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer)
		return false;

	C3D_BindProgram(&s_shaderProgram);
	C3D_SetAttrInfo(orthographic ? &s_attrInfo2D : &s_attrInfo3D);

	if (!orthographic)
		upload_matrices();
	else
	{
		/* 2D: identity modelview, orthographic projection */
		C3D_Mtx ortho, identity;
		Mtx_Identity(&identity);
		Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
			render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, false);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
	}

	/* Set up vertex buffer */
	BufInfo_Init(&bufInfo);
	BufInfo_Add(&bufInfo, renderObject->lpVertexBuffer,
		orthographic ? sizeof(TLVERTEX) : sizeof(LVERTEX),
		3, /* 3 attributes: pos, color, texcoord */
		orthographic ? 0x210 : 0x210);
	C3D_SetBufInfo(&bufInfo);

	for (group = 0; group < renderObject->numTextureGroups; group++)
	{
		TEXTUREGROUP *tg = &renderObject->textureGroups[group];

		/* Alpha test for colorkey textures */
		if (tg->colourkey)
		{
			C3D_AlphaTest(true, GPU_GREATER, 0x64);
			if (!_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
		}

		/* TODO Phase 2: bind tg->texture as C3D_Tex */

		/* Draw indexed triangles */
		if (renderObject->lpIndexBuffer && tg->numTriangles > 0)
		{
			WORD *indices = (WORD*)renderObject->lpIndexBuffer + tg->startIndex;
			C3D_DrawElements(
				primitive_type == 1 ? GPU_GEOMETRY_PRIM : GPU_TRIANGLES,
				tg->numTriangles * 3,
				C3D_UNSIGNED_SHORT,
				indices);
		}

		/* Restore state after colorkey */
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
