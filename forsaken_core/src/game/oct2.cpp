
#include "game/main.h"
#include "util/platform.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "world/node.h"
#include "net/networking.h"
#include "rendering/2dtextures.h"
#include "data/mload.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "game/ships.h"
#include "world/collision.h"
#include "rendering/2dpolys.h"
#include "rendering/lines.h"
#include "rendering/polys.h"
#include "rendering/lights.h"
#include "game/models.h"
#include "rendering/2dtextures.h"
#include "world/visi.h"
#include "data/tload.h"
#include "audio/sfx.h"
#include "rendering/transexe.h"
#include "rendering/text.h"
#include "data/mxaload.h"
#include "game/camera.h"
#include "ui/title.h"
#include "rendering/screenpolys.h"
#include "input/controls.h"
#include "net/net.h"
#include "game/triggers.h"
#include "game/enemies.h"
#include "world/bsp.h"
#include "game/trigarea.h"
#include "net/multiplayer.h"
#include "data/file.h"
#include "ai/ai.h"
#include "game/water.h"
#include "game/spotfx.h"
#include "game/extforce.h"
#include "game/teleport.h"
#include "rendering/rtlight.h"
#include "game/restart.h"
#include "ui/local.h"
#include "game/goal.h"
#include "data/loadsave.h"
#include "game/stats.h"
#include "util/timer.h"
#include "data/file.h"
#include "game/singleplayer.h"
#include "rendering/render.h"
#include "input/input.h"
#include "game/oct2.h"
#ifdef __3DS__
#include "audio/music.h"
#endif

#include "data/config.h"
#include "game/skin.h"
#include "util/xmem.h"
#include "util/util.h"
#include "util/hunk_alloc.h"

void SetCam(int ship, int Cam);

VECTOR OldPos;
float SpeedInterval = 0.0F;
float BikeSpeed = 0.0F;

bool CheatsDisabled = false;
bool WaitingToQuit;

bool Cheated = false;


u_int32_t        AnimOncePerFrame = 0;         // used for stuff that is displayed more than once in a single frame..

extern int Num_StatsMessage_Parts;

BYTE  PreSynchupStatus;
const char *CurrentLevelsList;

px_timer_t level_time;

// Ai Modes

bool  IMustQuit = false;
// Capture The Flag Stuff
bool  CaptureTheFlag	= false;
// End of Capture The Flag

// Bounty Hunt
bool  BountyHunt = false;

const char *InitViewMessages[] = {
	"Loading OffScreen Surfaces" ,
	"Texture Load Prep         " ,
	"Loading Textures          " ,
	"Loading Models            " ,
	"Loading World Mesh        " ,
	"Loading Collision Skins   " ,
	"Init Sound Info           " ,
	"Init Ambient Lighting     " ,
	"Loading Sfx               " ,
	"Everything Else...Nodes   " ,
	""
};

bool  InsideResizeViewport = false;

bool  JustExitedMenu =false;

bool  Inside;
int   Depth;

void CalculateFramelag( void );

float FPS = 0.0F;         // Frames Per Second...
//double  TPS = 0.0;          // Textures Per Second...

#define MYTIMER
#undef MYTIMER

#define LEVELSFILE      "levels.dat"

#define FOV_GROW(A)     ((A) + 10.0F)
#define FOV_SHRINK(A)   ((A) - 10.0F)


bool InitViewport( void );


BYTE  InitView_MyGameStatus;
BYTE  ChangeLevel_MyGameStatus;

#ifdef __3DS__
/* One-shot flag: cleared on the FIRST STATUS_SinglePlayer frame to suppress
 * a spurious SDLK_ESCAPE that can be injected by handle_events() between
 * STATUS_PostStartingSinglePlayer and STATUS_SinglePlayer, which would
 * otherwise open MENU_InGameSingle on the first gameplay frame. */
static bool _gameplay_start_guard = false;
#endif

void ProcessGameKeys( void );


float GetPlayerNumCount1 = 0.0F;
float GetPlayerNumCount2 = 0.0F;
int   GetPlayerNumCount = 0;

px_timer_t framelag_timer;

int i, j; // index counters

bool RearCameraActive;
int MissileCameraActive = 0;
u_int16_t  CameraMissile = INDEX_NONE;
BYTE  TempMissileCam;

bool
Our_CalculateFrameRate(void);

bool ScoreDisplay();
bool InitScoreDisplay();
bool InitStatsDisplay();

void ShowInGameStats();
void ShowDeathModeStats();
void ShowGameStats();
void DisplayNetworkInfo();

int Secrets = 0;
int TotalSecrets = 0;
int CrystalsFound = 0;

bool  ClearZBuffer( void );

bool clear_black(void)
{
	if ( WhiteOut > 0.0f )
		return true;
	return FSClearBlack();
}

bool RenderCurrentCamera( void );

render_viewport_t viewport;
int initfov = 0;
float viewplane_distance;
float hfov = START_FOV;
float chosen_fov = START_FOV;
float normal_fov = START_FOV;

bool  DrawPanel = false;

bool  Panel = true;

bool ChangeLevel( void );

float pixel_aspect_ratio;
float framelag = 0.0F;
float real_framelag = 0.0F;

px_timer_t level_timer;

int16_t   LevelNum = 0 ;
int16_t   NewLevelNum = 0 ;
int16_t   NumLevels = 0;
char  ShortLevelNames[MAXLEVELS][MAX_SHORT_LEVEL_NAME];
char  LevelNames[MAXLEVELS][128];
char  CollisionNames[MAXLEVELS][128];
char  CollisionZNames[MAXLEVELS][128];
char  BspNames[MAXLEVELS][128];
char  ZoneNames[MAXLEVELS][128];
char  WaterNames[MAXLEVELS][128];
char  TextNames[MAXLEVELS][128];
char  MsgNames[MAXLEVELS][128];
char  ExternalForceNames[MAXLEVELS][128];
char  TeleportNames[MAXLEVELS][128];

char MissionTextNames[MAXLEVELS][128];
char MissionTextPics[MAXLEVELS][128];

CamRendering CameraRendering = CamRendering::None;

bool SeriousError = false;
bool DoClipping = true;
bool OnceOnlyChangeLevel = false;

RENDEROBJECT RenderBufs[4];

void InitRenderBufs();
void ReleaseRenderBufs( void );
bool SetMatrixViewPort( void );
void PrintInitViewStatus( BYTE Status );


// if met max kill limit, sets flag to change level
void CheckMetKillLimit();

/*===================================================================
	Weapon Names...
===================================================================*/
char PrimaryNames[7][16] = {
	{"Pulsar" } ,
	{"Trojax" } ,
	{"Pyro" } ,
	{"Trans" } ,
	{"Suss" } ,
	{"Laser" } ,
	{"" }
};
char SecondaryNames[12][16] = {
	{"Mug" } ,
	{"Solaris" } ,
	{"Thief" } ,
	{"Scatter" } ,
	{"Gravgon" } ,
	{"MFRL" } ,
	{"Titan" } ,
	{"Purge" } ,
	{"Pine" } ,
	{"Quantum" } ,
	{"Spider" } ,
	{"" }

};

/*===================================================================
	Panel Description Stuff...
===================================================================*/

float ModeScaleX;
float ModeScaleY;

/*===================================================================
	Off Screen Sufaces...Used to Blit to screen...
===================================================================*/
MLOADHEADER  Mloadheader;
MCLOADHEADER MCloadheader;          //  inner skin collision map...
MCLOADHEADER MCloadheadert0;        //  0 thickness collision map...

bool MainGame();

bool DispTracker( void );


/*===================================================================
  Global Variables
===================================================================*/
/* Capacity of the per-frame batch buffers (RenderBufs[] and the stack
   buffers RenderScene and PrintInitViewStatus draw screen polys with), in
   vertices; their index buffers hold three per vertex. */
constexpr int BATCH_BUFFER_VERTS = 32767;

constexpr auto Near = ( 100.0F * GLOBAL_SCALE );
constexpr auto Far = ( 49152.0F * GLOBAL_SCALE );
constexpr auto MinNear = ( 50.0F * GLOBAL_SCALE );

RENDERMATRIX proj = {
	2.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 2.0F, 0.0F, 0.0F,
	0.0F, 0.0F, Far/(Far-Near), 1.0F,
	0.0F, 0.0F, -Far*Near/(Far-Near), 0.0F };

MATRIX  ProjMatrix = {
	2.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 2.0F, 0.0F, 0.0F,
	0.0F, 0.0F, Far/(Far-Near), 1.0F,
	0.0F, 0.0F, -Far*Near/(Far-Near), 0.0F };


VECTOR  SlideLeft = { -1.0F, 0.0F, 0.0F };
VECTOR  SlideRight  = { 1.0F, 0.0F, 0.0F };
VECTOR  SlideUp   = { 0.0F, 1.0F, 0.0F };
VECTOR  SlideDown = { 0.0F, -1.0F, 0.0F };
VECTOR  Forward   = { 0.0F, 0.0F, 1.0F };
VECTOR  Backward  = { 0.0F, 0.0F,-1.0F };
VECTOR  NullVector  = { 0.0F, 0.0F, 0.0F };

/*===================================================================
===================================================================*/

/*
 * Global projection, view, world and identity matricies
 */

RENDERMATRIX view;
RENDERMATRIX identity = {
	1.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 1.0F, 0.0F, 0.0F,
	0.0F, 0.0F, 1.0F, 0.0F,
	0.0F, 0.0F, 0.0F, 1.0F
};

RENDERMATRIX world = {
	1.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 1.0F, 0.0F, 0.0F,
	0.0F, 0.0F, 1.0F, 0.0F,
	0.0F, 0.0F, 0.0F, 1.0F
};

void stereo_adjust( RENDERMATRIX *m )
{
	float left, right, offset;

	if( render_info.stereo_position != ST_CENTER )
		offset = 0.5f * render_info.stereo_eye_sep / render_info.stereo_focal_dist;
	else
		return;

	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	right = pixel_aspect_ratio * tanf( hfov * (float)M_PI_2/180.0f );
	left = -right;

	switch( render_info.stereo_position )
	{
	case ST_LEFT:
		left -= offset;
		right -= offset;
		break;
	case ST_RIGHT:
		left += offset;
		right += offset;
		break;
	default:
		break;
	}

	m->_31 = (right+left)/(right-left);
}

bool SetFOV( float fov )
{
	float screen_width, screen_height;
	float Scale, NewNear;

	if ( fov <= 1.0F || fov >= 150.0F )
		fov = hfov;

	if (!FSGetViewPort(&viewport))
	{
		Msg( "GetViewport failed.\n%s", render_error_description(0) );
		return false;
	}

	if ( render_info.fullscreen )
	{
		screen_width = (float) render_info.ThisMode.w;
		screen_height = (float) render_info.ThisMode.h;
	}
	else
	{
		screen_width = (float) render_info.WindowsDisplay.w;
		screen_height = (float) render_info.WindowsDisplay.h;
	}

	pixel_aspect_ratio = render_info.aspect_ratio * screen_height / screen_width;
	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	viewplane_distance = (float) ( viewport.Width / ( 2 * tanf( DEG2RAD( fov ) * 0.5 ) ) );
	proj._11 = 2 * viewplane_distance / viewport.Width;
	proj._22 = 2 * viewplane_distance / ( viewport.Height / pixel_aspect_ratio );

	if( fov > START_FOV )
	{
		Scale = ( ( MAX_FOV - fov ) / ( MAX_FOV - START_FOV ) );
		NewNear = ( MinNear + ( ( Near - MinNear ) * Scale ) );
		proj._33 = Far/(Far-NewNear);
		proj._34 = 1.0F;
		proj._43 = -Far*NewNear/(Far-NewNear);
		proj._44 = 0.0F;
	}
	else
	{
		proj._33 = Far/(Far-Near);
		proj._34 = 1.0F;
		proj._43 = -Far*Near/(Far-Near);
		proj._44 = 0.0F;
	}

	stereo_adjust(&proj);

	/* Each backend converts the projection to its own clip conventions in FSSetProjection. */

	ProjMatrix._11 = proj._11;
	ProjMatrix._22 = proj._22;
	ProjMatrix._33 = proj._33;
	ProjMatrix._34 = proj._34;
	ProjMatrix._43 = proj._43;
	ProjMatrix._44 = proj._44;

	hfov = fov;

	if (!FSSetProjection(&proj))
		return false;

	return true;
}

void SetViewportError( const char *where, render_viewport_t *vp )
{
	static char msg[1024];
	sprintf( msg,
		"SetViewport failed in %s.\n"
		"size=%d\n"
		"xpos=%lu ypos=%lu\n"
		"width=%lu height=%lu\n"
		"xscale=%f yscale=%f\n"
		//"xmax=%f ymax=%f\n"
		"zmin=%f zmax=%f\n",
		where,
		(int) sizeof( *vp ),
		(unsigned long) vp->X, (unsigned long) vp->Y,
		(unsigned long) vp->Width, (unsigned long) vp->Height,
		vp->ScaleX, vp->ScaleY,
		vp->MinZ, vp->MaxZ );
	Msg( msg );
}

// read KEYS.txt and DEUBG_KEYS.txt before changing.
void ProcessGameKeys( void )
{

	static int16_t startpos = 0;

	// if were not (!) in normal or single-player mode
	// then don't listen to keys...
	if ( ! (
			 ( MyGameStatus == STATUS_Normal       ) ||
			 ( MyGameStatus == STATUS_SinglePlayer )
		)   )
	{
		return;
	}

	// ESCAPE — toggle in-game pause menu (Start button on 3DS)
	if ( input_buffer_find( SDLK_ESCAPE ) )
	{
		// if were not currently in a menu
		//   ignore keystroke if we just exited the menu
		//   stops going back itno menu from hitting to many times
		if ( !CurrentMenu && !JustExitedMenu )
		{
			// show the multi-player menu
			if( GameStatus[WhoIAm] != STATUS_SinglePlayer )
			{
				MenuRestart( &MENU_InGame );
			}
			// show the single-player menu
			else
			{
				MenuRestart( &MENU_InGameSingle );
			}
			// if were not in the menu
			// and the above check failed
			// reset variable so 3rd escape takes us back in
		}
		else
		{
			JustExitedMenu = false;
		}
	}

	// debuggin keys
	if ( DebugInfo )
	{

		// Shift Modifier
		if ( SDL_GetModState() & KMOD_SHIFT )
		{

			// Shift + F1
			if ( input_buffer_find( SDLK_F1 ) )
			{
				if ( !CurrentMenu )
				{
					MenuRestart( &MENU_DebugHelp );
				}
				else if ( CurrentMenu == &MENU_DebugHelp )
				{
					MenuExit();
				}
			}

			// Shift + Ctrl Modifier
			if ( SDL_GetModState() & KMOD_CTRL )
			{

				// Shift + Ctrl + F2
				if ( input_buffer_find( SDLK_F2 ) )
					SetFOV( chosen_fov = normal_fov );

				// Shift + Ctrl + F3
				if ( input_buffer_find( SDLK_F3 ) )
					SetFOV( chosen_fov = normal_fov );

			} // end shift + ctrl modifier
			else // shift only modifier
			{

				// Shift + F2
				if ( input_buffer_find( SDLK_F2 ) )
					SetFOV( chosen_fov = FOV_GROW( hfov ) );

				// Shift + F3
				if ( input_buffer_find( SDLK_F3 ) )
					SetFOV( chosen_fov = FOV_SHRINK( hfov ) );

			} // end shift only modifier

			// Shift + F4
			if ( input_buffer_find( SDLK_F4 ) )
				if ( !CurrentMenu )
					MenuRestart( &MENU_GotoRoom );

			// Shift + F5
			if ( input_buffer_find( SDLK_F5 ) )
			{
				InitShipStartPos( WhoIAm, ++startpos );
				AddColourMessageToQue(SystemMessageColour,
					"You are now in %s\n",
					Mloadheader.Group[ Ships[ WhoIAm ].Object.Group ].name
					);
			}

			// Shift + F6
			if ( input_buffer_find( SDLK_F6 ) )
				DoClipping = !DoClipping;

			// Shift + F7
			if ( input_buffer_find( SDLK_F7 ) )
				ShowBoundingBoxes = !ShowBoundingBoxes;

			// Shift + F8
			if ( input_buffer_find( SDLK_F8 ) )
				ShowEFZones = !ShowEFZones;

			// Shift + F9
			if ( input_buffer_find( SDLK_F9 ) )
			{
				if( ShowPortal == 4 )
				{
					ShowPortal = 0;
				}
				else
				{
					ShowPortal++;
				}
			}

			// Shift + F10
			if ( input_buffer_find( SDLK_F10 ) )
				ShowColZones = !ShowColZones;


			// Shift + F12
			if( input_buffer_find( SDLK_F12 ) )
				no_collision = !no_collision;

		} // end shift modifier
		// CTRL Modifiers
		else if ( SDL_GetModState() & KMOD_CTRL )
		{

			// Ctrl + F1
			if ( input_buffer_find( SDLK_F1 ) )
				ShowTeleports = !ShowTeleports;

			// Ctrl + F2
			if ( input_buffer_find( SDLK_F2 ) )
				ShowTrigZones = !ShowTrigZones;


		} // end ctrl modifier
		else // no modifiers
		{

			// F8
			if ( input_buffer_find( SDLK_F8 ) )
			{
				// Awesome !!!
				// We need to make this a standard feature!!!
				if( TargetComputerOn )
				{
					TargetComputerOn = false;
				}
				else
				{
					TargetComputerOn = true;
				}
			}
		}

	} // end debug keys
	else // non debugging mode keys
	{

		// F1
		if ( input_buffer_find( SDLK_F1 )  )
		{
			if ( !CurrentMenu )
			{
				MenuRestart( &MENU_Controls );
			}
			else if ( CurrentMenu == &MENU_Controls )
			{
				MenuExit();
			}
		}

		// F2
		if ( input_buffer_find( SDLK_F2 )  )
		{
			if ( !CurrentMenu )
			{
				MenuRestart( &MENU_Options );
			}
			else if ( CurrentMenu == &MENU_Options )
			{
				MenuExit();
			}
		}

		// single player
		if ( MyGameStatus == STATUS_SinglePlayer )
		{

			// F3
			if ( input_buffer_find( SDLK_F3 ) )
			{
				if ( !CurrentMenu )
				{
					MenuRestart( &MENU_LoadSavedGame );
				}
				else if ( CurrentMenu == &MENU_LoadSavedGame )
				{
					MenuExit();
				}
			}

			// F4
			if ( input_buffer_find( SDLK_F4 ) )
			{
				// quick save
				if ( !CurrentMenu )
				{
					MenuRestart( &MENU_SaveGame );
				}
				else if ( CurrentMenu == &MENU_SaveGame )
				{
					MenuExit();
				}
			}

		}


		// F5
		if ( input_buffer_find( SDLK_F5 ) )
			MissileCameraEnable = !MissileCameraEnable;

		// F6
		if ( input_buffer_find( SDLK_F6 ) )
			RearCameraActive = !RearCameraActive;

		// F7
		if ( input_buffer_find( SDLK_F7 ) )
			Panel = !Panel;

		// single player mode
		if( MyGameStatus == STATUS_SinglePlayer )
		{

			// F9
			if ( input_buffer_find( SDLK_F9 ) )
				LastTextMessage();

			// F10
			if ( input_buffer_find( SDLK_F10 ) )
				NextTextMessage();

		} // end single player mode
		else // not single player mode
		{

			// Shift Modifier
			if ( SDL_GetModState() & KMOD_SHIFT )
			{

				// Shift + F9
				if ( input_buffer_find( SDLK_F9 ) )
				{
					if ( !CurrentMenu )
					{
						MenuRestart( &MENU_EditMacro1 );
					}
					else if ( CurrentMenu == &MENU_EditMacro1 )
					{
						MenuExit();
					}
				}

				// Shift + F10
				if ( input_buffer_find( SDLK_F10 ) )
				{
					if ( !CurrentMenu )
					{
						MenuRestart( &MENU_EditMacro2 );
					}
					else if ( CurrentMenu == &MENU_EditMacro2 )
					{
						MenuExit();
					}
				}

				// Shift + F11
				if ( input_buffer_find( SDLK_F11 ) )
				{
					if ( !CurrentMenu )
					{
						MenuRestart( &MENU_EditMacro3 );
					}
					else if ( CurrentMenu == &MENU_EditMacro3 )
					{
						MenuExit();
					}
				}

			} // Shift Modifier
			else // no modifier
			{

				// F9
				if ( input_buffer_find( SDLK_F9 ) )
					SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_Taunt1, 0);

				// F10
				if ( input_buffer_find( SDLK_F10 ) )
					SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_Taunt2, 0);

				// F11
				if ( input_buffer_find( SDLK_F11 ) )
					SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_Taunt3, 0);

			} // end (not) shift modifier
		} // end not single player
	} // end normal keys
} // ProcessGameKeys

bool InitLevels( const char * levels_list )
{

	FILE *f;
	char levels_file[ 256 ];

	if ( !levels_list ){
		levels_list = LEVELSFILE;
	}

	// path to data\\levels\\<type>.bat
	sprintf( levels_file, "data\\levels\\%s", levels_list );

	// set some globals
	CurrentLevelsList = levels_list;

	/////
	// Read Levels From File
	/////

	f = file_open( levels_file, "r" );
	if (!f )
	{
		return false;
	}

	// scan each level name into ShortLevelNames[j]
	j=0;
	while ( j < MAXLEVELS && fscanf( f, " %s", ShortLevelNames[ j ] ) == 1 )
	{


		// get the path to the current mxv file
		sprintf( LevelNames[ j ], "data\\levels\\%s\\%s.mxv", ShortLevelNames[ j ], ShortLevelNames[ j ] );

		// check to see if the level mxv file exists
		if ( !File_Exists( LevelNames[ j ] ) )
			continue;

		// add paths for a level to the lists
		sprintf( CollisionNames[ j ],		"data\\levels\\%s\\%s.mc",    ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( CollisionZNames[ j ],		"data\\levels\\%s\\%sz.mc",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( BspNames[ j ],				"data\\levels\\%s\\%s.bsp",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( ZoneNames[ j ],			"data\\levels\\%s\\%s.zon",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( WaterNames[ j ],			"data\\levels\\%s\\%s.wat",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( TextNames[ j ],			"data\\levels\\%s\\%s.txt",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( MsgNames[ j ],				"data\\levels\\%s\\%s.msg",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( ExternalForceNames[ j ],	"data\\levels\\%s\\%s.gf",    ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( TeleportNames[ j ],		"data\\levels\\%s\\%s.tel",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( MissionTextNames[ j ],		"data\\levels\\%s\\%s.mis",   ShortLevelNames[ j ], ShortLevelNames[ j ] );
		sprintf( MissionTextPics[ j ],		"data\\levels\\%s\\%s.ppm",   ShortLevelNames[ j ], ShortLevelNames[ j ] );

		// increment counter
		j++;

	}

	NumLevels = j;
	fclose( f );


	// no levels were found
	// this should never happen
	// unless you delete all the level directories
	// or the level file doesn't exist
	if( !NumLevels ) return false;

	// select default level
	LevelList.items = 0;
	LevelList.top_item = 0;
	LevelList.display_items = 8;
	LevelList.selected_item = 0;  // you can only ever start game on first level!
	NewLevelNum = 0;

	// add each level to the master list
	for (j = 0; j < NumLevels; j++)
	{
		strcpy( (char*) &LevelList.item[LevelList.items][0] , (char*) &ShortLevelNames[j][0] );
		LevelList.items++;
	}

	// Read TXT in of default level
	LoadLevelText( nullptr );

	// success !
	return true;

}


bool SetMatrixViewPort( void )
{
	FSSetProjection(&proj);
	FSSetView(&identity);
	FSSetWorld(&world);

	world = identity;

	if (!FSGetViewPort(&viewport))
	{
		viewport.Width = 320;
		viewport.Height = 200;
	}

	if ( !initfov )
	{
		SetFOV( chosen_fov = normal_fov );
		initfov = 1;
	}

	if( InsideResizeViewport == false )
	{
		InitViewport();
	}
	else
	{
		InsideResizeViewport = false;
	}

	return true;
}

int GetCurPrimAmmo( void )
{
	if( Ships[WhoIAm].Primary == PYROLITE_RIFLE ) return PyroliteAmmo;
	else if( Ships[WhoIAm].Primary == SUSS_GUN )  return SussGunAmmo;
	else                                          return GeneralAmmo;
}

void DrawSimplePanel()
{
	int current = 0;
	int left  = render_info.window_size.cx - FontWidth*14;
	int right = render_info.window_size.cx - FontWidth*5;
	int top   = render_info.window_size.cy - FontHeight*30;
	int energy;
	u_int8_t Count;
	u_int8_t r = Colourtrans[HUDColour][0];
	u_int8_t g = Colourtrans[HUDColour][1];
	u_int8_t b = Colourtrans[HUDColour][2];
	char MessageBuff[150];

	if( Panel )
	{
		if( !DrawPanel )
		{
			// HUD settings only shown in normal mode
			if(!SwitchedToWatchMode)
			{
				if(ShowWeaponsPossessedOnHUD)
				{
					// primaries using general ammo
					for( Count = 0; Count < MAXPRIMARYWEAPONS; Count++ )
					{
						if(Count == PYROLITE_RIFLE || Count == SUSS_GUN)
							continue;
						current = (Count == Ships[WhoIAm].Primary);
						if( current || PrimaryWeaponsGot[ Count ] > 0 )
						{
							Print4x5Text( PrimaryNames[ Count ], left, top, HUDColour );
							if( Count == PULSAR )
								Printu_int16_t( GeneralAmmo, right, top, HUDColour );
						}
						top += FontHeight;
					}

					// primaries using pyro ammo
					current = (PYROLITE_RIFLE == Ships[WhoIAm].Primary);
					if( current || PrimaryWeaponsGot[ PYROLITE_RIFLE ] > 0 )
					{
						Print4x5Text( PrimaryNames[ PYROLITE_RIFLE ], left, top, HUDColour );
						Printu_int16_t( PyroliteAmmo, right, top, HUDColour );
					}
					top += FontHeight;

					// primaries using suss ammo
					current = (SUSS_GUN == Ships[WhoIAm].Primary);
					if( current || PrimaryWeaponsGot[ SUSS_GUN ] > 0 )
					{
						Print4x5Text( PrimaryNames[ SUSS_GUN ], left, top, HUDColour );
						Printu_int16_t( SussGunAmmo, right, top, HUDColour );
					}
					top += FontHeight*2;

					// Secondaries
					for( Count = 0; Count < MAXSECONDARYWEAPONS; Count++ )
					{
						current = ( Count == Ships[WhoIAm].Secondary || GetBestMine() == Count );
						if(Count == 7) top += FontHeight; // add space for mines
						if( current || (u_int16_t) SecondaryAmmo[ Count ] > 0 )
						{
							Print4x5Text( SecondaryNames[Count], left, top, HUDColour );
							Printu_int16_t( (u_int16_t) SecondaryAmmo[ Count ] , right, top, HUDColour );
						}
						top += FontHeight;
					}
				} // end of show full inventory

				// top right
				top = FontHeight;

				// mine
				int mine = (int) GetBestMine();
				if( mine != 65535 )
				{
					int left = render_info.window_size.cx;
					Print4x5Text( SecondaryNames[mine], left-FontWidth*10, top, HUDColour );
					Printu_int16_t( (u_int16_t) SecondaryAmmo[mine], left-FontWidth*2, top, HUDColour );
				}

				// bottom right
				top = render_info.window_size.cy - FontHeight*5;

				// trojax level
				energy = (int) ( ( PowerLevel * 0.01F ) * 9.0F );

				// or lazer temperature
				if( !energy )
					energy = (int) ( ( LaserTemperature *0.01F ) * 9.0F );

				// render trojax/lazer bar
				if( energy )
				{
					// 72   (first frame) is power bar charging
					// 72-8 (last frame)  is power bar at full charge
					if( energy > 8 ) energy = 8;
					AddScreenPolyText(
						(72-energy),
						(float) (viewport.X + (viewport.Width>>1))-16 ,
						(float) (viewport.Y + (viewport.Height>>1))+4 ,
						r, g, b, 255 );
				}

				// nitro bar
				if ( ( control.turbo || Ships[WhoIAm].Object.CruiseControl == CRUISE_NITRO ) && NitroFuel )
				{
					float bar = ( Ships[WhoIAm].Object.Flags & SHIP_SuperNashram ) ?
						(float) Ships[WhoIAm].SuperNashramTimer*0.008f :
						(float) NitroFuel*0.04f;
					AddScreenPolyTextScale(
						72,
						(float) ( (render_info.window_size.cx>>1) - (NitroFuel - 8) ),
						(float) (viewport.Y + (viewport.Height>>1)-7 ) ,
						bar,
						1.0F,
						r, g, b, 255 );
				}

				// Average Kills Per Minute
				if(ShowKPMOnHUD)
				{
					sprintf( MessageBuff, "KPM: %.2f", (float)
							(GetTotalKills(WhoIAm) - GetFriendlyKills(WhoIAm))
							/ (LevelTimeTaken / 60.0F) );
					Print4x5Text( &MessageBuff[0], FontWidth, render_info.window_size.cy - FontWidth*6, RED);
				}
			}
			// HUD settings only shown in watch mode
			else
			{
				// show who i am watching
				Print4x5Text( (char *)GetName(WatchPlayerSelect.value), render_info.window_size.cx - FontWidth*9, FontHeight, WHITE );
			}

			// HUD settings common to both normal and watch mode

			// bottom left
			top   = render_info.window_size.cy - FontWidth*3;
			left  = FontWidth;
			right = FontWidth*8;

			// shield
			Print4x5Text( "Shield", left, top, HUDColour );
			if( WatchPlayerSelect.value == WhoIAm )
			{
				Printu_int16_t( (u_int16_t) Ships[WhoIAm].Object.Shield, right, top, HUDColour );
				if( ShieldHit ) ShieldHit -=1;
			}
			else
				Printu_int16_t( PlayerHealths[WatchPlayerSelect.value].Shield, right, top, HUDColour );
			top += FontHeight;

			// hull
			top++;
			Print4x5Text( "Hull", left, top, HUDColour );
			if( WatchPlayerSelect.value == WhoIAm )
			{
				Printu_int16_t( (u_int16_t) Ships[WhoIAm].Object.Hull, right, top, HUDColour );
				if( HullHit ) HullHit -=1;
			}
			else
				Printu_int16_t( PlayerHealths[WatchPlayerSelect.value].Hull, right, top, HUDColour );
			top += FontHeight;

			// Add Crosshair Polygon..
			if(!CurrentMenu)
			{
#ifdef __3DS__
				/* Crosshair/reticle is an in-world overlay tied to the 3D
				   camera viewport — it belongs on the stereo top screen, not
				   the mono bottom. Temporarily clear the bottom-HUD routing
				   so this AddScreenPolyText creates a top-screen poly. */


				bool _was_routing = ScrPolyGetRouteBottom();
				ScrPolySetRouteBottom(false);
#endif
				AddScreenPolyText(
					(u_int16_t) 63 ,
					(float) (viewport.X + (viewport.Width>>1)) ,
					(float) (viewport.Y + (viewport.Height>>1)) ,
					r, g, b, 255 );
#ifdef __3DS__
				ScrPolySetRouteBottom(_was_routing);
#endif
			}

			// Game Clock
			if(ShowClockOnHUD)
			{
				sprintf( MessageBuff, "%02d:%02d", (int)(LevelTimeTaken / 60), (int)((int)LevelTimeTaken % 60) );
				Print4x5Text( &MessageBuff[0], FontWidth, render_info.window_size.cy - FontWidth*5, WHITE);
			}

			// Current Speed
			if(ShowSpeedOnHUD)
			{
				if(SpeedInterval <= 0.0F)
				{
					SpeedInterval = 10.0F;
					BikeSpeed = DistanceVector2Vector(&Ships[WatchPlayerSelect.value].Object.Pos, &OldPos);
					OldPos.x = Ships[WatchPlayerSelect.value].Object.Pos.x;
					OldPos.y = Ships[WatchPlayerSelect.value].Object.Pos.y;
					OldPos.z = Ships[WatchPlayerSelect.value].Object.Pos.z;
				}
				sprintf( MessageBuff, "%.0f", BikeSpeed/3.0F); // attempt scale 0-100
				Print4x5Text( &MessageBuff[0], FontWidth, render_info.window_size.cy - FontWidth*8, YELLOW);
				SpeedInterval -= framelag;
			}

			// bottom right
			top = render_info.window_size.cy - FontHeight*4;
			right = render_info.window_size.cx - FontWidth*5;
			left  = render_info.window_size.cx - FontWidth*14;

			// power pods
			Print4x5Text( "Power", left+FontWidth*3, top, HUDColour );
			Printu_int16_t( (u_int16_t) Ships[WatchPlayerSelect.value].Object.PowerLevel+1, right, top, HUDColour );
			top+=FontHeight;

			// primary
			Print4x5Text( PrimaryNames[Ships[WatchPlayerSelect.value].Primary], left, top, HUDColour );
			if( WatchPlayerSelect.value == WhoIAm )
				Printu_int16_t( (u_int16_t) GetCurPrimAmmo() , right, top, HUDColour );
			top+=FontHeight;

			// secondary
			Print4x5Text( SecondaryNames[Ships[WatchPlayerSelect.value].Secondary], left, top, HUDColour );
			if( WatchPlayerSelect.value == WhoIAm )
				Printu_int16_t( (u_int16_t) GetCurSecAmmo(), right, top, HUDColour );

			top = render_info.window_size.cy - FontHeight*7;

			// chaos
			if( Ships[WatchPlayerSelect.value].Invul )
			{
				Print4x5Text( "Chaos", right-FontWidth*9, top, HUDColour );
				if( WatchPlayerSelect.value == WhoIAm )
					Printu_int16_t( (u_int16_t) (Ships[WhoIAm].InvulTimer / 60.0F), right, top, HUDColour );
			}
			top += FontHeight;

			// gulden power pod
			if( Ships[WatchPlayerSelect.value].Object.Flags & SHIP_SuperNashram )
			{
				Print4x5Text( "GPP", right-FontWidth*9, top, HUDColour );
				if( WatchPlayerSelect.value == WhoIAm )
					Printu_int16_t( (u_int16_t) (Ships[WhoIAm].SuperNashramTimer / 60.0F), right, top, HUDColour );
			}
			top += FontHeight;

			// stealth
			if( Ships[WatchPlayerSelect.value].Object.Flags & SHIP_Stealth )
			{
				Print4x5Text( "Stealth", right-FontWidth*9, top, HUDColour );
				if( WatchPlayerSelect.value == WhoIAm )
					Printu_int16_t( (u_int16_t) (Ships[WhoIAm].StealthTime / 60.0F), right, top, HUDColour );
			}

		} // end of !DrawPanel

		if( (NamesAreLegal != 0) || IsHost )
		{
			ScoreSort();
			PrintScoreSort();
			CheckMetKillLimit();
		}

		// show long message history
		if(ShowMessages)
			MessageQuePrintAll();

		// show last 3 recent messages
		else
		{
			MessageQuePrint();
			PlayerMessageQuePrint();
		}

		if( ! (CurrentMenu && CurrentMenuItem ) )
		{
			// if we are dead and waiting for a game
			// show statistics
			if( Ships[ WhoIAm ].Object.Mode == LIMBO_MODE && !SwitchedToWatchMode )
				ShowDeathModeStats();

			// if we have show stats activated (ex: you pressed the stats button)
			// show statistics
			else if ( ShowStatistics)
				ShowInGameStats();

			else if( ShowNetworkInfo )
				DisplayNetworkInfo();
		}
	} // end of ( Panel )

	if( Ships[WhoIAm].Object.Mode == GAMEOVER_MODE )
		CenterPrint4x5Text(
			"Game Over" ,
			(render_info.window_size.cy >> 1) - (FontHeight*2) ,
			2 );

}

void ReleaseLevel(void)
{
	switch( MyGameStatus )
	{
	case  STATUS_Title:
	case  STATUS_BetweenLevels:
	case  STATUS_StartingSinglePlayer:
	case  STATUS_TitleLoadGameStartingSinglePlayer:
		break;

	default:
		if( GodMode ) LoseAllWeapons();

		ReleaseComponentedShips();
		KillThermo();
		NodeRelease();
		CameraRelease();
		ReleaseExternalForces();
		ReleaseTeleports();
		ReleaseTriggers();
		ReleaseTriggerArea();
		ReleaseRTLights();
		ReleaseGoal();

		if( CountDownOn )
		{
			DeleteCountdownDigits();
		}

		KillAllBGObjects();
		ReleaseAllEnemies();
		DestroySound( DESTROYSOUND_All );
		break;
	}
}

void
ReleaseScene(void)
{
	int16_t Count;

	if( RegenPoints != nullptr )
	{
		free( RegenPoints );
		RegenPoints = nullptr;
		NumRegenPoints = 0;

		for( Count = 0; Count < MAX_PLAYERS; Count++ )
		{
			if( RegenSlotsCopy[ Count ] != nullptr )
			{
				free( RegenSlotsCopy[ Count ] );
				RegenSlotsCopy[ Count ] = nullptr;
			}
		}
	}

	switch( MyGameStatus )
	{
	case  STATUS_BetweenLevels:
	case  STATUS_Title:
	case  STATUS_StartingSinglePlayer:
	case  STATUS_TitleLoadGameStartingSinglePlayer:
		break;

	default:
		DestroyGame();
		break;
	}

}

void ReleaseView(void)
{
	switch( MyGameStatus )
	{
	case  STATUS_StartingMultiplayer:
	case  STATUS_GetPlayerNum:
	case  STATUS_BetweenLevels:
	case  STATUS_Title:
	case  STATUS_TitleLoadGameStartingSinglePlayer:
	case  STATUS_StartingSinglePlayer:
	case  STATUS_QuittingFromTitles:
	case  STATUS_WaitingToStartMultiPlayerClient:
		ReleaseFlyGirl();
		FreeAllLastAFrameScrPolys();
		ReleaseTitle();
		ReleaseTloadheader( &Tloadheader );
		ReleaseTitleModels();
		Free_All_Off_Files( &Title_OffsetFiles[ 0 ] );
		ReleaseRenderBufs();
		/* Hunk_FreeAll(TAG_LEVEL) bulk-releases all the per-mesh
		 * textureGroups[] arrays the title loaders set up. ModelHeaders
		 * and MxaModelHeaders are intentionally NOT freed here — the
		 * next PreInitModel re-frees-and-allocs them, and leaving them
		 * live across this point keeps ReleaseLevel teardown
		 * (KillUsedModel → KillAttachedSoundFX, which reads
		 * MxaModelHeaders[i].NumSpotFX) safe in QuitCurrentGame /
		 * level-end paths that call ReleaseView before ReleaseLevel.
		 * After ReleaseTitleModels above, NumSpotFX is 0 so the read
		 * is a no-op. */
		{

			Hunk_FreeAll(TAG_LEVEL);
		}
		break;

	case STATUS_ViewingScore:
	case STATUS_ViewingStats:
		break;

	default:
		Bspfree();
		FreeAllLastAFrameScrPolys();
		ReleaseMloadheader(&Mloadheader);
		ReleaseTloadheader( &Tloadheader );
		ReleaseModels();
		if ( MCloadheader.Buffer )
		{
			free( MCloadheader.Buffer );
			MCloadheader.Buffer = nullptr;
		}
		if ( MCloadheadert0.Buffer )
		{
			free( MCloadheadert0.Buffer );
			MCloadheadert0.Buffer = nullptr;
		}
		Free_All_Off_Files( &OffsetFiles[ 0 ] );
		ReleaseSkinExecs();
		ReleasePortalExecs();
		ReleaseRenderBufs();
		WaterRelease();
		FreeTxtFile();
		FreeMsgFile();
		/* Same per-level Hunk teardown as the title-cleanup branch above.
		 * ModelHeaders / MxaModelHeaders are intentionally kept live until
		 * the next PreInitModel — see the title-branch comment for why. */
		{

			Hunk_FreeAll(TAG_LEVEL);
		}
	}
}

bool
InitScene(void)
{
	int i;

	OnceOnlyInitModel();
	InitXLights();
	InitPrimBulls();
	InitSecBulls();
	InitFmPoly();


	InitPolys();
	InitScrPolys();
	InitLines();
	SetupSpotFX();
	InitPickups();
	InitAmmoUsed();
	InitAfterBurners();
	InitBGObjects();
	InitEnemies();

	for( i = 0 ; i < MAX_PLAYERS ; i++ )
	{
		Ships[i].ModelNum = INDEX_NONE;
	}

	switch( MyGameStatus )
	{
	case  STATUS_BetweenLevels:
	case  STATUS_Title:
	case  STATUS_StartingSinglePlayer:
	case  STATUS_TitleLoadGameStartingSinglePlayer:
	case  STATUS_StartingMultiplayer:
	case  STATUS_GetPlayerNum:
		if (!LoadTitleModels())
		{ Msg( "LoadTitleModels failed.\n" );
			return false;
		}
		break;
	}

	return true;

}

/*===================================================================
  Procedure :   Init the view...This might be title or score or main game....
  Output    :   bool true/FALSE
===================================================================*/

bool InitView( void )
{
	DebugPrintf("InitView Starting...\n");

	CheatsDisabled = false;

	input_buffer_reset();

	InitModeCase();

	switch( MyGameStatus )
	{
	case  STATUS_BetweenLevels:
	case  STATUS_Title:
	case  STATUS_StartingSinglePlayer:
	case  STATUS_TitleLoadGameStartingSinglePlayer:
	case  STATUS_StartingMultiplayer:
	case  STATUS_GetPlayerNum:
		if( InitTitle() != true )
		{
			SeriousError = true;
			return false;
		}

		if ( !bSoundEnabled )
		{
			if (! InitializeSound( DESTROYSOUND_All ))
			{
				DebugPrintf("unable to initialise sound in initview\n");
				Msg("unable to initialise sound in initview\n");
				exit(1);
			}
		}

		InitRenderBufs();
		if( !SetMatrixViewPort() )
		{
			SeriousError = true;
			Msg( "SetMatrixViewPort() Failed\n" );
			return false;
		}
		// Init the Texture Handler
		InitTload( &Tloadheader );

		if( !Load_All_Off_Files( &Title_OffsetFiles[ 0 ] ) )
		{
			SeriousError = true;
			return false;
		}

		if( !PreLoadFlyGirl() )
		{
			SeriousError = true;
			return false;
		}

		if( !PreInitModel( TitleModelSet ) )
		{
			SeriousError = true;
			return false;
		}

		//  Load in And if nescessary ReScale Textures...
		if( !Tload( &Tloadheader ) )
		{
			SeriousError = true;
			return false;
		}
		if( !InitModel( TitleModelSet ) )
		{
			SeriousError = true;
			return false;
		}
		if ( !AllocateCompFlyGirl() )
		{
			SeriousError = true;
			return false;
		}
		else
		{
			UpdateFlyGirl( &BikePos, &MATRIX_Identity, false );
		}

		DummyTextureIndex = FindTexture( &Tloadheader, "data\\textures\\dummy.bmp" );
		if ( DummyTextureIndex != -1 )
		{
			if(!Tloadheader.PlaceHolderFile[ DummyTextureIndex ])
				Tloadheader.PlaceHolderFile[ DummyTextureIndex ] = ( char * )malloc( sizeof( char ) * 256 );
			Tloadheader.PlaceHolder[ DummyTextureIndex ] = true;
		}

		if ( !CurrentMenu )
		{
			MenuRestart( &MENU_Start );
		}
		break;

	case STATUS_ViewingScore:
		InitScoreDisplay();

		// just the basics to get text to render

		InitRenderBufs();

		set_normal_states();

		if( !Load_All_Off_Files( &Title_OffsetFiles[ 0 ] ) )
		{
			SeriousError = true;
			return false;
		}

		if( !Tload( &Tloadheader ) )
		{
			SeriousError = true;
			return false;
		}

		MakeViewMatrix(&View, &Look, &Up, &CurrentCamera.Mat);
		MatrixTranspose( &CurrentCamera.Mat, &CurrentCamera.InvMat );
		CurrentCamera.Pos = View;
		CurrentCamera.GroupImIn = -1;
		CurrentCamera.Viewport = viewport;
		CurrentCamera.Viewport.X = 0;
		CurrentCamera.Viewport.Y = 0;
		CurrentCamera.Viewport.Width = render_info.window_size.cx;
		CurrentCamera.Viewport.Height = render_info.window_size.cy;
		CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / (float)2.0;
		CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / (float)2.0;

		Build_View();
		CurrentCamera.View = view;

		break;

	case STATUS_ViewingStats:
		InitStatsDisplay();
		break;

	default:

		// this will cause a lovely game loop and crash the game
		// so don't remove this !!!!!!!!!
		if( MyGameStatus != STATUS_InitView_0 )
			InitView_MyGameStatus = MyGameStatus;

		MyGameStatus = STATUS_InitView_0;
		clear_black();
		CameraStatus = CAMERA_AtStart;  //prevents on screen menus from being suppressed
		break;
	}


	return true;
}

/*===================================================================
  Procedure :   Set the View Matrix
  Input   :   nothing...  Current_Camera_View must be set
				  to a legal ship number...
  Output    :   nothing
===================================================================*/

void Build_View()
{
	VECTOR  Trans;
	VECTOR  Trans2;

	Trans.x = -CurrentCamera.Pos.x;
	Trans.y = -CurrentCamera.Pos.y;
	Trans.z = -CurrentCamera.Pos.z;

	ApplyMatrix( &CurrentCamera.InvMat, &Trans, &Trans2 );      /* Set Translation */

	view._11 = CurrentCamera.Mat._11;
	view._12 = CurrentCamera.Mat._12;
	view._13 = CurrentCamera.Mat._13;
	view._14 = CurrentCamera.Mat._14;

	view._21 = CurrentCamera.Mat._21;
	view._22 = CurrentCamera.Mat._22;
	view._23 = CurrentCamera.Mat._23;
	view._24 = CurrentCamera.Mat._24;

	view._31 = CurrentCamera.Mat._31;
	view._32 = CurrentCamera.Mat._32;
	view._33 = CurrentCamera.Mat._33;
	view._34 = CurrentCamera.Mat._34;

	view._41 = Trans2.x;
	view._42 = Trans2.y;
	view._43 = Trans2.z;
	view._44 = CurrentCamera.Mat._44;
}


char TriggerName[256];

/*===================================================================
  Procedure :   Inits And Loads New Level..
  Input   :   nothing...
  Output    :   nothing
===================================================================*/
bool ChangeLevel( void )
{

	LevelNum = NewLevelNum;

#ifdef __3DS__
	music_play_for_level(LevelNum);
#endif

	ReleaseLevel();
	KillNodeCubeLines();
	KillBoxLines();

	// $$$
	NextworkOldBikeNum = -1;

	ActiveRemoteCamera = nullptr;
	MissileCameraActive = 0;
	CameraMissile = INDEX_NONE;

	InitScene();
	InitTextMessages();
	InitMultiSfxHandle();

	OnceOnlyChangeLevel = true;
	ChangeLevel_MyGameStatus = MyGameStatus;
	Change_Ext( &LevelNames[ LevelNum ][ 0 ], &TriggerName[ 0 ], ".TRG" );
	if( !Triggerload( TriggerName ) )
	{
		SeriousError = true;
		return( false );
	}

	MyGameStatus = STATUS_ChangeLevelPostInitView;
	InitView();

	return( true );
}

float Browl = 0.0F;
float HostMultiPlayerTimeout;

int colourflash = 0;
char NodeName[256];

/*===================================================================
  Procedure :   Game Status Control...
  Input   :   nothing...
  Output    :   nothing
===================================================================*/
/*===================================================================
  Procedure :   Game Status Control...
  Input   :   nothing...
  Output    :   nothing
===================================================================*/

bool RenderScene( void )
{
	u_int16_t  i,e;
	char  buf[256];
	static int WaitFrames = 2;
	bool done;

	if ( SeriousError )
		return false;


	// This is where in game we are getting input data read
	ReadInput();

	CalculateFramelag();
	AnimOncePerFrame++;

	if ( bSoundEnabled )
	{
		CheckSBufferList();
	}

	switch( MyGameStatus )
	{

	case STATUS_QuittingFromTitles:
		DebugState("STATUS_QuittingFromTitles\n");
		quitting = true;
		break;

	case STATUS_WaitingToStartSinglePlayer:
		DebugState("STATUS_WaitingToStartSinglePlayer\n");

		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}

		done = VduFinished( &MENU_NEW_BetweenLevels );

		if ( done )
			WaitFrames--;
		if ( !WaitFrames )
		{
			WaitFrames = 2;
			StartASinglePlayerGame( nullptr );
		}

		break;


	case STATUS_WaitingToStartEndGameSequence:
		DebugState("STATUS_WaitingToStartEndGameSequence\n");
		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}
		done = VduFinished( &MENU_NEW_GeneralLoading );

		if ( done )
			WaitFrames--;
		if ( !WaitFrames )
		{
			WaitFrames = 2;
			MyGameStatus = STATUS_Title;
			ReleaseView();
		}
		break;


	case STATUS_WaitingToStartMultiPlayerHost:
		DebugState("STATUS_WaitingToStartMultiPlayerHost\n");
		Browl -= framelag;

		if( Browl <= 0.0F )
		{
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}
		ReceiveGameMessages();

		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}
		if ( VduFinished( nullptr ) )
			WaitFrames--;
		if ( !WaitFrames )
		{
			WaitFrames = 2;
			MyGameStatus = STATUS_StartingMultiplayer;
			GoToSynchup( nullptr );
		}
		break;


	case STATUS_WaitingToStartMultiPlayerClient:
		DebugState("STATUS_WaitingToStartMultiPlayerClient\n");
		Browl -= framelag;

		if( Browl <= 0.0F )
		{
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}
		ReceiveGameMessages();

		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}
		if ( VduFinished( nullptr ) )
			WaitFrames--;
		if ( !WaitFrames )
		{
			WaitFrames = 2;
			LastMenu = CurrentMenu;
			VduClear();
			MenuAbort();
			ReleaseView();
			LevelNum = -1;

			MyGameStatus = STATUS_PreStartingMultiplayerSynch;
			if( !ChangeLevel() )
				return( false );
		}
		break;


	case STATUS_WaitingToStartTeamGame:
		DebugState("STATUS_WaitingToStartTeamGame\n");
		Browl -= framelag;

		if( Browl <= 0.0F )
		{
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}
		ReceiveGameMessages();

		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}

		if ( VduFinished( nullptr ) )
			WaitFrames--;
		if ( !WaitFrames )
		{
			WaitFrames = 2;
			LastMenu = CurrentMenu;
			VduClear();
			MenuAbort();
			MyGameStatus = PreSynchupStatus;
		}

		if( IMustQuit )
			SpecialDestroyGame();

		break;

		// the main menu screen
	case STATUS_Title:
		DebugState("STATUS_Title\n");
		[[fallthrough]];

		// the title screen runs the between-levels body too
	case STATUS_BetweenLevels:
		if(MyGameStatus != STATUS_Title)
			DebugState("STATUS_BetweenLevels\n");

		if( DisplayTitle() != true )
		{
			SeriousError = true;
			return false;
		}
		break;


	case STATUS_Normal:
		DebugState("STATUS_Normal\n");

		if ( !CheatsDisabled )
			DisableCheats();

		if( IMustQuit )
			SpecialDestroyGame();

		if( MainGame() != true )
			return false;

		if ( bSoundEnabled )
		{
			ProcessLoopingSfx();
		}

		LevelTimeTaken += timer_run( &level_timer );

		// if player is quiting nothing should stop him.....
		if( MyGameStatus != STATUS_QuitCurrentGame )
		{
			if( IsHost )
			{
				if( NewLevelNum != LevelNum )
				{
					for( i = 0 ; i < MAX_PLAYERS ; i++ )
						if( (GameStatus[i] != STATUS_LeftCrashed ) &&
							(GameStatus[i] != STATUS_Left        ) &&
							(GameStatus[i] != STATUS_Null        ) &&
							(GameStatus[i] != STATUS_Normal      ) &&
							i != WhoIAm )
							SendGameMessage(MSG_YOUQUIT, 0, (BYTE) i , 0, 0);

					// the level has ended or changed ..tell everyone to go onto new level
					MyGameStatus = STATUS_LevelEnd;
					GameStatus[WhoIAm] = MyGameStatus;
					RandomStartPosModify += 1;
					SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
				}
			}
			else
			{
				if( OverallGameStatus == STATUS_LevelEnd )
				{
					// tell the host that I am now in the same state as him....
					MyGameStatus = OverallGameStatus;
					GameStatus[WhoIAm] = MyGameStatus;
					SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
				}
			}
		}

		if ( WaitingToQuit )
		{
			clear_black();
			CenterPrint4x5Text( PLEASE_WAIT, (render_info.window_size.cy>>1)-(FontHeight>>1) , 2 );
			SelectQuitCurrentGame( nullptr );
		}
		break;


	case STATUS_LevelEnd:
		DebugState("STATUS_LevelEnd\n");

		clear_black();
		ReceiveGameMessages();
		Browl -= framelag;

		if( Browl <= 0.0F )
		{
			if( IsHost )
				SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
			else
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
			Browl = 30.0F;
		}
		if( IsHost )
		{
			// have all the enabled players switched over to STATUS_LevelEnd??
			for( i = 0 ; i < MAX_PLAYERS ; i++ )
			{
				if( ( Ships[i].enable != 0 ) && ( GameStatus[i] != STATUS_LevelEnd ) && ( i != WhoIAm ) )
					break;
			}
			if( i != MAX_PLAYERS )
				break;  // not yet..

			// un load all graphics
			ReleaseView();
			ReleaseLevel();

			// tell them all they can now look at the scores...
			MyGameStatus = STATUS_ViewingScore;
			GameStatus[WhoIAm] = MyGameStatus;
			SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);

			InitScene();  // STATSTEST
			InitView();

			NextworkOldBikeNum = -1;
			HostMultiPlayerTimeout = 60.0F * 60.0F * 2.0F * 2.0F;

		}
		else
		{
			if( MyGameStatus != OverallGameStatus )
			{

				OutputVisiStats( &Mloadheader, LevelNames[ LevelNum ] );
				// un load all graphics
				ReleaseView();
				ReleaseLevel();
				// tell the host that I am now in the same state as him....
				MyGameStatus = STATUS_ViewingScore;

				GameStatus[WhoIAm] = MyGameStatus;
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);

				InitScene();  // STATSTEST
				InitView();

				NextworkOldBikeNum = -1;
				HostMultiPlayerTimeout = 60.0F * 60.0F * 2.0F * 2.0F;
			}
		}
		break;


	case STATUS_ViewingScore:
		DebugState("STATUS_ViewingScore\n");

		ReceiveGameMessages();

		Browl -= framelag;
		if( Browl <= 0.0F )
		{
			if( IsHost )
				SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
			else
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
			Browl = 30.0F;
		}

		if (!FSBeginScene())
		{
			Msg( "DisplayTitle() : BeginScene failed\n" );
			return false;
		}

		if (ClearBuffers() != true )
		{
			Msg( "DisplayTitle() : ClearBuffers failed\n" );
			return false;
		}

		ScoreDisplay();

		if( !DisplaySolidScrPolys( &RenderBufs[ 3 ] ) )
			return false;

		if (!FSEndScene())
		{
			Msg( "DisplayTitle() : EndScene failed\n" );
			return false;
		}

		ScreenPolyProcess(); // only needed for flashing text

		HostMultiPlayerTimeout -= framelag;

		if( input_buffer_find( SDLK_SPACE ) || ( OverallGameStatus == STATUS_WaitingAfterScore ) )
		{
			clear_black();
			HostMultiPlayerTimeout = 60.0F * 60.0F * 2.0F;

			if( IsHost )
			{
				// tell them all I Am waiting
				ReleaseView();
				MyGameStatus = STATUS_WaitingAfterScore;
				GameStatus[WhoIAm] = MyGameStatus;
				SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
				if( !ChangeLevel() )
					return( false );
			}
			else
			{
				// tell the host that I am now Waiting for him to finish viewing the score....
				ReleaseView();
				MyGameStatus = STATUS_WaitingAfterScore;
				GameStatus[WhoIAm] = MyGameStatus;
				if( !ChangeLevel() )
					return( false );
			}
		}
		break;


	case STATUS_WaitingAfterScore:
		DebugState("STATUS_WaitingAfterScore\n");

		clear_black();
		e = 0;

		if( IsHost )
		{
			Printu_int16_t( (u_int16_t) (HostMultiPlayerTimeout / 60.0F) , 0 , 0 , 2 );
			HostMultiPlayerTimeout -= framelag;
		}

		// you must clear player scores if MaxKills limit is set...
		// other wise the game just keeps on changing levels...
		// cause at the start of each level they already have max kills...

		// reset all player's stats
		if( MaxKills || ResetKillsPerLevel )
			ResetAllStats();

		for( i = 0 ; i < MAX_PLAYERS ; i++ )
		{
			if( (GameStatus[i] != STATUS_LeftCrashed ) && (GameStatus[i] != STATUS_Left ) && (GameStatus[i] != STATUS_Null ) )
			{

				{
					sprintf( buf, "%-8s status %14s\n", &Names[i][0] , &StatusTab[ GameStatus[i] ][0] );
					CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy>>1)-( ( (FontHeight+2) * MAX_PLAYERS ) >> 1 )+ (e * (FontHeight+(FontHeight>>1)) ) , GameStatus[i] == STATUS_WaitingAfterScore ? 2 : 1 );
				}

				e++;
			}
		}

		CenterPrint4x5Text( "waiting for all other players" , (render_info.window_size.cy>>1)-(FontHeight>>1) + ( ( FontHeight+2) * (MAX_PLAYERS+1)) , (colourflash>>3) &3);

		Browl -= framelag;
		if( Browl < 0.0F )
		{
			colourflash++;
			if( IsHost )
				SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
			else
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
			Browl = 30.0F;
		}
		ReceiveGameMessages();

		if( IsHost )
		{

			// have all the enabled players switched over to STATUS_LevelEnd??
			for( i = 0 ; i < MAX_PLAYERS ; i++ )
			{
				if( ( ( ( GameStatus[ i ] >= STATUS_InitView_0 ) && ( GameStatus[ i ] <= STATUS_InitView_9 ) ) || ( GameStatus[i] == STATUS_ViewingScore ) ) && ( i != WhoIAm ) ) break;
			}
			if( HostMultiPlayerTimeout >= 0.0F )
			{
				if( i != MAX_PLAYERS )
					break;  // not yet..
			}
			// tell them all they can now restart a new level...
			if( CountDownOn )
			{
				CreateCountdownDigits();
				StartCountDown( (int16_t) TimeLimit.value, 0 );
			}

			MyGameStatus = STATUS_Normal;
			GameStatus[WhoIAm] = MyGameStatus;
			SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);

		}else{
			if( OverallGameStatus == STATUS_Normal )
			{
				if( CountDownOn )
				{
					CreateCountdownDigits();
					StartCountDown( (int16_t) TimeLimit.value, 0 );
				}
				// tell the host that I am now in the same state as him....
				MyGameStatus = OverallGameStatus;
				GameStatus[WhoIAm] = MyGameStatus;
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
			}
		}
		break;


	case STATUS_Joining:
		DebugState("STATUS_Joining\n");
		clear_black();

		ReceiveGameMessages();
		if( IMustQuit || IsHost || ( OverallGameStatus != STATUS_Normal ) )
		{
			SpecialDestroyGame();
			break;
		}
		Browl -= framelag;
		if( Browl <= 0.0F )
		{
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}

		{
			RENDEROBJECT ro;
			TEXTUREGROUP ro_groups[MAX_TEXTURE_GROUPS];
			ZERO_STACK_MEM(ro);
			ro.textureGroups = ro_groups;
			FSCreateDynamic2dVertexBuffer(&ro, BATCH_BUFFER_VERTS);
			FSCreateIndexBuffer(&ro, BATCH_BUFFER_VERTS * 3);


			CenterPrint4x5Text( "Pickups Left   " , (render_info.window_size.cy>>1)-(FontHeight<<2), GRAY );
			Printu_int16_t( (u_int16_t) Ships[WhoIAm].Pickups , (render_info.window_size.cx>>1)+((17*FontWidth>>1)), (render_info.window_size.cy>>1)-(FontHeight<<2), 2 );

			CenterPrint4x5Text( "RegenSlots Left" , (render_info.window_size.cy>>1)-(FontHeight<<1), GRAY );
			Printu_int16_t( (u_int16_t) Ships[WhoIAm].RegenSlots , (render_info.window_size.cx>>1)+((17*FontWidth>>1)), (render_info.window_size.cy>>1)-(FontHeight<<1), 2 );

			CenterPrint4x5Text( "Mines Left     " , (render_info.window_size.cy>>1), GRAY );
			Printu_int16_t( (u_int16_t) Ships[WhoIAm].Mines , (render_info.window_size.cx>>1)+((17*FontWidth>>1)), (render_info.window_size.cy>>1), 2 );

			CenterPrint4x5Text( "Triggers Left  " , (render_info.window_size.cy>>1)+(FontHeight<<1), GRAY );
			Printu_int16_t( (u_int16_t) Ships[WhoIAm].Triggers , (render_info.window_size.cx>>1)+((17*FontWidth>>1)), (render_info.window_size.cy>>1)+(FontHeight<<1), 2 );

			CenterPrint4x5Text( "TrigVars Left  " , (render_info.window_size.cy>>1)+(FontHeight<<2), GRAY );
			Printu_int16_t( (u_int16_t) Ships[WhoIAm].TrigVars , (render_info.window_size.cx>>1)+((17*FontWidth>>1)), (render_info.window_size.cy>>1)+(FontHeight<<2), 2 );


			DisplayNonSolidScrPolys(&ro);
			DisplaySolidScrPolys(&ro);
			FSReleaseRenderObject(&ro);
			// lock rendering to lower fps to stop tearing on loading screen
			{
				constexpr auto LOADING_SCREEN_FPS = 1.0f / 10.0f;
				static float counter = LOADING_SCREEN_FPS;
				counter += real_framelag;
				if(counter > LOADING_SCREEN_FPS)
				{
					render_flip(&render_info);
					counter = 0;
				}
			}
			ScreenPolyProcess();
		}

		// wait for all the pickup and mine stuff to be sent to me.....
		if( ( Ships[WhoIAm].Pickups == 0 ) && ( Ships[WhoIAm].Mines == 0 ) && ( Ships[WhoIAm].RegenSlots == 0 ) &&
			( Ships[WhoIAm].Triggers == 0 ) && ( Ships[WhoIAm].TrigVars == 0 ) && OverallGameStatus == STATUS_Normal)
		{
			// broadcast "x is joining the game" to all players...
			if (TeamGame)
				SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_JoiningTeamGame, 0);

			MyGameStatus = OverallGameStatus;
			input_grab( true );


			GameStatus[WhoIAm] = OverallGameStatus;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
			SyncMines();
			InitShipRandomPos( WhoIAm );
			NextworkOldBikeNum = -1;
			if( CountDownOn )
			{
				CreateCountdownDigits();
				StartCountDown( (int16_t) TimeLimit.value, 0 );
				IllegalTime = true;
			}
		}
		break;

		// in the multiplayer screen waiting for the game to start
	case STATUS_StartingMultiplayer:
		DebugState("STATUS_StartingMultiplayer\n");

		Browl -= framelag;

		if( Browl <= 0.0F )
		{
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}

		ReceiveGameMessages();

		if( ( MyGameStatus == STATUS_QuitCurrentGame ) || ( MyGameStatus == STATUS_Title ) )
		{
			break;
		}

		if( ( CurrentMenu ) && ( CurrentMenuItem ) )
		{
			if( DisplayTitle() != true )
				return false;
		}

		// if status has been changed from titles, should exit this state straight away
		if ( MyGameStatus != STATUS_StartingMultiplayer )
			break;

		if( ( IsHost ) && ( !CurrentMenu ) && ( !CurrentMenuItem ) )
		{
			clear_black();
			ReleaseView();
			// tell them all to load up a level
			MyGameStatus = STATUS_StartingMultiplayerSynch;
			GameStatus[WhoIAm] = MyGameStatus;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);

			LevelNum = -1;
			if( !ChangeLevel() ) return( false );
			HostMultiPlayerTimeout = 60.0F * 15.0F;
		}
		else
		{
			if( !IsHost )
			{
				if( ( OverallGameStatus == STATUS_StartingMultiplayerSynch ) ||
					( (OverallGameStatus == STATUS_Normal) && (!TeamGame || PlayerReady[WhoIAm]) ) )
				{

					if ((CameraStatus == CAMERA_AtLeftVDU) || (CameraStatus == CAMERA_AtRightVDU))
					{
						MenuChangeEx( &MENU_NEW_GeneralLoading );
						MenuDraw( CurrentMenu );  // because menu processing will not be done when MyGameStatus changes
					}
					else
					{
						clear_black();
					}


					PreSynchupStatus = MyGameStatus;
					MyGameStatus = STATUS_WaitingToStartMultiPlayerClient;
				}
			}
		}
		break;


	case  STATUS_PreStartingMultiplayerSynch:
		DebugState("STATUS_PreStartingMultiplayerSynch\n");

		ReceiveGameMessages();
		if( IMustQuit )
		{
			SpecialDestroyGame();
			break;
		}
		MyGameStatus = STATUS_StartingMultiplayerSynch;
		if( OverallGameStatus == STATUS_Normal )
		{
			initShip( WhoIAm );
			MyGameStatus = STATUS_Joining;
			Browl = 0.0F;
		}else{
			smallinitShip( WhoIAm );
			MyGameStatus = STATUS_StartingMultiplayerSynch;
		}
		SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		GameStatus[WhoIAm] = MyGameStatus;
		PrintInitViewStatus( MyGameStatus );
		break;


	case  STATUS_StartingMultiplayerSynch:
		DebugState("STATUS_StartingMultiplayerSynch\n");

		ReceiveGameMessages();

		if( IMustQuit )
		{
			SpecialDestroyGame();
			break;
		}

		clear_black();
		e = 0;

		for( i = 0 ; i < MAX_PLAYERS ; i++ )
		{
			if( ( GameStatus[i] != STATUS_GetPlayerNum )&& (GameStatus[i] != STATUS_LeftCrashed ) && (GameStatus[i] != STATUS_Left ) && (GameStatus[i] != STATUS_Null ) )
			{
				sprintf( buf, "%-8s status %14s\n", &Names[i][0] , &StatusTab[ GameStatus[i] ][0] );
				CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy>>1)-( ( (FontHeight+2) * MAX_PLAYERS ) >> 1 )+ (e * (FontHeight+(FontHeight>>1)) ) , GameStatus[i] == STATUS_StartingMultiplayerSynch ? 2 : 1 );
				e++;
			}
		}

		CenterPrint4x5Text( "all players synching" , (render_info.window_size.cy>>1)-(FontHeight>>1) + ( ( FontHeight+2) * (MAX_PLAYERS+1)) , (colourflash>>3) &3);
		Browl -= framelag;
		if( Browl <= 0.0F )
		{
			colourflash++;
			Browl = 30.0F;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}

		if( CountDownOn )
		{
			CreateCountdownDigits();
			StartCountDown( (int16_t) TimeLimit.value, 0 );
			IllegalTime = false;
		}

		if( IsHost )
		{
			Printu_int16_t( (u_int16_t) (HostMultiPlayerTimeout / 60.0F) , 0 , 0 , 2 );
			HostMultiPlayerTimeout -= framelag;

			// have all the enabled players switched over to STATUS_StartingMultiplayer??
			for( i = 0 ; i < MAX_PLAYERS ; i++ )
			{
				if( (GameStatus[i] != STATUS_GetPlayerNum ) &&(GameStatus[i] != STATUS_LeftCrashed ) && (GameStatus[i] != STATUS_Left ) && ( (GameStatus[i] != STATUS_Null ) && ( GameStatus[i] != STATUS_StartingMultiplayerSynch ) ) && ( i != WhoIAm ) )
					break;
			}
			if( HostMultiPlayerTimeout >= 0.0F )
			{
				if( i != MAX_PLAYERS )
					break;  // not yet..
			}
			// tell them all they can now restart a new level...
			DebugPrintf("STATUS_StartingMultiplayerSynch setting MyGameStatus to STATUS_Normal\n");
			MyGameStatus = STATUS_Normal;
			GameStatus[WhoIAm] = STATUS_Normal;
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);

			// $$$
			NextworkOldBikeNum = -1;

			input_grab( true );

		}
		else
		{
			if( OverallGameStatus == STATUS_Normal )
			{
				MyGameStatus = OverallGameStatus;
				GameStatus[WhoIAm] = MyGameStatus;
				SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
				// $$$
				NextworkOldBikeNum = -1;
			}
		}
		break;


	case STATUS_QuitCurrentGame:
		DebugState("STATUS_QuitCurrentGame\n");

		OutputVisiStats( &Mloadheader, LevelNames[ LevelNum ] );
		MenuAbort();
		MenuRestart( &MENU_Start );
		DestroyGame();  // DestroyGame changes MyGameStatus..So Set it back
		MyGameStatus = STATUS_QuitCurrentGame;
		ReleaseView();
		ReleaseLevel();
		MyGameStatus = STATUS_Title;
#ifdef __3DS__
		/* Wipe the gameplay HUD off the bottom screen — the title screen
		 * never draws there, so without this the last HUD frame stays
		 * frozen until the next gameplay session takes over. */
		c3d_clear_bottom_screen();
#endif
		InitScene();
		InitView();

		break;

	case STATUS_GetPlayerNum:
		DebugState("STATUS_GetPlayerNum\n");

		clear_black();
		ReceiveGameMessages();

		CenterPrint4x5Text( "Requesting Player Number" , (render_info.window_size.cy>>1)-(FontHeight>>1) + ( ( FontHeight+2) * (MAX_PLAYERS+1)) , 2 );

		sprintf( &buf[0] , "Attempt %d\n" , GetPlayerNumCount );
		CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy>>1)-(FontHeight>>1) , 2 );

		GetPlayerNumCount1 -= framelag;
		GetPlayerNumCount2 -= framelag;

		if( WhoIAm != UNASSIGNED_SHIP )
		{
			if(WhoIAm >= MAX_PLAYERS)
			{
				switch(WhoIAm)
				{
				case MAX_PLAYERS:
					PrintErrorMessage ( "The Game is Currently Un-Joinable", 3, nullptr, ERROR_USE_MENUFUNCS );
					MyGameStatus = STATUS_Title;
					break;
				case MAX_PLAYERS+1:
					PrintErrorMessage ( "You Dont have the level they are playing", 3, nullptr, ERROR_USE_MENUFUNCS );
					MyGameStatus = STATUS_Title;
					break;
				case MAX_PLAYERS+2:
					PrintErrorMessage ( "The Game is Full", 3, nullptr, ERROR_USE_MENUFUNCS );
					MyGameStatus = STATUS_Title;
					break;
				case MAX_PLAYERS+3:
					PrintErrorMessage ( "You do have the right version", 3, nullptr, ERROR_USE_MENUFUNCS );
					MyGameStatus = STATUS_Title;
					break;
				default:
					PrintErrorMessage ( "Could not join game host gave no reason why", 3, nullptr, ERROR_USE_MENUFUNCS );
					MyGameStatus = STATUS_Title;
				}
				break; // we are done
			}

			// go to next menu
			MenuState = MENUSTATE_Select;
			MenuChangeEx( GetPlayerNumMenu );

			break;
		}

		if( GetPlayerNumCount2 <= 0.0F )
		{
			// Time is Up No Player Number was got so quit back to the title screen...
			PrintErrorMessage ( "Couldnt Get a Player Number", 3, nullptr, ERROR_USE_MENUFUNCS );
			MyGameStatus = STATUS_Title;
			break;
		}

		if( GetPlayerNumCount1 <= 0.0F )
		{
			GetPlayerNumCount++;

			// Time to send another request for a Player Number...
			GetPlayerNumCount1 = 60.0F;   // approx 1 Second...
			SendGameMessage(MSG_HEREIAM, 0, 0, 0, 0); // can I join your game...
			DebugPrintf("sending MSG_HEREIAM\n");
		}

		break;


	case STATUS_InitView_0:
		DebugState("STATUS_InitView_0\n");

		if( IsHost )
		{
			SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
		}
		else
		{
			SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		}

		clear_black();

		JustExitedMenu = false;

		QuickStart = QuickStartMode::None;
		WaitingToQuit = false;

		ReceiveGameMessages();
		if( !SetMatrixViewPort() )
		{
			SeriousError = true;
			Msg( "SetMatrixViewPort() failed\n" );
			return false;
		}
		InitFont();


		InitSkinExecs();
		InitPortalExecs();
		InitRenderBufs();

		// Init the Texture Handler
		if( !InitTload( &Tloadheader ) )
		{
			SeriousError = true;
			Msg( "InitTLoad failed\n" );
			return false;
		}

		//  Prep the Texture Handler.....
		if( !PreMload( (char*) &LevelNames[LevelNum][0] , &Mloadheader ) )
		{
			SeriousError = true;
			return false; // the model and visipoly data
		}

		// Can Cope with no .Wat file!!!
		PreWaterLoad( (char*) &WaterNames[LevelNum][0] );

		if( OnceOnlyChangeLevel )
		{
			OnceOnlyChangeLevel = false;
			if( !PreLoadShips() )
			{
				SeriousError = true;
				return false;
			}
			if( !PreLoadBGOFiles() )
			{
				SeriousError = true;
				return false;
			}
			if( !PreLoadEnemies() )
			{
				SeriousError = true;
				return false;
			}
#ifdef EDITION_REMASTER
			/* Reserve ModelNames[] slots for the N64 single-mesh pickups
			 * (extracted from ForsakenEX.kpf). Must run after PreLoadEnemies
			 * (which writes back to NextNewModel) so the pickup slots end
			 * up past the enemy slots. PreInitModel below loads everything
			 * registered up to NextNewModel. */
			{
				if( !PreLoadPickups() )
				{
					SeriousError = true;
					return false;
				}
			}
#endif /* EDITION_REMASTER */
		}

		EnableRelavantModels( &ModelNames[0] );
		if( !PreInitModel( &ModelNames[0] ) )
		{
			SeriousError = true;
			return false;
		}
		if( !Load_All_Off_Files( &OffsetFiles[ 0 ] ) )
		{
			SeriousError = true;
			return false;
		}
		//  Load in And if nescessary ReScale Textures...
		if( !Tload( &Tloadheader ) )
		{
			SeriousError = true;
			return false;
		}

		if( !InitModel( &ModelNames[0] ) )
		{
			SeriousError = true;
			return false;               // all 3d models....
		}

		if( !Mload( (char*) &LevelNames[LevelNum][0] , &Mloadheader ) )
		{
			SeriousError = true;
			return false; // the model and visipoly data
		}

		InitVisiStats( &Mloadheader );

		if ( !Bspload( (char*) &BspNames[LevelNum][0], &Bsp_Header[ 0 ] ) )
		{
			SeriousError = true;
			Msg( "Bspload failed\n" );
			return false;   // the collision data
		}

		// might not be any water...
		WaterLoad();

		ReadTxtFile( (char*) &TextNames[LevelNum][0] );

		ReadMsgFile( (char*) &MsgNames[LevelNum][0] );

		if( !MCload( (char*) &CollisionNames[LevelNum][0] , &MCloadheader ) )
		{
			SeriousError = true;
			Msg( "MCload non zero failed\n" );
			return false;   // the collision data
		}
		if( !MCload( (char*) &CollisionZNames[LevelNum][0] , &MCloadheadert0 ) )
		{
			SeriousError = true;
			Msg( "MCload zero failed\n" );
			return false; // the collision data skin thickness 0
		}

		InitSoundInfo( &Mloadheader );

		InitShipSpeeds();

		// this will cause a lovely game loop and crash the game
		// so don't remove this !!!!!!!!!
		if( InitView_MyGameStatus != STATUS_InitView_0 )
			MyGameStatus = InitView_MyGameStatus;

		break;


	case STATUS_ChangeLevelPostInitView:
		DebugState("STATUS_ChangeLevelPostInitView\n");
		Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NodeName[ 0 ], ".NOD" );
		if( !Nodeload( NodeName ) )
		{
			SeriousError = true;
			return( false );
		}

		if( !LoadBGOFiles() )
		{
			SeriousError = true;
			return( false );
		}

		if( !LoadEnemies() )
		{
			SeriousError = true;
			Msg( "LoadEnemies() failed\n" );
			return( false );
		}

		if( !LoadSpotFX() )
		{
			SeriousError = true;
			Msg( "LoadSpotFX() failed\n" );
			return( false );
		}

		if( !LoadStartPoints() )
		{
			SeriousError = true;
			Msg( "LoadStartPoints() failed\n" );
			return( false );
		}

		if ( !InitializeSound( DESTROYSOUND_All ))
		{
			Msg("InitializeSound() failed\n");
			return false;
		}

		Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NodeName[ 0 ], ".CAM" );

		if( !Cameraload( NodeName ) )
		{
			SeriousError = true;
			return( false );
		}

		// might not be any real-time lights
		LoadRTLights();

		if ( CaptureTheFlag || CTF )
			GoalLoad();

		InitThermo();

		if( ( ChangeLevel_MyGameStatus == STATUS_SinglePlayer ) ||
			( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
			( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer) )
		{
			LoadTimeForLevel();
		}

		if( !LoadPickupsPositions() )
		{
			SeriousError = true;
			Msg( "LoadPickupsPositions() failed\n" );
			return( false );
		}

		// might not be any External Forces...
		ExternalForcesLoad( (char*) &ExternalForceNames[LevelNum][0] );

		// might not be any Teleports...
		TeleportsLoad( (char*) &TeleportNames[LevelNum][0] );

		// Can Cope with no Zone file!!!
		TriggerAreaload( (char*) &ZoneNames[LevelNum][0] );

		InitShipsChangeLevel(&Mloadheader);

		if( ( ChangeLevel_MyGameStatus == STATUS_SinglePlayer ) ||
			( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
			( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer) )
		{
			if( CountDownOn )
			{
				CreateCountdownDigits();
				StartCountDown( (int16_t) SinglePlayerTimeLimit, 0 );
			}
			else
			{
				DeleteCountdownDigits();
			}
		}

		GodModeOnceOnly = true;

		MyGameStatus = ChangeLevel_MyGameStatus;

		PrintInitViewStatus( MyGameStatus );


		break;

		//  *********************** Single Player Game Stuff **********************************


	case  STATUS_StartingSinglePlayer:
		DebugState("STATUS_StartingSinglePlayer\n");

		MenuAbort();
		ReleaseView();

		MyGameStatus = STATUS_PostStartingSinglePlayer;
		GameStatus[WhoIAm] = MyGameStatus;
		SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
		LevelNum = -1;
		if( !NewLevelNum )
		{
			NumGoldBars = 0;
			CrystalsFound = 0;
		}
		GameCompleted = GameComplete::NotComplete;
		if( !ChangeLevel() ) return( false );
#ifdef __3DS__
		/* [3DS] Clear the colour buffer to black after level load completes.
		 *
		 * ChangeLevel() contains its own internal render loop (STATUS_InitView_0,
		 * STATUS_ChangeLevelPostInitView, etc.) that renders loading-screen frames
		 * into the colour buffer and pumps c3d_present(), updating GX
		 * display buffer A.  When ChangeLevel returns, one of the two GX display
		 * buffers (A) still contains the last loading-screen frame.
		 *
		 * On the STATUS_StartingSinglePlayer break, c3d_present() is called once
		 * more by platform_render_present(); this frame uses whatever is currently
		 * in the colour buffer (the last loading frame).  That result is
		 * DMA'd into GX display buffer B, contaminating the second buffer too.
		 *
		 * Fix: clear the colour buffer to black here so that the
		 * c3d_present() call at the end of this frame sends black to display B
		 * instead of the stale loading frame.  STATUS_PostStartingSinglePlayer
		 * (next frame) clears it again, which sends black to display A.
		 * Both GX display buffers are now black before the first gameplay frame
		 * renders, eliminating the alternating stale-frame flicker. */
		FSClearBlack();
#endif
		break;


	case  STATUS_PostStartingSinglePlayer:
		DebugState("STATUS_PostStartingSinglePlayer\n");

#ifdef __3DS__
		/* [3DS] Second black-frame clear — flushes the remaining GX display buffer.
		 *
		 * The GX double-buffer scheme means there are always two display buffers
		 * (A and B) alternating each swap.  STATUS_StartingSinglePlayer's
		 * FSClearBlack() + c3d_present() cleared buffer B.  This frame's
		 * FSClearBlack() + c3d_present() clears buffer A.  After both frames
		 * complete, both display buffers contain black and neither holds stale
		 * loading-screen or title-screen content.  The first STATUS_SinglePlayer
		 * frame renders real gameplay into clean buffers. */
		FSClearBlack();

		/* [3DS] Eliminate stale ESCAPE injection that opens MENU_InGameSingle.
		 *
		 * Three-step fix:
		 *
		 * 1. hidScanInput() — updates the libctru HID "previous state" baseline.
		 *    ChangeLevel() blocks for several seconds without calling hidScanInput(),
		 *    so buttons pressed during loading appear as kDown (fresh press) on the
		 *    first post-load handle_events() scan.  Calling it here means the next
		 *    handle_events() scan sees those buttons as kHeld, not kDown — so no
		 *    spurious SDLK_ESCAPE is injected on Frame N+1.
		 *
		 * 2. input_buffer_reset() — clears any SDLK_ESCAPE that THIS frame's
		 *    handle_events() already added before we ran.  handle_events() runs
		 *    first in the main loop (before RenderScene), so the escape key is
		 *    already in input_buffer by the time we reach STATUS_PostStartingSinglePlayer.
		 *    ProcessGameKeys() is only called from STATUS_SinglePlayer (next frame),
		 *    but we clear the buffer here so the next frame's handle_events()
		 *    input_buffer_reset() at line 151 has a clean slate.
		 *
		 * 3. MenuAbort() — directly sets CurrentMenu = NULL as a hard safety net.
		 *    Even if SDLK_ESCAPE somehow reached MenuRestart via an earlier code
		 *    path, this ensures no menu is active when STATUS_SinglePlayer begins.
		 */
		hidScanInput();
		input_buffer_reset();
		MenuAbort();
		_gameplay_start_guard = true;  /* arm one-shot for STATUS_SinglePlayer */
#endif

		smallinitShip( WhoIAm );

		MyGameStatus = STATUS_SinglePlayer;
		GameStatus[WhoIAm] = MyGameStatus;
		LevelTimeTaken = 0.0F;
		timer_run( &level_time );

		input_grab( true );

		if( SavedGameDamaged )
		{
			AddColourMessageToQue( SystemMessageColour, SAVED_GAME_DAMAGED );
			SavedGameDamaged = false;
		}

		break;


	case STATUS_SinglePlayer:
		DebugState("STATUS_SinglePlayer\n");

#ifdef __3DS__
		/* [3DS] One-shot guard: on the very first STATUS_SinglePlayer frame, clear
		 * any SDLK_ESCAPE that handle_events() may have injected this frame
		 * (from stale kDown of buttons held during ChangeLevel), and ensure
		 * CurrentMenu is NULL.  This fires BEFORE MainGame() / ProcessGameKeys()
		 * so no spurious MENU_InGameSingle can open on the first gameplay frame. */
		if (_gameplay_start_guard) {
			_gameplay_start_guard = false;
			input_buffer_reset();
			MenuAbort();
			/* Flush any accumulated screen polys (menu text, VDU text) that were
			 * added during the title/loading phase but never killed.  Without this
			 * they all render on the first gameplay frame producing the hall-of-
			 * mirrors text overlay. */
			/* Kill ALL persistent screen polys (menu items, VDU text, level
			 * description text) left over from the title screen.  VduClear() only
			 * clears VDU-specific entries; InitScrPolys() resets the entire
			 * ScrPolys[] linked list so nothing stale renders this frame. */

			InitScrPolys();
		}
#endif

		if( MainGame() != true )
			return false;

		if ( bSoundEnabled && !CurrentMenu )
		{
			ProcessLoopingSfx();
			ProcessEnemyBikerTaunt();
		}

		LevelTimeTaken += timer_run( &level_timer );

		// if player is quiting nothing should stop him.....
		if( MyGameStatus != STATUS_QuitCurrentGame )
		{
			if( IsHost )
			{
				if( NewLevelNum != LevelNum )
				{
					clear_black();
					ReleaseView();
					// the level has ended or changed...
					MyGameStatus = STATUS_ViewingStats;
					GameStatus[WhoIAm] = MyGameStatus;
					InitView();
				}
			}
		}
		break;


	case STATUS_ViewingStats:
		DebugState("STATUS_ViewingStats\n");

		clear_black();

		ReleaseLevel();

		ReleaseView();

		MyGameStatus = STATUS_BetweenLevels;
		InitScene();
		InitView();
		MenuAbort();
		MenuRestart( &MENU_NEW_NumberOfCrystals );
		CameraStatus = CAMERA_AtRightVDU;
		break;


	case  STATUS_TitleLoadGameStartingSinglePlayer:
		DebugState("STATUS_TitleLoadGameStartingSinglePlayer\n");

		MenuAbort();
		ReleaseView();
		// tell them all to load up a level
		MyGameStatus = STATUS_TitleLoadGamePostStartingSinglePlayer;
		GameStatus[WhoIAm] = MyGameStatus;
		LevelNum = -1;
		NumGoldBars = 0;
		CrystalsFound = 0;
		GameCompleted = GameComplete::NotComplete;
		if( !ChangeLevel() ) return( false );
		break;


	case  STATUS_InGameLoadGameStartingSinglePlayer:
		DebugState("STATUS_InGameLoadGameStartingSinglePlayer\n");

		NewLevelNum = InGameLoadGameLevelNum;
		MenuAbort();
		ReleaseView();
		// tell them all to load up a level
		MyGameStatus = STATUS_TitleLoadGamePostStartingSinglePlayer;
		GameStatus[WhoIAm] = MyGameStatus;
		LevelNum = -1;
		NumGoldBars = 0;
		CrystalsFound = 0;
		GameCompleted = GameComplete::NotComplete;
		if( !ChangeLevel() ) return( false );
		break;


	case  STATUS_TitleLoadGamePostStartingSinglePlayer:
		DebugState("STATUS_TitleLoadGamePostStartingSinglePlayer\n");

		smallinitShip( WhoIAm );
		if( !InGameLoad( nullptr ) )
		{
			/* The save is truncated or damaged, and part of it may already
			   be applied over the level. Start its level again from the
			   beginning instead; STATUS_PostStartingSinglePlayer says why. */
			SavedGameDamaged = true;
			StartASinglePlayerGame( nullptr );
			break;
		}


		MyGameStatus = STATUS_SinglePlayer;
		GameStatus[WhoIAm] = MyGameStatus;
		break;

	default:
		DebugPrintf("MyGameStatus has a bad setting!!!!\n");
		break;

		//  ******************** End of Single Player Game Stuff *******************************

	}

	return true;
}

void MainGameDemoRoutines(){
}

/*===================================================================
  Procedure :   Main Routines...
  Input   :   nothing...
  Output    :   nothing
===================================================================*/
void MainRoutines( void )
{
	ProcessShips();


	FirePrimary();
	FireSecondary();
	ProcessEnemies();
	ProcessSpotFX();
	ProcessPrimaryBullets();
	ProcessSecondaryBullets();
	RegeneratePickups();
	ProcessPickups();
	ProcessBGObjects( true );
	ProcessModels();
	ProcessPolys();
	ProcessXLights( &Mloadheader );
	DoAfterBurnerEffects();
	FmPolyProcess();
	CheckTimeLimit();
	if( CountDownOn ) UpdateCountdownDigits();
	if(!CurrentMenu)
		ShowScreenMultiples();
	ProcessActiveConditions();
	ProcessTriggerAreas();
	ProcessGoals();
	WaterProcess();
	ProcessRTLights();

}

void CheckLevelEnd ( void )
{

	if( HasLevelEnded() )
	{
		PauseAllSfx();
		LevelNum = -1;
		if( NewLevelNum == ( LevelList.items - 2 ) )
		{
			// if they cheated at any point
			if ( Cheated )
			{
				GameCompleted = GameComplete::Cheated;
			}else
			{
				// Check for Secret level...
				if( CrystalsFound == ( LevelList.items - 1 ) )
				{
					//They can go to the secret level
					NewLevelNum = ( LevelList.items - 1 );
					GameCompleted = GameComplete::GotAllCrystalsSoFar;
				}else{
					// go straight to the end sequence
					GameCompleted = GameComplete::WithoutAllCrystals;
				}
			}
		}else if( NewLevelNum == ( LevelList.items - 1 ) ){
			// They have completed the Secret level
			if( CrystalsFound == ( LevelList.items ) )
			{
				GameCompleted = GameComplete::WithAllCrystals;
			}else
			{
				GameCompleted = GameComplete::WithAllCrystalsExceptLast;
			}
		}else{
			NewLevelNum++;
			GameCompleted = GameComplete::NotComplete;
		}

		return;
	}

}


/* "Invert 3D depth" toggle (3DS crate menu Options -> 3DS Visual Settings,
 * persisted as Invert3D in configs/main.txt). Flips the sign of the per-eye
 * parallax offset below, swapping pop-out vs. recede-into-depth without
 * touching the eye->panel (GFX_LEFT/RIGHT) mapping. Default false = the
 * shipped behaviour. Defined here (always compiled) so the flip works for
 * every stereo mode; the toggle UI is citro3d-only. */
bool g_stereo_invert = false;

/* The group an eye is in: walk its offset from the camera through the
   portals. Each eye sits half the eye separation to the camera's side; near
   a doorway that can be past the portal plane while the camera (the ship)
   is not. FindVisible starts from the camera's group and rejects a portal
   whose plane the viewpoint is behind, so an eye left in the ship's group
   would not see the room it is already in, and that eye showed black for
   a frame or two when flying through doorways at an angle. */
static int16_t EyeGroup( VECTOR *eye_offset )
{
	if ( CurrentCamera.GroupImIn < 0 || CurrentCamera.GroupImIn >= Mloadheader.num_groups )
		return CurrentCamera.GroupImIn;
	return (int16_t) MoveGroup( &Mloadheader, &CurrentCamera.Pos, (u_int16_t) CurrentCamera.GroupImIn, eye_offset );
}

bool RenderCurrentCameraInStereo( RenderCurrentCameraPt render_camera )
{
	VECTOR cam_offset;
	VECTOR left_offset;
	render_viewport_t old_viewport = viewport;
	int16_t centre_group = CurrentCamera.GroupImIn;
	int16_t left_group, right_group;

	/* Compute the camera-right offset in world space.
	 * In D3D row-vector convention (FinalMat), the camera local X axis in
	 * world space is the first ROW of the rotation submatrix: (_11,_12,_13).
	 * Do NOT use ApplyMatrix — it adds _41/_42/_43 (world translation). */
	{
		float half_sep = render_info.stereo_eye_sep / 2.0f;
		if (g_stereo_invert) half_sep = -half_sep;  /* pop-out <-> depth */
		cam_offset.x = CurrentCamera.Mat._11 * half_sep;
		cam_offset.y = CurrentCamera.Mat._12 * half_sep;
		cam_offset.z = CurrentCamera.Mat._13 * half_sep;
	}
	left_offset.x = -cam_offset.x;
	left_offset.y = -cam_offset.y;
	left_offset.z = -cam_offset.z;
	left_group = EyeGroup( &left_offset );
	right_group = EyeGroup( &cam_offset );

	if(render_info.stereo_mode == STEREO_MODE_HALF_HEIGHT)
	{
		viewport.Height /= 2;
		viewport.ScaleX /= 2;
		CurrentCamera.Viewport.Height = viewport.Height;
		CurrentCamera.Viewport.ScaleY = viewport.ScaleX;
	}
	else if (render_info.stereo_mode == STEREO_MODE_HALF_WIDTH)
	{
		viewport.Width /= 2;
		viewport.ScaleY /= 2;
		CurrentCamera.Viewport.Width = viewport.Width;
		CurrentCamera.Viewport.ScaleX = viewport.ScaleY;
	}
	//
	// render left eye
	//
	render_info.stereo_position = ST_LEFT;
	CurrentCamera.Pos.x -= cam_offset.x;
	CurrentCamera.Pos.y -= cam_offset.y;
	CurrentCamera.Pos.z -= cam_offset.z;
	CurrentCamera.GroupImIn = left_group;
	if(render_info.stereo_mode == STEREO_MODE_COLOR)
		render_set_filter( 1, 0, 0 );
	if( !render_camera() )
	{
		CurrentCamera.GroupImIn = centre_group;
		return false;
	}
#ifdef __3DS__
	/* Hardware stereo: the second pass draws to the right-eye target. */
	if(render_info.stereo_mode == STEREO_MODE_3DS)
	{

		c3d_transfer_eye(GFX_LEFT);
	}
#endif
	//
	// render right eye
	//
	render_info.stereo_position = ST_RIGHT;
	CurrentCamera.Pos.x += 2.0f * cam_offset.x;
	CurrentCamera.Pos.y += 2.0f * cam_offset.y;
	CurrentCamera.Pos.z += 2.0f * cam_offset.z;
	CurrentCamera.GroupImIn = right_group;
	if(render_info.stereo_mode == STEREO_MODE_COLOR)
	{
		switch( render_info.stereo_right_color )
		{
		case ST_GREEN:
			render_set_filter( 0, 1, 0 );
			break;
		case ST_BLUE:
			render_set_filter( 0, 0, 1 );
			break;
		default:
			render_set_filter( 0, 1, 1 );
			break;
		}
	}
	else if ( render_info.stereo_mode == STEREO_MODE_HALF_HEIGHT )
	{
		viewport.Y += viewport.Height; // already split in 2 above
		CurrentCamera.Viewport.Y = viewport.Y;
	}
	else if ( render_info.stereo_mode == STEREO_MODE_HALF_WIDTH )
	{
		viewport.X += viewport.Width; // already split in 2 above
		CurrentCamera.Viewport.X = viewport.X;
	}
	if( !render_camera() )
	{
		CurrentCamera.GroupImIn = centre_group;
		return false;
	}
#ifdef __3DS__
	if(render_info.stereo_mode == STEREO_MODE_3DS)
	{

		c3d_transfer_eye(GFX_RIGHT);
	}
#endif
	//
	// reset back to normal center camera
	//
	render_info.stereo_position = ST_CENTER;
	viewport = old_viewport;
	CurrentCamera.Pos.x -= cam_offset.x;
	CurrentCamera.Pos.y -= cam_offset.y;
	CurrentCamera.Pos.z -= cam_offset.z;
	CurrentCamera.GroupImIn = centre_group;
	CurrentCamera.Viewport.Height = viewport.Height;
	CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / 2.0f;
	CurrentCamera.Viewport.Width = viewport.Width;
	CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / 2.0f;
	CurrentCamera.Viewport.Y = viewport.Y;
	CurrentCamera.Viewport.X = viewport.X;
	if(render_info.stereo_mode == STEREO_MODE_COLOR)
		render_set_filter( 1, 1, 1 );
	if (!FSSetViewPort(&viewport))
	{
	#ifdef DEBUG_VIEWPORT
		SetViewportError( "RenderCurrentCamera2", &viewport );
	#else
		Msg("SetViewport failed.\n%s", render_error_description(0));
	#endif
		return false;
	}
	return true;
}


void DrawMainGameMenu(void)
{
	if( CurrentMenu && CurrentMenuItem )
	{
		MenuDraw( CurrentMenu );
		MenuItemDrawCursor( CurrentMenuItem );
	}
#ifdef __3DS__
	/* [3DS citro3d] Unconditionally tag DrawSimplePanel's screen polys with
	   SCRFLAG_BottomHUD so the two-pass drain routes them to the mono
	   bottom screen. The in-world crosshair inside DrawSimplePanel clears
	   the flag locally so it stays on the stereo top screen. Override
	   render_info dims to 320×240 around the call so Print4x5Text's
	   position math targets the bottom resolution. */
	ScrPolySetRouteBottom(true);
	render_info.window_size.cx = 320;
	render_info.window_size.cy = 240;
	render_info.ThisMode.w = 320;
	render_info.ThisMode.h = 240;
	{

		if (controls_3ds_menu_is_open()) {
			/* Menu open: skip HUD, draw menu text only. The next
			 * frame after menu closes restores HUD automatically. */
			controls_3ds_render_overlay();
		} else {
			DrawSimplePanel();
		}
	}
	ScrPolySetRouteBottom(false);
	render_info.window_size.cx = 400;
	render_info.window_size.cy = 240;
	render_info.ThisMode.w = 400;
	render_info.ThisMode.h = 240;
#else
	DrawSimplePanel();
#endif
}

bool RenderMainCamera2dPolys(void);
bool RenderCurrentCameraWithMainGameMenu(void)
{
	if(!RenderCurrentCamera())
		return false;
	DrawMainGameMenu();

	/* Missile / Pip camera inset — must render here (per-eye) rather
	 * than after the stereo block. In stereo mode, RenderCurrentCameraInStereo
	 * invokes this callback twice (once per eye target). Rendering the
	 * inset outside the stereo block means it only lands in one eye's
	 * target, producing a visible "primary bleed-through" in the other
	 * eye's inset region. Putting it inside here gives both eyes a
	 * consistent inset.
	 *
	 * We save/restore CurrentCamera + CameraRendering so the stereo
	 * dispatcher's per-eye position offset math (applied to
	 * CurrentCamera.Pos) uses the SHIP's matrix between eye calls,
	 * not the leftover missile state from this block. */
	if( ActiveRemoteCamera || (MissileCameraActive && MissileCameraEnable) )
	{
		CAMERA saved_camera = CurrentCamera;
		CamRendering saved_cam_rendering = CameraRendering;
		BYTE saved_current_cam_view = Current_Camera_View;
		float main_fov;

		TempMissileCam = Current_Camera_View;
		Current_Camera_View = INVALID_CAMERA_VIEW;
		if( ActiveRemoteCamera )
		{
			CameraRendering = CamRendering::Pip;
			CurrentCamera.enable = 1;
			CurrentCamera.GroupImIn = ActiveRemoteCamera->Group;
			CurrentCamera.Mat = ActiveRemoteCamera->Mat;
			CurrentCamera.InvMat = ActiveRemoteCamera->InvMat;
			CurrentCamera.Pos = ActiveRemoteCamera->Pos;
		}
		else
		{
			CameraRendering = CamRendering::Missile;
			CurrentCamera.enable = 1;
			CurrentCamera.GroupImIn = SecBulls[ CameraMissile ].GroupImIn;
			CurrentCamera.Mat = SecBulls[ CameraMissile ].Mat;
			MatrixTranspose( &SecBulls[ CameraMissile ].Mat, &CurrentCamera.InvMat );
			CurrentCamera.Pos = SecBulls[ CameraMissile ].Pos;
		}
		CurrentCamera.Viewport = viewport;
		main_fov = hfov;
		SetFOV( normal_fov );
		CurrentCamera.Proj = proj;

		CurrentCamera.Viewport.X = viewport.X + (viewport.Width >>4);
		CurrentCamera.Viewport.Y = viewport.Y + (viewport.Height >>4);
		CurrentCamera.Viewport.Width = viewport.Width >>2;
		CurrentCamera.Viewport.Height = viewport.Height >>2;
		CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / (float)2.0;
		CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / (float)2.0;

		CurrentCamera.UseLowestLOD = true;

#ifdef __3DS__
		c3d_memo_suspend(true);
#endif
		bool inset_ok = RenderCurrentCamera();
#ifdef __3DS__
		c3d_memo_suspend(false);
#endif
		if( inset_ok != true )
			return false;

		Current_Camera_View = TempMissileCam;
		SetFOV( main_fov );

		/* Restore ship's camera state so the next eye iteration's
		 * stereo-offset math operates on the ship, not on leftover
		 * missile position. */
		CurrentCamera = saved_camera;
		CameraRendering = saved_cam_rendering;
		Current_Camera_View = saved_current_cam_view;
	}

#ifdef __3DS__
	/* [3DS citro3d] Two-pass screen-poly drain:
	 *   Pass 1: mode=1, current (stereo top) target — draws everything
	 *           WITHOUT SCRFLAG_BottomHUD (in-world overlays: aim reticle,
	 *           enemy markers, damage flash).
	 *   Pass 2: mode=2, bottom target — draws only polys WITH
	 *           SCRFLAG_BottomHUD (gameplay text HUD tagged inside
	 *           DrawSimplePanel).
	 * Mode=0 restored so Display* runs outside this path (title, crate
	 * menu) behave as before. c3d_hud_begin_bottom is a no-op on the second
	 * eye pass; the bottom screen is mono and doesn't need a second copy. */
	/* FPS overlay — queue an FPS-counter text poly with the bottom-HUD
	 * route flag so RenderMainCamera2dPolys's pass-2 (mode=2) picks
	 * it up. Sits just above the lower-left game clock readout (drawn
	 * at y = window_cy - FontWidth*5 in DrawSimplePanel). Top-right
	 * placement collided with the secondary-weapon name slot which
	 * cycles through "Quantum" / "Solaris" / etc and obscured the
	 * counter completely. */
	{



		char fps_buf[24];
		snprintf(fps_buf, sizeof(fps_buf), "FPS:%d", forsaken_get_fps());
		ScrPolySetRouteBottom(true);
		/* Bottom screen is 320×240. Clock sits at y = 240 - 30 = 210
		 * (DrawSimplePanel) but the actual character glyphs render
		 * up from a baseline that starts a few px above the y arg,
		 * so y=200 had the FPS overlap with the clock. y=190 leaves
		 * a clean ~10px gap between FPS row and clock row. */
		Print4x5Text(fps_buf, 6, 190, 1 /* white */);
		ScrPolySetRouteBottom(false);
	}

	ScrPolySetDisplayMode(1);
	RenderMainCamera2dPolys();
	if (c3d_hud_begin_bottom())
	{
		ScrPolySetDisplayMode(2);
		RenderMainCamera2dPolys();
		c3d_hud_end_bottom();
	}
	ScrPolySetDisplayMode(0);
#else
	RenderMainCamera2dPolys();
#endif
	return true;
}

void SetFOVBasedOnShipSpeed(void)
{
	static float fov_inc = 0.0F;
	if( (Ships[WhoIAm].Object.Speed.z) > (MaxMoveSpeed) )
	{
		float diff;
		diff = ( Ships[WhoIAm].Object.Speed.z - MaxMoveSpeed ) / (  MaxTurboSpeed - MaxMoveSpeed );

		if( diff > 0.0F )
		{
			fov_inc += diff * diff * framelag;
			if ( fov_inc > 30.0F )
				fov_inc = 30.0F;
		}
		else
		{
			fov_inc *= (float) powf( 0.95, framelag );
		}
	}
	else
	{
		fov_inc *= (float) powf( 0.95, framelag );
	}
	SetFOV( chosen_fov + fov_inc );
}

bool MainGameRender(void)
{
	if (!FSBeginScene())
	{
		return false;
	}

	// show the stats screen
	if(ShowStats)
	{
		ScoreDisplay();
	}

	// show regular view
	else if( !FullRearView )
	{
		CameraRendering = CamRendering::Main;
		MainCamera.enable = 1;
		MainCamera.GroupImIn = Ships[Current_Camera_View].Object.Group;
		MainCamera.Mat = Ships[Current_Camera_View].Object.FinalMat;
		MainCamera.InvMat = Ships[Current_Camera_View].Object.FinalInvMat;
		MainCamera.Pos = Ships[Current_Camera_View].Object.Pos;
		MainCamera.Viewport = viewport;
		MainCamera.Proj = proj;

		CurrentCamera = MainCamera;

		HUDNames();
		DispHUDNames();
		DispUntriggeredNMEs();

		CurrentCamera.UseLowestLOD = false;

#ifdef __3DS__
		/* [3DS] Drive stereo from the 3D slider. */
		{

			float slider = platform_get_3d_slider();
			if (slider > 0.0f)
			{
				render_info.stereo_enabled = true;
				gfxSet3D(true);
				render_info.stereo_mode = STEREO_MODE_3DS;
				/* Max 3D effect halved 2026-11-14 (owner: full slider
				 * was too strong; half-slider was the de facto max).
				 * Slider now maps 0..15 instead of 0..30 eye-sep units. */
				render_info.stereo_eye_sep = slider * 15.0f;
			}
			else
			{
				gfxSet3D(false);
				render_info.stereo_enabled = false;
			}
		}
#endif

		if( render_info.stereo_enabled )
		{
			if(!RenderCurrentCameraInStereo(RenderCurrentCameraWithMainGameMenu))
				return false;
		}
		else // non stereo - normal rendering
		{
			if( RenderCurrentCameraWithMainGameMenu() != true )
			{
				return false;
			}
		}

#ifdef __3DS__
		/* [3DS] Rear camera disabled on 3DS.
		 * The rear camera requires a second full render pass (BSP traversal,
		 * texture binding, GPU command submission) into a sub-viewport.  The
		 * PICA200 renders ~1-2M triangles/sec practical, so the extra pass can
		 * drop the frame below 20fps and cause the GX command queue to overflow,
		 * producing tearing artefacts.  The rear-view feature is removed for now;
		 * it can be re-enabled if performance allows. */
		if( false )
#else
			if( RearCameraActive && !RearCameraDisable )
#endif
			{
				CameraRendering = CamRendering::Rear;

			CurrentCamera.enable = 1;
			CurrentCamera.UseLowestLOD = true;
			CurrentCamera.GroupImIn = Ships[Current_Camera_View].Object.Group;
			CurrentCamera.Mat = Ships[Current_Camera_View].Object.FinalMat;
			CurrentCamera.InvMat = Ships[Current_Camera_View].Object.FinalInvMat;
			CurrentCamera.Pos = Ships[Current_Camera_View].Object.Pos;
			CurrentCamera.Viewport = viewport;
			CurrentCamera.Proj = proj;
			CurrentCamera.InvMat._31 *= -1.0F;
			CurrentCamera.InvMat._32 *= -1.0F;
			CurrentCamera.InvMat._33 *= -1.0F;
			CurrentCamera.InvMat._11 *= -1.0F;
			CurrentCamera.InvMat._12 *= -1.0F;
			CurrentCamera.InvMat._13 *= -1.0F;
			CurrentCamera.Mat._13 *= -1.0F;
			CurrentCamera.Mat._23 *= -1.0F;
			CurrentCamera.Mat._33 *= -1.0F;
			CurrentCamera.Mat._11 *= -1.0F;
			CurrentCamera.Mat._21 *= -1.0F;
			CurrentCamera.Mat._31 *= -1.0F;
			CurrentCamera.Viewport.X = ( viewport.X + viewport.Width ) - ( (viewport.Width >>4) + ( viewport.Width >>2 ) );
			CurrentCamera.Viewport.Y = viewport.Y + (viewport.Height >>4);
			CurrentCamera.Viewport.Width = viewport.Width >>2;
			CurrentCamera.Viewport.Height = viewport.Height >>2;
			CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / 2.0f;
			CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / 2.0f;

			CurrentCamera.UseLowestLOD = true;

			if( RenderCurrentCamera() != true )
				return false;
		}

		/* Missile / Pip camera inset is now rendered inside
		 * RenderCurrentCameraWithMainGameMenu so that, in stereo mode,
		 * both eye targets get a consistent inset rather than only the
		 * right eye (which made the other eye show primary geometry
		 * where the inset should be, visible in anaglyph as
		 * "primary clipping through"). */

		// Observatory - show four ship camaras
		if( SwitchedToWatchMode && WatchPlayerSelect.value == MAX_PLAYERS+1 )
		{
			float main_fov;
			TempMissileCam = Current_Camera_View;
			Current_Camera_View = INVALID_CAMERA_VIEW;
			CameraRendering = CamRendering::Pip;
			CurrentCamera.enable = 1;
			CurrentCamera.Viewport = viewport;
			CurrentCamera.Viewport.Width = viewport.Width / 2.0f;
			CurrentCamera.Viewport.Height = viewport.Height / 2.0f;
			CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / 2.0f;
			CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / 2.0f;
			CurrentCamera.UseLowestLOD = true;
			main_fov = hfov;
			SetFOV( normal_fov );
			CurrentCamera.Proj = proj;
			Current_Camera_View=TempMissileCam;
			SetFOV( main_fov );

			int CamerasSet=0;
			int i;
			// pick first four active ship camaras
			for(i=0; i<MAX_PLAYERS && CamerasSet < 4; i++)
			{
				if(GameStatus[ i ] == STATUS_Normal && i != WhoIAm)
				{
					SetCam(i, CamerasSet);
					CamerasSet++;
				}
			}
			// black out other camaras
			while(CamerasSet < 4)
			{
				SetCam(MAX_PLAYERS+1, CamerasSet);
				CamerasSet++;
			}
		}
	}

	// Full Screen Rear View....
	else
	{
		CameraRendering = CamRendering::Rear;
		CurrentCamera.enable = 1;
		CurrentCamera.GroupImIn = Ships[Current_Camera_View].Object.Group;
		CurrentCamera.Mat = Ships[Current_Camera_View].Object.FinalMat;
		CurrentCamera.InvMat = Ships[Current_Camera_View].Object.FinalInvMat;
		CurrentCamera.Pos = Ships[Current_Camera_View].Object.Pos;
		CurrentCamera.Viewport = viewport;
		CurrentCamera.Proj = proj;
		CurrentCamera.InvMat._31 *= -1.0F;
		CurrentCamera.InvMat._32 *= -1.0F;
		CurrentCamera.InvMat._33 *= -1.0F;
		CurrentCamera.InvMat._11 *= -1.0F;
		CurrentCamera.InvMat._12 *= -1.0F;
		CurrentCamera.InvMat._13 *= -1.0F;
		CurrentCamera.Mat._13 *= -1.0F;
		CurrentCamera.Mat._23 *= -1.0F;
		CurrentCamera.Mat._33 *= -1.0F;
		CurrentCamera.Mat._11 *= -1.0F;
		CurrentCamera.Mat._21 *= -1.0F;
		CurrentCamera.Mat._31 *= -1.0F;
		CurrentCamera.UseLowestLOD = false;

		if( RenderCurrentCamera() != true )
			return false;
	}

	if( Our_CalculateFrameRate() != true)
		return false;

	/* do the target c omputer trick */
	if( TargetComputerOn )
	{
		set_alpha_states();
		DispTracker();
		set_normal_states();
	}

	if (!FSEndScene())
	{
		return false;
	}

	return true;
}


/*===================================================================
  Procedure :   Main Render Loop...
  Input   :   nothing...
  Output    :   nothing
===================================================================*/
bool MainGame( void )
{
	int i;

	MainGameDemoRoutines();

#ifdef DEBUG_ON
	if ( framelag > 10.0F ) // check framelag out of reasonable range -> probably debugging
	{
		framelag = 10.0F; // clamp framelag to something reasonable
	}
#endif


	/*===================================================================
	  Procedure :  Main Routines to be called before Rendering....
	===================================================================*/

	InitIndirectVisible( Ships[Current_Camera_View].Object.Group );

	if( ActiveRemoteCamera || (MissileCameraActive && MissileCameraEnable) )
		AddIndirectVisible( (u_int16_t) ( ( ActiveRemoteCamera ) ? ActiveRemoteCamera->Group : SecBulls[ CameraMissile ].GroupImIn ) );

	MainRoutines();

	if( MyGameStatus == STATUS_QuitCurrentGame )
		return true;

	memset( (void*) &IsGroupVisible[0] , 0 , MAXGROUPS * sizeof(u_int16_t) );
	cral += (framelag*2.0F);

	for( i = 0 ; i < MAX_SFX ; i++ )
		LastDistance[i] = 100000.0F;

	if(!MainGameRender())
		return false;

	MenuProcess(); // menu keys are processed here
	ProcessGameKeys(); // here is where we process F keys
	ScreenPolyProcess();

	Inside = PointInsideSkin( &Ships[WhoIAm].Object.Pos, Ships[WhoIAm].Object.Group );

	/* Secondary routines called after rendering */

	SetFOVBasedOnShipSpeed();

	CheckLevelEnd();

	NetworkGameUpdate();

	return true;
}

/*===================================================================
  Procedure :   Set Camera on the specified ship (for observatory)...
===================================================================*/
void SetCam(int ship, int Cam)
{
	CurrentCamera.GroupImIn = Ships[ship].Object.Group;
	CurrentCamera.Mat = Ships[ship].Object.FinalMat;
	CurrentCamera.InvMat = Ships[ship].Object.FinalInvMat;
	CurrentCamera.Pos = Ships[ship].Object.Pos;

	switch(Cam)
	{
		// top left
	case 0:
		CurrentCamera.Viewport.X = 0.0;
		CurrentCamera.Viewport.Y = 0.0;
		Print4x5Text( (char *)GetName(ship), CurrentCamera.Viewport.Width/(float)2.0, CurrentCamera.Viewport.Height-(2.0*FontHeight), 4 );
		break;

		// bottom left
	case 1:
		CurrentCamera.Viewport.X = 0.0;
		CurrentCamera.Viewport.Y = CurrentCamera.Viewport.Height;
		Print4x5Text( (char *)GetName(ship), CurrentCamera.Viewport.Width/(float)2.0, viewport.Height-(2.0*FontHeight), 4 );
		break;

		// top right
	case 2:
		CurrentCamera.Viewport.X = CurrentCamera.Viewport.Width;
		CurrentCamera.Viewport.Y = 0.0;
		Print4x5Text( (char *)GetName(ship), (viewport.Width/(float)2.0) + (CurrentCamera.Viewport.Width/(float)2.0), CurrentCamera.Viewport.Height-(2.0*FontHeight), 4 );
		break;

		// bottom right
	case 3:
		CurrentCamera.Viewport.X = CurrentCamera.Viewport.Width;
		CurrentCamera.Viewport.Y = CurrentCamera.Viewport.Height;
		Print4x5Text( (char *)GetName(ship), (viewport.Width/(float)2.0) + (CurrentCamera.Viewport.Width/(float)2.0), viewport.Height-(2.0*FontHeight), 4 );
		break;
	}

	if( RenderCurrentCamera() != true )
		return;
}


/*===================================================================
  Procedure :   Show In Game Stats...
  Input   :   nothing...
  Output    :   nothing..
===================================================================*/

void ShowDeathModeStats()
{
	ShowGameStats();
	if( Ships[ WhoIAm ].Timer < RESPAWN_TIMER )
		CenterPrint4x5Text( "Press any key to continue" , render_info.window_size.cy - (FontHeight*2) , GREEN );
}

/* Display the Statistics in-game when key is pressed */
void ShowInGameStats()
{
	ShowGameStats();
}


bool StatsNamePulse( void )
{
	static float pulse = 0.0F;
	pulse += real_framelag;

	if (pulse > 1.0F)
		pulse -= (float) floorf( (double) pulse );

	if (pulse <= 0.5F)
		return true;

	return false;
}

void DisplayNetworkInfo()
{
	char buf[256];
	int total_height = 0;
	int top_offset = 0;
	int row_height = (FontHeight+(FontHeight/2));
	int x_center = ( render_info.window_size.cx >>1 );
	int y_center = ( render_info.window_size.cy >>1 );
	int ShipID;

	// get layout information
	for( i = 0; i < MAX_PLAYERS; i++ )
	{
		if( GameStatus[i] != STATUS_Normal || WhoIAm == i )
			continue;

		total_height += (3*row_height);
	}

	top_offset = ( y_center - (total_height / 2) );

	Print4x5Text( "ENET NETWORK INFO:", x_center-(9*FontWidth), top_offset-(row_height*2),  WHITE );

	// print the network information
	for( i = 0; i < MAX_PLAYERS; i++ )
	{
		ShipID = GetPlayerByRank(i);

		if( GameStatus[ShipID] != STATUS_Normal || ShipID == WhoIAm )
			continue;

		int left_offset = x_center - (20*FontWidth);

		// for packet loss
		DisplayConnectionStatus( ShipID, left_offset, top_offset );
		left_offset += ( 2 * FontWidth );

		// print name
		if( TeamGame )
			Print4x5Text( &Names[ShipID][0], left_offset, top_offset, TeamCol[TeamNumber[ShipID]]);
		else
			Print4x5Text( &Names[ShipID][0], left_offset, top_offset,  WHITE );

		left_offset += ( 8 * FontWidth );

		if( Ships[ShipID].network_player != nullptr )
		{
			sprintf( (char*) &buf[0] ,"IP: %s PORT: %d",
				Ships[ShipID].network_player->ip,
				Ships[ShipID].network_player->port);

			Print4x5Text( &buf[0] , left_offset, top_offset, GREEN );

			top_offset+=row_height;

			sprintf( (char*) &buf[0] ,"PING: %lu LOSS: %lu LOST: %lu",
				Ships[ShipID].network_player->ping,
				Ships[ShipID].network_player->packet_loss,
				Ships[ShipID].network_player->packets_lost);

			Print4x5Text( &buf[0] , left_offset, top_offset, GREEN );

			top_offset+=row_height;
			top_offset+=row_height; // blank line for spacing
		}
	}
}

void ShowGameStats()
{
	int active_players = 0;
	int total_rows = 0;
	int total_height = 0;
	int top_offset = 0;
	int row_height = (FontHeight+(FontHeight/2));
	int x_center = ( render_info.window_size.cx >>1 );
	int y_center = ( render_info.window_size.cy >>1 );
	bool FlashName = StatsNamePulse();

	// generate active players

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if ( scoreable_status(i) )
			active_players++;
	}

	// get the total height of everything

	total_rows = 9 + (2* active_players); // calculated statically based on the code  SUCKSSSSS
	total_height = (total_rows * row_height);
	top_offset = ( y_center - (total_height / 2) ); // center whole thing

	// ROWS += 1


	//
	// Kill Matrix
	//

	{
		const char* title = "KILL MATRIX";
		Print4x5Text( title, (x_center-((strlen(title)*FontWidth)/2)), top_offset, WHITE );
		top_offset += (row_height * 2);
	}

	// ROWS += 2

	{
		int i;

		int col_width = (FontWidth * 3);
		int name_width = (FontWidth * MAXSHORTNAME);

		int line_width = name_width + (col_width * active_players);
		int left_offset = x_center - (line_width / 2); // center

		// print first letter of each name

		for (i = 0; i < active_players; i++)
		{
			// calculate name color
			int color = GRAY;
			if (!FlashName && GetPlayerByRank(i) == WhoIAm)
				color = WHITE;
			else if (TeamGame)
				color = TeamCol[TeamNumber[GetPlayerByRank(i)]];

			// x axis
			int xpos = left_offset + name_width + (i * col_width);

			// create string of first letter of player name
			char FirstLetter[2];
			FirstLetter[0] = Names[GetPlayerByRank(i)][0];
			FirstLetter[1] = 0;

			//
			Print4x5Text( FirstLetter,  xpos, top_offset, player_left(GetPlayerByRank(i)) ? DARKGRAY : color );
		}

		top_offset += row_height;

		// ROWS += 1

		// display kill matrix
		for (i = 0; i < active_players; i++)
		{
			// reset value
			int j = 0;
			int xpos = left_offset;

			// calculate name color
			int color = GRAY;
			if (!FlashName && GetPlayerByRank(i) == WhoIAm)
				color = WHITE;
			else if (player_left(GetPlayerByRank(i)))
				color = DARKGRAY;
			else if (TeamGame)
				color = TeamCol[TeamNumber[GetPlayerByRank(i)]];

			// print name
			Print4x5Text( (char*) &Names[GetPlayerByRank(i)], xpos, top_offset, color );
			xpos += name_width;

			// print kill matrix line
			for (j = 0; j < active_players; j++)
			{
				int color = player_left(GetPlayerByRank(i)) ? DARKGRAY : ((i==j) ? RED : GRAY);
				Printint16_t( GetKillStats(GetPlayerByRank(i),GetPlayerByRank(j)), xpos, top_offset, color ); // suacides are red
				xpos+=col_width; // next col
			}

			// next row
			top_offset += row_height;
		}

		// ROWS += active players

	}

	top_offset += (row_height * 4);

	// ROWS += 4

	//
	// Player Stats
	//

	{
		const char* title = "Player Stats";
		Print4x5Text( title, (x_center-((strlen(title)*FontWidth)/2)), top_offset, WHITE );
		top_offset += (row_height * 2);
	}

	// ROWS += 2


	{
		int i;

		int col_width = (FontWidth * MAXSHORTNAME);
		int ncols = (TeamGame) ? 5 : 4;
		int line_width = col_width * ncols;
		int left_offset = x_center - (line_width / 2); // center

		{
			int xpos;
			int col;

			// names and colors
			int columns = 4;
			const char* names[4] = {"TEAM", "OVERALL", "KILLS", "DEATHS"};
			int colors[4]  = {YELLOW,      GRAY,   GREEN,      RED};

			// NOT team game
			if(!TeamGame)
				names[0] = nullptr;

			// start at left
			xpos = left_offset;

			// print the names
			for( col = 0; col < columns; col++ )
				if( names[col] != nullptr )
					Print4x5Text( names[col], (xpos+=col_width), top_offset, colors[col] );

		}

		top_offset += (row_height * 1);

		// ROWS += 1

		for (i = 0; i < active_players; i++)
		{
			// start of line
			int xpos = left_offset;

			// if the player left the game
			int left = player_left(GetPlayerByRank(i));

			// calculate name color
			int color = GRAY;
			if (!FlashName && GetPlayerByRank(i) == WhoIAm)
				color = WHITE;
			else if (TeamGame)
				color = TeamCol[TeamNumber[GetPlayerByRank(i)]];

			//
			// print line
			//

			// name
			Print4x5Text( (char*)&Names[GetPlayerByRank(i)], xpos, top_offset, (left) ? DARKGRAY : color );

			if(TeamGame)
			{
				// all players (points + kills - suicides - friendly - deaths)
				Printint16_t( GetTeamScore(GetPlayerByRank(i)),	(xpos+=col_width), top_offset, (left) ? DARKGRAY : YELLOW );
			}

			// points + kills - suicides - friendly - deaths
			Printint16_t( GetRealScore(GetPlayerByRank(i)),	(xpos+=col_width), top_offset, (left) ? DARKGRAY : GRAY	);
			// kills - suicides - friendly
			Printint16_t( GetKills(GetPlayerByRank(i)),	(xpos+=col_width), top_offset, (left) ? DARKGRAY : GREEN );
			// suicides + deaths
			Printint16_t( GetTotalDeaths(GetPlayerByRank(i)), (xpos+=col_width), top_offset, (left) ? DARKGRAY : RED );

			// go to next row
			top_offset += row_height;
		}
	}

	// ROWS += active players

}

/*===================================================================
  Procedure :   Score Display...
  Input   :   nothing...
  Output    :   bool true/false
===================================================================*/
/* Display the Statistics */
bool ScoreDisplay()
{
	ShowGameStats();
	CenterPrint4x5Text( "Press Space to continue" , render_info.window_size.cy - (FontHeight*2) , 0 );
	return true;
}


/*===================================================================
  Procedure :   Init Score Display Stuff...
  Input   :   nothing...
  Output    :   bool true/false
===================================================================*/
bool  InitScoreDisplay()
{

	//Create the offscreen surface, by loading our bitmap.

	InitFont();

	// process scores...
	ScoreSort();

	return true;
}

// Clears the target(back) and zbuffer for the current camera
bool ClearBuffers( void )
{
	XYRECT dummy;

	if (!render_info.ok_to_render)
		return false;

	dummy.x1 = CurrentCamera.Viewport.X;
	dummy.y1 = CurrentCamera.Viewport.Y;
	dummy.x2 = CurrentCamera.Viewport.X + CurrentCamera.Viewport.Width;
	dummy.y2 = CurrentCamera.Viewport.Y + CurrentCamera.Viewport.Height;

	if ( WhiteOut > 0.0f )
	{
		FSClearDepth(&dummy);
		return true;
	}

	return FSClear(&dummy);
}

// Clear the Zbuffer
bool ClearZBuffer()
{
	XYRECT dummy;

	if (!render_info.ok_to_render)
		return false;

	dummy.x1 = CurrentCamera.Viewport.X;
	dummy.y1 = CurrentCamera.Viewport.Y;
	dummy.x2 = CurrentCamera.Viewport.X+CurrentCamera.Viewport.Width;
	dummy.y2 = CurrentCamera.Viewport.Y+CurrentCamera.Viewport.Height;

	return FSClearDepth(&dummy);
}


/* Process-lifetime scratch storage for the dynamic per-frame poly
 * batchers (polys.cpp / 2dpolys.cpp / screenpolys.cpp). They write
 * textureGroups[numTextureGroups++] for each face accumulated this
 * frame. Was an inline RENDEROBJECT.textureGroups[64] BSS field
 * before the textureGroups migration; now an explicit MAX_TEXTURE_GROUPS-
 * sized scratch attached to each RenderBuf at InitRenderBufs time. */
static TEXTUREGROUP s_renderbuf_groups[4][MAX_TEXTURE_GROUPS];

void InitRenderBufs()
{
	int i;
	DebugPrintf("InitRenderBufs\n");
	ReleaseRenderBufs();
	// just vertex data
	FSCreateDynamicVertexBuffer(&RenderBufs[0], BATCH_BUFFER_VERTS);
	FSCreateDynamicVertexBuffer(&RenderBufs[1], BATCH_BUFFER_VERTS);
	// index data as well
	FSCreateDynamicVertexBuffer(&RenderBufs[2], BATCH_BUFFER_VERTS);
	FSCreateDynamicIndexBuffer(&RenderBufs[2], BATCH_BUFFER_VERTS * 3);
	// indexed pre-transformed (2d)
	FSCreateDynamic2dVertexBuffer(&RenderBufs[3], BATCH_BUFFER_VERTS);
	FSCreateDynamicIndexBuffer(&RenderBufs[3], BATCH_BUFFER_VERTS * 3);
	for (i = 0; i < 4; i++) {
		RenderBufs[i].textureGroups = s_renderbuf_groups[i];
		RenderBufs[i].vertOwner = VERT_OWNER_SHARED;
	}
}

void ReleaseRenderBufs( void )
{
	DebugPrintf("ReleaseRenderBufs\n");
	FSReleaseRenderObject(&RenderBufs[0]);
	FSReleaseRenderObject(&RenderBufs[1]);
	FSReleaseRenderObject(&RenderBufs[2]);
	FSReleaseRenderObject(&RenderBufs[3]);
}


bool RenderMainCamera2dPolys( void) // renders in game menu and other 2d elements
{
	set_alpha_states();
	DoLensflareEffect();
	DoAllSecBullLensflare();
	if( !DisplayNonSolidScrPolys( &RenderBufs[ 3 ] ) )
	{
		return false;
	}
	set_normal_states();

	if( !DisplaySolidScrPolys( &RenderBufs[ 3 ] ) )
	{
		return false;
	}
	return true;
}

/*===================================================================
  Procedure :  Render 1 Frame Using CurrentCamera...
  Input   :
  Output    : bool true/false
===================================================================*/
bool RenderCurrentCamera( void )
{
	int16_t Count;
	VISGROUP  *g;
	u_int16_t  group;
	NumOfTransExe = 0;
	Build_View();
	CurrentCamera.View = view;
	if (!FSSetView(&view))
	{
		return false;
	}
	if (!FSSetViewPort(&CurrentCamera.Viewport)) {
#ifdef DEBUG_VIEWPORT
		SetViewportError( "RenderCurrentCamera1", &CurrentCamera.Viewport );
#else
		Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
		return false;
	}

	// Ship Model Enable/Disable
	SetShipsVisibleFlag();

	// find visible groups
	FindVisible( &CurrentCamera, &Mloadheader );

	BuildVisibleLightList( CurrentCamera.GroupImIn );

#ifdef GPU_LIGHTING
	/* Pack the camera's visible XLight list into the shader uniform
	 * once per pass. No per-group filter — see c3d_upload_xlights for
	 * why occlusion-aware filtering isn't tractable on PICA200. */
	{ c3d_upload_xlights(); }
#endif

	UpdateBGObjectsClipGroup( &CurrentCamera );
	UpdateEnemiesClipGroup( &CurrentCamera );

	if (ClearBuffers() != true )
	{
		return false;
	}
	// reset all the normal execute status flags...
	set_normal_states();

	// set all the Translucent execute status flags...
	if( WhiteOut != 0.0F)
	{
		set_whiteout_state();
	}

	// display background
	if ( !DisplayBackground( &Mloadheader, &CurrentCamera ) )
		return false;

	// reset all the normal execute status flags...
	if( WhiteOut == 0.0F)
	{
		set_normal_states();
	}


	/* Fresh memo generation for this camera's visible-group walk: ModelDisp
	 * is called once per group and the per-model visibility predicate is
	 * camera-constant, so recomputing it per group is pure waste. */
	ModelVisCache_NewFrame();
	// display clipped opaque objects
	for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
	{
		group = g->group;

		// Do the Background animation for that group.....
		BackGroundTextureAnimation( &Mloadheader , group );

		ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );

		ExecuteLines( group, &RenderBufs[ 0 ] );
		ClipGroup( &CurrentCamera, group );

		if( !ModelDisp( group, &ModelNames[0] ) )
		{
			return false;
		}

	}

	ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );

	// set all the Translucent execute status flags...
	set_alpha_states();


	/*===================================================================
	  Display Skin & BSP Node Info
	===================================================================*/
	if( OldNodeCube || NodeCube || ShowTrigZones || ShowColZones || ShowEFZones || ShowTeleports )
	{
		if( OldNodeCube || NodeCube )
			ClearZBuffer();

		for( Count = 0; Count < MAXGROUPS; Count++ )
		{
			{
				if (!draw_object(&Skin_Execs[ Count ]))
				{
					return false;
				}
			}
		}

	}

	set_alpha_states();

	// display clipped translucencies
	for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
	{
		group = g->group;

		ClipGroup( &CurrentCamera, group );
		GroupWaterProcessDisplay( group );

		/*===================================================================
		  Display Group Clipped Non Faceme Transluecent Polys
		===================================================================*/

		if( !DisplayGroupClippedPolys( &RenderBufs[ 2 ], group ) )
			return false;


		/*===================================================================
		Display Group Clipped Faceme Transluecent Polys
		===================================================================*/

		if( !DisplayGroupClippedFmPolys( &RenderBufs[ 2 ], group ) )
			return false;

		ExecuteTransExe( group );
	}


	ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );


	/*===================================================================
	  Display Non Group Clipped Faceme Transluecent Polys
	===================================================================*/

	if( !DisplayGroupUnclippedFmPolys( &RenderBufs[ 2 ] ) )
		return false;

	/*===================================================================
	  Display Non Group Clipped Non Faceme Transluecent Polys
	===================================================================*/
	if( !DisplayGroupUnclippedPolys( &RenderBufs[ 2 ] ) )
		return false;

	// display unclipped translucencies
	for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
	{
		group = g->group;
		ExecuteTransExeUnclipped( group );
	}


	/*===================================================================
	  Display Portals
	===================================================================*/
	if( ShowPortal )
	{
		for( Count = 0; Count < MAXGROUPS; Count++ )
		{
			{
				if (!draw_object(&Portal_Execs[ Count ]))
				{
					return false;
				}
			}
		}
	}

	set_normal_states();

	if (!FSSetViewPort(&viewport)) {
#ifdef DEBUG_VIEWPORT
		SetViewportError( "RenderCurrentCamera2", &viewport );
#else
		Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
		return false;
	}

	return true;
}

/*===================================================================
  Procedure :  Our Calculate frame rate...
  Input   :
  Output    : bool true/false
===================================================================*/

px_timer_t our_timer;
float our_count = 0;

bool Our_CalculateFrameRate(void)
{
	char buf[256];
	static int avg_time_per_frame = 0;

	// every 25 frames
	our_count++;
	if (our_count >= 30) // every 0.5 seconds on vsync pc
	{
		// calculate time passed
		float seconds = timer_run( &our_timer );

		// first time running so bank
		if ( seconds != 0.0F )
		{
			// calculate average frames per second
			FPS = our_count / seconds;

			// average time per frame in milliseconds
			avg_time_per_frame = (int)((1.0F / FPS) * 1000.0F);

			//
			our_count = 0;
		}
	}

	// display the framerate
	if( ShowFrameRate )
	{
		sprintf(&buf[0], "FPS %d - AVG F %d MS", (int) FPS, avg_time_per_frame );
		CenterPrint4x5Text( (char *) &buf[0] , FontHeight, 2 );
	}

	if( ShowInfo )
	{

		// group information
		sprintf(&buf[0], "Groups Visible %d - Current Group %s", (int) NumGroupsVisible,
			(GroupImIn == INDEX_NONE) ? "(outside)" : Mloadheader.Group[GroupImIn].name );
		CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*3, 2 );

		// memory information
		sprintf(&buf[0], "Mem %d",(int)MemUsed );
		CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*4, 2 );

		// show polygon information
		sprintf(&buf[0], "Face Me Polys %d - Dynamic? Polys %d - Screen Polys %d - Verts Touched (lighting?) %d",
			(int) TotalFmPolysInUse,(int) TotalPolysInUse,(int) TotalScrPolysInUse, NumOfVertsTouched);
		CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*6, 2 );

		if ( ! ShowWeaponKills ) //ShowNetworkInfo)
		{

			// newtork info
			sprintf( &buf[0], "Network" );
			CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*8, 2 );

			sprintf( &buf[0], "BPS %5d CurSent %5d MaxRec %5d MaxSent %5d",
				(int)CurrentBytesPerSecRec , (int)CurrentBytesPerSecSent , (int)MaxCurrentBytesPerSecRec , (int)MaxCurrentBytesPerSecSent );
			CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*9, 2 );

			sprintf( &buf[0], "ReceiveSize %4d MaxReceiveSize %4d", (int)RecPacketSize , (int)MaxRecPacketSize );
			CenterPrint4x5Text( (char *) &buf[0], (FontHeight+3)*10, 2 );

		}

		if (ShowWeaponKills)
		{
			// show all primary weapon kills
			for( i = 0 ; i < MAXPRIMARYWEAPONS+1 ; i++ )
			{
				// display primary weapon name
				Print4x5Text( GetWeaponName(WEPTYPE_Primary,i),	(render_info.window_size.cx>>1)-(11*FontWidth),	(viewport.Y + (viewport.Height>>2))+( i * ( FontHeight+(FontHeight>>1) ) ), 2 );
				// display primary weapon kills
				Printu_int16_t( GetWeaponKillStats(WhoIAm,WEPTYPE_Primary,i),	(render_info.window_size.cx>>1)-(15*FontWidth), (viewport.Y + (viewport.Height>>2))+( i * ( FontHeight+(FontHeight>>1) ) ), 2 );
			}

			// show all secondary weapon kills
			for( i = 0 ; i < TOTALSECONDARYWEAPONS ; i++ )
			{
				// display secondary weapon name
				Print4x5Text( GetWeaponName(WEPTYPE_Secondary,i),	(render_info.window_size.cx>>1)+(5*FontWidth),	(viewport.Y + (viewport.Height>>2))+( i * ( FontHeight+(FontHeight>>1) ) ), 2 );
				// display secondary weapon kills
				Printu_int16_t( GetWeaponKillStats(WhoIAm,WEPTYPE_Secondary,i) , (render_info.window_size.cx>>1)+(1*FontWidth),	(viewport.Y + (viewport.Height>>2))+( i * ( FontHeight+(FontHeight>>1) ) ), 2 );
			}
		}

	}


	return true;
}

/*===================================================================
  Procedure : Init View port without calling initview..
  Input   : Nothing
  Output    : Nothing
===================================================================*/
bool
InitViewport( void )
{
	int left, top;
	int width, height;
	int maxwidth, maxheight;
	float scale = 1.1F;

	/*
	 * Setup the viewport for specified viewing area
	 */

	if (!FSGetViewPort(&viewport))
	{
		Msg( "GetViewport failed.\n%s", render_error_description(0) );
		return false;
	}

	maxwidth = render_info.window_size.cx;
	maxheight = render_info.window_size.cy;

	width = maxwidth;
	height = maxheight;
	if ( scale > 1.11F )
	{
		DrawPanel = true;
		scale = 1.2F;
	}
	else
	{
		DrawPanel = false;
	}
	left = 0;
	top = 0;

	viewport.X = left;
	viewport.Y = top;
	viewport.Width = width;
	viewport.Height = height;
	viewport.ScaleX = viewport.Width / (float)2.0;
	viewport.ScaleY = viewport.Height / (float)2.0;

	if (!FSSetViewPort(&viewport))
	{
#ifdef DEBUG_VIEWPORT
		SetViewportError( "InitViewport", &viewport );
#else
		Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
		return false;
	}

	SetFOV( hfov );

	clear_black();

	return true;
}

/*===================================================================
  Procedure :   Special Destroy game for dplay fuck up..
  Input   :   nothing..
  Output    :   nothing..
===================================================================*/
void SpecialDestroyGame( void )
{
	IMustQuit = false;

	DebugPrintf("SpecialDestroyGame()\n");

	switch( MyGameStatus )
	{
	case STATUS_WaitingToStartTeamGame:
	case STATUS_StartingMultiplayer:
	case STATUS_GetPlayerNum:
		MyGameStatus = STATUS_Title;
		MenuRestart( &MENU_ForceAbort );
		break;
	default:
		MyGameStatus = STATUS_QuitCurrentGame;
		break;
	}

	network_cleanup();
}
/*===================================================================
  Procedure :   Calculate the framelag..
  Input   :   nothing..
  Output    :   nothing..
===================================================================*/
void CalculateFramelag( void )
{

	// forsaken multiplies seconds * 71.0F
	// the idea of multiplying by anything here would be a base conversion
	// such as from seconds into ms or ns to instead of a long fraction

	// but 71.0F is not a power of 10 so this has really bad affects
	// you can end up with exponential varying results on diff systems

	// one loop sample on a 2GHZ P4 with 768MB of ram and an nivdia vanta
	// 0.00744508 * 71  = 0.52860068

	// lets say that computer B taks 50% longer to compute the game loop
	// 0.00744508 * 1.5 = 0.01116762

	// that means the calculation look like the following for computer B
	// 0.01116762 * 71  = 0.80406864

	// now our game loop timing is being tweaked differently on both pc's
	// the problem is where our game loop has a real difference of:
	// 0.00372254 seconds 1.5% apart

	// they are now this far apart
	// 0.27546796 seconds 1.5211267605633802816901408450704%

	// resulting in a drift per loop of
	// 0.27546796 - 0.00372254 = 0.27174542 seconds

	// now to get the number of loops in 1 second on the slower pc
	// 1 / 0.01116762 = 89.544594103309389108870108402686

	// the slower pc is adding the drift 89.5 times per second
	// 89.544594103309389108870108402686 * 0.27174542 = 24.333333333333333333333333333332

	// thus  internal clock between the two pc's will be altered by 24.3
	// with the slower pc being *ahead* of the faster one in game time

	// not sure on the meaning of the math here but it looks like
	// this should represent a change of +0.18th of a second
	// 24.3 * 0.007 = 0.18225

	// on top of all this fact is that this is just a single sample
	// pc's could perhaps very more than 1.5% at fluctuating rates
	// until we have some real data from all our pc's we won't really know

	// forsaken uses framelag (fraction) to multiply by other numbers
	// so the faster pc with a smaller fraction gets smaller results
	// meaning that a faster pc will evolve the world at smaller steps
	// and the slower pc you should see the world moving at faster steps

	// now this is only relavant to places that actually use this variable
	// but it could have impact on various time based world updates such as:
	// packes per second, pickup rotation, mine movement/shooting,
	// bikes speed, input reading, lighting etc...

	// thus all of the above would work faster on a slower pc...
	// or perhaps even that missiles could fly *slower* on faster machines!!!!

	// note that player positions are not affected
	// since they are constantly updated via networking
	// and are not time based at all....

	while( !(real_framelag = timer_run( &framelag_timer )) )
	{
		SDL_Delay(10);
	}

	framelag = real_framelag * 71.0F;


	//  who knows why
	if( CurrentMenu && (MyGameStatus == STATUS_SinglePlayer) )
		framelag = 0.0F;

}


/*===================================================================
  Procedure : Disp Tracker
  Output    : bool        true/false
===================================================================*/
bool DispTracker( void )
{
	u_int16_t      i;
	VECTOR      TempVector;
	MATRIX      TempMatrix;
	float       TargetDistance;
	float       Red, Green, Blue, Trans;
	VECTOR      ShipDir, TargetDir;
	float       Cos;

	MatrixTranspose( &Ships[ WhoIAm ].Object.FinalMat, &TempMatrix );
	ShadeModel( MODEL_Tracker, &TempMatrix, ( 40.0F * GLOBAL_SCALE ), ( 220.0F * GLOBAL_SCALE ) );

	TempMatrix = MATRIX_Identity;
	TempMatrix._11 = 1.25F;
	TempMatrix._22 = 1.25F;
	TempMatrix._33 = 1.25F;
	MatrixMultiply( &TempMatrix, &Ships[ WhoIAm ].Object.FinalMat, &TempMatrix );

	view._11 = TempMatrix._11;
	view._12 = TempMatrix._12;
	view._13 = TempMatrix._13;
	view._14 = TempMatrix._14;

	view._21 = TempMatrix._21;
	view._22 = TempMatrix._22;
	view._23 = TempMatrix._23;
	view._24 = TempMatrix._24;

	view._31 = TempMatrix._31;
	view._32 = TempMatrix._32;
	view._33 = TempMatrix._33;
	view._34 = TempMatrix._34;

	view._41 = 0.0F;
	view._42 = 0.0F;
	view._43 = ( 280.0F * GLOBAL_SCALE );
	view._44 = 1.0F;

	if (!FSSetView(&view))
		return false;

	if (ReallyExecuteMxloadHeader( &ModelHeaders[MODEL_Tracker], INDEX_NONE ) != true )
		return false;

	i = FindClosestShip();

	if( i != INDEX_NONE )
	{
		Red = 255.0F;
		Green = 255.0F;
		Blue = 255.0F;
		Trans = 255.0F;
		TempVector.x = ( ( Ships[ i ].Object.Pos.x - Ships[ WhoIAm ].Object.Pos.x ) / 50.0F );
		TempVector.y = ( ( Ships[ i ].Object.Pos.y - Ships[ WhoIAm ].Object.Pos.y ) / 50.0F );
		TempVector.z = ( ( Ships[ i ].Object.Pos.z - Ships[ WhoIAm ].Object.Pos.z ) / 50.0F );
		TargetDistance = VectorLength( &TempVector );

		if( TargetDistance > ( 130.0F * GLOBAL_SCALE ) )
		{
			NormaliseVector( &TempVector );
			TempVector.x *= ( 130.0F * GLOBAL_SCALE );
			TempVector.y *= ( 130.0F * GLOBAL_SCALE );
			TempVector.z *= ( 130.0F * GLOBAL_SCALE );
			Green = 64.0F;
			Blue = 64.0F;
		}
		else
		{
			ApplyMatrix( &Ships[ WhoIAm ].Object.FinalMat, &Forward, &ShipDir );
			NormaliseVector( &ShipDir );
			TargetDir = TempVector;
			NormaliseVector( &TargetDir );
			Cos = DotProduct( &ShipDir, &TargetDir );
			if( Cos < 0.0F )
			{
				Red = 256.0F + ( Cos * 192.0F );
				Green = 256.0F + ( Cos * 192.0F );
			}
		}

		TintModel( MODEL_Ping, Red, Green, Blue, Trans );

		MatrixTranspose( &Ships[ WhoIAm ].Object.FinalMat, &TempMatrix );
		ApplyMatrix( &TempMatrix, &TempVector, &TempVector ); // Calc Up Vector

		view = identity;
		view._11 = 1.25F;
		view._22 = 1.25F;
		view._33 = 1.25F;
		view._41 = TempVector.x;
		view._42 = TempVector.y;
		view._43 = TempVector.z + ( 280.0F * GLOBAL_SCALE );
		view._44 = 1.0F;

		if (!FSSetView(&view))
			return false;

		if (ReallyExecuteMxloadHeader( &ModelHeaders[MODEL_Ping], INDEX_NONE ) != true )
			return false;
	}

	return true;
}


/*===================================================================
  Procedure : Change Background Colour
  Input   : float R ( 0.0F - 1.0F )
		: float G ( 0.0F - 1.0F )
		: float B ( 0.0F - 1.0F )
  Output    : bool  true/false
===================================================================*/
// TODO - might want this

/*===================================================================
  Procedure : Print up how far the init view has to go...
  Input   : BYTE GameStatus
  Output    : void
===================================================================*/
void PrintInitViewStatus( BYTE Status )
{
	int i;
	RENDEROBJECT ro;
	TEXTUREGROUP ro_groups[MAX_TEXTURE_GROUPS];
	ZERO_STACK_MEM(ro);
	ro.textureGroups = ro_groups;
	{
		bool _vok = FSCreateDynamic2dVertexBuffer(&ro, BATCH_BUFFER_VERTS);
		bool _iok = FSCreateIndexBuffer(&ro, BATCH_BUFFER_VERTS * 3);
		/* If either allocation failed (linear heap exhausted/fragmented),
		 * skip the draw rather than feeding NULL buffers into the renderer. */
		if (!_vok || !_iok) {
			FSReleaseRenderObject(&ro);
			render_flip(&render_info);
			return;
		}
	}
	for( i = 0 ; i < ( Status - STATUS_InitView_0 )+1 ; i ++ )
		CenterPrint4x5Text(
			InitViewMessages[i],
			( render_info.window_size.cy >> 2 ) +
			( i * ( FontHeight + ( FontHeight>>1 ) ) ) ,
			GREEN );

#ifdef __3DS__
	/* Stale screen-poly safety, ALWAYS-ON.
	 *
	 * `PrintInitViewStatus` is called on every IV phase during a level
	 * load to render the "Loading: …" status text. The text is added via
	 * `CenterPrint4x5Text` which calls `AddScrPolyToTPage` — those polys
	 * accumulate in `ScrPolys[]` across the whole load, plus whatever
	 * the title screen / inter-level menu added before that. By the
	 * final PIVS call (called with `Status = STATUS_PostStartingSinglePlayer`
	 * = 33), the for-loop above adds zero new text (it gates on
	 * `Status >= STATUS_InitView_0 = 128`), but `DisplayNonSolidScrPolys`
	 * still walks the full accumulated list and renders every poly.
	 *
	 * The polys reference texture data via `Frm_Info` pointers. When
	 * `ChangeLevel` swapped from title `Tloadheader` to gameplay, the
	 * title-side textures were freed — leaving stale `Frm_Info` pointers
	 * in the still-live ScrPolys entries. Walking them dereferences
	 * freed memory: data abort.
	 *
	 * Triggered first on fishy (level 30, 86 BSP groups) — possibly
	 * because its much larger Tloadheader caused different malloc
	 * arena state that made the freed pointers consistently invalid,
	 * whereas earlier levels happened to land on still-mapped
	 * (but logically-stale) memory.
	 *
	 * Fix: clear the ScrPolys list at status=33 entry. Status messages
	 * from earlier IV phases were already rendered + flipped in their
	 * own PIVS calls, so clearing now is purely losing the redundant
	 * post-load redraw. The title-screen state is also cleaned up
	 * naturally because every ScrPoly was added by code that's done
	 * with it now.
	 *
	 * The narrow earlier check ("FirstScrPolyUsed empty but a TPage
	 * has polys") was a different inconsistency — that one matters too
	 * but it's a subset of this broader cleanup. Calling InitScrPolys
	 * always at status=33 covers both. */
	if (Status == 33) {

		InitScrPolys();
	}
#endif
	DisplayNonSolidScrPolys(&ro);
	DisplaySolidScrPolys(&ro);
	FSReleaseRenderObject(&ro);
	render_flip(&render_info);
}

/*===================================================================
  Procedure :   Init Stats Display Stuff...
  Input   :   nothing...
  Output    :   bool true/false
===================================================================*/
bool  InitStatsDisplay()
{
	// Create the offscreen surface, by loading our bitmap.

	InitFont();

	HowManySecrets( &TotalSecrets , &Secrets );

	return true;
}

// ModeScaleX use to have hard coded spacing value and such for fonts etc...
// now we just use index 0 and set it dynamically to whatever screen size we are at

void InitModeCase(void)
{
	ModeScaleX = (float)render_info.window_size.cx / 320.0F;
	ModeScaleY = (float)render_info.window_size.cy / 200.0F;
}


/*===================================================================
  Procedure :   Save SnapShot screen for saved game
  Input   :   int8_t  * Filename
  Output    :   bool    True/False
===================================================================*/
bool SaveSnapShot( int8_t * /*Filename*/ )
{
	return true;
}

// if we met the max kills limit then set flag to change level
void CheckMetKillLimit()
{
	bool	KillsReached = false;
	bool teamOK[MAX_TEAMS];
	bool teamleft[MAX_TEAMS];

	// i'm the host so i should check if we have met the level kill threshold if there is one
	if(IsHost && MaxKills && (MyGameStatus == STATUS_Normal) )
	{
		// not team game
		if( !TeamGame )
		{
			// for every player
			for ( i = 0; i < MAX_PLAYERS; i++ )
			{
				// whose status is normal
				if( (GameStatus[GetPlayerByRank(i)] == STATUS_Normal) )
				{
					// if they scored more than the kills threshold then flag we finished it
					if( GetScoreStats(GetPlayerByRank(i)) >= MaxKills && LevelNum != -1 )
						KillsReached = true;
				}
			}
		}
		// team game
		else
		{
			// identify which teams are active
			for (i = 0; i < MAX_TEAMS; i++)
			{
				teamOK[i] = false;
				teamleft[i] = true;
			}

			// if any of the active teams scored more than the kills threshold then flag we finished it
			for( i = 0 ; i < MAX_TEAMS ; i++ )
			{
				if( GetTeamScore(i) >= MaxKills && !teamleft[i] && teamOK[i])
					KillsReached = true;
			}
		}

		// kill threshold reached so change level
		if( KillsReached )
		{
			LevelNum = -1;
			NewLevelNum++;
			if( NewLevelNum >= NumLevels ) NewLevelNum = 0;
			NetworkGameUpdate();
		}
	}

}

