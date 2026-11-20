#pragma once

/* lights.h -- dynamic point/spot lights (XLights) and vertex relighting
 *
 * XLights[] is a fixed pool of MAXXLIGHTS dynamic lights owned by weapons,
 * ships, pickups, spot effects and real-time level lights (rtlight). It is
 * per level: InitXLights (from InitScene) resets it. Owners take a light with
 * FindFreeXLight, set Pos / Group / colour / Size, and either free it with
 * KillUsedXLight or give it a non-zero SizeCount (SetLightDie does this), in
 * which case ProcessXLights shrinks and dims it once per frame and frees it
 * when Size reaches 0. The used list starts at FirstXLightUsed and is walked
 * through Prev.
 *
 * Once per camera pass BuildVisibleLightList links the lights that are
 * Visible and whose group overlaps the camera's group (visi VisibleOverlap)
 * into the FirstLightVisible chain; Group == INDEX_NONE takes every used
 * light. The vertex relighting functions read only that chain.
 *
 * XLight1Group runs per visible level group before it is drawn: it applies
 * POLYANIM UV frame changes, restores each mesh's baked colours from
 * originalVerts (or writes the white-out / underwater shimmer, phase cral),
 * then adds each visible light's falloff on the CPU. XLightMxloadHeader /
 * XLightMxaloadHeader do the restore + add for one MODFLAG_RealLight model.
 * With GPU_LIGHTING (the 3DS build) the per-light CPU loops are compiled out
 * and the citro3d vertex shader adds the lights instead (uploaded by
 * c3d_upload_xlights); XLightMxloadHeader then also skips the colour restore.
 * The PC build lights on the CPU.
 *
 * FindNearestCellColour samples the level's baked colour grid at a point
 * (alpha 0; halved while BrightShips is on in STATUS_Normal).
 */

#include <math.h>
#include "game/main.h"

#include "math/new3d.h"
#include "game/ships.h"
#include "data/mload.h"
#include "data/mxload.h"
#include "data/mxaload.h"
#include "world/collision.h"

/*
 * defines
 */
constexpr int MAXXLIGHTS = 256;
constexpr int POINT_LIGHT = 0;
constexpr int SPOT_LIGHT = 1;

constexpr auto MIN_LIGHT_SIZE = ( 1536.0F * GLOBAL_SCALE );

/*
 * structures
 */

struct XLIGHT{
	int		Type;
	bool	Visible;
	u_int16_t	Index;
	float	r;
	float	g;
	float	b;
	float	Size;
	float	SizeCount;
	float	ColorCount;
	float	CosArc;			// how wide is my light.......
	u_int16_t	Next;			// Next Free
	u_int16_t	Prev;			// Previous used...
	VECTOR	Pos;
	VECTOR	Dir;			// Normalised....
	u_int16_t	Group;
	struct XLIGHT * NextVisible;
};

/*
* fn prototypes
*/

bool	ProcessXLights( MLOADHEADER * Mloadheader );
void	InitXLights();
u_int16_t	FindFreeXLight();
void	KillUsedXLight(u_int16_t);

void	SetLightDie ( u_int16_t light );
bool	XLight1Group( MLOADHEADER * Mloadheader, u_int16_t group );

bool	XLightMxloadHeader( MXLOADHEADER * MXloadheader , VECTOR * Pos , float Radius , MATRIX * Matrix );
bool	XLightMxaloadHeader( MXALOADHEADER * MXAloadheader , VECTOR * Pos , float Radius , MATRIX * Matrix );


COLOR FindNearestCellColour( MLOADHEADER * Mloadheader,VECTOR * Pos, u_int16_t group );
void BuildVisibleLightList( u_int16_t Group );
FILE * SaveXLights( FILE * fp );
FILE * LoadXLights( FILE * fp );

/* Defined in lights.cpp; shared with the modules that use it. */
extern XLIGHT * FirstLightVisible;
extern XLIGHT XLights[ MAXXLIGHTS ];
extern float cral;
