#pragma once

/* multiplayer.h -- title-menu actions for hosting, joining and team setup
 *
 * multiplayer.cpp backs the network menus in title.cpp. Hosting seeds the
 * shared RNG (Seed1/Seed2), sets the game-type flags (SetUpGameType) and
 * starts the session; the team functions fill the team lists before synchup.
 * The My* settings keep the local player's host preferences, because joining
 * a game overwrites the live settings with the host's. RandomStartPosModify
 * offsets spawn-point and goal selection.
 */

#include "game/main.h"
#include "game/constants.h"
#include "ui/title.h"

//
// Networking functions mainly used by title.c
//

void StartAHostSession ( MENUITEM * Item );
void GetPlayersInCurrentSession( MENUITEM *Item );
void GoToSynchup ( MENUITEM * Item );
void BailMultiplayer( MENU * Menu );
void TeamGoToSynchup ( MENUITEM * Item );
void InitTeamSelection( MENU *Menu );
void InitTeamLists( MENU *Menu );
void InitExistingGameJoin( MENU *Menu );
void CheckForMenuChange(int *dummy);
void BailMultiplayerFrontEnd( MENU * Menu );
void DistributeTeamsToLists(int *dummy);
void SetUpGameType( int type );

/* Defined in multiplayer.cpp; shared with the modules that use it. */
extern u_int16_t RandomStartPosModify;
extern bool IpOnCLI;
extern bool TintBikeTeamColor;
extern bool ResetKillsPerLevel;
extern MENU * GetPlayerNumMenu;
extern bool MyResetKillsPerLevel;
extern int TeamMembers[ MAX_TEAMS ];
