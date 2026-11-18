
/*==========================================================================
 *
 *
 *  File:       2dpolys.h
 *  Content:    2dpolys.h include file
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

/*===================================================================
	Defines
===================================================================*/
#define	EXPLO_DISTANCE		( 256.0F * GLOBAL_SCALE )
constexpr int MAXNUMOF2DPOLYS = 2000;
#define MaxColDistance		( 64000.0F * GLOBAL_SCALE )
constexpr int MAXFMPOLYVERTS = 700;

/*===================================================================
	Faceme Sequences
===================================================================*/
constexpr int FM_ZERO = 0; // Do Nothing
constexpr int FM_ANIM = 1; // Animate then die (Wall Explo).
constexpr int FM_ANIM2 = 2; // Animate then die (Ship Explo).
constexpr int FM_SMOKE_TRAIL = 3; // Move and animate X2 then die.
constexpr int FM_TROJAX_EXP = 4; // Trojax Explosion.
constexpr int FM_TROJAX = 5; // Trojax 1 Bullet
constexpr int FM_PULSAR_EXP = 6; // Pulsar Explosion
constexpr int FM_PULSAR = 7; // Pulsar
constexpr int FM_SHRAPNEL_EXP = 8; // Shrapnel Explosion
constexpr int FM_FLAME = 9; // Flame Sequence
constexpr int FM_STATIC = 10; // Static Sequence
constexpr int FM_ARC = 11; // Arc Sequence
constexpr int FM_ARC2 = 12; // Arc Glow Sequence
constexpr int FM_ARC_SPARK = 13; // Arc Spark Sequence
constexpr int FM_BITS = 14; // Bits Sequence
constexpr int FM_ARC_RING = 15; // Arc Explosion Ring
constexpr int FM_FLARE = 16; // Arc Explosion Flare
constexpr int FM_GRAVGON_TRAIL = 17; // Gravgon Smoke Trail
constexpr int FM_SOLARIS_TRAIL = 18; // Solaris Smoke Trail
constexpr int FM_NEW_TRAIL = 19; // New Smoke Trail
constexpr int FM_PICKUP_REGEN = 20; // Regenerating Pickup?
constexpr int FM_BLOOD = 21; // Blood?
constexpr int FM_BLOODSPLAT = 22; // Blood Splat?
constexpr int FM_BLOODDRIBBLE = 23; // Blood Dribble?
constexpr int FM_FLASH = 24; // Flash?
constexpr int FM_BLACK_SMOKE = 25; // Black Smoke?
constexpr int FM_NMEBULLET1 = 26; // NME Bullet1?
constexpr int FM_MUZZLEFLASH = 27; // Muzzle Flash?
constexpr int FM_MUSHROOM = 28; // Mushroom Explosion?
constexpr int FM_SMOKE_RING = 29; // Smoke Ring?
constexpr int FM_SPOTFX_SMOKE = 30; // Smoke?
constexpr int FM_SPOTFX_FLAME = 31; // Flame?
constexpr int FM_SPOTFX_STEAM = 32; // Steam?
constexpr int FM_SPOTFX_SPARKS = 33; // Sparks?
constexpr int FM_SPOTFX_SPARKSFACEME = 34; // Sparks FaceMe?
constexpr int FM_SPOTFX_GRAVGON_TRAIL = 35; // Gravgon Trail?
constexpr int FM_SPOTFX_GLOW = 36; // Glow?
constexpr int FM_NME_TRAIL = 37; // NME Trail?
constexpr int FM_NME_VAPOUR_TRAIL = 38; // NME Vapour Trail?
constexpr int FM_SPOTFX_REALSPARKS = 39; // Real Sparks?
constexpr int FM_SPOTFX_SHRAPNEL_TRAIL = 40; // Shrapnel Trail?
constexpr int FM_SPOTFX_BUBBLE = 41; // Bubble?
constexpr int FM_NMEGEN_EFFECT = 42; // Enemy Generation Effect?
constexpr int FM_NMEGEN_FLARE = 43; // Enemy Generation Flare?
constexpr int FM_SPOTFX_DRIP = 44; // Drip?
constexpr int FM_PHOTON = 45; // Photon Torpedo?
constexpr int FM_SPOTFX_BURNING = 46; // Burning?
constexpr int FM_ANIMSPEED = 47; // Anim using animspeed
constexpr int FM_SPOTFX_BEARD_AFTERBURNER = 48; // Beard Afterburner
constexpr int FM_RESTART_INACTIVE = 49; // Restart Point Inactive
constexpr int FM_RESTART_ACTIVE = 50; // Restart Point Active
constexpr int FM_SPOTFX_WATERSPLASH = 51; // Water Splash
constexpr int FM_HK5_TRAIL = 52; // Hk5 Trail
constexpr int FM_BLUEPHOTON = 53; // Blue Photon Torpedo?
constexpr int FM_FIREBALL = 54; // Fireball
constexpr int FM_SPARKLE = 55; // Sparkle?
constexpr int FM_GOALSPARKLE = 56; // Hacky goal Sparkle?
	
/*===================================================================
	Faceme Flags
===================================================================*/
constexpr int FM_FLAG_ZERO = 0; // Do Nothing
constexpr int FM_FLAG_MOVEOUT = 1; // Move 2D Sprite toward Camera.
constexpr int FM_FLAG_DIRCONST = 2; // Direction and Up Vector Constant.
constexpr int FM_FLAG_TWOSIDED = 4; // Double Sided Poly
constexpr int FM_FLAG_DONTCLIP = 8; // Don't Clip
constexpr int FM_FLAG_RELATIVE = 16; // Direction and Up Vector Relative to camera.

/*===================================================================
		TPage Info Structure for FmPolys/Polys/ScreenPolys
===================================================================*/
typedef struct TPAGEINFO {

	u_int16_t		FirstPoly;		// Index to first Poly in TPage
	int16_t		StartVert;		// Filled in by PolyDisp
	int16_t		NumVerts;		// Filled in by PolyDisp
	int16_t		NumTris;		// Filled in by PolyDisp

} TPAGEINFO;

/*===================================================================
	FmPoly Structure
===================================================================*/
typedef struct FMPOLY{

	u_int16_t			Next;
	u_int16_t			Prev;
	u_int16_t			NextInTPage;
	u_int16_t			PrevInTPage;
	float			LifeCount;			// LifeCount
	VECTOR			Pos;				// Position
	u_int8_t			Start_R;			// Start Colour
	u_int8_t			Start_G;
	u_int8_t			Start_B;
	u_int8_t			R;					// Colour
	u_int8_t			G;
	u_int8_t			B;
	u_int8_t			Trans;				// Amount of transparency
	u_int8_t			OnceOnly;			// Once Only
	float			Rot;				// Rotation Angle
	float			RotSpeed;			// Rotation Speed
	float			Frame;				// Animation Frame
	float			AnimSpeed;			// Animation Speed
	int16_t			Flags;				// Display Flags
	FRAME_INFO	**	Frm_Info;			// Offset Info
	int16_t			SeqNum;				// Sequence Number
	VECTOR			Dir;				// Current Direction
	VECTOR			DirVector;			// Direction to face
	VECTOR			UpVector;			// Up Vector to face
	MATRIX			Mat;				// Matrix ( Trojax );
	u_int16_t			Group;				// GroupImIn
	float			Speed;				// Speed
	float			UpSpeed;			// Speed
	float			xsize;				// in world space
	float			ysize;				// in world space
	
}FMPOLY;


/*===================================================================
	Protptypes
===================================================================*/
void InitFmPoly( void );
void KillUsedFmPoly( u_int16_t i );
u_int16_t FindFreeFmPoly();
void FmPolyProcess( void );
void InitFmPolyTPages( void );
void AddFmPolyToTPage( u_int16_t i, int16_t TPage );
void RemoveFmPolyFromTPage( u_int16_t i, int16_t TPage );
bool DisplayGroupClippedFmPolys( RENDEROBJECT *renderObject, u_int16_t Group );
bool DisplayGroupUnclippedFmPolys( RENDEROBJECT *renderObject );
bool FmPolyDispGroupClipped( u_int16_t Group, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextFmPoly );
bool FmPolyDispGroupUnclipped( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextFmPoly );
FILE * SaveFmPolys( FILE * fp );
FILE * LoadFmPolys( FILE * fp );

/* Defined in 2dpolys.cpp; shared with the modules that use it. */
extern FMPOLY FmPolys[ MAXNUMOF2DPOLYS ];
extern u_int32_t TotalFmPolysInUse;
extern u_int16_t FirstFmPolyUsed;
