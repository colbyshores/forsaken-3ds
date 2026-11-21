
//
// Networking functions mainly used by title.cpp
//

#include <stdio.h>

#include "game/main.h"
#include "util/platform.h"
#include "net/net.h"
#include "rendering/render.h"
#include "ui/title.h"

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "net/multiplayer.h"
#include "data/mxload.h"
#include "data/mxaload.h"
#include <stdio.h>
#include "ui/local.h"
#include "rendering/text.h"
#include "game/ships.h"
#include "data/loadsave.h"
#include "game/pickups.h"
#include "game/primary.h"
#include "input/controls.h"
#include "util/util.h"
#include "string.h"
#include "util/util.h"
#include "net/net_tracker.h"
#include "game/oct2.h"
#include "rendering/screenpolys.h"

/*
 * Externals
 */

bool ResetKillsPerLevel;
bool MyResetKillsPerLevel;


/*
 * Globals to this module
 */


MENU  *				GetPlayerNumMenu;

u_int16_t	RandomStartPosModify = 0;

void SetUpGameSubType( int /*type*/ )
{
	switch ( CTFSlider.value )
	{
	case CTF_MUST_CARRY:
		NeedFlagAtHome			= true;
		OwnFlagTeleportsHome	= false;
		CanCarryOwnFlag			= true;
		break;
	case CTF_CAN_CARRY:
		NeedFlagAtHome			= false;
		OwnFlagTeleportsHome	= false;
		CanCarryOwnFlag			= true;
		break;
	case CTF_ONE_TOUCH:
		NeedFlagAtHome			= false;
		OwnFlagTeleportsHome	= true;
		CanCarryOwnFlag			= false;
		break;
	case CTF_NO_RETURN:
		NeedFlagAtHome			= false;
		OwnFlagTeleportsHome	= false;
		CanCarryOwnFlag			= false;
		break;
	case CTF_STANDARD:
	default:
		NeedFlagAtHome			= true;
		OwnFlagTeleportsHome	= true;
		CanCarryOwnFlag			= false;
	}
}

void SetUpGameType( int type )
{
	TeamGame = false;
	CTF = false;
	NeedFlagAtHome			= false;
	OwnFlagTeleportsHome	= false;
	CanCarryOwnFlag			= false;
	CaptureTheFlag = false;
	BountyHunt = false;

	switch ( type )
	{
	case GAME_Normal:
		break;
	case GAME_Team:
		TeamGame = true;
		break;
	case GAME_CTF:
		CTF = true;
		TeamGame = true;

		SetUpGameSubType( CTFSlider.value );

		break;
	case GAME_FlagChase:
		CaptureTheFlag = true;
		TeamGame = true;
		break;
	case GAME_BountyHunt:
		BountyHunt = true;
		break;
	case GAME_TeamBountyHunt:
		BountyHunt = true;
		TeamGame = true;
		break;
	}
}


/*===================================================================
	Procedure	:	Hosting a session...
	Input		:	nothing
	Output		:	nothing
===================================================================*/

// these settings get over ridden when you join a game
// so we need a separate copy of them to backup our settings
void copy_in_my_settings( void )
{
	memset( PickupValid, 0, sizeof(PickupValid) );
	memmove( PickupValid, MyPickupValid, sizeof(PickupValid) );//memcpy

	TimeLimit.value		= MyTimeLimit.value;
	PacketsSlider.value	= MyPacketsSlider.value;

	ColPerspective		= MyColPerspective;
	ResetKillsPerLevel	= MyResetKillsPerLevel;
	RandomPickups		= MyRandomPickups;
	UseShortPackets		= MyUseShortPackets;
}

void StartAHostSession ( MENUITEM * Item )
{
	int i;
	u_int32_t	ms = SDL_GetTicks();
	u_int32_t	Seed;
	network_return_t rv;

	SetGamePrefs();

	copy_in_my_settings();

	Seed = SDL_GetTicks();
	Seed1 = (u_int16_t) ( ( Seed >> 16 ) & 0xffff );
	Seed2 = (u_int16_t) ( Seed & 0xffff );
	CopyOfSeed1 = Seed1;
	CopyOfSeed2 = Seed2;



	IsHost = true;

	TeamGame = false;
	CaptureTheFlag = false;
	BountyHunt = false;
	CTF = false;

	SetUpGameType( GameType );

	// set bike mode to normal
	SetBikeMods( 0 );

	MaxKills = MaxKillsSlider.value;

	RandomStartPosModify = (u_int16_t) ( ms * 71.42857143 );

	NetCfg.local_port = atoi(local_port_str.text);
	SetGamePrefs();

	rv = network_setup( &biker_name[0], NetCfg.local_port );

	if( rv != NETWORK_OK )
	{
		char error_str[600] = "Failed to setup network!";
		switch( rv )
		{
		case NETWORK_ERROR_INIT:
			break;
		case NETWORK_ERROR_BIND:
			{
				const char * str = "";
				if( NetCfg.local_port < 1024 )
					str = "Ports bellow 1024 are normally restricted.";
				snprintf(error_str, sizeof( error_str ), "%s %s %s %s",
						"The selected port is in use or invalid.",
						str,
						"Disable the program using the port or",
						"select a different local port under network options." );
			}
			break;
		default:
			break;
		}
		PrintErrorMessage(error_str, 2, &MENU_NEW_CreateGame, 0);
		return;
	}

	network_host();

	NetUpdateInterval	= 60.0F / PacketsSlider.value;
	OldPPSValue = PacketsSlider.value;
	OldColPerspective = ColPerspective;
	OldUseShortPackets = UseShortPackets;

	SetupNetworkGame();

	for( i = 0 ; i < MAX_PLAYERS ; i++ )
	{
		GameStatus[i] = STATUS_Null;
	}

	WhoIAm = 0;								// I was the first to join...
	Ships[WhoIAm].network_player = nullptr;

	if ( TeamGame )
		MenuChange( &NewTeamItem );
	else
		MenuChange( Item );

	MyGameStatus = STATUS_StartingMultiplayer;

	Current_Camera_View = 0;				// set camera to that view
	Ships[WhoIAm].Pickups = 0;
	Ships[WhoIAm].RegenSlots = 0;
	Ships[WhoIAm].Mines = 0;
	Ships[WhoIAm].Triggers = 0;
	Ships[WhoIAm].TrigVars = 0;

	set_my_player_name();

	Ships[ WhoIAm ].BikeNum = ( SelectedBike % MAXBIKETYPES );

	NewLevelNum = LevelList.selected_item;	// I Select Which Level We Start on...

	if( TimeLimit.value )
	{
		Countdown.switch_on();
	}
	else
	{
		Countdown.switch_off();
	}



	VideoOpts.BrightShips = VideoOpts.MyBrightShips;

	return;
}

/*===================================================================
	Procedure	:	Get The Players In the Current Session...
	Input		:	MENUITEM * Item
	Output		:	nothing
===================================================================*/
void GetPlayersInCurrentSession( MENUITEM * /*Item*/ )
{
	int i;

	PlayersList.items = 0;
	PlayersList.top_item = 0;
	PlayersList.selected_item = -1;

	NumOfPlayersSlider.value = 0;

	for ( i = 0; i < MAX_PLAYERS; i++ )
		if( ( GameStatus[ i ] == MyGameStatus ) || ( i == WhoIAm ) )
		{
			NumOfPlayersSlider.value++;
		copy_string( (char*)PlayersList.item[ PlayersList.items++ ], sizeof( PlayersList.item[ 0 ] ), (char*)Names[ i ] );
	}

	if (NumOfPlayersSlider.value != NumOfPlayersSlider.oldvalue)
		NumOfPlayersSlider.redraw_req = true;
}

/*===================================================================
	Procedure	:	The host has waited for everyone now the game will init and start..
	Input		:	MENUITEM * Item
	Output		:	nothing
===================================================================*/
void GoToSynchup ( MENUITEM * /*Item*/ )
{

	if( TeamGame )
	{
		if (CameraStatus == CAMERA_AtStart)
		{
			MenuChange( &TeamItem );
		}else
		{
			MenuChange( &NewTeamItem );
		}
	}else{

		if (CameraStatus != CAMERA_AtStart)
		{
			LastMenu = CurrentMenu;
			VduClear();
		}

		CurrentMenu = nullptr;
		CurrentMenuItem = nullptr;
		MenuStackLevel = 0;
	}

}
/*===================================================================
	Procedure	:	The host has waited for everyone now the game will init and start..
	Input		:	MENUITEM * Item
	Output		:	nothing
===================================================================*/
void TeamGoToSynchup ( MENUITEM * /*Item*/ )
{
	PreSynchupStatus = MyGameStatus;
	MyGameStatus = STATUS_WaitingToStartTeamGame;
	MenuChangeEx( &MENU_NEW_GeneralLoading );
	MenuDraw( CurrentMenu );	// because menu processing will not be done when MyGameStatus changes

}


/*===================================================================
	Procedure	:	Bail out of a multiplayer game before it starts...
	Input		:	MENU * Menu
	Output		:	nothing
===================================================================*/
void BailMultiplayer( MENU * /*Menu*/ )
{
	MyGameStatus = STATUS_Left;
	if ( WhoIAm < MAX_PLAYERS )
		SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
	if(IsHost)
		send_tracker_finished( NetCfg.tracker_server, NetCfg.tracker_port );
	network_cleanup();
	MenuRestart( &MENU_Start );
	MyGameStatus = STATUS_Title;
}

void BailMultiplayerFrontEnd( MENU *Menu )
{
	BailMultiplayer( Menu );
}

void InitTeamLists( MENU * /*Menu*/ )
{
	int i;

	TeamList[0] = PlayersList;
	TeamList[0].selected_item = -1;
	TeamList[0].display_items = 12;

	for (i = 1; i < MAX_TEAMS; i++)
	{
		TeamList[i].items = 0;
		TeamList[i].top_item = 0;
		TeamList[i].display_items = 12;			// hard wired in because MAX_PLAYERS will increase
		TeamList[i].selected_item = -1;

	}

	for ( i = 0; i < MAX_PLAYERS; i++ )
		PlayerReady[ i ] = false;
}


/*===================================================================
	Procedure	:	Set the Team player lists..
	Input		:	MENU * Menu
	Output		:	nothing
===================================================================*/
void InitTeamSelection( MENU *Menu )
{
	int i;
	MENUITEM *item;

	InitTeamLists( nullptr );

	for( item = Menu->Item; item->x >=0; item++ )
	{
		if ( item->FuncDraw == DrawReadyButton )
			item->FuncSelect = PlayerConfirm;

		if 	( ( item->Variable == &TeamList[0] ) ||
			( item->Variable == &TeamList[1] ) ||
			( item->Variable == &TeamList[2] ) ||
			( item->Variable == &TeamList[3] ) )
		{
			item->FuncSelect = SelectTeamList;
		}
	}

	for( i = 0 ; i < MAX_PLAYERS ; i++ )
		if( IsHost )
			TeamNumber[i] = 255;

	TeamNumber[WhoIAm] = 0;		// start myself off in team 0

	// if we went into this menu automatically (ie. we are not the host), Menustate
	// will still be in whatever state it was in in the last menu, so reset...
	MenuState = MENUSTATE_Select;

	InitTitleMessaging();
}


void DistributeTeamsToLists(int * /*dummy*/)
{
	int team, player, num_players;
	bool OkayToStart = true;
	int16_t TeamScore[MAX_TEAMS];

	UpdateSessions( nullptr );
	memset(&TeamScore, 0, sizeof(int16_t) * MAX_TEAMS);

	for (team = 0; team < MAX_TEAMS; team++)
	{
		TeamList[team].items = 0;
		TeamList[team].top_item = 0;
		TeamList[team].display_items = 12;
	}

	num_players = 0;

	for ( player = 0; player < MAX_PLAYERS; player++ )	// server dosn't count as player
	{
		// player has left, therefore reset their team number...
		if ((GameStatus[player] == STATUS_Left) || (GameStatus[player] == STATUS_LeftCrashed))
		{
			TeamNumber[player] = 255;
		}

		team = TeamNumber[player];
		if (team < MAX_TEAMS)
		{
			if ( ( player == WhoIAm ) || ( GameStatus[ player ] == STATUS_StartingMultiplayer ) || ( GameStatus[ player ] == STATUS_Normal ) )
			{
				num_players++;

				if ( !PlayerReady[player] && ( GameStatus[ player ] != STATUS_Normal ) )
				{
					if (Pulse >= 0.5F)
					{
						TeamList[team].item[TeamList[team].items++][0] = 0;
					}
					else
					{
						copy_string( (char*)TeamList[team].item[TeamList[team].items], sizeof( TeamList[team].item[0] ), Names[player] );
						TeamList[team].items++;
					}

					OkayToStart = false;
				}else
				{
					copy_string( (char*)TeamList[team].item[TeamList[team].items], sizeof( TeamList[team].item[0] ), Names[player] );
					TeamList[team].items++;
				}
			}
		}
	}

	if ( IsHost && OkayToStart && num_players )
	{
		TeamGoToSynchup(nullptr);
	}
}
