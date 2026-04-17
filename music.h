#ifndef MUSIC_INCLUDED
#define MUSIC_INCLUDED

#include "main.h"

/*
 * CD audio music playback system.
 *
 * WAV files are loaded from romfs:/music/ and streamed via ndsp.
 * Format expected: 16-bit signed LE PCM, mono, 32000 Hz.
 *
 * Track numbering matches the original 1998 CD:
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

#define MUSIC_FIRST_TRACK   2
#define MUSIC_LAST_TRACK   10
#define MUSIC_NUM_TRACKS   (MUSIC_LAST_TRACK - MUSIC_FIRST_TRACK + 1)

/* Title screen track */
#define MUSIC_TITLE_TRACK   2

bool music_init(void);
void music_shutdown(void);
void music_play(int track);     /* play track N (2-10), looping */
void music_stop(void);
bool music_is_playing(void);
void music_set_volume(float vol); /* 0.0 - 1.0 */

/* Called from ChangeLevel() / title screen to play the right track */
void music_play_for_level(int level_num);
void music_play_title(void);

/* Call once per frame to refill stream buffers */
void music_update(void);

#endif
