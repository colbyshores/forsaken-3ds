#pragma once

#include "game/main.h"
#include "game/constants.h"
#define GOALSTATE_Off	(0)
#define GOALSTATE_On	(1)

#define GOAL_NONE		(0)
#define GOAL_SCORED		(1)
#define GOAL_WRONG		(-1)

typedef struct GOAL
{
	u_int16_t	state;
	int		team;
	u_int16_t	group;
	u_int16_t	type;
	VECTOR	pos;
	VECTOR	half_size;
	VECTOR	dir;
	VECTOR	up;
	float	width;
	float	height;
	float	depth;
	u_int16_t	num_sides;
	TRIGGER_ZONE	*Zone;
} GOAL;


bool GoalLoad( void );
void ReleaseGoal( void );
void ProcessGoals( void );
GOAL *TeamGoal( int team );
bool GenerateFlagAtHome( int team );
int GoalCheckTeam( VECTOR * OldPos , VECTOR * NewPos, u_int16_t Group, int team );
int DisplayGoal( u_int16_t group );


/* Defined in goal.cpp; shared with the modules that use it. */
extern int GoalTeamMembers[ MAX_TEAMS ];
void SetTeamGoals( u_int16_t *TeamGoals );
extern int TeamFlag[ MAX_TEAMS ];
