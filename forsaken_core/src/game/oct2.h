#pragma once

#include "game/main.h"
#include "data/mload.h"
#include "math/new3d.h"
#include "rendering/render.h"
#include "util/timer.h"
#include "world/collision.h"

#define MAX_SHORT_LEVEL_NAME 32
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
extern bool Cheated;
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
extern int16_t CameraRendering;
extern char PrimaryNames[ 7 ][ 16 ];
extern u_int16_t CameraMissile;
extern int MissileCameraActive;
void SpecialDestroyGame( void );
extern float real_framelag;
extern float Oldframelag;
extern float Demoframelag;
extern bool OldDrawPanel;
bool InitLevels( char * levels_list );
extern char * CurrentLevelsList;
extern int Depth;
extern bool Inside;
extern px_timer_t level_timer;
extern bool WaitingToQuit;
extern bool CheatsDisabled;
extern bool Panel;
extern BYTE PreSynchupStatus;
extern float GetPlayerNumCount1;
extern float GetPlayerNumCount2;
extern int GetPlayerNumCount;
extern bool IMustQuit;
extern VECTOR NullVector;
extern float pixel_aspect_ratio;
void SetViewportError( char *where, render_viewport_t *vp );
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
 * depth vs. popping out of the screen. Saved as Invert3D in Configs/main.txt. */
extern bool g_stereo_invert;
extern float hfov;
extern bool DoClipping;
extern RENDERMATRIX proj;
