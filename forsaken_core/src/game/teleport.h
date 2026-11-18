#pragma once

/* teleport.h -- level teleporters: entry zones and their destinations
 *
 * teleport.cpp loads the level's .tel file (TeleportsLoad) into a per-level
 * malloc'd Teleports[] array, chained per group through TeleportsGroupLink.
 * Status is inverted: TELEPORTACTIVE is 0. Links[] are indices into
 * Teleports[]; an object crossing an active zone is moved to a random link's
 * Pos, Group and Dir/Up orientation. Type is neither loaded nor tested, so
 * ships and enemies both teleport. StartTeleport / StopTeleport take a
 * teleport index from trigger event data. TeleportsAreaCheck takes the new
 * position first.
 */

#include "game/main.h"
#include "math/new3d.h"
#include "game/triggers.h"
#include "game/trigarea.h"

/*
 * Defines
 */
constexpr int TELEPORTINACTIVE = 1;
constexpr int TELEPORTACTIVE = 0;

constexpr int TELEPORTSHIPS = 1;
constexpr int TELEPORTENEMIES = 2;
constexpr int MAXTELEPORTLINKS = 8;
/*
 * Structures
 */
typedef struct TELEPORT{
		u_int16_t	Status;
		u_int16_t	Group;
		u_int16_t	Type;					// what stuff can teleport
		u_int16_t	zone_type;				// sphere / box / convex hull
		VECTOR	Pos;
		VECTOR	Dir;
		VECTOR	Up;
		VECTOR	half_size;
		TRIGGER_ZONE	*Zone;
		u_int16_t	num_sides;

		u_int16_t	num_links;
		u_int16_t	Links[MAXTELEPORTLINKS];
struct	TELEPORT * NextInGroup;
}TELEPORT;

/*
 * fn prototypes
 */
void ReleaseTeleports( void );
bool TeleportsLoad( char * Filename );
void TeleportsZoneCheck( VECTOR * OldPos , VECTOR * NewPos , TELEPORT * Tport );
bool TeleportsAreaCheck( VECTOR * NewPos , VECTOR * OldPos ,u_int16_t Group, OBJECT *obj );
void StartTeleport( u_int16_t * Data );
void StopTeleport( u_int16_t * Data );
void DisplayTeleportsInGroup( u_int16_t Group );
FILE * SaveTeleports( FILE * fp );
FILE * LoadTeleports( FILE * fp );
