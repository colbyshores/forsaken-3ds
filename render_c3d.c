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
#include "camera.h"

/* ---- shader binary (compiled from shaders/render_c3d.v.pica) ---- */
#include "render_c3d_shbin.h"

/* ---- uniform register locations (must match .v.pica) ---- */
#define UNIFORM_PROJECTION  0
#define UNIFORM_MODELVIEW   4

/* ---- citro3d state ---- */

static DVLB_s          *s_shaderDVLB    = NULL;
static shaderProgram_s  s_shaderProgram;
static bool             s_shaderReady   = false;
static C3D_RenderTarget *s_targetLeft    = NULL;
static C3D_RenderTarget *s_targetRight   = NULL;
static C3D_RenderTarget *s_targetBottom  = NULL;  /* mono 320×240 for HUD */
static C3D_RenderTarget *s_targetCurrent = NULL;  /* active render target */
static C3D_RenderTarget *s_targetSaved   = NULL;  /* stashed target for HUD restore */
static bool s_hudDrawnThisFrame = false;          /* HUD drawn once per frame */
/* Saved render_info state during HUD-on-bottom pass (restored on End) */
static int  s_hudSavedThisModeW = 0;
static int  s_hudSavedThisModeH = 0;
static int  s_hudSavedWinCx     = 0;
static int  s_hudSavedWinCy     = 0;
static bool              s_stereoFrame   = false;

/* Color write mask — always GPU_WRITE_ALL on citro3d (software anaglyph
 * removed; hardware stereo uses separate render targets instead). */
#define s_colorMask GPU_WRITE_ALL

/* ---- GPU vertex format (all floats, matches shader inputs) ---- */
typedef struct {
	float pos[3];
	float color[4];
	float texcoord[2];
} gpu_vertex_t;  /* 36 bytes */

/* ---- Single-pass stereo display list ---- */

/* Records one texture group draw for replay on the second eye.
 * Vertex data stays in the scratch buffer; only the MVP changes. */
typedef struct {
	int      scratchOffset;   /* index into s_scratch */
	int      vertexCount;
	MATRIX   worldMatrix;
	MATRIX   projMatrix;
	render_viewport_t viewport;
	void    *texture;         /* texture_t* or NULL */
	bool     colourkey;
	bool     orthographic;
	bool     additive_blend;
	bool     has_texture;
} dl_entry_t;

#define MAX_DL_ENTRIES 4096
static dl_entry_t s_dl[MAX_DL_ENTRIES];
static int  s_dlCount     = 0;
static bool s_dlRecording = false;
static bool s_dlReplay    = false;  /* true = skip draw, replay handles it */

/* Scratch buffer for vertex conversion.
 * 4MB = ~116000 verts at 36 bytes each.  Needs to handle heavy
 * firefight frames (many ships + projectiles + explosion particles +
 * laser beams + mipped BSP walls) without FrameSplit stalls or
 * silent geometry drops at the overflow-after-flush fallback. */
#define GPU_SCRATCH_SIZE  (4 * 1024 * 1024)
static gpu_vertex_t *s_scratch = NULL;
static int s_scratchUsed = 0;
static int s_scratchMax = 0;

/* ---- globals expected by the engine ---- */

render_info_t render_info;

typedef struct { float anisotropic; } gl_caps_t;
gl_caps_t caps;

/* Forward declarations */
static void upload_matrices(void);

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

/* TexEnv cache: skip redundant GPU state changes when consecutive
 * draws use the same texture or same untextured mode. */
static void *s_lastBoundTexture = NULL;  /* last texture_t* passed to C3D_TexBind */
static bool  s_lastTexEnvTextured = false;  /* last TexEnv was textured vs vertex-only */


/* picaGL is NOT linked for the citro3d renderer. We provide our own
 * pglInit/pglExit/pglSwapBuffers/pglTransferEye implementations. */
void pglInit(void)
{
	/* Idempotent: paired with c3d_renderer_init's own idempotency. If
	 * we let C3D_Init re-fire on a render_mode_select retrigger, it
	 * resets citro3d's context state — but the render targets we
	 * created in the previous c3d_renderer_init are tied to the OLD
	 * context. They become orphaned pointers with stale GPU bookkeeping
	 * and subsequent draws end up in undefined state. The portal
	 * "black void" regression on the autotest sweep traced to this:
	 * primary geometry rendered fine but through-portal groups landed
	 * on a target citro3d no longer treated as live. */
	static bool _pglInited = false;
	if (_pglInited) return;
	_pglInited = true;

	/* 4× the default GPU command buffer (256KB → 1MB).
	 * Forsaken draws hundreds of texture groups per frame, each a
	 * separate C3D_DrawArrays call that appends ~100 bytes of GPU
	 * commands.  Laser beams and heavy particle scenes can produce
	 * thousands of draw calls that overflow smaller buffers. */
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
}

void pglExit(void)
{
	c3d_renderer_cleanup();
	C3D_Fini();
}

void pglSwapBuffers(void)
{
	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}
	/* Reset stereo state for next frame */
	s_stereoFrame = false;
	s_dlReplay = false;
	s_dlRecording = false;
	s_hudDrawnThisFrame = false;
}

/*===================================================================
	HUD bottom-screen redirection
	-------------------------------------------------------------------
	The gameplay HUD (ammo / weapons / shield / messages) is rendered
	on the mono bottom screen instead of overlaying the stereo top
	screen. pglHudBeginBottom() stashes the current top-screen target,
	clears the bottom target, switches C3D's draw target to it, and
	returns true if the caller should actually issue draws. It returns
	false on the second eye's pass so we only draw the HUD once per
	frame (bottom is mono — one copy is all we need).
===================================================================*/
bool pglHudBeginBottom(void)
{
	if (!s_targetBottom || !s_inFrame)
		return false;
	if (s_hudDrawnThisFrame)
		return false;
	s_hudDrawnThisFrame = true;

	/* Save + override dimensions so Print4x5Text / 2D layout math
	   targets 320×240 instead of 400×240. */
	s_hudSavedThisModeW = (int)render_info.ThisMode.w;
	s_hudSavedThisModeH = (int)render_info.ThisMode.h;
	s_hudSavedWinCx     = (int)render_info.window_size.cx;
	s_hudSavedWinCy     = (int)render_info.window_size.cy;
	render_info.ThisMode.w    = 320;
	render_info.ThisMode.h    = 240;
	render_info.window_size.cx = 320;
	render_info.window_size.cy = 240;

	s_targetSaved = s_targetCurrent;
	C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	C3D_FrameDrawOn(s_targetBottom);
	s_targetCurrent = s_targetBottom;
	/* PICA portrait viewport for the 240×320 bottom target. */
	C3D_SetViewport(0, 0, 240, 320);

	/* Re-bind shader state for the new target (matches pglTransferEye). */
	C3D_BindProgram(&s_shaderProgram);
	{
		C3D_AttrInfo *ai = C3D_GetAttrInfo();
		AttrInfo_Init(ai);
		AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
		AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
		AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
	}
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);
	C3D_DepthMap(true, 1.0f, 1.0f);
	return true;
}

/* Wipe the bottom-screen render target via the same GPU-side pattern
 * used in render_init().  Call this when leaving gameplay for the
 * title screen — without it, the gameplay HUD's last frame stays
 * frozen on the bottom screen until the next gameplay session draws
 * over it (the title screen never touches the bottom target).
 *
 * Safe to call between frames; if the engine is in an open frame,
 * close it first so the C3D_FrameBegin below doesn't conflict. */
void pglClearBottomScreen(void)
{
	if (!s_targetBottom)
		return;

	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}

	int i;
	for (i = 0; i < 3; i++)
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetBottom);
		C3D_FrameEnd(0);
	}

	s_targetCurrent = NULL;
	s_hudDrawnThisFrame = false;
}

void pglHudEndBottom(void)
{
	if (!s_targetSaved)
		return;
	C3D_FrameDrawOn(s_targetSaved);
	s_targetCurrent = s_targetSaved;
	s_targetSaved = NULL;

	/* Restore saved dimensions. */
	render_info.ThisMode.w     = s_hudSavedThisModeW;
	render_info.ThisMode.h     = s_hudSavedThisModeH;
	render_info.window_size.cx = s_hudSavedWinCx;
	render_info.window_size.cy = s_hudSavedWinCy;

	/* Restore top-screen PICA portrait viewport (240×400). */
	C3D_SetViewport(0, 0, 240, 400);

	/* Re-bind shader state for the restored top target. */
	C3D_BindProgram(&s_shaderProgram);
	{
		C3D_AttrInfo *ai = C3D_GetAttrInfo();
		AttrInfo_Init(ai);
		AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
		AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
		AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
	}
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);
	C3D_DepthMap(true, 1.0f, 1.0f);
}

/* Replay the display list recorded during the first eye.
 * Reuses vertex data already in the scratch buffer — only
 * recomputes the MVP with the updated view matrix. */
static void replay_display_list(void)
{
	int i;
	void *dl_lastTex = NULL;
	bool dl_lastTextured = false;

	for (i = 0; i < s_dlCount; i++)
	{
		dl_entry_t *e = &s_dl[i];
		gpu_vertex_t *dst = s_scratch + e->scratchOffset;

		/* Restore per-entry state */
		if (e->additive_blend)
		{
			C3D_DepthTest(true, GPU_LESS, s_colorMask & ~GPU_WRITE_DEPTH);
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
				GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
		}
		else
		{
			C3D_DepthTest(true, GPU_LESS, s_colorMask);
			if (e->has_texture)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
			else
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
		}

		if (e->colourkey)
			C3D_AlphaTest(true, GPU_GREATER, 0x64);

		/* Viewport */
		{
			int bottom = (int)render_info.ThisMode.h - (int)(e->viewport.Y + e->viewport.Height);
			int pica_x = bottom < 0 ? 0 : bottom;
			int pica_y = (400 - (int)e->viewport.Width) - (int)e->viewport.X;
			if (pica_y < 0) pica_y = 0;
			C3D_SetViewport((u32)pica_x, (u32)pica_y,
				(u32)e->viewport.Height, (u32)e->viewport.Width);
		}

		/* Recompute MVP with new view matrix (eye offset already applied
		 * by the engine before pglTransferEye was called) */
		if (!e->orthographic)
		{
			memmove(&world_matrix, &e->worldMatrix, sizeof(MATRIX));
			memmove(&proj_matrix, &e->projMatrix, sizeof(MATRIX));
			upload_matrices();
		}
		else
		{
			C3D_Mtx ortho, identity;
			Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
				render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
			Mtx_Identity(&identity);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
		}

		/* Texture binding — cached within replay */
		if (e->has_texture && e->texture)
		{
			texture_t *texdata = (texture_t*)e->texture;
			if (texdata->initialized)
			{
				if ((void*)texdata != dl_lastTex || !dl_lastTextured)
				{
					C3D_TexBind(0, &texdata->tex);
					C3D_TexEnv *env = C3D_GetTexEnv(0);
					C3D_TexEnvInit(env);
					C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
					C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
					dl_lastTex = (void*)texdata;
					dl_lastTextured = true;
				}
				else
					C3D_TexBind(0, &texdata->tex);
			}
			else if (dl_lastTextured)
				goto dl_notex;
		}
		else if (dl_lastTextured)
		{
		dl_notex:;
			C3D_TexEnv *env = C3D_GetTexEnv(0);
			C3D_TexEnvInit(env);
			C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
			dl_lastTex = NULL;
			dl_lastTextured = false;
		}

		/* Buffer pointer only — AttrInfo set once in pglTransferEye */
		{
			C3D_BufInfo *bi = C3D_GetBufInfo();
			BufInfo_Init(bi);
			BufInfo_Add(bi, dst, sizeof(gpu_vertex_t), 3, 0x210);
		}
		C3D_DrawArrays(GPU_TRIANGLES, 0, e->vertexCount);

		/* Restore */
		if (e->colourkey)
			C3D_AlphaTest(false, GPU_ALWAYS, 0);
	}
}

void pglTransferEye(unsigned int eye)
{
	if (eye == GFX_LEFT)
	{
		/* Left eye finished. Stop recording, prepare for replay. */
		s_dlRecording = false;

		/* Switch to right eye target */
		C3D_RenderTargetClear(s_targetRight, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetRight);
		s_targetCurrent = s_targetRight;

		/* Re-initialize GPU state for the new render target */
		C3D_BindProgram(&s_shaderProgram);
		{
			C3D_AttrInfo *ai = C3D_GetAttrInfo();
			AttrInfo_Init(ai);
			AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
			AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
			AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
		}
		C3D_DepthTest(true, GPU_LESS, s_colorMask);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
		C3D_DepthMap(true, 1.0f, 1.0f);

		/* Replay disabled (see FSBeginScene comment). The right eye pass
		 * runs the callback fully, so we just need the target switch
		 * above — no replay flag. */
		s_dlReplay = false;
	}
	if (eye == GFX_RIGHT)
	{
		/* Right eye done. Clear replay flag so post-stereo rendering
		 * (missile camera, rear view, HUD overlays) draws normally. */
		s_dlReplay = false;
	}
	s_stereoFrame = true;
}

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
	/* Idempotent: subsequent calls (engine triggers render_mode_select on
	 * stereo toggles, mode changes, fullscreen flips, etc.) would otherwise
	 * leak ~5MB of linear memory each time — shader DVLB, 4MB scratch, three
	 * render targets — because render_cleanup is a no-op. After ~5 calls
	 * linear heap is exhausted and FSCreateIndexBuffer starts returning NULL.
	 * Diagnosed via 246 "render targets created" lines in forsaken_c3d.log
	 * paired with 10+ "FSCreateIndexBuffer: linearAlloc FAILED" entries near
	 * EOF, immediately preceding GPUCMD_AddInternal's full-buffer svcBreak. */
	{
		char _b[64];
		snprintf(_b, sizeof(_b), "c3d_renderer_init: enter shaderReady=%d", (int)s_shaderReady);
		c3d_trace(_b);
	}
	if (s_shaderReady) {
		c3d_trace("c3d_renderer_init: already initialized, skipping");
		return true;
	}
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

	/* Allocate scratch buffer for vertex conversion */
	s_scratch = (gpu_vertex_t*)linearAlloc(GPU_SCRATCH_SIZE);
	s_scratchMax = GPU_SCRATCH_SIZE / sizeof(gpu_vertex_t);
	s_scratchUsed = 0;

	/* Create render targets and link to display output.
	 * Both left and right eye targets are created unconditionally —
	 * the right target is only drawn to when stereo is active. */
	{
		u32 transferFlags =
			GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
			GX_TRANSFER_RAW_COPY(0) |
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
			GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

		s_targetLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetLeft) {
			c3d_trace("FAILED to create left render target");
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetLeft, GFX_TOP, GFX_LEFT, transferFlags);

		s_targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetRight) {
			c3d_trace("FAILED to create right render target");
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetRight, GFX_TOP, GFX_RIGHT, transferFlags);

		/* Bottom screen (320×240, mono). Stored in GPU as 240×320 portrait
		 * like the top. Used for gameplay HUD text so the stereo 3D image
		 * above isn't cluttered. */
		s_targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetBottom) {
			c3d_trace("FAILED to create bottom render target");
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetBottom, GFX_BOTTOM, GFX_LEFT, transferFlags);
	}
	c3d_trace("render targets created and linked to display");

	/* Match picaGL depth map: scale=1.0, offset=1.0 */
	C3D_DepthMap(true, 1.0f, 1.0f);

	s_shaderReady = true;
	c3d_trace("c3d_renderer_init: OK — shader ready, render targets created");
	return true;
}

void c3d_renderer_cleanup(void)
{
	c3d_trace("c3d_renderer_cleanup: called");
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
	/* Match the working trash/forsaken renderer exactly:
	 * compute combined MVP in engine format, convert to C3D_Mtx,
	 * apply PICA fix, upload via C3D_FVUnifMtx4x4. */
	MATRIX mvp;
	C3D_Mtx c3d_mvp, identity;

	MatrixMultiply(&world_matrix, &view_matrix, &mvp);
	MatrixMultiply(&mvp, &proj_matrix, &mvp);

	matrix_to_c3d((const RENDERMATRIX*)&mvp, &c3d_mvp);

	/* PICA depth remap: D3D [0,1] → PICA [-1,0] */
	c3d_mvp.r[2].x -= c3d_mvp.r[3].x;
	c3d_mvp.r[2].y -= c3d_mvp.r[3].y;
	c3d_mvp.r[2].z -= c3d_mvp.r[3].z;
	c3d_mvp.r[2].w -= c3d_mvp.r[3].w;

	/* 90° screen rotation */
	{
		C3D_FVec row0 = c3d_mvp.r[0];
		c3d_mvp.r[0] = c3d_mvp.r[1];
		c3d_mvp.r[1].x = -row0.x;
		c3d_mvp.r[1].y = -row0.y;
		c3d_mvp.r[1].z = -row0.z;
		c3d_mvp.r[1].w = -row0.w;
	}

	Mtx_Identity(&identity);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &c3d_mvp);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
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

	/* Wipe the VRAM framebuffers via the GPU before any engine code
	 * can render.  gfxInit(..., true) hands us framebuffers backed by
	 * VRAM which comes up holding whatever was last written to those
	 * addresses — GPU debris, the previous .3dsx's render target,
	 * uninitialised noise.  The engine's title-screen load takes
	 * ~2 seconds during which the top framebuffer would otherwise
	 * display garbage, and the bottom screen would stay garbage until
	 * the gameplay HUD first drew over it.
	 *
	 * CPU memset is unsafe at this stage — the VRAM mapping is not
	 * fully writable from CPU before the first GPU frame, so naive
	 * memset crashes with a data abort on FAR=0x1f300000.
	 *
	 * The GPU-side pattern: C3D_RenderTargetClear queues a clear
	 * command, but the clear only actually executes once the target
	 * is bound via C3D_FrameDrawOn inside a frame, and the cleared
	 * pixels only reach the visible buffer after C3D_FrameEnd
	 * triggers display transfer.  Bind every target each frame and
	 * run a few frame cycles to catch both halves of the
	 * double-buffer pair on top + bottom. */
	{
		int i;
		for (i = 0; i < 3; i++)
		{
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

			C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetLeft);

			C3D_RenderTargetClear(s_targetRight, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetRight);

			C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetBottom);

			C3D_FrameEnd(0);
		}
	}

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
	/* No-op on citro3d — software anaglyph removed.
	 * Hardware stereo uses separate render targets instead of color masking.
	 * Function retained because shared code in oct2.c calls it. */
	(void)red; (void)green; (void)blue;
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
	/* Single-pass stereo: if replay already drew the second eye, skip. */
	if (s_dlReplay)
		return true;
	/* Single-pass stereo replay is DISABLED.
	 *
	 * The replay mechanism recorded left-eye draws into s_dl[] and re-submitted
	 * them with the right-eye view matrix, saving one BSP traversal per frame.
	 * But it silently dropped any 2D/HUD draw issued AFTER the replay was
	 * triggered on the second eye — menu text, reticles, missile viewport
	 * all ended up on only one eye as a result. Disabling replay makes both
	 * eye callbacks run fully (CPU cost ~2x 3D traversal in stereo mode), so
	 * every draw lands on both targets naturally.
	 *
	 * If FPS on Old 3DS drops too far, the smarter fix is to skip replay
	 * for ortho (2D) draws only, or to split the callback into "3D once + 2D
	 * twice" explicitly. Leaving that for later. */
	s_dlCount = 0;
	s_dlRecording = false;
	s_scratchUsed = 0;
	if (!s_inFrame)
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetLeft);
		s_targetCurrent = s_targetLeft;
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
		C3D_DepthTest(true, GPU_LESS, s_colorMask);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
		C3D_DepthMap(true, 1.0f, 1.0f);

		/* Reset TexEnv cache — force first draw to set up GPU state */
		s_lastBoundTexture = NULL;
		s_lastTexEnvTextured = false;

		/* Initialize modelView uniform (register 4) to identity.
		 * upload_matrices writes combined MVP to register 0 (projection),
		 * so the shader's modelView * pos must be identity to avoid
		 * double-transforming the already-combined MVP result. */
		/* Write identity to modelView uniform (register 4) via GPUCMD */
		{
			float id[16] = {
				1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
			};
			GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | UNIFORM_MODELVIEW);
			GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)id, 16);
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
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
}

void disable_zbuff_write(void)
{
	/* Write color only (no depth), respecting anaglyph channel mask */
	GPU_WRITEMASK mask = s_colorMask & ~GPU_WRITE_DEPTH;
	C3D_DepthTest(true, GPU_LESS, mask);
}

void disable_zbuff(void)
{
	GPU_WRITEMASK mask = s_colorMask & ~GPU_WRITE_DEPTH;
	C3D_DepthTest(false, GPU_ALWAYS, mask);
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

/* Scissored sub-viewport clear via rasterized quad. C3D_RenderTargetClear
 * can only clear the entire render target (no scissor support), so a
 * sub-viewport (missile cam, PIP, rear view) can't rely on it to wipe
 * just its inset rectangle. Instead we draw a full-NDC black quad at
 * z=1 with GPU_ALWAYS + depth write: the rasterizer naturally clips it
 * to the currently-bound C3D_SetViewport, writing black color and
 * far-plane depth only within the inset. This kills primary's
 * color-bleed-through and gives secondary's GPU_LESS draws a clean
 * depth slate. */
static void clear_sub_viewport_via_quad(void)
{
	if (!s_scratch || s_scratchUsed + 6 > s_scratchMax) return;

	gpu_vertex_t *dst = s_scratch + s_scratchUsed;
	s_scratchUsed += 6;

	/* PICA's clip range is [-1, 0] in z (not [-1, 1] like GL), so
	 * z=1 would be clipped. Use z=0 (far plane in this convention). */
	static const float verts[6][3] = {
		{-1.0f, -1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f},
		{ 1.0f,  1.0f, 0.0f},
		{-1.0f, -1.0f, 0.0f},
		{ 1.0f,  1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
	};

	for (int i = 0; i < 6; i++) {
		dst[i].pos[0] = verts[i][0];
		dst[i].pos[1] = verts[i][1];
		dst[i].pos[2] = verts[i][2];
		dst[i].color[0] = 0.0f;
		dst[i].color[1] = 0.0f;
		dst[i].color[2] = 0.0f;
		dst[i].color[3] = 1.0f;
		dst[i].texcoord[0] = 0.0f;
		dst[i].texcoord[1] = 0.0f;
	}

	/* Identity MVP so NDC vertices go straight to clip space; the
	 * viewport transform maps them onto the inset rectangle. */
	C3D_Mtx identity;
	Mtx_Identity(&identity);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &identity);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &identity);

	C3D_DepthTest(true, GPU_ALWAYS, s_colorMask);
	C3D_CullFace(GPU_CULL_NONE);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);

	C3D_BufInfo *bi = C3D_GetBufInfo();
	BufInfo_Init(bi);
	BufInfo_Add(bi, dst, sizeof(gpu_vertex_t), 3, 0x210);
	C3D_DrawArrays(GPU_TRIANGLES, 0, 6);

	/* Restore standard game-draw state */
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);
}

bool FSClear(XYRECT *rect)
{
	(void)rect;
	if (!s_targetCurrent || s_dlReplay) return true;

	/* Sub-viewports (missile cam, PIP, rear view) render AFTER the main
	 * camera. Rasterized quad clear scissored to viewport via the
	 * rasterizer — wipes color + depth inside just the inset rect.
	 * See clear_sub_viewport_via_quad comment for why. */
	extern int16_t CameraRendering;
	if (CameraRendering != CAMRENDERING_None &&
	    CameraRendering != CAMRENDERING_Main)
	{
		clear_sub_viewport_via_quad();
		return true;
	}

	C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	return true;
}

bool FSClearBlack(void)
{
	if (!s_targetCurrent) return true;
	C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_COLOR, 0x000000FF, 0);
	return true;
}

bool FSClearDepth(XYRECT *rect)
{
	(void)rect;
	if (s_targetCurrent)
		C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_DEPTH, 0, 0xFFFFFF);
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

	/* 3DS portal rendering: full-screen viewport + GPU scissor to the
	 * portal-aperture sub-rect. The original engine path used a sub-rect
	 * viewport plus a magnified projection (visi.c:1098-1125 unmodified
	 * branch) to fit the through-portal frustum into the aperture; on
	 * citro3d that produced black voids on Remaster's stableizers /
	 * powerdown / starship / battlebase (vol2 happened to dodge it
	 * because of specific portal-aperture positions, not because the
	 * math was sound). The naive replacement (cam->Proj + sub-rect
	 * viewport) squishes the full camera view into the aperture, which
	 * is wrong scale.
	 *
	 * This path keeps the engine's per-group g->viewport semantics
	 * (= portal-aperture screen rect) but applies it as a scissor at
	 * the GPU rather than a viewport. Geometry renders at correct
	 * screen positions via cam's projection, and only the aperture
	 * pixels get written. Bonus: visible-group count stays small
	 * because portal traversal is unchanged, so we don't pay the
	 * outside_map=1 overdraw cost that crashes scratch on big levels. */
	{
		/* Translate game-space sub-rect into PICA portrait scissor coords. */
		int bottom = (int)render_info.ThisMode.h - (int)(view->Y + view->Height);
		int gl_x = (int)view->X;
		int pica_x = bottom < 0 ? 0 : bottom;
		int pica_y = (400 - (int)view->Width) - gl_x;
		if (pica_y < 0) pica_y = 0;
		int pica_w = (int)view->Height;
		int pica_h = (int)view->Width;

		/* Full-screen viewport for the actual rasterisation. */
		C3D_SetViewport(0, 0, 240, 400);
		/* Scissor restricts color/depth writes to the portal-aperture
		 * sub-rect. C3D_SCISSOR_NORMAL means "draw inside the rect". */
		C3D_SetScissor(GPU_SCISSOR_NORMAL,
		               (u32)pica_x, (u32)pica_y,
		               (u32)(pica_x + pica_w), (u32)(pica_y + pica_h));
	}

	return true;
}

/*===================================================================
	Matrix state
===================================================================*/

bool FSSetView(RENDERMATRIX *matrix)
{
	memmove(&view_matrix, &matrix->m, sizeof(view_matrix));
	return true;
}

bool FSSetWorld(RENDERMATRIX *matrix)
{
	memmove(&world_matrix, &matrix->m, sizeof(world_matrix));
	return true;
}

bool FSGetWorld(RENDERMATRIX *matrix)
{
	memmove(&matrix->m, &world_matrix, sizeof(matrix->m));
	return true;
}

bool FSSetProjection(RENDERMATRIX *matrix)
{
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

/* Try loading an HD texture in t3x format (ETC1A4 compressed).
 * Path mapping: data\textures\foo.bmp → romfs:/hd/textures/foo.t3x
 * Uses Tex3DS_TextureImportStdio which handles format, tiling, VRAM. */
#include <tex3ds.h>

static bool try_load_hd_texture(LPTEXTURE *t, const char *path,
	u_int16_t *width, u_int16_t *height, bool *colorkey)
{
	char hd_path[512];
	char *p;
	const char *data_start = NULL;
	int i;
	FILE *f;
	texture_t *texdata;
	Tex3DS_Texture t3x;

	/* Find "data\" in path */
	for (i = 0; path[i]; i++)
	{
		if ((path[i] == 'd' || path[i] == 'D') &&
		    (path[i+1] == 'a' || path[i+1] == 'A') &&
		    (path[i+2] == 't' || path[i+2] == 'T') &&
		    (path[i+3] == 'a' || path[i+3] == 'A') &&
		    (path[i+4] == '\\' || path[i+4] == '/'))
		{
			data_start = &path[i+5];
			break;
		}
	}
	if (!data_start)
		return false;

	snprintf(hd_path, sizeof(hd_path), "romfs:/hd_textures/%s", data_start);
	for (p = hd_path; *p; p++)
	{
		if (*p == '\\') *p = '/';
		if (*p >= 'A' && *p <= 'Z') *p += 32;
	}
	p = strrchr(hd_path, '.');
	if (p) strcpy(p, ".t3x");
	else strcat(hd_path, ".t3x");

	f = fopen(hd_path, "rb");
	if (!f)
		return false;

	/* Allocate texture_t wrapper */
	if (*t == NULL)
	{
		texdata = malloc(sizeof(texture_t));
		if (!texdata) { fclose(f); return false; }
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

	/* Tex3DS handles everything: format detection, decompression,
	 * tiling, and upload. Use linear memory (vram=false) since 6MB
	 * VRAM can't hold all HD textures at once. */
	t3x = Tex3DS_TextureImportStdio(f, &texdata->tex, NULL, false);
	fclose(f);

	if (!t3x)
	{
		DebugPrintf("HD texture: Tex3DS import failed for %s\n", hd_path);
		if (*t == NULL) free(texdata);
		return false;
	}

	/* OG 3DS mip-0 strip.
	 *
	 * The pack ships 512x512 base textures with a full Gaussian mip chain.
	 * New 3DS uses the whole thing. OG 3DS has a ~4x smaller linear heap
	 * and can't afford those full-size textures, so we rebuild each one
	 * at half dimensions (256 base, one fewer mip level), copying mip 1..N
	 * from the imported texture into mip 0..N-1 of a smaller allocation.
	 *
	 * PICA200 morton tiling is a function of mip dimensions only — mip 1
	 * of a 512 texture has the identical byte layout as mip 0 of a 256
	 * texture, so memcpy per level works without any tiling conversion.
	 *
	 * Peak memory during the rebuild is ~1.25x one texture (old + new
	 * briefly coexist); textures load serially so this doesn't compound.
	 */
	{
		bool is_new_3ds = false;
		APT_CheckNew3DS(&is_new_3ds);
		if (!is_new_3ds && texdata->tex.maxLevel > 0)
		{
			C3D_Tex small;
			u16 sw = texdata->tex.width  / 2;
			u16 sh = texdata->tex.height / 2;
			int new_max = texdata->tex.maxLevel - 1;
			if (!C3D_TexInitWithParams(&small, NULL, (C3D_TexInitParams){
				sw, sh, (u8)new_max,
				texdata->tex.fmt, GPU_TEX_2D, false
			}))
			{
				c3d_trace("try_load_hd_texture: OG mip-0 strip alloc FAILED");
				C3D_TexDelete(&texdata->tex);
				Tex3DS_TextureFree(t3x);
				if (*t == NULL) free(texdata);
				return false;
			}
			{
				int level;
				for (level = 0; level <= new_max; level++)
				{
					u32 size = 0;
					void *src = C3D_Tex2DGetImagePtr(&texdata->tex, level + 1, &size);
					void *dst = C3D_Tex2DGetImagePtr(&small,         level,     NULL);
					if (src && dst && size) memcpy(dst, src, size);
				}
			}
			C3D_TexDelete(&texdata->tex);
			texdata->tex = small;
		}
	}

	/* Bilinear-mipmap (GPU_NEAREST for mip selection): picks ONE mip level
	 * per fragment, then bilinear-filters within it. Gives anti-moiré
	 * without the trilinear softness or the doubled per-fragment texture
	 * taps. If the t3x has no mip chain, GPU clamps to mip 0. */
	C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetFilterMipmap(&texdata->tex, GPU_NEAREST);
	C3D_TexSetWrap(&texdata->tex, GPU_REPEAT, GPU_REPEAT);

	*width = texdata->tex.width;
	*height = texdata->tex.height;
	/* Match colorkey to the texture format. ETC1 has no alpha plane
	 * at all, so enabling alpha-test on it would just always pass.
	 * ETC1A4 carries real per-pixel alpha for sprites/grates. */
	*colorkey = (texdata->tex.fmt == GPU_ETC1A4);

	texdata->initialized = true;
	*t = (LPTEXTURE)texdata;

	Tex3DS_TextureFree(t3x);

	DebugPrintf("HD texture: loaded %s (%dx%d)\n",
		hd_path, texdata->tex.width, texdata->tex.height);
	return true;
}

static bool create_texture(LPTEXTURE *t, const char *path,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	texture_image_t image;

	/* Try HD texture first (4K source → 512x512 ETC1A4 via tex3ds) */
	if (try_load_hd_texture(t, path, width, height, colorkey))
		return true;

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
				/* Colourkey: pure black (0,0,0) → alpha=0 for transparency.
				 * Check BEFORE gamma because gamma_table clamps 0→1,
				 * turning (0,0,0) into (1,1,1) and breaking R+G+B==0. */
				bool is_colorkey = (((u_int8_t)image.data[idx] + (u_int8_t)image.data[idx+1] + (u_int8_t)image.data[idx+2]) == 0);
				image.data[idx]   = (char)gamma_table[(u_int8_t)image.data[idx]];
				image.data[idx+1] = (char)gamma_table[(u_int8_t)image.data[idx+1]];
				image.data[idx+2] = (char)gamma_table[(u_int8_t)image.data[idx+2]];
				if (is_colorkey)
					image.data[idx+3] = 0;
				else
					image.data[idx+3] = (char)gamma_table[(u_int8_t)image.data[idx+3]];
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
			C3D_TexFlush(&texdata->tex);
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
	Lighting — CPU-side per-vertex, ported from render_gl1.c
===================================================================*/

/* Lighting globals — set by the engine's TransExe system per object */
int render_color_blend_red   = 0;
int render_color_blend_green = 0;
int render_color_blend_blue  = 0;

int render_lighting_enabled = 0;
int render_lighting_point_lights_only = 1;
int render_lighting_use_only_light_color = 0;
int render_lighting_use_only_light_color_and_blend = 0;

int render_light_ambience = 0;
int render_light_ambience_alpha = 255.0f;

int render_lighting_env_water         = 0;
int render_lighting_env_water_level   = 0;
float render_lighting_env_water_red   = 0.0f;
float render_lighting_env_water_green = 0.0f;
float render_lighting_env_water_blue  = 0.0f;

int render_lighting_env_whiteout = 0;

void render_reset_lighting_variables(void)
{
	render_color_blend_red   = 0;
	render_color_blend_green = 0;
	render_color_blend_blue  = 0;
	render_lighting_enabled = 0;
	render_lighting_point_lights_only = 1;
	render_lighting_use_only_light_color = 0;
	render_lighting_use_only_light_color_and_blend = 0;
	render_light_ambience = 0;
	render_light_ambience_alpha = 255.0f;
	render_lighting_env_water         = 0;
	render_lighting_env_water_level   = 0;
	render_lighting_env_water_red   = 0.0f;
	render_lighting_env_water_green = 0.0f;
	render_lighting_env_water_blue  = 0.0f;
	render_lighting_env_whiteout = 0;
}

void do_water_effect(VECTOR *pos, COLOR *color)
{
	u_int32_t r, g, b;
	int x, y, z;
	float intensity, seconds;
	static float speed = 71.0f;
	if (render_lighting_env_water == 2 && pos->y >= render_lighting_env_water_level)
		return;
	r = color[2] >> 2;
	g = color[1] >> 2;
	b = color[0] >> 2;
	x = (float)((int)(pos->x * 0.35f) % 360);
	y = (float)((int)(pos->y * 0.35f) % 360);
	z = (float)((int)(pos->z * 0.35f) % 360);
	seconds = SDL_GetTicks() / 1000.0f;
	intensity = (float)(
		(fast_sinf(D2R(x + seconds * speed)) +
		 fast_sinf(D2R(y + seconds * speed)) +
		 fast_sinf(D2R(z + seconds * speed))
		) * 127.0f * 0.3333333f + 128.0f
	);
	r += render_lighting_env_water_red   * intensity;
	g += render_lighting_env_water_green * intensity;
	b += render_lighting_env_water_blue  * intensity;
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	color[2] = (u_int8_t)r;
	color[1] = (u_int8_t)g;
	color[0] = (u_int8_t)b;
}

void do_whiteout_effect(VECTOR *pos, COLOR *color)
{
	int x, y, z, intensity;
	float seconds;
	static float speed = 71.0f;
	x = (float)((int)(pos->x * 0.35f) % 360);
	y = (float)((int)(pos->y * 0.35f) % 360);
	z = (float)((int)(pos->z * 0.35f) % 360);
	seconds = SDL_GetTicks() / 1000.0f;
	intensity = (int)(
		(fast_sinf(D2R(x + seconds * speed)) +
		 fast_sinf(D2R(y + seconds * speed)) +
		 fast_sinf(D2R(z + seconds * speed))
		) * 127.0f * 0.3333333f + 128.0f
	);
	intensity += render_lighting_env_whiteout;
	if (intensity > 255) intensity = 255;
	*color &= 0xffff;
	*color |= (intensity << 24) + (intensity << 16);
}

extern XLIGHT *FirstLightVisible;

void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A)
{
	VECTOR ray;
	float rlen, rlen2, lsize2, intensity, cosa, cosarc2;
	XLIGHT *LightPnt = FirstLightVisible;

	*R = *G = *B = render_light_ambience;
	*A = render_light_ambience_alpha;

	while (LightPnt)
	{
		ray.x = Pos->x - LightPnt->Pos.x;
		ray.y = Pos->y - LightPnt->Pos.y;
		ray.z = Pos->z - LightPnt->Pos.z;

		rlen2 = ray.x * ray.x + ray.y * ray.y + ray.z * ray.z;
		lsize2 = LightPnt->Size * LightPnt->Size;

		if (rlen2 < lsize2)
		{
			if (render_lighting_point_lights_only || LightPnt->Type == POINT_LIGHT)
			{
				intensity = 1.0f - rlen2 / (int)lsize2;
			}
			else if (LightPnt->Type == SPOT_LIGHT)
			{
				if (rlen2 > 0.0f)
				{
					rlen = sqrtf(rlen2);
					ray.x /= rlen;
					ray.y /= rlen;
					ray.z /= rlen;
				}
				cosa = ray.x * LightPnt->Dir.x +
				       ray.y * LightPnt->Dir.y +
				       ray.z * LightPnt->Dir.z;

				if (rlen2 > lsize2 * 0.5f)
				{
					if (cosa > LightPnt->CosArc)
						intensity = ((lsize2 - rlen2) / (0.75f * lsize2)) *
						            ((cosa - LightPnt->CosArc) / (1.0f - LightPnt->CosArc));
					else
						goto NEXT_LIGHT;
				}
				else if (rlen2 > MIN_LIGHT_SIZE)
				{
					cosarc2 = LightPnt->CosArc *
						(1.0f - (lsize2 * 0.5f - rlen2) / (lsize2 * 0.5f - MIN_LIGHT_SIZE));
					if (cosa > cosarc2)
						intensity = ((lsize2 - rlen2) / (lsize2 - MIN_LIGHT_SIZE)) *
						            ((cosa - cosarc2) / (1.0f - cosarc2));
					else
						goto NEXT_LIGHT;
				}
				else
				{
					intensity = (cosa > 0.0f) ? 1.0f : 1.0f + cosa;
				}
			}
			else
				goto NEXT_LIGHT;

			*R += LightPnt->r * intensity;
			*G += LightPnt->g * intensity;
			*B += LightPnt->b * intensity;
			*A += 255.0f * intensity;
		}

NEXT_LIGHT:
		LightPnt = LightPnt->NextVisible;
	}

	if (*R > 255.0f) *R = 255.0f;
	if (*G > 255.0f) *G = 255.0f;
	if (*B > 255.0f) *B = 255.0f;
	if (*A > 255.0f) *A = 255.0f;
}

#define MINUS(X, Y) \
	tmp = X; tmp -= Y; \
	X = (tmp < 0) ? 0 : (tmp > 255) ? 255 : tmp

#define ADD(X, Y) MINUS(X, -Y)

#define MIX_COLOR_BLEND_LIGHT(COLOR, BLEND, LIGHT) \
	ADD(COLOR, LIGHT); MINUS(COLOR, BLEND)

void light_vert(LVERTEX *vert, u_int8_t *color)
{
	int tmp;
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
	VECTOR world, v = { vert->x, vert->y, vert->z };
	MxV(&world_matrix, &v, &world);
	if (render_lighting_env_whiteout)
		do_whiteout_effect(&world, color);
	else if (render_lighting_env_water)
		do_water_effect(&world, color);
#ifndef LIGHT_EVERYTHING
	if (render_lighting_enabled)
#endif
		GetRealLightAmbientWorldSpace(&world, &r, &g, &b, &a);
	if (render_lighting_use_only_light_color)
	{
		color[0] = b;
		color[1] = g;
		color[2] = r;
		color[3] = a;
	}
	else if (render_lighting_use_only_light_color_and_blend)
	{
		color[0] = b;
		color[1] = g;
		color[2] = r;
		color[3] = a;
		ADD(color[0], render_color_blend_blue);
		ADD(color[1], render_color_blend_green);
		ADD(color[2], render_color_blend_red);
	}
	else
	{
		MIX_COLOR_BLEND_LIGHT(color[0], render_color_blend_blue,  b);
		MIX_COLOR_BLEND_LIGHT(color[1], render_color_blend_green, g);
		MIX_COLOR_BLEND_LIGHT(color[2], render_color_blend_red,   r);
	}
}

/*===================================================================
	Draw functions — GL-based via picaGL (proves pipeline correctness)
	TODO: replace with raw citro3d C3D_DrawArrays once visual output confirmed
===================================================================*/

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;

	/* Single-pass stereo: on the first draw call of the second eye,
	 * replay the entire display list with the updated view matrix
	 * (the engine has now set up the right eye camera).  Then skip
	 * all subsequent draw calls for this pass. */
	if (s_dlReplay)
	{
		if (s_dlCount > 0)
		{
			replay_display_list();
			s_dlCount = 0;  /* prevent re-replay */
		}
		return true;
	}

#if defined(VERBOSE_TRACE) && defined(__3DS__)
	/* Draw-side detector for the portal black-void bug: log when a draw
	 * is rejected here — most commonly because lpVertexBuffer is NULL,
	 * which would explain the "next room geometry doesn't render" symptom
	 * when an earlier load step (or memory overflow into this struct)
	 * clobbered the buffer pointer. Once-per-second per failure path. */
	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer || !s_scratch)
	{
		static int s_rej_count = 0;
		s_rej_count++;
		if ((s_rej_count % 60) == 1) {
			extern void trace(const char*);
			char _b[160];
			snprintf(_b, sizeof(_b),
			         "DRAW_REJ: shaderReady=%d renderObject=%p lpVertex=%p s_scratch=%p numTG=%d (rej#%d)",
			         (int)s_shaderReady, (void*)renderObject,
			         renderObject ? renderObject->lpVertexBuffer : NULL,
			         (void*)s_scratch,
			         renderObject ? renderObject->numTextureGroups : -1,
			         s_rej_count);
			trace(_b);
		}
		return false;
	}
#else
	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer || !s_scratch)
		return false;
#endif

	if (!orthographic)
	{
		upload_matrices();
	}
	else
	{
		C3D_Mtx ortho, identity;
		Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
			render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
		Mtx_Identity(&identity);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
	}

	/* ---- Texture sorting ----
	 * Sort texture groups by texture pointer so consecutive groups share
	 * the same texture, maximizing draw call batching below.  Uses a
	 * local index array (not modifying the renderObject) and insertion
	 * sort — fast for the small N (max 64 on 3DS). */
	int sorted_order[MAX_TEXTURE_GROUPS];
	int numGroups = renderObject->numTextureGroups;
	{
		int si, sj;
		for (si = 0; si < numGroups; si++)
			sorted_order[si] = si;
		for (si = 1; si < numGroups; si++)
		{
			int key = sorted_order[si];
			void *key_tex = (void*)renderObject->textureGroups[key].texture;
			sj = si - 1;
			while (sj >= 0 &&
			       (void*)renderObject->textureGroups[sorted_order[sj]].texture > key_tex)
			{
				sorted_order[sj + 1] = sorted_order[sj];
				sj--;
			}
			sorted_order[sj + 1] = key;
		}
	}

	/* ---- Draw call batching ----
	 * Accumulate consecutive texture groups with the same GPU state
	 * (texture, colourkey, blend) into a single C3D_DrawArrays call.
	 * Vertices are contiguous in the scratch buffer so batching just
	 * extends the vertex count without issuing intermediate draws. */
	int   batch_startOffset = 0;
	int   batch_vertCount   = 0;
	void *batch_texture     = NULL;
	bool  batch_colourkey   = false;
	bool  batch_hasTexture  = false;

/* Macro: flush the current batch (draw + display list + state restore) */
#define FLUSH_BATCH() do { \
	if (batch_vertCount > 0) { \
		C3D_BufInfo *_bi = C3D_GetBufInfo(); \
		BufInfo_Init(_bi); \
		BufInfo_Add(_bi, s_scratch + batch_startOffset, sizeof(gpu_vertex_t), 3, 0x210); \
		C3D_DrawArrays(GPU_TRIANGLES, 0, batch_vertCount); \
		if (s_dlRecording && s_dlCount < MAX_DL_ENTRIES) { \
			dl_entry_t *_e = &s_dl[s_dlCount++]; \
			_e->scratchOffset = batch_startOffset; \
			_e->vertexCount   = batch_vertCount; \
			memmove(&_e->worldMatrix, &world_matrix, sizeof(MATRIX)); \
			memmove(&_e->projMatrix,  &proj_matrix,  sizeof(MATRIX)); \
			_e->viewport       = s_viewport; \
			_e->texture        = batch_texture; \
			_e->colourkey      = batch_colourkey; \
			_e->orthographic   = orthographic; \
			_e->additive_blend = _additive_blend_active; \
			_e->has_texture    = batch_hasTexture; \
		} \
		if (batch_colourkey) \
			C3D_AlphaTest(false, GPU_ALWAYS, 0); \
		if (batch_hasTexture && !_additive_blend_active) \
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, \
				GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO); \
		batch_vertCount = 0; \
	} \
} while(0)

	for (group = 0; group < numGroups; group++)
	{
		TEXTUREGROUP *tg = &renderObject->textureGroups[sorted_order[group]];
		int numIndices = tg->numTriangles * 3;
		int startVert = tg->startVert;
		int totalVerts, i;
		gpu_vertex_t *dst;

		if (numIndices <= 0) continue;

		/* Mid-frame flush: if scratch is nearly full, flush pending batch
		 * and split the frame to reset the scratch pointer. */
		if (s_scratchUsed + numIndices > s_scratchMax)
		{
			if (s_inFrame)
			{
				FLUSH_BATCH();
				C3D_FrameSplit(0);
				s_scratchUsed = 0;
				C3D_BindProgram(&s_shaderProgram);
				{
					C3D_AttrInfo *ai = C3D_GetAttrInfo();
					AttrInfo_Init(ai);
					AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
					AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
					AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
				}
				s_lastBoundTexture = NULL;
				s_lastTexEnvTextured = false;
				if (!orthographic)
					upload_matrices();
				else
				{
					C3D_Mtx ortho, identity;
					Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
						render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
					Mtx_Identity(&identity);
					C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
					C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
				}
			}
			if (s_scratchUsed + numIndices > s_scratchMax)
				continue;
		}

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

		/* Determine effective texture state for this group */
		bool eff_hasTexture = false;
		if (tg->texture)
		{
			texture_t *texdata = (texture_t*)tg->texture;
			if (texdata->initialized)
				eff_hasTexture = true;
		}

		/* Check if we need to break the batch — state changed */
		if (batch_vertCount > 0 &&
		    ((void*)tg->texture != batch_texture ||
		     tg->colourkey      != batch_colourkey ||
		     eff_hasTexture     != batch_hasTexture))
		{
			FLUSH_BATCH();
		}

		/* Start a new batch if needed — set GPU state */
		if (batch_vertCount == 0)
		{
			batch_startOffset = s_scratchUsed;
			batch_texture     = (void*)tg->texture;
			batch_colourkey   = tg->colourkey;
			batch_hasTexture  = eff_hasTexture;

			if (tg->colourkey)
				C3D_AlphaTest(true, GPU_GREATER, 0x64);
			if (eff_hasTexture && !_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

			/* Texture binding with TexEnv cache */
			if (eff_hasTexture)
			{
				texture_t *texdata = (texture_t*)tg->texture;
				if ((void*)texdata != s_lastBoundTexture || !s_lastTexEnvTextured)
				{
					C3D_TexBind(0, &texdata->tex);
					C3D_TexEnv *env = C3D_GetTexEnv(0);
					C3D_TexEnvInit(env);
					C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
					C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
					s_lastBoundTexture = (void*)texdata;
					s_lastTexEnvTextured = true;
				}
				else
				{
					C3D_TexBind(0, &texdata->tex);
				}
			}
			else if (s_lastTexEnvTextured)
			{
				C3D_TexEnv *env = C3D_GetTexEnv(0);
				C3D_TexEnvInit(env);
				C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
				s_lastBoundTexture = NULL;
				s_lastTexEnvTextured = false;
			}
		}

		/* Accumulate into current batch */
		batch_vertCount += totalVerts;
		s_scratchUsed += totalVerts;
	}

	/* Flush the final pending batch */
	FLUSH_BATCH();

#undef FLUSH_BATCH

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
