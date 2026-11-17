
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

#ifdef OPT_ON
#pragma optimize( "gty", on )
#endif

/*===================================================================
	Procedure	:	AIR Idle
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_IDLE( register ENEMY * Enemy )
{
	if ( Enemy->Timer )
	{
		Enemy->Timer -= framelag;

      
		if ( Enemy->Timer < 0.0F )
			Enemy->Timer = 0.0F;
		return;         
	}         
	// Switch to AIMODE_SCAN
	Enemy->Object.AI_Mode = AIMODE_SCAN;
	Enemy->Timer  =	RESET_SCAN_TIME + (float) Random_Range( (u_int16_t) RESET_SCAN_TIME );
}
/*===================================================================
	Procedure	:	TURRET Idle
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_TURRET_IDLE( register ENEMY * Enemy )
{
	if( Enemy->Object.Animating )
	{
		return;
	}
	
	if( Enemy->Object.CurAnimSeq != TURRETSEQ_Closed )
		SetCurAnimSeq( TURRETSEQ_Closed, &Enemy->Object );
	
	AI_UPDATEGUNS( Enemy );

	if ( Enemy->Timer )
	{
		Enemy->Timer -= framelag;
		if ( Enemy->Timer < 0.0F )
			Enemy->Timer = 0.0F;
		return;         
	}         
	// Switch to AIMODE_SCAN

	AI_THINK( Enemy , true , true );

	if( Enemy->AIFlags & AI_ANYPLAYERINRANGE )
	{
		Enemy->Object.AI_Mode = AIMODE_SCAN;
		Enemy->Timer  =	RESET_SCAN_TIME;
	}else{
		Enemy->Timer  =	RESET_IDLE_TIME * 2.0F;
	}
}

/*===================================================================
	Procedure	:	CRAWL Idle
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_CRAWL_IDLE( register ENEMY * Enemy )
{
	if( Enemy->Object.Animating )
	{
		return;
	}

	if ( Enemy->Timer )
	{
		Enemy->Timer -= framelag;
		if ( Enemy->Timer < 0.0F )
			Enemy->Timer = 0.0F;
		return;         
	}         
	// Switch to AIMODE_SCAN
	Enemy->Object.AI_Mode = AIMODE_SCAN;
	Enemy->Timer  =	RESET_SCAN_TIME + (float) Random_Range( (u_int16_t) RESET_SCAN_TIME );
}


#ifdef OPT_ON
#pragma optimize( "", off )
#endif

