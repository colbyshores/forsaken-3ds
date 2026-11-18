#pragma once

/* lines.h -- pool of coloured 3D lines, drawn per group in debug mode
 *
 * Lines[] is a fixed pool of MAXLINES world-space segments, each with start /
 * end colour and alpha and the group it belongs to. It is per level:
 * InitLines (from InitScene) resets it. FindFreeLine / KillUsedLine move an
 * entry between the free list and the used list; the used list starts at
 * FirstLineUsed (newest) and is walked through Prev. Owners (bounding boxes
 * and collision skins in models.cpp / skin.cpp, CreateDebugLine in
 * primary.cpp) fill in the fields, keep the index and free it themselves.
 *
 * ExecuteLines is called once per visible group; it batches that group's
 * lines (at most 400 per batch) into the given RENDEROBJECT as untextured
 * two-vertex primitives and draws each batch with draw_line_object. Nothing
 * is drawn unless DebugInfo is set.
 */

/*===================================================================
	Includes
===================================================================*/
#include "game/main.h"

#include <math.h>

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
constexpr int MAXLINES = 2000;

/*===================================================================
	Structures
===================================================================*/
typedef struct MY_RGB {

	u_int8_t		R;
	u_int8_t		G;
	u_int8_t		B;

} MY_RGB;

typedef struct LINE {

	u_int16_t		Next;
	u_int16_t		Prev;
	VECTOR		StartPos;			// Start Position
	VECTOR		EndPos;				// End Position
	MY_RGB		StartCol;			// Start Colour
	MY_RGB		EndCol;				// End Colour
	u_int8_t		StartTrans;			// Start Transparency
	u_int8_t		EndTrans;			// End Transparency
	u_int16_t		Group;				// Group ImIn

} LINE;


/*===================================================================
	Protptypes
===================================================================*/
void InitLines( void );
bool ExecuteLines( u_int16_t group, RENDEROBJECT *renderObject );
void KillUsedLine( u_int16_t i );
u_int16_t FindFreeLine( void );

/* Defined in lines.cpp; shared with the modules that use it. */
extern LINE Lines[ MAXLINES ];
extern u_int16_t FirstLineUsed;
