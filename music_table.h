#ifndef MUSIC_TABLE_INCLUDED
#define MUSIC_TABLE_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Shared level->track mapping for the music backends (music_3ds.c,
 * music_openal.c). The table is edition-dependent data; it lives
 * here once so a mapping fix lands in a single place. Returns a
 * .dsp track number (MUSIC_FIRST_TRACK..MUSIC_LAST_TRACK range). */
int music_track_for_level(int level_num);

#ifdef __cplusplus
}
#endif

#endif
