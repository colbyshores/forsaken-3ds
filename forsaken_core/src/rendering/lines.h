/*==========================================================================
 *  L i n e s . h
 ***************************************************************************/
#ifndef LINES_INCLUDED
#define LINES_INCLUDED

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
#define MAXLINES			2000

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


#endif


