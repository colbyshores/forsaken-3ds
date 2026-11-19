#pragma once

/* loadsave.h -- single-player saved games (see loadsave.cpp for the file
 * layout). Saves go to SAVEGAME_FOLDER: the SD card on the 3DS (romfs is
 * read-only), the Savegame folder on PC. */

#include "game/main.h"

#ifdef __3DS__
/* 3DS: saves go to SD card (romfs is read-only) */
#define SAVEGAME_FOLDER			"sdmc:/3ds/forsaken/savegame"
#else
#define SAVEGAME_FOLDER			"Savegame"
#endif
#define SAVEGAME_EXTENSION		".SAV"
#define SAVEGAMEPIC_EXTENSION	".PPM"

constexpr int LOADSAVE_VERSION_NUMBER = 5;

/*
 * fn prototypes
 */
bool InGameLoad( MENUITEM * MenuItem );
extern bool SavedGameDamaged;
bool InGameSave( MENUITEM * MenuItem );
bool PreInGameLoad( MENUITEM * MenuItem );
char *SaveGameFileName( int slot );
char *SaveGamePicFileName( int slot );
char *SavedGameInfo( int slot );
char *GetMissionName( char *levelname );
bool SaveGameSlotUsed( int slot );
