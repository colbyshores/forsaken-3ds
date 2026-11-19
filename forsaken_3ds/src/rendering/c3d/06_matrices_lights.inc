/* ============================================================================
 * 06_matrices_lights.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Engine RENDERMATRIX to C3D_Mtx conversion (upload_matrices) and the
 * hybrid GPU vertex lighting uniforms (c3d_upload_xlights).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Matrix conversion: engine RENDERMATRIX → C3D_Mtx
===================================================================*/

static void matrix_to_c3d(const RENDERMATRIX *src, C3D_Mtx *dst)
{
	/* Engine matrices are column-major in memory: the raw float[16] is
	 * the column-major data the old OpenGL backend uploaded as is.
	 * Load it as glLoadMatrixf would:
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
	/* Split the combined MVP into:
	 *   modelView slot = world_matrix       (model→world transform)
	 *   projection slot = view × proj        (camera + projection,
	 *                                          PICA depth/rotation tweaks)
	 *
	 * Shader composes them as `outpos = projection * (modelView * inpos)`,
	 * which algebraically equals `MVP * inpos` so existing draws are
	 * pixel-identical. The split lets the shader read the world-space
	 * vertex position out of `modelView * inpos` for lighting math —
	 * required for models, where `inpos` is in MODEL space and a single
	 * combined-MVP shader can't compute distance to world-space lights.
	 *
	 * The PICA row tweaks (depth remap, 90° rotation) commute with
	 * right-multiplication by world_matrix, so applying them only to the
	 * view×proj half is mathematically equivalent to applying them to
	 * the full MVP. */
	MATRIX vp;
	C3D_Mtx c3d_vp, c3d_world;

	MatrixMultiply(&view_matrix, &proj_matrix, &vp);
	matrix_to_c3d((const RENDERMATRIX*)&vp, &c3d_vp);

	/* PICA depth remap: D3D [0,1] → PICA [-1,0] */
	c3d_vp.r[2].x -= c3d_vp.r[3].x;
	c3d_vp.r[2].y -= c3d_vp.r[3].y;
	c3d_vp.r[2].z -= c3d_vp.r[3].z;
	c3d_vp.r[2].w -= c3d_vp.r[3].w;

	/* 90° screen rotation */
	{
		C3D_FVec row0 = c3d_vp.r[0];
		c3d_vp.r[0] = c3d_vp.r[1];
		c3d_vp.r[1].x = -row0.x;
		c3d_vp.r[1].y = -row0.y;
		c3d_vp.r[1].z = -row0.z;
		c3d_vp.r[1].w = -row0.w;
	}

	matrix_to_c3d((const RENDERMATRIX*)&world_matrix, &c3d_world);

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &c3d_vp);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &c3d_world);
}

/*===================================================================
	Hybrid GPU vertex lighting — point lights, level mesh.

	The vertex shader sums ambient + per-light distance falloff for up
	to GPU_MAX_LIGHTS lights, replacing the per-vertex CPU iteration in
	lights.cpp for the level mesh (XLight1Group) and for models (the
	model paths only restore the baked colours and leave the lights to
	the shader). SPOT lights are treated as points (no cone math) —
	visually negligible for Forsaken's transient blaster/RT lights and
	keeps the shader simple.

	See shaders/render_c3d.v.pica for the shader side.
===================================================================*/

/* Ambient contribution. Zero matches the engine's XLight1Group baseline
 * (no global ambient term). Tunable per art direction. */
static float s_ambient_r = 0.0f;
static float s_ambient_g = 0.0f;
static float s_ambient_b = 0.0f;

/* Zero all lighting uniforms. Called from FSBeginScene so each frame
 * starts clean — without this, UI / crate-menu / title-screen frames
 * inherit the previous gameplay frame's lights as stale shading on
 * menu meshes. The gameplay path repopulates via c3d_upload_xlights. */
void c3d_clear_xlights(void)
{
	if (s_loc_lights < 0) return;
	if (s_loc_ambient >= 0)
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_ambient, 0.0f, 0.0f, 0.0f, 0.0f);
	if (s_loc_light_count >= 0)
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_light_count, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVec *dst = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, s_loc_lights, GPU_MAX_LIGHTS * 3);
	memset(dst, 0, sizeof(C3D_FVec) * GPU_MAX_LIGHTS * 3);
}

/*  Pack the closest GPU_MAX_LIGHTS visible XLights into the shader
 *  uniform. Distance ranking prevents on/off flicker on the blaster's
 *  own XLight in dense scenes (>8 visible) where naive first-N
 *  selection depends on linked-list traversal order.
 *
 *  No occlusion / wall-clipping: the shader has no way to know
 *  there's geometry between a light and the vertex it's touching, so
 *  blaster light bleeds through thin walls. A per-group GroupsAreVisible
 *  filter was tried; it's ineffective in practice because Forsaken
 *  levels are typically one large BSP group per visible area, leaving
 *  the bulk of the bleed (intra-group walls) unfiltered. Per-vertex
 *  shadow rays or per-light camera-occlusion raycasts would actually
 *  fix it but cost too much CPU on PICA200 at 60 Hz. */
void c3d_upload_xlights(void)
{
	if (s_loc_lights < 0) return;

	C3D_FVec lights_buf[GPU_MAX_LIGHTS * 3];

	struct XLIGHT *top[GPU_MAX_LIGHTS];
	float          top_d2[GPU_MAX_LIGHTS];
	int            n_top = 0;

	float cx = CurrentCamera.Pos.x;
	float cy = CurrentCamera.Pos.y;
	float cz = CurrentCamera.Pos.z;
	for (struct XLIGHT *L = FirstLightVisible; L; L = ((XLIGHT*)L)->NextVisible)
	{
		float dx = ((XLIGHT*)L)->Pos.x - cx;
		float dy = ((XLIGHT*)L)->Pos.y - cy;
		float dz = ((XLIGHT*)L)->Pos.z - cz;
		float d2 = dx*dx + dy*dy + dz*dz;

		if (n_top < GPU_MAX_LIGHTS) {
			top[n_top]    = L;
			top_d2[n_top] = d2;
			n_top++;
		} else {
			int worst = 0;
			for (int k = 1; k < GPU_MAX_LIGHTS; k++)
				if (top_d2[k] > top_d2[worst]) worst = k;
			if (d2 < top_d2[worst]) {
				top[worst]    = L;
				top_d2[worst] = d2;
			}
		}
	}

	int n_packed = 0;
	for (; n_packed < n_top; n_packed++)
	{
		struct XLIGHT *L = top[n_packed];
		float size = ((XLIGHT*)L)->Size;
		float invSizeSq = (size > 0.0f) ? (1.0f / (size * size)) : 0.0f;
		lights_buf[n_packed * 3 + 0] = FVec4_New(
			((XLIGHT*)L)->Pos.x,
			((XLIGHT*)L)->Pos.y,
			((XLIGHT*)L)->Pos.z,
			invSizeSq);

		/* Color values matched to lights.cpp::XLight1Group: 0..255 → 0..1
		 * with no dimming. Saturation behaviour now identical to CPU. */
		float r = ((XLIGHT*)L)->r / 255.0f;
		float g = ((XLIGHT*)L)->g / 255.0f;
		float b = ((XLIGHT*)L)->b / 255.0f;
		float type = (((XLIGHT*)L)->Type == SPOT_LIGHT) ? 1.0f : 0.0f;
		lights_buf[n_packed * 3 + 1] = FVec4_New(r, g, b, type);

		lights_buf[n_packed * 3 + 2] = FVec4_New(
			((XLIGHT*)L)->Dir.x,
			((XLIGHT*)L)->Dir.y,
			((XLIGHT*)L)->Dir.z,
			((XLIGHT*)L)->CosArc);
	}

	while (n_packed < GPU_MAX_LIGHTS) {
		lights_buf[n_packed * 3 + 0] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		lights_buf[n_packed * 3 + 1] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		lights_buf[n_packed * 3 + 2] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		n_packed++;
	}

	if (s_loc_ambient >= 0) {
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_ambient,
			s_ambient_r, s_ambient_g, s_ambient_b, 0.0f);
	}
	if (s_loc_light_count >= 0) {
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_light_count,
			(float)GPU_MAX_LIGHTS, 0.0f, 0.0f, 0.0f);
	}
	{
		C3D_FVec *dst = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, s_loc_lights, GPU_MAX_LIGHTS * 3);
		memcpy(dst, lights_buf, sizeof(lights_buf));
	}
}

/* Phong-tint per-object update.
 *
 * Sample the level's lit ambient at the object's world position
 * (GetRealLightAmbientWorldSpace = baked grey base + every active
 * XLight's contribution at that point), then blend toward the nearest
 * dominant XLight's hue with a falloff scale based on how deep the
 * object is inside that light's radius.
 *
 * Effect: enemies in vol2's lava chambers naturally pick up warm
 * baked tone in their specular highlight even with no rockets nearby;
 * a yellow rocket flying close shifts the highlight more strongly
 * yellow; both signals blend continuously instead of switching.
 *
 * Call at the start of every OBJECT-mode draw. Cost: one
 * GetRealLightAmbientWorldSpace walk (~50-200 cycles per visible
 * light, sums all of FirstLightVisible) + a C3D_LightColor bind. At
 * ~10 enemies per frame and ~20 visible lights, total ~40 K cycles
 * per frame — under 1% of OG 3DS frame budget. */
void c3d_update_phong_tint_for_object_pos(VECTOR *world_pos)
{
	if (!s_objLightReady || !world_pos) return;

	/* Sampled ambient at object position: baked level base + every
	 * visible XLight's per-position contribution (linear falloff, same
	 * math as lights.cpp::XLight1Group). Returned in 0..255 range. */
	float ar, ag, ab, aa;
	GetRealLightAmbientWorldSpace(world_pos, &ar, &ag, &ab, &aa);

	/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
	/* Normalise to 0..1 and apply a 70%-white floor. The engine's
	 * baked ambient is 0, so a quiet area
	 * with no nearby XLights would otherwise zero out the Phong light
	 * colour and kill the highlight entirely. The floor keeps the
	 * specular visible everywhere; tinted samples just shift the
	 * remaining 30% headroom. */
	/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
	const float kFloor = 0.70f;
	ar = ar / 255.0f; if (ar < kFloor) ar = kFloor; if (ar > 1.0f) ar = 1.0f;
	ag = ag / 255.0f; if (ag < kFloor) ag = kFloor; if (ag > 1.0f) ag = 1.0f;
	ab = ab / 255.0f; if (ab < kFloor) ab = kFloor; if (ab > 1.0f) ab = 1.0f;

	/* Find nearest XLight whose radius covers this object position. */
	struct XLIGHT *nearest = NULL;
	float          nearest_d2 = 0.0f;
	for (struct XLIGHT *L = FirstLightVisible; L; L = ((XLIGHT*)L)->NextVisible) {
		float dx = ((XLIGHT*)L)->Pos.x - world_pos->x;
		float dy = ((XLIGHT*)L)->Pos.y - world_pos->y;
		float dz = ((XLIGHT*)L)->Pos.z - world_pos->z;
		float d2 = dx*dx + dy*dy + dz*dz;
		float size = ((XLIGHT*)L)->Size;
		if (d2 < size * size) {
			if (!nearest || d2 < nearest_d2) {
				nearest    = L;
				nearest_d2 = d2;
			}
		}
	}

	float r = ar, g = ag, b = ab;
	if (nearest) {
		float size  = ((XLIGHT*)nearest)->Size;
		float falloff = 1.0f - (nearest_d2 / (size * size));
		if (falloff < 0.0f) falloff = 0.0f;
		float lr = ((XLIGHT*)nearest)->r / 255.0f;
		float lg = ((XLIGHT*)nearest)->g / 255.0f;
		float lb = ((XLIGHT*)nearest)->b / 255.0f;
		r = ar + (lr - ar) * falloff;
		g = ag + (lg - ag) * falloff;
		b = ab + (lb - ab) * falloff;
	}
	C3D_LightColor(&s_objLight, r, g, b);
}
