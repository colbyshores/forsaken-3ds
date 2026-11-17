#ifndef MULTIPLAYER_INCLUDED
#define MULTIPLAYER_INCLUDED
#include "game/main.h"
#include "game/constants.h"
#include "ui/title.h"
#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
#endif	// MULTIPLAYER_INCLUDED
