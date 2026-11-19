/* ============================================================================
 * 04_hud_screen.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * HUD bottom-screen redirection, the level-cache release
 * (c3d_release_level_cache) and the stereo eye transfer (c3d_transfer_eye).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	HUD bottom-screen redirection
	-------------------------------------------------------------------
	The gameplay HUD (ammo / weapons / shield / messages) is rendered
	on the mono bottom screen instead of overlaying the stereo top
	screen. c3d_hud_begin_bottom() stashes the current top-screen target,
	clears the bottom target, switches C3D's draw target to it, and
	returns true if the caller should actually issue draws. It returns
	false on the second eye's pass so we only draw the HUD once per
	frame (bottom is mono — one copy is all we need).
===================================================================*/
bool c3d_hud_begin_bottom(void)
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

	/* Re-bind shader state for the new target (matches c3d_transfer_eye). */
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
void c3d_clear_bottom_screen(void)
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

/* Level release: the persistent cache's buffers belong to the level
 * being released. Wait until the GPU has finished every queued draw
 * that may still read them (an empty SYNCDRAW frame waits for the
 * queue, as c3d_clear_bottom_screen does), then free them, so the next
 * level loads with that linear memory back.
 *
 * Not when the app is closing (HOME -> Close Software, power off): the
 * HOME menu holds the GPU by then, so the empty frame never completes and
 * teardown hung there with the music still playing. Nothing of ours will
 * draw again, so the buffers are freed without waiting. */
void c3d_release_level_cache(void)
{
	if (s_persistBytes == 0)
		return;
	if (!aptShouldClose())
	{
		if (s_inFrame)
		{
			C3D_FrameEnd(0);
			s_inFrame = false;
		}
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_FrameEnd(0);
	}
	persist_free_all();
}

void c3d_hud_end_bottom(void)
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

void c3d_transfer_eye(unsigned int eye)
{
	if (eye == GFX_LEFT)
	{
		/* Left eye finished. Switch the memo from record to lookup:
		 * the right eye reuses recorded ranges with its own live
		 * uniforms and draw order. */
		if (s_memoMode == SMEMO_RECORD)
			s_memoMode = SMEMO_LOOKUP;

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

		/* No trigger needed: the memo is keyed per batch, so every
		 * right-eye draw looks its own range up (hit = resubmit,
		 * miss = convert live). No draw is ever skipped, so inset,
		 * HUD and post-stereo draws are unaffected by construction.
		 * Default (flag off): s_memoMode stays OFF, both eyes run
		 * fully, exactly as before. */
	}
	if (eye == GFX_RIGHT)
	{
		/* Right eye done. Disarm the memo; post-stereo rendering
		 * (missile camera, rear view, HUD overlays) draws normally. */
		s_memoMode = SMEMO_OFF;
	}
	s_stereoFrame = true;
}

/* Gamma table for texture loading */
u_int8_t gamma_table[256];
