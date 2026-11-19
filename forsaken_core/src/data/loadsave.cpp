/* loadsave.cpp -- single-player saved games
 *
 * InGameSave writes the whole running level to SAVEGAME_FOLDER\saveNN.SAV
 * (slot NN = LoadSavedGameList.selected_item + 1) plus a screenshot
 * (saveNN.PPM) for the load menu. PreInGameLoad reads just the level name so
 * the level can be loaded first; InGameLoad then restores the state over it.
 * SavedGameInfo reads the header for the menu line.
 *
 * Save file (values in the machine's own byte order and sizes):
 *   magic, version (u32 each; LOADSAVE_VERSION_NUMBER)
 *   level short name, pilot name (NUL-terminated)
 *   hours, minutes, seconds of level time, enemies at start, enemies
 *     killed, lives (i16 each)
 *   then each module's own section, written and read in this order by its
 *   Save* / Load* pair: ships, enemies, sound effects, texture animations,
 *   start / restart points, remote cameras, screen polys, trigger areas,
 *   external forces, teleports, real-time lights, lights, triggers,
 *   background objects, pickups, primary bullets, secondary bullets,
 *   models, polys, face-me polys, spot effects, text messages
 *   Cheated (bool)
 *
 * The sections hold values and array indices only, never pointers
 * (checked 2026-11-18 on four levels: each routine's output scanned for
 * address-like values across two runs, and every field type checked).
 * A file with the wrong magic or version is still loaded after a warning.
 * InGameLoad returns false when a read runs past the end of the file (it
 * checks between modules) or a module's loader fails; the level has then
 * been partly restored, and oct2.cpp starts it again from the beginning.
 * InGameSave writes saveNN.TMP and renames it over saveNN.SAV when complete.
 */

/*===================================================================
		Include Files...
===================================================================*/
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/main.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "data/file.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "game/enemies.h"
#include "data/mload.h"

#include "game/ships.h"
#include "ui/title.h"
#include "ui/local.h"
#include "rendering/2dpolys.h"
#include "rendering/polys.h"
#include "rendering/screenpolys.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/spotfx.h"
#include "game/pickups.h"
#include "data/loadsave.h"
#include "rendering/rtlight.h"
#include "game/teleport.h"
#include "game/extforce.h"
#include "game/camera.h"
#include "game/restart.h"
#include "audio/sfx.h"
#include "rendering/text.h"
#include "data/file.h"
#include "util/util.h"
#include "util/timer.h"
#include "game/oct2.h"

/*===================================================================
		Defines
===================================================================*/
constexpr int MAX_SAVEGAME_FILENAME = 128;

/*===================================================================
		Globals ...
===================================================================*/


char *SaveGameFileName( int slot )
{
	static char name[ MAX_SAVEGAME_FILENAME ];

	snprintf( name, sizeof( name ), SAVEGAME_FOLDER "\\save%02d" SAVEGAME_EXTENSION, slot + 1 );
	strtoupper( name );
	return name;
}


char *SaveGamePicFileName( int slot )
{
	static char name[ MAX_SAVEGAME_FILENAME ];

	snprintf( name, sizeof( name ), SAVEGAME_FOLDER "\\save%02d" SAVEGAMEPIC_EXTENSION, slot + 1 );
	strtoupper( name );
	return name;
}


/* After one module's Load*: false if the module failed (it returns NULL,
   having closed fp) or any read so far ran past the end of the file or
   failed (fp's end-of-file and error flags stay set once a read does), in
   which case fp is closed here. */
static bool load_ok( FILE * fp )
{
	if( !fp )
		return false;
	if( feof( fp ) || ferror( fp ) )
	{
		fclose( fp );
		return false;
	}
	return true;
}

/* Set when InGameLoad refused a save and the level was restarted, so the
   restarted level can say so (oct2.cpp STATUS_PostStartingSinglePlayer). */
bool SavedGameDamaged = false;

bool PreInGameLoad( MENUITEM * /*MenuItem*/ )
{
	FILE	*	fp;
	int16_t		i;
	char		buf[ 256 ];
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;

	fp = file_open( SaveGameFileName( LoadSavedGameList.selected_item ), "rb" );

	if( fp != nullptr )
	{
		file_read( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		file_read( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != LOADSAVE_VERSION_NUMBER  )
			|| !file_read_cstring( fp, buf, sizeof( buf ) ) )
		{
			fclose( fp );
			return false;
		}

		for (i = 0; i < NumLevels; i++)
		{

			if( strcasecmp( ShortLevelNames[i], buf ) == 0 )
			{
				NewLevelNum = i;
				break;
			}
		}

		if( NewLevelNum == -1 )
		{
			fclose( fp );
			return false;
		}
		fclose( fp );
		return true;
	}
	return false;
}

/*===================================================================
	Procedure	:	InGame Load
	Input		:	MENUITEM * MenuItem
	Output		:	nothing
===================================================================*/
bool InGameLoad( MENUITEM * /*MenuItem*/ )
{
	FILE	*	fp;
	char		buf[ 256 ];
	char		biker[ 256 ];
	int16_t		Hours, Minutes, Seconds;
	int16_t		KilledEnemiesNum, InitEnemiesNum;
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;

	fp = file_open( SaveGameFileName( LoadSavedGameList.selected_item ), "rb" );

	if( fp != nullptr )
	{
		file_read( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		file_read( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != LOADSAVE_VERSION_NUMBER  )
			|| !file_read_cstring( fp, buf, sizeof( buf ) ) || !file_read_cstring( fp, biker, sizeof( biker ) ) )
		{
			fclose( fp );
			return false;
		}

		file_read( &Hours, sizeof( Hours ), 1, fp );
		file_read( &Minutes, sizeof( Minutes ), 1, fp );
		file_read( &Seconds, sizeof( Seconds ), 1, fp );
		file_read( &InitEnemiesNum, sizeof( NumInitEnemies ), 1, fp );
		file_read( &KilledEnemiesNum, sizeof( NumKilledEnemies ), 1, fp );
		file_read( &Lives, sizeof( Lives ), 1, fp );

		DebugPrintf( "Loading Ships\n" );

		if( !load_ok( fp ) ) return false;	// the header
		fp = LoadShips( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Enemies\n" );

		if( !Enemy_Load( fp ) )
			return false;	// Enemy_Load closed fp
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading LoadSFX\n" );

		fp = LoadAllSfx( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Texture Animations\n" );

		fp = LoadTextureAnimations( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading RestartPoints\n" );

		fp = LoadStartRestartPoints( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading RemoteCameras\n" );

		fp = LoadRemoteCameras( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading ScreePolys\n" );

		fp = LoadScreenPolys( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading TriggerAreas\n" );

		fp = LoadTriggerAreas( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading ExternalForces\n" );

		fp = LoadExternalForces( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Teleports\n" );

		fp = LoadTeleports( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Realtime Lights\n" );

		fp = LoadRealTimeLights( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading XLights\n" );

		fp = LoadXLights( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Triggers\n" );

		fp = LoadAllTriggers( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading BGObjects\n" );

		fp = LoadBGObjects( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Pickups\n" );

		fp = LoadAllPickups( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading PrimBulls\n" );

		fp = LoadPrimBulls( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading SecBulls\n" );

		fp = LoadSecBulls( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Models\n" );

		fp = LoadModels( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Polys\n" );

		fp = LoadPolys( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading FmPolys\n" );

		fp = LoadFmPolys( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading SpotFX\n" );

		fp = LoadAllSpotFX( fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loading Text\n" );

		fp = LoadAllText( fp );
		if( !load_ok( fp ) ) return false;

		file_read( &Cheated, sizeof( bool ), 1, fp );
		if( !load_ok( fp ) ) return false;

		DebugPrintf( "Loaded OK\n" );
		fclose( fp );

		timer_clear( &countdown_timer );
		return true;
	}
	return false;
}


char *GetMissionName( char *levelname )
{
	static char RealLevelName[ 256 ];
	FILE *f;
	int16_t i;

	for (i = 0; i < NumLevels; i++)
	{

		if( strcasecmp( (char*) &ShortLevelNames[i][0] , levelname ) == 0 )
		{
			break;
		}
	}
	if ( i >= NumLevels )
		return levelname;

	f = file_open( MissionTextNames[ i ], "r" );

	RealLevelName[ 0 ] = 0;
	if( f )
	{
		i = 0;
		do
		{
			RealLevelName[ i ] = fgetc( f );
			i++;
		}
		while( ( ( RealLevelName[ i - 1 ] != 13 ) && ( RealLevelName[ i - 1 ] != 10 ) ) );

		if( i )
		{
			RealLevelName[ i - 1 ] = 0;
		}

		fclose( f );
	}
	return RealLevelName;
}


/*===================================================================
	Procedure	:	InGame Save
	Input		:	MENUITEM * MenuItem
	Output		:	bool	True/False
===================================================================*/
bool InGameSave( MENUITEM * /*MenuItem*/ )
{
	FILE	*	fp;
	FILE	*	f;
	int16_t		i;
	int8_t		RealLevelName[ 128 + 1 ];
	int8_t		Filename[ 256 ];
	int8_t		TmpFilename[ 256 ];
	int8_t		SFilename[ 256 ];
	FILE	*	out;
	int16_t		Hours, Minutes, Seconds;
	int8_t		sep = 0;
	u_int32_t		MagicNumber = MAGIC_NUMBER;
	u_int32_t		VersionNumber = LOADSAVE_VERSION_NUMBER;

	f = file_open( MissionTextNames[ NewLevelNum ], "r" );

	if( f )
	{
		i = 0;
		do
		{
			RealLevelName[ i ] = fgetc( f );
			i++;
		}
		while( ( ( RealLevelName[ i - 1 ] != 13 ) && ( RealLevelName[ i - 1 ] != 10 ) ) );

		if( i )
		{
			RealLevelName[ i - 1 ] = 0;
		}

		fclose( f );

		Minutes = ( u_int16_t ) ( LevelTimeTaken / 60 );
		Seconds = ( u_int16_t ) LevelTimeTaken;
		Hours = ( u_int16_t ) ( Minutes / 60 );
		Minutes = ( u_int16_t ) ( Minutes % 60 );

		copy_string( (char*)Filename, sizeof( Filename ), SaveGameFileName( LoadSavedGameList.selected_item ) );
		copy_string( (char*)SFilename, sizeof( SFilename ), SaveGamePicFileName( LoadSavedGameList.selected_item ) );

		DebugPrintf( "%s\n", &Filename[ 0 ] );
	}
	else
	{
		return false;
	}

	if ( !folder_exists( SAVEGAME_FOLDER ) )
	{
		return false;
	}

	if( !SaveSnapShot( &SFilename[ 0 ] ) )
	{
		return false;
	}

	/* Write to a temporary file and replace the slot's save only once the
	 * whole state is written, so a failure part way (a full card, the console
	 * switched off) leaves the previous save intact instead of a truncated
	 * one. The Save* calls hand fp back, or NULL on failure; out is kept to
	 * close the file either way. */
	Change_Ext( (char*)Filename, (char*)TmpFilename, sizeof( TmpFilename ), ".TMP" );	// saveNN.SAV -> saveNN.TMP
	out = fp = file_open( (char*)&TmpFilename[ 0 ], "wb" );

	if( fp != nullptr )
	{
		fwrite( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		fwrite( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		i = 0;
		do
		{
			fwrite( &ShortLevelNames[NewLevelNum][i], sizeof(char), 1, fp );
		}
		while( ShortLevelNames[NewLevelNum][i++] != 0 );

		fprintf( fp, "%s", biker_name );
		fwrite( &sep, sizeof( sep ), 1, fp );
		fwrite( &Hours, sizeof( Hours ), 1, fp );
		fwrite( &Minutes, sizeof( Minutes ), 1, fp );
		fwrite( &Seconds, sizeof( Seconds ), 1, fp );
		fwrite( &NumInitEnemies, sizeof( NumInitEnemies ), 1, fp );
		fwrite( &NumKilledEnemies, sizeof( NumKilledEnemies ), 1, fp );
		fwrite( &Lives, sizeof( Lives ), 1, fp );

		fp = SaveShips( fp );
		if( !fp ) goto fail;
		if( !Enemy_Save( fp ) )
			goto fail;
		fp = SaveAllSfx( fp );
		if( !fp ) goto fail;
		fp = SaveTextureAnimations( fp );
		if( !fp ) goto fail;
		fp = SaveStartRestartPoints( fp );
		if( !fp ) goto fail;
		fp = SaveRemoteCameras( fp );
		if( !fp ) goto fail;
		fp = SaveScreenPolys( fp );
		if( !fp ) goto fail;
		fp = SaveTriggerAreas( fp );
		if( !fp ) goto fail;
		fp = SaveExternalForces( fp );
		if( !fp ) goto fail;
		fp = SaveTeleports( fp );
		if( !fp ) goto fail;
		fp = SaveRealTimeLights( fp );
		if( !fp ) goto fail;
		fp = SaveXLights( fp );
		if( !fp ) goto fail;
		fp = SaveAllTriggers( fp );
		if( !fp ) goto fail;
		fp = SaveBGObjects( fp );
		if( !fp ) goto fail;
		fp = SaveAllPickups( fp );
		if( !fp ) goto fail;
		fp = SavePrimBulls( fp );
		if( !fp ) goto fail;
		fp = SaveSecBulls( fp );
		if( !fp ) goto fail;
		fp = SaveModels( fp );
		if( !fp ) goto fail;
		fp = SavePolys( fp );
		if( !fp ) goto fail;
		fp = SaveFmPolys( fp );
		if( !fp ) goto fail;
		fp = SaveAllSpotFX( fp );
		if( !fp ) goto fail;
		fp = SaveAllText( fp );
		if( !fp ) goto fail;

		fwrite( &Cheated, sizeof( bool ), 1, fp );

		if( ferror( out ) )
			goto fail;
		if( fclose( out ) != 0 )
		{
			delete_file( (char*)TmpFilename );
			return false;
		}
		if( !replace_file( (char*)TmpFilename, (char*)Filename ) )
		{
			delete_file( (char*)TmpFilename );
			return false;
		}
		return true;
	}
	return false;

	fail:
	fclose( out );
	delete_file( (char*)TmpFilename );
	return false;
}


char *SavedGameInfo( int slot )
{
	static char info[ 256 ];
	FILE	*	fp;
	char		buf[ 256 ];
	char		biker[ 256 ];
	int16_t		Hours, Minutes, Seconds;
	int16_t		KilledEnemiesNum, InitEnemiesNum;
	int16_t		Lives;
	char	*	LevelName;
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;

	fp = file_open( SaveGameFileName( slot ), "rb" );

	if( fp != nullptr )
	{
		file_read( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		file_read( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != LOADSAVE_VERSION_NUMBER  )
			|| !file_read_cstring( fp, buf, sizeof( buf ) ) || !file_read_cstring( fp, biker, sizeof( biker ) ) )
		{
			fclose( fp );
			snprintf( info, sizeof( info ), SAVED_GAME_SLOT_DAMAGED, slot + 1 );
			return info;
		}

		LevelName = GetMissionName( buf );

		file_read( &Hours, sizeof( Hours ), 1, fp );
		file_read( &Minutes, sizeof( Minutes ), 1, fp );
		file_read( &Seconds, sizeof( Seconds ), 1, fp );
		file_read( &InitEnemiesNum, sizeof( NumInitEnemies ), 1, fp );
		file_read( &KilledEnemiesNum, sizeof( NumKilledEnemies ), 1, fp );
		file_read( &Lives, sizeof( Lives ), 1, fp );
		if ( strcasecmp( biker, DEFAULT_PLAYER_NAME ) )
			snprintf( info, sizeof( info ), "%s in %s %hd'%02hd %hd/%hd",
				biker,
				LevelName,
				Hours * 60 + Minutes, Seconds,
				KilledEnemiesNum, InitEnemiesNum );
		else
			snprintf( info, sizeof( info ), "%s %hd'%02hd %hd/%hd",
				LevelName,
				Hours * 60 + Minutes, Seconds,
				KilledEnemiesNum, InitEnemiesNum );
		fclose( fp );
	}
	else
	{
		snprintf( info, sizeof( info ), "unused slot %d", slot + 1 );
	}

	return info;
}


/* A slot is used when its file exists, whatever state it is in: the
   player can always save over it. */
bool SaveGameSlotUsed( int slot )
{
	FILE	*	fp;

	fp = file_open( SaveGameFileName( slot ), "rb" );
	if ( fp )
	{
		fclose( fp );
		return true;
	}
	return false;
}
