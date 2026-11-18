#pragma once

/* aim_assist.h -- steering pull on the player's primary bullets
 *
 * ProcessPrimaryBullets (primary.cpp) calls aim_assist_steer on every
 * ship-owned primary bullet each frame, before the bullet moves. It bends
 * the bullet's Dir toward the enemy or other ship closest to its line of
 * flight inside a narrow forward cone, if no wall is in the way; the pull
 * fades with distance so long shots are unassisted. Tunables (cone, turn
 * rate, near/far range) are in aim_assist.cpp.
 *
 * Strength is player_config->aim_assist_strength, 0.0 (off, a cheap
 * early-out) to 1.0: saved per pilot as AIMASSIST (config.cpp, default
 * 0.25) and adjusted in the 3DS Controls menu (controls_3ds.cpp).
 * Laser-class weapons are left alone: they track the ship's barrel each
 * frame and the assist would fight that. A normalized lerp on Dir stands in
 * for slerp at these small per-frame angles.
 */

#include "math/new3d.h"  /* VECTOR */
#include "game/main.h"   /* u_int16_t */

/* Steer the bullet's direction toward a valid target if one is in
 * the forward cone with LOS. No-op if player_config->aim_assist_strength is
 * zero or no target found.
 *
 *   pos    — current bullet world position
 *   dir    — current bullet world direction (modified in place)
 *   group  — bullet's level group (for LOS check)
 *   weapon — primary weapon type (used to skip laser-class)
 *
 * Returns true if dir was modified, false otherwise. */
bool aim_assist_steer( VECTOR *pos, VECTOR *dir, u_int16_t group, int8_t weapon );
