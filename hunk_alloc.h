/*===================================================================
 *  Q3-style hunk allocator.
 *
 *  Two arenas in one fixed-size buffer:
 *    TAG_STARTUP — process-lifetime allocations, grow from low end.
 *    TAG_LEVEL   — per-level allocations, grow from high end. Freed
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
 *  Lives entirely on the malloc heap — Hunk_Init pulls one big
 *  block, every Hunk_Alloc bumps an offset within it. No per-alloc
 *  free; only Hunk_FreeAll(tag) which resets the tag's cursor.
 *
 *  Allocation is 8-byte aligned. Memory is zeroed before return
 *  (callers depend on the BSS-zero semantics they had before).
 *===================================================================*/

#ifndef HUNK_ALLOC_INCLUDED
#define HUNK_ALLOC_INCLUDED

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TAG_STARTUP = 0,
    TAG_LEVEL   = 1,
    TAG_COUNT
} HunkTag;

bool   Hunk_Init(size_t bytes);
void   Hunk_Shutdown(void);

void * Hunk_Alloc(HunkTag tag, size_t size);
void   Hunk_FreeAll(HunkTag tag);

size_t Hunk_Used(HunkTag tag);
size_t Hunk_Available(void);
size_t Hunk_Total(void);

#endif
