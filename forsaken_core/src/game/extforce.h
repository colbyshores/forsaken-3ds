#pragma once

/* extforce.h -- level force zones: push, shield change, screen shake
 *
 * extforce.cpp owns the per-level array loaded from the level's .gf file
 * (no file = no forces), linked per group through ExternalForcesGroupLink
 * and NextInGroup for lookup. Each force is a trigger zone (sphere, box
 * or convex hull). Ships and enemies inside an active zone receive a
 * force that falls linearly from MaxForce at Origin to MinForce at Range,
 * scaled by framelag. Range is stored as its reciprocal after load.
 * Status 0 (EXTERNALFORCEACTIVE) means on; trigger events start and stop
 * a force by index. Released with the level.
 */

#include "game/main.h"
/*
 * Defines
 */
#define	EXTERNALFORCEACTIVE 0
#define	EXTERNALFORCEINACTIVE 1

#define	EXTERNALFORCETYPE_Move 0
#define	EXTERNALFORCETYPE_Shield 1
#define	EXTERNALFORCETYPE_Shake 2
 
/*
 * Structures
 */
typedef struct EXTERNALFORCE{
		u_int16_t	Status;
		u_int16_t	ForceType;
		u_int16_t	Group;
		VECTOR	Origin;
		VECTOR	Pos;
		VECTOR	Dir;
		VECTOR	Up;
		float	MaxForce;
		float	MinForce;
		float	Width;
		float	Height;
		float	Range;
		u_int16_t	Type;				// sphere / box / convex hull
		VECTOR	half_size;
		TRIGGER_ZONE	*Zone;
		u_int16_t	num_sides;
struct	EXTERNALFORCE * NextInGroup;
}EXTERNALFORCE;

/*
 * fn prototypes
 */
void ReleaseExternalForces( void );
bool ExternalForcesLoad( char * Filename );
void ExternalForcesZoneCheck( VECTOR * OldPos , VECTOR * NewPos , EXTERNALFORCE * EFpnt );
bool ExternalForcesAreaCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group , VECTOR * ExtForce , float * Shield);
void StartExternalForce( u_int16_t * Data );
void StopExternalForce( u_int16_t * Data );
void DisplayExternalForcesInGroup( u_int16_t Group );
FILE * LoadExternalForces( FILE * fp );
FILE * SaveExternalForces( FILE * fp );
