/*===================================================================
 * aim_assist.h — Subtle aim-assist for player primary bullets.
 *
 * Compensates for circle-pad input imprecision on 3DS by applying a
 * small steering pull on player-fired primary bullets toward the
 * nearest enemy/ship within a narrow forward cone. Distance-faded so
 * long-range "skill" shots are unassisted.
 *
 * Strength is tunable via Configs/main.txt (aim_assist_strength) and
 * the in-game options menu. 0.0 = off (free early-out), 1.0 = full
 * assist using the constants below.
 *
 *   Cone half-angle:       5°    (narrow — bullets near the target only)
 *   Base turn rate:        0.5°  per frame at strength=1.0
 *   Distance falloff:      smoothstep, full at <=200u, zero at >=800u
 *
 * No quaternion needed: we use a simple normalized lerp on the bullet's
 * VECTOR Dir, which approximates slerp for the tiny per-frame angles
 * this feature operates at. Excludes laser-class instant-hit weapons
 * (those track the ship's barrel each frame; assist would fight the
 * tracking).
 *===================================================================*/

#ifndef AIM_ASSIST_H
#define AIM_ASSIST_H

#include "new3d.h"  /* VECTOR */
#include "main.h"   /* u_int16_t */

/* Steer the bullet's direction toward a valid target if one is in
 * the forward cone with LOS. No-op if Config.aim_assist_strength is
 * zero or no target found.
 *
 *   pos    — current bullet world position
 *   dir    — current bullet world direction (modified in place)
 *   group  — bullet's BSP group (for LOS check)
 *   weapon — primary weapon type (used to skip laser-class)
 *
 * Returns true if dir was modified, false otherwise. */
bool aim_assist_steer( VECTOR *pos, VECTOR *dir, u_int16_t group, int8_t weapon );

#endif /* AIM_ASSIST_H */
