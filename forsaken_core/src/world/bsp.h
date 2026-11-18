#pragma once

/* bsp.h -- per-group collision BSP trees and portal BSPs
 *
 * bsp.cpp loads the level .BSP (one tree per visibility group) into
 * Bsp_Header[0] and the matching .PBS portal trees into Bsp_Portal_Header;
 * both are per level and freed by Bspfree. Bsp_Header[1] is never loaded, so
 * code that tests it is inert.
 *
 * RayCollide's Dir is a displacement whose length is the ray length, not a
 * unit vector; planes are offset by the global CollisionRadius (callers set
 * it, currently always 0). A NULL front child is open space and a NULL back
 * child is solid; PointInsideSkin returns true in open space. TOLER is the
 * on-plane tolerance in world units.
 */

#include "game/main.h"
#include "math/new3d.h"

/*
 * Defines
 */
constexpr double TOLER = (0.03125); //0.0000076F


constexpr int MAX_BSP_MODELS = (2);
/*
 * Structures
 */
 typedef struct BSP_RAWNODE
 {
	VECTOR	Normal;
	float	Offset;
	int		Front;
	int		Back;
	int		Colour;
 }BSP_RAWNODE;
static_assert( sizeof( BSP_RAWNODE ) == 28, "BSP_RAWNODE is a file record" );

typedef struct BSP_NODE
{
	VECTOR	Normal;
	float	Offset;
	int		Colour;
struct	BSP_NODE * Parent;
struct	BSP_NODE * Front;
struct	BSP_NODE * Back;
}BSP_NODE;

typedef struct BSP_TREE
{
   int	NumNodes;
   BSP_NODE * Root;
}BSP_TREE;

typedef struct BSP_HEADER
{
	bool	State;
	int NumGroups;
	BSP_TREE Bsp_Tree[MAXGROUPS];
}BSP_HEADER;


typedef struct _BSP_PORTAL
{
	u_int16_t		group;
	VECTOR		normal;
	float		offset;
	BSP_TREE	bsp;
} BSP_PORTAL;

typedef struct _BSP_PORTAL_GROUP
{
	int			portals;
	BSP_PORTAL	*portal;
} BSP_PORTAL_GROUP;


typedef struct _BSP_PORTAL_HEADER
{
	bool		state;
	int			groups;
	BSP_PORTAL_GROUP	group[ MAXGROUPS ];
} BSP_PORTAL_HEADER;


/*
 * global vars
 */
extern	BSP_HEADER Bsp_Header[];
extern	BSP_PORTAL_HEADER Bsp_Portal_Header;


 /*
 * fn prototypes
 */
void Bspfree( void );
bool Bspload( char * Filename,  BSP_HEADER *Bsp_Header );
bool RayCollide( BSP_HEADER *Bsp_Header, VECTOR *StartPos, VECTOR *Dir, VECTOR *ImpactPoint, VECTOR *ImpactNormal, float *ImpactOffset ,u_int16_t group );

bool PointInsideSkin( VECTOR *Pos, u_int16_t Group );
/* Defined in bsp.cpp; shared with the modules that use it. */
extern BSP_NODE * BSP_Nodes[ 256 ];
extern float CollisionRadius;
bool PISDistRecursive( VECTOR *Pos, BSP_NODE *node);
extern BSP_NODE * OldCollideNode;
