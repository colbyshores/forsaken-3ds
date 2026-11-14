/* stereomemo.c — one-frame converted-vertex memo for single-pass stereo.
 *
 * Compiles to nothing unless STEREO_REPLAY is defined (same discipline
 * as interp_3ds.c / atm_prof_3ds.c), so default builds are unaffected.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stereomemo.h"

#define SMEMO_BITS 12
#define SMEMO_SIZE (1u << SMEMO_BITS)
#define SMEMO_MASK (SMEMO_SIZE - 1u)
#define SMEMO_PROBE_MAX 64

typedef struct {
	uintptr_t ro;
	int sub;
	long offset;
	int count;
	uint32_t epass;
	uint32_t epgroup;
	uint32_t etpage;
	unsigned gen;
} smemo_entry_t;

static smemo_entry_t s_tab[SMEMO_SIZE];
static unsigned s_gen = 1; /* 0 = invalid (never a valid generation) */
static unsigned s_persist_gen = 1; /* R05: slower generation, see header */

static unsigned smemo_hash(uintptr_t ro, int sub)
{
	/* splitmix64 finalize on the packed key; low bits index the table. */
	uint64_t z = (uint64_t)ro * 0x9E3779B97F4A7C15ULL + (uint64_t)sub;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	/* Narrowing assignment, not a conversion: low bits index the table. */
	{
		unsigned h = (z ^ (z >> 31)) & SMEMO_MASK;
		return h;
	}
}

/* Bump the generation, invalidating every entry without clearing.
 * Shared by frame begin and mid-frame invalidation. */
static void smemo_bump_gen(void)
{
	++s_gen;
	if (s_gen == 0) {
		memset(s_tab, 0, sizeof(s_tab));
		s_gen = 1;
	}
}

void smemo_begin_frame(void)
{
	smemo_bump_gen();
}

/* Mid-frame invalidation: texture animation steps and water integration
 * mutate source vertices between the eyes (the advance fires once per
 * frame, mid-eye-L). Bumping here makes pre-step records invisible to
 * eye-R lookups (safe live-convert misses) while post-step records
 * still hit. Call exactly when shared vertex content actually changes,
 * not merely when an advance function runs, or idle frames lose hits. */
void smemo_invalidate(void)
{
	smemo_bump_gen();
	++s_persist_gen;
}

unsigned smemo_persist_gen(void)
{
	return s_persist_gen;
}

static unsigned s_persist_resets = 0;

void smemo_persist_reset(void)
{
	++s_persist_gen;
	if (s_persist_gen == 0)
		s_persist_gen = 1;
	++s_persist_resets;
}

unsigned smemo_persist_resets(void)
{
	return s_persist_resets;
}

void smemo_record(const void *renderObject, int sub,
                   long offset, int count,
                   uint32_t epass, uint32_t epgroup, uint32_t etpage)
{
	unsigned i = smemo_hash((uintptr_t)renderObject, sub);
	unsigned n;
	for (n = 0; n < SMEMO_PROBE_MAX; n++) {
		smemo_entry_t *e = &s_tab[(i + n) & SMEMO_MASK];
		if (e->gen != s_gen || (e->ro == (uintptr_t)renderObject && e->sub == sub)) {
			e->ro = (uintptr_t)renderObject;
			e->sub = sub;
			e->offset = offset;
			e->count = count;
			e->epass = epass;
			e->epgroup = epgroup;
			e->etpage = etpage;
			e->gen = s_gen;
			return;
		}
	}
	/* Table pressure: drop the record (eye 2 converts live). A miss is
	 * always safe; a false hit never is — so never evict on the read. */
}

bool smemo_lookup(const void *renderObject, int sub,
                   long *offset, int *count,
                   uint32_t epass, uint32_t epgroup, uint32_t etpage)
{
	unsigned i = smemo_hash((uintptr_t)renderObject, sub);
	unsigned n;
	for (n = 0; n < SMEMO_PROBE_MAX; n++) {
		const smemo_entry_t *e = &s_tab[(i + n) & SMEMO_MASK];
		/* INVARIANT (load-bearing): record() always claims the first
		 * stale slot in the probe chain, so no current-generation entry
		 * for this hash can sit beyond a stale slot. The early return
		 * below is sound only while that holds — any change to the
		 * insertion policy (e.g. random replacement) must revisit it. */
		if (e->gen != s_gen)
			return false;
		if (e->ro == (uintptr_t)renderObject && e->sub == sub) {
			if (e->epass != epass || e->epgroup != epgroup || e->etpage != etpage)
				return false;
			*offset = e->offset;
			*count = e->count;
			return true;
		}
	}
	return false;
}

