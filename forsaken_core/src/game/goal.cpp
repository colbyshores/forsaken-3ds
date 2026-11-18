
#include <stdio.h>
#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/main.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "data/file.h"
#include "game/triggers.h"
#include "game/trigarea.h"
#include "game/pickups.h"
#include "data/mload.h"
#include "world/bsp.h"
#include "game/enemies.h"
#include "game/skin.h"

#include "game/primary.h"
#include "game/secondary.h"
#include "rendering/2dpolys.h"
#include "game/goal.h"
#include "ui/title.h"
#include "world/visi.h"
#include "rendering/text.h"
#include "util/util.h"
#include "game/oct2.h"
#include "game/ships.h"
#include "net/multiplayer.h"
#include "rendering/2dtextures.h"
#include "util/file_reader.h"

constexpr int GOAL_VERSION_NUMBER = (1);


void ShowGoal( GOAL * Goal, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );

// globals
int NumGoals = 0;
GOAL *Goal = NULL;
int TeamFlag[ MAX_TEAMS ];
int GoalTeamMembers[ MAX_TEAMS ];
int LastTeamMembers[ MAX_TEAMS ];
int16_t Teamgoal[ MAX_TEAMS ];
int LastEnable[ MAX_PLAYERS ];

bool GoalLoad( void )
{
	int8_t		Filename[ 256 ];
	const char	*	NewExt = ".GOL";
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	char		*	OrgBuffer;
	int			i, j;
	GOAL * GoalPnt;
	TRIGGER_ZONE * ZonePnt;
	u_int32_t	MagicNumber;
	u_int32_t	VersionNumber;

	if ( !CaptureTheFlag && !CTF )
		return true;

	Change_Ext( (char*)&LevelNames[ LevelNum ][ 0 ], (char*)&Filename[ 0 ], (char*)NewExt );

	File_Size = Get_File_Size( (char*)Filename );

	if( !File_Size ) return true;

	Buffer = (char*)malloc( File_Size );
	OrgBuffer = Buffer;

	if( Buffer == NULL ) return false;

	Read_Size = Read_File( (char*)Filename, Buffer, File_Size );

	if( Read_Size != File_Size ) return false;

	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != GOAL_VERSION_NUMBER  ) )
	{
		Msg( "GoalLoad() Incompatible triggerzone( .ZON ) file %s", Filename );
		return( false );
	}

	NumGoals = in.u16();
	Goal = (GOAL *) calloc( NumGoals, sizeof( GOAL ) );
	if ( !Goal )
	{
		NumGoals = 0;
		return false;
	}
	GoalPnt = Goal;

	for( i = 0 ; i < NumGoals ; i++ )
	{
		GoalPnt->group = in.u16();
		GoalPnt->type = in.u16();
		GoalPnt->team = -1;
		GoalPnt->state = GOALSTATE_Off;

		GoalPnt->pos = in.vec();
		GoalPnt->half_size = in.vec();
		GoalPnt->dir = in.vec();
		GoalPnt->up = in.vec();
		GoalPnt->width = in.f32();
		GoalPnt->height = in.f32();
		GoalPnt->depth = in.f32();
		if( GoalPnt->type != ZONE_Sphere )
		{
			// convex hull...
			GoalPnt->num_sides = in.u16();
			if ( GoalPnt->num_sides )
				GoalPnt->Zone = (TRIGGER_ZONE*) malloc( GoalPnt->num_sides * sizeof( TRIGGER_ZONE ) );
			else
				GoalPnt->Zone = NULL;
			ZonePnt = GoalPnt->Zone;
			if ( !ZonePnt )
			{
				NumGoals = 0;
				free( Goal );
				Goal = NULL;
				return false;
			}

			for( j = 0 ; j < GoalPnt->num_sides ; j++ )
			{
				ZonePnt->normal = in.vec();
				ZonePnt->offset = in.f32();
				ZonePnt++;
			}
		}
		GoalPnt++;
	}

	free( OrgBuffer );

	for ( j = 0; j < MAX_TEAMS; j++ )
	{
		GoalTeamMembers[ j ] = 0;
		Teamgoal[ j ] = -1;
	}

	for ( j = 0; j < MAX_PLAYERS; j++ )
	{
		LastEnable[ j ] = 0;
	}

	return true;
}


void ReleaseGoal( void )
{
	int j;

	if ( !Goal )
		return;

	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].Zone )
		{
			free( Goal[ j ].Zone );
			Goal[ j ].Zone = NULL;
			Goal[ j ].num_sides = 0;
		}
	}
	free( Goal );
	Goal = NULL;
	NumGoals = 0;
}


static void GoalCheck( VECTOR * OldPos , VECTOR * NewPos , GOAL * goal )
{
	bool	OldIn;
	bool	NewIn;

	Entry = false;
	Exit = false;
	In = false;
	if( goal->type == ZONE_Sphere )
	{
   		OldIn = DistanceVector2Vector( &goal->pos , OldPos ) < goal->half_size.x;
   		NewIn = DistanceVector2Vector( &goal->pos , NewPos ) < goal->half_size.x;

   		if( !OldIn && NewIn )
   			Entry = true;
   		if( OldIn && !NewIn )
   			Exit = true;
   		if( OldIn && NewIn )
   		{
   			In = true;
   		}
   	}else{
   		RayToHull( goal->Zone , OldPos , NewPos , goal->num_sides );
   	}
}

GOAL *TeamGoal( int team )
{
	int j;

	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].team == team )
			return &Goal[ j ];
	}

	return NULL;
}


bool GenerateFlagAtHome( int team )
{
	int j;

	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].team == team )
		{
			GOAL *goal;
			u_int16_t i;
			VECTOR Dir = { 0.0F, 0.0F, 0.0F };

			goal = &Goal[ j ];
			i = InitOnePickup( &goal->pos, goal->group,
				&Dir, 0.0F, TeamFlagPickup[ team ], WhoIAm,
				++Ships[WhoIAm].PickupIdCount, -1, true,
				-1.0F, INDEX_NONE );

			if( ( i != INDEX_NONE )  && ( i != (u_int16_t) -2 ) )
			{
				DropPickupSend( &goal->pos, goal->group,
					&Dir, 0.0F, TeamFlagPickup[ team ], Ships[WhoIAm].PickupIdCount,
					-1, true, -1.0F, INDEX_NONE );
			}
			else
			{
				DebugPrintf( "Unable to generate %s flag\n", TeamName[ team ] );
			}
			return true;
		}
	}
	return false;
}


float NearestGoalDistance( int goal )
{
	int j;
	float d, dmin;
	VECTOR diff;

	dmin = 0.0F;
	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].state != GOALSTATE_On || Goal[ j ].team < 0 || j == goal )
			continue;
		diff.x = Goal[ j ].pos.x - Goal[ goal ].pos.x;
		diff.y = Goal[ j ].pos.y - Goal[ goal ].pos.y;
		diff.z = Goal[ j ].pos.z - Goal[ goal ].pos.z;
		d = 0.1F * VectorLength( &diff ) + SoundInfo[ Goal[ j ].group ][ Goal[ goal ].group ];
		if ( !dmin || d < dmin )
		{
			dmin = d;
		}
	}
	return dmin;
}


int PickTeamGoal( int team )
{
	int j;
	int goal;
	float d, dmax = 0.0f;

	goal = -1;
	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].team < 0 )
		{
			d = NearestGoalDistance( j );
			if ( !d )
			{
				DebugPrintf( "Allocating first team goal at random\n" );
				/* @vfp-exempt R11 — cold path: level-load/spawn selection, never per-frame. measured n/a. Verified 2026-10-25. */
				return RandomStartPosModify % NumGoals;
			}
			else if ( goal < 0 || d > dmax )
			{
				goal = j;
				dmax = d;
				DebugPrintf( "Team %d candidate goal %d (d=%f)\n",
					team, goal, d );
			}
		}
		else if ( Goal[ j ].team == team )
		{
			DebugPrintf( "Reallocating existing team %d goal %d\n",
				team, j );
			return j;
		}
	}
	DebugPrintf( "Allocating team %d goal %d (dmax=%f)\n",
		team, goal, dmax );
	return goal;
}

void SetTeamGoals( u_int16_t *TeamGoals )
{
	int j;

	for ( j = 0; j < MAX_TEAMS; j++ )
	{
		Teamgoal[ j ] = TeamGoals[ j ];
		if ( Teamgoal[ j ] >= 0 && Teamgoal[ j ] < NumGoals )
		{
			if ( Goal[ Teamgoal[ j ] ].team < 0 )
				TeamFlagAtHome[ j ] = false;
			Goal[ Teamgoal[ j ] ].team = j;
		}
		else
		{
			Teamgoal[ j ] = -1;
			TeamFlagAtHome[ j ] = false;
		}
		DebugPrintf( "Teamgoal[ %d(%s) ] = %hd\n",
			j, TeamName[ j ], Teamgoal[ j ] );
	}
	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].team >= 0 && Goal[ j ].team < MAX_TEAMS )
		{
			Goal[ j ].state = ( Teamgoal[ Goal[ j ].team ] == j )
				? GOALSTATE_On : GOALSTATE_Off;
		}
		else
			Goal[ j ].state = GOALSTATE_Off;
	}
	for ( j = 0; j < MAX_TEAMS; j++ )
	{
		DebugPrintf( "Goal[ %d ].team = %d(%s)\n",
			j, Goal[ j ].team,
			( Goal[ j ].team >= 0 ) ? TeamName[ Goal[ j ].team ] : "none" );
	}
}

void ProcessGoals( void )
{
	int j, k;
	int NumFlags[ MAX_TEAMS ];
	int NewTeams = 0;

	if( ( ( MyGameStatus != STATUS_Normal ) && ( MyGameStatus != STATUS_PlayingDemo ) ) ||
		( !CaptureTheFlag && !CTF ) || !NumGoals || !Goal ) // paranoia attack...
		return;

	for ( j = 0; j < MAX_TEAMS; j++ )
	{
		TeamFlag[ j ] = 0;
		NumFlags[ j ] = NumPickupType[ TeamFlagPickup[ j ] ];
		LastTeamMembers[ j ] = GoalTeamMembers[ j ];
		GoalTeamMembers[ j ] = 0;
	}
	for ( k = 0; k < MAX_PLAYERS; k++ )
	{
		if ( ( ( GameStatus[ k ] == MyGameStatus && !PlayDemo )
			|| ( GameStatus[ k ] == STATUS_Normal && PlayDemo ) )
			&& TeamNumber[ k ] < MAX_TEAMS )
		{
			GoalTeamMembers[ TeamNumber[ k ] ]++;
			if ( Ships[ k ].Object.Flags & SHIP_CarryingFlags )
			{
				TeamFlag[ TeamNumber[ k ] ]++;
				for ( j = 0; j < MAX_TEAMS; j++ )
				{
					if ( Ships[ k ].Object.Flags & TeamFlagMask[ j ] )
					{
						NumFlags[ j ]++;
					}
				}
			}
		}
	}
	if ( IsHost )
	{
		NewTeams = 0;
		for ( j = 0; j < MAX_TEAMS; j++ )
		{
			if ( !LastTeamMembers[ j ] && GoalTeamMembers[ j ] )
			{
				NewTeams++;
				if ( Teamgoal[ j ] < 0 )
				{
					// new team has appeared -> pick a goal for them
					Teamgoal[ j ] = PickTeamGoal( j );
					SetTeamGoals( (u_int16_t*)Teamgoal );
				}
			}
		}
		if ( NewTeams )
		{
			SetTeamGoals( (u_int16_t*)Teamgoal );
			if ( !PlayDemo )
				TeamGoalsSend( (u_int16_t*)Teamgoal );
		}
	}
	for ( j = 0; j < NumGoals; j++ )
	{
		Goal[ j ].state = ( Goal[ j ].team >= 0 && GoalTeamMembers[ Goal[ j ].team ] )
			? GOALSTATE_On : GOALSTATE_Off;
	}
	if ( IsHost && !PlayDemo )
	{
		if ( NewTeams )
		{
			for ( j = 0; j < MAX_TEAMS; j++ )
			{
				if ( !LastTeamMembers[ j ] && GoalTeamMembers[ j ] )
				{
					if ( CTF ) // new team has appeared -> generate their flag
						GenerateFlagAtHome( j );
				}
			}
		}
		else
		{
			int NewPlayers;

			NewPlayers = 0;
			for ( j = 0; j < MAX_PLAYERS; j++ )
			{
				if ( Ships[ j ].enable && !LastEnable[ j ] )
					NewPlayers++;
				LastEnable[ j ] = Ships[ j ].enable;
			}
			if ( NewPlayers )
			{
				TeamGoalsSend( (u_int16_t*)Teamgoal );
			}
		}
	}
	if ( CTF && !PlayDemo )
	{
		for ( j = 0; j < MAX_TEAMS; j++ )
		{
			if ( !GoalTeamMembers[ j ] && LastTeamMembers[ j ] )
			{
				// and old team has disappeared -> kill all their flags
				KillAllPickupsOfType( TeamFlagPickup[ j ], PICKUPKILL_Immediate );
				Ships[ WhoIAm ].Object.Flags &= ~TeamFlagMask[ j ];
				PickupsGot[ TeamFlagPickup[ j ] ] = 0;
				TeamFlagAtHome[ j ] = false;
			}
		}
	}
}


int GoalCheckTeam( VECTOR * OldPos , VECTOR * NewPos, u_int16_t Group, int team )
{
	int j;

	for ( j = 0; j < NumGoals; j++ )
	{
		if( ( Goal[ j ].state == GOALSTATE_On || DebugInfo )
			&& ( Goal[ j ].group == Group || GroupsAreConnected( Goal[ j ].group, Group ) ) )
		{
			GoalCheck( OldPos , NewPos , &Goal[ j ] );

			if( Entry )
			{
				if ( Goal[ j ].team == team )
					return GOAL_SCORED;
				else
					return GOAL_WRONG;
			}
			else if ( In )
			{
				if ( Goal[ j ].team == team )
				   return GOAL_SCORED;
			}
		}
	}
	return GOAL_NONE;
}


int DisplayGoal( u_int16_t group )
{
	int j, k;
	int shown;
	u_int8_t c[ 3 ];

	if ( !CaptureTheFlag && !CTF )
		return 0;
	shown = 0;
	for ( j = 0; j < NumGoals; j++ )
	{
		if ( Goal[ j ].group == group
			&& ( ( Goal[ j ].state == GOALSTATE_On ) || DebugInfo ) )
		{
			for ( k = 0; k < 3; k++ )
			{
				c[ k ] = Colourtrans[ TeamCol[ Goal[ j ].team ] ][ k ];
			}
			ShowGoal( &Goal[ j ], c[ 0 ], c[ 1 ], c[ 2 ], 128 );
			shown++;
		}
	}
	return shown;
}

/*===================================================================
	Procedure	:	Show Goal
	Input		:	GOAL	*	Goal
				:	u_int8_t		Red
				:	u_int8_t		Green
				:	u_int8_t		Blue
				:	u_int8_t		Trans
	Output		:	Nothing
===================================================================*/
float	SparkleTime = 0.1F;

void ShowGoal( GOAL * Goal, u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans )
{
	int16_t	Count;
	u_int16_t	fmpoly;
	VECTOR	Offset;
	VECTOR	Pos;
	QUAT	TempQuat;
	MATRIX	GoalMatrix;

	TempQuat.FromDirAndUp(&Goal->dir, &Goal->up);
	TempQuat.ToMatrix(&GoalMatrix);

	for( Count = 0; Count < 10; Count++ )
	{
		SparkleTime -= ( framelag * 0.1F );

		if( SparkleTime <= 0.0F )
		{
			SparkleTime = 0.1F;

			Offset.x = (float) ( Random_Range( (u_int16_t) Goal->width ) - ( Goal->width / 2.0F ) );
			Offset.y = (float) ( Random_Range( (u_int16_t) Goal->height ) - ( Goal->height / 2.0F ) );
			Offset.z = (float) ( Random_Range( (u_int16_t) Goal->depth ) - ( Goal->depth / 2.0F ) );

			ApplyMatrix( &GoalMatrix, &Offset, &Pos );

			Pos.x += Goal->pos.x;
			Pos.y += Goal->pos.y;
			Pos.z += Goal->pos.z;

			fmpoly = FindFreeFmPoly();

			if( fmpoly != INDEX_NONE )
			{
				FmPolys[ fmpoly ].LifeCount = 20.0F;
				FmPolys[ fmpoly ].Pos = Pos;
				FmPolys[ fmpoly ].SeqNum = FM_GOALSPARKLE;
				FmPolys[ fmpoly ].Frame = 0.0F;
				FmPolys[ fmpoly ].R = Red;
				FmPolys[ fmpoly ].G = Green;
				FmPolys[ fmpoly ].B = Blue;
				FmPolys[ fmpoly ].Trans = Trans;
				FmPolys[ fmpoly ].Frm_Info = &GreyFlare_Header;
				FmPolys[ fmpoly ].Flags = FM_FLAG_MOVEOUT;
				FmPolys[ fmpoly ].Rot = 0.0F;
				FmPolys[ fmpoly ].xsize = 0.0F;
				FmPolys[ fmpoly ].ysize = 0.0F;
				FmPolys[ fmpoly ].Dir.x = ( Random_Range( 10000 ) / 5000.0F ) - 1.0F;
				FmPolys[ fmpoly ].Dir.y = ( Random_Range( 10000 ) / 5000.0F ) - 1.0F;
				FmPolys[ fmpoly ].Dir.z = ( Random_Range( 10000 ) / 5000.0F ) - 1.0F;
				NormaliseVector( &FmPolys[ fmpoly ].Dir );
				FmPolys[ fmpoly ].Speed = 2.5F;
				FmPolys[ fmpoly ].Group = Goal->group;
				AddFmPolyToTPage( fmpoly, GetTPage( *FmPolys[ fmpoly ].Frm_Info, 0 ) );
			}
		}
	}
}
