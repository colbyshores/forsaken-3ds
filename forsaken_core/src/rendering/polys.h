#pragma once

/* polys.h -- world-space textured quads (lasers, shields, afterburner trails)
 *
 * Polys[] is a fixed pool of MAXPOLYS four-corner quads whose corners are set
 * explicitly by the owner (weapons in primary.cpp / secondary.cpp, the title
 * screen, and the afterburner flames DoAfterBurnerEffects keeps per ship), as
 * opposed to the camera-facing sprites in 2dpolys.h. The pool is per level:
 * InitPolys (from InitScene) resets it. FindFreePoly / KillUsedPoly move an
 * entry between the free list and the used list (FirstPolyUsed, walked
 * through Prev). Each used poly must also be linked into its texture page with
 * AddPolyToTPage(GetTPage(*Frm_Info, 0)); KillUsedPoly unlinks it, so Frm_Info
 * must be valid when it is called. ProcessPolys advances laser / INVBITS
 * animation frames and the shield / INVBITS fades once per frame.
 *
 * Drawing is per page: Display*Polys walk PolyTPages from page 0 and call the
 * matching *PolyDisp* fill, which counts verts per page (pass 1), then writes
 * one texture group per quad into the shared RENDEROBJECT (pass 2), stopping
 * at MAXPOLYVERTS and returning the page / poly to resume from; the caller
 * draws each batch with draw_billboard_object, culling off. "Clipped" fills
 * take polys of one group (drawn while that group is clipped); "Unclipped"
 * fills take POLY_FLAG_DONTCLIP polys. Vertex alpha is
 * always 255; Trans1..4 are not used by the fills.
 *
 * On 3DS textureGroups only has MAX_TEXTURE_GROUPS entries, so a fill that
 * fills it draws the batch mid-fill (unlock, draw, re-lock) and carries on
 * from the start of the buffers. Each texture group's startVert is then
 * the write position within the current batch (batchVert), not the pass-1
 * PolyTPages[].StartVert offsets, which count from the start of the
 * unflushed buffer (and reserve every bit of a multi-bit frame although
 * only one quad is written). Only pages 0..MAXTPAGESPERTLOAD-1 are drawn;
 * the extra PolyTPages slot holds polys whose page is -1.
 */

/*===================================================================
	Includes
===================================================================*/
#include "game/main.h"
#include <math.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/ships.h"
#include "data/mload.h"
#include "world/collision.h"
#include "rendering/lights.h"
#include "rendering/2dtextures.h"
#include "rendering/2dpolys.h"
#include "data/tload.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAXPOLYS = 2000;
constexpr int MAXPOLYVERTS = 800;

constexpr int POLY_FLAG_NOTHING = 0;
constexpr int POLY_FLAG_TWOSIDED = 1;
constexpr int POLY_FLAG_DONTCLIP = 2;

constexpr int POLY_NOTHING = 0;
constexpr int POLY_LASER = 1;
constexpr int POLY_PULSAR_TRAIL = 2;
constexpr int POLY_SHIELD = 3;
constexpr int POLY_AFTERBURNER = 4;
constexpr int POLY_SCATTER_TRAIL = 5;
constexpr int POLY_INVBITS = 6;
constexpr int POLY_POWERLASER = 7;

/*===================================================================
	Structures
===================================================================*/
struct POLY_RGB {

	u_int8_t	R;
	u_int8_t	G;
	u_int8_t	B;

};

struct POLY {

	u_int16_t			Next;
	u_int16_t			Prev;

	u_int16_t			NextInTPage;
	u_int16_t			PrevInTPage;

	VECTOR			Pos1;				// Position 1
	VECTOR			Pos2;				// Position	2
	VECTOR			Pos3;				// Position	3
	VECTOR			Pos4;				// Position	4
	POLY_RGB		Col1;				// Colour 1
	POLY_RGB		Col2;				// Colour 2
	POLY_RGB		Col3;				// Colour 3
	POLY_RGB		Col4;				// Colour 4
	u_int8_t			Trans1;				// Amount of transparency 1
	u_int8_t			Trans2;				// Amount of transparency 2
	u_int8_t			Trans3;				// Amount of transparency 3
	u_int8_t			Trans4;				// Amount of transparency 4
	int16_t			Flags;				// Flags
	float			Scale;				// Scale Factor
	float			Frame;				// Animation Frame
	float			AnimSpeed;			// Animation Speed
	FRAME_INFO	**	Frm_Info;			// Offset Info
	int16_t			SeqNum;				// Process Sequence
	u_int16_t			Group;				// Group
	float			TimeStep;			// Time step for interpolation
	QUATLERP		Qlerp;				// Quaternion Interpolation
	QUAT			Quat;				// Quat
	u_int16_t			Ship;				// Ship

};

/*===================================================================
	Prototypes
===================================================================*/
void InitPolys( void );
void KillUsedPoly( u_int16_t i );
u_int16_t FindFreePoly( void );
void ProcessPolys( void );
void DoAfterBurnerEffects( void );
void InitAfterBurners( void );
void InitPolyTPages( void );
void AddPolyToTPage( u_int16_t i, int16_t TPage );
void RemovePolyFromTPage( u_int16_t i, int16_t TPage );
bool DisplayGroupClippedPolys( RENDEROBJECT *renderObject, u_int16_t Group );
bool DisplayGroupUnclippedPolys( RENDEROBJECT *renderObject );
bool PolyDispGroupClipped( u_int16_t Group, RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextPoly );
bool PolyDispGroupUnclipped( RENDEROBJECT *renderObject, int16_t * TPage, u_int16_t * NextPoly );


FILE * SavePolys( FILE * fp );
FILE * LoadPolys( FILE * fp );

/* Defined in polys.cpp; shared with the modules that use it. */
extern POLY Polys[ MAXPOLYS ];
extern u_int32_t TotalPolysInUse;
extern u_int16_t FirstPolyUsed;
extern VECTOR ShieldVerts[ 4 ];
