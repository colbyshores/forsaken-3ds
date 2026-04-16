
#ifndef LOADSAVE_INCLUDED
#define LOADSAVE_INCLUDED
/*
 * Defines
 */
#define SAVEGAME_SLOTS
#include "main.h"

#ifdef __3DS__
/* 3DS: saves go to SD card (romfs is read-only) */
#define SAVEGAME_FOLDER			"sdmc:/3ds/forsaken/savegame"
#else
#define SAVEGAME_FOLDER			"Savegame"
#endif
#define SNAPSHOT_FOLDER			"ScreenShots"
#define FMVSNAPSHOT_FOLDER		"ScreenShots"
#define SAVEGAME_EXTENSION		".SAV"
#define SAVEGAME_FILESPEC		"save??"
#define SAVEGAMEPIC_EXTENSION	".PPM"

#define	LOADSAVE_VERSION_NUMBER	5

/*
 * fn prototypes
 */
void InGameLoad( MENUITEM * MenuItem );
bool InGameSave( MENUITEM * MenuItem );
bool PreInGameLoad( MENUITEM * MenuItem );
char *SaveGameFileName( int slot );
char *SaveGamePicFileName( int slot );
char *SavedGameInfo( int slot );
char *GetMissionName( char *levelname );
bool SaveGameSlotUsed( int slot );
#endif	// LOADSAVE_INCLUDED

