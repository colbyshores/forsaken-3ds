/* hunk_alloc.cpp -- two-tag bump allocator; see hunk_alloc.h. */
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
    char  *base  = nullptr;   /* owned buffer, for the life of the process */
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

};

Hunk g_hunk;

} // namespace

bool Hunk_Init(size_t bytes) { return g_hunk.init(bytes); }
void *Hunk_Alloc(HunkTag tag, size_t size) { return g_hunk.alloc(tag, size); }
void Hunk_FreeAll(HunkTag tag) { g_hunk.free_all(tag); }
