#pragma once

/* main.h -- program entry, game-status codes and engine-wide basics
 *
 * main.cpp owns main(). AppInit runs platform init, -chdir, the check for
 * the Configs / Data / Pilots / Scripts folders, Lua, the config file and
 * command-line overrides, video, input, then InitScene / InitView for the
 * title; main then loops handle_events + RenderLoop until QuitRequested is
 * set. On 3DS the loop is also gated on aptMainLoop.
 * CleanUpAndPostQuit tears everything down once; it is safe to call more
 * than once, and code that only wants to quit sets QuitRequested and lets
 * the loop run the teardown after the current frame.
 *
 * This header is included by nearly every module. Besides the STATUS_*
 * codes (MyGameStatus and the per-player status sent over the network) it
 * carries the shared libc includes, the u_int*_t / DWORD / BYTE / WORD
 * aliases, MAGIC_NUMBER (the "PRJX" tag at the start of the engine's level
 * and model data files) and GLOBAL_SCALE (the factor the code's hard-coded
 * sizes, speeds and distances are multiplied by) and BLOCKSIZE (one level
 * block, 1024 units before scaling; most ranges are written in blocks).
 *
 * MAX and MIN expand without outer parentheses; use them only as a whole
 * expression or wrap the call.
 */


/*===================================================================
	Game Status Defines...
===================================================================*/

constexpr int STATUS_UNUSED = 0;
constexpr int STATUS_Title = 2; // On the Title Screen...
constexpr int STATUS_StartingMultiplayer = 4; // On the Multiplayer Starting Screen...
constexpr int STATUS_PreStartingMultiplayerSynch = 5; // Waiting for everyone to have loaded the right level and be ready
constexpr int STATUS_StartingMultiplayerSynch = 6; // Waiting for everyone to have loaded the right level and be ready
							// to Start...

constexpr int STATUS_QuitCurrentGame = 7; // About to quit a game and return to the title screen..
constexpr int STATUS_GetPlayerNum = 8; // Trying to aquire a Player Number...
constexpr int STATUS_Normal = 10; // normal in game...
constexpr int STATUS_LevelEnd = 11; // the level has ended tell the host I know the level has ended...
constexpr int STATUS_BetweenLevels = 12; // in title room between levels

constexpr int STATUS_ViewingScore = 13; // tell the host I am looking at the Score....
constexpr int STATUS_WaitingAfterScore = 14; // waiting for other players or waitng for the host to start...
constexpr int STATUS_Joining = 15; // I Want to join your game....
constexpr int STATUS_Left = 16; // I have left your game...
constexpr int STATUS_ChangeLevelPostInitView = 17; // after a change level init view

constexpr int STATUS_StartingSinglePlayer = 32; // Starting Single Player in game...
constexpr int STATUS_PostStartingSinglePlayer = 33; // Starting Single Player in game after the level has been loaded...
constexpr int STATUS_SinglePlayer = 34; // Single Player in game...
constexpr int STATUS_ViewingStats = 35; // Viewing Stats Single Player...

constexpr int STATUS_TitleLoadGameStartingSinglePlayer = 36;
constexpr int STATUS_TitleLoadGamePostStartingSinglePlayer = 37;
constexpr int STATUS_InGameLoadGameStartingSinglePlayer = 39;

constexpr int STATUS_WaitingToStartSinglePlayer = 48; // waiting to start single player game...
constexpr int STATUS_WaitingToStartMultiPlayerHost = 49; // waiting to start multi player game for host...
constexpr int STATUS_WaitingToStartMultiPlayerClient = 50; // waiting to start multi player game for client...
constexpr int STATUS_WaitingToStartTeamGame = 51; // waiting to start a team game...
constexpr int STATUS_QuittingFromTitles = 52; // quitting game from title screen
constexpr int STATUS_WaitingToStartEndGameSequence = 54; // waiting to start end of game demo playback sequence

constexpr int STATUS_InitView_0 = 128; // 0 of Preping for a level..
constexpr int STATUS_InitView_9 = 137; // 9 of Preping for a level..

constexpr int STATUS_LeftCrashed = 254; // I have left and crashed...
constexpr int STATUS_Null = 255; // Im not doing anything yet...

#define STATUS_VALID_PLAYER( x ) \
	(x != STATUS_GetPlayerNum && x != STATUS_Joining && x != STATUS_Left && \
	 x != STATUS_LeftCrashed && x != STATUS_Null && x != STATUS_UNUSED)

/*===================================================================
	Some Global Defines...
===================================================================*/

constexpr float START_FOV = (90.0F); // initial horizontal field-of-view (in degrees)
constexpr float MAX_FOV = (120.0F); // max horizontal field-of-view (in degrees)

constexpr int MAXLEVELS = (64);
constexpr auto MAGIC_NUMBER = 0x584A5250;

constexpr float GLOBAL_SCALE = (0.25F);
constexpr float BLOCKSIZE = ( 1024.0F * GLOBAL_SCALE ); // one level block

#undef MAX
#define MAX(x, y) ((x) > (y)) ? (x) : (y)

#undef MIN
#define MIN(x, y) ((x) > (y)) ? (y) : (x)

#undef ZERO_STACK_MEM
#define ZERO_STACK_MEM(x) memset(&x, 0, sizeof(x))


#include "math/math_error.h"

// c99 boolean support
#include <stdbool.h>

// on osx trying to include malloc will not work
// this brings in malloc on linux and osx
#include <stdlib.h>

// xmem also needs strdup to be pulled in first
#include <string.h>

// should be included after malloc/stdlib
#include "util/xmem.h"

#include <stdint.h>
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef u_int32_t DWORD;
typedef u_int8_t  BYTE;
typedef u_int16_t WORD;

/* The 'none' value of a 16-bit index: the end of a linked list, no group,
   no model, no light, no owner. */
constexpr u_int16_t INDEX_NONE = (u_int16_t) -1;

/* Defined in main.cpp; shared with the modules that use it. */
extern bool Debug;
extern bool ShowFrameRate;
extern bool ShowInfo;
void CleanUpAndPostQuit(void);

/* Set when the player or the system asks to quit; the main loop exits. */
extern bool QuitRequested;
