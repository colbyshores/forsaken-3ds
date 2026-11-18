#pragma once

/* music_table.h -- which music track plays on which level
 *
 * music_track_for_level is the level -> track mapping both music backends
 * use (music_play_for_level in music_3ds.cpp and music_openal.cpp).
 * level_num indexes the loaded level list (ShortLevelNames); the lookup is
 * by the level's name, with a separate table per edition. Returns a CD-style
 * track number in MUSIC_FIRST_TRACK..MUSIC_LAST_TRACK (music.h); levels
 * with no entry, such as multiplayer arenas, cycle through the tracks by
 * level_num.
 */
int music_track_for_level(int level_num);
