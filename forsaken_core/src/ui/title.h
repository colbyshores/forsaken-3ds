#pragma once

/* title.h -- menu system for the front end and the in-game pause menus
 *
 * title.cpp owns the static MENU / MENUITEM trees (process lifetime), the
 * current menu (CurrentMenu, MenuChange / MenuBack) and the title-scene event
 * timers that drive camera pans and disc animations. Item behaviour is held in
 * function pointers: FuncSelect on activation, FuncDraw each frame; Variable
 * and Value point at the setting an item edits.
 *
 * A menu's Item[] list ends at the first entry with x == -1. Item coordinates
 * are VDU coordinates, not screen pixels. Timer Interval is in seconds; event
 * StartTime / EndTime and timer CurrentTime are normalised 0..1.
 */

#include "ui/localtitle.h"

#include "math/new3d.h"
#include "rendering/2dtextures.h"
#include "game/constants.h"
#include "net/networking.h"

constexpr int QUICKSTART_None = 0;
constexpr int QUICKSTART_Start = 1;
constexpr int QUICKSTART_Join = 2;
constexpr int QUICKSTART_Notify = 5;

#define ALLOWED_BIKETYPES	MAXBIKETYPES

constexpr int MAX_LEVEL_TEXT_LENGTH = 512;
constexpr int MAX_LEVEL_NAME_LENGTH = 128;
constexpr int MAX_SESSION_DESC_LENGTH = 64;

#define DEFAULT_PLAYER_NAME		"Player"

#define DEFAULT_LEVELS			"levels.dat"
#define SINGLEPLAYER_LEVELS		"mission.dat"
#define MULTIPLAYER_LEVELS		"battle.dat"
#define SPLASH_LEVELS			"splash.dat"

#define DEMO_LEVELS				MULTIPLAYER_LEVELS

constexpr int MAXMENULEVELS = 16;
constexpr int MAXMENUITEMS = 128;
constexpr int MAXLISTITEMS = 128;
constexpr int MAXVDULINES = 64;
constexpr int MAXVDULINELENGTH = 64;
constexpr int MAXTEXTITEMS = 128;

constexpr int ERROR_DONTUSE_MENUFUNCS = 0;
constexpr int ERROR_USE_MENUFUNCS = 1;
constexpr int ERROR_QUIT = 2;

constexpr int DIFF_Easy = 0;
constexpr int DIFF_Norm = 1;
constexpr int DIFF_Hard = 2;
constexpr int DIFF_Vhard = 3;

constexpr int CONTROL_Mouse = 0;
constexpr int CONTROL_Keyboard = 1;
constexpr int CONTROL_Joystick = 2;
constexpr int CONTROL_Gamepad = 3;

// order of this list must match GameTypeNameTable
constexpr int MAX_GAMETYPE = 5;
typedef enum
{
GAME_Normal,			// 0 free for all
GAME_FlagChase,			// 1
GAME_BountyHunt,		// 2
GAME_TeamBountyHunt,	// 3
GAME_Team,				// 4
GAME_CTF,				// 5
} game_t;

constexpr int GAMECOMPLETE_NotComplete = 0;
constexpr int GAMECOMPLETE_WithoutAllCrystals = 1; // never got to secret level because not all crystals were found ( standard end game sequence )
constexpr int GAMECOMPLETE_GotAllCrystalsSoFar = 2; // got all crystals so far ( will now go to secret level )
constexpr int GAMECOMPLETE_WithAllCrystals = 3; // with all crystals plus secret level crystal ( secret biker end game sequence )
constexpr int GAMECOMPLETE_WithAllCrystalsExceptLast = 4; // with all crystals except secret level crystal ( standard end game sequence )
constexpr int GAMECOMPLETE_Cheated = 5; // got to last but one level, but with cheating
constexpr int GAMECOMPLETE_AdditionalLevels = 6; // completed all levels ( not original levels )

constexpr float PAN_TIME = 1.0F;

typedef enum
{
	MENUSTATE_Select,
	MENUSTATE_Keydef,
	MENUSTATE_Keydef2,
	MENUSTATE_Slider,
	MENUSTATE_Slider2,
	MENUSTATE_List,
	MENUSTATE_Text,
	MENUSTATE_Text2,
	MENUSTATE_SelectBiker,
	MENUSTATE_SelectPlayer,
	MENUSTATE_WeaponOrder,
	MENUSTATE_SelectLevelQuick,
	MENUSTATE_SelectLevelSlow,
	MENUSTATE_DifficultySet,
	MENUSTATE_SelectSavedGame,
	MENUSTATE_SelectKeydef,
}MENUSTATE;

enum
{
	TEAM_SelectTeam,
  	TEAM_SelectPlayer,
  	TEAM_PlacePlayer,
};

// status of disc stack
enum
{
	MENU_AtStart,
	MENU_AtDiscs,
	MENU_AtLeftVDU,
	MENU_AtRightVDU,
};

// status of the discs
enum
{
	DISC_NOTHING,
	DISC_EXPAND,
	DISC_WIGGLE,
	DISC_ROTATE,
	DISC_RETRACT,
};

enum
{
	DISC_MODE_NONE,
	DISC_MODE_ALL,
	DISC_MODE_ONE,
};

// status of the camera
enum
{
	CAMERA_AtStart,
	CAMERA_AtDiscs,
	CAMERA_AtLeftVDU,
	CAMERA_AtRightVDU,
};

enum
{
	FONT_Small,
	FONT_Medium,
	FONT_Large,
	FONT_Slider,
};

constexpr int NUMSCREENFONTS = 15;

enum
{
	SLIDER_Normal,
	SLIDER_Value,
	SLIDER_Percent,
	SLIDER_Sensitivity,
	SLIDER_Time,
	SLIDER_Bike,
	SLIDER_Demo,
	SLIDER_User,
};

enum
{
	// this is the only font file we have
	FONT_512X384_Small,
	FONT_512X384_Medium,
	FONT_512X384_Large,
};

enum
{
	BIKER_INVALID,
	BIKER_TPAGE_1,
	BIKER_TPAGE_2,
};

enum
{
	WIPE_Done,
 	WIPE_In,
 	WIPE_Out,
	WIPE_WaitingToProcess,
};


#define	MAXTEXTMSG			512		// the number of chars in a message you can send.....
//	Strucures
constexpr int TEXTFLAG_Nothing = 0;
constexpr int TEXTFLAG_CentreX = 1; // centre text along x axis of bounding box
constexpr int TEXTFLAG_CentreY = 2; // centre text along y axis of bounding box
constexpr int TEXTFLAG_BottomY = 4; // justify text to bottom of box
constexpr int TEXTFLAG_Instant = 8; // text is displayed instantly
constexpr int TEXTFLAG_RightX = 16; // right justify
constexpr int TEXTFLAG_NotImplemented = 32; // display text dimmed (only works with screenpoly text, ie. 3dfx)
constexpr int TEXTFLAG_AutoSelect = 64; // automatically select this menu item if highlighted
constexpr int TEXTFLAG_RefreshList = 128; // not currently used (all lists are continually refreshed)
constexpr int TEXTFLAG_ForceFit = 256; // do not wrap text, truncate with ... if neccesary
constexpr int TEXTFLAG_SuppressHighlight = 512; // prevent highlight box from appearing around text
constexpr int TEXTFLAG_Unselectable = 1024; // do not allow this item to be selected
constexpr int TEXTFLAG_CheckForRefresh = 2048; // this text item may change - refresh if neccesary
constexpr int TEXTFLAG_ManualListHighlight = 4096; // I want to specify how a list is highlighted manually
											// ( default is pulsing when active, static when not active )
constexpr int TEXTFLAG_CentreYBunched = 8192; // text is bunched up in centre of bounding box
constexpr int TEXTFLAG_Pulse = 16384; // pulse text on and off
constexpr int TEXTFLAG_TeamItem = 32768; // team select item

constexpr int MAXNUMLINES = 64;
constexpr int MAXCHARSPERLINE = 128;
constexpr int MAXNUMOFVDUPOLYS = 512;
constexpr int MAXPOINTERS = 8;

constexpr int MAX_TEXT_LENGTH = 64;

typedef struct
{
	u_int8_t	*text;						// text string
	u_int8_t	oldtext[MAX_TEXT_LENGTH];	// used for checking if text has changed
	bool	highlight;					// item highlighted?
	int		highlighttype;				// highlight type?
	int		xmin;						// 
	int		ymin;						// bounding box for text
	int		xmax;						// (VDU co-ordinates)
	int		ymax;						// 
	int		font;						// font size (small, medium, large)
	int		flags;						// formatting info
	int		num_lines;					// number of lines once formatted
	float	length[MAXNUMLINES];		// length of formatted line (pixels)
	float	height[MAXNUMLINES];		// height of formatted line (pixels)
	int		startpoint[MAXNUMLINES];	// start character of line (in original text string)
	int		endpoint[MAXNUMLINES];		// end character of line

	int		currentchar[MAXNUMLINES];	// used for teletype mode
	float 	currentx[MAXNUMLINES];		// used for teletype mode
	bool	boxdone;					// used for teletype mode
	int		num_pointers_to_free;		
	void	*pointer[MAXPOINTERS];		

	float 	currenty;		// possibly replace with local variables?
	float	char1x;
	float	text_y_min;
	float	text_y_max;
	bool	force_fit;
	bool	finished[MAXNUMLINES];
}TEXTINFO;

typedef struct _MENU MENU;
typedef struct _MENUITEM MENUITEM;
typedef struct _PARAM PARAM;
typedef struct _STATUS STATUS;
typedef struct _TITLE_EVENT TITLE_EVENT;
typedef struct _TITLE_EVENT_TIMER TITLE_EVENT_TIMER;
typedef struct _TEXT TEXT;
typedef struct _SLIDER SLIDER;
typedef struct _LIST LIST;

typedef void (* MenuFunc)( MENU * );
typedef void (* MenuItemFunc)( MENUITEM * );
typedef void (* MenuStatusFunc)(int *);
typedef void (* EventFunc)(TITLE_EVENT *);
typedef void (* TimerFunc)(TITLE_EVENT_TIMER *);
typedef char (* TextTypeFunc)( TEXT *, char );
typedef const char *(* SliderUserFunc)( SLIDER * );
typedef bool (* SliderSetFunc)( SLIDER * );
typedef bool (* ListDeleteFunc) ( LIST *, int );
typedef void (* ListInfoFunc) ( LIST * );

// NOTE!!! CTF type is propagated to clients in 4 bits - cannot have more than 15 types!!
enum
{
	CTF_STANDARD,
	CTF_MUST_CARRY,
	CTF_NO_RETURN,
	CTF_CAN_CARRY,
	CTF_ONE_TOUCH,
	CTF_MAX
};


enum
{
	EVENT_TRIGGERED,
	TIMER_TRIGGERED,
	EXTERNALLY_TRIGGERED,
};

#define EVENT_DONE			true
#define	EVENT_NOT_DONE		false

#define EVENT_ACTIVE		true
#define EVENT_NOT_ACTIVE	false

constexpr int MAXTITLEEVENTS = 128;
constexpr int MAXTITLETIMERS = 32;

constexpr int DINPUT_BUFFERSIZE = 32; /* Number of buffer elements (for menu screens) */

enum
{
	TITLE_EVENT_STATUS_IDLE,
	TITLE_EVENT_STATUS_ACTIVE,
	TITLE_EVENT_STATUS_FINISHED,
};


// timers...
enum
{
	TITLE_TIMER_NULL,
	TITLE_TIMER_PanToDiscsInit,
	TITLE_TIMER_PanToDiscsFromRight,
	TITLE_TIMER_PanToDiscsFromLeft,
	TITLE_TIMER_PanToRightVDU,
	TITLE_TIMER_InterLevelPan,
	TITLE_TIMER_PanToLeftVDU,
	TITLE_TIMER_SwapBikes,
	TITLE_TIMER_ChooseDiscPan,
	TITLE_TIMER_Visuals,
	TITLE_TIMER_PanToDiscsFromVisuals,
	TITLE_TIMER_NormalPanToRightVDU,
	TITLE_TIMER_NormalPanToDiscsFromRight,
	//TITLE_TIMER_SwapObjects,
	TITLE_TIMER_LowerHolopad,
	TITLE_TIMER_ShrinkObject,
	TITLE_TIMER_ExpandObject,
};

struct _TITLE_EVENT
{
	EventFunc	InitFunc;
	EventFunc	ProcessFunc;	//must set EventStatus to TITLE_EVENT_FINISHED when this function is finished
	EventFunc	ExitFunc;
	int			TriggeredBy;	// the timer that is the trigger
	float		StartTime;	//0 = start, 1 = finish
	float		EndTime;	//0 = start, 1 = finish
	int			Status;
};

enum
{	
	TITLE_EVENT_TIMER_IDLE,
	TITLE_EVENT_TIMER_ACTIVE,
	TITLE_EVENT_TIMER_FINISHED,
	TITLE_EVENT_TIMER_AWAITINGRESET,
};

struct _TITLE_EVENT_TIMER
{
	float		Interval;		//lifespan of timer (secs)
	float		CurrentTime;	//0 = start, 1 = end
	int			Status;
	TimerFunc	InitFunc;
};

typedef struct _LEVEL_LOOKUP
{
	char		*name;
	int			frame_no;
	FRAME_INFO	**tpage;
	bool		rotated;
}LEVEL_LOOKUP;

constexpr int MAXTEXTPERMENUITEM = 32;

typedef struct
{
	VECTOR start;
	VECTOR end;
	TRIANGLE poly;
} SCANLINES;


struct _MENUITEM
{
	int		x;
	int		y;
	int		xmax;
	int		ymax;
	int		FrameNo;
	u_int8_t *	StrPnt;
	int		font;
	int		highlightflags;
	void *	Variable;
	void *	Value;
	MenuItemFunc FuncSelect;
	MenuItemFunc FuncDraw;
	TEXTINFO *TextInfo[MAXTEXTPERMENUITEM];
	int numtextitems;
	int EventToTrigger;
};


struct _MENU
{
	u_int8_t *			Name;
	MenuFunc		FuncEntry;
	MenuFunc		FuncExit;
	MenuStatusFunc	MenuFuncDraw;
	int				MenuStatus;
	MENUITEM Item[ MAXMENUITEMS ];
};

struct _SLIDER
{
	int min;
	int max;
	int step;
	int value;
	int oldvalue;
	float dt;
	float timer;
	u_int16_t poly;
	bool redraw_req;
	SliderUserFunc FuncUser;
	SliderSetFunc FuncSet;
	SliderSetFunc FuncRefresh;
};


struct _LIST
{
	int items;
	int display_items;
	int top_item;
	int selected_item;
	u_int8_t item[MAXLISTITEMS][64];
	int old_top_item;
	int old_selected_item;
	ListDeleteFunc FuncDelete;
	ListInfoFunc FuncInfo;	// used to get info from a particular item
	u_int8_t old_item[MAXLISTITEMS][64];
	bool Static;	// list is 'frozen' & awaiting event before allowing processing
};


struct _TEXT
{
	int insert_pos;
	int VDU_insert_pos;
	char text[MAXTEXTMSG];
	MenuItemFunc FuncSet;
	int limit_length;
	TextTypeFunc FuncType;
};

constexpr int EXCLUDE_ALL = -1;
constexpr int MAXEXCLUDEDVERTICES = 16;

typedef struct
{	int Group;
	int ExecBuf;
	int NumVertices;
	int VertexPtr[MAXEXCLUDEDVERTICES];
} EXCLUDEDVERTICES;

// globals

extern TEXT TCPAddress;

extern MENU * CurrentMenu;
extern MENUITEM * CurrentMenuItem;
extern MENU MENU_ForceAbort;
extern MENU MENU_Start;
extern MENU MENU_InGame;
extern MENU MENU_InGameSingle;
extern MENU MENU_Help;
extern MENU	MENU_DebugHelp;
extern MENU	MENU_GotoRoom;
extern MENU MENU_DemoPlaying;
extern MENU MENU_NEW_JoinWaitingToStart;
extern MENU MENU_NEW_HostWaitingToStart;
extern MENU MENU_NEW_WatchTeamSelect;
extern MENU MENU_NEW_ChooseTeamForExistingGame;
extern MENU MENU_NEW_PseudoHostWaitingForAck;
extern MENU MENU_NEW_Joining;
extern MENU MENU_NEW_PseudoHostWaitingToStart;
extern MENU MENU_NEW_ChooseConnectionToJoin;
extern MENU MENU_NEW_GeneralLoading;

extern MENU	MENU_STATS_Start;

extern bool quitting;

extern MENU *LastMenu;

//	Function Prototypes

bool InitTitle();
void ReleaseTitle(void);
bool DisplayTitle(void);

void	MenuRestart( MENU * Menu );
void	MenuExit( void );
void	MenuAbort( void );
void	MenuChange( MENUITEM * Item );
void	MenuBack();
void	MenuProcess( void );
void	MenuItemDrawName( MENUITEM * Item );
void	MenuDraw( MENU * Menu );
void	SetDifficulty( MENUITEM * Item );
void	MenuItemDrawCursor( MENUITEM * Item );
void	SelectMatureContentToggle( MENUITEM *Item );
void	DrawMatureContentToggle( MENUITEM *Item );
void	SelectColourToggle( MENUITEM *Item );
void	SelectToggle( MENUITEM *Item );
void	DrawToggle( MENUITEM *Item );
void	SelectRadioButton( MENUITEM *Item );
void	SelectSlider( MENUITEM *Item );
void	DrawSlider( MENUITEM *Item );
void	SelectKey( MENUITEM *Item );
void	DrawKey( MENUITEM *Item );
void	MenuItemBack( MENUITEM *item );
void	DecrementSlider( MENUITEM *Item );
void	IncrementSlider( MENUITEM *Item );
void	SliderHome( MENUITEM *Item );
void	SliderEnd( MENUITEM *Item );
void	SliderSet( MENUITEM *Item );
void	SliderCancel( MENUITEM *Item );
void	SelectList( MENUITEM *Item );
void	DrawList( MENUITEM *Item );
void	SelectListPrev( LIST *l );
void	SelectListNext( LIST *l );
void	SelectListNextPage( LIST *l );
void	SelectListPrevPage( LIST *l );
void	SelectListHome( LIST *l );
void	SelectListEnd( LIST *l );
void	SelectListSearch( LIST *l, char *keyword );
void	CancelListSelection( LIST *l );
void	SelectText( MENUITEM *Item );
void	DrawTextItem( MENUITEM *Item );
void	TextLeft( TEXT *t );
void	TextRight( TEXT *t );
void	TextType( TEXT *t, char c );
void	TextBackspace( TEXT *t );
void	TextDelete( TEXT *t );
void	TextClear( TEXT *t );
void	TextHome( TEXT *t );
void	TextEnd( TEXT *t );
void	TextEnter( MENUITEM *item );
void	TextCancel( TEXT *t );
bool	ProcessKeydef( int Key );
bool	ProcessText( int Key );
bool	ProcessBikerList ( int Key );
bool	ProcessList( int Key );
bool	ProcessSlider( int Key );
void	ProcessSelect( int Key );

void MenuGoFullScreen( MENUITEM *Item );
void MenuSelectMode( MENU *Menu );
void MakeModeList( MENU *Menu );
bool LoadTitleModels(void);
void VduClear ( void );
void UpdateSessions ( int *dummy );
void SelectQuit( MENUITEM *Item );
void PrintErrorMessage (const char * ErrorStr, int BackBy, MENU *NewMenu, int type);
void MenuItemBackFromError ( MENUITEM *Item );
void MenuChangeEx( MENU *Menu );
void LoadLevelText( MENU *Menu );
bool SetGamma( SLIDER *slider );
void InitInGameMenu( MENU *Menu );
void InitTitleMessaging( void );
void DrawReadyButton( MENUITEM *Item );
void PlayerConfirm( MENUITEM *Item );
void SelectTeamList( MENUITEM *Item );
void TitleReset( MENUITEM *item );
void SetTitleNotify( const char * str );

/* Defined in title.cpp; shared with the modules that use it. */
void DrawFlatMenuItem( MENUITEM *Item );
extern char biker_config[ 256 ];
extern bool ShowNode;
extern SLIDER BikeCompSpeechSlider;
extern SLIDER BikerSpeechSlider;
extern SLIDER SfxSlider;
extern LIST BikeList;
extern LIST BikeComputerList;
extern TEXT MacroText1;
extern TEXT MacroText2;
extern TEXT MacroText3;
extern char biker_name[ 256 ];
extern float LevelTimeTaken;
extern LIST LoadSavedGameList;
extern BYTE TeamNumber[ MAX_PLAYERS ];
extern bool TeamGame;
extern bool CTF;
extern int CameraStatus;
extern bool PlayDemo;
void DebugLastError( void );
extern int DifficultyLevel;
extern bool DebugInfo;
extern bool GodMode;
extern int NetUpdateIntervalCmdLine;
extern char * config_name;
extern TEXT local_port_str;
extern bool SpaceOrbSetup;
extern TEXT host_port_str;
extern u_int8_t QuickStart;
void GetGamePrefs( void );
void GetDefaultPilot(void);
extern VECTOR HoloRot;
extern bool BikeExpanded;
extern bool BikeExhausts;
extern bool BikeEnginesOn;
extern int KillMessageColour;
extern int MatureContent;
extern int HUDColour;
extern bool ShowWeaponsPossessedOnHUD;
extern bool ShowClockOnHUD;
extern bool ShowKPMOnHUD;
extern bool ShowSpeedOnHUD;
extern VECTOR View;
extern VECTOR Look;
extern VECTOR Up;
extern VECTOR DiscLookPos;
extern VECTOR DiscViewPos;
extern VECTOR StartLookPos;
extern VECTOR StartViewPos;
extern bool NeedFlagAtHome;
extern bool OwnFlagTeleportsHome;
extern bool CanCarryOwnFlag;
extern VECTOR BikePos;
extern int16_t DummyTextureIndex;
extern TEXT DemoGameName;
extern MENU MENU_Controls;
extern MENU MENU_Options;
extern MENU MENU_LoadSavedGame;
extern MENU MENU_SaveGame;
extern MENU MENU_NEW_BetweenLevels;
extern MENU MENU_NEW_NumberOfCrystals;
extern float VDUoffsetX;
extern float VDUoffsetY;
extern LIST LevelList;
extern MENUSTATE MenuState;
extern bool MenuFrozen;
extern int16_t InGameLoadGameLevelNum;
extern bool BrightShips;
extern int GameCompleted;
extern bool ScreenSaving;
extern MENU MENU_EditMacro1;
extern MENU MENU_EditMacro2;
extern MENU MENU_EditMacro3;
extern bool PauseDemo;
extern bool RecordDemo;
extern SLIDER DemoSpeed;
extern SLIDER DemoEyesSelect;
extern bool ShowWeaponKills;
extern int16_t SelectedBike;
extern SLIDER TimeLimit;
extern bool NodeCube;
extern bool OldNodeCube;
extern bool MissileCameraEnable;
bool VduFinished( MENU *Menu );
void SelectQuitCurrentGame( MENUITEM *Item );
extern int SystemMessageColour;
extern int FlagMessageColour;
extern SLIDER WatchPlayerSelect;
extern int GoalScore;
extern int PickupMessageColour;
extern bool ShowPlayerHealthAboveBikes;
extern bool ScaleFontPlayerHealthAboveBikes;
extern int PlayerMessageColour;
extern int BountyBonusInterval;
extern bool NodeCubeType;
extern int MilestoneMessagesColour;
extern MENU MENU_NEW_Battle;
extern bool JoystickInput;
extern MENU MENU_QuickTextSend;
extern MENU MENU_QuickTextSendWhisper;
extern MENU MENU_NEW_StartSinglePlayer;
extern bool NoMenuBack;
extern SLIDER CTFSlider;
extern bool MyBrightShips;
extern float Pulse;
extern const char *EmptyString;
extern LIST PlayersList;
extern LIST TeamList[ MAX_TEAMS ];
extern MENUITEM TeamItem;
extern MENUITEM NewTeamItem;
extern SLIDER MyTimeLimit;
extern SLIDER MaxPlayersSlider;
extern SLIDER MaxKillsSlider;
extern SLIDER NumOfPlayersSlider;
extern int MenuStackLevel;
extern SLIDER PacketsSlider;
extern MENUITEM JoinItem;
extern MENUITEM NewJoinItem;
extern MENUITEM WatchTeamSelectionItem;
extern int GameType;
void SetGamePrefs( void );
extern SLIDER MyPacketsSlider;
extern MENU MENU_NEW_CreateGame;
extern SLIDER GoalScoreSlider;
extern bool BountyBonus;
extern TEXT MacroText4;
extern TEXT QuickText;
extern TEXT QuickTextWhisper;
void AddTitleMessage(LPTEXTMSG lpTextMsg);
extern int TauntMessageColour;
extern int MyMessageColour;
extern SLIDER FlagSfxSlider;
extern bool ShowPlaneRGB;
extern bool ShowTeamInfo;
extern SLIDER TextScaleSlider;
extern bool ShowPlayerHealthByScores;
extern bool ShowPlayersOnHUD;
extern bool ShowPlayersOnHUDbyKills;
bool RenderModeReset( void );
void FadeHoloLight(float Brightness );
extern float HoloLightBrightness;
extern float RoomDarkness;
void DarkenRoom2(float darkness);
void ProcessVduItems( MENU *Menu );
extern bool InitialTexturesSet;

/* Nonzero while a front-end menu is up (the 3DS input layer routes keys). */
int _3ds_is_menu_open(void);
