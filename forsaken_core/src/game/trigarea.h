
#pragma once

/* trigarea.h -- trigger areas: level volumes that fire trigger mods
 *
 * trigarea.cpp loads the level's .zon file (TriggerAreaload, called from
 * oct2.cpp's level init; a missing file just means no areas) into the Zones
 * array and builds four per-group singly linked lists: areas with a player
 * in/enter/exit action, a player-shoots action, an enemy in/enter/exit action,
 * and an enemy-shoots action. All of it is per level and is freed by
 * ReleaseTriggerArea.
 *
 * Movers call the Check functions with the segment they moved along this
 * frame and the group they are in (ships, enemies, primary and secondary
 * bullets). Each area that is On classifies the segment as Entry / In / Exit
 * and applies TrigMods[when_*] for the matching event; 0xffff means no
 * action. A sphere zone uses half_size.x as its radius; a box or polygonal
 * zone is a convex hull of TRIGGER_ZONE planes whose inside is where
 * dot(pos, normal) + offset <= 0 for every side. Shoot checks only fire on
 * Entry, so a bullet spawned inside an area does not trigger it.
 *
 * RayToHull reports through the Entry / Exit / In globals and only ever sets
 * them; callers clear them first. teleport, extforce and goal reuse it for
 * their own hull zones. Area on/off state is the only thing saved with a
 * game (SaveTriggerAreas / LoadTriggerAreas).
 */

/*
 * Defines
 */
constexpr int TRIGGER_AREA_STATE_Off = 0;
constexpr int TRIGGER_AREA_STATE_On = 1;

constexpr int ZONE_Sphere = 0;
constexpr int ZONE_Box = 1;
constexpr int ZONE_Polygonal = 2;

enum {

	TRIGGER_AREA_GENTYPE_Initialised,		// On from level start
	TRIGGER_AREA_GENTYPE_Time,			// Turned on once generation_delay runs out (ProcessTriggerAreas)
	TRIGGER_AREA_GENTYPE_Trigger,			// Turned on by an EnableTriggerArea trigger event
};

/*
 * Structures
 */
typedef struct TRIGGER_ZONE
{
	VECTOR	normal;
	float	offset;
}TRIGGER_ZONE;

typedef struct TRIGGER_AREA
{
	u_int16_t	state;
	u_int16_t	group;
	u_int16_t	generation_type;
	float	generation_delay;
	u_int16_t	type;
	VECTOR	pos;
	VECTOR	half_size;
	TRIGGER_ZONE	*Zone;
	u_int16_t	num_sides;
	u_int16_t	when_player_isin;
	u_int16_t	when_player_enters;
	u_int16_t	when_player_exits;
	u_int16_t	when_player_shoots;
	u_int16_t	when_enemy_isin;
	u_int16_t	when_enemy_enters;
	u_int16_t	when_enemy_exits;
	u_int16_t	when_enemy_shoots;
	int16_t	player_primary;
	int16_t	player_secondary;
	int16_t	enemy_primary;
	int16_t	enemy_secondary;
struct	TRIGGER_AREA * NextSameGroup_player;
struct	TRIGGER_AREA * NextSameGroup_player_shoots;
struct	TRIGGER_AREA * NextSameGroup_enemy;
struct	TRIGGER_AREA * NextSameGroup_enemy_shoots;
}TRIGGER_AREA;

/*
 * fn prototypes
 */

bool TriggerAreaload( char * Filename );
void ReleaseTriggerArea( void );
bool TriggerAreaPlayerCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group );
bool TriggerAreaPlayerShootsCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group, u_int8_t WeaponType, u_int8_t Weapon );
bool TriggerAreaEnemyCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group );
bool TriggerAreaEnemyShootsCheck( VECTOR * OldPos , VECTOR * NewPos , u_int16_t Group, u_int8_t WeaponType, u_int8_t Weapon );
void DispGroupTriggerAreas( u_int16_t Group );
void EnableTriggerArea( u_int16_t * Data );
void DisableTriggerArea( u_int16_t * Data );
void ProcessTriggerAreas( void );
FILE * SaveTriggerAreas( FILE * fp );
FILE * LoadTriggerAreas( FILE * fp );
/* Defined in trigarea.cpp; shared with the modules that use it. */
bool RayToHull( TRIGGER_ZONE * StartSide , VECTOR * StartPos , VECTOR * EndPos , u_int16_t StartNumSides );
bool ClassifyPointInHull( VECTOR * Pos, TRIGGER_ZONE * Sides, int16_t NumSides, int16_t Side );
extern bool Entry;
extern bool Exit;
extern bool In;
