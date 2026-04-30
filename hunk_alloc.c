#include "hunk_alloc.h"

#include <stdlib.h>
#include <string.h>

#define HUNK_ALIGN 8
#define HUNK_ALIGN_UP(x) (((x) + (HUNK_ALIGN - 1)) & ~((size_t)HUNK_ALIGN - 1))

static char  *s_base  = NULL;
static size_t s_total = 0;
static size_t s_low   = 0;   /* TAG_STARTUP cursor: bytes used from low end */
static size_t s_high  = 0;   /* TAG_LEVEL   cursor: bytes used from high end */

bool Hunk_Init(size_t bytes)
{
    if (s_base) return true;        /* already initialised */
    if (bytes == 0) return false;
    s_base = (char *)malloc(bytes);
    if (!s_base) {
        s_total = 0;
        return false;
    }
    s_total = bytes;
    s_low   = 0;
    s_high  = 0;
    return true;
}

void Hunk_Shutdown(void)
{
    if (s_base) free(s_base);
    s_base  = NULL;
    s_total = 0;
    s_low   = 0;
    s_high  = 0;
}

void *Hunk_Alloc(HunkTag tag, size_t size)
{
    if (!s_base || size == 0) return NULL;
    size_t aligned = HUNK_ALIGN_UP(size);
    if (s_low + s_high + aligned > s_total) return NULL;

    void *p;
    if (tag == TAG_STARTUP) {
        p = s_base + s_low;
        s_low += aligned;
    } else {
        s_high += aligned;
        p = s_base + s_total - s_high;
    }
    memset(p, 0, aligned);
    return p;
}

void Hunk_FreeAll(HunkTag tag)
{
    if (tag == TAG_STARTUP) s_low  = 0;
    else                    s_high = 0;
}

size_t Hunk_Used(HunkTag tag)      { return tag == TAG_STARTUP ? s_low : s_high; }
size_t Hunk_Available(void)        { return s_total - s_low - s_high; }
size_t Hunk_Total(void)            { return s_total; }
