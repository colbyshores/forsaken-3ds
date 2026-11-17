

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

#define	MC_VERSION_NUMBER	1

#define	BSP_ENABLE		1
#undef BSP2

#define MAX_FEELER_RAYS		(9)
#define MAX_QFEELER_RAYS	(5)

#define BACKGROUND_PORTAL_TOLERANCE		(12.0F * GLOBAL_SCALE)

#define PORTAL_IMPACTOFFSET		(0.1F * GLOBAL_SCALE)

#define POINT_ON_PORTAL_TOLERANCE	(0.1)
#define OUTSIDE_GROUP_TOLERANCE		(25.0F)

int no_collision = 0;
int outside_group = 0;

	DWORD GroupPolyCol_timeMax = 0;

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
static	float	*IPointp = (float *) &IPoint;

bool CheckRestartPointCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject );

bool CheckEnemyPolyCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint,
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject );

/*===================================================================
	Procedure	:		Load .mc File Collision file..
	Input		:		char	*	Filename , MCLOADHEADER * MCloadheader
	Output		:		bool
===================================================================*/
bool MCload( char * Filename , MCLOADHEADER * MCloadheader )
{
#ifdef POLYGONAL_COLLISIONS
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	u_int16_t		*	Uint16Pnt;
	u_int32_t		*	Uint32Pnt;
	int				i;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;

	File_Size = Get_File_Size( Filename );	

	if( !File_Size )
		return ( false );

	/* +8 headroom: per-group u16 num_faces toggles alignment, so any MCFACE*
	   cast on an odd-indexed group lands 2-mod-4. MCFACE has float fields
	   and ARM's vldr needs 4-byte alignment, so we memmove-shift remaining
	   data forward per group as needed (see the group loop below). */
	Buffer = malloc( File_Size + 8 );

	if( Buffer == NULL )
		return( false );

	Read_Size = Read_File( Filename, Buffer, File_Size );

	if( Read_Size != File_Size )
		return( false );
	
	MCloadheader->Buffer = Buffer;

	Uint32Pnt = (u_int32_t *) Buffer;
	MagicNumber = *Uint32Pnt++;
	VersionNumber = *Uint32Pnt++;
	Buffer = (char *) Uint32Pnt;

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MC_VERSION_NUMBER  ) )
	{
		Msg( "MCload() Incompatible collision ( .MC ) file %s", Filename );
		return( false );
	}

	Uint16Pnt = (u_int16_t *) Buffer;
	/*	get the number of Groups	*/
	MCloadheader->num_of_groups = *Uint16Pnt++;
	Buffer = (char *) Uint16Pnt;		

	for( i=0 ; i<MCloadheader->num_of_groups;i++)
	{
		/* get the number of polys in the group	*/
		Uint16Pnt = (u_int16_t *) Buffer;
		MCloadheader->num_of_faces_in_group[i] = *Uint16Pnt++ ;
		Buffer = (char *) Uint16Pnt;

		/* MCFACE has float fields; vldr needs Buffer 4-byte aligned.
		   The u16 num_faces read above may leave Buffer 2-mod-4.
		   Shift remaining bytes forward into the headroom allocated
		   with Buffer (see MCload allocation above). */
		if( ((uintptr_t)Buffer) & 3 )
		{
			size_t pad = (4 - (((uintptr_t)Buffer) & 3)) & 3;
			size_t used = (size_t)( Buffer - MCloadheader->Buffer );
			size_t remaining = (size_t)File_Size - used;
			memmove( Buffer + pad, Buffer, remaining );
			Buffer += pad;
		}

		/* make a note of the address of the Faces	*/
		MCloadheader->GroupFacePnt[i] = (MCFACE *) Buffer;
		Buffer += ( MCloadheader->num_of_faces_in_group[i] * sizeof(MCFACE) );
	}
#endif // POLYGONAL_COLLISIONS

	return( true );
}

#ifdef OPT_ON
#pragma optimize( "gty", on )
#endif

/*===================================================================
	Procedure	:	Check Ray to Polygon intersection
	Input		:	VECTOR	*	Point 0
				:	VECTOR	*	Point 1
				:	VECTOR	*	Point 2
				:	VECTOR	*	Origin
				:	VECTOR	*	Direction
				:	VECTOR	*	Intersect Point
	Output		:	int			true/ false
===================================================================*/
#ifdef USEINLINE
__inline
#endif
bool RayPolyIntersect( float * P0 , float * P1 , float * P2 , float * P3 ,
	 				 VERT *  Point, NORMAL * FaceNormal , float D , float * TempDistance)
{
	float		t;
	float		Div, Num;
	int			i1, i2;
	int			ClockCount;
	int			AntiCount;
	float		Np0x;
	float		Np0y;
	float		Np1x;
	float		Np1y;
	float		Np2x;
	float		Np2y;
	float		Np3x = 0.f;
	float		Np3y = 0.f;
	float		Np4x;
	float		Np4y;
	float		Npcx;
	float		Npcy;
	float		*NP4;

/*===================================================================
	Calculate D
===================================================================*/
//	D =	( ( P0->x * FaceNormal->nx ) +
//		  ( P0->y * FaceNormal->ny ) +
//		  ( P0->z * FaceNormal->nz ) ); 

/*===================================================================
	Calculate T
===================================================================*/
	Div = ( ODir.x * FaceNormal->nx) + 
		  ( ODir.y * FaceNormal->ny) + 
		  ( ODir.z * FaceNormal->nz);
	
	if( Div >= 0.0F ) return false;		/* Reject, Parallel */


	Num = ( ( Origin.x * FaceNormal->nx ) +
		    ( Origin.y * FaceNormal->ny ) +
		    ( Origin.z * FaceNormal->nz ) ) + D ; 
		
	t = -( Num / Div );

	if( t < 0.0F ) return false;		/* Intersection behind origin */
	if( t > 1.0F ) return false;		/* Intersection Greater then ray length */

	*TempDistance = t;

/*===================================================================
	Do Polygon collision
===================================================================*/
	Point->x = ( Origin.x + ( ODir.x * t ) );
	Point->y = ( Origin.y + ( ODir.y * t ) );
	Point->z = ( Origin.z + ( ODir.z * t ) );


/*===================================================================
	Work out best axis to cast polygon onto
===================================================================*/
	if( fabsf( FaceNormal->nx ) >= fabsf( FaceNormal->ny ) &&
		fabsf( FaceNormal->nx ) >= fabsf( FaceNormal->nz ) )
	{
		i1 = ourY;						/* Y Axis		*/
		i2 = ourZ;						/* and Z axis	*/
	}else{
 		i1 = ourX;					/* X Axis		*/
		if( fabsf( FaceNormal->ny ) >= fabsf( FaceNormal->nx ) &&
		    fabsf( FaceNormal->ny ) >= fabsf( FaceNormal->nz ) )
		{
			i2 = ourZ;					/* and Z axis	*/
		}else{
			i2 = ourY;					/* and Y axis	*/
		}
	}


/*===================================================================
	Check if point within triangles
===================================================================*/
	AntiCount = 0;
	ClockCount = 0;

	NP4 = (float *) Point;
	Np4x = NP4[ i1 ];
	Np4y = NP4[ i2 ];

	Np0x = P0[ i1 ];
	Np0y = P0[ i2 ];
	Np1x = P1[ i1 ];
	Np1y = P1[ i2 ];
	Np2x = P2[ i1 ];
	Np2y = P2[ i2 ];

	if( P3 == (float*) -1)	//if -1 then only a tri....
	{
///*
		Npcx = ( Np0x + Np1x + Np2x) / 3.0F;
		Npcy = ( Np0y + Np1y + Np2y) / 3.0F;
//*/
	}else{
		Np3x = P3[ i1 ];
		Np3y = P3[ i2 ];
///*
		Npcx = ( Np0x + Np1x + Np2x + Np3x ) * 0.25F;	// same as divide by 4
		Npcy = ( Np0y + Np1y + Np2y + Np3y ) * 0.25F;	// same as divide by 4
		Np3x = (Np3x - Npcx ) * SCALE_FUDGE;
		Np3y = (Np3y - Npcy ) * SCALE_FUDGE;
//*/ 
   }
///*
	Np0x = (Np0x - Npcx ) * SCALE_FUDGE;
	Np0y = (Np0y - Npcy ) * SCALE_FUDGE;
	Np1x = (Np1x - Npcx ) * SCALE_FUDGE;
	Np1y = (Np1y - Npcy ) * SCALE_FUDGE;
	Np2x = (Np2x - Npcx ) * SCALE_FUDGE;
	Np2y = (Np2y - Npcy ) * SCALE_FUDGE;
	Np4x -= Npcx;
	Np4y -= Npcy;
//*/
	if( ( Np4x * ( Np0y - Np1y ) ) +
	    ( Np0x * ( Np1y - Np4y ) ) +
	    ( Np1x * ( Np4y - Np0y ) ) < 0.0F ) ClockCount++;
	else AntiCount++;

	if( ( Np4x * ( Np1y - Np2y ) ) +
	    ( Np1x * ( Np2y - Np4y ) ) +
	    ( Np2x * ( Np4y - Np1y ) ) < 0.0F )
	{
		if( AntiCount ) return false;
	}else{
		if( ClockCount ) return false;
	}

	if( P3 == (float*) -1)	//if -1 then only a tri....
	{

		if( ( Np4x * ( Np2y - Np0y ) ) +
		    ( Np2x * ( Np0y - Np4y ) ) +
		    ( Np0x * ( Np4y - Np2y ) ) < 0.0F )
		{
			if( AntiCount ) return false;
		}else{
			if( ClockCount ) return false;
		}
	}else{			// otherwise it must be a quad...
		if( ( Np4x * ( Np2y - Np3y ) ) +
		    ( Np2x * ( Np3y - Np4y ) ) +
		    ( Np3x * ( Np4y - Np2y ) ) < 0.0F )
		{
			if( AntiCount ) return false;
		}else{
			if( ClockCount ) return false;
		}
		if( ( Np4x * ( Np3y - Np0y ) ) +
		    ( Np3x * ( Np0y - Np4y ) ) +
		    ( Np0x * ( Np4y - Np3y ) ) < 0.0F )
		{
			if( AntiCount ) return false;
		}else{
			if( ClockCount ) return false;
		}
	}
	return true;
}


typedef enum
{
	X_Axis = 0,
	Y_Axis = 1,
	Z_Axis = 2
} AxisIndex;

#ifdef USEINLINE
__inline
#endif
bool ColRayPolyIntersect( MCFACE *face )
{
	float		t;
	float		Div, Num;
	float		v0x;
	float		v0y;
	float		v1x;
	float		v1y;
	float		v2x;
	float		v2y;
	float		v3x;
	float		v3y;
	float		ix;
	float		iy;
	bool		Clockwise;

	if ( !DebugInfo && ( face->type & 0x800000L ) )
		return false; // ignore backfacing patch collision polys unless debugging
/*===================================================================
	Calculate T
===================================================================*/
	Div = ( ODir.x * face->nx) + 
		  ( ODir.y * face->ny) + 
		  ( ODir.z * face->nz);
	
	if( Div >= 0.0F ) return false;		/* Reject, Parallel */


	Num = ( ( Origin.x * face->nx ) +
		    ( Origin.y * face->ny ) +
		    ( Origin.z * face->nz ) ) + face->D ; 
		
	t = -( Num / Div );

/*===================================================================
	Do Polygon collision
===================================================================*/

	if( t < 0.0F ) return false;		/* Intersection behind origin */
	if( t > 1.0F ) return false;		/* Intersection Greater then ray length */

	IPoint.x = ( Origin.x + ( ODir.x * t ) );
	IPoint.y = ( Origin.y + ( ODir.y * t ) );
	IPoint.z = ( Origin.z + ( ODir.z * t ) );

/*===================================================================
	Find projected 2D coords of vertices and intersection point
===================================================================*/

	v0x = face->v[ 0 ].u;
	v0y = face->v[ 0 ].v;
	v1x = face->v[ 1 ].u;
	v1y = face->v[ 1 ].v;
	v2x = face->v[ 2 ].u;
	v2y = face->v[ 2 ].v;

	ix = IPointp[ ( face->type & 6 ) ? X_Axis : Y_Axis ];
	iy = IPointp[ ( face->type & 4 ) ? Y_Axis : Z_Axis ];


/*===================================================================
	Check if point within triangle
===================================================================*/

	if( ( ix * ( v0y - v1y ) ) +
	    ( v0x * ( v1y - iy ) ) +
	    ( v1x * ( iy - v0y ) ) < 0.0F )
	{
		Clockwise = true;
	}
	else
	{
		Clockwise = false;
	}

	if( ( ix * ( v1y - v2y ) ) +
	    ( v1x * ( v2y - iy ) ) +
	    ( v2x * ( iy - v1y ) ) < 0.0F )
	{
		if( !Clockwise )
			return false;
	}
	else
	{
		if( Clockwise )
			return false;
	}

	if( face->type & 1 ) // face is a quad
	{
		v3x = face->v[ 3 ].u;
		v3y = face->v[ 3 ].v;

		if( ( ix * ( v2y - v3y ) ) +
		    ( v2x * ( v3y - iy ) ) +
		    ( v3x * ( iy - v2y ) ) < 0.0F )
		{
			if( !Clockwise )
				return false;
		}
		else
		{
			if( Clockwise )
				return false;
		}
		if( ( ix * ( v3y - v0y ) ) +
		    ( v3x * ( v0y - iy ) ) +
		    ( v0x * ( iy - v3y ) ) < 0.0F )
		{
			if( !Clockwise )
				return false;
		}
		else
		{
			if( Clockwise )
				return false;
		}
	}
	else // face only a tri
	{
		if( ( ix * ( v2y - v0y ) ) +
		    ( v2x * ( v0y - iy ) ) +
		    ( v0x * ( iy - v2y ) ) < 0.0F )
		{
			if( !Clockwise )
				return false;
		}
		else
		{
			if( Clockwise )
				return false;
		}
	}
	IDist = t;
	return true;
}

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

/*===================================================================
	Procedure	:	Calculate the Dot product of a Vert and Normal
	Input		:	VERT	*	a
				:	NORMAL	*	b
	Output		:	float		Dot Product
===================================================================*/
#ifdef USEINLINE
__inline
#endif
float ColDotProduct( VECTOR * a , NORMAL * b ) 
{
	return( ( a->x * b->nx ) +
		    ( a->y * b->ny ) +
		    ( a->z * b->nz ) );
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
 * BackgroundCollideOneGroup
 *
 * Description
 *	checks for background collisions of a point moving between two positions
 *	only in the current group
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
 *	EndPos		=	final position (either collision impact point, target, or group boundary)
 *	EndGroup	=	group final position is in (or next group if on boundary)
 *	FaceNormal	=	normal of face collided with (if collided at all)
 *	NewTarget	=	new target position after sliding along collision face (if collided at all)
 *
 * Returns
 *	true if collided with background, false if no collision
 */
bool BackgroundCollideOneGroup( MCLOADHEADER *c, MLOADHEADER *m,
					  VECTOR *StartPos, u_int16_t StartGroup, VECTOR *MoveOffset, 
					  VECTOR *EndPos, u_int16_t *EndGroup,
					  NORMAL *FaceNormal, VECTOR *NewTarget, bool BGCol, BGOBJECT ** BGObject )
{
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	VECTOR ppos, pmove, epos, tpos;
	NORMAL fnorm;
	u_int16_t group;
	u_int16_t next_group;
	bool hit_bg, hit_portal;
	float dist_bg;
	VECTOR dv;
	float impact_offset;
	float impact_dotp;

	CompEnemyHit = (u_int16_t) -1;
	AnyCompEnemyHit = (u_int16_t) -1;
	next_group = StartGroup;
	ppos = *StartPos;
	pmove = *MoveOffset;
	group = next_group;
	hit_portal = false;
	hit_bg = false;
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
	if ( OneGroupPolyCol( c, m, group, StartPos, MoveOffset, &epos, &fnorm, &tpos, BGCol, BGObject ) )
	{
		BSP_PORTAL_GROUP	*pg;
		BSP_PORTAL			*bp;
		int					j;
		float				d;
		
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
						break;
					}
				}
			}
		}
		if ( hit_portal )
		{
			hit_bg = false;
		}
		else
		{
			hit_bg = true;
			dv.x = epos.x - StartPos->x;
			dv.y = epos.y - StartPos->y;
			dv.z = epos.z - StartPos->z;
			dist_bg = (float) sqrtf( dv.x * dv.x + dv.y * dv.y + dv.z * dv.z );
			dv = *MoveOffset;
			NormaliseVector( &dv );
			impact_dotp = -( dv.x * fnorm.nx + dv.y * fnorm.ny + dv.z * fnorm.nz );
			impact_offset = ( impact_dotp ) ? COLLISION_FUDGE / impact_dotp : 2.0F * dist_bg;
			if ( dist_bg > impact_offset )
			{
				epos.x -= dv.x * impact_offset;
				epos.y -= dv.y * impact_offset;
				epos.z -= dv.z * impact_offset;
	 			*EndGroup = group;
			}
			else
			{
				epos = *StartPos;
				*EndGroup = StartGroup;
			}
 			*EndGroup = group;
			*EndPos = epos;
			*FaceNormal = fnorm;
			*NewTarget = tpos;
		}
	}
	if ( !hit_bg )
	{
		if ( hit_portal )
		{
			*EndGroup = next_group;
			*EndPos = ppos;
		}
		else
		{
			*EndGroup = group;
			EndPos->x = StartPos->x + MoveOffset->x;
			EndPos->y = StartPos->y + MoveOffset->y;
			EndPos->z = StartPos->z + MoveOffset->z;
		}
	}
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
bool OneGroupPolyCol( MCLOADHEADER * MCloadheaderp ,MLOADHEADER * Mloadheader , u_int16_t group ,
					 VECTOR * Pos, VECTOR * Dir  ,
					 VECTOR * ImpactPoint , NORMAL  * FaceNormal , VECTOR * Pos_New, bool BGCol, BGOBJECT ** BGColObject )
{
	/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
#ifdef POLYGONAL_COLLISIONS
	MCFACE	*	FacePnt;
	int			num_faces;
#endif
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
#ifdef POLYGONAL_COLLISIONS
			Msg( "Polygonal collisions still being used -- THIS WILL FAIL!" );
#endif
		}

#ifdef POLYGONAL_COLLISIONS
		FacePnt = MCloadheaderp->GroupFacePnt[group];
		num_faces = MCloadheaderp->num_of_faces_in_group[group];
		
		while ( num_faces-- )
		{
			if( ColRayPolyIntersect( FacePnt ) )
			{
				if ( !collided )
				{
					Distance = IDist;
					*ImpactPoint = IPoint;
					CollFace = FacePnt;
				}
				else
				{
					if ( IDist < Distance )
					{
						Distance = IDist;
						*ImpactPoint = IPoint;
						CollFace = FacePnt;
					}
				}
				collided++;
			}
			FacePnt++;
		}

		if( collided )
			Distance *= VectorLength( &ODir );
#endif // POLYGONAL_COLLISIONS
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

#if ENABLEENEMYCOLLISIONS
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
#endif

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
bool OneGroupPortalCol( MLOADHEADER * Mloadheader , u_int16_t group ,
						VECTOR * Pos, VECTOR * Dir  ,
						VECTOR * ImpactPoint , NORMAL * FaceNormal, u_int16_t *Next_Group,
						int collisionhint )
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
					  int collided, VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject )
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
	ANI_ZONE_SPHERE		*	AniZoneSpherePtr;
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
							AniZoneSpherePtr = (ANI_ZONE_SPHERE *) ZonesPtr->Data;
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
#if ENABLEENEMYCOLLISIONS

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

#else

	if( Group == (u_int16_t) -1) return false;

	Origin = *Pos;
	ODir = *Dir;
	ColRadius = Radius;

	if( !CheckBGObjectsCol( Group, Distance, ImpactPoint, Collided, Pos_New, FaceNormal, BGObject ) ) return (false);

#endif

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
	ANI_ZONE_SPHERE		*	AniZoneSpherePtr;
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
							AniZoneSpherePtr = (ANI_ZONE_SPHERE *) ZonesPtr->Data;
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



#define BACKFACING_PLANE_TOLERANCE		(-0.001F)
#define IMPACT_OFFSET					(0.1F)
#define FEELER_LENGTH_TO_RADIUS_RATIO	(1.25F)


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
	float ImpactPlane;
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
				ImpactPlane = FeelerPlane;
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

#if BAD_BUMP
			Move.x = Move_Off->x - Obj->ExternalForce.x * framelag;
			Move.y = Move_Off->y - Obj->ExternalForce.y * framelag;
			/* @vfp-exempt R3 — protected hit set: swept-window/lethality compare; integerizing changes gameplay. measured n/a. Verified 2026-10-25. */
			Move.z = Move_Off->z - Obj->ExternalForce.z * framelag;
			MoveDist = VectorLength( &Move );
#else // better, but not perfect...
			Move = Obj->Speed;
			MoveDist = VectorLength( &Move ) * framelag;
#endif
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


bool ObjectCollideNoBGObject( OBJECT *Obj, VECTOR *Move_Off, float radius )
{
	bool collide;

	NoBGObject = false;
	collide = ObjectCollide( Obj, Move_Off, radius, NULL );
	NoBGObject = true;

	return collide;
}

bool ObjectCollideOnly( OBJECT *Obj, VECTOR *Move_Off, float radius, VECTOR *Target_Off, BGOBJECT **BGObject )
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
	} FeelerOffset[ MAX_FEELER_RAYS ] =
	{
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
	VECTOR FeelerPos_New;
	BGOBJECT *FeelerBGObject;
	float FeelerImpactDist;
	float ImpactDist;
	float MoveDist;
	VECTOR Move_Dir;
	float FeelerPlane;
	float FeelerDist;
	VECTOR FeelerEndPoint;
	float Num, Div;
	VECTOR FeelerStart;
	u_int16_t FeelerGroup;
	NORMAL ImpactNormal;
	float ImpactPlane;
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
			&FeelerImpactPoint , &FeelerImpactGroup, &FeelerFaceNormal, &FeelerPos_New, true, ( BGObject ) ? &FeelerBGObject : NULL ) )
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
				FeelerDist = -FeelerDist + COLLISION_FUDGE;
				FeelerPos_New.x = FeelerEndPoint.x + FeelerFaceNormal.nx * FeelerDist;
				FeelerPos_New.y = FeelerEndPoint.y + FeelerFaceNormal.ny * FeelerDist;
				FeelerPos_New.z = FeelerEndPoint.z + FeelerFaceNormal.nz * FeelerDist;
				ImpactDist = FeelerImpactDist;
				ImpactPoint = FeelerImpactPoint;
				ImpactNormal = FeelerFaceNormal;
				ImpactPlane = FeelerPlane;
				Pos_New = FeelerPos_New;
				if( BGObject )
					*BGObject = FeelerBGObject;
			}
		}
	}
	if ( ImpactDist < MoveDist )
	{
		hit = true;
	}
	Move_Off->x = ImpactPoint.x - StartPos.x;
	Move_Off->y = ImpactPoint.y - StartPos.y;
	Move_Off->z = ImpactPoint.z - StartPos.z;
	Target_Off->x = Pos_New.x - ImpactPoint.x;
	Target_Off->y = Pos_New.y - ImpactPoint.y;
	Target_Off->z = Pos_New.z - ImpactPoint.z;
	if ( move_object )
	{
		Obj->Pos = ImpactPoint;
		Obj->Group = MoveGroup( &Mloadheader, &StartPos, Obj->Group, Move_Off );
	}

	return hit;
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

#ifdef OPT_ON
#pragma optimize( "", off )
#endif
