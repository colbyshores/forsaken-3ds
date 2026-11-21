#pragma once

/* oct2.h -- game-state machine, per-frame driver and current-level globals
 *
 * oct2.cpp owns the level list (LevelNames, LevelNum / NewLevelNum, filled by
 * InitLevels from a mission list), the current level's mesh and collision
 * headers (Mloadheader, MCloadheader*), the view/projection state, and
 * RenderScene, which runs one frame of whatever MyGameStatus selects.
 * InitView / ReleaseView load and free the title or level assets by status.
 * framelag is elapsed real seconds x 71; every per-tick rate in the engine is
 * scaled by it, and it is forced to 0 while a menu is open in single player.
 * real_framelag is the raw seconds. hfov, normal_fov and SetFOV are
 * horizontal degrees. RenderBufs[4] are process-lifetime shared batch buffers:
 * [0]/[1] vertex-only, [2] indexed 3D, [3] indexed pre-transformed 2D.
 */

#include "game/main.h"
#include "data/mload.h"
#include "math/new3d.h"
#include "rendering/render.h"
#include "util/timer.h"
#include "world/collision.h"

constexpr int MAX_SHORT_LEVEL_NAME = 32;
extern char ShortLevelNames[MAXLEVELS][MAX_SHORT_LEVEL_NAME];

extern int16_t NewLevelNum;

/* Defined in oct2.cpp; shared with the modules that use it. */
void PrintInitViewStatus( BYTE Status );
bool Our_CalculateFrameRate(void);
extern float framelag;
extern VECTOR Forward;
extern VECTOR SlideUp;
extern MLOADHEADER Mloadheader;
extern VECTOR SlideLeft;
extern VECTOR SlideRight;
extern MCLOADHEADER MCloadheader;
extern MCLOADHEADER MCloadheadert0;
bool SaveSnapShot( int8_t * Filename );

/* The cheat switches and the flag that records their use. GodMode and the two
 * weapon cheats are set by the cheat codes (and the title menu); CheatsDisabled
 * turns the codes off; Cheated marks a run that used one (saved with the game,
 * so keep it a bool). */
struct CheatState
{
	bool GodMode = false;
	bool CheatsDisabled = false;
	bool Cheated = false;
	bool PrimaryWeaponCheat = false;
	bool SecondaryWeaponCheat = false;
};
extern CheatState CheatFlags;
extern int CrystalsFound;
extern int16_t LevelNum;
extern char MissionTextNames[ MAXLEVELS ][ 128 ];
extern int16_t NumLevels;
extern MATRIX ProjMatrix;
extern u_int32_t AnimOncePerFrame;
extern RENDERMATRIX view;
extern RENDERMATRIX identity;
extern bool DrawPanel;
extern VECTOR Backward;
extern VECTOR SlideDown;
extern char LevelNames[ MAXLEVELS ][ 128 ];
extern bool CaptureTheFlag;
extern bool BountyHunt;
bool ChangeLevel( void );
extern BYTE ChangeLevel_MyGameStatus;
extern float normal_fov;
void ReleaseView(void);
void ReleaseScene(void);
bool InitView( void );
bool InitScene(void);
extern RENDEROBJECT RenderBufs[ 4 ];
bool RenderScene( void );
extern bool SeriousError;
extern CamRendering CameraRendering;
extern char PrimaryNames[ 7 ][ 16 ];
extern u_int16_t CameraMissile;
extern int MissileCameraActive;
void SpecialDestroyGame( void );
extern float real_framelag;
bool InitLevels( const char * levels_list );
extern const char *CurrentLevelsList;
extern int Depth;
extern bool Inside;
extern px_timer_t level_timer;
extern bool WaitingToQuit;
extern bool Panel;
extern BYTE PreSynchupStatus;
extern float GetPlayerNumCount1;
extern float GetPlayerNumCount2;
extern int GetPlayerNumCount;
extern bool IMustQuit;
extern VECTOR NullVector;
extern float pixel_aspect_ratio;
void SetViewportError( const char *where, render_viewport_t *vp );
extern int Secrets;
extern int TotalSecrets;
extern char MissionTextPics[ MAXLEVELS ][ 128 ];
extern bool JustExitedMenu;
extern bool RearCameraActive;
void Build_View();
extern float ModeScaleX;
extern float ModeScaleY;
bool ClearBuffers( void );
extern render_viewport_t viewport;
bool SetFOV( float fov );
void InitModeCase(void);
/* Flips the stereoscopic per-eye parallax sign so the 3D reads as receding
 * depth vs. popping out of the screen. Saved as Invert3D in configs/main.txt. */
extern bool g_stereo_invert;
extern float hfov;
extern bool DoClipping;
extern RENDERMATRIX proj;
