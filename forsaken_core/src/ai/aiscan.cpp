/* aiscan.cpp -- AIMODE_SCAN: look for the player. The FLY_AI version goes to
 * MOVETOTARGET if it sees or hears the player, else IDLE; the turret version
 * opens and goes to FIRE once a player is in its view cone with LOS, and
 * drops to IDLE when no player is in range; the crawl version just times out
 * to IDLE. AI_DO_SCAN is the one-shot target check used by crawl
 * follow-path and spline enemies.
 */

#include <stdio.h>
#include <math.h>

#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "data/mload.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "world/node.h"
#include "net/networking.h"
#include "game/ships.h"
#include "rendering/text.h"
#include "game/triggers.h"
#include "math/sphere.h"
#include "game/pickups.h"
#include "game/enemies.h"
#include "ai/ai.h"
#include "game/oct2.h"

/*===================================================================
	Procedure	:	AIR Scan for a Target
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_SCAN( ENEMY * Enemy )
{
	OBJECT * TObject;
	float dist;

	// Scan for target
	Tinfo->Flags = 0;
	SET_TARGET_PLAYERS;

	AI_GetDistToNearestTarget( Enemy );


	if( Tinfo->TFlags == _TARGET_PLAYERS )
	{
  		TObject = (OBJECT*)Tinfo->TObject;
		dist = DistanceVector2Vector( &Enemy->Object.Pos , &Tinfo->Pos ) + SoundInfo[Enemy->Object.Group][TObject->Group];

		if( AI_InViewCone( &Enemy->Object.Pos , &Enemy->Object.Mat , &TObject->Pos, Enemy->Viewcone ) || ( (dist <= EnemyTypes[Enemy->Type].Behave.HearRange) && TObject->Noise ) )
		{
			if( AI_ClearLOS( &Enemy->Object.Pos , Enemy->Object.Group , &TObject->Pos ) )
			{
				Enemy->TShip = Tinfo->TObject;
				AI_SetMOVETOTARGET( Enemy );
				return;
			}else{
				if(  Enemy->TShip && TObject->NearestNode )
				{
					Enemy->TShip = Tinfo->TObject;
					Enemy->TNode = (NODE*) WhichNode( Enemy->Object.NodeNetwork , (NODE*)Enemy->Object.NearestNode , (NODE*)TObject->NearestNode );
					AI_SetMOVETOTARGET( Enemy );
					return;
				}
			}
		}
	}

	// Switch to AIMODE_IDLE
	AI_SetIDLE( Enemy );

}

/*===================================================================
	Procedure	:	AI for a turret while its SCANNING..
	Input		:	ENEMY	*	Enemy
	Output		:	Nothing
===================================================================*/
void AI_TURRET_SCAN( ENEMY * Enemy )
{
	OBJECT * TObject;

	Enemy->Timer -= framelag;
	if( Enemy->Object.Animating )
	{
		return;
	}
 	// Scan for target
	if ( Enemy->Timer <= 0.0F )
	{
		AI_THINK( Enemy , true , true );
		if( Enemy->AIFlags & AI_ANYPLAYERINRANGE )
		{
			SET_TARGET_PLAYERS;

			AI_GetDistToNearestTarget( Enemy );

			switch( Tinfo->TFlags )
			{
			case _TARGET_PLAYERS:

				TObject = (OBJECT*)Tinfo->TObject;
				if( AI_InViewCone( &Enemy->Object.Pos ,&Enemy->Object.Mat , &TObject->Pos, Enemy->Viewcone ) )
				{
					if( AI_ClearLOS( &Enemy->Object.Pos, Enemy->Object.Group , &TObject->Pos ) )
					{
						Enemy->AIFlags |= AI_ICANSEEPLAYER;
						SetCurAnimSeq( TURRETSEQ_Opening, &Enemy->Object );
						Enemy->TShip = TObject;
						AI_SetFIREATTARGET( Enemy );
					}
				}
				break;
			default:
				// Switch to AIMODE_IDLE
				Enemy->Object.AI_Mode = AIMODE_IDLE;
				Enemy->Timer  =	RESET_IDLE_TIME;
				break;

			}
		}else{
			Enemy->Object.AI_Mode = AIMODE_IDLE;
			Enemy->Timer  =	RESET_IDLE_TIME;
		}
	}
}

/*===================================================================
	Procedure	:	AI for a Crawling Enemy while its SCANNING..
	Input		:	ENEMY	*	Enemy
	Output		:	Nothing
===================================================================*/
void AI_CRAWL_SCAN( ENEMY * Enemy )
{

	if( Enemy->Object.Animating )
	{
		return;
	}

	if ( Enemy->Timer )
	{
		Enemy->Timer -= framelag;


		if ( Enemy->Timer < 0.0F )
		{
			//------------------------------------------------------------------------------
			// Switch to AIMODE_IDLE
			//------------------------------------------------------------------------------
			Enemy->Object.AI_Mode = AIMODE_IDLE;
			Enemy->Timer  =	RESET_SCAN_TIME + (float) Random_Range( (u_int16_t) RESET_SCAN_TIME );
			return;
		}
	}


}


/*===================================================================
	Procedure	:	AI Do a Scan: set TShip if the nearest player is seen or heard...
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_DO_SCAN( ENEMY * Enemy )
{
	OBJECT * TObject;
	// Scan for target
	Tinfo->Pos.x 	= Enemy->Object.Pos.x;
	Tinfo->Pos.y 	= Enemy->Object.Pos.y;
	Tinfo->Pos.z 	= Enemy->Object.Pos.z;
	Tinfo->Group 	= Enemy->Object.Group;
	Tinfo->Nearest = BIGDISTANCE;
	Tinfo->TFlags = 0;
	Tinfo->SObject	= &Enemy->Object;
	Tinfo->TObject	= NULL;

	SET_TARGET_PLAYERS;

	AI_GetDistToNearestTarget( Enemy );

	if( Tinfo->TFlags & _TARGET_PLAYERS )
	{
  		TObject = (OBJECT*)Tinfo->TObject;
		if( AI_InViewCone( &Enemy->Object.Pos , &Enemy->Object.Mat , &TObject->Pos, Enemy->Viewcone ) || ( ( Enemy->LastDistance <= EnemyTypes[Enemy->Type].Behave.HearRange) && TObject->Noise ) )
		{
			if( AI_ClearLOS( &Enemy->Object.Pos , Enemy->Object.Group , &TObject->Pos ) )
			{
				Enemy->TShip = Tinfo->TObject;
			}
		}
	}
}
