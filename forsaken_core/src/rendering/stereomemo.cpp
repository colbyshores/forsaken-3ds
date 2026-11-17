/* stereomemo.cpp - one-frame converted-vertex memo for single-pass
 * stereo. See stereomemo.h for the key-stability contract.
 *
 * C-with-classes (AGENTS.md S9): the table and its generation
 * counters are one value type with explicit ownership; the public
 * header stays a flat C API over a single static instance so the
 * render backend's call sites are unchanged. */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "rendering/stereomemo.h"

namespace {

constexpr unsigned SMEMO_BITS = 12;
constexpr unsigned SMEMO_SIZE = 1u << SMEMO_BITS;
constexpr unsigned SMEMO_MASK = SMEMO_SIZE - 1u;
constexpr unsigned SMEMO_PROBE_MAX = 64;

struct Entry {
    uintptr_t ro;
    int sub;
    long offset;
    int count;
    uint32_t epass;
    uint32_t epgroup;
    uint32_t etpage;
    unsigned gen;
};

/* One-frame memo table. Entries are never cleared; a generation
 * counter invalidates them in O(1) per frame (and mid-frame when
 * shared vertex content changes). A second, slower generation
 * (persist_gen) tracks records that survive across frames for the
 * mono-only persist path. */
struct StereoMemo {
    Entry tab[SMEMO_SIZE];
    unsigned gen = 1;         /* 0 = invalid (never a valid generation) */
    unsigned persist_gen = 1; /* slower generation, see header */
    unsigned persist_resets = 0;

    static unsigned hash_key(uintptr_t ro, int sub)
    {
        /* splitmix64 finalize on the packed key; low bits index. */
        uint64_t z = (uint64_t)ro * 0x9E3779B97F4A7C15ULL + (uint64_t)sub;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        /* Narrowing assignment, not a conversion: low bits index. */
        return (unsigned)((z ^ (z >> 31)) & SMEMO_MASK);
    }

    void bump_gen()
    {
        ++gen;
        if (gen == 0) {
            memset(tab, 0, sizeof(tab));
            gen = 1;
        }
    }

    void begin_frame() { bump_gen(); }

    /* Mid-frame invalidation: texture animation steps and water
     * integration mutate source vertices between the eyes. Bumping
     * here makes pre-step records invisible to eye-R lookups (safe
     * live-convert misses) while post-step records still hit. Call
     * exactly when shared vertex content actually changes. */
    void invalidate()
    {
        bump_gen();
        ++persist_gen;
    }

    void persist_reset()
    {
        ++persist_gen;
        if (persist_gen == 0) persist_gen = 1;
        ++persist_resets;
    }

    void record(const void *renderObject, int sub,
                long offset, int count,
                uint32_t epass, uint32_t epgroup, uint32_t etpage)
    {
        const unsigned i = hash_key((uintptr_t)renderObject, sub);
        for (unsigned n = 0; n < SMEMO_PROBE_MAX; n++) {
            Entry *e = &tab[(i + n) & SMEMO_MASK];
            if (e->gen != gen || (e->ro == (uintptr_t)renderObject && e->sub == sub)) {
                e->ro = (uintptr_t)renderObject;
                e->sub = sub;
                e->offset = offset;
                e->count = count;
                e->epass = epass;
                e->epgroup = epgroup;
                e->etpage = etpage;
                e->gen = gen;
                return;
            }
        }
        /* Table pressure: drop the record (eye 2 converts live). A
         * miss is always safe; a false hit never is. */
    }

    bool lookup(const void *renderObject, int sub,
                long *offset, int *count,
                uint32_t epass, uint32_t epgroup, uint32_t etpage) const
    {
        const unsigned i = hash_key((uintptr_t)renderObject, sub);
        for (unsigned n = 0; n < SMEMO_PROBE_MAX; n++) {
            const Entry *e = &tab[(i + n) & SMEMO_MASK];
            /* INVARIANT (load-bearing): record() always claims the
             * first stale slot in the probe chain, so no current-
             * generation entry for this hash can sit beyond a stale
             * slot. The early return below is sound only while that
             * holds - any change to the insertion policy (e.g.
             * random replacement) must revisit it. */
            if (e->gen != gen)
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
};

StereoMemo g_memo;

} // namespace

void smemo_begin_frame(void) { g_memo.begin_frame(); }
void smemo_invalidate(void) { g_memo.invalidate(); }
unsigned smemo_persist_gen(void) { return g_memo.persist_gen; }
void smemo_persist_reset(void) { g_memo.persist_reset(); }
unsigned smemo_persist_resets(void) { return g_memo.persist_resets; }

void smemo_record(const void *renderObject, int sub,
                  long offset, int count,
                  uint32_t epass, uint32_t epgroup, uint32_t etpage)
{
    g_memo.record(renderObject, sub, offset, count, epass, epgroup, etpage);
}

bool smemo_lookup(const void *renderObject, int sub,
                  long *offset, int *count,
                  uint32_t epass, uint32_t epgroup, uint32_t etpage)
{
    return g_memo.lookup(renderObject, sub, offset, count, epass, epgroup, etpage);
}
