

/*===================================================================
*	c o l l i s o n . c
*	All routines needed to load in a .mc file....
*	and do collision to any polygon in a specified group..
*	or to the nearest polygon..
===================================================================*/
#include "game/main.h"
#include <stdio.h>
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "world/collision.h"
#include "game/trigarea.h"
#include "world/bsp.h"
#include "rendering/lines.h"
#include "input/controls.h"
#include "game/triggers.h"
#include "game/enemies.h"

#include "math/sphere.h"
#include "game/secondary.h"
#include "game/restart.h"
#include "util/util.h"
#include "data/mload.h"
#include "game/oct2.h"
#include "game/ships.h"
#include "ui/title.h"
#include "world/visi.h"

constexpr int MC_VERSION_NUMBER = 1;

constexpr int BSP_ENABLE = 1;
#undef BSP2

constexpr int MAX_FEELER_RAYS = (9);
constexpr int MAX_QFEELER_RAYS = (5);

#define BACKGROUND_PORTAL_TOLERANCE		(12.0F * GLOBAL_SCALE)

#define PORTAL_IMPACTOFFSET		(0.1F * GLOBAL_SCALE)

constexpr double POINT_ON_PORTAL_TOLERANCE = (0.1);
constexpr float OUTSIDE_GROUP_TOLERANCE = (25.0F);

int no_collision = 0;
int outside_group = 0;

static VECTOR		ColPoint;
static ZONESIDE	*	ColSide;
static float		ColDist;
	COMP_OBJ	*	ColChild;
static int			ColCollided;
static float		ColRadius;
static VECTOR		ColDir;
static BGOBJECT *	ColParent;
static int16_t		ColZoneNum;

static	u_int16_t		CompEnemyHit = (u_int16_t) -1;
static	u_int16_t		AnyCompEnemyHit = (u_int16_t) -1;
static	VECTOR		CompEnemyHitPos;
static	NORMAL		CompEnemyHitNormal;
static	float		CompEnemyHitDist;

static	VECTOR	Origin;
static	VECTOR	ODir;
static	VECTOR	IPoint;
static	float	IDist;

bool CheckRestartPointCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject );

bool CheckEnemyPolyCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject );

/*===================================================================
	Procedure	:		Load .mc File Collision file..
	Input		:		char	*	Filename , MCLOADHEADER * MCloadheader
	Output		:		bool
===================================================================*/
bool MCload( char * /*Filename*/ , MCLOADHEADER * /*MCloadheader*/ )
{

	return( true );
}


typedef enum
{
	X_Axis = 0,
	Y_Axis = 1,
	Z_Axis = 2
} AxisIndex;


bool ColRayPlaneIntersect( VECTOR *normal, float offset )
{
	float		t;
	float		Div, Num;

/*===================================================================
	Calculate T
===================================================================*/
	Div = ( ODir.x * normal->x) + 
		  ( ODir.y * normal->y) + 
		  ( ODir.z * normal->z);
	
	if( Div >= 0.0F ) return false;		/* Reject, Parallel */


	Num = ( ( Origin.x * normal->x ) +
		    ( Origin.y * normal->y ) +
		    ( Origin.z * normal->z ) ) + offset; 
		
	t = -( Num / Div );

/*===================================================================
	Do plane collision
===================================================================*/

	if( t < 0.0F ) return false;		/* Intersection behind origin */
	if( t > 1.0F ) return false;		/* Intersection Greater then ray length */

	IPoint.x = ( Origin.x + ( ODir.x * t ) );
	IPoint.y = ( Origin.y + ( ODir.y * t ) );
	IPoint.z = ( Origin.z + ( ODir.z * t ) );

	IDist = t;
	return true;
}

#define NIL ((u_int16_t) -1)

static float hit_portal_offset = 0.0F;

// version 3...
/*
 * BackgroundCollide
 *
 * Description
 *	checks for background collisions of a point moving between two positions
 *
 * Inputs
 *	c			=	background collision model
 *	m			=	background display model
 *	StartPos	=	start position
 *	StartGroup	=	group start position is in
 *	MoveOffset	=	movement offset
 *	BGCol		=	BGObject Collision wanted
 *
 * Outputs
 *	EndPos		=	final position (either collision impact point or target)
 *	EndGroup	=	group final position is in
 *	FaceNormal	=	normal of face collided with (if collided at all)
 *	NewTarget	=	new target position after sliding along collision face (if collided at all)
 *
 * Returns
 *	true if collided with background, false if no collision
 */
bool BackgroundCollide( MCLOADHEADER *c, MLOADHEADER *m,
					  VECTOR *StartPos, u_int16_t StartGroup, VECTOR *MoveOffset, 
					  VECTOR *EndPos, u_int16_t *EndGroup,
					  NORMAL *FaceNormal, VECTOR *NewTarget, bool BGCol, BGOBJECT ** BGObject )
{
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	float poffset, pdist;
	VECTOR ppos, pmove, epos, tpos;
	NORMAL fnorm, pnorm; ZERO_STACK_MEM(pnorm);
	u_int16_t group;
	u_int16_t next_group;
	u_int16_t last_group = 0;
	bool hit_bg, hit_portal, hit_any_portal;
	float dist_bg;
	VECTOR dv;
	float impact_offset;
	VECTOR	OldPMove;
	VECTOR	OldPPos;
	float impact_dotp;

	CompEnemyHit = (u_int16_t) -1;
	AnyCompEnemyHit = (u_int16_t) -1;
	next_group = StartGroup;
	ppos = *StartPos;
	pmove = *MoveOffset;
	hit_any_portal = false;
	do {
		group = next_group;
		hit_portal = false;
		OldPMove = pmove;
		OldPPos = ppos;
		hit_bg = false;
		if ( OneGroupPolyCol( c, m, group, &OldPPos, &OldPMove, &epos, &fnorm, &tpos, BGCol, BGObject ) )
		{
			BSP_PORTAL_GROUP	*pg;
			BSP_PORTAL			*bp;
			int					j;
			float				d;
			
			dv.x = epos.x - StartPos->x;
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			dv.y = epos.y - StartPos->y;
			dv.z = epos.z - StartPos->z;
			dist_bg = (float) sqrtf( dv.x * dv.x + dv.y * dv.y + dv.z * dv.z );
			
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			pg = &Bsp_Portal_Header.group[ group ];
			for ( j = 0; j < pg->portals; j++ )
			{
				bp = &pg->portal[ j ];
				if ( DotProduct( &bp->normal, MoveOffset ) < 0.0F )
				{
					d = epos.x * bp->normal.x + epos.y * bp->normal.y + epos.z * bp->normal.z + bp->offset;
					if ( fabsf( d ) < POINT_ON_PORTAL_TOLERANCE )
					{
						if ( PISDistRecursive( &epos, bp->bsp.Root ) )
						{
							hit_portal = true;
							next_group = bp->group;
							pmove.x += ppos.x - epos.x;
							pmove.y += ppos.y - epos.y;
							pmove.z += ppos.z - epos.z;
							ppos = epos;
							hit_any_portal = true;
							pnorm.nx = bp->normal.x;
							pnorm.ny = bp->normal.y;
							pnorm.nz = bp->normal.z;
							hit_portal_offset = bp->offset;
							last_group = group;
							break;
						}
					}
				/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
				}
			}
			if ( hit_portal )
			{
				hit_bg = false;
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			}
			else
			{
				hit_bg = true;
				dv = *MoveOffset;
				NormaliseVector( &dv );
				impact_dotp = -( dv.x * fnorm.nx + dv.y * fnorm.ny + dv.z * fnorm.nz );
				impact_offset = ( impact_dotp ) ? COLLISION_FUDGE / impact_dotp : 2.0F * dist_bg;
				if ( dist_bg > impact_offset )
				{
					epos.x -= dv.x * impact_offset;
					epos.y -= dv.y * impact_offset;
					epos.z -= dv.z * impact_offset;
					if ( hit_any_portal )
					{
						/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
						// find distance of end point from portal plane
						poffset = hit_portal_offset;
						pdist = epos.x * pnorm.nx + epos.y * pnorm.ny + epos.z * pnorm.nz + poffset;
						// NB this test is NOT perfect -- ideally should fire ray from StartPos to epos to find EndGroup more reliably
						if ( pdist < 0.0F ) // end point still behind last portal plane -> collide in this group
							*EndGroup = group;
						else // end point now in front of portal plane -> collide in last group
							*EndGroup = last_group;
					}
					else
					{
	 					*EndGroup = group;
					}
				}
				else
				{
					epos = *StartPos;
					*EndGroup = StartGroup;
				}
				*EndPos = epos;
				*FaceNormal = fnorm;
				*NewTarget = tpos;
			}
		}
	} while ( !hit_bg && hit_portal );
	if ( !hit_bg )
	{
		*EndGroup = group;
		EndPos->x = StartPos->x + MoveOffset->x;
		EndPos->y = StartPos->y + MoveOffset->y;
		EndPos->z = StartPos->z + MoveOffset->z;
	}
	dv.x = (float) fabsf( EndPos->x - m->Group[ *EndGroup ].center.x );
	dv.y = (float) fabsf( EndPos->y - m->Group[ *EndGroup ].center.y );
	dv.z = (float) fabsf( EndPos->z - m->Group[ *EndGroup ].center.z );
	outside_group = dv.x > m->Group[ *EndGroup ].half_size.x + OUTSIDE_GROUP_TOLERANCE ||
					dv.y > m->Group[ *EndGroup ].half_size.y + OUTSIDE_GROUP_TOLERANCE ||
					dv.z > m->Group[ *EndGroup ].half_size.z + OUTSIDE_GROUP_TOLERANCE;
	return hit_bg;
}

/*
 * MoveGroup
 *
 * Description
 *	checks for portal collisions of a point moving between two positions
 *
 * Inputs
 *	m			=	background display model
 *	StartPos	=	start position
 *	StartGroup	=	group start position is in
 *	MoveOffset	=	movement offset
 *
 * Outputs
 *	None
 *
 * Returns
 *	EndGroup	=	group final position is in
 */
u_int16_t MoveGroup( MLOADHEADER *m, VECTOR *StartPos, u_int16_t StartGroup, VECTOR *MoveOffset )
{
	VECTOR ppos, pmove, epos;
	NORMAL pnorm;
	u_int16_t group;
	u_int16_t next_group;
	bool hit_portal;
	VECTOR dv;

	next_group = StartGroup;
	ppos = *StartPos;
	pmove = *MoveOffset;
	do {
		group = next_group;
		hit_portal = false;
		if ( OneGroupPortalCol( m, group, &ppos, &pmove, &epos, &pnorm, &next_group, 0 ) )
		{
			hit_portal = true;
			pmove.x += ppos.x - epos.x;
			pmove.y += ppos.y - epos.y;
			pmove.z += ppos.z - epos.z;
			ppos = epos;
		}
	} while ( hit_portal );

	dv.x = (float) fabsf( StartPos->x + MoveOffset->x - m->Group[ group ].center.x );
	dv.y = (float) fabsf( StartPos->y + MoveOffset->y - m->Group[ group ].center.y );
	dv.z = (float) fabsf( StartPos->z + MoveOffset->z - m->Group[ group ].center.z );
	outside_group = dv.x > m->Group[ group ].half_size.x + OUTSIDE_GROUP_TOLERANCE ||
					dv.y > m->Group[ group ].half_size.y + OUTSIDE_GROUP_TOLERANCE ||
					dv.z > m->Group[ group ].half_size.z + OUTSIDE_GROUP_TOLERANCE;

	return group;
}


/*===================================================================
	Procedure	:	Does the collision between a specified group
	Input		:	MCLOADHEADER *
				:	MLOADHEADER *
				:	u_int16_t group
				:	VECTOR * Pos
				:	VECTOR * Dir
				:	VECTOR * ImpactPoint
				:	NORMAL * FaceNormal
				:	VECTOR * Pos_New
				:	bool	BGCol
  Output		:	bool
===================================================================*/
bool OneGroupPolyCol( MCLOADHEADER * MCloadheaderp ,MLOADHEADER * /*Mloadheader*/ , u_int16_t group ,
					 VECTOR * Pos, VECTOR * Dir  ,
					 VECTOR * ImpactPoint , NORMAL  * FaceNormal , VECTOR * Pos_New, bool BGCol, BGOBJECT ** BGColObject )
{
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	MCFACE	*	CollFace;
	float		Distance = 0;
	float		D;
	VECTOR		Dn;
	float		C;
	VECTOR		e;
	float		nDOTe;
	int			collided = 0;

	VECTOR		ImpactNormal;
	float		ImpactOffset;
	MCFACE		BSPFace; ZERO_STACK_MEM(BSPFace);
	
	if( group == (u_int16_t) -1)
		return false;

	Origin.x = Pos->x;
	Origin.y = Pos->y;
	Origin.z = Pos->z;
	ODir.x = Dir->x;
	ODir.y = Dir->y;
	ODir.z = Dir->z;

	if( (MCloadheaderp == &MCloadheadert0) && Bsp_Header[ 0 ].State )
	{
		CollisionRadius = 0.0F;
		ImpactPoint->x = 0.0F;
		ImpactPoint->y = 0.0F;
		ImpactPoint->z = 0.0F;
		collided = RayCollide( &Bsp_Header[ 0 ], &Origin, &ODir, ImpactPoint, &ImpactNormal, &ImpactOffset , group );
		if ( collided )
		{
			e.x = ImpactPoint->x - Pos->x;
			e.y = ImpactPoint->y - Pos->y;
			e.z = ImpactPoint->z - Pos->z;
			Distance = VectorLength( &e );
			BSPFace.nx = ImpactNormal.x;
			BSPFace.ny = ImpactNormal.y;
			BSPFace.nz = ImpactNormal.z;
			BSPFace.D = ImpactOffset;
			CollFace = &BSPFace;
		}
	}
	else if( (MCloadheaderp == &MCloadheader) && Bsp_Header[ 1 ].State )
	{
		/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
		CollisionRadius = 0.0F;
		collided = RayCollide( &Bsp_Header[ 1 ], &Origin, &ODir, ImpactPoint, &ImpactNormal, &ImpactOffset , group );
		if ( collided )
		{
			e.x = ImpactPoint->x - Pos->x;
			e.y = ImpactPoint->y - Pos->y;
			e.z = ImpactPoint->z - Pos->z;
			Distance = VectorLength( &e );
			BSPFace.nx = ImpactNormal.x;
			BSPFace.ny = ImpactNormal.y;
			BSPFace.nz = ImpactNormal.z;
			BSPFace.D = ImpactOffset;
			CollFace = &BSPFace;
		}
	}
	else
	{
		static int not_bsp = 0;

		if ( !not_bsp++ )
		{
		}

	}

#if ENABLE_RESTART
	if( CheckRestartPointCol( group, Distance, ImpactPoint, collided, Pos_New, FaceNormal, BGColObject ) )
	{
		collided += ColCollided;
		e.x = ( ImpactPoint->x - Pos->x );
		e.y = ( ImpactPoint->y - Pos->y );
		e.z = ( ImpactPoint->z - Pos->z );
		Distance = VectorLength( &e );
		BSPFace.nx = FaceNormal->nx;
		BSPFace.ny = FaceNormal->ny;
		BSPFace.nz = FaceNormal->nz;
		BSPFace.D = -DotProduct( (VECTOR *) FaceNormal, ImpactPoint );
		CollFace = &BSPFace;
	}
#endif

	if( BGCol )
	{
		if( MCloadheaderp == &MCloadheadert0 ) ColRadius = 0.0F;
		if( MCloadheaderp == &MCloadheader ) ColRadius = ( 300.0F * GLOBAL_SCALE );

		if( CheckEnemyPolyCol( group, Distance, ImpactPoint, collided, Pos_New, FaceNormal, BGColObject ) )
		{
			collided += ColCollided;
			e.x = ( ImpactPoint->x - Pos->x );
			e.y = ( ImpactPoint->y - Pos->y );
			e.z = ( ImpactPoint->z - Pos->z );
			Distance = VectorLength( &e );
			BSPFace.nx = FaceNormal->nx;
			BSPFace.ny = FaceNormal->ny;
			BSPFace.nz = FaceNormal->nz;
			BSPFace.D = -DotProduct( (VECTOR *) FaceNormal, ImpactPoint );
			CollFace = &BSPFace;
		}

		if( CheckBGObjectsCol( group, Distance, ImpactPoint, collided, Pos_New, FaceNormal, BGColObject ) )
		{
			CompEnemyHit = (u_int16_t) -1;
			AnyCompEnemyHit = (u_int16_t) -1;
			collided += ColCollided;
			e.x = ( ImpactPoint->x - Pos->x );
			e.y = ( ImpactPoint->y - Pos->y );
			e.z = ( ImpactPoint->z - Pos->z );
			Distance = VectorLength( &e );
			BSPFace.nx = FaceNormal->nx;
			BSPFace.ny = FaceNormal->ny;
			BSPFace.nz = FaceNormal->nz;
			BSPFace.D = -DotProduct( (VECTOR *) FaceNormal, ImpactPoint );
			CollFace = &BSPFace;
		}
		else
		{
			if( !collided ) return (false);
		   	
			FaceNormal->nx = CollFace->nx;
			FaceNormal->ny = CollFace->ny;
			FaceNormal->nz = CollFace->nz;
			C = CollFace->D;
		   	
			e.x = Pos->x + Dir->x;	
			e.y = Pos->y + Dir->y;	
			e.z = Pos->z + Dir->z;	
		   	
			nDOTe =	( ( e.x * FaceNormal->nx ) + ( e.y * FaceNormal->ny ) + ( e.z * FaceNormal->nz ) ) + C;
		   		
			D = ( (float) fabsf(nDOTe) ) + COLLISION_FUDGE ; 
		   	
			Dn.x = D * FaceNormal->nx; 
			Dn.y = D * FaceNormal->ny; 
			Dn.z = D * FaceNormal->nz; 
		   	
			Pos_New->x = e.x + Dn.x;
			Pos_New->y = e.y + Dn.y;
			Pos_New->z = e.z + Dn.z;
		}
	}
	else
	{
		if( !collided ) return (false);
	   	
		FaceNormal->nx = CollFace->nx;
		FaceNormal->ny = CollFace->ny;
		FaceNormal->nz = CollFace->nz;
		C = CollFace->D;
	   	
		e.x = Pos->x + Dir->x;	
		e.y = Pos->y + Dir->y;	
		e.z = Pos->z + Dir->z;	
	   	
		nDOTe =	( ( e.x * FaceNormal->nx ) + ( e.y * FaceNormal->ny ) + ( e.z * FaceNormal->nz ) ) + C;
	   		
		D = ( (float) fabsf(nDOTe) ) + COLLISION_FUDGE ; 
	   	
		Dn.x = D * FaceNormal->nx; 
		Dn.y = D * FaceNormal->ny; 
		Dn.z = D * FaceNormal->nz; 
	   	
		Pos_New->x = e.x + Dn.x;
		Pos_New->y = e.y + Dn.y;
		Pos_New->z = e.z + Dn.z;
	}
	return(true);
}


/*===================================================================
	Procedure	:	Does the collision between the portals in a specified group
	Input		:	MLOADHEADER *
				:	u_int16_t group
				:	VECTOR * Pos
				:	VECTOR * Dir
				:	VECTOR * ImpactPoint
				:	NORMAL * FaceNormal
				:	u_int16_t * PortalHit
  Output		:	bool
===================================================================*/
bool OneGroupPortalCol( MLOADHEADER * /*Mloadheader*/ , u_int16_t group ,
						VECTOR * Pos, VECTOR * Dir  ,
						VECTOR * ImpactPoint , NORMAL * FaceNormal, u_int16_t *Next_Group,
						int /*collisionhint*/ )
{
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	float		Distance = 0.0f;
	u_int16_t		flag = 0;	
	bool		res;
	BSP_PORTAL_GROUP	*pg;
	BSP_PORTAL			*bp;
	int					j;
	
	if( group == (u_int16_t) -1)
		return false;

	Origin.x = Pos->x;
	Origin.y = Pos->y;
	Origin.z = Pos->z;
	ODir.x = Dir->x;
	ODir.y = Dir->y;
	ODir.z = Dir->z;

	pg = &Bsp_Portal_Header.group[ group ];
	for ( j = 0; j < pg->portals; j++ )
	{
		bp = &pg->portal[ j ];
		res = ColRayPlaneIntersect( &bp->normal, bp->offset );
		if( res == true )
		{
			if ( PISDistRecursive( &IPoint, bp->bsp.Root ) )
			{
				if ( flag == 0 )
				{
					Distance = IDist;
					ImpactPoint->x = IPoint.x;
					ImpactPoint->y = IPoint.y;
					ImpactPoint->z = IPoint.z;
					FaceNormal->nx = bp->normal.x;
					FaceNormal->ny = bp->normal.y;
					FaceNormal->nz = bp->normal.z;
					hit_portal_offset = bp->offset;
					*Next_Group = bp->group;
					flag = 1;
				}else{
					if ( IDist < Distance )
					{
						Distance = IDist;
						ImpactPoint->x = IPoint.x;
						ImpactPoint->y = IPoint.y;
						ImpactPoint->z = IPoint.z;
						FaceNormal->nx = bp->normal.x;
						FaceNormal->ny = bp->normal.y;
						FaceNormal->nz = bp->normal.z;
						hit_portal_offset = bp->offset;
						*Next_Group = bp->group;
					}
				}
			}
		}
	}

	return ( flag ) ? true : false;
}

bool AmIOutsideGroup( MLOADHEADER * m, VECTOR * EndPos, u_int16_t EndGroup )
{
	VECTOR	dv;

	dv.x = (float) fabsf( EndPos->x - m->Group[ EndGroup ].center.x );
	dv.y = (float) fabsf( EndPos->y - m->Group[ EndGroup ].center.y );
	dv.z = (float) fabsf( EndPos->z - m->Group[ EndGroup ].center.z );
	return( dv.x > m->Group[ EndGroup ].half_size.x + OUTSIDE_GROUP_TOLERANCE ||
			dv.y > m->Group[ EndGroup ].half_size.y + OUTSIDE_GROUP_TOLERANCE ||
			dv.z > m->Group[ EndGroup ].half_size.z + OUTSIDE_GROUP_TOLERANCE );
}

static BGOBJECT	*	CurParent = NULL;

/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
/*===================================================================
	Procedure	:	Check Ship to Background Objects Collision
	Input		:	u_int16_t	Group
	Output		:	Nothing
===================================================================*/
/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
bool CheckBGObjectsCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject )
{
	float			D;
	VECTOR			Dn;
	float			C;
	VECTOR			e;
	float			nDOTe;
	BGOBJECT	*	Object;
	VECTOR			NormDir;
	float			RayLength;

	ColDist = Distance;
	ColChild = NULL;
	ColCollided = collided;
	NormDir = ODir;
	RayLength = VectorLength( &NormDir );
	NormaliseVector( &NormDir );

	Object = FirstBGObjectUsed;

	while( Object != NULL )
	{
		CurParent = Object;

		if( !SoundInfo[ Object->Group ][ Group ] )
		{
			if( Object->NumChildren && Object->Children )
			{
				if( RaytoSphereShort( &Object->ColCenter, ( Object->ColRadius + ( SHIP_RADIUS * 1.5F ) ), &Origin, &NormDir, RayLength ) )
				{
					CollideBGOChildren( Object->Children, Object->NumChildren );
				}
			}
		}

		Object = Object->NextUsed;
	}

	if( ColChild )
	{
		if( BGObject  )
		{
			if( ColChild->ColZones[ ColZoneNum ].Sensitive )
			{
				*BGObject = ColParent;
			}
			else
			{
				if( ColSide->Sensitive )
				{
					*BGObject = ColParent;
				}
				else
				{
					*BGObject = NULL;
				}
			}
		}

		ApplyMatrix( &ColChild->DisplayMatrix, &ColPoint, ImpactPoint );
		ImpactPoint->x += ColChild->DisplayPos.x;
		ImpactPoint->y += ColChild->DisplayPos.y;
		ImpactPoint->z += ColChild->DisplayPos.z;
		ApplyMatrix( &ColChild->DisplayMatrix, &ColSide->Normal, (VECTOR *) FaceNormal );

		C = -( ( FaceNormal->nx * ImpactPoint->x ) + ( FaceNormal->ny * ImpactPoint->y ) + ( FaceNormal->nz * ImpactPoint->z ) );

		e.x = Origin.x + ODir.x;	
		e.y = Origin.y + ODir.y;	
		e.z = Origin.z + ODir.z;	
	
		nDOTe =	( ( e.x * FaceNormal->nx ) + ( e.y * FaceNormal->ny ) + ( e.z * FaceNormal->nz ) ) + C;
		
		D = ( (float) fabsf(nDOTe) ) + COLLISION_FUDGE ; 
	
		Dn.x = D * FaceNormal->nx; 
		Dn.y = D * FaceNormal->ny; 
		Dn.z = D * FaceNormal->nz; 
	
		New_Pos->x = e.x + Dn.x;
		New_Pos->y = e.y + Dn.y;
		New_Pos->z = e.z + Dn.z;
		return( true );
	}
	else
	{
		if( BGObject )
		{
			*BGObject = NULL;
		}
	}

	return( false );
}

/*===================================================================
	Procedure	:	Check Ship to RestartPoints Collision
	Input		:	u_int16_t		Group
				:	float		Distance to intersection
				:	VECTOR	*	ImpactPoint
				:	int			collided
				:	VECTOR	*	New_Pos
				:	NORMAL	*	Collision Normal
				:	BGOBJECT ** Bgobject
	Output		:	bool		True/False
===================================================================*/
bool CheckRestartPointCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject )
{
	float			D;
	VECTOR			Dn;
	float			C;
	VECTOR			e;
	float			nDOTe;

	RESTART		*	Object;

	ColDist = Distance;
	ColChild = NULL;
	ColCollided = collided;
	CurParent = NULL;

	Object = FirstRestartUsed;

	while( Object != NULL )
	{
		if( !SoundInfo[ Object->Group ][ Group ] )
		{
			if( Object->Components )
			{
				CollideBGOChildren( Object->Components, 1 );
			}
		}

		Object = Object->NextUsed;
	}

	if( ColChild )
	{
		ApplyMatrix( &ColChild->DisplayMatrix, &ColPoint, ImpactPoint );
		ImpactPoint->x += ColChild->DisplayPos.x;
		ImpactPoint->y += ColChild->DisplayPos.y;
		ImpactPoint->z += ColChild->DisplayPos.z;
		ApplyMatrix( &ColChild->DisplayMatrix, &ColSide->Normal, (VECTOR *) FaceNormal );

		C = -( ( FaceNormal->nx * ImpactPoint->x ) + ( FaceNormal->ny * ImpactPoint->y ) + ( FaceNormal->nz * ImpactPoint->z ) );

		e.x = Origin.x + ODir.x;	
		e.y = Origin.y + ODir.y;	
		e.z = Origin.z + ODir.z;	
	
		nDOTe =	( ( e.x * FaceNormal->nx ) + ( e.y * FaceNormal->ny ) + ( e.z * FaceNormal->nz ) ) + C;
		
		D = ( (float) fabsf(nDOTe) ) + COLLISION_FUDGE ; 
	
		Dn.x = D * FaceNormal->nx; 
		Dn.y = D * FaceNormal->ny; 
		Dn.z = D * FaceNormal->nz; 
	
		New_Pos->x = e.x + Dn.x;
		New_Pos->y = e.y + Dn.y;
		New_Pos->z = e.z + Dn.z;
		return( true );
	}

	if( BGObject )
	{
		*BGObject = NULL;
	}

	return( false );
}

/*===================================================================
	Procedure	:	Check Ray to Enemy Polygonal Collision
	Input		:	u_int16_t		Group
				:	float		Distance to intersection
				:	VECTOR	*	ImpactPoint
				:	int			collided
				:	VECTOR	*	New_Pos
				:	NORMAL	*	Collision Normal
				:	BGOBJECT ** Bgobject
	Output		:	bool		True/False
===================================================================*/
bool CheckEnemyPolyCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** /*BGObject*/ )
{
	float			D;
	VECTOR			Dn;
	float			C;
	VECTOR			e;
	float			nDOTe;
	ENEMY		*	Object;
	int				OldColCollided;
	u_int16_t			EnemyHit = (u_int16_t) -1;

	ColDist = Distance;
	ColChild = NULL;
	ColCollided = collided;
	CurParent = NULL;

	Object = FirstEnemyUsed;

	while( Object != NULL )
	{
		if( ( Object->Status & ENEMY_STATUS_Enable ) && ( Object->CompCollision == COMPONENTCOLLISION ) )
		{
			if( Object->Type == ENEMY_Boss_Avatar )
			{
				if( !IsAvatarActive() )
					goto SkipIt;
			}

			if( !SoundInfo[ Object->Object.Group ][ Group ] )
			{
				if( Object->Object.Components )
				{
					OldColCollided = ColCollided;

					CollideBGOChildren( Object->Object.Components, 1 );

					if( OldColCollided != ColCollided )
					{
						EnemyHit = Object->Index;
					}
				}
			}
		}

SkipIt:
		Object = Object->NextUsed;
	}

	if( ColChild )
	{
		ApplyMatrix( &ColChild->DisplayMatrix, &ColPoint, ImpactPoint );
		ImpactPoint->x += ColChild->DisplayPos.x;
		ImpactPoint->y += ColChild->DisplayPos.y;
		ImpactPoint->z += ColChild->DisplayPos.z;
		ApplyMatrix( &ColChild->DisplayMatrix, &ColSide->Normal, (VECTOR *) FaceNormal );

		C = -( ( FaceNormal->nx * ImpactPoint->x ) + ( FaceNormal->ny * ImpactPoint->y ) + ( FaceNormal->nz * ImpactPoint->z ) );

		e.x = Origin.x + ODir.x;	
		e.y = Origin.y + ODir.y;	
		e.z = Origin.z + ODir.z;	
	
		nDOTe =	( ( e.x * FaceNormal->nx ) + ( e.y * FaceNormal->ny ) + ( e.z * FaceNormal->nz ) ) + C;
		
		D = ( (float) fabsf(nDOTe) ) + COLLISION_FUDGE ; 
	
		Dn.x = D * FaceNormal->nx; 
		Dn.y = D * FaceNormal->ny; 
		Dn.z = D * FaceNormal->nz; 
	
		New_Pos->x = e.x + Dn.x;
		New_Pos->y = e.y + Dn.y;
		New_Pos->z = e.z + Dn.z;

		AnyCompEnemyHit = EnemyHit;

		if( ( ColChild->ColZones[ ColZoneNum ].Sensitive ) ||
			( !( ColChild->ColZones[ ColZoneNum ].Sensitive ) && ( ColSide->Sensitive ) ) )
		{
			CompEnemyHit = EnemyHit;
			CompEnemyHitPos.x = ImpactPoint->x;
			CompEnemyHitPos.y = ImpactPoint->y;
			CompEnemyHitPos.z = ImpactPoint->z;
			CompEnemyHitNormal.nx = FaceNormal->nx;
			CompEnemyHitNormal.ny = FaceNormal->ny;
			CompEnemyHitNormal.nz = FaceNormal->nz;
			CompEnemyHitDist = ColDist;
		}
		return( true );
	}

	return( false );
}

/*===================================================================
	Procedure	:	Process components of Background Object
	Input		:	COMP_OBJ	*	Children
				:	int16_t			NumChildren
	Output		:	Nothing
===================================================================*/
void CollideBGOChildren( COMP_OBJ * Children, int16_t NumChildren )
{
	int16_t					Zone;
	int16_t					Count;
	VECTOR					NewOrigin;
	VECTOR					NewDir;
	VECTOR					NewEnd;
	ANI_ZONES			*	ZonesPtr;
	ANI_ZONE_BOX		*	AniZoneBoxPtr;
	ANI_ZONE_POLYGONAL	*	AniZonePolyPtr;
	VECTOR					IntPoint;
	ZONESIDE			*	IntSide;
	float					DistToInt;
	VECTOR					TempVector;

	if( Children )
	{
		for( Count = 0; Count < NumChildren; Count++ )
		{
			if( Children->NumColZones && ( Children->Flags & COMPFLAG_Enable ) )
			{
				ApplyMatrix( &Children->InvDisplayMatrix, &ODir, &NewDir );
				NewOrigin.x = ( Origin.x - Children->DisplayPos.x );
				NewOrigin.y = ( Origin.y - Children->DisplayPos.y );
				NewOrigin.z = ( Origin.z - Children->DisplayPos.z );
				ApplyMatrix( &Children->InvDisplayMatrix, &NewOrigin, &NewOrigin );

				NewEnd.x = ( NewOrigin.x + NewDir.x );
				NewEnd.y = ( NewOrigin.y + NewDir.y );
				NewEnd.z = ( NewOrigin.z + NewDir.z );

				ZonesPtr = Children->ColZones;

				for( Zone = 0; Zone < Children->NumColZones; Zone++ )
				{
					switch( ZonesPtr->Type )
					{
						case ZONE_Sphere:
							/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
							break;
	
						case ZONE_Box:
							AniZoneBoxPtr = (ANI_ZONE_BOX *) ZonesPtr->Data;

							if( RayToColZone( &NewOrigin, &NewEnd,
											  AniZoneBoxPtr->Sides, AniZoneBoxPtr->NumSides,
											  &IntPoint, &IntSide, ColRadius ) )
							{
								TempVector.x = ( IntPoint.x - NewOrigin.x );
								TempVector.y = ( IntPoint.y - NewOrigin.y );
								TempVector.z = ( IntPoint.z - NewOrigin.z );
								DistToInt = VectorLength( &TempVector );

								if( !ColCollided )
								{
									ColPoint = IntPoint;
									ColSide = IntSide;
									ColDist = DistToInt;
									ColChild = Children;
									ColParent = CurParent;
									ColZoneNum = Zone;
									ColCollided++;
								}
								else
								{
									if( DistToInt < ColDist )
									{
										ColPoint = IntPoint;
										ColSide = IntSide;
										ColDist = DistToInt;
										ColChild = Children;
										ColParent = CurParent;
										ColZoneNum = Zone;
										ColCollided++;
									}
								}

							/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
							}
							break;
	
						case ZONE_Polygonal:
							AniZonePolyPtr = (ANI_ZONE_POLYGONAL *) ZonesPtr->Data;

							if( RayToColZone( &NewOrigin, &NewEnd,
											  AniZonePolyPtr->Sides, AniZonePolyPtr->NumSides,
											  &IntPoint, &IntSide, ColRadius ) )
							{
								TempVector.x = ( IntPoint.x - NewOrigin.x );
								TempVector.y = ( IntPoint.y - NewOrigin.y );
								TempVector.z = ( IntPoint.z - NewOrigin.z );
								DistToInt = VectorLength( &TempVector );

								if( !ColCollided )
								{
									ColPoint = IntPoint;
									ColSide = IntSide;
									ColDist = DistToInt;
									ColChild = Children;
									ColParent = CurParent;
									ColZoneNum = Zone;
									ColCollided++;
								}
								else
								{
									if( DistToInt < ColDist )
									{
										ColPoint = IntPoint;
										ColSide = IntSide;
										ColDist = DistToInt;
										ColChild = Children;
										ColZoneNum = Zone;
										ColParent = CurParent;
										ColCollided++;
									}
								}
							}
							break;
					}

					ZonesPtr++;
				}
			}
					
			if( Children->NumChildren )
			{
				CollideBGOChildren( Children->Children, Children->NumChildren );
			}

			Children++;
		}
	}
}

/*===================================================================
	Procedure	:	Does the collision between a specified group
	Input		:	float		Distance to int
				:	int16_t		Collided
				:	u_int16_t		group
				:	VECTOR	*	Pos
				:	VECTOR	*	Dir
				:	VECTOR	*	ImpactPoint
				:	NORMAL	*	FaceNormal
				:	VECTOR	*	Pos_New
				:	BGOBJECT **	BGObject collided with
				:	float		Radius
  Output		:	bool
===================================================================*/
bool OneGroupBGObjectCol( float Distance, int16_t Collided, u_int16_t Group, VECTOR * Pos, VECTOR * Dir  ,
					 VECTOR * ImpactPoint , NORMAL  * FaceNormal , VECTOR * Pos_New, BGOBJECT ** BGObject, float Radius )
{

	int16_t	ColCount = 0;

	if( Group == (u_int16_t) -1) return false;

	Origin = *Pos;
	ODir = *Dir;
	ColRadius = Radius;
	AnyCompEnemyHit = (u_int16_t) -1;
	CompEnemyHit = (u_int16_t) -1;

	if(	CheckEnemyPolyCol( Group, Distance, ImpactPoint, Collided, Pos_New, FaceNormal, BGObject ) )
	{
		Distance = ColDist;
		ColCount++;
	}

	if( CheckBGObjectsCol( Group, Distance, ImpactPoint, ( Collided + ColCount ), Pos_New, FaceNormal, BGObject ) )
	{
		ColCount++;
	}

	if( !ColCount ) return( false );


	return(true);
}

/*===================================================================
	Procedure	:	Check Background Objects To Object Collision
	Input		:	VECTOR	*	Pos
				:	BGOBJECT *	Object
				:	VECTOR	*	Push Vector ( TBFI )
				:	float	*	DamagePtr
				:	float		Radius
	Output		:	Nothing
===================================================================*/
bool CheckBGObjectCollision( VECTOR * Pos, BGOBJECT * Object, VECTOR * PushVector, float * DamagePtr, float Radius )
{
	float	Speed;

	ColChild = NULL;
	ColCollided = 0;
	Origin = *Pos;
	ColRadius = Radius;

	if( !PointToSphere( (VERT *) &Object->ColCenter, ( Object->ColRadius +  ( SHIP_RADIUS * 1.5F ) ), (VERT *) Pos ) )
		return( false );

	if( Object->NumChildren && Object->Children )
	{
		CollideBGOToCompObjChildren( Object->Children, Object->NumChildren );

		if( ColChild )
		{
			if( ColChild->ColZones[ ColZoneNum ].Damage )
			{
				*DamagePtr = ColChild->ColZones[ ColZoneNum ].Damage;
			}
			else
			{
				if( ColSide->Damage )
				{
					*DamagePtr = ColSide->Damage;
				}
				else
				{
					*DamagePtr = 0.0F;
				}
			}

			ApplyMatrix( &ColChild->DisplayMatrix, &ColDir, &ColDir );
			PushVector->x = ( ColDir.x * -1.0F );
			PushVector->y = ( ColDir.y * -1.0F );
			PushVector->z = ( ColDir.z * -1.0F );
			Speed = VectorLength( PushVector );
			Speed *= 0.5F;
			NormaliseVector( PushVector );
			PushVector->x *= Speed;
			PushVector->y *= Speed;
			PushVector->z *= Speed;
			return( true );
		}
	}

	return( false );
}

/*===================================================================
	Procedure	:	Check Background Objects To Enemy Collision
	Input		:	VECTOR	*	Pos
				:	COMP_OBJ *	CompObjects
				:	VECTOR	*	Push Vector ( TBFI )
				:	float	*	DamagePtr
				:	float		Radius
	Output		:	Nothing
===================================================================*/
bool CheckCompObjectCollision( VECTOR * Pos, COMP_OBJ * Comps, VECTOR * PushVector, float * DamagePtr, float Radius )
{
	float	Speed;

	ColChild = NULL;
	ColCollided = 0;
	Origin = *Pos;
	ColRadius = Radius;

	if( Comps )
	{
		CollideBGOToCompObjChildren( Comps, 1 );

		if( ColChild )
		{
			if( ColChild->ColZones[ ColZoneNum ].Damage )
			{
				*DamagePtr = ColChild->ColZones[ ColZoneNum ].Damage;
			}
			else
			{
				if( ColSide->Damage )
				{
					*DamagePtr = ColSide->Damage;
				}
				else
				{
					*DamagePtr = 0.0F;
				}
			}

			ApplyMatrix( &ColChild->DisplayMatrix, &ColDir, &ColDir );
			PushVector->x = ( ColDir.x * -1.0F );
			PushVector->y = ( ColDir.y * -1.0F );
			PushVector->z = ( ColDir.z * -1.0F );
			Speed = VectorLength( PushVector );
			Speed *= 0.5F;
			NormaliseVector( PushVector );
			PushVector->x *= Speed;
			PushVector->y *= Speed;
			PushVector->z *= Speed;
			return( true );
		}
	}

	return( false );
}

/*===================================================================
	Procedure	:	Process components of Background Object
	Input		:	COMP_OBJ	*	Children
				:	int16_t			NumChildren
	Output		:	Nothing
===================================================================*/
void CollideBGOToCompObjChildren( COMP_OBJ * Children, int16_t NumChildren )
{
	int16_t					Zone;
	int16_t					Count;
	VECTOR					NewOrigin;
	VECTOR					NewDir;
	VECTOR					NewEnd;
	ANI_ZONES			*	ZonesPtr;
	ANI_ZONE_BOX		*	AniZoneBoxPtr;
	ANI_ZONE_POLYGONAL	*	AniZonePolyPtr;
	VECTOR					IntPoint;
	ZONESIDE			*	IntSide;
	float					DistToInt;
	VECTOR					TempVector;
	VECTOR					OldOrigin;
	float					Length;

	if( Children )
	{
		for( Count = 0; Count < NumChildren; Count++ )
		{
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			if( Children->NumColZones && ( Children->Flags & COMPFLAG_Enable ) )
			{
				OldOrigin.x = ( Origin.x - Children->OldDisplayPos.x );
				OldOrigin.y = ( Origin.y - Children->OldDisplayPos.y );
				OldOrigin.z = ( Origin.z - Children->OldDisplayPos.z );
				ApplyMatrix( &Children->OldInvDisplayMatrix, &OldOrigin, &OldOrigin );

				NewOrigin.x = ( Origin.x - Children->DisplayPos.x );
				NewOrigin.y = ( Origin.y - Children->DisplayPos.y );
				NewOrigin.z = ( Origin.z - Children->DisplayPos.z );
				ApplyMatrix( &Children->InvDisplayMatrix, &NewOrigin, &NewOrigin );

				NewDir.x = ( NewOrigin.x - OldOrigin.x );
				NewDir.y = ( NewOrigin.y - OldOrigin.y );
				NewDir.z = ( NewOrigin.z - OldOrigin.z );
	
				NewOrigin = OldOrigin;

				Length = VectorLength( &NewDir );
				if( Length < 0.065F )
				{
					Length = 0.065F;
					NormaliseVector( &NewDir );
					NewDir.x *= Length;
					NewDir.y *= Length;
					NewDir.z *= Length;
				}

				NewEnd.x = ( NewOrigin.x + NewDir.x );
				NewEnd.y = ( NewOrigin.y + NewDir.y );
				NewEnd.z = ( NewOrigin.z + NewDir.z );

				ZonesPtr = Children->ColZones;

				for( Zone = 0; Zone < Children->NumColZones; Zone++ )
				{
					switch( ZonesPtr->Type )
					{
						case ZONE_Sphere:
							/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
							break;
	
						case ZONE_Box:
							AniZoneBoxPtr = (ANI_ZONE_BOX *) ZonesPtr->Data;

							if( RayToColZone( &NewOrigin, &NewEnd,
											  AniZoneBoxPtr->Sides, AniZoneBoxPtr->NumSides,
											  &IntPoint, &IntSide, ColRadius ) )
							{
								TempVector.x = ( IntPoint.x - NewOrigin.x );
								TempVector.y = ( IntPoint.y - NewOrigin.y );
								TempVector.z = ( IntPoint.z - NewOrigin.z );
								DistToInt = VectorLength( &TempVector );

								if( !ColCollided )
								{
									ColPoint = IntPoint;
									ColSide = IntSide;
									ColDist = DistToInt;
									ColChild = Children;
									ColDir = NewDir;
									ColZoneNum = Zone;
									ColParent = CurParent;
									ColCollided++;
								}
								else
								{
									if( DistToInt < ColDist )
									{
										ColPoint = IntPoint;
										ColSide = IntSide;
										ColDist = DistToInt;
										ColChild = Children;
										ColDir = NewDir;
										ColParent = CurParent;
										ColZoneNum = Zone;
									}
								}

							}
							/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
							break;
	
						case ZONE_Polygonal:
							AniZonePolyPtr = (ANI_ZONE_POLYGONAL *) ZonesPtr->Data;

							if( RayToColZone( &NewOrigin, &NewEnd,
											  AniZonePolyPtr->Sides, AniZonePolyPtr->NumSides,
											  &IntPoint, &IntSide, ColRadius ) )
							{
								TempVector.x = ( IntPoint.x - NewOrigin.x );
								TempVector.y = ( IntPoint.y - NewOrigin.y );
								TempVector.z = ( IntPoint.z - NewOrigin.z );
								DistToInt = VectorLength( &TempVector );

								if( !ColCollided )
								{
									ColPoint = IntPoint;
									ColSide = IntSide;
									ColDist = DistToInt;
									ColChild = Children;
									ColDir = NewDir;
									ColParent = CurParent;
									ColZoneNum = Zone;
									ColCollided++;
								}
								else
								{
									if( DistToInt < ColDist )
									{
										ColPoint = IntPoint;
										ColSide = IntSide;
										ColDist = DistToInt;
										ColChild = Children;
										ColDir = NewDir;
										ColParent = CurParent;
										ColZoneNum = Zone;
									}
								}
							}
							break;
					}

					ZonesPtr++;
				}
			}
					
			if( Children->NumChildren )
			{
				CollideBGOToCompObjChildren( Children->Children, Children->NumChildren );
			}

			Children++;
		}
	}
}


static bool move_object = true;
static bool bounce_object = true;
static bool NoBGObject = true;


constexpr float BACKFACING_PLANE_TOLERANCE = (-0.001F);
constexpr float IMPACT_OFFSET = (0.1F);
constexpr float FEELER_LENGTH_TO_RADIUS_RATIO = (1.25F);


bool ObjectCollide( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject )
{
	VECTOR Up, Right;
	QUAT MoveQuat;
	MATRIX MoveMat;
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	int f;
	float dx0, dy0;
	float dx1, dy1;
	VECTOR Feeler;
	static struct
	{
		struct {
			float dx, dy;
		} start, end;
	} FeelerOffset[ MAX_FEELER_RAYS ] =
	{
		/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
		{ 0.0F, 0.0F,  0.0F, 0.0F },
		{ 0.0F, 0.0F,  1.0F, 1.0F },
		{ 0.0F, 0.0F,  1.0F,-1.0F },
		{ 0.0F, 0.0F, -1.0F,-1.0F },
		{ 0.0F, 0.0F, -1.0F, 1.0F },
		{ 0.0F, 1.0F,  0.0F,-1.0F },
		{ 0.0F,-1.0F,  0.0F, 1.0F },
		{ 1.0F, 0.0F, -1.0F, 0.0F },
		{-1.0F, 0.0F,  1.0F, 0.0F },
	};
	VECTOR FeelerImpactPoint;
	u_int16_t FeelerImpactGroup;
	NORMAL FeelerFaceNormal;
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	VECTOR FeelerPos_New;
	BGOBJECT *FeelerBGObject;
	float FeelerImpactDist;
	float ImpactDist;
	VECTOR Move;
	float MoveDist;
	VECTOR Move_Dir;
	VECTOR New_Off;
	float FeelerPlane;
	float FeelerDist;
	VECTOR FeelerEndPoint;
	float Num, Div;
	VECTOR FeelerStart;
	u_int16_t FeelerGroup;
	NORMAL ImpactNormal;
	float FeelerLength = FEELER_LENGTH_TO_RADIUS_RATIO * radius;
	VECTOR StartPos;
	VECTOR Pos_New; ZERO_STACK_MEM(Pos_New);
	bool hit;
	VECTOR ImpactPoint;
	
	hit = false;
	StartPos = Obj->Pos;
	Move_Dir = *Move_Off;
	NormaliseVector( &Move_Dir );
	MoveQuat.FromVector2(&Move_Dir);
	MoveQuat.ToMatrix(&MoveMat);
	ApplyMatrix( &MoveMat, &SlideUp, &Up );
	ApplyMatrix( &MoveMat, &SlideRight, &Right );
	MoveDist = VectorLength( Move_Off );
	ImpactDist = MoveDist;
	ImpactPoint.x = StartPos.x + Move_Off->x;
	ImpactPoint.y = StartPos.y + Move_Off->y;
	ImpactPoint.z = StartPos.z + Move_Off->z;
	for ( f = 0; f < MAX_FEELER_RAYS; f++ )
	{
		dx0 = FeelerOffset[ f ].start.dx;
		dy0 = FeelerOffset[ f ].start.dy;
		dx1 = FeelerOffset[ f ].end.dx;
		dy1 = FeelerOffset[ f ].end.dy;
		FeelerStart.x = radius * ( dx0 * Right.x + dy0 * Up.x );
		FeelerStart.y = radius * ( dx0 * Right.y + dy0 * Up.y );
		FeelerStart.z = radius * ( dx0 * Right.z + dy0 * Up.z );
		FeelerGroup = Obj->Group;
		FeelerStart.x += StartPos.x;
		FeelerStart.y += StartPos.y;
		FeelerStart.z += StartPos.z;
		Feeler = Move_Dir;
		Feeler.x *= ( MoveDist + FeelerLength );
		Feeler.y *= ( MoveDist + FeelerLength );
		Feeler.z *= ( MoveDist + FeelerLength );
		Feeler.x += radius * ( dx1 * Right.x + dy1 * Up.x );
		Feeler.y += radius * ( dx1 * Right.y + dy1 * Up.y );
		Feeler.z += radius * ( dx1 * Right.z + dy1 * Up.z );
		if ( BackgroundCollide( &MCloadheadert0, &Mloadheader,
			&FeelerStart, FeelerGroup, &Feeler,
			&FeelerImpactPoint , &FeelerImpactGroup, &FeelerFaceNormal, &FeelerPos_New, NoBGObject, ( BGObject ) ? &FeelerBGObject : NULL ) )
		{
			FeelerPlane = -DotProduct( (VECTOR *)&FeelerFaceNormal, &FeelerImpactPoint ) - radius;
			Div = DotProduct( &Move_Dir , (VECTOR *)&FeelerFaceNormal );
			if( Div >= BACKFACING_PLANE_TOLERANCE )
			{
				continue;
			}
			Num = ( FeelerPlane + DotProduct( &StartPos, (VECTOR *)&FeelerFaceNormal) );
			FeelerImpactDist = -( Num / Div );
			if ( FeelerImpactDist > IMPACT_OFFSET )
			{
				FeelerImpactDist -= IMPACT_OFFSET;
			}
			else
			{
				FeelerImpactDist = 0.0F;
			}
			FeelerImpactPoint.x = StartPos.x + Move_Dir.x * FeelerImpactDist;
			FeelerImpactPoint.y = StartPos.y + Move_Dir.y * FeelerImpactDist;
			FeelerImpactPoint.z = StartPos.z + Move_Dir.z * FeelerImpactDist;
			if ( ImpactDist > FeelerImpactDist )
			{
				FeelerEndPoint.x = StartPos.x + Move_Off->x;
				FeelerEndPoint.y = StartPos.y + Move_Off->y;
				FeelerEndPoint.z = StartPos.z + Move_Off->z;
				FeelerDist = ( DotProduct( &FeelerEndPoint, (VECTOR *)&FeelerFaceNormal ) + FeelerPlane );
				FeelerDist = -FeelerDist;
				FeelerPos_New.x = FeelerEndPoint.x + FeelerFaceNormal.nx * FeelerDist;
				FeelerPos_New.y = FeelerEndPoint.y + FeelerFaceNormal.ny * FeelerDist;
				FeelerPos_New.z = FeelerEndPoint.z + FeelerFaceNormal.nz * FeelerDist;
				ImpactDist = FeelerImpactDist;
				ImpactPoint = FeelerImpactPoint;
				ImpactNormal = FeelerFaceNormal;
				Pos_New = FeelerPos_New;
				if( BGObject )
					*BGObject = FeelerBGObject;
			}
		}
	}
	if ( ImpactDist < MoveDist )
	{
		if ( move_object && bounce_object )
		{
			static MATRIX InvMat;
			float MaxPerpendicularAccel;
			VECTOR BumpForce;

			Move = Obj->Speed;
			MoveDist = VectorLength( &Move ) * framelag;
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			MaxPerpendicularAccel = TurboAccell * MoveDist;
			BumpForce.x = ImpactNormal.nx * MaxPerpendicularAccel;
			BumpForce.y = ImpactNormal.ny * MaxPerpendicularAccel;
			BumpForce.z = ImpactNormal.nz * MaxPerpendicularAccel;
			New_Off.x = Pos_New.x - ImpactPoint.x;
			New_Off.y = Pos_New.y - ImpactPoint.y;
			New_Off.z = Pos_New.z - ImpactPoint.z;
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			NormaliseVector( &New_Off );
			if( framelag )
			{
				MoveDist /= framelag;
			}
			else
			{
				MoveDist = 0.0F;
			}
			New_Off.x *= MoveDist;
			New_Off.y *= MoveDist;
			New_Off.z *= MoveDist;
			MatrixTranspose( &Obj->Mat, &InvMat );
			ApplyMatrix( &InvMat, &New_Off, &Move ); // modify ship's speed
			Obj->Speed = Move;
			if ( DotProduct( &Obj->ExternalForce, (VECTOR *) &ImpactNormal ) < 0.0F )
				ReflectVector( &Obj->ExternalForce, &ImpactNormal, &Obj->ExternalForce );
			ObjForceExternalOneOff( Obj, &BumpForce );
			if ( ImpactDist < 0.0F )
			{
				ImpactPoint = StartPos;
				DebugPrintf( "impactdist = %f, no movement\n", ImpactDist );
			}
		}
		hit = true;
	}
	Move_Off->x = ImpactPoint.x - StartPos.x;
	Move_Off->y = ImpactPoint.y - StartPos.y;
	Move_Off->z = ImpactPoint.z - StartPos.z;
	if ( move_object )
	{
		Obj->Pos = ImpactPoint;
		Obj->Group = MoveGroup( &Mloadheader, &StartPos, Obj->Group, Move_Off );
	}

	return hit;
}


bool WouldObjectCollide( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject )
{
	bool collide;

	move_object = false;
	collide = ObjectCollide( Obj, Move_Off, radius, BGObject );
	move_object = true;

	return collide;
}


bool ObjectCollideNoBounce( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject )
{
	bool collide;

	bounce_object = false;
	collide = ObjectCollide( Obj, Move_Off, radius, BGObject );
	bounce_object = true;

	return collide;
}


bool QCollide( VECTOR *Start_Pos, u_int16_t Start_Group, VECTOR *Move_Off, float radius, VECTOR *Impact_Point, u_int16_t *Impact_Group, NORMAL *Impact_Normal )
{
	VECTOR Up, Right;
	QUAT MoveQuat;
	MATRIX MoveMat;
	int f;
	float dx0, dy0;
	float dx1, dy1;
	VECTOR Feeler;
	static struct
	{
		struct {
			float dx, dy;
		} start, end;
	} FeelerOffset[ MAX_QFEELER_RAYS ] =
	{
		{ 0.0F, 0.0F,  0.0F, 0.0F },
		{ 0.0F, 0.0F,  1.0F, 1.0F },
		{ 0.0F, 0.0F,  1.0F,-1.0F },
		{ 0.0F, 0.0F, -1.0F,-1.0F },
		{ 0.0F, 0.0F, -1.0F, 1.0F },
	};
	VECTOR FeelerImpactPoint;
	u_int16_t FeelerImpactGroup;
	NORMAL FeelerFaceNormal;
	VECTOR FeelerPos_New;
	float FeelerImpactDist;
	float ImpactDist;
	VECTOR Move;
	float MoveDist;
	VECTOR Move_Dir;
	float FeelerPlane;
	float FeelerDist;
	VECTOR FeelerEndPoint;
	float Num, Div;
	VECTOR FeelerStart;
	u_int16_t FeelerGroup;
	NORMAL ImpactNormal; ZERO_STACK_MEM(ImpactNormal);
	float FeelerLength = FEELER_LENGTH_TO_RADIUS_RATIO * radius;
	VECTOR StartPos;
	bool hit;
	VECTOR ImpactPoint;
	
	hit = false;
	StartPos = *Start_Pos;
	Move_Dir = *Move_Off;
	NormaliseVector( &Move_Dir );
	MoveQuat.FromVector2(&Move_Dir);
	MoveQuat.ToMatrix(&MoveMat);
	ApplyMatrix( &MoveMat, &SlideUp, &Up );
	ApplyMatrix( &MoveMat, &SlideRight, &Right );
	MoveDist = VectorLength( Move_Off );
	ImpactDist = MoveDist;
	ImpactPoint.x = StartPos.x + Move_Off->x;
	ImpactPoint.y = StartPos.y + Move_Off->y;
	ImpactPoint.z = StartPos.z + Move_Off->z;
	Move = *Move_Off;
	for ( f = 0; f < MAX_QFEELER_RAYS; f++ )
	{
		dx0 = FeelerOffset[ f ].start.dx;
		dy0 = FeelerOffset[ f ].start.dy;
		dx1 = FeelerOffset[ f ].end.dx;
		dy1 = FeelerOffset[ f ].end.dy;
		FeelerStart.x = radius * ( dx0 * Right.x + dy0 * Up.x );
		FeelerStart.y = radius * ( dx0 * Right.y + dy0 * Up.y );
		FeelerStart.z = radius * ( dx0 * Right.z + dy0 * Up.z );
		FeelerGroup = Start_Group;
		FeelerStart.x += StartPos.x;
		FeelerStart.y += StartPos.y;
		FeelerStart.z += StartPos.z;
		Feeler = Move_Dir;
		Feeler.x *= ( MoveDist + FeelerLength );
		Feeler.y *= ( MoveDist + FeelerLength );
		Feeler.z *= ( MoveDist + FeelerLength );
		Feeler.x += radius * ( dx1 * Right.x + dy1 * Up.x );
		Feeler.y += radius * ( dx1 * Right.y + dy1 * Up.y );
		Feeler.z += radius * ( dx1 * Right.z + dy1 * Up.z );
		if ( BackgroundCollide( &MCloadheadert0, &Mloadheader,
			&FeelerStart, FeelerGroup, &Feeler,
			&FeelerImpactPoint , &FeelerImpactGroup, &FeelerFaceNormal, &FeelerPos_New, true, NULL ) )
		{
			FeelerPlane = -DotProduct( (VECTOR *)&FeelerFaceNormal, &FeelerImpactPoint ) - radius;
			Div = DotProduct( &Move_Dir , (VECTOR *)&FeelerFaceNormal );
			if( Div >= BACKFACING_PLANE_TOLERANCE )
			{
				continue;
			}
			Num = ( FeelerPlane + DotProduct( &StartPos, (VECTOR *)&FeelerFaceNormal) );
			FeelerImpactDist = -( Num / Div );
			if ( FeelerImpactDist > IMPACT_OFFSET )
			{
				FeelerImpactDist -= IMPACT_OFFSET;
			}
			else
			{
				FeelerImpactDist = 0.0F;
			}
			FeelerImpactPoint.x = StartPos.x + Move_Dir.x * FeelerImpactDist;
			FeelerImpactPoint.y = StartPos.y + Move_Dir.y * FeelerImpactDist;
			FeelerImpactPoint.z = StartPos.z + Move_Dir.z * FeelerImpactDist;
			if ( ImpactDist > FeelerImpactDist )
			{
				FeelerEndPoint.x = StartPos.x + Move_Off->x;
				FeelerEndPoint.y = StartPos.y + Move_Off->y;
				FeelerEndPoint.z = StartPos.z + Move_Off->z;
				FeelerDist = ( DotProduct( &FeelerEndPoint, (VECTOR *)&FeelerFaceNormal ) + FeelerPlane );
				FeelerDist = -FeelerDist;
				FeelerPos_New.x = FeelerEndPoint.x + FeelerFaceNormal.nx * FeelerDist;
				FeelerPos_New.y = FeelerEndPoint.y + FeelerFaceNormal.ny * FeelerDist;
				FeelerPos_New.z = FeelerEndPoint.z + FeelerFaceNormal.nz * FeelerDist;
				ImpactDist = FeelerImpactDist;
				ImpactPoint = FeelerImpactPoint;
				ImpactNormal = FeelerFaceNormal;
			}
		}
	}
	if ( ImpactDist < MoveDist )
	{
		Move.x = ImpactPoint.x - StartPos.x;
		Move.y = ImpactPoint.y - StartPos.y;
		Move.z = ImpactPoint.z - StartPos.z;
		*Impact_Point = ImpactPoint;
		*Impact_Normal = ImpactNormal;
		hit = true;
	}
	*Impact_Group = MoveGroup( &Mloadheader, &StartPos, Start_Group, &Move );
	return hit;
}

/*===================================================================
	Procedure	:		Get CompEnemyHit Info
	Input		:		VECTOR	*	Collision Point (TBFI)
				:		NORMAL	*	Collision Normal (TBFI)
				:		float	*	Collision Distance (TBFI)
	Output		:		u_int16_t		EnemyHit (0xffff) None
===================================================================*/
u_int16_t GetComEnemyHitInfo( VECTOR * IntPoint, NORMAL * IntNormal, float * IntDistance )
{
	if( CompEnemyHit == (u_int16_t) -1 )
	{
		return( CompEnemyHit );
	}
	
	IntPoint->x = CompEnemyHitPos.x;
	IntPoint->y = CompEnemyHitPos.y;
	IntPoint->z = CompEnemyHitPos.z;
	IntNormal->nx = CompEnemyHitNormal.nx;
	IntNormal->ny = CompEnemyHitNormal.ny;
	IntNormal->nz = CompEnemyHitNormal.nz;
	*IntDistance = CompEnemyHitDist;

	return( CompEnemyHit );
}

/*===================================================================
	Procedure	:		Check if hit any enemy
	Input		:		Nothing
	Output		:		u_int16_t		EnemyHit (0xffff) None
===================================================================*/
u_int16_t CheckAnyEnemyHit( void )
{
	return( AnyCompEnemyHit );
}
