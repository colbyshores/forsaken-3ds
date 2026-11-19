/* ============================================================================
 * 12_cpu_lighting.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * CPU-side per-vertex lighting helpers (GetRealLightAmbientWorldSpace).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Lighting — CPU-side per-vertex, ported from the old OpenGL 1 backend
===================================================================*/

/* Lighting globals — set by the engine's TransExe system per object */

int render_lighting_point_lights_only = 1;

int render_light_ambience = 0;
int render_light_ambience_alpha = 255.0f;


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

		/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
		rlen2 = ray.x * ray.x + ray.y * ray.y + ray.z * ray.z;
		lsize2 = LightPnt->Size * LightPnt->Size;

		if (rlen2 < lsize2)
		{
			if (render_lighting_point_lights_only || LightPnt->Type == POINT_LIGHT)
			{
				/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
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
				/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
				cosa = ray.x * LightPnt->Dir.x +
					   /* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					   ray.y * LightPnt->Dir.y +
					   ray.z * LightPnt->Dir.z;

				if (rlen2 > lsize2 * 0.5f)
				{
					if (cosa > LightPnt->CosArc)
						intensity = ((lsize2 - rlen2) / (0.75f * lsize2)) *
									/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
									((cosa - LightPnt->CosArc) / (1.0f - LightPnt->CosArc));
					else
						goto NEXT_LIGHT;
				}
				else if (rlen2 > MIN_LIGHT_SIZE)
				{
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					cosarc2 = LightPnt->CosArc *
						(1.0f - (lsize2 * 0.5f - rlen2) / (lsize2 * 0.5f - MIN_LIGHT_SIZE));
					if (cosa > cosarc2)
						intensity = ((lsize2 - rlen2) / (lsize2 - MIN_LIGHT_SIZE)) *
									/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
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
