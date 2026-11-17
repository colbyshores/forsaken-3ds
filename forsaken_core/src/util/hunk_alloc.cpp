/*===================================================================
 *  Q3-style hunk allocator.
 *
 *  Two arenas in one fixed-size buffer:
 *    TAG_STARTUP - process-lifetime allocations, grow from low end.
 *    TAG_LEVEL   - per-level allocations, grow from high end. Freed
 *                  in bulk via Hunk_FreeAll(TAG_LEVEL) on level
 *                  transition.
 *
 *  Why this exists rather than plain malloc/free: we have a few large
 *  per-level structures (the model-header arrays, per-execbuf
 *  texture-group arrays) that the engine used to keep in BSS at
 *  worst-case size. Migrating them to dynamic allocation is what
 *  recovers the BSS memory; the hunk just keeps fragmentation
 *  bounded across many level transitions.
 *
 *  Lives entirely on the malloc heap - Hunk_Init pulls one big
 *  block, every allocation bumps an offset within it. No per-alloc
 *  free; only free_all(tag), which resets the tag's cursor.
 *
 *  Allocation is 8-byte aligned. Memory is zeroed before return
 *  (callers depend on the BSS-zero semantics they had before).
 *
 *  C-with-classes (AGENTS.md S9): the allocator is one class owning
 *  the buffer and both cursors; the public header stays a flat C
 *  API over a single static instance so existing call sites in C
 *  translation units keep working unchanged.
 *===================================================================*/
#include "util/hunk_alloc.h"

#include <stdlib.h>
#include <string.h>

namespace {

constexpr size_t HUNK_ALIGN = 8;
constexpr size_t align_up(size_t x)
{
    return (x + (HUNK_ALIGN - 1)) & ~((size_t)HUNK_ALIGN - 1);
}

/* Two-tag bump allocator over one malloc'd buffer. TAG_STARTUP
 * grows from the low end, TAG_LEVEL from the high end; the tags
 * meet in the middle and allocation fails when they would cross. */
struct Hunk {
    char  *base  = nullptr;   /* owned buffer, freed by shutdown() */
    size_t total = 0;
    size_t low   = 0;         /* TAG_STARTUP cursor: bytes used from low end */
    size_t high  = 0;         /* TAG_LEVEL   cursor: bytes used from high end */

    bool init(size_t bytes)
    {
        if (base) return true;        /* already initialised */
        if (bytes == 0) return false;
        base = (char *)malloc(bytes);
        if (!base) { total = 0; return false; }
        total = bytes;
        low = high = 0;
        return true;
    }

    void shutdown()
    {
        free(base);
        base = nullptr;
        total = low = high = 0;
    }

    void *alloc(HunkTag tag, size_t size)
    {
        if (!base || size == 0) return nullptr;
        const size_t aligned = align_up(size);
        if (low + high + aligned > total) return nullptr;
        void *p;
        if (tag == TAG_STARTUP) {
            p = base + low;
            low += aligned;
        } else {
            high += aligned;
            p = base + total - high;
        }
        memset(p, 0, aligned);
        return p;
    }

    void free_all(HunkTag tag)
    {
        if (tag == TAG_STARTUP) low = 0;
        else                    high = 0;
    }

    size_t used(HunkTag tag) const { return tag == TAG_STARTUP ? low : high; }
    size_t available() const { return total - low - high; }
};

Hunk g_hunk;

} // namespace

bool Hunk_Init(size_t bytes) { return g_hunk.init(bytes); }
void Hunk_Shutdown(void) { g_hunk.shutdown(); }
void *Hunk_Alloc(HunkTag tag, size_t size) { return g_hunk.alloc(tag, size); }
void Hunk_FreeAll(HunkTag tag) { g_hunk.free_all(tag); }
size_t Hunk_Used(HunkTag tag) { return g_hunk.used(tag); }
size_t Hunk_Available(void) { return g_hunk.available(); }
size_t Hunk_Total(void) { return g_hunk.total; }
