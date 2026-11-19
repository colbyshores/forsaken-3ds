/* test_checkhitenemy.c — host harness for CheckHitEnemy's SoundInfo gate.
 *
 * Links the REAL checkenemy.c + REAL sphere.c against stubbed data-layer
 * deps (same pattern as test_secwalk.c). Pins the pre-filter contract:
 * enemies in groups with no direct visibility from the bullet's group
 * (SoundInfo != 0) are skipped; connected groups behave exactly as the
 * unfiltered ray test dictates. Case 3 fails against the pre-gate code
 * (sealed-nearer would win), so the suite discriminates the change.
 *
 * Stubs (documented, all trivial): PointToSphere (real home secondary.c,
 * too heavy to link; obvious point-in-sphere), VectorLength (exact
 * sqrt-of-dot, as in new3d.c), NormaliseVector/DotProduct (test_secwalk
 * pattern; only reached by Trojax/Transpulse paths, not exercised here),
 * IsAvatarActive (non-avatar types used; present for link).
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "data/mload.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "world/node.h"
#include "net/networking.h"
#include "game/ships.h"
#include "rendering/2dpolys.h"
#include "rendering/lights.h"
#include "game/models.h"
#include "rendering/text.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/triggers.h"
#include "math/sphere.h"
#include "game/pickups.h"
#include "game/enemies.h"
#include "input/controls.h"
#include "ai/ai.h"
#include "rendering/lines.h"

/* ---- stubs for the data layer (game-owned, defined here for host) ---- */
ENEMY *FirstEnemyUsed;
ENEMY_TYPES EnemyTypes[MAX_ENEMY_TYPES];
float SoundInfo[MAXGROUPS][MAXGROUPS];

bool PointToSphere(VERT *TargPos, float TargRadius, VERT *BulPos)
{
	float dx = (float)(BulPos->x - TargPos->x);
	float dy = (float)(BulPos->y - TargPos->y);
	float dz = (float)(BulPos->z - TargPos->z);
	return (dx * dx + dy * dy + dz * dz) <= TargRadius * TargRadius;
}

float VectorLength(VECTOR *v)
{
	return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

void NormaliseVector(VECTOR *v)
{
	float l = VectorLength(v);
	if (l > 0.0f) {
		v->x /= l;
		v->y /= l;
		v->z /= l;
	}
}

float DotProduct(VECTOR *a, VECTOR *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

bool IsAvatarActive(void) { return true; }

/* ---- fixture ---- */
static ENEMY eA, eB;

static void mk_enemy(ENEMY *e, int idx, int group, float x, float y, float z, bool enable)
{
	memset(e, 0, sizeof(*e));
	e->Index = (u_int16_t)idx;
	e->Type = 0;
	e->Object.Group = (u_int16_t)group;
	e->Object.Pos.x = x;
	e->Object.Pos.y = y;
	e->Object.Pos.z = z;
	if (enable)
		e->Status |= ENEMY_STATUS_Enable;
	e->CompCollision = SPHERECOLLISION;
	e->NextUsed = nullptr;
}

static int failures;

#define CHECK(cond, tag) do { \
	if (!(cond)) { printf("FAIL %s\n", tag); failures++; } \
	else { printf("ok %s\n", tag); } \
} while (0)

#define WITHIN(a, b, eps) (fabsf((a) - (b)) <= (eps))

int main(void)
{
	VECTOR pos, dir, up, ip, ip2;
	float dist;
	ENEMY *hit;

	EnemyTypes[0].Radius = 10.0f;

	pos.x = 100.0f; pos.y = 0.0f; pos.z = 0.0f;
	dir.x = -1.0f; dir.y = 0.0f; dir.z = 0.0f;
	up.x = 0.0f; up.y = 1.0f; up.z = 0.0f;

	/* Case 1: connected group, on-ray enemy is hit at the sphere surface. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 0.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == &eA, "connected on-ray enemy hit");
	CHECK(WITHIN(dist, 90.0f, 0.01f), "hit distance at sphere surface");

	/* Case 2: sealed group (-1): same geometry is skipped, Dist untouched. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	SoundInfo[1][0] = -1.0f;
	mk_enemy(&eB, 6, 1, 50.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eB;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == nullptr, "sealed-group enemy skipped");
	CHECK(dist == 1000.0f, "Dist untouched on skip");

	/* Case 3: sealed-nearer (dist 40) loses to connected-farther (dist 90).
	 * Pre-gate code returns &eB here, so this case discriminates. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	SoundInfo[1][0] = 5.0f; /* indirect route: also not directly visible */
	mk_enemy(&eA, 5, 0, 0.0f, 0.0f, 0.0f, true);
	mk_enemy(&eB, 6, 1, 50.0f, 0.0f, 0.0f, true);
	eA.NextUsed = &eB;
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == &eA, "sealed-nearer loses to connected-farther");
	CHECK(WITHIN(dist, 90.0f, 0.01f), "connected distance wins");

	/* Case 4: owner self-hit still excluded (connected, on-ray). */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 0.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_ENEMY, 5, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == nullptr, "owner self excluded");

	/* Case 5: disabled enemy ignored (connected, on-ray). */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 0.0f, 0.0f, 0.0f, false);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == nullptr, "disabled enemy ignored");

	/* Case 6: connected but off-ray still misses (gate passes, ray decides). */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 0.0f, 60.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == nullptr, "connected off-ray misses");

	/* Case 7: far enemy beyond reach is skipped, Dist untouched.
	 * dist 100 > DirLength 50 + Radius 10 + WeaponRadius 0. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 200.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 50.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == nullptr, "beyond-reach enemy skipped");
	CHECK(dist == 1000.0f, "Dist untouched on skip");

	/* Case 8: just inside reach still hits (no over-cull at boundary).
	 * dist 59 <= 50 + 10, on-ray: surface at 49. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 41.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 50.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == &eA, "inside-reach enemy hit");
	CHECK(WITHIN(dist, 49.0f, 0.01f), "inside-reach distance");

	/* Case 9: Trojax far enemy skipped (Cos path never runs). */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, -200.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 50.0f,
						&ip, &ip2, &dist, 5.0f, COLTYPE_Trojax);
	CHECK(hit == nullptr, "trojax far enemy skipped");
	CHECK(dist == 1000.0f, "Dist untouched on trojax skip");

	/* Case 10: negative WeaponRadius falls through to old logic (guard). */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 0.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 1000.0f,
						&ip, &ip2, &dist, -1.0f, COLTYPE_Sphere);
	CHECK(hit == &eA, "negative weapon radius still hits near");
	CHECK(WITHIN(dist, 90.0f, 0.01f), "negative-W distance");

	/* Case 11: zero-length ray overlapping bullet still hits inside. */
	memset(SoundInfo, 0, sizeof(SoundInfo));
	mk_enemy(&eA, 5, 0, 100.0f, 0.0f, 0.0f, true);
	FirstEnemyUsed = &eA;
	dist = 1000.0f;
	hit = CheckHitEnemy(OWNER_SHIP, 0, &pos, 0, &dir, &up, 0.0f,
						&ip, &ip2, &dist, 0.0f, COLTYPE_Sphere);
	CHECK(hit == &eA, "zero-length overlapping ray hits");
	CHECK(dist == 0.0f, "inside-start distance zero");

	if (failures == 0) printf("PASS checkhitenemy\n");
	return failures != 0;
}
