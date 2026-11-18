#pragma once

/* hunk_alloc.h -- two-tag bump allocator for per-level load data
 *
 * Hunk_Init mallocs one block at startup (8 MB, from main_3ds.cpp on 3DS and
 * main_sdl.cpp on PC) and keeps it for the life of the process; a second call
 * returns true without resizing. Hunk_Alloc bumps a cursor inside it:
 * TAG_STARTUP grows from the low end, TAG_LEVEL from the high end, and an
 * allocation that would make them cross returns NULL, as does any call before
 * Hunk_Init. Sizes are rounded up to 8 bytes and the memory comes back zeroed.
 *
 * There is no per-block free. Hunk_FreeAll(tag) resets that tag's cursor, and
 * every pointer from that tag is dead afterwards. ReleaseView (oct2.cpp)
 * calls Hunk_FreeAll(TAG_LEVEL) at the end of both its title and level
 * teardown, after the release functions that still walk the arrays. TAG_LEVEL
 * holds the per-execbuf textureGroups[] arrays of the level mesh and models
 * (mload / mxload / mxaload) and one texture group per water object
 * (water.cpp). Nothing allocates from TAG_STARTUP.
 */

#include <stddef.h>
#include <stdbool.h>


typedef enum {
    TAG_STARTUP = 0,
    TAG_LEVEL   = 1,
    TAG_COUNT
} HunkTag;

bool   Hunk_Init(size_t bytes);

void * Hunk_Alloc(HunkTag tag, size_t size);
void   Hunk_FreeAll(HunkTag tag);

