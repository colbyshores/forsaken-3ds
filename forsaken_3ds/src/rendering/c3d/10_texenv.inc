/* ============================================================================
 * 10_texenv.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * The TexEnv combiner pipeline: DIFFUSE, AUX (ProcTex wall grain) and
 * OBJECT (Phong shine) modes.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	TexEnv combiner pipeline.

	Three modes the renderer can put the GPU into per draw:

	  TEXENV_MODE_NONE     — untextured (HUD vertex-color paths).
	  TEXENV_MODE_DIFFUSE  — single-stage modulate: TEX0 × PRIMARY_COLOR.
							 Used for textured draws that get neither
							 of the modes below.
	  TEXENV_MODE_AUX      — walls: TEX0 × PRIMARY, then ADD_SIGNED the
							 ProcTex output (TEXTURE3), a noise palette
							 centred on grey, so the grain both
							 brightens and darkens without washing out.
	  TEXENV_MODE_OBJECT   — models: TEX0 × PRIMARY, then ADD the
							 fragment-light specular
							 (FRAGMENT_SECONDARY_COLOR).

	Cached per-mode + per-texdata pointer so identical adjacent draws
	skip GPU state changes.
===================================================================*/

static void configure_texenv_diffuse(void)
{
	/* Stage 0: TEXTURE0 * PRIMARY_COLOR (vertex color carries baked
	 * lighting + per-vertex GPU light contribution). Stages 1+2
	 * always reset — across-frame state could otherwise leak the
	 * prior frame's AUX/OBJECT stage config since lastTexEnvMode
	 * resets to NONE at frame start (not OBJECT/AUX), so a
	 * conditional reset wouldn't fire. */
	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
				  GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	C3D_TexEnvInit(C3D_GetTexEnv(1));
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

static void configure_texenv_none(void)
{
	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR,
				  GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_TexEnvInit(C3D_GetTexEnv(1));
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

static void configure_texenv_aux(texture_t *texdata)
{
	/* Wall detail via ProcTex unit. Stage 0 is the legacy diffuse-
	 * modulate path so walls keep proper color + baked lighting;
	 * stage 1 ADD_SIGNEDs the procedural noise (TEXTURE3 = proctex
	 * output). The proctex configuration was selected per material
	 * class by the caller binding s_wallProcTex[texdata->material_class].
	 * Alpha REPLACE preserves stage 0's alpha so colour-keyed
	 * transparent textures still work. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(0);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
					  GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}

	{
		C3D_TexEnv *env = C3D_GetTexEnv(1);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE3,
					  GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_RGB,   GPU_ADD_SIGNED);
		C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
	}
	(void)texdata;
}

static void configure_texenv_object(void)
{
	/* Bind the object Phong light env so fragment-lighting hardware
	 * computes FRAGMENT_SECONDARY_COLOR (specular highlight) using
	 * the per-vertex outnq quat from the vshader. */
	C3D_LightEnvBind(&s_objLightEnv);

	/* Stage 0: legacy TEX0 × PRIMARY — proper model color + baked
	 * lighting + GPU vertex-light contribution preserved. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(0);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
					  GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}

	/* Stage 1: + FRAGMENT_SECONDARY_COLOR (Phong specular). The
	 * hardware computes this from the per-vertex normal-quat × the
	 * scene light × the material specular coefficient. Adds a moving
	 * highlight as the camera angle changes. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(1);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS,
					  GPU_FRAGMENT_SECONDARY_COLOR, (GPU_TEVSRC)0);
		C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);
	}

	/* Stage 2 left empty. A ProcTex grain stage (ADD_SIGNED, as in AUX
	 * mode) could go here; it was dropped to keep models to the shine
	 * alone. */
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

/* Apply the right TexEnv mode for a textured draw, given the bound
 * texture handle. Caches by mode + texdata pointer (+ ProcTex bucket in
 * AUX mode) to skip redundant GPU state changes within a batch. */
static void apply_texenv_for_texture(texture_t *texdata)
{
	/* OBJECT (Phong shine on enemies / pickups) takes precedence over
	 * AUX (wall ProcTex grain) — the same texture would never want
	 * both, but the object toggle wraps narrow draw_object call sites
	 * while AUX is the fall-through path for level-mesh draws. */
	texenv_mode_t mode;
	if (s_objProcTexOn && s_objLightReady && g_object_shine &&
		s_objShineEligible)
		mode = TEXENV_MODE_OBJECT;
	else if (s_procTexReady && g_wall_detail && !s_isOrthoDraw &&
			 !s_skipAuxProcTex)
		mode = TEXENV_MODE_AUX;
	else
		mode = TEXENV_MODE_DIFFUSE;

	/* Compute desired ProcTex state up front so the cache key includes
	 * the per-mesh atlas bucket. Two adjacent meshes sharing the
	 * same texture but different atlas seeds will land in different
	 * buckets and force a (cheap) NoiseCoefs rewrite — the color LUT
	 * stays cached across the bucket change. */
	material_class_t desired_mc = MAT_METAL;
	int desired_bucket = -1;
	if (mode == TEXENV_MODE_AUX)
	{
		desired_mc = (material_class_t)texdata->material_class;
		if ((unsigned)desired_mc >= MAT_COUNT) desired_mc = MAT_METAL;
		float seed_phase =
			((s_atlasSeed >> 8) & 0xFFFF) * (1.0f / 65536.0f);
		float combined = texdata->proctex_phase + seed_phase;
		combined -= fast_floorf(combined);
		desired_bucket = (int)(combined * (float)ATLAS_PHASE_BUCKETS);
		if (desired_bucket >= ATLAS_PHASE_BUCKETS)
			desired_bucket = ATLAS_PHASE_BUCKETS - 1;
	}

	bool mode_changed = (mode != s_lastTexEnvMode) || !s_lastTexEnvTextured;
	bool aux_state_changed = (mode == TEXENV_MODE_AUX) &&
		(desired_bucket != s_lastPhaseBucket ||
		 (int)desired_mc != s_lastMatClass);

	/* Cache check — same mode + same texture + (for AUX) same atlas
	 * bucket means no GPU state change needed. */
	if (!mode_changed && !aux_state_changed &&
		s_lastBoundTexture == (void*)texdata)
		return;

	/* R08: bind only on a cache miss. The early-return path provably
	 * already has this texture bound, so binding there was a redundant
	 * GPU register write per cached call. */
	C3D_TexBind(0, &texdata->tex);

	if (mode == TEXENV_MODE_AUX && (mode_changed || aux_state_changed))
	{
		/* Cheap path when only the bucket changed: rewrite NoiseCoefs +
		 * ProcTexBind. The color LUT (256 entries) stays cached unless
		 * the material class changed. */
		material_class_t mc = desired_mc;
		float base_freq = (mc == MAT_ROCK)    ? 13.0f
						: (mc == MAT_ORGANIC) ? 14.0f
											  : 16.0f;
		float amp       = (mc == MAT_ROCK)    ? 0.31f
						: (mc == MAT_ORGANIC) ? 0.27f
											  : 0.25f;
		float phase = ((float)desired_bucket + 0.5f)
					* (1.0f / (float)ATLAS_PHASE_BUCKETS);
		/* Use the low 3 bits of the bucket as an orthogonal frequency
		 * axis — same phase value × 8 different frequencies = 8× the
		 * visible variants per phase position. Combined with 64 phase
		 * buckets this is effectively 64 distinct surface "looks" per
		 * material class. */
		int   freq_step = desired_bucket & 7;          /* 0..7 */
		float freq      = base_freq * (1.0f + freq_step * 0.06f);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[mc],
			C3D_ProcTex_UV, amp, freq, phase);
		C3D_ProcTexBind(0, &s_wallProcTex[mc]);
		if (mode_changed || (int)mc != s_lastMatClass)
			C3D_ProcTexColorLutBind(&s_wallProcColor[mc]);
		s_lastMatClass    = (int)mc;
		s_lastPhaseBucket = desired_bucket;
	}

	if (mode_changed)
	{
		if (mode == TEXENV_MODE_AUX)
			configure_texenv_aux(texdata);
		else if (mode == TEXENV_MODE_OBJECT)
			configure_texenv_object();
		else
			configure_texenv_diffuse();
	}

	s_lastTexEnvMode      = mode;
	s_lastBoundTexture    = (void*)texdata;
	s_lastTexEnvTextured  = true;
}

static void apply_texenv_untextured(void)
{
	if (s_lastTexEnvMode == TEXENV_MODE_NONE && !s_lastTexEnvTextured)
		return;
	configure_texenv_none();
	s_lastTexEnvMode     = TEXENV_MODE_NONE;
	s_lastBoundTexture   = nullptr;
	s_lastTexEnvTextured = false;
}
