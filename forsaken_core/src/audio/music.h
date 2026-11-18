#pragma once

/* music.h -- CD-track music playback seam
 *
 * Implemented by music_3ds.cpp (ndsp hardware DSP-ADPCM decode, streamed
 * from romfs:/music/trackNN.dsp by a worker thread) and music_openal.cpp
 * (whole track decoded in software and looped as one OpenAL buffer); both
 * read the same .dsp files. Track numbers follow the original CD, with
 * track 02 as the title theme; EDITION_REMASTER extends the range to 19.
 * music_play_for_level maps a level to a track through music_table.
 * music_update is a no-op on both backends.
 */

#include "game/main.h"


/* Track numbering matches the original 1998 CD:
 *   Track 02 - "Forsaken" (main theme)
 *   Track 03 - "Gargantuan"
 *   Track 04 - "Sanctuary of Tloloc"
 *   Track 05 - "Volcano"
 *   Track 06 - "Reactor"
 *   Track 07 - "Pure Bitch Power"
 *   Track 08 - "Condemned"
 *   Track 09 - "Flameout"
 *   Track 10 - "The Dead System"
 */

constexpr int MUSIC_FIRST_TRACK = 2;
#ifdef EDITION_REMASTER
/* Remaster: 18 OGG tracks from Forsaken Remastered's music/OGG/ library
 * (tracks 1-9 are the original CD compositions, 10-18 are new — Labyrinth,
 * Nubia, Pyrolite, the Force-Of-Angels remix, four N64 arrangements, and
 * Cyclotron). On disk we keep CD-style 1-offset numbering so the user's
 * existing track02-10 from extract_assets.py stay valid; OGG 10-18 land
 * at track11.dsp..track19.dsp. extract_remaster_levels.py converts them. */
constexpr int MUSIC_LAST_TRACK = 19;
#else
constexpr int MUSIC_LAST_TRACK = 10;
#endif
constexpr auto MUSIC_NUM_TRACKS = (MUSIC_LAST_TRACK - MUSIC_FIRST_TRACK + 1);

/* Title screen track */
constexpr int MUSIC_TITLE_TRACK = 2;

bool music_init(void);
void music_shutdown(void);
void music_play(int track);     /* play track N (2-10), looping */
void music_stop(void);

/* Called from ChangeLevel() / title screen to play the right track */
void music_play_for_level(int level_num);
void music_play_title(void);

/* Call once per frame to refill stream buffers */
void music_update(void);
