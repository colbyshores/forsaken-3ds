/* test_secwalk.c — host harness for the SecBull used-chain walks.
 *
 * Reproduces the BSS-zero hang: FirstSecBullUsed == 0 with an all-zero
 * SecBulls table makes AI_GetNearestMINETarget loop forever
 * (SecBulls[0].Prev == 0, so i never becomes (u_int16_t)-1).
 *
 * Links the REAL ailib.c against stubbed data-layer deps. Run with a
 * timeout: pre-fix the BSS-zero case never returns (expect exit 124
 * from `timeout`); post-fix all three cases exit 0.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
SECONDARYWEAPONBULLET SecBulls[MAXSECONDARYWEAPONBULLETS];
u_int16_t FirstSecBullUsed;
float SoundInfo[MAXGROUPS][MAXGROUPS];
ENEMY Enemies[MAXENEMIES];
GLOBALSHIP Ships[MAX_PLAYERS + 1];
BYTE WhoIAm;
ENEMY_TYPES EnemyTypes[MAX_ENEMY_TYPES];
VECTOR Forward, SlideRight, SlideUp;
NODENETWORKHEADER NodeNetworkHeader;
void ApplyMatrix(MATRIX *m, VECTOR *v0, VECTOR *v1) { (void)m; *v1 = *v0; }
void NormaliseVector(VECTOR *v) { (void)v; }
u_int16_t Random_Range(u_int16_t Max) { return Max ? (u_int16_t)(Max - 1) : 0; }
float DotProduct(VECTOR *a, VECTOR *b) { return a->x*b->x + a->y*b->y + a->z*b->z; }
MLOADHEADER Mloadheader;
MCLOADHEADER MCloadheader, MCloadheadert0;
bool BackgroundCollide(MCLOADHEADER *c, MLOADHEADER *m, VECTOR *a, u_int16_t b, VECTOR *d, VECTOR *e, u_int16_t *f, NORMAL *g, VECTOR *h, bool i, BGOBJECT **j) { (void)c;(void)m;(void)a;(void)b;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j; return false; }
bool WouldObjectCollide(OBJECT *o, VECTOR *v, float r, BGOBJECT **b) { (void)o;(void)v;(void)r;(void)b; return false; }
bool QCollide(VECTOR *a, u_int16_t b, VECTOR *c, float d, VECTOR *e, u_int16_t *f, NORMAL *g) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return false; }

float DistanceVector2Vector(VECTOR *a, VECTOR *b)
{
	float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

extern TARGETDATA *Tinfo;

/* Replicates the AI_GetDistToNearestTarget prologue (ailib.c:408-417),
 * the only legitimate caller: Pos/Group/ Nearest init + flag setup. */
static ENEMY *mk_enemy(void)
{
	static ENEMY e;
	memset(&e, 0, sizeof(e));
	e.Object.Group = 0;
	e.Object.Pos.x = 1.0f;
	e.Object.Pos.y = 2.0f;
	e.Object.Pos.z = 3.0f;
	Tinfo->Pos = e.Object.Pos;
	Tinfo->Group = e.Object.Group;
	Tinfo->Nearest = BIGDISTANCE;
	Tinfo->TFlags = 0;
	Tinfo->SObject = &e.Object;
	Tinfo->TObject = nullptr;
	return &e;
}

static int failures;

#define CHECK(cond, tag) do { \
	if (!(cond)) { printf("FAIL %s\n", tag); failures++; } \
	else { printf("ok %s\n", tag); } \
} while (0)

int main(void)
{
	/* Case 1: BSS-zero state (fresh boot before any init). Must return. */
	memset(SecBulls, 0, sizeof(SecBulls));
	FirstSecBullUsed = 0;
	mk_enemy();
	Tinfo->Flags = _TARGET_MINES;
	AI_GetNearestMINETarget();
	CHECK(Tinfo->TObject == nullptr, "bss-zero returns with no target");

	/* Case 2: healthy empty list. Must return at once. */
	FirstSecBullUsed = (u_int16_t)-1;
	mk_enemy();
	Tinfo->Flags = _TARGET_MINES;
	AI_GetNearestMINETarget();
	CHECK(Tinfo->TObject == nullptr, "empty list returns with no target");

	/* Case 3: one live mine is found. */
	memset(SecBulls, 0, sizeof(SecBulls));
	FirstSecBullUsed = 0;
	SecBulls[0].SecType = SEC_MINE;
	SecBulls[0].OwnerType = OWNER_SHIP;
	SecBulls[0].GroupImIn = 0;
	SecBulls[0].Prev = (u_int16_t)-1;
	SecBulls[0].Pos.x = 4.0f;
	SecBulls[0].Pos.y = 5.0f;
	SecBulls[0].Pos.z = 6.0f;
	mk_enemy();
	Tinfo->Flags = _TARGET_MINES;
	AI_GetNearestMINETarget();
	CHECK(Tinfo->TObject == (void *)&SecBulls[0], "live mine found");

	if (failures == 0) printf("PASS secwalk\n");
	return failures != 0;
}
