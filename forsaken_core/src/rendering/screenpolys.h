/*==========================================================================
 *
 *
 *  File:       ScreenPolys.h
 *  Content:    ScreenPolys.h include file
 *
 *
 ***************************************************************************/

#pragma once

/*===================================================================
	Includes
===================================================================*/
#include <math.h>
#include "game/main.h"

#include "math/new3d.h"
#include "game/ships.h"
#include "data/mload.h"
#include "world/collision.h"
#include "rendering/lights.h"
#include "rendering/2dtextures.h"
#include "data/tload.h"
#include "util/timer.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAXNUMOFSCRPOLYS = 1000;
constexpr int MAXSCREENPOLYVERTS = 800;
constexpr auto TIMERSTARTSCREENX = ( 320.0F - 64.0F );
constexpr auto TIMERSTARTSCREENY = ( 240.0F - 36.0F );
constexpr float TIMERSTARTSCALE = ( 0.3F );
constexpr float TIMERENDSCREENX = ( 160.0F );
constexpr float TIMERENDSCREENY = ( 120.0F );
constexpr float TIMERENDSCALE = ( 1.0F );

/*===================================================================
	Types
===================================================================*/
constexpr int SCRTYPE_Normal = 0;
constexpr int SCRTYPE_LastAFrame = 1;

/*===================================================================
	Flags
===================================================================*/
constexpr int SCRFLAG_Nothing = 0;
constexpr int SCRFLAG_Scale = 1;
constexpr int SCRFLAG_UseCoords = 2;
constexpr int SCRFLAG_Solid = 8;
constexpr int SCRFLAG_Rot90 = 16;
#define	SCRFLAG_BottomHUD	32  /* [3DS] route this poly to the mono bottom screen
                                      instead of the stereo top screen */

/*===================================================================
	Process Sequences
===================================================================*/
constexpr int SCRSEQ_Nothing = 0;
constexpr int SCRSEQ_Anim = 1;
constexpr int SCRSEQ_Fade = 2;
constexpr int SCRSEQ_Thermo = 3;
constexpr int SCRSEQ_Logo = 4;
constexpr int SCRSEQ_FadeUp = 5;

/*===================================================================
	Structures
===================================================================*/
typedef	struct SCRPOLY_RGB {

	u_int8_t	R;
	u_int8_t	G;
	u_int8_t	B;
	u_int8_t	Trans;

} SCRPOLY_RGB;

typedef struct SCRPOLY {

	u_int16_t			Next;
	u_int16_t			Prev;

	u_int16_t			NextInTPage;
	u_int16_t			PrevInTPage;

	u_int16_t			Type;				// Type
	u_int16_t			Flags;				// Flags
	VECTOR			Pos;				// Position
	u_int8_t			R;					// Colour
	u_int8_t			G;					// Colour
	u_int8_t			B;					// Colour
	u_int8_t			Trans;				// Amount of transparency
	float			Xscale;				// XScale
	float			Yscale;				// XScale
	float			Frame;				// Animation Frame
	float			FrameRate;			// FrameRate
	FRAME_INFO	**	Frm_Info;			// Offset Info
	int16_t			SeqNum;				// FrameRate
	float			LifeCount;			// LifeCount
	float			FadeRed;			// Red
	float			FadeGreen;			// Green
	float			FadeBlue;			// Blue
	float			FadeTrans;			// Trans
	float			x1;					// x1
	float			y1;					// y1
	float			x2;					// x2
	float			y2;					// y2
	float			x3;					// x3
	float			y3;					// y3
	float			x4;					// x4
	float			y4;					// y4
	SCRPOLY_RGB		Col1;				// Colour 1
	SCRPOLY_RGB		Col2;				// Colour 2
	SCRPOLY_RGB		Col3;				// Colour 3
	SCRPOLY_RGB		Col4;				// Colour 4

} SCRPOLY;

/*===================================================================
	Protptypes
===================================================================*/
void InitScrPolys( void );
void KillUsedScrPoly( u_int16_t i );
u_int16_t FindFreeScrPoly( void );

/* [3DS bottom-screen HUD routing] */
void ScrPolySetRouteBottom(bool b);
void ScrPolySetDisplayMode(int mode);
bool ScrPolyGetRouteBottom(void);
void ScreenPolyProcess( void );
void DoLensflareEffect( void );
void Conv3DTo2D( VECTOR * SrcVert, VECTOR * DstVert, MATRIX * FinalMat );
void DoAllSecBullLensflare( void );
void SecBullLensflare( u_int16_t i );
void CreateCountdownDigits( void );
void DeleteCountdownDigits( void );
void CreateDigit( u_int16_t * DigitArray, float XPos, float YPos );
void UpdateCountdownDigits( void );
void UpdateDigit( u_int16_t * DigitArray, float XPos, float YPos, int16_t Number, float Scale, float Col ,FRAME_INFO	** Graphics );
void CreateSeperatorDigit( u_int16_t * DigitArray, float XPos, float YPos );
void DeleteDigit( u_int16_t * DigitArray );
void DeleteSeperatorDigit( u_int16_t * DigitArray );
void StartCountDown( int16_t Minutes, int16_t Seconds );
void ClearCountdownBuffers( void );
void AddScreenPolyText( u_int16_t Frame, float XPos, float YPos, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void AddScreenPolyTextScale( u_int16_t Frame, float XPos, float YPos, float XScale, float YScale, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void ShowScreenMultiples( void );
void MakeScreenFlash( u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans, u_int16_t *ScreenPolyStore, int16_t SeqNum );
void InitThermo( void );
void KillThermo( void );
void ResetCountDownBombTag( float Amount );
void InitScrPolyTPages( void );
void AddScrPolyToTPage( u_int16_t i, int16_t TPage );
void RemoveScrPolyFromTPage( u_int16_t i, int16_t TPage );
bool DisplaySolidScrPolys( RENDEROBJECT *renderObject );
bool DisplayNonSolidScrPolys( RENDEROBJECT *renderObject );
bool ScrPolyDispSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly );
bool ScrPolyDispNonSolid( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextScrPoly );
void FreeAllLastAFrameScrPolys( void );
void LoadTimeForLevel( void );
FILE * SaveScreenPolys( FILE * fp );
FILE * LoadScreenPolys( FILE * fp );
void ShowFlagOnShip( u_int16_t Ship );
void ShowFlag( u_int16_t Ship );
void ShowBountyOnShip( u_int16_t Ship );
void ShowBounty( u_int16_t Ship );
void ShowCTFFlagsOnShip( u_int16_t Ship );
void ShowCTFFlags( u_int16_t Ship );

/* Defined in screenpolys.cpp; shared with the modules that use it. */
extern px_timer_t countdown_timer;
extern bool CountDownOn;
extern bool IllegalTime;
extern int SinglePlayerTimeLimit;
extern u_int32_t TotalScrPolysInUse;
extern u_int16_t FirstScrPolyUsed;
extern SCRPOLY ScrPolys[ MAXNUMOFSCRPOLYS ];
extern u_int16_t FlashScreenPoly;
extern bool KilledPlayer;
extern float Countdown_Float;
