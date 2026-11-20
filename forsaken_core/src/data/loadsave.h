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
bool InGameSave( MENUITEM * MenuItem );
bool PreInGameLoad( MENUITEM * MenuItem );
char *SaveGameFileName( int slot );
char *SaveGamePicFileName( int slot );
char *SavedGameInfo( int slot );
char *GetMissionName( char *levelname );
bool SaveGameSlotUsed( int slot );

/* What the saved-game loader has to tell the level it restarts (one
 * instance, SaveGames, defined in loadsave.cpp). When InGameLoad refuses a
 * truncated or damaged save, oct2.cpp restarts that level from the beginning
 * and notes it here; STATUS_PostStartingSinglePlayer then shows
 * SAVED_GAME_DAMAGED once. */
class SavedGames {
	public:
	// InGameLoad refused the save; its level is being started again
	void note_damaged_save() { SavedGameDamaged = true; }
	// the restarted level has not yet said why
	bool damaged_save_pending() const { return SavedGameDamaged; }
	// the restarted level has said why
	void damaged_save_reported() { SavedGameDamaged = false; }

	private:
	bool SavedGameDamaged = false;
};

extern SavedGames SaveGames;
