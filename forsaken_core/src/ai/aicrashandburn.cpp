
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
#include "game/secondary.h"
#include "game/primary.h"
#include "game/oct2.h"
#include "game/spotfx.h"

/*===================================================================
	Procedure	:	AIR Crash and burn...
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_DEATH_CRASHANDBURN( ENEMY * Enemy )
{
	VECTOR		NewDir = { 0.0F, 1.0F, 0.0F };
	VECTOR		NewPos;

	Enemy->AIFlags |= AI_ANYPLAYERINRANGE;

	Enemy->Timer -= framelag;
	if( (Enemy->Timer < 0.0F) || (Enemy->AIMoveFlags & AI_CONTROL_COLLISION) )
	{
		Enemy->Timer = 0.0F;
		// Time to Die properly
		CreateNewExplosion( &Enemy->Object.Pos, &NewDir, Enemy->Object.Group );
		KillUsedEnemy( Enemy );
		return;
	}
	Enemy->Object.ExternalForce.x -= Mloadheader.Group[ Enemy->Object.Group ].up.x * 0.55F * framelag;
	Enemy->Object.ExternalForce.y -= Mloadheader.Group[ Enemy->Object.Group ].up.y * 0.55F * framelag;
	Enemy->Object.ExternalForce.z -= Mloadheader.Group[ Enemy->Object.Group ].up.z * 0.55F * framelag;

	NewPos = Enemy->Object.Pos;

	NewPos.x -= Mloadheader.Group[ Enemy->Object.Group ].up.x * (EnemyTypes[Enemy->Type].Radius * 0.75F);
	NewPos.y -= Mloadheader.Group[ Enemy->Object.Group ].up.y * (EnemyTypes[Enemy->Type].Radius * 0.75F);
	NewPos.z -= Mloadheader.Group[ Enemy->Object.Group ].up.z * (EnemyTypes[Enemy->Type].Radius * 0.75F);

	AI_AimAtTarget( &Enemy->Object.InvMat , &Enemy->Object.Pos, &NewPos );
	Enemy->AI_Angle = AimData.Angle;
	Enemy->AIMoveFlags |= AimData.Flags;

	CreateSpotFXBurning( &NewPos, &Mloadheader.Group[ Enemy->Object.Group ].up, Enemy->Object.Group );

}
