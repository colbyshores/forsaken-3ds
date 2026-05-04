#define INSIDE_BSP // disable to use bounding box inside check instead
#define BSP_ONLY

#include "main.h"
#include "platform.h"
#ifdef __3DS__
#include <GL/gl.h>
#endif
#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "node.h"
#include "networking.h"
#include "2dtextures.h"
#include "mload.h"
#include "primary.h"
#include "secondary.h"
#include "triggers.h"
#include "pickups.h"
#include "ships.h"
#include "collision.h"
#include "2dpolys.h"
#include "lines.h"
#include "polys.h"
#include "lights.h"
#include "models.h"
#include "2dtextures.h"
#include "visi.h"
#include "tload.h"
#include "sfx.h"
#include "transexe.h"
#include "text.h"
#include "mxaload.h"
#include "camera.h"
#include "title.h"
#include "screenpolys.h"
#include "controls.h"
#include "net.h"
#include "triggers.h"
#include "enemies.h"
#include "bsp.h"
#include "trigarea.h"
#include "multiplayer.h"
#include "file.h"
#include "ai.h"
#include "water.h"
#include "spotfx.h"
#include "extforce.h"
#include "teleport.h"
#include "rtlight.h"
#include "restart.h"
#include "local.h"
#include "goal.h"
#include "loadsave.h"
#include "stats.h"
#include "timer.h"
#include "demo.h"
#include "file.h"
#include "singleplayer.h"
#include "render.h"
#include "input.h"
#include "oct2.h"
#ifdef __3DS__
#include "music.h"
#endif

#ifdef SHADOWTEST
#include "triangles.h"
#include "shadows.h"
#endif

void SetCam(int ship, int Cam);

VECTOR OldPos;
float SpeedInterval = 0.0F;
float BikeSpeed = 0.0F;

extern SHIPHEALTHMSG PlayerHealths[ MAX_PLAYERS+1 ];
extern u_int8_t ShipHealthColour[ MAX_PLAYERS+1 ];
extern int HUDColour;
extern bool ShowWeaponsPossessedOnHUD;
extern bool ShowClockOnHUD;
extern bool ShowKPMOnHUD;
extern bool ShowSpeedOnHUD;

extern render_info_t render_info;
extern bool Bsp_Duplicate( BSP_HEADER *src, BSP_HEADER *dup );
extern bool Bsp_Identical( BSP_HEADER *b1, BSP_HEADER *b2 );
BSP_HEADER Bsp_Original;

extern	render_viewport_t viewport;
extern VECTOR View, Look, Up, PanFrom, PanTo, PanFrom2, PanTo2, VDULookPos, DiscLookPos, DiscViewPos, VDUViewPos, StartLookPos, StartViewPos;
extern	RENDERMATRIX view;
extern int    BountyInLevel;
extern int    FlagsInLevel;
extern int    TeamFlagsInLevel[ MAX_TEAMS ];
extern int    TeamFlagsInShips[ MAX_TEAMS ];

extern void input_grab( bool clip );
extern bool NeedFlagAtHome;
extern bool OwnFlagTeleportsHome;
extern bool CanCarryOwnFlag;

bool OriginalLevels = false;
bool CheatsDisabled = false;
bool WaitingToQuit;

void ConfigureSpaceorbAxis( int joystick );
void DefaultJoystickSettings( USERCONFIG *u );

extern const char last_compiled[];

extern  u_int32_t RecPacketSize;
extern  u_int32_t MaxRecPacketSize;

extern  u_int32_t CurrentBytesPerSecRec;
extern  u_int32_t CurrentBytesPerSecSent;
extern  u_int32_t MaxCurrentBytesPerSecRec;
extern  u_int32_t MaxCurrentBytesPerSecSent;


extern char *JoystickPOVDirections[];
extern VECTOR BikePos;

extern bool CTF;

extern u_int8_t QuickStart;

extern char *TeamName[ MAX_TEAMS ];
extern int16_t DummyTextureIndex;

extern void Printint16_t( int16_t num , int x , int y , int col );

extern void InitIndirectVisible( u_int16_t g );

extern void AddIndirectVisible( u_int16_t g );
extern  bool  RearCameraDisable;
extern  MODELNAME   SplashModelNames[MAXMODELHEADERS];
extern USERCONFIG *player_config;
extern char biker_config[];

extern MODELNAME *TitleModelSet;
extern float LevelTimeTaken;

extern BYTE PreDemoEndMyGameStatus;
extern  TEXT  DemoGameName;

extern  bool FullRearView;
extern  bool ShowStats;

extern  int AckMsgsActiveMax;
extern  int AckMsgsActive;

extern MENU MENU_Controls;
extern MENU MENU_Options;
extern MENU MENU_LoadSavedGame;
extern MENU MENU_SaveGame;
extern MENU MENU_NEW_BetweenLevels;
extern MENU MENU_NEW_BetweenLevels;
extern MENU MENU_NEW_NumberOfCrystals;
extern MENU MENU_NEW_WatchTeamSelect;

extern float VDUoffsetX;
extern float VDUoffsetY;
extern  int16_t NextworkOldBikeNum;

extern  size_t  MemUsed;

#ifdef DEBUG_ON
extern u_int32_t CurrentBikeCompSpeech;
#endif

bool Cheated = false;

void InitSoundInfo( MLOADHEADER * Mloadheader );
void InitShipSpeeds( void );

#define SAVESCREEN_3DFX

u_int32_t        AnimOncePerFrame = 0;         // used for stuff that is displayed more than once in a single frame..

extern bool bSoundEnabled;

extern int	TeamCol[ MAX_TEAMS ];
extern int Num_StatsMessage_Parts;
extern LIST BikeList;
extern LIST LevelList;

extern  bool ResetKillsPerLevel;

extern int  outside_map;

extern  int16_t NextNewModel;
extern MENUSTATE MenuState;
extern  bool  ShowEFZones;
extern  bool  ShowTeleports;
extern bool MenuFrozen;
extern  int16_t MaxKills;
extern  int16_t NumGoldBars;
extern bool ReloadSfx;
extern float ReloadSfxPause;
extern  bool  IllegalTime;
extern  int   SinglePlayerTimeLimit;
extern  bool  BilinearSolidScrPolys;
extern  u_int16_t  RandomStartPosModify;
extern  char * StatusTab[256];

extern bool ShowMessages; // show long list of chat messages
extern bool ShowStatistics; // show in-game statistics
extern bool ShowNetworkInfo; // show in-game enet info

BYTE  PreSynchupStatus;
char *CurrentLevelsList;

px_timer_t level_time;

void InitModeCase(void);
void DebugState( const char * str );


// Ai Modes
char * AiModes[] = {
  "AIMODE_PLAYER",          // Controlled by PLAYER
  "AIMODE_FOLLOWPATH",        // Follow node path
  "AIMODE_IDLE",          // Do nothing for a while
  "AIMODE_SCAN",          // Look around for a target
  "AIMODE_MOVETOTARGET",      // Move towards selected target   
  "AIMODE_FIRE",      // Move towards selected target   
  "AIMODE_DOGFIGHT",        // Dogfight with the selected target   
  "AIMODE_KILLMINE",        // Attack And kill a Mine....
  "AIMODE_RETREAT",         // Try and get away from PLAYER
  "AIMODE_FORMATION",       // Stay in Formation and attack whan necessary...

  "AIMODE_DEATH_CRASHNBURN",    // Fall to the ground and Shatter...
  "AIMODE_DEATH_PINGOFF",     // Ping off in the dir of the bullet that killed me spinning as I go...
  "AIMODE_DEATH_PSYCHO",      // Twist and turn and shoot and explode...
  "",
  "",
  "",
};

extern  int EnemiesActive;
void CheckTimeLimit( void );
extern  int16_t InGameLoadGameLevelNum;

bool  ScoreDisplaySfx = true;
bool  IMustQuit = false;
// Capture The Flag Stuff
bool  CaptureTheFlag	= false;
bool  IHaveAFlag			= false;
// End of Capture The Flag

// Bounty Hunt
bool  BountyHunt = false;

extern int CameraStatus;  

char *  InitViewMessages[] = {
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

extern  float MaxMoveSpeed;
extern  float MoveAccell;
extern  float MoveDecell;
extern  float MaxTurboSpeed;
extern  float TurboAccell;
extern  float TurboDecell;
extern  float MaxTurnSpeed;
extern  float TurnAccell;
extern  float TurnDecell;
extern  float MaxRollSpeed;
extern  float RollAccell;
extern  float RollDecell;
extern  float MaxBankAngle;
extern  float BankAccell;
extern  float BankDecell;

extern bool BrightShips;

extern  float LastDistance[MAX_SFX];

extern int GameCompleted;
extern  OFF_FILES OffsetFiles[];
extern  OFF_FILES Title_OffsetFiles[];
extern  OFF_FILES Splash_OffsetFiles[];

extern  int   DifficultyLevel;

extern int outside_group;

extern  bool  ScreenSaving;
extern  float WhiteOut;
extern float mouse_dx;
extern float mouse_dy;

void	MessageQuePrintAll( void );

bool  InsideResizeViewport = false;

DWORD CurrentSrcBlend;
DWORD CurrentDestBlend;
DWORD CurrentTextureBlend;
 
#ifdef DEMO_SUPPORT
LONGLONG  GameStartedTime;    // when the game started
LONGLONG  GameElapsedTime;    // Real how long the game has been going in game time not real..
LONGLONG  TempGameElapsedTime;  // Real how long the game has been going in game time not real..
LONGLONG  GameCurrentTime;    // How long the game has been going...
LONGLONG  TimeDiff;
LONGLONG  Freq;
#endif

bool  JustExitedMenu =false;

bool  Inside;
int   Depth;

void CalculateFramelag( void );

MENU  MENU_EditMacro1;
MENU  MENU_EditMacro2;
MENU  MENU_EditMacro3;

extern  FILE  * DemoFp;
extern  FILE  * DemoFpClean;
extern  bool  PlayDemo;
extern  bool  PauseDemo;
extern  bool  RecordDemo;
extern  SLIDER  DemoSpeed;
extern  SLIDER  DemoEyesSelect;
extern  bool  ShowWeaponKills;
extern  bool ShowStats; 

extern  u_int16_t  IsGroupVisible[MAXGROUPS];

extern  MXLOADHEADER *ModelHeaders;
extern  MODELNAME TitleModelNames[MAXMODELHEADERS]; 

extern  char  TitleNames[8][64];  

extern  ENEMY * FirstEnemyUsed;

extern  CAMERA  CurrentCamera;
extern  CAMERA  MainCamera;     // the main viewing screen...

float FPS = 0.0F;         // Frames Per Second...
//double  TPS = 0.0;          // Textures Per Second...     

#define MYTIMER
#undef MYTIMER

#define LEVELSFILE      "levels.dat"

extern  MENU  *       GetPlayerNumMenu;


#define FOV_GROW(A)     ((A) + 10.0F)
#define FOV_SHRINK(A)   ((A) - 10.0F)

#ifdef DEMO_SUPPORT
LONGLONG  DemoStartedTime;    // when the game started
LONGLONG  DemoEndedTime;      // when the game started
float   DemoTotalTime = 0.0F; // total game time (in seconds)
int32_t   DemoGameLoops = 0;
float DemoAvgFps = 0.0F;
extern  LONGLONG  DemoTimeSoFar;
#endif

#define MIN_VIEWPORT_WIDTH  (64)
#define MIN_VIEWPORT_HEIGHT (64)

#define VIEWPORT_GROW   (1.1F)
#define VIEWPORT_SHRINK   (1.0F/1.1F)

extern  TLOADHEADER Tloadheader;
extern  int16_t   SecondaryAmmo[ MAXSECONDARYWEAPONS ];
bool LockOutWindows = true;

extern u_int16_t OnceOnly;
extern char         MyName[ 32 ];
extern  bool                    IsHost;
extern float  MessageTime[MAX_MESSAGES];
extern  char MessageBank[MAX_MESSAGES][200];
extern  int CurrentMessage;
extern  BYTE          MyGameStatus;
BYTE PreWaitingToSendMessagesStatus;
extern  BYTE          GameStatus[]; // Game Status for every Ship...
extern  BYTE          OverallGameStatus;      // what the host says he is doing...
//extern  LPDIRECT3DEXECUTEBUFFER Skin_Execs[ MAXGROUPS ];
//extern  LPDIRECT3DEXECUTEBUFFER Portal_Execs[ MAXGROUPS ];
extern RENDEROBJECT Skin_Execs[ MAXGROUPS ];
extern RENDEROBJECT	Portal_Execs[ MAXGROUPS ];
extern  float PyroliteAmmo;
extern  float SussGunAmmo;
extern  float GeneralAmmo;
extern  SHIPCONTROL control;
extern  REGENPOINT  * RegenPoints;
extern  int16_t     NumRegenPoints;
extern  float PowerLevel;
extern  float LaserTemperature;
extern  float NitroFuel;
extern  SECONDARYWEAPONBULLET SecBulls[MAXSECONDARYWEAPONBULLETS];
extern  ENEMY * TestEnemy;

extern  char  biker_name[256];
extern  int16_t SelectedBike;

extern  float cral;
extern  int   HullHit;
extern  int   ShieldHit;
extern  bool  TargetComputerOn;

extern  int16_t NumPickupsPerGroup[ MAXGROUPS ];
extern  PICKUP  Pickups[ MAXPICKUPS ];

extern  bool  DebugInfo;
extern  bool  GodMode;
extern  bool  GodModeOnceOnly;

extern  bool  TeamGame;
extern  BYTE  TeamNumber[MAX_PLAYERS];

extern  int   no_collision;   // disables player ship-to-background collisions
extern  SLIDER  TimeLimit;
extern  bool  CountDownOn;
extern  REGENPOINT  * RegenSlotsCopy[ MAX_PLAYERS ];

extern  bool  ShowTrigZones;
extern  bool  ShowColZones;
extern  bool  NodeCube;
extern  bool  OldNodeCube;
extern  MATRIX  MATRIX_Identity;

extern  u_int32_t  TotalFmPolysInUse;
extern  u_int32_t  TotalPolysInUse;
extern  u_int32_t  TotalScrPolysInUse;
extern  MODELNAME ModelNames[MAXMODELHEADERS];
extern  bool  quitting; 
extern  bool  ShowBoundingBoxes;

bool InitViewport( void );


BYTE  InitView_MyGameStatus;
BYTE  ChangeLevel_MyGameStatus;

#ifdef __3DS__
/* One-shot flag: cleared on the FIRST STATUS_SinglePlayer frame to suppress
 * a spurious SDLK_ESCAPE that can be injected by handle_events() between
 * STATUS_PostStartingSinglePlayer and STATUS_SinglePlayer, which would
 * otherwise open MENU_InGameSingle on the first gameplay frame. */
static bool _gameplay_start_guard = false;

/* Context string for MenuRestart caller tracking — set at key call sites
 * so the BLOCKED trace in MenuRestart can identify which function called it. */
const char *_3ds_mr_ctx = "startup";
#endif

//LPDIRECTDRAWPALETTE ddpal;

void ProcessGameKeys( void );


float GetPlayerNumCount1 = 0.0F;
float GetPlayerNumCount2 = 0.0F;
int   GetPlayerNumCount = 0;

px_timer_t framelag_timer;

int i, j; // index counters

bool RearCameraActive;
int MissileCameraActive = 0;
u_int16_t  CameraMissile = (u_int16_t) -1;
extern bool MissileCameraEnable;
BYTE  TempMissileCam;

extern  REMOTECAMERA * ActiveRemoteCamera;

bool
Our_CalculateFrameRate(void);
bool VduFinished( MENU *Menu );
bool WriteMessage(const char *format, ...); // printf-a-like for bottom line of window

bool ScoreDisplay();
bool InitScoreDisplay();
bool StatsDisplay();
bool InitStatsDisplay();
bool ScoreDisplayOrig(void);

typedef enum {
	NO_BG,
	FULL_BG,
	BOX_BG
} stats_mode_t;

void ShowDetailedStats(int NumActivePlayers, bool TeamsGame, bool KillsBased, bool DetailedStats);
void ShowBasicStats(int NumActivePlayers);
void ShowInGameStats();
void ShowDeathModeStats();
void ShowGameStats(stats_mode_t mode);
void DisplayNetworkInfo();

int Secrets = 0;
int TotalSecrets = 0;
int CrystalsFound = 0;
extern  int16_t   NumInitEnemies;
extern  int16_t   NumKilledEnemies;

bool  ResizeViewport( void );
bool  FullScreenViewport();
bool  ClearBuffers( void );
bool  ClearZBuffer( void );

bool clear_black(void)
{
  if ( WhiteOut > 0.0f )
    return true;
	return FSClearBlack();
}

bool RenderCurrentCamera( void );

void  PlotSimplePanel( void );

render_viewport_t viewport;
render_viewport_t oldviewport;
int initfov = 0;
float viewplane_distance;
float hfov = START_FOV;
float chosen_fov = START_FOV;
float normal_fov = START_FOV;

bool  DrawPanel = false;
bool  ReMakeSimplePanel = true;

bool  OldDrawPanel = true;
bool  Panel = true;

bool ChangeLevel( void );
void SelectQuitCurrentGame( MENUITEM *Item );

float pixel_aspect_ratio;
float Oldframelag;  
float framelag = 0.0F; 
float real_framelag = 0.0F;
float Demoframelag = 0.5F;

px_timer_t level_timer;

extern int FontWidth;
extern int FontHeight;
extern int FontSourceWidth;
extern int FontSourceHeight;

extern  int PlayerSort[MAX_PLAYERS];
extern int16_t NumOfActivePlayers;

int16_t   LevelNum = 0 ;
int16_t   NewLevelNum = 0 ;
int16_t   NumLevels = 0;
char  ShortLevelNames[MAXLEVELS][MAX_SHORT_LEVEL_NAME];
char  LevelNames[MAXLEVELS][128];
char  CollisionNames[MAXLEVELS][128];
char  CollisionZNames[MAXLEVELS][128];
char  BspNames[MAXLEVELS][128];
char  BspZNames[MAXLEVELS][128];
char  ZoneNames[MAXLEVELS][128];
char  WaterNames[MAXLEVELS][128];
char  TextNames[MAXLEVELS][128];
char  MsgNames[MAXLEVELS][128];
char  ExternalForceNames[MAXLEVELS][128];
char  TeleportNames[MAXLEVELS][128];

char MissionTextNames[MAXLEVELS][128];
char MissionTextPics[MAXLEVELS][128];
char level_names[MAXLEVELS][128];

int16_t CameraRendering = CAMRENDERING_None;

bool SeriousError = false;
bool DoClipping = true;
bool OnceOnlyChangeLevel = false;

//bjdLPDIRECT3DEXECUTEBUFFER RenderBufs[ 2 ] = { NULL, NULL };
RENDEROBJECT RenderBufs[4];

void InitRenderBufs(/*LPDIRECT3DDEVICE lpDev*/ ); // bjd
void ReleaseRenderBufs( void );
//bool ChangeBackgroundColour( float R, float G, float B );
bool SetMatrixViewPort( void );
void PrintInitViewStatus( BYTE Status );

void UpdateBGObjectsClipGroup( CAMERA * Camera );
void UpdateEnemiesClipGroup( CAMERA * Camera  );
void SpecialDestroyGame( void );

extern  int16_t PrimaryInLevel[ MAXPRIMARYWEAPONS ];
extern  int16_t SecondaryInLevel[ MAXSECONDARYWEAPONS ];
extern  int16_t PrimaryInPlayers[ MAXPRIMARYWEAPONS ];
extern  int16_t SecondaryInPlayers[ MAXSECONDARYWEAPONS ];
extern  int16_t PrimaryToGenerate[ MAXPRIMARYWEAPONS ];
extern  int16_t SecondaryToGenerate[ MAXSECONDARYWEAPONS ];
extern  int16_t MinesInLevel[ MAXSECONDARYWEAPONS ];
extern  int16_t OrbsInLevel;
extern  int16_t OrbsToGenerate;
extern  int16_t OrbsInPlayers;

extern int16_t PrimaryWeaponsGot[ MAXPRIMARYWEAPONS ];

// if met max kill limit, sets flag to change level
void CheckMetKillLimit();

// message colours (Title.c)
extern int KillMessageColour; 
extern int SystemMessageColour;
extern int FlagMessageColour;

// for HUD colour (text.c)
extern u_int8_t Colourtrans[MAXFONTCOLOURS][3];

// watch mode select player (Title.c)
extern SLIDER WatchPlayerSelect;

// (Ships.c)
extern char* GetName(int Player);

// (stats.c)
extern void ScoreSort();

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

//extern  LPDIRECT3DEXECUTEBUFFER Portal_lpD3DExBuf;

extern  u_int16_t      FirstFmPolyUsed;
extern  u_int16_t      FirstPolyUsed;
extern  u_int16_t      FirstLineUsed;
extern  u_int16_t      FirstScrPolyUsed;
extern  BYTE      WhoIAm;
extern  bool      RemoteCameraActive;

extern  int16_t     ShowPortal;
extern  int16_t     ShowSkin;
extern  int16_t         NamesAreLegal;
extern  SHORTNAMETYPE     Names;  // all the players short Names....

bool MainGame(); // bjd

void Build_View();
bool DispTracker( void ); // bjd


/*===================================================================
  Global Variables
===================================================================*/   
#define Near  ( 100.0F * GLOBAL_SCALE )
//#define Far   ( 65535.0F * GLOBAL_SCALE )
#define Far   ( 49152.0F * GLOBAL_SCALE )
#define MinNear ( 50.0F * GLOBAL_SCALE )

RENDERMATRIX proj = {
    RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(Far/(Far-Near)), RENDERVAL(1.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(-Far*Near/(Far-Near)), RENDERVAL(0.0) };

RENDERMATRIX panelproj = {
    RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(Far/(Far-Near)), RENDERVAL(1.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(-Far*Near/(Far-Near)), RENDERVAL(0.0) };

RENDERMATRIX trackerproj = {
    RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(Far/(Far-Near)), RENDERVAL(1.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(-Far*Near/(Far-Near)), RENDERVAL(0.0) };

MATRIX  ProjMatrix = {
    RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(2.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(Far/(Far-Near)), RENDERVAL(1.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(-Far*Near/(Far-Near)), RENDERVAL(0.0) };


  VECTOR  SlideLeft = { -1.0F, 0.0F, 0.0F };
  VECTOR  SlideRight  = { 1.0F, 0.0F, 0.0F };
  VECTOR  SlideUp   = { 0.0F, 1.0F, 0.0F };
  VECTOR  SlideDown = { 0.0F, -1.0F, 0.0F };
  VECTOR  Forward   = { 0.0F, 0.0F, 1.0F };
  VECTOR  Backward  = { 0.0F, 0.0F,-1.0F };
  VECTOR  NullVector  = { 0.0F, 0.0F, 0.0F };

  VECTOR  Camera_Dir = { 0.0F, 0.0F,0.0F };

/*===================================================================
===================================================================*/
//static D3DEXECUTEDATA d3dExData;
//static D3DEXECUTEBUFFERDESC debDesc;

/*
 * Global projection, view, world and identity matricies
 */

RENDERMATRIX view;
RENDERMATRIX identity = {
    RENDERVAL(1.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(1.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(1.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(1.0)
};

RENDERMATRIX world = {
    RENDERVAL(1.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(1.0), RENDERVAL(0.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(1.0), RENDERVAL(0.0),
    RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(0.0), RENDERVAL(1.0)
};

void stereo_adjust( RENDERMATRIX *m )
{
	float left, right, offset;

	if( render_info.stereo_position != ST_CENTER )
		offset = 0.5f * render_info.stereo_eye_sep / render_info.stereo_focal_dist;
	else
		return;

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
	viewplane_distance = (float) ( viewport.Width / ( 2 * tanf( DEG2RAD( fov ) * 0.5 ) ) );
	proj._11 = 2 * viewplane_distance / viewport.Width;
	proj._22 = 2 * viewplane_distance / ( viewport.Height / pixel_aspect_ratio );

	if( fov > START_FOV )
	{
		Scale = ( ( MAX_FOV - fov ) / ( MAX_FOV - START_FOV ) );
		NewNear = ( MinNear + ( ( Near - MinNear ) * Scale ) );
		proj._33 = RENDERVAL(Far/(Far-NewNear));
		proj._34 = RENDERVAL( 1.0 );
		proj._43 = RENDERVAL(-Far*NewNear/(Far-NewNear));
		proj._44 = RENDERVAL( 0.0 );
	}
	else
	{
		proj._33 = RENDERVAL(Far/(Far-Near));
		proj._34 = RENDERVAL( 1.0 );
		proj._43 = RENDERVAL(-Far*Near/(Far-Near));
		proj._44 = RENDERVAL( 0.0 );
	}

	stereo_adjust(&proj);

	/* D3D->OpenGL projection conversion is handled in FSSetProjection on 3DS */

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


void SetViewportError( char *where, render_viewport_t *vp )
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
    sizeof( *vp ),
    vp->X, vp->Y,
    vp->Width, vp->Height,
    vp->ScaleX, vp->ScaleY,
    //vp->dvMaxX, vp->dvMaxY,
    vp->MinZ, vp->MaxZ );
  Msg( msg );
}

// read KEYS.txt and DEUBG_KEYS.txt before changing.
void ProcessGameKeys( void )
{

  static int16_t startpos = 0;

  // if were not (!) in (normal,single,attract or demo) modes
  // then don't listen to keys...
  if ( ! (
           ( MyGameStatus == STATUS_Normal       ) ||
           ( MyGameStatus == STATUS_SinglePlayer ) ||
           ( MyGameStatus == STATUS_PlayingDemo  )
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
      // if were playing a demo
      if( PlayDemo )
			{
        // show the demo menu
        MenuRestart( &MENU_DemoPlaying );
			}
      // if were playing the game
      else
		  {
				// show the multi-player menu
				if(GameStatus[WhoIAm] != STATUS_PlayingDemo &&
					GameStatus[WhoIAm] != STATUS_SinglePlayer)
				{
					MenuRestart( &MENU_InGame );
				}
				// show the single-player menu
				else
				{
					 MenuRestart( &MENU_InGameSingle );
			  }
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

#ifdef POLYGONAL_COLLISIONS

      // Shift + F11
      if ( input_buffer_find( SDLK_F11 ) )
        if( ShowSkin == 6 )
          ShowSkin = 0;
        else
          ShowSkin++;

#endif

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

#ifndef POLYGONAL_COLLISIONS
#ifdef REMOTE_CAMERA_ENABLED
		// Ctrl + F3
		if ( input_buffer_find( SDLK_F3 ) )
			if( !RemoteCameraActive )
			{
				RemoteCameraActive = true;
				Ships[MAX_PLAYERS].Object.Pos = Ships[WhoIAm].Object.Pos;
				Ships[MAX_PLAYERS].Object.Group = Ships[WhoIAm].Object.Group;
				Ships[MAX_PLAYERS].Object.Mode = NORMAL_MODE;
				Ships[MAX_PLAYERS].enable = 1;
				Current_Camera_View = MAX_PLAYERS;    // which object is currently using the camera view....
			}else{
				RemoteCameraActive = false;
				Ships[MAX_PLAYERS].enable = 0;
				Current_Camera_View = WhoIAm;   // which object is currently using the camera view....
			}
#endif
#endif

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

	// multiplayer
	else
	{
		// F3
		// F4
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

bool
ResizeViewport( void )
{
	int left, top;
	int width, height;
	int maxwidth, maxheight;
	bool  NewDrawPanel;
	bool  NewDrawSimplePanel;
	float scale = 1.0F;
  
	InsideResizeViewport = true;

	/*
     * Setup the viewport for specified viewing area
     */

	if (!FSGetViewPort(&viewport))
	{
        Msg( "GetViewport failed.\n%s", render_error_description(0) );
        return false;
    }
	maxwidth = render_info.window_size.cx;

	NewDrawSimplePanel = false;
  
	maxheight = render_info.window_size.cy;

	if ( scale < 1.01F )
	{
		width = (int) floorf( maxwidth * scale );
		if ( width < MIN_VIEWPORT_WIDTH )
		{
			width = MIN_VIEWPORT_WIDTH;
			scale = (float) width / maxwidth;
		}
		height = (int) floorf( maxheight * scale );

		NewDrawPanel = false;

		left = ( ( maxwidth - width ) >> 1 ) & ~1;
		top = ( ( maxheight - height ) >> 1 ) & ~1;
	}
	else
	{
		width = maxwidth;
		height = maxheight;
		if ( scale > 1.11F )
		{
			NewDrawPanel = true;
			scale = 1.2F;
		}
		else
		{
			NewDrawPanel = false;
		}
		left = 0;
		top = 0;
	}

	// This disables the 3d panel...
	NewDrawPanel = DrawPanel;

	viewport.X = left;
	viewport.Y = top;
	viewport.Width = width;
	viewport.Height = height;
	viewport.ScaleX = viewport.Width / (float)2.0;
	viewport.ScaleY = viewport.Height / (float)2.0;

	if (!FSSetViewPort(&viewport))
	{
#ifdef DEBUG_VIEWPORT
    SetViewportError( "ResizeViewport", &viewport );
#else
        Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
        return false;
    }
  
	SetFOV( hfov );

	clear_black();

	return true;
}

bool FullScreenViewport()
{
	int left, top;
	int width, height;
	int maxwidth, maxheight;

	if (!FSGetViewPort(&viewport))
	{
        Msg( "GetViewport failed.\n%s", render_error_description(0) );
        return false;
    }

	maxwidth = render_info.window_size.cx;
	maxheight = render_info.window_size.cy;
	width = maxwidth;
	height = maxheight;
	left = 0;
	top = 0;

	viewport.X = left;
	viewport.Y = top;
    viewport.Width = width;
    viewport.Height = height;
    viewport.ScaleX = viewport.Width / (float)2.0;
    viewport.ScaleY = viewport.Height / (float)2.0;
/* bjd 
    viewport.dvMaxX = (float)D3DDivide(RENDERVAL(viewport.dwWidth),
                                       RENDERVAL(2 * viewport.dvScaleX));
    viewport.dvMaxY = (float)D3DDivide(RENDERVAL(viewport.dwHeight),
                                       RENDERVAL(2 * viewport.dvScaleY));
*/

	if (!FSSetViewPort(&viewport))
	{
#ifdef DEBUG_VIEWPORT
    SetViewportError( "FullScreenViewport", &viewport );
#else
        Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
        return false;
    }
	SetFOV( hfov );
	clear_black();
	return true;
}

bool InitLevels( char * levels_list )
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
  OriginalLevels = true;

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
	sprintf( BspZNames[ j ],			"data\\levels\\%s\\%sz.bsp",  ShortLevelNames[ j ], ShortLevelNames[ j ] );
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
  LoadLevelText( NULL );

  // success !
  return true;

}


bool SetMatrixViewPort( void )
{
/* TODO - what did this function do in d3d6 ?
	if (lpView->lpVtbl->SetBackground(lpView, hBmat) != D3D_OK) {
		return false;
	}
*/

#if 0 // bjd - TODO - CHECK
      STATE_DATA(D3DLIGHTSTATE_AMBIENT, RGBA_MAKE(255, 255, 255, 0), lpPointer);
#endif

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

float HealthCount = 0.0F;
int PowerSizes[6] = { 0 , 4 , 16 , 24 , 40 , 56 };
void DrawSimplePanel()
{
	int current = 0;
	int left  = render_info.window_size.cx - FontWidth*14;
	int right = render_info.window_size.cx - FontWidth*5;
	int top   = render_info.window_size.cy - FontHeight*30;
	int energy;
	u_int8_t Count; u_int8_t pos = 0;
 	u_int8_t r = Colourtrans[HUDColour][0];
	u_int8_t g = Colourtrans[HUDColour][1];
	u_int8_t b = Colourtrans[HUDColour][2];
    char MessageBuff[150];

	if( Panel && !PlayDemo )
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
#if defined(__3DS__) && defined(RENDERER_C3D)
				/* Crosshair/reticle is an in-world overlay tied to the 3D
				   camera viewport — it belongs on the stereo top screen, not
				   the mono bottom. Temporarily clear the bottom-HUD routing
				   so this AddScreenPolyText creates a top-screen poly. */
				extern bool ScrPolyGetRouteBottom(void);
				extern void ScrPolySetRouteBottom(bool b);
				bool _was_routing = ScrPolyGetRouteBottom();
				ScrPolySetRouteBottom(false);
#endif
				AddScreenPolyText(
					(u_int16_t) 63 ,
					(float) (viewport.X + (viewport.Width>>1)) ,
					(float) (viewport.Y + (viewport.Height>>1)) ,
					r, g, b, 255 );
#if defined(__3DS__) && defined(RENDERER_C3D)
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
	} // end of ( Panel && !PlayDemo )
	else
	{
		if( Panel )
		{
			ScoreSort();
			PrintScoreSort();

			if(ShowMessages) 
				MessageQuePrintAll();
			else
			{
				MessageQuePrint();
				PlayerMessageQuePrint();
			}

			if( DemoEyesSelect.value != MAX_PLAYERS )
			Print4x5Text( 
				Names[DemoEyesSelect.value],
				render_info.window_size.cx - (FontWidth*9), 
				FontHeight, 
				0 );
		}
	}

	if( Ships[WhoIAm].Object.Mode == GAMEOVER_MODE )
		CenterPrint4x5Text(
			"Game Over" ,
			(render_info.window_size.cy >> 1) - (FontHeight*2) ,
			2 );

#if defined(__3DS__) && defined(RENDERER_C3D)
	/* Stereo diagnostic overlay — kept behind the "Show stereo debug"
	   menu toggle. The SD-card file log was removed: fopen / fprintf /
	   fclose on sdmc:/ every 20 frames cost enough milliseconds per
	   write to visibly hitch frame pacing. On-screen text is free. */
	{
		extern bool g_show_stereo_debug;
		if (g_show_stereo_debug)
		{
			extern float platform_get_3d_slider(void);
			char _dbg[64];
			float _slider = platform_get_3d_slider();
			sprintf(_dbg, "slider: %.3f",  _slider);
			Print4x5Text(_dbg, FontWidth, FontHeight*2, 1);
			sprintf(_dbg, "eye_sep: %.2f", render_info.stereo_eye_sep);
			Print4x5Text(_dbg, FontWidth, FontHeight*3, 1);
			sprintf(_dbg, "stereo: %s", render_info.stereo_enabled ? "on" : "off");
			Print4x5Text(_dbg, FontWidth, FontHeight*4, 1);
		}
	}
#endif
}

void ReleaseLevel(void)
{
#if defined(__3DS__) && (defined(DEATH_SIM) || defined(VERBOSE_TRACE))
#define _RL_TRACE(s) do { extern void trace(const char*); trace("DEATH_SIM/RL: " s); } while(0)
#else
#define _RL_TRACE(s) ((void)0)
#endif
  _RL_TRACE("enter");
  switch( MyGameStatus )
  {
  case  STATUS_Title:
  case  STATUS_BetweenLevels:
  case  STATUS_StartingSinglePlayer:
  case  STATUS_TitleLoadGameStartingSinglePlayer:
    _RL_TRACE("skip-list branch");
    break;

  default:
    _RL_TRACE("default branch");
    if( GodMode ) LoseAllWeapons();

    _RL_TRACE("ReleaseComponentedShips");
    ReleaseComponentedShips();
    _RL_TRACE("KillThermo");
    KillThermo();
    _RL_TRACE("NodeRelease");
    NodeRelease();
    _RL_TRACE("CameraRelease");
    CameraRelease();
    _RL_TRACE("ReleaseExternalForces");
    ReleaseExternalForces();
    _RL_TRACE("ReleaseTeleports");
    ReleaseTeleports();
    _RL_TRACE("ReleaseTriggers");
    ReleaseTriggers();
    _RL_TRACE("ReleaseTriggerArea");
    ReleaseTriggerArea();
    _RL_TRACE("ReleaseRTLights");
    ReleaseRTLights();
    _RL_TRACE("ReleaseGoal");
    ReleaseGoal();

    if( CountDownOn )
    {
      _RL_TRACE("DeleteCountdownDigits");
      DeleteCountdownDigits();
    }

    _RL_TRACE("KillAllBGObjects");
    KillAllBGObjects();
    _RL_TRACE("ReleaseAllEnemies");
    ReleaseAllEnemies();
    _RL_TRACE("ReleaseAllRestartPoints");
    ReleaseAllRestartPoints();
    _RL_TRACE("DestroySound");
    DestroySound( DESTROYSOUND_All );
    _RL_TRACE("default DONE");
    break;
  }
#undef _RL_TRACE
}



void
ReleaseScene(void)
{
  int16_t Count;

  if( RegenPoints != NULL )
  {
    free( RegenPoints );
    RegenPoints = NULL;
    NumRegenPoints = 0;

    for( Count = 0; Count < MAX_PLAYERS; Count++ )
    {
      if( RegenSlotsCopy[ Count ] != NULL )
      {
        free( RegenSlotsCopy[ Count ] );
        RegenSlotsCopy[ Count ] = NULL;
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
#ifdef __3DS__
  { extern void trace(const char*); char _b[64]; snprintf(_b,sizeof(_b),"ReleaseView: status=%d",(int)MyGameStatus); trace(_b); }
#endif
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
  case  STATUS_WaitingToStartDemo:        // Added by DC 24/1/98
    ReleaseFlyGirl();
    FreeAllLastAFrameScrPolys();
    ReleaseTitle();
    ReleaseTloadheader( &Tloadheader );
    ReleaseTitleModels();
    Free_All_Off_Files( &Title_OffsetFiles[ 0 ] );
    ReleaseRenderBufs();
    /* Hunk_FreeAll(TAG_LEVEL) bulk-releases all the per-execbuf
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
      extern void Hunk_FreeAll(int);
      Hunk_FreeAll(/*TAG_LEVEL=*/1);
    }
    break;

  case STATUS_ViewingScore:
  case STATUS_ViewingStats:
    break;

  default:
#ifdef __3DS__
#define _RV_TRACE(s) do { extern void trace(const char*); trace("ReleaseView/default: " s); } while(0)
#else
#define _RV_TRACE(s) ((void)0)
#endif
    _RV_TRACE("Bspfree");
    Bspfree();
    _RV_TRACE("FreeAllLastAFrameScrPolys");
    FreeAllLastAFrameScrPolys();
    _RV_TRACE("ReleaseMloadheader");
    ReleaseMloadheader(&Mloadheader);
    _RV_TRACE("ReleaseTloadheader");
    ReleaseTloadheader( &Tloadheader );
    _RV_TRACE("ReleaseModels");
    ReleaseModels();
    if ( MCloadheader.Buffer )
    {
      _RV_TRACE("free MCloadheader.Buffer");
      free( MCloadheader.Buffer );
      MCloadheader.Buffer = NULL;
    }
    if ( MCloadheadert0.Buffer )
    {
      _RV_TRACE("free MCloadheadert0.Buffer");
      free( MCloadheadert0.Buffer );
      MCloadheadert0.Buffer = NULL;
    }
    _RV_TRACE("Free_All_Off_Files");
		Free_All_Off_Files( &OffsetFiles[ 0 ] );
    _RV_TRACE("ReleaseSkinExecs");
    ReleaseSkinExecs();
    _RV_TRACE("ReleasePortalExecs");
    ReleasePortalExecs();
    _RV_TRACE("ReleaseRenderBufs");
    ReleaseRenderBufs();
    _RV_TRACE("WaterRelease");
		WaterRelease();
    _RV_TRACE("FreeTxtFile");
    FreeTxtFile();
    _RV_TRACE("FreeMsgFile");
    FreeMsgFile();
    /* Same per-level Hunk teardown as the title-cleanup branch above.
     * ModelHeaders / MxaModelHeaders are intentionally kept live until
     * the next PreInitModel — see the title-branch comment for why. */
    {
      extern void Hunk_FreeAll(int);
      Hunk_FreeAll(/*TAG_LEVEL=*/1);
    }
    _RV_TRACE("default DONE");
#undef _RV_TRACE
  }
}

bool
InitScene(void)
{
  int i;

#if defined(__3DS__) && (defined(DEATH_SIM) || defined(VERBOSE_TRACE))
#define _IS_TRACE(s) do { extern void trace(const char*); trace("DEATH_SIM/IS: " s); } while(0)
#else
#define _IS_TRACE(s) ((void)0)
#endif
  _IS_TRACE("enter");
  _IS_TRACE("OnceOnlyInitModel");
  OnceOnlyInitModel();
  _IS_TRACE("InitXLights");
  InitXLights();
  _IS_TRACE("InitPrimBulls");
  InitPrimBulls();
  _IS_TRACE("InitSecBulls");
  InitSecBulls();
  _IS_TRACE("InitFmPoly");
  InitFmPoly();

#ifdef SHADOWTEST
  InitTriangles();
#endif

  _IS_TRACE("InitPolys");
  InitPolys();
  _IS_TRACE("InitScrPolys");
  InitScrPolys();
  _IS_TRACE("InitLines");
  InitLines();
  _IS_TRACE("SetupSpotFX");
  SetupSpotFX();
  _IS_TRACE("InitPickups");
  InitPickups();
  _IS_TRACE("InitAmmoUsed");
  InitAmmoUsed();
  _IS_TRACE("InitAfterBurners");
  InitAfterBurners();
  _IS_TRACE("InitBGObjects");
  InitBGObjects();
  _IS_TRACE("InitRestartPoints");
  InitRestartPoints();
  _IS_TRACE("InitEnemies");
  InitEnemies();

  for( i = 0 ; i < MAX_PLAYERS ; i++ )
  {
    Ships[i].ModelNum = (u_int16_t) -1;
  }

  switch( MyGameStatus )
  {
  case  STATUS_BetweenLevels:
  case  STATUS_Title:
  case  STATUS_StartingSinglePlayer:
  case  STATUS_TitleLoadGameStartingSinglePlayer:
  case  STATUS_StartingMultiplayer:
  case  STATUS_GetPlayerNum:
    _IS_TRACE("LoadTitleModels");
    if (!LoadTitleModels())
    { Msg( "LoadTitleModels failed.\n" );
      _IS_TRACE("LoadTitleModels FAILED");
      return false;
    }
    break;
  }
  _IS_TRACE("DONE");
#undef _IS_TRACE

  return true;

}

/*===================================================================
  Procedure :   Init the view...This might be title or score or main game....
  Output    :   bool true/FLASE
===================================================================*/

bool InitView( void )
{
	DebugPrintf("InitView Starting...\n");

#if defined(__3DS__) && (defined(DEATH_SIM) || defined(VERBOSE_TRACE))
#define _IV_TRACE(s) do { extern void trace(const char*); trace("DEATH_SIM/IV: " s); } while(0)
#else
#define _IV_TRACE(s) ((void)0)
#endif
	_IV_TRACE("enter");

	CheatsDisabled = false;

	input_buffer_reset();

	_IV_TRACE("InitModeCase");
	InitModeCase();

	switch( MyGameStatus )
	{
		case  STATUS_BetweenLevels:
		case  STATUS_Title:
		case  STATUS_StartingSinglePlayer:
		case  STATUS_TitleLoadGameStartingSinglePlayer:
		case  STATUS_StartingMultiplayer:
		case  STATUS_GetPlayerNum:
		_IV_TRACE("title-branch InitTitle");
		if( InitTitle() != true ) // bjd
		{
			_IV_TRACE("InitTitle FAILED");
			SeriousError = true;
			return false;
		}

		if ( !bSoundEnabled )
		{
			_IV_TRACE("InitializeSound");
			if (! InitializeSound( DESTROYSOUND_All ))
			{
				DebugPrintf("unable to initialise sound in initview\n");
				Msg("unable to initialise sound in initview\n");
				exit(1);
			}
		}

		_IV_TRACE("InitRenderBufs");
		InitRenderBufs();
		_IV_TRACE("SetMatrixViewPort");
		if( !SetMatrixViewPort() )
		{
		  SeriousError = true;
		  Msg( "SetMatrixViewPort() Failed\n" );
		  return false;
		}
		// Init the Texture Handler
		_IV_TRACE("InitTload");
		InitTload( &Tloadheader );

		_IV_TRACE("Load_All_Off_Files");
		if( !Load_All_Off_Files( &Title_OffsetFiles[ 0 ] ) )
		{
		  SeriousError = true;
		  return false;
		}

		_IV_TRACE("PreLoadFlyGirl");
		if( !PreLoadFlyGirl() )
		{
		  SeriousError = true;
		  return false;
		}

		_IV_TRACE("PreInitModel(TitleModelSet)");
		if( !PreInitModel( TitleModelSet ) ) // bjd
		{
		  SeriousError = true;
		  return false;
		}

		//  Load in And if nescessary ReScale Textures...
		_IV_TRACE("Tload");
		if( !Tload( &Tloadheader ) )
		{
		  SeriousError = true;
		  return false;
		}
		_IV_TRACE("InitModel(TitleModelSet)");
		if( !InitModel( TitleModelSet ) ) // bjd
		{
		  SeriousError = true;
		  return false;
		}
		_IV_TRACE("AllocateCompFlyGirl");
		if ( !AllocateCompFlyGirl() )
		{
		  SeriousError = true;
		  return false;
		}
		else
		{
		  UpdateFlyGirl( &BikePos, &MATRIX_Identity, false );
		}

		_IV_TRACE("FindTexture(dummy)");
		DummyTextureIndex = FindTexture( &Tloadheader, "data\\textures\\dummy.bmp" );
		if ( DummyTextureIndex != -1 )
		{
			if(!Tloadheader.PlaceHolderFile[ DummyTextureIndex ])
				Tloadheader.PlaceHolderFile[ DummyTextureIndex ] = ( char * )malloc( sizeof( char ) * 256 );
			Tloadheader.PlaceHolder[ DummyTextureIndex ] = true;
		}

		if ( !CurrentMenu )
		{
		  _IV_TRACE("MenuRestart(MENU_Start)");
		  MenuRestart( &MENU_Start );
		}
		_IV_TRACE("title-branch DONE");
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
  _IV_TRACE("DONE");
#undef _IV_TRACE


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
	//if( NewLevelNum != LevelNum )
	//  return true;

	LevelNum = NewLevelNum;

#ifdef __3DS__
	music_play_for_level(LevelNum);
#endif

	//NumGoldBars = 0;

	ReleaseLevel();
	KillNodeCubeLines();
	KillBoxLines();

	// $$$
	NextworkOldBikeNum = -1;

	ActiveRemoteCamera = NULL;
	MissileCameraActive = 0;
	CameraMissile = (u_int16_t) -1;

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

void GetLevelName( char *buf, int bufsize, int level )
{
  FILE *f;
  char *pCh;
  
  buf[ 0 ] = 0;

  if ( level < 0 )
    return;
  
  f = file_open( MissionTextNames[ level ], "r" );
  if ( f )
  {
    // read level name
    if ( !fgets( buf, bufsize, f ) )
    {
        // if error ( as oposed to end of file )
      if ( ferror( f ) != 0 )
      {
        buf[ 0 ] = 0;
      }
    }
  }
  if ( !buf[ 0 ] )
  {
    strncpy( buf, LevelList.item[ level ], bufsize );
  }

  pCh = strchr( buf, '\n' );
  if ( pCh )
    *pCh = 0;
  
  buf[ bufsize - 1 ] = 0; // ensure null terminated 
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
extern void ReleaseView(void);
bool RenderScene( void )
{
  u_int16_t  i,e;
  char  buf[256];
  //struct _stat stat_buf;
  //int result;
  static int WaitFrames = 2;
  bool done;

  //DebugPrintf("RenderScene Started\n");

#if defined(VERBOSE_TRACE) && defined(__3DS__)
  { extern void trace(const char*); extern int _vt_flip_remaining;
    if (_vt_flip_remaining > 0) {
      char _b[64];
      snprintf(_b, sizeof(_b), "RS: entry status=%u SeriousError=%d", (unsigned)MyGameStatus, (int)SeriousError);
      trace(_b);
    }
  }
#endif

  if ( SeriousError )
    return false;

#ifdef DEATH_SIM
  { extern void death_sim_tick(void); death_sim_tick(); }
#endif

#ifdef __3DS__
  _3ds_mr_ctx = "pre-ReadInput";
#endif
  // This is where in game we are getting input data read
  ReadInput();
#ifdef __3DS__
  _3ds_mr_ctx = "post-ReadInput";
#endif

  //if ( !Bsp_Identical( &Bsp_Header[ 0 ], &Bsp_Original ) )
  //  hr = 0;

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
#ifdef __3DS__
	{ extern void trace_enable(void); extern void trace(const char*);
	  static int _wtss_count = 0;
	  if (_wtss_count == 0) { trace_enable(); trace("WTSS: first frame"); }
	  _wtss_count++;
	  if (_wtss_count == 10) trace("WTSS: frame 10");
	  if (_wtss_count == 60) trace("WTSS: frame 60 (VduFinished not done?)");
	}
#endif

    if( DisplayTitle() != true )
    {
      SeriousError = true;
      return false;
    }

    done = VduFinished( &MENU_NEW_BetweenLevels );
#ifdef __3DS__
	{ extern void trace(const char*); static int _vf_logged = 0;
	  if (done && !_vf_logged) { _vf_logged=1; trace("WTSS: VduFinished=true"); }
	}
#endif

    if ( done )
      WaitFrames--;
    if ( !WaitFrames )
    {
      WaitFrames = 2;
#ifdef __3DS__
	{ extern void trace(const char*); trace("WTSS: calling StartASinglePlayerGame"); }
#endif
      StartASinglePlayerGame( NULL );
    }

    break;


  case STATUS_WaitingToStartDemo:
	DebugState("STATUS_WaitingToStartDemo\n");
    if( DisplayTitle() != true )
    {
      SeriousError = true;
      return false;
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
    if ( VduFinished( NULL ) )
      WaitFrames--;
    if ( !WaitFrames )
    {
      WaitFrames = 2;
      MyGameStatus = STATUS_StartingMultiplayer;
      GoToSynchup( NULL );
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
    if ( VduFinished( NULL ) )
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

    if ( VduFinished( NULL ) )
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

#if defined(__3DS_DEBUG__) || defined(DEBUG_ON) || defined(AUTOTEST_REMASTER)
  {
    /* Auto-boot directly into a level. Default target = Volcano
     * (NewLevelNum=0). With AUTOTEST_REMASTER set, target becomes the
     * first new Remaster slot (defend2). Either way: run DisplayTitle()
     * for N frames first so it can complete its one-time initialization
     * (title models, HoloModel, etc.), then call StartASinglePlayerGame()
     * to skip manual menu navigation. */
    static int _autoboot_frames = 0;
    extern bool StartASinglePlayerGame(MENUITEM*);
#ifdef __3DS__
    int _autoboot_max = 3;
#else
    int _autoboot_max = 30;
#endif

    if (_autoboot_frames < _autoboot_max) {
        _autoboot_frames++;
        DebugPrintf("autoboot: title frame %d/%d\n", _autoboot_frames, _autoboot_max);
        if (!DisplayTitle()) { SeriousError = true; return false; }
        break;
    }
    DebugPrintf("autoboot: calling StartASinglePlayerGame\n");
    VduClear();
    input_buffer_reset();
    StartASinglePlayerGame(NULL);
    /* StartASinglePlayerGame -> InitLevels(SINGLEPLAYER_LEVELS) resets
     * NewLevelNum to 0 as a side effect (singleplayer.c:99). Apply our
     * desired starting level AFTER the call so it survives. */
#ifdef AUTOTEST_REMASTER
    {
      extern int autotest_first_level(void);
      NewLevelNum = autotest_first_level();
      extern void trace(const char*);
      char _b[96]; snprintf(_b, sizeof(_b),
        "AUTOTEST: autoboot override -> NewLevelNum=%d", (int)NewLevelNum); trace(_b);
    }
#else
    /* DEBUG-build autoboot target. Defaults to slot 0 (vol2 / volcano) but
     * can be overridden via -DDEBUG_AUTOBOOT_SLOT=N for boss-rendering /
     * level-specific tests. */
#ifndef DEBUG_AUTOBOOT_SLOT
#define DEBUG_AUTOBOOT_SLOT 0
#endif
    NewLevelNum = DEBUG_AUTOBOOT_SLOT;
#if DEBUG_AUTOBOOT_SLOT == 0
    /* Start at checkpoint 1 (lava tube area) for easier debugging */
    {
        extern u_int16_t last_start_position;
        last_start_position = 1;
    }
#endif
#endif
    break;
  }
#endif

  // ??
  case STATUS_BetweenLevels:
	if(MyGameStatus != STATUS_Title)
		DebugState("STATUS_BetweenLevels\n");

    if( DisplayTitle() != true )
    {
      SeriousError = true;
      return false;
    }
#ifdef AUTOTEST_REMASTER
    /* Natural-mode autotest hook: counts BetweenLevels frames while a
     * transition is in flight and auto-confirms the inter-level menu
     * via StartASinglePlayerGame(NULL) once a short dwell elapses.
     * No-op when no transition is armed (s_advancing == false). */
    { extern void autotest_between_levels_tick(void);
      autotest_between_levels_tick(); }
#endif
    break;


  case STATUS_Normal:
    DebugState("STATUS_Normal\n");

    if ( !CheatsDisabled )
      DisableCheats();
  
    if( IMustQuit )
      SpecialDestroyGame();

    if( MainGame() != true ) // bjd
      return false;

    if ( bSoundEnabled )
    {
      ProcessLoopingSfx();
#ifdef PLAYER_SPEECH_TAUNTS
      ProcessTaunt();
#endif
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
      SelectQuitCurrentGame( NULL );
    }
    break;


  case STATUS_LevelEnd:
	DebugState("STATUS_LevelEnd\n");

    clear_black();
    ReceiveGameMessages();
    Browl -= framelag;

    StopDemoRecording();

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

      // Added the if check to not call OutputVisiStats improperly
      // The order of code will always result in LevelNum being -1 here
      // So no point in even leaving this code uncommented...
      // We'll probably just end up getting rid of visi stats anyway later
      //if ( LevelNum != -1 )
      //	OutputVisiStats( &Mloadheader, LevelNames[ LevelNum ] );

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

		//        if( GameStatus[i] == STATUS_ViewingScore )
		//        {
		//          sprintf( buf, "%-8s status %8s\n", &Names[i][0] , "viewing score" );
		//          CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy>>1)-( ( (FontHeight+2) * MAX_PLAYERS ) >> 1 )+ (e * (FontHeight+(FontHeight>>1)) ) , GameStatus[i] == STATUS_StartingMultiplayerSynch ? 2 : 1 );
		//        }
		//        else
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

      //DebugPrintf("STATUS_WaitingAfterScore setting MyGameStatus to STATUS_Normal\n");
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
	FSCreateDynamic2dVertexBuffer(&ro, 32767);
	FSCreateIndexBuffer(&ro, 32767*3);


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
		#define LOADING_SCREEN_FPS 1.0f/10.0f
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
	  
#ifdef DEMO_SUPPORT
      QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
#endif

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
      if( TeamGame &&  (OverallGameStatus == STATUS_Normal) )
	  {
		  //SendGameMessage(MSG_SENDKILLSDEATHSBIKENUM, 0, 0, 0, 0);
	  }
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
            //LastMenu = CurrentMenu;
            //VduClear();
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
#ifdef DEMO_SUPPORT
      QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
#endif
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
#ifdef DEMO_SUPPORT
        QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
#endif
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

#if defined(__3DS__) && (defined(DEATH_SIM) || defined(VERBOSE_TRACE))
#define _DS_TRACE(s) do { extern void trace(const char*); trace("DEATH_SIM/QCG: " s); } while(0)
#else
#define _DS_TRACE(s) ((void)0)
#endif
    _DS_TRACE("enter case");
    _DS_TRACE("OutputVisiStats");
    OutputVisiStats( &Mloadheader, LevelNames[ LevelNum ] );
    _DS_TRACE("MenuAbort");
    MenuAbort();
    _DS_TRACE("MenuRestart");
    MenuRestart( &MENU_Start );
    _DS_TRACE("DestroyGame");
    DestroyGame();  // DestroyGame changes MyGameStatus..So Set it back
    MyGameStatus = STATUS_QuitCurrentGame;
    _DS_TRACE("ReleaseView");
    ReleaseView();
    _DS_TRACE("ReleaseLevel");
    ReleaseLevel();
    _DS_TRACE("set STATUS_Title");
    MyGameStatus = STATUS_Title;
#if defined(__3DS__) && defined(RENDERER_C3D)
    /* Wipe the gameplay HUD off the bottom screen — the title screen
     * never draws there, so without this the last HUD frame stays
     * frozen until the next gameplay session takes over. */
    _DS_TRACE("pglClearBottomScreen");
    pglClearBottomScreen();
#endif
    _DS_TRACE("InitScene");
    InitScene();
    _DS_TRACE("InitView");
    InitView();
    _DS_TRACE("QCG complete");
#undef _DS_TRACE

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
				PrintErrorMessage ( "The Game is Currently Un-Joinable", 3, NULL, ERROR_USE_MENUFUNCS );
				MyGameStatus = STATUS_Title;
				break;
			case MAX_PLAYERS+1:
				PrintErrorMessage ( "You Dont have the level they are playing", 3, NULL, ERROR_USE_MENUFUNCS );
				MyGameStatus = STATUS_Title;
				break;
			case MAX_PLAYERS+2:
				PrintErrorMessage ( "The Game is Full", 3, NULL, ERROR_USE_MENUFUNCS );
				MyGameStatus = STATUS_Title;
				break;
			case MAX_PLAYERS+3:
				PrintErrorMessage ( "You do have the right version", 3, NULL, ERROR_USE_MENUFUNCS );
				MyGameStatus = STATUS_Title;
				break;
			default:
				PrintErrorMessage ( "Could not join game host gave no reason why", 3, NULL, ERROR_USE_MENUFUNCS );
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
      PrintErrorMessage ( "Couldnt Get a Player Number", 3, NULL, ERROR_USE_MENUFUNCS );
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

#ifdef DEMO_SUPPORT
  case STATUS_PlayingDemo:
	DebugState("STATUS_PlayingDemo\n");

    DemoGameLoops++;

    if( DemoSpeed.value > 8 )
    {
      // slower or normal playback speed...
      Demoframelag = 1.0F / (float) ( DemoSpeed.value - 7 );
    }else{
      Demoframelag = 1.0F * (float) ( 9 - DemoSpeed.value );
    }
    
    Oldframelag = framelag;

    if( PauseDemo )
    {
      framelag = 0.0F;
    }else{
      framelag *= Demoframelag;
    }

    if( MainGame() != true ) // bjd
      return false;

    break;
#endif

  case STATUS_InitView_0:
	DebugState("STATUS_InitView_0\n");
#ifdef __3DS__
	{ extern void trace_enable(void); extern void trace(const char*); trace_enable(); trace("IV0: entry"); }
#endif

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();

    MenuFrozen = false; // ensure that menus are OK to use once in game
    JustExitedMenu = false;

    QuickStart = QUICKSTART_None;
    WaitingToQuit = false;

    ReceiveGameMessages();
    if( !SetMatrixViewPort() )
    {
      SeriousError = true;
      Msg( "SetMatrixViewPort() failed\n" );
      return false;
    }
    InitFont();

/*
    MyGameStatus = STATUS_InitView_1;

    break;
  
  case STATUS_InitView_1:
	DebugState("STATUS_InitView_1\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();

    ReceiveGameMessages();
*/

    ReMakeSimplePanel = true;

		// InitVisiExecList( lpDev );
    InitSkinExecs();
    InitPortalExecs();
    InitRenderBufs();

#ifdef __3DS__
	{ extern void trace(const char*); char _b[96];
	  snprintf(_b, sizeof(_b), "IV0: InitTload start (LevelNum=%d name=%s)",
	           LevelNum, LevelNames[LevelNum]); trace(_b); }
#endif
    // Init the Texture Handler
    if( !InitTload( &Tloadheader ) )
    {
      SeriousError = true;
      Msg( "InitTLoad failed\n" );
      return false;
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: InitTload OK -> PreMload"); }
#endif

    //  Prep the Texture Handler.....
    if( !PreMload( (char*) &LevelNames[LevelNum][0] , &Mloadheader ) )
    {
      SeriousError = true;
      return false; // the model and visipoly data
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: PreMload OK -> PreWaterLoad"); }
#endif

    // Can Cope with no .Wat file!!!
    PreWaterLoad( (char*) &WaterNames[LevelNum][0] );
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: PreWaterLoad OK"); }
#endif

    if( OnceOnlyChangeLevel )
    {
      OnceOnlyChangeLevel = false;
#ifdef __3DS__
	  { extern void trace(const char*); trace("IV0: OnceOnly: PreLoadShips"); }
#endif
      if( !PreLoadShips() )
      {
        SeriousError = true;
        return false;
      }
#ifdef __3DS__
	  { extern void trace(const char*); trace("IV0: OnceOnly: PreLoadBGOFiles"); }
#endif
      if( !PreLoadBGOFiles() )
      {
        SeriousError = true;
        return false;
      }
#ifdef __3DS__
	  { extern void trace(const char*); trace("IV0: OnceOnly: PreLoadRestartPoints"); }
#endif
      if( !PreLoadRestartPoints() )
      {
        SeriousError = true;
        return false;
      }
#ifdef __3DS__
	  { extern void trace(const char*); trace("IV0: OnceOnly: PreLoadEnemies"); }
#endif
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
#ifdef __3DS__
      { extern void trace(const char*); trace("IV0: OnceOnly: PreLoadPickups"); }
#endif
      { extern bool PreLoadPickups(void);
        if( !PreLoadPickups() )
        {
          SeriousError = true;
          return false;
        }
      }
#endif /* EDITION_REMASTER */
#ifdef __3DS__
	  { extern void trace(const char*); trace("IV0: OnceOnly done"); }
#endif
    }

#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: EnableRelavantModels"); }
#endif
    EnableRelavantModels( &ModelNames[0] );
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: EnableRelavantModels OK -> PreInitModel"); }
#endif
		if( !PreInitModel( /*lpDev,*/ &ModelNames[0] ) ) // bjd
		{
			SeriousError = true;
			return false;
		}
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: PreInitModel OK -> Load_All_Off_Files"); }
#endif
		if( !Load_All_Off_Files( &OffsetFiles[ 0 ] ) )
		{
			SeriousError = true;
			return false;
		}
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: Load_All_Off_Files OK -> Tload"); }
#endif
    //  Load in And if nescessary ReScale Textures...
    if( !Tload( &Tloadheader ) )
    {
      SeriousError = true;
      return false;
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: Tload OK"); }
#endif

/*
    MyGameStatus = STATUS_InitView_2;
    PrintInitViewStatus( MyGameStatus );
    break;
  

  case STATUS_InitView_2:
	DebugState("STATUS_InitView_2\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();

    ReceiveGameMessages();

    MyGameStatus = STATUS_InitView_3;
    PrintInitViewStatus( MyGameStatus );
    break;
  

  case STATUS_InitView_3:
	DebugState("STATUS_InitView_3\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();

    ReceiveGameMessages();
*/

#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: InitModel"); }
#endif
    if( !InitModel( &ModelNames[0] ) )
    {
       SeriousError = true;
       return false;               // all 3d models....
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: InitModel OK"); }
#endif

/*
    MyGameStatus = STATUS_InitView_4;
    PrintInitViewStatus( MyGameStatus );
    break;


  case STATUS_InitView_4:
	DebugState("STATUS_InitView_4\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();

    ReceiveGameMessages();
*/
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: Mload"); }
#endif
    if( !Mload( (char*) &LevelNames[LevelNum][0] , &Mloadheader ) )
    {
      SeriousError = true;
      return false; // the model and visipoly data
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: Mload OK -> InitVisiStats"); }
#endif

    InitVisiStats( &Mloadheader );

#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: InitVisiStats OK -> Bspload"); }
#endif

/*
    MyGameStatus = STATUS_InitView_5;
    PrintInitViewStatus( MyGameStatus );
    break;
  

  case STATUS_InitView_5:
	DebugState("STATUS_InitView_5\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();
    ReceiveGameMessages();
*/

    // Can Cope with no Bsp file!!!
#ifdef LOAD_ZBSP
    Bspload( (char*) &BspZNames[LevelNum][0], &Bsp_Header[ 0 ] );
    Bspload( (char*) &BspNames[LevelNum][0], &Bsp_Header[ 1 ] );
#else
#ifdef BSP_ONLY
    if ( !Bspload( (char*) &BspNames[LevelNum][0], &Bsp_Header[ 0 ] ) )
    {
      SeriousError = true;
      Msg( "Bspload failed\n" );
      return false;   // the collision data
    }
#else
    Bspload( (char*) &BspNames[LevelNum][0], &Bsp_Header[ 0 ] ); // load .BSP file into 0 skin
#endif
    Bsp_Header[ 1 ].State = false; // no non-zero .BSP any more
#endif
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: Bspload OK -> WaterLoad"); }
#endif
	//    if ( !Bsp_Duplicate( &Bsp_Header[ 0 ], &Bsp_Original ) )
	//    {
	//		    SeriousError = true;
	//		    Msg( "Bsp_Duplicate failed\n" );
	//		    return false;
	//    }
    
    // might not be any water...
    WaterLoad();
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: WaterLoad OK -> ReadTxt/MsgFile"); }
#endif

    ReadTxtFile( (char*) &TextNames[LevelNum][0] );

    ReadMsgFile( (char*) &MsgNames[LevelNum][0] );

#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: TxtMsg OK -> MCload"); }
#endif
    if( !MCload( (char*) &CollisionNames[LevelNum][0] , &MCloadheader ) )
    {
      SeriousError = true;
      Msg( "MCload non zero failed\n" );
      return false;   // the collision data
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: MCload nz OK -> MCload zero"); }
#endif
    if( !MCload( (char*) &CollisionZNames[LevelNum][0] , &MCloadheadert0 ) )
    {
      SeriousError = true;
      Msg( "MCload zero failed\n" );
      return false; // the collision data skin thickness 0
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: MCload zero OK -> SetUpShips"); }
#endif

    SetUpShips();
#ifdef __3DS__
	{ extern void trace(const char*); trace("IV0: SetUpShips OK"); }
#endif

/*
    MyGameStatus = STATUS_InitView_6;
    PrintInitViewStatus( MyGameStatus );

    break;

  case STATUS_InitView_6:
	DebugState("STATUS_InitView_6\n");

    if( IsHost )
    {
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
    }
		else
		{
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);
    }

    clear_black();
    ReceiveGameMessages();
*/

    InitSoundInfo( &Mloadheader );

/*
    MyGameStatus = STATUS_InitView_7;
    PrintInitViewStatus( MyGameStatus );
    break;


  case STATUS_InitView_7:
	DebugState("STATUS_InitView_7\n");

    if( IsHost )
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
		else
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);

    clear_black();
    ReceiveGameMessages();
*/

#ifdef NO_PRECALCULATED_CELL_COLOURS
    CreateCellColours( &Mloadheader );
#endif

/*
    MyGameStatus = STATUS_InitView_8;
    PrintInitViewStatus( MyGameStatus );
    break;


  case STATUS_InitView_8:
	DebugState("STATUS_InitView_8\n");

    if( IsHost )
      SendGameMessage(MSG_LONGSTATUS, 0, 0, 0, 0);
		else
      SendGameMessage(MSG_STATUS, 0, 0, 0, 0);

    clear_black();
    ReceiveGameMessages();

		DebugState("STATUS_InitView_9\n");
    MyGameStatus = STATUS_InitView_9;
    PrintInitViewStatus( MyGameStatus );
*/

    InitShipSpeeds();

    // this will cause a lovely game loop and crash the game
		// so don't remove this !!!!!!!!!
    if( InitView_MyGameStatus != STATUS_InitView_0 )
			MyGameStatus = InitView_MyGameStatus;

/*
		//
		input_grab( true );
*/

    break;


  case STATUS_ChangeLevelPostInitView:
	DebugState("STATUS_ChangeLevelPostInitView\n");
#ifdef __3DS__
	/* [3DS] Debug traces for the level-load sequence.  Each checkpoint writes
	 * to sdmc:/forsaken_trace.txt so a crash can be pinpointed without a
	 * debugger.  Safe to remove once the port is stable. */
	{ extern void trace(const char*); trace("CLPIV: entry"); }
#endif

    Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NodeName[ 0 ], ".NOD" );
    if( !Nodeload( NodeName ) )
    {
      SeriousError = true;
      return( false );
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: Nodeload OK"); }
#endif

    if( !LoadBGOFiles() )
    {
      SeriousError = true;
      return( false );
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: LoadBGOFiles OK"); }
#endif

    if( !LoadEnemies() )
    {
      SeriousError = true;
      Msg( "LoadEnemies() failed\n" );
      return( false );
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: LoadEnemies OK"); }
#endif

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

    if( !LoadRestartPoints() )
    {
      SeriousError = true;
      Msg( "LoadRestartPoints() failed\n" );
      return( false );
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: LoadRestartPoints OK"); }
#endif

      if ( !InitializeSound( DESTROYSOUND_All ))
      {
        Msg("InitializeSound() failed\n");
        return false;
      }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: InitializeSound OK"); }
#endif

    Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NodeName[ 0 ], ".CAM" );

    if( !Cameraload( NodeName ) )
    {
      SeriousError = true;
      return( false );
    }
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: Cameraload OK"); }
#endif

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
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: LoadPickupsPositions OK"); }
#endif

    // might not be any External Forces...
    ExternalForcesLoad( (char*) &ExternalForceNames[LevelNum][0] );

    // might not be any Teleports...
    TeleportsLoad( (char*) &TeleportNames[LevelNum][0] );

    // Can Cope with no Zone file!!!
    TriggerAreaload( (char*) &ZoneNames[LevelNum][0] );

    InitShipsChangeLevel(&Mloadheader);
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: InitShipsChangeLevel OK"); }
#endif

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
#ifdef __3DS__
	{ extern void trace(const char*); char _b[64]; snprintf(_b,sizeof(_b),"CLPIV: done, MyGameStatus=%d",(int)MyGameStatus); trace(_b); }
#endif

    PrintInitViewStatus( MyGameStatus );
#ifdef __3DS__
	{ extern void trace(const char*); trace("CLPIV: post-PrintInitViewStatus"); }
#endif


    break;


#ifdef DEMO_SUPPORT
  case STATUS_ChangeLevelPostPlayingDemo:
	DebugState("STATUS_ChangeLevelPostPlayingDemo\n");

    WhoIAm = MAX_PLAYERS;
    Ships[WhoIAm].Object.Mode = DEMO_MODE;
    Ships[WhoIAm].enable = 1;
    IsHost = true;
    Current_Camera_View = MAX_PLAYERS;    // which object is currently using the camera view....
  
    for( i = 0 ; i < MAX_PLAYERS ; i++ )
    {
      GameStatus[i] = STATUS_Null;
    }
  
    DemoTimeSoFar = 0;
    if( DemoSpeed.value > 8 )
    {
      // slower or normal playback speed...
      Demoframelag = 1.0F / (float) ( DemoSpeed.value - 7 );
    }else{
      Demoframelag = 1.0F * (float) ( 9 - DemoSpeed.value );
    }
  
    GameElapsedTime = 0;

    QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
    QueryPerformanceCounter((LARGE_INTEGER *) &DemoStartedTime);
    DemoGameLoops = 0;
    TempGameElapsedTime = GameStartedTime;
    MyGameStatus = STATUS_PlayingDemo;
    break;
#endif

	//  *********************** Single Player Game Stuff **********************************


  case  STATUS_StartingSinglePlayer:
	DebugState("STATUS_StartingSinglePlayer\n");
#ifdef __3DS__
	{ extern void trace_enable(void); extern void trace(const char*); trace_enable(); trace("SSP: entry"); }
#endif

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
    GameCompleted = GAMECOMPLETE_NotComplete;
    if( !ChangeLevel() ) return( false );
#ifdef __3DS__
    /* [3DS] Clear picaGL color buffer to black after level load completes.
     *
     * ChangeLevel() contains its own internal render loop (STATUS_InitView_0,
     * STATUS_ChangeLevelPostInitView, etc.) that renders loading-screen frames
     * into the picaGL color buffer and pumps pglSwapBuffers(), updating GX
     * display buffer A.  When ChangeLevel returns, one of the two GX display
     * buffers (A) still contains the last loading-screen frame.
     *
     * On the STATUS_StartingSinglePlayer break, pglSwapBuffers() is called once
     * more by platform_render_present(); this frame uses whatever is currently
     * in the picaGL color buffer (the last loading frame).  That result is
     * DMA'd into GX display buffer B, contaminating the second buffer too.
     *
     * Fix: clear the picaGL color buffer to black here so that the
     * pglSwapBuffers() call at the end of this frame sends black to display B
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
	{ extern void trace(const char*); trace("PSP: entry"); }
#endif

#ifdef __3DS__
    /* [3DS] Second black-frame clear — flushes the remaining GX display buffer.
     *
     * The GX double-buffer scheme means there are always two display buffers
     * (A and B) alternating each swap.  STATUS_StartingSinglePlayer's
     * FSClearBlack() + pglSwapBuffers() cleared buffer B.  This frame's
     * FSClearBlack() + pglSwapBuffers() clears buffer A.  After both frames
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

#ifdef __3DS__
	{ extern void trace(const char*); trace("PSP: -> smallinitShip"); }
#endif
    smallinitShip( WhoIAm );
#ifdef __3DS__
	{ extern void trace(const char*); trace("PSP: smallinitShip OK"); }
#endif
	//    if( CountDownOn )
	//    {
	//      CreateCountdownDigits();
	//      StartCountDown( (int16_t) TimeLimit.value, 0 );
	//    }

#ifdef DEMO_SUPPORT
    QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
#endif

    MyGameStatus = STATUS_SinglePlayer;
    GameStatus[WhoIAm] = MyGameStatus;
#ifdef __3DS__
	{ extern void trace(const char*); trace("PSP: -> STATUS_SinglePlayer"); }
#endif
    LevelTimeTaken = 0.0F;
    timer_run( &level_time );

		input_grab( true );

    break;


  case STATUS_SinglePlayer:
	DebugState("STATUS_SinglePlayer\n");

#ifdef __3DS__
	{ static int _sp_first = 1; extern void trace(const char*);
	  if (_sp_first) { _sp_first = 0; trace("SP: first frame entry"); } }
#endif

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
        extern void InitScrPolys(void);
        InitScrPolys();
    }
#endif

#ifdef __3DS__
	/* Per-level first-frame trace markers — fire once per level
	 * transition so we can see whether MainGame() itself completes
	 * for each level. The original `_mg_first` static fired only on
	 * the very first STATUS_SinglePlayer frame in the whole process,
	 * which is useless for diagnosing late-sweep crashes. */
	{ static int _mg_last_level = -2; extern void trace(const char*);
	  extern int16_t LevelNum;
	  if (LevelNum != _mg_last_level) {
	    _mg_last_level = LevelNum;
	    char _b[64]; snprintf(_b,sizeof(_b),"SP: -> MainGame first frame for level %d", (int)LevelNum); trace(_b);
	  } }
#endif
    if( MainGame() != true ) // bjd
      return false;
#ifdef __3DS__
	{ static int _mg_done_last_level = -2; extern void trace(const char*);
	  extern int16_t LevelNum;
	  if (LevelNum != _mg_done_last_level) {
	    _mg_done_last_level = LevelNum;
	    char _b[64]; snprintf(_b,sizeof(_b),"SP: MainGame OK first frame for level %d", (int)LevelNum); trace(_b);
	  } }
#endif

#ifdef AUTOTEST_REMASTER
    /* Per-gameplay-frame tick of the autonomous level cycler. Runs after
     * MainGame() so the level is fully initialised before its frame is
     * counted. autotest_tick() advances NewLevelNum and forces LevelNum
     * = -1 once the dwell expires (the existing "if NewLevelNum != LevelNum"
     * branch below picks that up and triggers ReleaseView/InitView). */
    { extern void autotest_tick(void); autotest_tick(); }
#endif

#ifdef __3DS__
    _3ds_mr_ctx = "post-MainGame";
#endif
    if ( bSoundEnabled && !CurrentMenu )
    {
      ProcessLoopingSfx();
      ProcessEnemyBikerTaunt();
    }
#ifdef __3DS__
    _3ds_mr_ctx = "post-SoundProcessing";
#endif

    LevelTimeTaken += timer_run( &level_timer );

    //sprintf( buf, "time so far %d", ((u_int32_t)LevelTimeTaken) / 100 );
    //Print4x5Text( buf, 10, 30, 2 );

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

    //clear_black();
    MenuAbort();
    ReleaseView();
    // tell them all to load up a level
    MyGameStatus = STATUS_TitleLoadGamePostStartingSinglePlayer;
    GameStatus[WhoIAm] = MyGameStatus;
    LevelNum = -1;
    NumGoldBars = 0;
    CrystalsFound = 0;
    GameCompleted = GAMECOMPLETE_NotComplete;
    if( !ChangeLevel() ) return( false );
    break;


  case  STATUS_InGameLoadGameStartingSinglePlayer:
	DebugState("STATUS_InGameLoadGameStartingSinglePlayer\n");

    //clear_black();

    NewLevelNum = InGameLoadGameLevelNum;
    MenuAbort();
    ReleaseView();
    // tell them all to load up a level
    MyGameStatus = STATUS_TitleLoadGamePostStartingSinglePlayer;
    GameStatus[WhoIAm] = MyGameStatus;
    LevelNum = -1;
    NumGoldBars = 0;
    CrystalsFound = 0;
    GameCompleted = GAMECOMPLETE_NotComplete;
    if( !ChangeLevel() ) return( false );
    break;


  case  STATUS_TitleLoadGamePostStartingSinglePlayer:
	DebugState("STATUS_TitleLoadGamePostStartingSinglePlayer\n");

    smallinitShip( WhoIAm );
    InGameLoad( NULL );
    
#ifdef DEMO_SUPPORT
    QueryPerformanceCounter((LARGE_INTEGER *) &GameStartedTime);
#endif

    MyGameStatus = STATUS_SinglePlayer;
    GameStatus[WhoIAm] = MyGameStatus;
    break;

  default:
	DebugPrintf("MyGameStatus has a bad setting!!!!\n");
	break;

	//  ******************** End of Single Player Game Stuff *******************************

  }

 // DebugPrintf("RenderScene Ended\n");

  return true;
}

void MainGameDemoRoutines(){
#ifdef DEMO_SUPPORT
  QueryPerformanceCounter((LARGE_INTEGER *) &GameCurrentTime);
  if( PlayDemo )
  {
    if( PauseDemo )
    {
      TempGameElapsedTime = GameCurrentTime;
    }else{
      GameElapsedTime += (LONGLONG) ( ( GameCurrentTime - TempGameElapsedTime ) * Demoframelag );
      TempGameElapsedTime = GameCurrentTime;
      GameCurrentTime = GameCurrentTime - GameStartedTime;
      GameCurrentTime = (LONGLONG) ( GameCurrentTime * Demoframelag );
    }
  }else{
    NetworkGameUpdate();
  }
#endif
}


float Hdiv[MAX_PLAYERS] = { 1.0F ,
                1.0F ,
                1.0F ,
                0.5F ,
                0.5F ,
                0.5F ,
                1.0F ,
                0.5F };
float Vdiv[MAX_PLAYERS] = { 1.0F ,
                0.5F ,
                0.33333333F ,
                0.5F ,
                0.2F ,
                0.33333333F ,
                0.142857142F ,
                0.25F  };
float Hpos[MAX_PLAYERS][MAX_PLAYERS] = { { 0.0F } ,
                       { 0.0F , 0.0F } ,
                       { 0.0F , 0.0F , 0.0F } ,
                       { 0.0F , 1.0F , 0.0F , 1.0F } ,
                       { 0.0F , 0.0F , 0.0F , 0.0F , 0.0F } ,
                       { 0.0F , 1.0F , 0.0F , 1.0F , 0.0F , 1.0F} ,
                       { 0.0F , 0.0F , 0.0F , 0.0F , 0.0F , 0.0F , 0.0F} ,
                       { 0.0F , 1.0F , 0.0F , 1.0F , 0.0F , 1.0F , 0.0F , 1.0F } };
float Vpos[MAX_PLAYERS][MAX_PLAYERS] = { { 0.0F } ,
                       { 0.0F , 1.0F } ,
                       { 0.0F , 1.0F , 2.0F } ,
                       { 0.0F , 0.0F , 1.0F , 1.0F } ,
                       { 0.0F , 1.0F , 2.0F , 3.0F , 4.0F } ,
                       { 0.0F , 0.0F , 1.0F , 1.0F , 2.0F , 2.0F} ,
                       { 0.0F , 1.0F , 2.0F , 3.0F , 4.0F , 5.0F , 6.0F} ,
                       { 0.0F , 0.0F , 1.0F , 1.0F , 2.0F , 2.0F , 3.0F , 3.0F } };
                       

/*===================================================================
  Procedure :   Main Routines...
  Input   :   nothing...
  Output    :   nothing
===================================================================*/
void MainRoutines( void )
{
  if( PlayDemo )
  {
    DemoPlayingNetworkGameUpdate();
  }

  ProcessShips();

#ifdef SHADOWTEST
//  CreateSpotLight( (u_int16_t) WhoIAm, SHIP_RADIUS, &Mloadheader );
//  CreateShadowsForShips();
#endif

  FirePrimary();
  FireSecondary();
  ProcessEnemies();
  ProcessSpotFX();
  ProcessPrimaryBullets();
  ProcessSecondaryBullets();
  if( !PlayDemo ) RegeneratePickups();
  ProcessPickups();
  ProcessBGObjects( true );
  ProcessRestartPoints();
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
#ifdef DOESNT_WORK_AND_A_BAD_IDEA_ANYWAY
  if ( outside_map && !DebugInfo && ( Ships[WhoIAm].Object.Mode != DEATH_MODE ) && ( Ships[WhoIAm].Object.Mode != LIMBO_MODE ) )
  {
    // bugger -- the player has managed to get outside the map, so kill him off...
    Ships[WhoIAm].ShipThatLastHitMe = MAX_PLAYERS;
    Ships[WhoIAm].Damage = 255 + 255; //make sure I Die....
    DoDamage( OVERRIDE_INVUL );
  
  
    Ships[WhoIAm].Object.Mode = DEATH_MODE;
    Ships[WhoIAm].Timer = 0.0F;
    ShipDiedSend( WEPTYPE_Primary, 0);
  }
#endif
  WaterProcess();
  ProcessRTLights();
  
	//DebugPrintf("MainRoutines Finished...\n");
}

void CheckLevelEnd ( void )
{

  if( HasLevelEnded() )
  {
    PauseAllSfx();
#ifdef FINAL_RELEASE
    if( OriginalLevels )
    {
#endif
      LevelNum = -1;
      if( NewLevelNum == ( LevelList.items - 2 ) )
      {
        // if they cheated at any point
        if ( Cheated )
        {
          GameCompleted = GAMECOMPLETE_Cheated;
        }else
        {
          // Check for Secret level...
          if( CrystalsFound == ( LevelList.items - 1 ) )
          {
            //They can go to the secret level
            NewLevelNum = ( LevelList.items - 1 );
            GameCompleted = GAMECOMPLETE_GotAllCrystalsSoFar;
          }else{
            // go straight to the end sequence
            GameCompleted = GAMECOMPLETE_WithoutAllCrystals;
          }
        }
      }else if( NewLevelNum == ( LevelList.items - 1 ) ){
        // They have completed the Secret level
        if( CrystalsFound == ( LevelList.items ) )
        {
          GameCompleted = GAMECOMPLETE_WithAllCrystals;
        }else
        {
          GameCompleted = GAMECOMPLETE_WithAllCrystalsExceptLast;
        }
      }else{
        NewLevelNum++;
        GameCompleted = GAMECOMPLETE_NotComplete;
      }
      
      return;
#ifdef FINAL_RELEASE
    }else
    {
      if( NewLevelNum == ( LevelList.items - 1 ) )
      {
        // completed all levels ( not original levels )
        GameCompleted = GAMECOMPLETE_AdditionalLevels;
        return;
      }else
      {
        NewLevelNum++;
        GameCompleted = GAMECOMPLETE_NotComplete;
      }
    }
#endif
  }

}


bool RenderCurrentCameraInStereo( RenderCurrentCameraPt render_camera )
{
#ifdef __3DS__
	static int _rcs_stage_last_level = -2;
	bool _rcs_stage_trace = false;
	{
		extern int16_t LevelNum;
		if (LevelNum != _rcs_stage_last_level) {
			_rcs_stage_last_level = LevelNum;
			_rcs_stage_trace = true;
		}
	}
	#define _RCS_STAGE(s) do { if (_rcs_stage_trace) { extern void trace(const char*); trace("RCS/" s); } } while(0)
#else
	#define _RCS_STAGE(s) ((void)0)
#endif
	_RCS_STAGE("entry");
	VECTOR cam_offset;
	render_viewport_t old_viewport = viewport;

        /* Compute the camera-right offset in world space.
         * In D3D row-vector convention (FinalMat), the camera local X axis in
         * world space is the first ROW of the rotation submatrix: (_11,_12,_13).
         * Do NOT use ApplyMatrix — it adds _41/_42/_43 (world translation). */
        {
                float half_sep = render_info.stereo_eye_sep / 2.0f;
                cam_offset.x = CurrentCamera.Mat._11 * half_sep;
                cam_offset.y = CurrentCamera.Mat._12 * half_sep;
                cam_offset.z = CurrentCamera.Mat._13 * half_sep;
        }

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
	if(render_info.stereo_mode == STEREO_MODE_COLOR)
        	render_set_filter( 1, 0, 0 );
        _RCS_STAGE("calling render_camera (LEFT)");
        if( !render_camera() )
          return false;
        _RCS_STAGE("post render_camera (LEFT)");
#if defined(__3DS__) && defined(RENDERER_C3D)
	/* Hardware stereo is citro3d-only. picaGL builds force
	 * platform_get_3d_slider() to 0, so stereo_mode is never set to
	 * STEREO_MODE_3DS and this branch is dead there — but we also
	 * gate the symbol reference at compile time so picaGL doesn't
	 * have to export pglTransferEye at all. */
	if(render_info.stereo_mode == STEREO_MODE_3DS)
	{
		extern void pglTransferEye(unsigned int eye);
		_RCS_STAGE("pglTransferEye(LEFT)");
		pglTransferEye(GFX_LEFT);
		_RCS_STAGE("post pglTransferEye(LEFT)");
	}
#endif
	//
	// render right eye
	//
        render_info.stereo_position = ST_RIGHT;
        CurrentCamera.Pos.x += 2.0f * cam_offset.x;
        CurrentCamera.Pos.y += 2.0f * cam_offset.y;
        CurrentCamera.Pos.z += 2.0f * cam_offset.z;
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
        _RCS_STAGE("calling render_camera (RIGHT)");
        if( !render_camera() )
          return false;
        _RCS_STAGE("post render_camera (RIGHT)");
#if defined(__3DS__) && defined(RENDERER_C3D)
	if(render_info.stereo_mode == STEREO_MODE_3DS)
	{
		extern void pglTransferEye(unsigned int eye);
		_RCS_STAGE("pglTransferEye(RIGHT)");
		pglTransferEye(GFX_RIGHT);
		_RCS_STAGE("post pglTransferEye(RIGHT)");
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
	CurrentCamera.Viewport.Height = viewport.Height;
	CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / (float)2.0;
	CurrentCamera.Viewport.Width = viewport.Width;
	CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / (float)2.0;
	CurrentCamera.Viewport.Y = viewport.Y;
	CurrentCamera.Viewport.X = viewport.X;
	if(render_info.stereo_mode == STEREO_MODE_COLOR)
        	render_set_filter( 1, 1, 1 );
	_RCS_STAGE("pre FSSetViewPort");
	if (!FSSetViewPort(&viewport))
	{
	#ifdef DEBUG_VIEWPORT
	    SetViewportError( "RenderCurrentCamera2", &viewport );
	#else
	    Msg("SetViewport failed.\n%s", render_error_description(0));
	#endif
	    return false;
	}
	_RCS_STAGE("exit OK");
	#undef _RCS_STAGE
	return true;
}

#if defined(__3DS__) && defined(RENDERER_C3D)
extern bool pglHudBeginBottom(void);
extern void pglHudEndBottom(void);
extern void pglClearBottomScreen(void);
extern void ScrPolySetRouteBottom(bool b);
extern void ScrPolySetDisplayMode(int mode);
#endif

void DrawMainGameMenu(void)
{
    if( CurrentMenu && CurrentMenuItem )
    {
      MenuDraw( CurrentMenu );
      MenuItemDrawCursor( CurrentMenuItem );
    }
#if defined(__3DS__) && defined(RENDERER_C3D)
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
    DrawSimplePanel();
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
		int16_t saved_cam_rendering = CameraRendering;
		BYTE saved_current_cam_view = Current_Camera_View;
		float main_fov;

		TempMissileCam = Current_Camera_View;
		Current_Camera_View = INVALID_CAMERA_VIEW;
		if( ActiveRemoteCamera )
		{
			CameraRendering = CAMRENDERING_Pip;
			CurrentCamera.enable = 1;
			CurrentCamera.GroupImIn = ActiveRemoteCamera->Group;
			CurrentCamera.Mat = ActiveRemoteCamera->Mat;
			CurrentCamera.InvMat = ActiveRemoteCamera->InvMat;
			CurrentCamera.Pos = ActiveRemoteCamera->Pos;
		}
		else
		{
			CameraRendering = CAMRENDERING_Missile;
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

		if( RenderCurrentCamera() != true )
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

#if defined(__3DS__) && defined(RENDERER_C3D)
	/* [3DS citro3d] Two-pass screen-poly drain:
	 *   Pass 1: mode=1, current (stereo top) target — draws everything
	 *           WITHOUT SCRFLAG_BottomHUD (in-world overlays: aim reticle,
	 *           enemy markers, damage flash).
	 *   Pass 2: mode=2, bottom target — draws only polys WITH
	 *           SCRFLAG_BottomHUD (gameplay text HUD tagged inside
	 *           DrawSimplePanel).
	 * Mode=0 restored so Display* runs outside this path (title, crate
	 * menu) behave as before. pglHudBeginBottom is a no-op on the second
	 * eye pass; the bottom screen is mono and doesn't need a second copy. */
	/* FPS overlay — queue an FPS-counter text poly with the bottom-HUD
	 * route flag so RenderMainCamera2dPolys's pass-2 (mode=2) picks it
	 * up. Bottom screen is 320×240, so position in the top-right
	 * corner clear of the existing HUD elements. */
	{
		extern void ScrPolySetRouteBottom(bool b);
		extern int  forsaken_get_fps(void);
		extern int  Print4x5Text(char *Text, int x, int y, int color);
		char fps_buf[24];
		snprintf(fps_buf, sizeof(fps_buf), "FPS:%d", forsaken_get_fps());
		ScrPolySetRouteBottom(true);
		Print4x5Text(fps_buf, 250, 4, 1 /* white */);
		ScrPolySetRouteBottom(false);
	}

	ScrPolySetDisplayMode(1);
	RenderMainCamera2dPolys();
	if (pglHudBeginBottom())
	{
		ScrPolySetDisplayMode(2);
		RenderMainCamera2dPolys();
		pglHudEndBottom();
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
#ifdef __3DS__
	/* Per-level first-frame stage trace. Same pattern as MainGame's
	 * stage tracing: fires once per level transition. */
	static int _mgr_stage_last_level = -2;
	bool _mgr_stage_trace = false;
	{
		extern int16_t LevelNum;
		if (LevelNum != _mgr_stage_last_level) {
			_mgr_stage_last_level = LevelNum;
			_mgr_stage_trace = true;
		}
	}
	#define _MGR_STAGE(s) do { if (_mgr_stage_trace) { extern void trace(const char*); trace("MGR/" s); } } while(0)
#else
	#define _MGR_STAGE(s) ((void)0)
#endif

	_MGR_STAGE("entry");
	if (!FSBeginScene())
	{
		_MGR_STAGE("FSBeginScene FAILED");
		return false;
	}
	_MGR_STAGE("post-FSBeginScene");

	// show the stats screen
	if(ShowStats)
	{
		_MGR_STAGE("ShowStats branch");
		ScoreDisplay();
	}

   // show regular view
   else if( !FullRearView )
   {
      _MGR_STAGE("regular-view branch");
      CameraRendering = CAMRENDERING_Main;
      MainCamera.enable = 1;
      MainCamera.GroupImIn = Ships[Current_Camera_View].Object.Group;
      MainCamera.Mat = Ships[Current_Camera_View].Object.FinalMat;
      MainCamera.InvMat = Ships[Current_Camera_View].Object.FinalInvMat;
      MainCamera.Pos = Ships[Current_Camera_View].Object.Pos;
      MainCamera.Viewport = viewport;
      MainCamera.Proj = proj;

      CurrentCamera = MainCamera;
      _MGR_STAGE("post-camera-setup");

      HUDNames();
      _MGR_STAGE("post-HUDNames");
      DispHUDNames();
      _MGR_STAGE("post-DispHUDNames");
      DispUntriggeredNMEs();
      _MGR_STAGE("post-DispUntriggeredNMEs");

      CurrentCamera.UseLowestLOD = false;

#ifdef __3DS__
      /* [3DS] Drive stereo from the 3D slider.
       * Only the citro3d renderer supports stereo efficiently via display
       * list replay (one BSP pass, replayed for the second eye).  picaGL
       * would need two full render passes which halves the framerate. */
#ifdef RENDERER_C3D
      {
          extern float platform_get_3d_slider(void);
          float slider = platform_get_3d_slider();
          if (slider > 0.0f)
          {
              render_info.stereo_enabled = true;
              gfxSet3D(true);
              render_info.stereo_mode = STEREO_MODE_3DS;
              render_info.stereo_eye_sep = slider * 30.0f;
          }
          else
          {
              gfxSet3D(false);
              render_info.stereo_enabled = false;
          }
      }
#else
      render_info.stereo_enabled = false;
#endif
#endif

      _MGR_STAGE("pre-render-camera");
      if( render_info.stereo_enabled )
      {
				_MGR_STAGE("calling RenderCurrentCameraInStereo");
				if(!RenderCurrentCameraInStereo(RenderCurrentCameraWithMainGameMenu))
					return false;
				_MGR_STAGE("post-RenderCurrentCameraInStereo");
      }
      else // non stereo - normal rendering
	  	{
				_MGR_STAGE("calling RenderCurrentCameraWithMainGameMenu");
				if( RenderCurrentCameraWithMainGameMenu() != true ) // bjd
				{
					return false;
				}
				_MGR_STAGE("post-RenderCurrentCameraWithMainGameMenu");
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
        CameraRendering = CAMRENDERING_Rear;

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
        CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / (float)2.0;
        CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / (float)2.0;

/* bjd
        CurrentCamera.Viewport.dvMaxX = (float)D3DDivide(RENDERVAL(CurrentCamera.Viewport.dwWidth),
                           RENDERVAL(2 * CurrentCamera.Viewport.dvScaleX));
        CurrentCamera.Viewport.dvMaxY = (float)D3DDivide(RENDERVAL(CurrentCamera.Viewport.dwHeight),
                           RENDERVAL(2 * CurrentCamera.Viewport.dvScaleY));
*/        
        CurrentCamera.UseLowestLOD = true;

        if( RenderCurrentCamera() != true ) // bjd
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
        CameraRendering = CAMRENDERING_Pip;
        CurrentCamera.enable = 1;
        CurrentCamera.Viewport = viewport;
        CurrentCamera.Viewport.Width = viewport.Width / (float)2.0;
        CurrentCamera.Viewport.Height = viewport.Height / (float)2.0;
        CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.Width / (float)2.0;
        CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.Height / (float)2.0;
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
      CameraRendering = CAMRENDERING_Rear;
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

      if( RenderCurrentCamera() != true ) // bjd
          return false;
    }

  if( Our_CalculateFrameRate() != true)
      return false;

  /* do the target c omputer trick */
  if( TargetComputerOn )
  {
		set_alpha_states();
		DispTracker(); // bjd
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
bool MainGame( void ) // bjd
{
  int i;

#ifdef __3DS__
  /* Per-level first-MainGame stage trace. Fires the FIRST time
   * MainGame is called for each new level so a hang inside MainGame
   * can be pinpointed to a specific stage. Cheap (5 lines per level
   * transition, not per frame). */
  static int _mg_stage_last_level = -2;
  bool _mg_stage_trace = false;
  {
    extern int16_t LevelNum;
    if (LevelNum != _mg_stage_last_level) {
      _mg_stage_last_level = LevelNum;
      _mg_stage_trace = true;
    }
  }
  #define _MG_STAGE(s) do { if (_mg_stage_trace) { extern void trace(const char*); trace("MG/" s); } } while(0)
#else
  #define _MG_STAGE(s) ((void)0)
#endif

  _MG_STAGE("entry");
  MainGameDemoRoutines();
  _MG_STAGE("post-MainGameDemoRoutines");

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

  _MG_STAGE("pre-MainRoutines");
#ifdef __3DS__
  _3ds_mr_ctx = "pre-MainRoutines";
#endif
  MainRoutines();
#ifdef __3DS__
  _3ds_mr_ctx = "post-MainRoutines";
#endif
  _MG_STAGE("post-MainRoutines");

  if( MyGameStatus == STATUS_QuitCurrentGame )
    return true;

  memset( (void*) &IsGroupVisible[0] , 0 , MAXGROUPS * sizeof(u_int16_t) );
  cral += (framelag*2.0F);

  for( i = 0 ; i < MAX_SFX ; i++ )
    LastDistance[i] = 100000.0F;

  _MG_STAGE("pre-MainGameRender");
  if(!MainGameRender())
    return false;
  _MG_STAGE("post-MainGameRender");

#ifdef __3DS__
  _3ds_mr_ctx = "post-MainGameRender";
#endif
  MenuProcess(); // menu keys are processed here
  ProcessGameKeys(); // here is where we process F keys
#ifdef __3DS__
  _3ds_mr_ctx = "post-ProcessGameKeys";
#endif
  ScreenPolyProcess();
#ifdef __3DS__
  _3ds_mr_ctx = "post-ScreenPolyProcess";
#endif

#ifdef INSIDE_BSP
  Inside = PointInsideSkin( &Ships[WhoIAm].Object.Pos, Ships[WhoIAm].Object.Group );
#endif

/* Secondary routines called after rendering */

  SetFOVBasedOnShipSpeed();

  CheckLevelEnd();

  if(!PlayDemo)
    NetworkGameUpdate();

  _MG_STAGE("exit OK");
  #undef _MG_STAGE
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
	ShowGameStats( BOX_BG );
	if( Ships[ WhoIAm ].Timer < RESPAWN_TIMER )
		CenterPrint4x5Text( "Press any key to continue" , render_info.window_size.cy - (FontHeight*2) , GREEN );	
}

/* Display the Statistics in-game when key is pressed */
void ShowInGameStats()
{
	ShowGameStats( NO_BG );	// don't use BLT background
}

void PaintBackground( rect_t * box ) // pass NULL to black out all of the screen

{
	/* would simply blit the area defined by box on the backbuffer to black */
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

extern int GetPlayerByRank( int rank );

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
		
		if( Ships[ShipID].network_player != NULL )
		{
			sprintf( (char*) &buf[0] ,"IP: %s PORT: %d",
				Ships[ShipID].network_player->ip,
				Ships[ShipID].network_player->port);
			
			Print4x5Text( &buf[0] , left_offset, top_offset, GREEN );

			top_offset+=row_height;
			
			sprintf( (char*) &buf[0] ,"PING: %d LOSS: %d LOST: %d", 
				Ships[ShipID].network_player->ping,
				Ships[ShipID].network_player->packet_loss,
				Ships[ShipID].network_player->packets_lost);

			Print4x5Text( &buf[0] , left_offset, top_offset, GREEN );

			top_offset+=row_height;
			top_offset+=row_height; // blank line for spacing
		}
	}
}

void ShowGameStats( stats_mode_t mode )
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

	// paint black background or black box

	if( 0 && mode != NO_BG ) // TODO - could use flashbackgroundcolor function here if we want later
	{
		rect_t box;
		if( mode == BOX_BG )
		{
			int y_padding = FontHeight*2;

			// whole width
			box.left = 0;
			box.right = render_info.window_size.cx;

			//box.left = left_offset - x_padding;						// begging of first col
			//box.right = column[ ncols-1 ] + col_width + x_padding;	// end of last col

			box.top = top_offset - y_padding;							// begging of first row
			box.bottom = top_offset + total_height + y_padding;			// bottom of last row
		}
		PaintBackground(&box);
	}

	//
	// Kill Matrix
	//

	{
		char* title = "KILL MATRIX";
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
		char* title = "Player Stats";
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
			char* names[4] = {"TEAM", "OVERALL", "KILLS", "DEATHS"};
			int colors[4]  = {YELLOW,      GRAY,   GREEN,      RED};

			// NOT team game
			if(!TeamGame)
				names[0] = NULL;

			// start at left
			xpos = left_offset;

			// print the names
			for( col = 0; col < columns; col++ )
				if( names[col] != NULL )
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
	ShowGameStats( FULL_BG ); // use BLT background
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
#ifdef SCROLLING_MESSAGES
  int i;
#endif

   //Create the offscreen surface, by loading our bitmap.

  InitFont();
  //ChangeBackgroundColour( 0, 0, 0 );

#ifdef SCROLLING_MESSAGES
  // get random phrases...
  for (i = 0; i < Num_StatsMessage_Parts; i++)
  {
    StatsMessages[i].current_variation = Random_Range(StatsMessages[i].num_variations);
  }
#endif
  // process scores...
  ScoreSort();
  ScoreDisplaySfx = true;

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
 * batchers (polys.c / 2dpolys.c / screenpolys.c). They write
 * textureGroups[numTextureGroups++] for each face accumulated this
 * frame. Was an inline RENDEROBJECT.textureGroups[64] BSS field
 * before the textureGroups migration; now an explicit MAX_TEXTURE_GROUPS-
 * sized scratch attached to each RenderBuf at InitRenderBufs time. */
static TEXTUREGROUP s_renderbuf_groups[4][MAX_TEXTURE_GROUPS];

void InitRenderBufs(/* LPDIRECT3DDEVICE lpDev */) // bjd
{
	int i;
	DebugPrintf("InitRenderBufs\n");
	ReleaseRenderBufs();
	// just vertex data
	FSCreateDynamicVertexBuffer(&RenderBufs[0], 32767);
	FSCreateDynamicVertexBuffer(&RenderBufs[1], 32767);
	// index data as well
	FSCreateDynamicVertexBuffer(&RenderBufs[2], 32767);
	FSCreateDynamicIndexBuffer(&RenderBufs[2], 32767*3);
	// indexed pre-transformed (2d)
	FSCreateDynamic2dVertexBuffer(&RenderBufs[3], 32767);
	FSCreateDynamicIndexBuffer(&RenderBufs[3], 32767*3);
	for (i = 0; i < 4; i++)
		RenderBufs[i].textureGroups = s_renderbuf_groups[i];
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
#ifdef __3DS__
	/* Per-level first-call diagnostic. Fires once per level transition.
	 * IMPORTANT: do NOT add an eye-check here — stereo rendering
	 * alternates left/right per call, so any per-eye gate fires the
	 * trace on every single call. With ~18 stages × write() syscall
	 * to SD per fire, that's ~30 SD writes per frame = ~300 ms/frame
	 * = ~3 fps. (Diagnosed empirically: FPS log showed 0-1 fps with
	 * an eye-check; sweep traces showed 82 firings of stages that
	 * should have fired 3 times. The eye-check is what introduced
	 * the perf regression.) */
	static int _rc_stage_last_level = -2;
	bool _rc_stage_trace = false;
	{
		extern int16_t LevelNum;
		if (LevelNum != _rc_stage_last_level) {
			_rc_stage_last_level = LevelNum;
			_rc_stage_trace = true;
		}
	}
	#define _RC_STAGE(s) do { if (_rc_stage_trace) { extern void trace(const char*); trace("RC/" s); } } while(0)
#else
	#define _RC_STAGE(s) ((void)0)
#endif
	_RC_STAGE("entry");
	int16_t Count;
	VISGROUP  *g;
	u_int16_t  group;
//	float R, G, B;
	NumOfTransExe = 0;
	Build_View();
	_RC_STAGE("post-Build_View");
	CurrentCamera.View = view;
	if (!FSSetView(&view))
	{
		return false;
	}
	_RC_STAGE("post-FSSetView");
    if (!FSSetViewPort(&CurrentCamera.Viewport)) {
#ifdef DEBUG_VIEWPORT
    SetViewportError( "RenderCurrentCamera1", &CurrentCamera.Viewport );
#else
        Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
        return false;
    }
	_RC_STAGE("post-FSSetViewPort");

  // Ship Model Enable/Disable
  SetShipsVisibleFlag();
  _RC_STAGE("post-SetShipsVisibleFlag");

  // find visible groups
  FindVisible( &CurrentCamera, &Mloadheader );
  _RC_STAGE("post-FindVisible");

  BuildVisibleLightList( CurrentCamera.GroupImIn );
  _RC_STAGE("post-BuildVisibleLightList");

#if defined(RENDERER_C3D) && defined(GPU_LIGHTING)
  /* Pack the camera's visible XLight list into the shader uniform
   * once per pass. No per-group filter — see c3d_upload_xlights for
   * why occlusion-aware filtering isn't tractable on PICA200. */
  { extern void c3d_upload_xlights(void); c3d_upload_xlights(); }
  _RC_STAGE("post-c3d_upload_xlights");
#endif

  UpdateBGObjectsClipGroup( &CurrentCamera );
  UpdateEnemiesClipGroup( &CurrentCamera );
  _RC_STAGE("post-UpdateClipGroups");

  /*
  if( CurrentCamera.GroupImIn != (u_int16_t) -1 )
  {
    for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
    {
      if( Mloadheader.Group[ g->group ].BGClear_Flag )
      {
        if ( Mloadheader.Group[ g->group ].BGClear_Flag & 0x80 )
        {
          R = Mloadheader.Group[ g->group ].BGClear_Red;
          G = Mloadheader.Group[ g->group ].BGClear_Green;
          B = Mloadheader.Group[ g->group ].BGClear_Blue;
          ChangeBackgroundColour( R, G, B );
        }
        break;
      }
    }
  }
  */

  if (ClearBuffers() != true )
  {
    return false;
  }
  _RC_STAGE("post-ClearBuffers");
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
	_RC_STAGE("post-DisplayBackground");

	// reset all the normal execute status flags...
	if( WhiteOut == 0.0F)
	{
		set_normal_states();
	}

/*===================================================================
  Display Non Group Clipped Non Faceme Transluecent Polys
===================================================================*/
    if( !DisplaySolidGroupUnclippedPolys( &RenderBufs[ 2 ] ) ) // bjd
        return false;
    _RC_STAGE("post-DisplaySolidGroupUnclippedPolys");

#ifdef SHADOWTEST
    if( !DisplaySolidGroupUnclippedTriangles( RenderBufs[ 0 ], lpDev, lpView ) )
        return false;
#endif

  /* Per-group draw loop. Trace iteration count so an infinite loop or
   * hang on a specific group is visible. Counter resets per-call. */
#ifdef __3DS__
  int _rc_group_iter = 0;
#endif
  _RC_STAGE("entering opaque-draw loop");
  // display clipped opaque objects
  for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
  {
    group = g->group;
#ifdef __3DS__
    if (_rc_stage_trace && _rc_group_iter < 8) {
      char _b[64]; snprintf(_b,sizeof(_b),"RC/opaque-loop iter=%d group=%d", _rc_group_iter, (int)group);
      extern void trace(const char*); trace(_b);
    }
    _rc_group_iter++;
#endif

    // Do the Background animation for that group.....
    BackGroundTextureAnimation( &Mloadheader , group );

#ifdef __3DS__
    /* Per-group sub-stage trace. Fires only on the FIRST few iterations
     * of the FIRST per-level call so we can pinpoint the exact sub-call
     * that hangs without spamming the trace at 60 fps. */
    bool _rc_substage_trace = (_rc_stage_trace && _rc_group_iter <= 3);
    #define _RC_SUBSTAGE(s) do { if (_rc_substage_trace) { extern void trace(const char*); \
        char _b[80]; snprintf(_b,sizeof(_b),"RC/group=%d %s",(int)group,s); trace(_b); } } while(0)
#else
    #define _RC_SUBSTAGE(s) ((void)0)
#endif
    _RC_SUBSTAGE("post-BackGroundTextureAnimation");

#ifdef CLIP_LINES
    ClipGroup( &CurrentCamera, group );
#else
    ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );
#endif
    _RC_SUBSTAGE("post-ClipGroup-1");

	ExecuteLines( group, &RenderBufs[ 0 ] );
    _RC_SUBSTAGE("post-ExecuteLines");

    ClipGroup( &CurrentCamera, group );
    _RC_SUBSTAGE("post-ClipGroup-2");

    _RC_SUBSTAGE("calling ModelDisp");
      if( !ModelDisp( group, /*lpDev,*/ &ModelNames[0] ) ) // bjd
        return false;
    _RC_SUBSTAGE("post-ModelDisp");

/*===================================================================
  Display Group Clipped Non Faceme Transluecent Polys
===================================================================*/

    _RC_SUBSTAGE("calling DisplaySolidGroupClippedPolys");
  if( !DisplaySolidGroupClippedPolys( &RenderBufs[ 2 ], group ) ) // bjd
    return false;
    _RC_SUBSTAGE("post-DisplaySolidGroupClippedPolys");
#ifdef SHADOWTEST
  if( !DisplaySolidGroupClippedTriangles( RenderBufs[ 1 ], group, lpDev, lpView ) )
    return false;
#endif
    #undef _RC_SUBSTAGE
  }
  _RC_STAGE("opaque-loop EXIT");

  ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );
  _RC_STAGE("post-ClipGroup-cam");

	// set all the Translucent execute status flags...
  	set_alpha_states();
  _RC_STAGE("post-set_alpha_states-1");


/*===================================================================
  Display Skin & BSP Node Info
===================================================================*/
  if( ShowSkin || OldNodeCube || NodeCube || ShowTrigZones || ShowColZones || ShowEFZones || ShowTeleports )
  {
	if( OldNodeCube || NodeCube )
		ClearZBuffer();

	for( Count = 0; Count < MAXGROUPS; Count++ )
	{
//		if( Skin_Execs[ Count ] != NULL )
		{
			if (!draw_object(&Skin_Execs[ Count ]))
			{
				return false;
			}
		}
	}

#if 0
    for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
    {
		group = g->group;
		ClipGroup( &CurrentCamera, group );
		ExecuteLines( group, &RenderBufs[ 0 ] );
    }
#endif

  }

		set_alpha_states();
  _RC_STAGE("post-set_alpha_states-2");

  /* Translucent draw loop. Trace each iteration's group ID for the
   * first ~3 iterations on the first per-level call so a hang in
   * GroupWaterProcessDisplay / DisplayGroupClippedPolys / etc. on a
   * specific group is visible. */
#ifdef __3DS__
  int _rc_trans_iter = 0;
#endif
  _RC_STAGE("entering translucent-draw loop");
  // display clipped translucencies
  for ( g = CurrentCamera.visible.first_visible; g; g = g->next_visible )
  {
    group = g->group;

#ifdef __3DS__
    bool _rc_trans_substage_trace = (_rc_stage_trace && _rc_trans_iter <= 3);
    if (_rc_trans_substage_trace) {
      char _b[80]; snprintf(_b,sizeof(_b),"RC/trans-loop iter=%d group=%d",_rc_trans_iter,(int)group);
      extern void trace(const char*); trace(_b);
    }
    _rc_trans_iter++;
    #define _RC_TRANS_SUBSTAGE(s) do { if (_rc_trans_substage_trace) { extern void trace(const char*); \
        char _b[80]; snprintf(_b,sizeof(_b),"RC/trans-group=%d %s",(int)group,s); trace(_b); } } while(0)
#else
    #define _RC_TRANS_SUBSTAGE(s) ((void)0)
#endif

    ClipGroup( &CurrentCamera, group );
    _RC_TRANS_SUBSTAGE("post-ClipGroup");
    GroupWaterProcessDisplay( group );
    _RC_TRANS_SUBSTAGE("post-GroupWaterProcessDisplay");



/*===================================================================
  Display Group Clipped Non Faceme Transluecent Polys
===================================================================*/

  if( !DisplayGroupClippedPolys( &RenderBufs[ 2 ], group ) ) // bjd
    return false;
    _RC_TRANS_SUBSTAGE("post-DisplayGroupClippedPolys");

#ifdef SHADOWTEST
  if( !DisplayGroupClippedTriangles( RenderBufs[ 1 ], group, lpDev, lpView ) )
    return false;
#endif

/*===================================================================
Display Group Clipped Faceme Transluecent Polys
===================================================================*/

  if( !DisplayGroupClippedFmPolys( &RenderBufs[ 2 ], group ) ) // bjd
      return false;
    _RC_TRANS_SUBSTAGE("post-DisplayGroupClippedFmPolys");

  ExecuteTransExe( group );
    _RC_TRANS_SUBSTAGE("post-ExecuteTransExe");
    #undef _RC_TRANS_SUBSTAGE
  }
  _RC_STAGE("translucent-loop EXIT");


  ClipGroup( &CurrentCamera, CurrentCamera.GroupImIn );
  _RC_STAGE("post-ClipGroup-cam-2");


/*===================================================================
  Display Non Group Clipped Faceme Transluecent Polys
===================================================================*/

    if( !DisplayGroupUnclippedFmPolys( &RenderBufs[ 2 ] ) ) // bjd
        return false;
  _RC_STAGE("post-DisplayGroupUnclippedFmPolys");

/*===================================================================
  Display Non Group Clipped Non Faceme Transluecent Polys
===================================================================*/
    if( !DisplayGroupUnclippedPolys( &RenderBufs[ 2 ] ) ) // bjd
        return false;
  _RC_STAGE("post-DisplayGroupUnclippedPolys");
#ifdef SHADOWTEST
    if( !DisplayGroupUnclippedTriangles( RenderBufs[ 0 ], lpDev, lpView ) )
      return false;
#endif
  
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
//		if( Portal_Execs[ Count ] != NULL )
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
  

extern  int   NumOfVertsConsidered;
extern  int   NumOfVertsTouched;

/*===================================================================
  Procedure :  Our Calculate frame rate...
  Input   :
  Output    : bool true/false
===================================================================*/

px_timer_t our_timer;
float our_count = 0;
extern bool ShowFrameRate;
extern bool ShowInfo;
extern	u_int16_t		NumGroupsVisible;
extern u_int16_t	GroupImIn;

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
			(GroupImIn == (u_int16_t) -1) ? "(outside)" : Mloadheader.Group[GroupImIn].name );
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

#ifdef DEMO_SUPPORT
    // some stupid place for a demo calculation
	if( MyGameStatus == STATUS_PlayingDemo )
	{
		QueryPerformanceFrequency((LARGE_INTEGER *) &Freq);
		QueryPerformanceCounter((LARGE_INTEGER *) &DemoEndedTime);
		TimeDiff = DemoEndedTime - DemoStartedTime;
		DemoTotalTime = ( (float) TimeDiff / (float) Freq );
		DemoAvgFps = DemoGameLoops / DemoTotalTime;
	}
#endif

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

	if ( PlayDemo )
		PlayDemo = false;

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
	  //DebugPrintf("WARNING: real_framelag=%d\n",real_framelag);
		SDL_Delay(10);
	}

  framelag = real_framelag * 71.0F;

  // debugging
  //if (TimeWorst == 0.0 || seconds > TimeWorst) TimeWorst = seconds;
  //if (TimeBest  == 0.0 || seconds < TimeBest)  TimeBest  = seconds;
  //DebugPrintf("Loop Time: seconds=%5f - worst:%5f - best:%5f\n",seconds,TimeWorst,TimeBest);
  
  //  who knows why
  if( CurrentMenu && (MyGameStatus == STATUS_SinglePlayer) )
    framelag = 0.0F;

}


/*===================================================================
  Procedure : Disp Tracker
  Output    : bool        true/false
===================================================================*/
bool DispTracker( void ) // bjd
{
	u_int16_t      i;
	XYRECT     dummy;
	render_viewport_t newviewport;
	float       screen_width, screen_height;
	VECTOR      TempVector;
	MATRIX      TempMatrix;
	float       TargetDistance;
	float       Red, Green, Blue, Trans;
	VECTOR      ShipDir, TargetDir;
	float       Cos;

/*
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

    newviewport.X = 0;	
	newviewport.Y = 0;
    newviewport.Width = ( render_info.window_size.cx / 3 ) & -2;
    newviewport.Height = (u_int32_t) ( (float) newviewport.Width * pixel_aspect_ratio );
    newviewport.ScaleX = newviewport.Width / (float)2.0;
    newviewport.ScaleY = newviewport.Height / (float)2.0;

#if 0 //bjd
    newviewport.dvMaxX = (float)D3DDivide(RENDERVAL(newviewport.dwWidth),
                                       RENDERVAL(2 * newviewport.dvScaleX));
    newviewport.dvMaxY = (float)D3DDivide(RENDERVAL(newviewport.dwHeight),
                                       RENDERVAL(2 * newviewport.dvScaleY));
#endif

	if (!FSSetViewPort(&newviewport))
		return false;

	viewplane_distance = (float) ( newviewport.Width / ( 2 * tanf( DEG2RAD( normal_fov ) * 0.5 ) ) );
	panelproj._11 = 2 * viewplane_distance / newviewport.Width;
	panelproj._22 = 2 * viewplane_distance / ( newviewport.Height / pixel_aspect_ratio );

	if (!FSSetProjection(&panelproj))
	{
		return false;
	}

	dummy.x1 = newviewport.X;
	dummy.x2 = newviewport.X + newviewport.Width;
	dummy.y1 = newviewport.Y;
	dummy.y2 = newviewport.Y + newviewport.Height;

	if (FSClearDepth(&dummy))
		return false;
*/

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

	if (ReallyExecuteMxloadHeader( &ModelHeaders[MODEL_Tracker], (u_int16_t) -1 ) != true )
		return false;

#if 0
  i = FindClosestPickup();
#else
  i = FindClosestShip();
#endif
  
  if( i != (u_int16_t) -1 )
  {
    Red = 255.0F;
    Green = 255.0F;
    Blue = 255.0F;
    Trans = 255.0F;
#if 0
    TempVector.x = ( ( Pickups[ i ].Pos.x - Ships[ WhoIAm ].Object.Pos.x ) / 50.0F );
    TempVector.y = ( ( Pickups[ i ].Pos.y - Ships[ WhoIAm ].Object.Pos.y ) / 50.0F );
    TempVector.z = ( ( Pickups[ i ].Pos.z - Ships[ WhoIAm ].Object.Pos.z ) / 50.0F );
#else
    TempVector.x = ( ( Ships[ i ].Object.Pos.x - Ships[ WhoIAm ].Object.Pos.x ) / 50.0F );
    TempVector.y = ( ( Ships[ i ].Object.Pos.y - Ships[ WhoIAm ].Object.Pos.y ) / 50.0F );
    TempVector.z = ( ( Ships[ i ].Object.Pos.z - Ships[ WhoIAm ].Object.Pos.z ) / 50.0F );
#endif
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

    if (ReallyExecuteMxloadHeader( &ModelHeaders[MODEL_Ping], (u_int16_t) -1 ) != true )
      return false;
  }

/*
	if (!FSSetViewPort(&viewport))
		return false;

	if (!FSSetProjection(&proj))
	{
		return false;
	}
*/

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
/*
bool ChangeBackgroundColour( float R, float G, float B )
{
	return true;
}
*/

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
#ifdef __3DS__
	/* Per-status one-shot diagnostic trace. Fires on the FIRST call to
	 * PIVS for each distinct Status value so we can pinpoint a hang
	 * without spamming on every IV phase. */
	{ static int _pivs_last_status = -1;
	  if ((int)Status != _pivs_last_status) {
	    _pivs_last_status = (int)Status;
	    extern void trace(const char*); char _b[64];
	    snprintf(_b, sizeof(_b), "PIVS/enter status=%u", (unsigned)Status);
	    trace(_b);
	  }
	}
	bool _pivs_trace_now = (Status == 33);
	#define _PIVS_STAGE(s) do { if (_pivs_trace_now) { extern void trace(const char*); trace("PIVS/" s); } } while(0)
#else
	#define _PIVS_STAGE(s) ((void)0)
#endif
	ZERO_STACK_MEM(ro);
	ro.textureGroups = ro_groups;
	_PIVS_STAGE("pre-FSCreate");
	{
		bool _vok = FSCreateDynamic2dVertexBuffer(&ro, 32767);
		bool _iok = FSCreateIndexBuffer(&ro, 32767*3);
#if defined(VERBOSE_TRACE) && defined(__3DS__)
		{ extern void trace(const char*); extern int _vt_flip_remaining;
		  if (_vt_flip_remaining > 0) {
		    char _b[64];
		    snprintf(_b, sizeof(_b), "PIVS: alloc v=%d i=%d", (int)_vok, (int)_iok);
		    trace(_b);
		  }
		}
#endif
#ifdef __3DS__
		if (_pivs_trace_now) {
			extern void trace(const char*); char _b[64];
			snprintf(_b, sizeof(_b), "PIVS/post-FSCreate v=%d i=%d", (int)_vok, (int)_iok);
			trace(_b);
		}
#endif
		/* If either allocation failed (linear heap exhausted/fragmented),
		 * skip the draw rather than feeding NULL buffers into the renderer. */
		if (!_vok || !_iok) {
			_PIVS_STAGE("alloc-fail early-return");
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
		extern void InitScrPolys(void);
		InitScrPolys();
	}
#endif
	_PIVS_STAGE("pre-DisplayNonSolidScrPolys");
	DisplayNonSolidScrPolys(&ro);
	_PIVS_STAGE("post-DisplayNonSolidScrPolys");
	DisplaySolidScrPolys(&ro);
	_PIVS_STAGE("post-DisplaySolidScrPolys");
	FSReleaseRenderObject(&ro);
	_PIVS_STAGE("post-FSReleaseRenderObject");
	render_flip(&render_info);
	_PIVS_STAGE("exit OK");
	#undef _PIVS_STAGE
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
  //ChangeBackgroundColour( 0, 0, 0 );

  HowManySecrets( &TotalSecrets , &Secrets );

  return true;
}

/*===================================================================
  Procedure :   Stats Display...
  Input   :   nothing...
  Output    :   bool true/false
===================================================================*/
bool StatsDisplay()
{
  rect_t    src;
  rect_t    dest;
  point_t	destp;
  int x,y;

  char buf[256];

  //  Blt Background
  src.top = 0;
  src.left = 0;
  src.right = render_info.window_size.cx;
  src.bottom = render_info.window_size.cy;
  x = 0;
  y = 0;

  dest.top = 0;
  dest.bottom = render_info.window_size.cy;
  dest.left = 0;
  dest.right = render_info.window_size.cx;
      
  destp.x = dest.left;
  destp.y = dest.top;

  if( !GameCompleted )
  {
    if( Secrets == 1 )
    {
      sprintf( (char*) &buf ,"%d Secret found out of %d", Secrets , TotalSecrets );
    }else{
      sprintf( (char*) &buf ,"%d Secrets found out of %d", Secrets , TotalSecrets );
    }
    CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy >> 1 ) - (FontHeight*2) , 2 );
    if( NumKilledEnemies == 1 )
    {
      sprintf( (char*) &buf ,"%d Enemy killed out of %d", NumKilledEnemies , NumInitEnemies );
    }else{
      sprintf( (char*) &buf ,"%d Enemies killed out of %d", NumKilledEnemies , NumInitEnemies );
    }
    CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy >> 1 ) + (FontHeight*0) , 2 );
    if( CrystalsFound == 1 )
    {
      sprintf( (char*) &buf ,"%d Crystal found so far", CrystalsFound );
    }else{
      sprintf( (char*) &buf ,"%d Crystals found so far", CrystalsFound );
    }
    CenterPrint4x5Text( &buf[0] , (render_info.window_size.cy >> 1 ) + (FontHeight*2) , 2 );
  }else{
    CenterPrint4x5Text( "Congratulations" , (render_info.window_size.cy >> 1 ) - (FontHeight*2) , 2 );
    CenterPrint4x5Text( "Demo Complete" , (render_info.window_size.cy >> 1 ) + (FontHeight*0) , 2 );
    if( DifficultyLevel != 3 )
    {
      CenterPrint4x5Text( "Now Try a Harder Difficulty Setting" , (render_info.window_size.cy >> 1 ) + (FontHeight*2) , 2 );
    }else{
      CenterPrint4x5Text( "Try the real Game for a harder challenge" , (render_info.window_size.cy >> 1 ) + (FontHeight*2) , 2 );
    }
  }
  
  CenterPrint4x5Text( "Press Space to continue" , render_info.window_size.cy - (FontHeight*2) , 2 );
//  Our_CalculateFrameRate();


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
  Procedure :   Get Bit Shift required to make first bit of
        :   mask bit 0
  Input   :   int32_t Mask
  Output    :   int16_t Shift Value
===================================================================*/
int16_t GetBitShift( int32_t Mask )
{
  int16_t Count;

  if( Mask )
  {
    Count = 0;

    while( !( Mask & 1 ) )
    {
      Count++;
      Mask >>= 1;
    }

    return( Count );
  }

  return( 0 );
}


/*===================================================================
  Procedure :   Render Snapshot
  Input   :   Nothing
  Output    :   Nothing
===================================================================*/
void RenderSnapshot( void )
{
#if 0 // bjd - CHECK
//  LPDIRECT3DDEVICE lpDev = render_info.lpD3DDevice;
    render_viewport_t View = render_info.D3DViewport;

//bjd  lpDev->lpVtbl->BeginScene(lpDev);

  CurrentCamera.enable = 1;
  CurrentCamera.UseLowestLOD = true;
  CurrentCamera.GroupImIn = Ships[Current_Camera_View].Object.Group;  
  CurrentCamera.Mat = Ships[Current_Camera_View].Object.FinalMat; 
  CurrentCamera.InvMat = Ships[Current_Camera_View].Object.FinalInvMat; 
  CurrentCamera.Pos = Ships[Current_Camera_View].Object.Pos;  
  CurrentCamera.Viewport = viewport;  
  CurrentCamera.Proj = proj;  
  
  CurrentCamera.Viewport.X = 0;
  CurrentCamera.Viewport.Y = 0;
  CurrentCamera.Viewport.Width = 128;
  CurrentCamera.Viewport.Height = 128;
  CurrentCamera.Viewport.ScaleX = CurrentCamera.Viewport.dwWidth / (float)2.0;
  CurrentCamera.Viewport.ScaleY = CurrentCamera.Viewport.dwHeight / (float)2.0;

/* bjd
  CurrentCamera.Viewport.dvMaxX = (float)D3DDivide(RENDERVAL(CurrentCamera.Viewport.dwWidth),
                     RENDERVAL(2 * CurrentCamera.Viewport.dvScaleX));
  CurrentCamera.Viewport.dvMaxY = (float)D3DDivide(RENDERVAL(CurrentCamera.Viewport.dwHeight),
                     RENDERVAL(2 * CurrentCamera.Viewport.dvScaleY));
*/

  CurrentCamera.UseLowestLOD = true;
  if( RenderCurrentCamera() != true ) // bjd
    return;
#endif
//bjd  lpDev->lpVtbl->EndScene(lpDev);
}

/*===================================================================
  Procedure :   Save PPM Bitmap
  Input   :   int8_t  * Filename
        :   int8_t  * Screen Pointer
        :   u_int32_t    Width
        :   u_int32_t    Height
        :   u_int32_t    Bytes Per Pixel
        :   u_int32_t    LineSize
        :   u_int32_t    RedMask
        :   u_int32_t    GreenMask
        :   u_int32_t    BlueMask
        :   u_int32_t    x1, y1, x2 ,y2 ( Portion of Screen )
  Output    :   bool    True/False
===================================================================*/
bool SavePPM( u_int8_t * Filename, u_int8_t * ScreenPtr, u_int32_t Width, u_int32_t Height, u_int32_t BytesPerPixel,
        u_int32_t LineSize, u_int32_t RedMask, u_int32_t GreenMask, u_int32_t BlueMask,
        u_int32_t x1, u_int32_t y1, u_int32_t x2, u_int32_t y2 )
{
  u_int32_t    XCount, YCount;
  u_int32_t    RedMul, GreenMul, BlueMul;
  u_int8_t   Red, Green, Blue;
  int16_t   RedShift, GreenShift, BlueShift;
  u_int8_t * LinePtr;
  int8_t    Header[ 256 ];
  FILE  * fp;
  int16_t   i;
  u_int32_t    Val;
  u_int32_t    Xoff, Yoff;
  u_int32_t    Xsize, Ysize;

  if( ( x1 == x2 ) && ( y1 == y2 ) )
  {
    x1 = 0;
    y1 = 0;
    x2 = Width;
    y2 = Height;
  }

  Xoff = x1;
  Yoff = y1;
  Xsize = ( x2 - x1 );
  Ysize = ( y2 - y1 );

  fp = file_open( Filename, "wb" );

  if( fp != NULL )
  {
    sprintf( &Header[ 0 ], "P6%c#Forsaken (c)1998%c%d %d%c255%c", 10, 10, (int)Xsize, (int)Ysize, 10, 10 );

    i = 0;
    while( Header[ i ] )
    {
      fwrite( &Header[ i ], sizeof( int8_t ), 1, fp );
      i++;
    }

    RedShift = GetBitShift( RedMask );
    GreenShift = GetBitShift( GreenMask );
    BlueShift = GetBitShift( BlueMask );

    RedMul = ( 256 / ( ( RedMask >> RedShift ) + 1 ) );
    GreenMul = ( 256 / ( ( GreenMask >> GreenShift ) + 1 ) );
    BlueMul = ( 256 / ( ( BlueMask >> BlueShift ) + 1 ) );

    ScreenPtr += ( Yoff * LineSize );

    for( YCount = 0; YCount < Ysize; YCount++ )
    {
      LinePtr = ( ScreenPtr + Xoff );

      for( XCount = 0; XCount < Xsize; XCount++ )
      {
        Val = 0;

        switch( BytesPerPixel )
        {
          case 1:
            Val = (u_int32_t) LinePtr[0];
            break;
          case 2:
            Val = (u_int32_t) ( (u_int32_t) LinePtr[0] ) |
                   ( ( (u_int32_t) LinePtr[1] ) << 8 );
            break;
          case 3:
            Val = (u_int32_t) ( (u_int32_t) LinePtr[0] ) |
                   ( ( (u_int32_t) LinePtr[1] ) << 8 ) |
                   ( ( (u_int32_t) LinePtr[2] ) << 16 );
            break;
        }
        LinePtr += BytesPerPixel;

        Red   = (u_int8_t) ( ( ( Val & RedMask ) >> RedShift ) * RedMul );
        Green = (u_int8_t) ( ( ( Val & GreenMask ) >> GreenShift ) * GreenMul );
        Blue  = (u_int8_t) ( ( ( Val & BlueMask ) >> BlueShift ) * BlueMul );

        fwrite( &Red, sizeof( Red ), 1, fp );
        fwrite( &Green, sizeof( Green ), 1, fp );
        fwrite( &Blue, sizeof( Blue ), 1, fp );
      }

      ScreenPtr += LineSize;
    }

    fclose( fp );
  }
  else
  {
    DebugPrintf( "Failed to create %s\n", Filename );
    return( false );
  }

  return( true );
}

/*===================================================================
  Procedure :   Save SnapShot screen for saved game
  Input   :   int8_t  * Filename
  Output    :   bool    True/False
===================================================================*/
bool SaveSnapShot( int8_t * Filename )
{
	return true;
#if 0 // bjd
  HRESULT     hr;
  DDSURFACEDESC SurfaceDesc;

  FreeAllLastAFrameScrPolys();
  RenderSnapshot();

  memset( &SurfaceDesc, 0, sizeof( SurfaceDesc ) );
  SurfaceDesc.dwSize = sizeof( SurfaceDesc );

  hr = render_info.lpBackBuffer->lpVtbl->Lock( render_info.lpBackBuffer, NULL, &SurfaceDesc,
        DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_READONLY, NULL );

  if ( hr != DD_OK )
  {
    switch( hr )
    {
      case DDERR_INVALIDOBJECT:
        DebugPrintf( "Error Locking Surface ( Invalid Object )\n" );
        break;

      case DDERR_INVALIDPARAMS:
        DebugPrintf( "Error Locking Surface ( Invalid Params )\n" );
        break;

      case DDERR_OUTOFMEMORY:
        DebugPrintf( "Error Locking Surface ( Out of Memory )\n" );
        break;

      case DDERR_SURFACEBUSY:
        DebugPrintf( "Error Locking Surface ( Surface Busy )\n" );
        break;

      case DDERR_SURFACELOST:
        DebugPrintf( "Error Locking Surface ( Surface Lost )\n" );
        break;

      case DDERR_WASSTILLDRAWING:
        DebugPrintf( "Error Locking Surface ( Was Still Drawing )\n" );
        break;
    }
    return( false );
  }

  SavePPM( Filename, SurfaceDesc.lpSurface, SurfaceDesc.dwWidth, SurfaceDesc.dwHeight,
        ( ( SurfaceDesc.ddpfPixelFormat.dwRGBBitCount + 7 )  / 8 ),
        SurfaceDesc.lPitch, SurfaceDesc.ddpfPixelFormat.dwRBitMask, SurfaceDesc.ddpfPixelFormat.dwGBitMask,
        SurfaceDesc.ddpfPixelFormat.dwBBitMask, 0, 0, 128, 128 );

  hr = render_info.lpBackBuffer->lpVtbl->Unlock( render_info.lpBackBuffer, NULL );
  if ( hr != DD_OK )
  {
    DebugPrintf( "Error Unlocking Surface\n" );
    return( false );
  }

  if( CurrentMenu ) MenuDraw( CurrentMenu );
  MainGame( /*render_info.lpD3DDevice,*/ render_info.lpD3DViewport ); // bjd

  return( true );
#endif
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

