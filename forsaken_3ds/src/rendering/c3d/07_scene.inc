/* ============================================================================
 * 07_scene.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Render init / cleanup / flip, scene begin / end and GPU state
 * management.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

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
	/* C3D_Init already called by c3d_context_init() stub */
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
	/* Cleanup handled by c3d_context_exit() stub */
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
	 * Function retained because shared code in oct2.cpp calls it. */
	(void)red; (void)green; (void)blue;
}

bool render_flip(render_info_t *info)
{
	/* platform_render_present calls our c3d_present which calls
	 * C3D_FrameEnd if a frame is active */
	platform_render_present(info);
	return true;
}

const char *render_error_description(int e)
{
	(void)e;
	return nullptr;
}

/*===================================================================
	Scene begin / end
===================================================================*/


bool FSBeginScene(void)
{
	static int s_first_begin_scene = 1;
	if (s_first_begin_scene) {
		s_first_begin_scene = 0;
	}
	/* Single-pass stereo vertex memo (STEREO_REPLAY): the left eye
	 * records converted ranges keyed by stable batch identity; the
	 * right eye reuses them with its own live uniforms and draw
	 * order. The old whole-list replay (replay_display_list +
	 * draw-skipping) is gone: it dropped inset/HUD draws on one eye
	 * and broke on per-eye culling differences. Here no draw is ever
	 * skipped — only the vertex-conversion memcpy, on memo hits. */
	smemo_begin_frame();
	/* Gated on 3DS stereo: mono frames must not pay the record cost. */
	/* A frame is hardware stereo only while the slider is up
	 * (stereo_enabled, refreshed every frame) and the technique is 3DS
	 * stereo. stereo_mode alone is the saved technique choice: it stays
	 * STEREO_MODE_3DS after the slider goes down, which used to keep the
	 * mono persistent cache off for good. The slider is read after this
	 * point, so the gate lags a transition by one frame; both caches are
	 * correct in either kind of frame, only the cost differs. */
	const bool stereo_frame = render_info.stereo_enabled &&
		render_info.stereo_mode == STEREO_MODE_3DS;
	s_memoMode = stereo_frame ? SMEMO_RECORD : SMEMO_OFF;
	s_memoSuspended = false;
	/* Slider gate: mono only (see s_persistActive comment). */
	s_persistActive = !stereo_frame;
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
		s_lastBoundTexture = nullptr;
		s_lastTexEnvTextured = false;
		s_lastTexEnvMode = TEXENV_MODE_NONE;
		s_lastMatClass = -1;
		s_lastPhaseBucket = -1;

		/* Initialize modelView uniform (register 4) to identity, so
		 * draws before the first upload_matrices (which writes
		 * world_matrix there) see no model transform. */
		/* Write identity to modelView uniform (register 4) via GPUCMD */
		{
			float id[16] = {
				1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
			};
			GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | UNIFORM_MODELVIEW);
			GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)id, 16);
		}

#if defined(GPU_LIGHTING)
		c3d_clear_xlights();
#endif

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
	GPU_WRITEMASK mask = (GPU_WRITEMASK)(s_colorMask & ~GPU_WRITE_DEPTH);
	C3D_DepthTest(true, GPU_LESS, mask);
}

void disable_zbuff(void)
{
	GPU_WRITEMASK mask = (GPU_WRITEMASK)(s_colorMask & ~GPU_WRITE_DEPTH);
	C3D_DepthTest(false, GPU_ALWAYS, mask);
}

void cull_none(void)
{
	C3D_CullFace(GPU_CULL_NONE);
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
