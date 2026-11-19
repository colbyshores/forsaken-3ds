/* ============================================================================
 * 05_shader.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * The gamma table shared with the texture loader, and shader / render
 * target / ProcTex / fragment-light init and cleanup.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Gamma table (shared with texture loader)
===================================================================*/

/* @vfp-exempt R6 — one-shot gamma table build at init, not a per-frame path;
   pow output IS the table. measured n/a. Verified 2026-10-25. */
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
	if (s_shaderReady) {
		return true;
	}
	s_shaderDVLB = DVLB_ParseFile((u32*)render_c3d_shbin, render_c3d_shbin_len);
	if (!s_shaderDVLB)
	{
		return false;
	}

	shaderProgramInit(&s_shaderProgram);
	if (shaderProgramSetVsh(&s_shaderProgram, &s_shaderDVLB->DVLE[0]) < 0) {
		return false;
	}

	/* Allocate scratch buffer for vertex conversion */
	s_scratch = (gpu_vertex_t*)linearAlloc(GPU_SCRATCH_SIZE);
	if (!s_scratch) {
		return false;
	}
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
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetLeft, GFX_TOP, GFX_LEFT, transferFlags);

		s_targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetRight) {
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetRight, GFX_TOP, GFX_RIGHT, transferFlags);

		/* Bottom screen (320×240, mono). Stored in GPU as 240×320 portrait
		 * like the top. Used for gameplay HUD text so the stereo 3D image
		 * above isn't cluttered. */
		s_targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetBottom) {
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetBottom, GFX_BOTTOM, GFX_LEFT, transferFlags);
	}

	/* Depth map scale 1.0, offset 1.0 (the values the earlier PICA200 GL shim used). */
	C3D_DepthMap(true, 1.0f, 1.0f);

	/* GPU-lighting uniform register IDs are hardcoded to match the
	 * declaration order in shaders/render_c3d.v.pica. libctru's
	 * shaderInstanceGetUniformLocation / DVLE_GetUniformRegister return
	 * -1 for fvec uniforms beyond projection/modelView even though they
	 * appear in the binary's symbol table — works around that. */
	s_loc_ambient     = UNIFORM_AMBIENT;
	s_loc_light_count = UNIFORM_LIGHT_COUNT;
	s_loc_lights      = UNIFORM_LIGHTS;

	if (!s_procTexReady)
	{
		/* Shared LUTs — noise smoothstep + RGB map. Both are linear
		 * identity ramps; per-class character comes from the noise
		 * coefficients and the color palette LUT, not these. Bound
		 * once at init; PICA200 keeps them globally until rebound. */
		{
			float ramp[129];
			int i;
			for (i = 0; i <= 128; i++)
				ramp[i] = (float)i / 128.0f;
			ProcTexLut_FromArray(&s_wallProcNoise, ramp);
			C3D_ProcTexLutBind(GPU_LUT_NOISE, &s_wallProcNoise);
			ProcTexLut_FromArray(&s_wallProcMap, ramp);
			C3D_ProcTexLutBind(GPU_LUT_RGBMAP, &s_wallProcMap);
		}

		/* Per-class noise coefs + color LUT. Color LUT uses 2 stops:
		 * dark stop at the low-noise end, light stop at the high-noise
		 * end. Both straddle grey 0.5 so ADD_SIGNED contributes signed
		 * symmetric per-pixel modulation. */

		/* The diffuse texture stays the dominant signal; ProcTex is a
		 * faint per-class accent that gives each material kind a hint
		 * of unique surface character. All three classes use the same
		 * narrow tonal band centred slightly below grey 0.5 so the
		 * ADD_SIGNED contribution biases toward a subtle darkening
		 * (reads as grime / wear, not as wash-out). Class differences
		 * are driven by noise frequency (pattern scale) and palette
		 * tint, both kept gentle. Magnitudes:
		 *   palette 0x70..0x82 = 0.439..0.510 → ADD_SIGNED: -6%..+1%
		 *   palette 0x70..0x8C = 0.439..0.549 → ADD_SIGNED: -6%..+5%
		 * (Ranges asymmetric on purpose — slightly more dark stops
		 * than bright, leaning the look toward "lived-in".) */

		/* All three classes use GPU_PT_RGB combiner — looks up the
		 * color LUT independently on U and V noise channels rather
		 * than via the radial sqrt(U²+V²) metric, which previously
		 * produced visible elliptical / oval blob artifacts at low
		 * frequency. RGB combiner gives uniform per-pixel speckle.
		 * Frequencies bumped to 8-12 cycles per UV face so noise is
		 * fine grain, not big blobs. NoiseCoefs phase is rewritten
		 * per draw with the texture's hash so two walls of the same
		 * material class get distinct pattern positions.
		 *
		 * Palettes still narrow (ADD_SIGNED contribution ~+/-5%) and
		 * biased slightly below grey 0.5 so noise reads as wear/
		 * grime, not wash-out. Diffuse stays the dominant signal. */

		/* "Lived in" wear pass. Palettes widened from ±~5% to ±~12%
		 * around grey 0.5 — symmetric so the ProcTex contributes equal
		 * darkening (scuffs / dirt) and brightening (highlights / wear
		 * polish) rather than only darkening (which read as fog).
		 * Frequencies bumped 30% so features are finer and more
		 * defined. Amplitudes bumped ~30% so the modulation actually
		 * registers visually rather than fading into a wash. */

		/* 4-stop stepped palettes break up the bilinear color blend
		 * that produces splotches with 2-stop ramps. The output as
		 * noise sweeps 0→1 oscillates dark→mid→light→mid instead of
		 * smoothly fading dark→light, which reads as small bumps and
		 * wear features rather than large flat blobs. Each stop stays
		 * within ±6% of grey 0.5 (ADD_SIGNED) so contribution is
		 * subtle and symmetric. */

		/* MAT_METAL — fine paint-grain + brushed sheen. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_METAL], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_METAL],
						 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_METAL],
							  C3D_ProcTex_UV, 0.25f, 16.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_METAL], false,
							GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_METAL], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_METAL], true);
		{
			u32 colors[4] = {
				0xFF727272u,  /*  -7% */
				0xFF909090u,  /*  +6% */
				0xFF787878u,  /*  -3% */
				0xFF8C8C8Cu,  /*  +5% */
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_METAL],
								  colors, 0, 4);
		}

		/* MAT_ROCK — pitted, cool/warm wear pattern. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_ROCK], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_ROCK],
						 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_ROCK],
							  C3D_ProcTex_UV, 0.31f, 13.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_ROCK], false,
							GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_ROCK], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_ROCK], true);
		{
			u32 colors[4] = {
				0xFF70757Cu,  /* cool dark */
				0xFF938D86u,  /* warm light */
				0xFF767A80u,  /* cool mid */
				0xFF8E8783u,  /* warm mid-light */
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_ROCK],
								  colors, 0, 4);
		}

		/* MAT_ORGANIC — soft, faint green tint. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_ORGANIC], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_ORGANIC],
						 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_ORGANIC],
							  C3D_ProcTex_UV, 0.27f, 14.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_ORGANIC], false,
							GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_ORGANIC], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_ORGANIC], true);
		{
			u32 colors[4] = {
				0xFF72766Eu,
				0xFF90948Cu,
				0xFF787C76u,
				0xFF8C9088u,
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_ORGANIC],
								  colors, 0, 4);
		}

		s_procTexReady = true;
	}

	if (!s_objLightReady)
	{
		C3D_LightEnvInit(&s_objLightEnv);

		/* Phong-30 LUT — tight specular highlight so the shine reads
		 * as a moving spot rather than a broad glow. */
		LightLut_Phong(&s_objLightLut, 30.0f);
		C3D_LightEnvLut(&s_objLightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN,
						false, &s_objLightLut);

		/* Material — strong specular, modest diffuse. We don't sample
		 * FRAGMENT_PRIMARY_COLOR (diffuse stays in the legacy TEX0 ×
		 * PRIMARY path), only FRAGMENT_SECONDARY_COLOR (specular). */
		{
			static const C3D_Material s_objMaterial = {
				{ 0.0f, 0.0f, 0.0f },  /* ambient — unused */
				{ 0.0f, 0.0f, 0.0f },  /* diffuse — unused */
				{ 0.6f, 0.6f, 0.6f },  /* specular0 — moving shine */
				{ 0.0f, 0.0f, 0.0f },  /* specular1 */
				{ 0.0f, 0.0f, 0.0f },  /* emission */
			};
			C3D_LightEnvMaterial(&s_objLightEnv, &s_objMaterial);
		}

		C3D_LightInit(&s_objLight, &s_objLightEnv);
		C3D_LightColor(&s_objLight, 1.0f, 1.0f, 1.0f);

		/* Positional light far up + forward — same convention as the
		 * earlier wall light. Distance falloff is dot-based via the
		 * LUT, not range-based, so absolute coords don't matter much. */
		{
			C3D_FVec pos = FVec4_New(500.0f, 500.0f, 200.0f, 1.0f);
			C3D_LightPosition(&s_objLight, &pos);
		}

		s_objLightReady = true;
	}

	s_shaderReady = true;
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
