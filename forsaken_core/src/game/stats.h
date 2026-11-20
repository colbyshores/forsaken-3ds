#pragma once

/* stats.h -- per-player kill, death, weapon and bonus scoring
 *
 * The tables are GameStats' (below, one instance Stats, defined in
 * stats.cpp); they last for a game session and are cleared by ResetAllStats /
 * ResetIndividualStats. Arrays are indexed by player id and sized
 * MAX_PLAYERS+1, though the loops cover 0..MAX_PLAYERS-1.
 * KillStats[Killer][Victim]; the diagonal counts suicides. GetKills is kills
 * minus suicides and team kills, GetScoreStats adds BonusStats (CTF / bounty
 * points), GetRealScore also subtracts deaths. UpdateKillStats announces
 * milestones and returns early, recording nothing, for Titan direct hits and
 * Gravgon kills.
 */

#include "game/main.h"
#include "game/constants.h"

/*******************************************************************\
*
*	stats.h
*
*	Header file for all statistical related proceedures
*
\*******************************************************************/

/* prototypes */
void UpdateKillStats(int Killer, int Victim, int WeaponType, int Weapon);		// update the kill statistics
void UpdateBonusStats(int Player, int Points);										// update the bonus statistics (e.g. CTF or bounty points)
void UpdateKillCount(int Killer);															// updates player's kill counter for this life
void ResetAllStats();																			// Resets all statistics
void ResetIndividualStats(int Player);													// Resets one player's statistics
void ScoreSort();																				// Sorts player's score from highest to lowest
void InitScoreSortTab(int Player);														// Initiate Score Sort Tab player IDs
int GetTotalKills(int Killer);																	// Get total number of kills (not including suicides)
int GetTotalDeaths(int Victim);															// Get total number of deaths
int GetKillStats(int Killer, int Victim);													// Get an individual kill statistic
int GetScoreStats(int Player);																// Get an individual score statistic
int GetRealScore(int Player);
int GetBonusStats(int Player);															// Get an individual bonus statistic
int GetTeamScore(int Player);																// Get a player's team's total score (including kills)
int GetWeaponKillStats(int PlayerID, int WeaponType, int Weapon);			// Get Weapon Kill Stats
int GetPlayerByRank(int Player);																// Get the id of the player at 1st, 2nd 3rd place etc.
int GetTeamScoreByTeamNumber(int Team);											// Get a player's team's total score (including kills)
const char* GetWeaponName(int WeaponType, int Weapon);							// Get weapon name

// new functions

int GetSuicides( int Player );
int GetKills( int Player );
int player_left( int player );
int scoreable_status( int player );

int GetFriendlyKills( int Player );

/* The game session's statistics (one instance, Stats): who killed whom, the
 * bonus points, each player's kills this life, kills by weapon, the score
 * ranking and the one-off first-blood and first-to-fifty announcements.
 * The C functions above forward to it or are built on GetKillStats.
 * networking.cpp copies the kill, kill-counter and bonus tables to and from
 * the INITMSG a joining player gets; the packet's tables are BYTE[16][16] /
 * BYTE[16] and the copies are the packet's size, as they always were (see
 * the FIXME there). */
class GameStats {
	public:
	int  kills( int Killer, int Victim ) { return KillStats[Killer][Victim]; }
	int  bonus( int Player ) { return BonusStats[Player]; }
	void add_bonus( int Player, int Points ) { BonusStats[Player] += Points; }
	void count_kill( int Killer );
	int  weapon_kills( int PlayerID, int WeaponType, int Weapon );
	void reset_all();
	void reset_player( int Player );
	void update_kills( int Killer, int Victim, int WeaponType, int Weapon );
	// defined in stats.cpp and inlined into ScoreSort, which runs every frame
	// the scoreboard shows, as it was before; other files call ScoreSort
	[[gnu::always_inline]] inline void sort_scores();
	int  player_by_rank( int rank );
	void init_sort_tab( int Player );

	// the host's tables, from the INITMSG a joining player receives
	void unpack_from_host( const BYTE (&HostKillStats)[MAX_PLAYERS][MAX_PLAYERS], const BYTE (&HostKillCounter)[MAX_PLAYERS], const BYTE (&HostBonusStats)[MAX_PLAYERS] )
	{
		memmove( KillStats,		HostKillStats,		sizeof(HostKillStats));
		memmove( KillCounter,	HostKillCounter,	sizeof(HostKillCounter));
		memmove( BonusStats,		HostBonusStats,		sizeof(HostBonusStats));
	}
	// this player's tables, into the INITMSG sent to a joining player
	void pack_for_joiner( BYTE (&MsgKillStats)[MAX_PLAYERS][MAX_PLAYERS], BYTE (&MsgKillCounter)[MAX_PLAYERS], BYTE (&MsgBonusStats)[MAX_PLAYERS] )
	{
		memmove( MsgKillStats,		KillStats,		sizeof(MsgKillStats));			// other players stats stats//memcpy
		memmove( MsgKillCounter,	KillCounter,	sizeof(MsgKillCounter));		// other players kills counts//memcpy
		memmove( MsgBonusStats,		BonusStats,		sizeof(MsgKillCounter));		// other players bonus stats//memcpy
	}

	private:
	int	KillStats[MAX_PLAYERS+1][MAX_PLAYERS+1] = {};						// each player's KillStats[Killer][Victim];
	int	PrimaryStats[MAX_PLAYERS+1][MAXPRIMARYWEAPONS+1] = {};				// PrimaryStats[Killer][PrimaryWeaponType];
	int	SecondaryStats[MAX_PLAYERS+1][TOTALSECONDARYWEAPONS] = {};		// SecondaryStats[Killer][SecondaryWeaponType];
	int	ScoreSortTab[MAX_PLAYERS + 1] = {};								// player numbers in order of highest score to lowest
	bool FirstBlood = false;
	bool FirstToFifty = false;
	// Static, so each stays its own 68-byte symbol as the old globals were:
	// - BonusStats is read for every score, every frame the scoreboard shows;
	//   as its own symbol that is one indexed load, not an add and a load;
	// - both are smaller than an INITMSG, so GCC can prove the joining
	//   player's 16-byte copies do not overlap the packet and inlines them
	//   (into Stats, 3 KB, they stay memmove calls).
	static int KillCounter[MAX_PLAYERS+1];											// number of kills made during this life
	static int BonusStats[MAX_PLAYERS+1];											// each player's ctf/bounty points
};

extern GameStats Stats;
