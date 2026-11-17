#include "audio/music_table.h"
#include "audio/music.h"

/* Level->track mapping. Track values are CD-style track numbers
 * naming romfs/music/trackNN.dsp files.
 *
 * Remaster: indices line up with Data/Levels/mission_remaster.dat
 * (the authoritative 24-entry SP order + 8 N64 secret levels); track
 * values come from the Remaster KPF's musictrack assignments.
 * Original: indices line up with Data/Levels/mission.dat (1998 SP
 * campaign); values extracted from ForsakenHW.exe's embedded
 * "<levelname> <trackN>" array - the authoritative 1998 mapping. */
#ifdef EDITION_REMASTER
static const int s_level_track[] = {
     5, 9, 6, 3, 2, 13, 8, 10, 7, 4, 12, 9, 3, 15, 7, 8,
     10, 5, 14, 4, 11, 2, 3, 6, 16, 12, 13, 11, 18, 15, 7, 8
};
#else
static const int s_level_track[] = {
    5, 9, 6, 3, 2, 8, 4, 9, 3, 7, 8, 10, 5, 3, 6
};
#endif
#define NUM_LEVEL_TRACKS (int)(sizeof(s_level_track) / sizeof(s_level_track[0]))

int music_track_for_level(int level_num)
{
    if (level_num < 0) level_num = 0;
    if (level_num < NUM_LEVEL_TRACKS)
        return s_level_track[level_num];
    return MUSIC_FIRST_TRACK + (level_num % MUSIC_NUM_TRACKS);
}
