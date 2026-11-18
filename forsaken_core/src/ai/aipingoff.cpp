
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

/*===================================================================
	Procedure	:	AIR Ping Off...
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_DEATH_PINGOFF( ENEMY * Enemy )
{
	OBJECT * SObject;
	VECTOR		NewDir = { 0.0F, 1.0F, 0.0F };

	SObject = &Enemy->Object;
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
	
	Enemy->Object.ExternalForce.x += (Enemy->LastDamageDir.x * (EnemyTypes[Enemy->Type].MoveRateAccell * Enemy->LastDamageSpeed) * framelag) * ( ( 1.0F/PINGOFFTIME )*Enemy->Timer);
	Enemy->Object.ExternalForce.y += (Enemy->LastDamageDir.y * (EnemyTypes[Enemy->Type].MoveRateAccell * Enemy->LastDamageSpeed) * framelag) * ( ( 1.0F/PINGOFFTIME )*Enemy->Timer);
	Enemy->Object.ExternalForce.z += (Enemy->LastDamageDir.z * (EnemyTypes[Enemy->Type].MoveRateAccell * Enemy->LastDamageSpeed) * framelag) * ( ( 1.0F/PINGOFFTIME )*Enemy->Timer);

	Enemy->Object.ExternalForce.x -= Mloadheader.Group[ Enemy->Object.Group ].up.x * 0.25F * framelag;
	Enemy->Object.ExternalForce.y -= Mloadheader.Group[ Enemy->Object.Group ].up.y * 0.25F * framelag;
	Enemy->Object.ExternalForce.z -= Mloadheader.Group[ Enemy->Object.Group ].up.z * 0.25F * framelag;


	ObjectRotateExternal( &Enemy->Object, &Enemy->LastDamagePos , &Enemy->LastDamageImpactPos,
						  &Enemy->LastDamageDir, D2R( -Enemy->LastDamage * framelag ) , EnemyTypes[Enemy->Type].Radius );

}
