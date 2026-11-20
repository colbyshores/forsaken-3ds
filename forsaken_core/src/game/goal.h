#pragma once

/* goal.h -- capture-the-flag goal zones
 *
 * goal.cpp owns the per-level goal array, loaded from the level's .GOL
 * file only in flag games (no file = no goals). Each goal is a trigger
 * zone; the host assigns goals to teams (SetTeamGoals), and a goal is
 * GOALSTATE_On while its team has players. GoalCheckTeam tests a move
 * from OldPos to NewPos against goals in or connected to Group: GOAL_SCORED
 * for the team's own goal, GOAL_WRONG for entering another team's,
 * GOAL_NONE otherwise. ProcessGoals recounts the players per team and
 * each team's players carrying a flag every frame; the HUD score list asks
 * Goals.team_carrying_flag(team) to flash a team that holds a flag.
 */

#include "game/main.h"
#include "game/constants.h"
constexpr int GOALSTATE_Off = (0);
constexpr int GOALSTATE_On = (1);

constexpr int GOAL_NONE = (0);
constexpr int GOAL_SCORED = (1);
constexpr int GOAL_WRONG = (-1);

struct GOAL
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
};


bool GoalLoad( void );
void ReleaseGoal( void );
void ProcessGoals( void );
GOAL *TeamGoal( int team );
bool GenerateFlagAtHome( int team );
int GoalCheckTeam( VECTOR * OldPos , VECTOR * NewPos, u_int16_t Group, int team );
int DisplayGoal( u_int16_t group );
void SetTeamGoals( u_int16_t *TeamGoals );

/* The level's capture-the-flag goals (one instance, Goals): the goals
 * loaded with the level, which goal each team scores at, the per-team
 * player and flag-carrier counts, and the sparkle timer of the goal
 * display. Loaded and released with the level, processed each frame, and
 * checked as ships move. The HUD reads team_carrying_flag(). */
class GoalZones {
	public:
	bool  load();
	void  release();
	GOAL *team_goal( int team );
	bool  generate_flag_at_home( int team );
	void  set_team_goals( u_int16_t *TeamGoals );
	void  process();
	int   check_team( VECTOR * OldPos, VECTOR * NewPos, u_int16_t Group, int team );
	int   display( u_int16_t group );

	/* Does any player of this team carry a flag? (score list, each frame) */
	bool  team_carrying_flag( int team ) const { return TeamFlag[ team ] != 0; }

	private:
	float nearest_distance( int goal );
	int   pick_team_goal( int team );
	void  show( GOAL * goal, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );

	int     TeamFlag[ MAX_TEAMS ] = {};
	GOAL   *Goal = nullptr;
	int     NumGoals = 0;
	int     GoalTeamMembers[ MAX_TEAMS ] = {};
	int     LastTeamMembers[ MAX_TEAMS ] = {};
	int16_t Teamgoal[ MAX_TEAMS ] = {};
	int     LastEnable[ MAX_PLAYERS ] = {};
	float   SparkleTime = 0.1F;
};

extern GoalZones Goals;
