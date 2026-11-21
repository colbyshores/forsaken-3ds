/* ============================================================================
 * 08_viewport.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Clears (including the scissored quad clear), viewports and the matrix
 * state calls.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

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
	if (!g_scratch.scratch || g_scratch.scratchUsed + 6 > g_scratch.scratchMax) return;

	gpu_vertex_t *dst = g_scratch.scratch + g_scratch.scratchUsed;
	g_scratch.scratchUsed += 6;

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

	bind_vertex_buffer(dst);
	C3D_DrawArrays(GPU_TRIANGLES, 0, 6);

	/* Restore standard game-draw state */
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);

	/* Quad clear writes TexEnv stage 0 directly without going through
	 * g_texenv.apply_*. Invalidate the TexEnv cache so the next textured
	 * draw re-runs configure_texenv_* — otherwise the cache lies and
	 * subsequent walls render untextured (chase-cam regression). */
	g_texenv.lastTexEnvMode     = TEXENV_MODE_NONE;
	g_texenv.lastBoundTexture   = nullptr;
	g_texenv.lastTexEnvTextured = false;
	g_texenv.lastMatClass       = -1;
	g_texenv.lastPhaseBucket    = -1;
}

bool FSClear(XYRECT *rect)
{
	(void)rect;
	if (!g_frame.targetCurrent) return true;

	/* Sub-viewports (missile cam, PIP, rear view) render AFTER the main
	 * camera. Rasterized quad clear scissored to viewport via the
	 * rasterizer — wipes color + depth inside just the inset rect.
	 * See clear_sub_viewport_via_quad comment for why. */
	if (CameraRendering != CamRendering::None &&
		CameraRendering != CamRendering::Main)
	{
		clear_sub_viewport_via_quad();
		return true;
	}

	C3D_RenderTargetClear(g_frame.targetCurrent, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	return true;
}

bool FSClearBlack(void)
{
	if (!g_frame.targetCurrent) return true;
	C3D_RenderTargetClear(g_frame.targetCurrent, C3D_CLEAR_COLOR, 0x000000FF, 0);
	return true;
}

bool FSClearDepth(XYRECT *rect)
{
	(void)rect;
	if (g_frame.targetCurrent)
		C3D_RenderTargetClear(g_frame.targetCurrent, C3D_CLEAR_DEPTH, 0, 0xFFFFFF);
	return true;
}

/*===================================================================
	Viewport
===================================================================*/

bool FSGetViewPort(render_viewport_t *view)
{
	*view = g_frame.viewport;
	return true;
}

void FSSetNextViewPortScissorMode(bool scissor_mode)
{
	g_frame.next_view_use_scissor = scissor_mode;
}

bool FSSetViewPort(render_viewport_t *view)
{
	g_frame.viewport = *view;

	/* Translate game-space sub-rect into PICA portrait coordinates.
	 * The engine viewport is 400×240 landscape; PICA top-screen target
	 * is 240×400 portrait (Y is across the long edge after the 90°
	 * rotation). Compute the same x/y/w/h pair regardless of which
	 * dispatch path we take below. */
	int bottom = (int)render_info.ThisMode.h - (int)(view->Y + view->Height);
	int gl_x = (int)view->X;
	int pica_x = bottom < 0 ? 0 : bottom;
	int pica_y = (400 - (int)view->Width) - gl_x;
	if (pica_y < 0) pica_y = 0;
	int pica_w = (int)view->Height;
	int pica_h = (int)view->Width;

	bool use_scissor = g_frame.next_view_use_scissor;
	g_frame.next_view_use_scissor = false;	/* one-shot — reset after consume */

	g_frame.current_view_used_scissor = use_scissor;
	if (use_scissor)
	{
		/* Per-group draws (level-mesh visibility groups). Full-screen
		 * GPU viewport + scissor to the sub-rect, paired with the
		 * unmagnified cam->Proj that visi.cpp uploads. Every visible
		 * group shares the same projection — eliminates the per-group
		 * magnified-projection flicker / cracks at portal boundaries
		 * that PICA200 produced under the sub-rect-viewport path. */
		C3D_SetViewport(0, 0, 240, 400);
		C3D_SetScissor(GPU_SCISSOR_NORMAL,
					   (u32)pica_x, (u32)pica_y,
					   (u32)(pica_x + pica_w), (u32)(pica_y + pica_h));
	}
	else
	{
		/* HUD / 2D / orthographic dispatch — sub-rect viewport, no
		 * scissor. Used for non-3D-mesh draws which don't go through
		 * visi.cpp's per-group projection setup. */
		C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
		C3D_SetViewport((u32)pica_x, (u32)pica_y,
						(u32)pica_w, (u32)pica_h);
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
