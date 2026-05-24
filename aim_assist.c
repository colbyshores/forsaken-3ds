/*===================================================================
 * aim_assist.c — Subtle aim-assist for player primary bullets.
 *
 * See aim_assist.h for design rationale + parameter rationale.
 *
 * Per-frame, per-player-bullet:
 *   1) Walk Enemies + Ships, build a candidate list of targets in
 *      a narrow forward cone (cosine >= AIM_ASSIST_CONE_COS).
 *   2) Among candidates, pick the one with the highest dot product
 *      (= smallest angular offset from bullet's current direction).
 *      "Angle-weighted nearest" preserves the player's intent:
 *      assist the target they almost hit, not a different one.
 *   3) LOS check via BackgroundCollide — skip if a wall is between.
 *   4) Compute distance falloff (smoothstep — full at <=NEAR, zero
 *      at >=FAR), multiply by user strength to get effective turn
 *      rate this frame.
 *   5) Lerp bullet dir toward target dir by effective turn rate.
 *      For the small per-frame angles this operates at, normalized
 *      lerp is an acceptable slerp approximation.
 *
 * Cost: a few dozen float ops per bullet per frame, plus one BSP
 * raycast per bullet that found a candidate. Negligible at the
 * single-digit live bullet count typical of normal play.
 *===================================================================*/

#include "aim_assist.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>     /* FILE * referenced in primary.h transitively */
#include "new3d.h"
#include "main.h"
#include "enemies.h"     /* ENEMY struct */
#include "collision.h"   /* BackgroundCollide */
#include "config.h"
#include "networking.h"  /* WhoIAm + Ships + GameStatus + STATUS_*  */

/* Weapon enum constants we exclude — duplicated here to avoid pulling
 * primary.h (which uses FILE* without #include <stdio.h>). */
#define AA_LASER          5
#define AA_NME_LASER      13
#define AA_NME_LIGHTNING  14
#define AA_NME_POWERLASER 16

/* externs not declared in headers */
extern ENEMY        *FirstEnemyUsed;
extern MLOADHEADER   Mloadheader;
extern MCLOADHEADER  MCloadheadert0;
extern USERCONFIG   *player_config;

/* ─── Tunables ──────────────────────────────────────────────────── */

/* Middle-ground tuning between the original conservative target
 * (5° / 0.5°/frame / 200u-800u) and the first aggressive test
 * (20° / 3°/frame / 400u-2000u). Multiplied by the user's strength
 * setting (0.0 = off, 1.0 = full).
 *
 * Default is 0.0 (off) — user opts in via the 3DS Controls menu. */
#define AIM_ASSIST_CONE_HALF_DEG    12.0F        /* 12° forward cone */
#define AIM_ASSIST_CONE_COS         0.9781476F   /* cos(12°) */

#define AIM_ASSIST_TURN_RATE_DEG    1.5F         /* deg/frame at strength=1 */

#define AIM_ASSIST_NEAR_RANGE       300.0F       /* full assist at <= this */
#define AIM_ASSIST_FAR_RANGE        1400.0F      /* zero assist at >= this */

/* ─── Helpers ───────────────────────────────────────────────────── */

static float smoothstep_clamped( float edge0, float edge1, float x )
{
	float t;
	if( x <= edge0 ) return 1.0F;          /* full assist inside near */
	if( x >= edge1 ) return 0.0F;          /* zero assist past far    */
	t = (x - edge0) / (edge1 - edge0);
	/* invert (1 = near, 0 = far) and smoothstep */
	t = 1.0F - t;
	return t * t * (3.0F - 2.0F * t);
}

static float vec_dot( const VECTOR *a, const VECTOR *b )
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

static void vec_normalize_safe( VECTOR *v )
{
	float len_sq = v->x*v->x + v->y*v->y + v->z*v->z;
	if( len_sq > 1e-8F )
	{
		float inv = 1.0F / sqrtf( len_sq );
		v->x *= inv; v->y *= inv; v->z *= inv;
	}
}

/* Should this weapon get aim-assist? Laser-class instant-hit weapons
 * track the ship's barrel each frame; assist would fight the tracking. */
static bool weapon_is_assisted( int8_t w )
{
	switch( w )
	{
		case AA_LASER:
		case AA_NME_LASER:
		case AA_NME_LIGHTNING:
		case AA_NME_POWERLASER:
			return false;
		default:
			return true;
	}
}

/* Try a candidate target. Returns true if accepted (and updates
 * best_cos + sets *out_dir + *out_dist). */
static bool consider_target( const VECTOR *bullet_pos, const VECTOR *bullet_dir,
                             u_int16_t bullet_group,
                             const VECTOR *target_pos, u_int16_t target_group,
                             float *best_cos, VECTOR *out_target_dir, float *out_dist )
{
	VECTOR dir_to_target;
	VECTOR norm;
	float dist_sq;
	float cos;

	/* Vector from bullet to target */
	dir_to_target.x = target_pos->x - bullet_pos->x;
	dir_to_target.y = target_pos->y - bullet_pos->y;
	dir_to_target.z = target_pos->z - bullet_pos->z;

	dist_sq = dir_to_target.x*dir_to_target.x
	        + dir_to_target.y*dir_to_target.y
	        + dir_to_target.z*dir_to_target.z;

	/* Skip targets past max range — falloff would zero them anyway */
	if( dist_sq >= AIM_ASSIST_FAR_RANGE * AIM_ASSIST_FAR_RANGE )
		return false;
	/* Skip degenerate (target on top of bullet) */
	if( dist_sq < 1.0F )
		return false;

	norm = dir_to_target;
	vec_normalize_safe( &norm );

	cos = vec_dot( &norm, bullet_dir );

	/* Cone check + better-than-current */
	if( cos < AIM_ASSIST_CONE_COS )
		return false;
	if( cos <= *best_cos )
		return false;

	(void)target_group;  /* group-audible gate handled by caller via SoundInfo if needed */

	/* Defer LOS check to caller — only test the best candidate at the end */
	*best_cos = cos;
	*out_target_dir = norm;
	*out_dist = sqrtf( dist_sq );
	return true;
}

/* ─── Public entry ──────────────────────────────────────────────── */

bool aim_assist_steer( VECTOR *bullet_pos, VECTOR *bullet_dir,
                       u_int16_t bullet_group, int8_t weapon )
{
	float strength;
	float best_cos;
	VECTOR target_dir;
	float target_dist;
	bool found_any;
	ENEMY *enemy;
	int16_t ship_idx;
	float falloff;
	float angle_rad;
	float turn_rad;
	float t;
	VECTOR new_dir;
	VECTOR int_pt, tmp;
	NORMAL int_norm;
	u_int16_t int_grp;
	VECTOR ray_dir;

	/* Cheap early-outs first — the off path must cost effectively nothing. */
	strength = player_config ? player_config->aim_assist_strength : 0.0F;
	if( strength <= 0.0F )
		return false;
	if( !weapon_is_assisted( weapon ) )
		return false;

	best_cos = AIM_ASSIST_CONE_COS;  /* must beat this to be considered */
	found_any = false;

	/* Walk live enemies */
	enemy = FirstEnemyUsed;
	while( enemy != NULL )
	{
		if( enemy->Status & ENEMY_STATUS_Enable )
		{
			if( consider_target( bullet_pos, bullet_dir, bullet_group,
			                     &enemy->Object.Pos, enemy->Object.Group,
			                     &best_cos, &target_dir, &target_dist ) )
			{
				found_any = true;
			}
		}
		enemy = enemy->NextUsed;
	}

	/* Also walk other ships (multiplayer; cheap when ship count is 1) */
	for( ship_idx = 0; ship_idx < MAX_PLAYERS; ship_idx++ )
	{
		if( ship_idx == WhoIAm ) continue;
		if( !Ships[ship_idx].enable ) continue;
		if( Ships[ship_idx].Object.Mode == LIMBO_MODE ) continue;
		if( !( GameStatus[ship_idx] == STATUS_Normal ||
		       GameStatus[ship_idx] == STATUS_SinglePlayer ) ) continue;

		if( consider_target( bullet_pos, bullet_dir, bullet_group,
		                     &Ships[ship_idx].Object.Pos, Ships[ship_idx].Object.Group,
		                     &best_cos, &target_dir, &target_dist ) )
		{
			found_any = true;
		}
	}

	if( !found_any )
		return false;

	/* LOS check on the best candidate only — saves doing N raycasts */
	ray_dir.x = target_dir.x * target_dist;
	ray_dir.y = target_dir.y * target_dist;
	ray_dir.z = target_dir.z * target_dist;
	if( BackgroundCollide( &MCloadheadert0, &Mloadheader, bullet_pos, bullet_group,
	                       &ray_dir, &int_pt, &int_grp, &int_norm, &tmp, true, NULL ) )
	{
		/* Wall between bullet and target — don't assist through walls */
		return false;
	}

	/* Distance falloff: smoothstep — full at <=NEAR, zero at >=FAR */
	falloff = smoothstep_clamped( AIM_ASSIST_NEAR_RANGE, AIM_ASSIST_FAR_RANGE, target_dist );
	if( falloff <= 0.0F )
		return false;

	/* Effective per-frame turn = base × falloff × user strength */
	turn_rad = (AIM_ASSIST_TURN_RATE_DEG * (3.14159265F / 180.0F))
	         * falloff * strength;

	/* How far off is the bullet from the target direction? */
	{
		float cos_now = vec_dot( bullet_dir, &target_dir );
		if( cos_now > 1.0F ) cos_now = 1.0F;
		if( cos_now < -1.0F ) cos_now = -1.0F;
		angle_rad = acosf( cos_now );
	}

	/* If we're already close enough to the target dir, snap (cheap) */
	if( angle_rad <= turn_rad )
	{
		*bullet_dir = target_dir;
		return true;
	}

	/* Otherwise, normalized lerp by t = turn / angle. For small turns
	 * this approximates slerp closely enough — the per-frame angular
	 * change is tiny (sub-degree), so the lerp error is negligible. */
	t = turn_rad / angle_rad;

	new_dir.x = bullet_dir->x + t * (target_dir.x - bullet_dir->x);
	new_dir.y = bullet_dir->y + t * (target_dir.y - bullet_dir->y);
	new_dir.z = bullet_dir->z + t * (target_dir.z - bullet_dir->z);
	vec_normalize_safe( &new_dir );

	*bullet_dir = new_dir;
	return true;
}
