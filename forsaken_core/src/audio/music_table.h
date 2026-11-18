#ifndef MUSIC_TABLE_INCLUDED
#define MUSIC_TABLE_INCLUDED


/* Shared level->track mapping for the music backends (music_3ds,
 * music_openal). level_num indexes the loaded level list
 * (ShortLevelNames); the table is keyed by the level's name. Returns a
 * .dsp track number (MUSIC_FIRST_TRACK..MUSIC_LAST_TRACK range). */
int music_track_for_level(int level_num);


#endif
