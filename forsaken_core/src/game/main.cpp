/* main.cpp -- program entry, command line and main loop; see main.h. */

#include "game/main.h"
#ifdef __3DS__
#include "platform_3ds/main_3ds.h"
#include <3ds.h>
#else
#include "main_sdl.h"
#include <SDL.h>
#endif
#include <stdio.h>
#include <string.h>
#ifndef __3DS__
#include <search.h>
#endif
#include "ui/version.h"
#include "rendering/render.h"
#include "util/util.h"
#include "data/file.h"
#include "net/net.h"
#include "ui/title.h"
#include "scripting/lua_common.h"
#include "audio/sfx.h"
#include "input/input.h"
#include "audio/sound.h"
#include "audio/music.h"

#include <unistd.h>
#include "game/oct2.h"
#include "game/skin.h"
#include "net/multiplayer.h"
#include "net/networking.h"


//
// GLOBAL VARIABLES
//

bool Debug = true;
bool ShowFrameRate = false;
bool ShowInfo = false;

int cliSleep = 0;

render_info_t render_info;

//
// Parses the directory to change to from the command line options
//

static bool parse_chdir( char *cli )
{
	char * option;
	char cmdline[256];
	size_t size;

	size = strlen(cli)+1;
	if ( size > sizeof(cmdline) )
	{
		Msg("Command line to long!");
		return false;
	}
	copy_string(cmdline, sizeof( cmdline ),cli);

	option = strtok(cmdline, " -+'\"");

	while(option != nullptr )
	{
		// WARNING:	chdir can only be the LAST option !
		// For running the binary from outside the game folder:
		// -chdir "/path/to/forsaken"
		if (!strcasecmp(option,"chdir"))
		{
			// get option enclosed in quotes
			// this does not consider space as separator
			option = strtok(nullptr, "\"'");

			if (!option)
			{
				Msg("Error using chdir");
				return false;
				break;
			}

			// change to root directory
			// the rest of the command line will be used as the path
			if( chdir( option ) != 0 )
			{
				// error
				Msg("Could not change to directory: %s", option);
				return false;
			}

			// dont loop anymore were done
			break;
		}
		// get the next token
		option = strtok(nullptr, " -+'\"");
	}

	return true;
}

//
// checks for all critical folders
//

constexpr int CRITICAL_FOLDERS = 4;
static bool missing_folders( void )
{
	int x = 0;
	const char* folders[CRITICAL_FOLDERS] = {"configs","data","pilots","scripts"};
	for( x = 0; x < CRITICAL_FOLDERS; x++ )
		if( ! is_folder(folders[x]) )
		{
			Msg("Could not locate the '%s' folder...\n%s\n%s", folders[x],
				"exe is most likely in the wrong directory.",
				"or you just need to create the folder.");
		return true;
	}
	return false;
}


//
// Parse the Command Line
//

static bool ParseCommandLine(char* args)
{

	//
	//  Locals
	//

	char * option;
	char cmdline[256];
	size_t size;

	//
	//  Set Global Defaults
	//

	NoSFX					= false; // turns off sound
	Debug					= false; // turns it off now

	NetUpdateIntervalCmdLine	= 0;

	//
	// Get the command line string
	//

	size = strlen(args)+1;
	if ( size > sizeof(cmdline) )
	{
		Msg("Command line to long!");
		return false;
	}
	copy_string(cmdline, sizeof( cmdline ),args);

	//
	// extract and process tokens from command line
	//

	option = strtok(cmdline, " -+'\"");

	// loop over every option
	while(option != nullptr )
	{

		// last option
		if (!strcasecmp(option,"chdir"))
		{
			// dont loop anymore were done
			break;
		}

		// turn on vertical syncing
		else if (!strcasecmp(option,"VSync")){
			render_info.vsync = true;
		}

		// debugging information send to Log...
		else if (!strcasecmp(option, "log"))
		{
			DebugLog = true;
		}

		// debugging information
		else if (!strcasecmp(option, "Debug"))
		{
			Debug = true;
		}

		// start in fullscreen mode
		else if (!strcasecmp(option,"Fullscreen"))
		{
			render_info.fullscreen = true;
		}

		// start in window mode
		else if (!strcasecmp(option,"Window"))
		{
			render_info.fullscreen = false;
		}

		// turn off sound
		else if (!strcasecmp(option, "NoSFX"))
		{
			NoSFX = true;
		}

		// jump to the host screen
		else if ( !strcasecmp( option, "QuickHost" ) )
		{
			QuickStart = QuickStartMode::Start;
		}

		// jump to the join game screen
		else if ( !strcasecmp( option, "QuickJoin" ) )
		{
			QuickStart = QuickStartMode::Join;
		}

		// set the ip address for game to join
		else if ( !strcasecmp( option, "TCP" ) )
		{
			char * port;
			char address[255];

			IpOnCLI = true;

			// extract the address
			option = strtok(nullptr, " ");
			copy_string( address, sizeof( address ), option );

			// try to find a port in the address
			port = strchr(address,':');

			// if port found assign it
			if( port )
			{
				*port = 0; // separate hostname from port
				copy_string( (char*) host_port_str.text, sizeof( host_port_str.text ), ++port );
			}
			// other wise set default port
			else
			{
				snprintf( (char*) host_port_str.text, sizeof( host_port_str.text ), "%d", NETWORK_DEFAULT_PORT );
			}

			// copy in the hostname
			copy_string( (char*)TCPAddress.text, sizeof( TCPAddress.text ), address );
		}

		// special override to allow setting up of spaceorb
		else if ( !strcasecmp( option, "SetupSpaceOrb" ) )
		{
			SpaceOrbSetup = true;
		}

		// use sscanf
		else
		{
			int w,h;

			// override local port
			if ( sscanf( option, "port:%s", (char*)&local_port_str.text[0] ) )
			{
				DebugPrintf("Command Line: local port set to %s\n", local_port_str.text);
			}

			// sleep time for every loop
			else if ( sscanf( option, "sleep:%d", &cliSleep )){}

			// select the pilot
			else if ( sscanf( option , "pilot:%s", config_name )){}

			// set the packets per second
			else if ( sscanf( option, "PPS:%d", &NetUpdateIntervalCmdLine ) ){}

			// resolution mode
			// must be a valid resolution list in the resolution list in game
			// other wise you will end up with the default highest possible resolution
			// note: if you pick a value not in the list then your window will be 1 size and your resolution another
			else if ( sscanf( option, "mode:%d:%d", &render_info.default_mode.w, &render_info.default_mode.h ) ){}

			// let user fix up aspect
			else if ( sscanf( option, "aspect:%d:%d", &w, &h ) )
			{
				DebugPrintf("cli: aspect ratio set to %d:%d\n",w,h);
				render_info.aspect_ratio = (float) w / (float) h;
			}

			// texel inset mload applies to level UVs (default UV_FUDGE)
			else if ( sscanf( option, "UVFix:%f", &UV_Fix ) ){}

			// set the horizontal frame of view
			// this is the screen stretching when you go into nitro
			// default is 90... max is 120...
			else if ( sscanf( option, "fov:%f", &normal_fov ) ){}

			//
			else {
				DebugPrintf("cli: unknown option: %s\n",option);
			}
		}

		// get the next token
		option = strtok(nullptr, " -+'\"");

	}

	return true;
}

//
// Cleans up the application before quiting
//


bool QuitRequested = false;
void CleanUpAndPostQuit(void)
{
	/* Idempotent — a separate "cleaned up" flag guards re-entry.
	 * QuitRequested is now just the signal telling the main loop to
	 * exit, not a proxy for "teardown done". */
	static bool _cleaned = false;
	if (_cleaned)
		return;
	_cleaned = true;

	// kill stuff
	ReleaseView();

	// stop rendering and destroy objects
	render_cleanup( &render_info );

	// destroy the sound
	DestroySound( DESTROYSOUND_All );

	// close the joysticks / game pads
	joysticks_cleanup();

	// release the scene
	ReleaseScene();

	// set flag
	QuitRequested = true;

	// we dont control the cursor anymore
	input_grab( false );

	// close up lua
	lua_shutdown();

	// cleanup networking
	network_cleanup();


#ifdef __3DS__
	// cleanup music
	music_shutdown();
#endif

	// cleanup sound system
	sound_destroy();


	// should come last
#ifdef __3DS__
	platform_shutdown();
#else
	SDL_Quit();
#endif
}

//
// Initializes the application
//


#include "data/mload.h"

static bool AppInit( char * args )
{
#ifdef DEBUG_ON
	InitMathErrors();
#endif

	ZERO_STACK_MEM(render_info);
	ZERO_STACK_MEM(RenderBufs);
	ZERO_STACK_MEM(Portal_Execs);
	ZERO_STACK_MEM(Skin_Execs);

	render_info.vsync = false;



#ifdef DEBUG_ON

	// special debuggin routines
	XMem_Init();

#endif

#ifdef __3DS__
	if(!platform_init()) {
		return false;
	}
#else
	if(!sdl_init())
		return false;
#endif

	trig_table_init();

	// parse chdir from command line first
	if(!parse_chdir(args)) {
		return false;
	}

	// we are now in the skeleton folder
	// now we need to see if we are in right place
	// check for missing folders
	if(missing_folders()) {
		return false;
	}

	// startup lua
	if( lua_init() != 0 ) {
		return false;
	}

	// copy game settings from config
	GetGamePrefs();

	// our configs are now loaded
	// now we can check the command line for overrides

	// parse the command line
	if(!ParseCommandLine(args)) {
		return false;
	}

	//
	// create and show the window
	//
#ifdef __3DS__
	if(!platform_init_video())
	{
		Msg("platform_init_video() returned false");
		return false;
	}
#else
	if(!sdl_init_video())
	{
		Msg("sdl_init_video() returned false");
		return false;
	}
#endif

	if (!joysticks_init())
	{
		Msg("Failed to initialized joysticks!");
		return false;
	}

	// this needs to come after joysticks_init
	// because joysticks_init will wipe the joystick settings
	GetDefaultPilot();

	// start the title scene
	MyGameStatus = STATUS_Title;

	if (!InitScene()) {
		return false;
	}

	music_init();

	// load the view
	if (!InitView() )
	{
		Msg("InitView failed.\n");
		return false;
	}

	// exclusively grab input in fullscreen mode
	input_grab( render_info.fullscreen );

	//
	SetSoundLevels( nullptr );

	// done
	DebugPrintf("AppInit finished...\n");
	return true;

}

//
// Render the next frame and update the window
//

static bool RenderLoop()
{
	if ( !render_info.ok_to_render || render_info.minimized || render_info.bPaused || QuitRequested )
		return true;

	// run one game frame (game logic + draw) for the current status
	if (!RenderScene())
	{
		Msg("RenderScene failed.\n");
		return false;
	}

	if ( quitting )
	{
		quitting = false;
		CleanUpAndPostQuit();
	}

	// present the frame
	if( !QuitRequested )
	{
		{
			// this is the actual call to render a frame...
			{
				if (!render_flip(&render_info))
				{
					Msg("RenderLoop: render_flip() failed\n");
					return false;
				}
			}
		}
	}

	//
	return true;
}

//
// The main routine
//


int main( int argc, char* argv[] )
{
#ifdef __3DS__

	/* R1 (AGENTS.md §3): establish FPSCR Flush-to-Zero (FZ) + Default-NaN
	 * (DN) on this thread before any float math runs. Denormal results
	 * stall the VFP11; FZ|DN are this engine's chosen numerical mode.
	 * FPSCR is per-thread state, so every thread that touches float
	 * establishes it at its own entry (workers do the same). Modern
	 * libctru initializes it on supported entry paths; this is the
	 * defensive pin with read-back, not a speedup claim. Refuse to run if
	 * the bits cannot be verified rather than silently run in a different
	 * numerical mode. */
	if (!vfp_set_fz_dn())
		return 1;
#endif
	int i;
	char cli[500];
	int failcount = 0; // number of times RenderLoop has failed

	// build cli string
	// TODO - cli parsing should be updated
	copy_string( cli, sizeof( cli ), " " );
	for ( i=1; i<argc; i++ )
	{
		if( strlen(cli) + strlen(argv[i]) -1 > 500 )
		{
			DebugPrintf("Stopped parsing cli options at argc='%s' because greator than 500 characters\n",argv[i]);
			break;
		}
		append_string( cli, sizeof( cli ), " " );
		append_string( cli, sizeof( cli ), argv[i] );
	}
	DebugPrintf("cli: %s\n",cli);

	// Create the window and initialize all objects needed to begin rendering
	if(!AppInit(cli))
		goto FAILURE;

#ifdef __3DS__
	/* aptMainLoop() must gate the main loop on 3DS. It handles HOME menu
	 * display, suspend on sleep-mode, and resume-from-sleep restoration.
	 * Without this, the app never yields to the OS for these events —
	 * HOME button is ignored and closing the lid freezes the game.
	 * Returns false when the OS signals the app should exit (user picked
	 * "Close" from HOME, or the console is being powered off). */
	while( aptMainLoop() && !QuitRequested )
#else
	while( !QuitRequested )
#endif
	{
		// process system events
		if(!handle_events())
			goto FAILURE;

		// Attempt to render a frame, if it fails, take a note.  If
		// rendering fails more than twice, abort execution.
		if( !RenderLoop() )
		{
			++failcount;

			if( SeriousError )
			{
				CleanUpAndPostQuit();
				break;
			}

			if (failcount == 3) {
				DebugPrintf("Rendering has failed too many times.  Aborting execution.\n");
				CleanUpAndPostQuit();
				break;
			}

		}

#ifdef __3DS__
		// no-op on both targets: a 3DS worker thread refills the music stream
		music_update();
#endif

		// command line asks us to sleep and free up sys resources a bit...
		if ( cliSleep )
#ifdef __3DS__
			platform_delay( cliSleep );
#else
			SDL_Delay( cliSleep );
#endif

	}

	/* Normal-exit teardown. On 3DS, QuitRequested is set by the
	 * Start+Select input handler or aptMainLoop returning false
	 * (HOME → Close). Cleanup runs ONCE here after the render loop
	 * has finished its current iteration so no freed resources get
	 * touched. */
	CleanUpAndPostQuit();

	DebugPrintf("exit(0)\n");

	return 0;

	FAILURE:

	CleanUpAndPostQuit();

#ifdef DEBUG_ON

	DebugMathErrors();

	if ( UnMallocedBlocks() )
		DebugPrintf( "Un-malloced blocks found!\n" );

#endif

	DebugPrintf("exit(1)\n");

	return 1;
}
