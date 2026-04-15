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
 *
 * Phase 1: skeleton implementing the same FS* interface as render_gl1.c
 */

#ifdef RENDERER_C3D

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "networking.h"
#include "render.h"
#include "render_gl_shared.h"
#include "lights.h"
#include "platform.h"

/* ---- shader binary (compiled from shaders/render_c3d.v.pica) ---- */
#include "render_c3d_shbin.h"

/* ---- uniform register locations (must match .v.pica) ---- */
#define UNIFORM_PROJECTION  0
#define UNIFORM_MODELVIEW   4

/* ---- citro3d state ---- */

static DVLB_s         *s_shaderDVLB    = NULL;
static shaderProgram_s s_shaderProgram;
static bool            s_shaderReady   = false;
static C3D_Mtx         s_projection;
static C3D_Mtx         s_modelview;

/* Render targets for stereo */
static C3D_RenderTarget *s_targetLeft  = NULL;
static C3D_RenderTarget *s_targetRight = NULL;
static C3D_RenderTarget *s_targetBot   = NULL;

/* ---- vertex attribute layout ----
 *
 * LVERTEX:   { float x,y,z; COLOR color; float tu,tv; }  = 20 bytes
 * TLVERTEX:  { float x,y,z,w; COLOR color; float tu,tv; } = 24 bytes
 *
 * We define two attribute configurations: one for 3D (LVERTEX) and
 * one for 2D pretransformed (TLVERTEX).
 */
static C3D_AttrInfo s_attrInfo3D;
static C3D_AttrInfo s_attrInfo2D;

/*===================================================================
	Shader init / cleanup
===================================================================*/

bool c3d_renderer_init(void)
{
	/* Load compiled vertex shader */
	s_shaderDVLB = DVLB_ParseFile((u32*)render_c3d_shbin, render_c3d_shbin_size);
	if (!s_shaderDVLB)
	{
		DebugPrintf("c3d_renderer_init: DVLB_ParseFile failed\n");
		return false;
	}

	shaderProgramInit(&s_shaderProgram);
	if (shaderProgramSetVsh(&s_shaderProgram, &s_shaderDVLB->DVLE[0]) < 0)
	{
		DebugPrintf("c3d_renderer_init: shaderProgramSetVsh failed\n");
		return false;
	}

	/* 3D vertex layout: pos(3f) + color(4ub) + texcoord(2f) = 20 bytes */
	AttrInfo_Init(&s_attrInfo3D);
	AttrInfo_AddLoader(&s_attrInfo3D, 0, GPU_FLOAT, 3);            /* v0: position */
	AttrInfo_AddLoader(&s_attrInfo3D, 1, GPU_UNSIGNED_BYTE, 4);    /* v1: color */
	AttrInfo_AddLoader(&s_attrInfo3D, 2, GPU_FLOAT, 2);            /* v2: texcoord */

	/* 2D vertex layout: pos(4f) + color(4ub) + texcoord(2f) = 24 bytes */
	AttrInfo_Init(&s_attrInfo2D);
	AttrInfo_AddLoader(&s_attrInfo2D, 0, GPU_FLOAT, 4);            /* v0: position (x,y,z,w) */
	AttrInfo_AddLoader(&s_attrInfo2D, 1, GPU_UNSIGNED_BYTE, 4);    /* v1: color */
	AttrInfo_AddLoader(&s_attrInfo2D, 2, GPU_FLOAT, 2);            /* v2: texcoord */

	/* Create render targets */
	s_targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	s_targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	s_targetBot   = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);

	C3D_RenderTargetSetOutput(s_targetLeft,  GFX_TOP,    GFX_LEFT,
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
		GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	C3D_RenderTargetSetOutput(s_targetRight, GFX_TOP,    GFX_RIGHT,
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
		GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	C3D_RenderTargetSetOutput(s_targetBot,   GFX_BOTTOM, GFX_LEFT,
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
		GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	/* Identity matrices */
	Mtx_Identity(&s_projection);
	Mtx_Identity(&s_modelview);

	s_shaderReady = true;
	DebugPrintf("c3d_renderer_init: OK\n");
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
	Bind shader and upload current matrices
===================================================================*/

static void c3d_bind_shader(void)
{
	C3D_BindProgram(&s_shaderProgram);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &s_projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &s_modelview);
}

/*===================================================================
	Convert engine RENDERMATRIX (row-major D3D) to C3D_Mtx (citro3d)
===================================================================*/

static void matrix_to_c3d(const RENDERMATRIX *src, C3D_Mtx *dst)
{
	/* RENDERMATRIX is row-major (D3D convention).
	 * C3D_Mtx stores rows in r[0..3] as C3D_FVec (WZYX order).
	 * C3D_FVec.x/y/z/w map to indices [3],[2],[1],[0] internally,
	 * but the Mtx_* functions handle this. We use direct assignment
	 * through the m[] accessor which is [row][col] in logical order. */
	int r, c;
	for (r = 0; r < 4; r++)
		for (c = 0; c < 4; c++)
			dst->m[r][c] = src->m[r][c];
}

/*===================================================================
	FS* interface — vertex/index buffer management
	(Phase 1: identical to GL1 — malloc on CPU side.
	 Phase 2 will replace with linearAlloc for GPU-visible memory.)
===================================================================*/

bool FSCreateVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = malloc(numVertices * sizeof(LVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSCreateDynamicVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	return FSCreateVertexBuffer(renderObject, numVertices);
}

bool FSCreateNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	renderObject->lpNormalBuffer = malloc(numNormals * sizeof(NORMAL));
	return renderObject->lpNormalBuffer != NULL;
}

bool FSCreateDynamicNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	return FSCreateNormalBuffer(renderObject, numNormals);
}

bool FSCreateIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	renderObject->lpIndexBuffer = malloc(numIndices * 3 * sizeof(WORD));
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

bool FSUnlockIndexBuffer(RENDEROBJECT *renderObject)  { return true; }
bool FSUnlockVertexBuffer(RENDEROBJECT *renderObject) { return true; }

bool FSLockNormalBuffer(RENDEROBJECT *renderObject, NORMAL **normals)
{
	*normals = renderObject->lpNormalBuffer;
	return true;
}

bool FSUnlockNormalBuffer(RENDEROBJECT *renderObject) { return true; }

bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = malloc(numVertices * sizeof(TLVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSLockPretransformedVertexBuffer(RENDEROBJECT *renderObject, TLVERTEX **verts)
{
	*verts = (TLVERTEX*)renderObject->lpVertexBuffer;
	return true;
}

/*===================================================================
	Lighting (Phase 1: CPU-side, same as GL1.  Phase 6 moves to GPU.)
===================================================================*/

void render_reset_lighting_variables(void)
{
	/* TODO: reset GPU light uniforms when Phase 6 implements GPU lighting */
}

/*===================================================================
	Draw functions (Phase 1: stub — will be fleshed out in Phase 2)
===================================================================*/

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	/* Phase 1 stub: bind shader, set up buffers, issue draw calls.
	 * For now this is a minimal implementation to get the build
	 * compiling.  Phase 2 will implement the actual citro3d draw path
	 * with C3D_DrawElements. */

	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer)
		return false;

	/* TODO Phase 2:
	 * 1. c3d_bind_shader()
	 * 2. C3D_SetAttrInfo(orthographic ? &s_attrInfo2D : &s_attrInfo3D)
	 * 3. For each textureGroup:
	 *    a. Bind texture
	 *    b. Set up C3D_BufInfo pointing to renderObject->lpVertexBuffer
	 *    c. C3D_DrawElements(GPU_TRIANGLES, ...)
	 */

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
	/* Lines use GPU_LINES primitive instead of GPU_TRIANGLES */
	return draw_render_object(renderObject, 1, false);
}

/*===================================================================
	Water / whiteout effects (CPU-side vertex color modification)
===================================================================*/

void do_water_effect(VECTOR *pos, COLOR *color)
{
	/* Same as GL1 — modify vertex color based on water depth */
	/* TODO: copy from render_gl1.c */
}

void do_whiteout_effect(VECTOR *pos, COLOR *color)
{
	/* Same as GL1 — flash screen white on damage */
	/* TODO: copy from render_gl1.c */
}

void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A)
{
	/* TODO: copy from render_gl1.c */
	*R = *G = *B = 1.0f;
	*A = 1.0f;
}

void light_vert(LVERTEX *vert, u_int8_t *color)
{
	/* Phase 1: no lighting.  Phase 6 moves this to vertex shader. */
	(void)vert;
	(void)color;
}

#endif /* RENDERER_C3D */
