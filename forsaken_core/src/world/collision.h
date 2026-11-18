#pragma once

/* collision.h -- ray and swept-sphere collision against walls, portals,
 * BGObjects and enemy components
 *
 * Wall collision is a ray cast into the level's BSP, Bsp_Header[0] (bsp.cpp,
 * loaded per level). MCload is a stub that returns true, so MCLOADHEADER's
 * fields stay zero; the header passed to BackgroundCollide / OneGroupPolyCol
 * only selects behaviour. &MCloadheadert0 casts against Bsp_Header[0] and
 * tests BGObject / enemy zones with a zero radius. &MCloadheader would cast
 * against Bsp_Header[1], which the level loader always leaves unloaded, so it
 * tests only BGObject / enemy zones, with a 300 * GLOBAL_SCALE radius.
 *
 * BackgroundCollide sweeps a point from StartPos by MoveOffset, following
 * portals (Bsp_Portal_Header) from group to group, and on a hit returns the
 * impact point backed off the face by about COLLISION_FUDGE, the end group,
 * the face normal and a slide target. With BGCol set it also tests BGObject
 * and enemy-component collision zones along the ray. MoveGroup only follows
 * portals to find the group a move ends in.
 *
 * ObjectCollide and QCollide fire a fan of feeler rays (9 and 5) around a sphere of the given radius and clip the move to the
 * nearest front-facing hit; ObjectCollide also moves the object, updates its
 * group and deflects its speed. WouldObjectCollide is ObjectCollide without
 * moving or bouncing (it still clips *Move_Off); ObjectCollideNoBounce moves
 * without bouncing.
 *
 * Results pass through file-scope state in collision.cpp (the current ray and
 * the nearest component hit), so none of this is re-entrant.
 *
 * GetComEnemyHitInfo / CheckAnyEnemyHit report the enemy whose components were
 * hit by the most recent BackgroundCollide or OneGroupBGObjectCol (the first
 * only for sensitive zones); read them before the next collision call.
 */

#include "game/main.h"
#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"
#include "data/mload.h"
#include "game/object.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"

/*
 * defines
 */

constexpr auto MAXCOLGROUPS = MAXGROUPS;
constexpr auto COLLISION_FUDGE = (10.0F*GLOBAL_SCALE);

/*
 * structures
 */


typedef struct MCLOADHEADER{
	int		state;
	char * Buffer;
	u_int16_t	num_of_groups;
	u_int16_t	num_of_faces_in_group[MAXCOLGROUPS];
	MCFACE	*GroupFacePnt[MAXCOLGROUPS];
}MCLOADHEADER;


/*
 * fn prototypes
 */

bool MCload( char * Filename , MCLOADHEADER * MCloadheader );
bool BackgroundCollide( MCLOADHEADER *c, MLOADHEADER *m,
					  VECTOR *StartPos, u_int16_t StartGroup, VECTOR *MoveOffset, 
					  VECTOR *EndPos, u_int16_t *EndGroup,
					  NORMAL *FaceNormal, VECTOR *NewTarget, bool BGCol, BGOBJECT ** BGObject );
u_int16_t MoveGroup( MLOADHEADER *m, VECTOR *StartPos, u_int16_t StartGroup, VECTOR *MoveOffset );
bool OneGroupPolyCol( MCLOADHEADER * MCloadheader ,MLOADHEADER * Mloadheader , u_int16_t group ,
					VECTOR * Pos, VECTOR * Dir  ,
					VECTOR * ImpactPoint , NORMAL  * FaceNormal , VECTOR * Pos_New, bool BGCol,
					BGOBJECT ** BGObject );
bool OneGroupPortalCol( MLOADHEADER * Mloadheader , u_int16_t group ,
						VECTOR * Pos, VECTOR * Dir  ,
						VECTOR * ImpactPoint , NORMAL * FaceNormal, u_int16_t * PortalHit,
						int collisionhint );
bool AmIOutsideGroup( MLOADHEADER * m, VECTOR * EndPos, u_int16_t EndGroup );
bool CheckBGObjectsCol( u_int16_t Group, float Distance, VECTOR * ImpactPoint, int collided,
						VECTOR * New_Pos, NORMAL * FaceNormal, BGOBJECT ** BGObject );

bool OneGroupBGObjectCol( float Distance, int16_t Collided, u_int16_t Group, VECTOR * Pos, VECTOR * Dir  ,
					 VECTOR * ImpactPoint , NORMAL  * FaceNormal , VECTOR * Pos_New, BGOBJECT ** BGObject, float Radius );

bool CheckBGObjectCollision( VECTOR * Pos, BGOBJECT * Object, VECTOR * PushVector, float * DamagePtr, float Radius );
void CollideBGOToCompObjChildren( COMP_OBJ * Children, int16_t NumChildren );
void CollideBGOChildren( COMP_OBJ * Children, int16_t NumChildren );

bool ObjectCollide( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject );
bool WouldObjectCollide( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject );
bool QCollide( VECTOR *Start_Pos, u_int16_t Start_Group, VECTOR *Move_Off, float radius,
			  VECTOR *Impact_Point, u_int16_t *Impact_Group, NORMAL *Impact_Normal );
bool ObjectCollideNoBounce( OBJECT *Obj, VECTOR *Move_Off, float radius, BGOBJECT **BGObject );
bool CheckCompObjectCollision( VECTOR * Pos, COMP_OBJ * Comps, VECTOR * PushVector, float * DamagePtr, float Radius );
u_int16_t GetComEnemyHitInfo( VECTOR * IntPoint, NORMAL * IntNormal, float * IntDistance );
u_int16_t CheckAnyEnemyHit( void );

/* Defined in collision.cpp. no_collision is a debug toggle (Shift+F12 in
 * oct2.cpp) read by ships.cpp and pickups.cpp; while set, ships skip
 * background collision. ColChild is the component hit by the last zone test;
 * nothing outside collision.cpp reads it. */
extern int no_collision;
extern COMP_OBJ * ColChild;
