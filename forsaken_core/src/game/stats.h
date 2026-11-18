#pragma once

/* stats.h -- per-player kill, death, weapon and bonus scoring
 *
 * stats.cpp owns the tables; they last for a game session and are cleared by
 * ResetAllStats / ResetIndividualStats. Arrays are indexed by player id and
 * sized MAX_PLAYERS+1, though the loops cover 0..MAX_PLAYERS-1.
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

/* Defined in stats.cpp; shared with the modules that use it. */
int GetFriendlyKills( int Player );
extern int KillStats[ MAX_PLAYERS+1 ][ MAX_PLAYERS+1 ];
extern int BonusStats[ MAX_PLAYERS+1 ];
extern int KillCounter[ MAX_PLAYERS+1 ];
