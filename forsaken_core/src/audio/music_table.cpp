/* music_table.cpp -- per-edition level -> music track tables; see music_table.h. */

#include <string.h>

#include "audio/music_table.h"
#include "audio/music.h"
#include "game/oct2.h"

/* Level->track mapping, keyed by level name. Track values are CD-style
 * track numbers naming the music/trackNN.dsp files.
 *
 * Keyed by name, not by position in the level list: the list is data
 * (assets/level_orderings/, and InitLevels skips levels whose .mxv is
 * missing), so a positional table silently shifts every later level's
 * music whenever a level is added, dropped or absent.
 *
 * Remaster: the Remaster KPF's musictrack assignments for its SP
 * campaign and the N64 secret levels (biolab and tuben64 are out of
 * the rotation but keep their tracks).
 * Original: ForsakenHW.exe's embedded "<levelname> <trackN>" array,
 * the authoritative 1998 mapping. */
namespace {

struct LevelTrack
{
	const char *name;
	int track;
};

#ifdef EDITION_REMASTER
constexpr LevelTrack kLevelTracks[] = {
	{ "vol2",         5 }, { "asubchb",      9 }, { "nps-sp01",     6 },
	{ "thermal",      3 }, { "fedbankv",     2 }, { "defend2",     13 },
	{ "pship",        8 }, { "stableizers", 10 }, { "powerdown",    7 },
	{ "spc-sp01",     4 }, { "starship",    12 }, { "bio-sphere",   9 },
	{ "nukerf",       3 }, { "battlebase",  15 }, { "capship",      7 },
	{ "space",        8 }, { "high",        10 }, { "military",     5 },
	{ "munitions",   14 }, { "biolab",       4 }, { "ramqan",      11 },
	{ "temple",       2 }, { "azt-sp01",     3 }, { "azchb",        6 },
	{ "nuken64",     16 }, { "shipn64",     12 }, { "aztec64",     13 },
	{ "blackhole",   11 }, { "genstation",  18 }, { "tuben64",     15 },
	{ "fishy",        7 }, { "final",        8 },
};
#else
constexpr LevelTrack kLevelTracks[] = {
	{ "vol2",         5 }, { "asubchb",      9 }, { "nps-sp01",     6 },
	{ "thermal",      3 }, { "fedbankv",     2 }, { "pship",        8 },
	{ "spc-sp01",     4 }, { "bio-sphere",   9 }, { "nukerf",       3 },
	{ "capship",      7 }, { "space",        8 }, { "high",        10 },
	{ "military",     5 }, { "azt-sp01",     3 }, { "azchb",        6 },
};
#endif

} // namespace

int music_track_for_level(int level_num)
{
	if (level_num < 0) level_num = 0;
	if (level_num < MAXLEVELS)
	{
		const char *name = ShortLevelNames[level_num];
		for (const LevelTrack &lt : kLevelTracks)
			if (strcmp(lt.name, name) == 0)
				return lt.track;
	}
	/* Levels with no assignment (multiplayer arenas) cycle the tracks. */
	return MUSIC_FIRST_TRACK + (level_num % MUSIC_NUM_TRACKS);
}
