/* fidelity_3ds.c — backing for fidelity.h. Compiles to nothing unless
 * FIDELITY_DUMP is defined, so default builds are unaffected.
 *
 * Writes one line per level to sdmc:/forsaken_fidelity.txt at the level
 * marker (SD write once per level, not per frame — same discipline as
 * perf_capture). See fidelity.h for the semantics of the hash.
 */
#ifdef FIDELITY_DUMP

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "fidelity.h"
#include "networking.h"
#include "enemies.h"
#include "object.h"
#include "2dtextures.h"
#include "node.h"
#include "primary.h"
#include "secondary.h"

extern ENEMY Enemies[MAXENEMIES];
extern PRIMARYWEAPONBULLET PrimBulls[MAXPRIMARYWEAPONBULLETS];
extern u_int16_t FirstPrimBullUsed;
extern SECONDARYWEAPONBULLET SecBulls[MAXSECONDARYWEAPONBULLETS];
extern u_int16_t FirstSecBullUsed;
extern BYTE WhoIAm;

static u_int32_t g_hash = 2166136261u;
static unsigned long g_ticks;
static int g_fd = -1;

static void fid_out(const char *line, int len)
{
	if (g_fd < 0)
		g_fd = open("sdmc:/forsaken_fidelity.txt",
		            O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (g_fd < 0) return;
	if (write(g_fd, line, (size_t)len) < 0) { /* SD full/removed: ignore */ }
}

static void hi(u_int32_t v)
{
	g_hash ^= v;
	g_hash *= 16777619u;
}

static void hf(float f)
{
	u_int32_t u;
	memcpy(&u, &f, sizeof(u));
	if (u == 0x80000000u) u = 0; /* -0.0 == +0.0 for fidelity purposes */
	hi(u);
}

static void hvec(const VECTOR *v)
{
	hf(v->x);
	hf(v->y);
	hf(v->z);
}

void fidelity_tick(void)
{
	int i;
	u_int16_t n;
	int prim_count = 0, sec_count = 0;

	hi((u_int32_t)g_ticks);

	for (i = 0; i <= MAX_PLAYERS; i++) {
		if (!Ships[i].enable) continue;
		hi((u_int32_t)i);
		hvec(&Ships[i].Object.Pos);
		hvec(&Ships[i].Object.Angle);
		hi(Ships[i].Object.Group);
		hf(Ships[i].Object.Shield);
		hf(Ships[i].Damage);
		if (i == (int)WhoIAm && Ships[i].Object.NearestNode) {
			NODE *nd = (NODE *)Ships[i].Object.NearestNode;
			hvec(&nd->Pos);
		}
	}

	for (i = 0; i < MAXENEMIES; i++) {
		if (!Enemies[i].Used) continue;
		hi((u_int32_t)i);
		hi((u_int32_t)Enemies[i].Alive);
		hvec(&Enemies[i].Object.Pos);
		hvec(&Enemies[i].Object.Angle);
		hi(Enemies[i].Object.Group);
		hf(Enemies[i].Object.Shield);
		hi((u_int32_t)Enemies[i].Type);
	}

	n = FirstPrimBullUsed;
	while (n != (u_int16_t)-1 && prim_count <= MAXPRIMARYWEAPONBULLETS) {
		hvec(&PrimBulls[n].Pos);
		prim_count++;
		n = PrimBulls[n].Prev;
	}
	hi((u_int32_t)prim_count);

	n = FirstSecBullUsed;
	while (n != (u_int16_t)-1 && sec_count <= MAXSECONDARYWEAPONBULLETS) {
		hvec(&SecBulls[n].Pos);
		sec_count++;
		n = SecBulls[n].Next;
	}
	hi((u_int32_t)sec_count);

	g_ticks++;

#ifdef FIDELITY_CHECKPOINTS
	if ((g_ticks % (unsigned long)FIDELITY_CHECKPOINT_TICKS) == 0) {
		char cl[128];
		int clen = snprintf(cl, sizeof(cl),
		                    "FIDC: tick=%lu hash=%08lx\n", g_ticks,
		                    (unsigned long)g_hash);
		if (clen > 0) fid_out(cl, clen);
	}
#endif
}

void fidelity_level(const char *level_name)
{
	char line[128];
	int len;

	len = snprintf(line, sizeof(line), "FID: level=%s ticks=%lu hash=%08lx\n",
	               level_name ? level_name : "?", g_ticks,
	               (unsigned long)g_hash);
	if (len > 0) fid_out(line, len);
	g_hash = 2166136261u;
	g_ticks = 0;
}

unsigned long fidelity_level_ticks(void)
{
	return g_ticks;
}

#endif /* FIDELITY_DUMP */
