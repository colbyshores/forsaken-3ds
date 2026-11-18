/* aikillmine.cpp -- AIMODE_KILLMINE for FLY_AI enemies: turn toward the
 * player mine at MinePos and shoot it with the primary. Entered from
 * FOLLOWPATH or DOGFIGHT once AI_AVOIDCHECK has seen a close mine
 * (AI_MINEAVOID); returns to FOLLOWPATH on the first frame that flag is clear.
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
#include "game/secondary.h"
#include "game/primary.h"
#include "game/oct2.h"

/*===================================================================
	Procedure	:	AIR KillMine
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_KILLMINE( ENEMY * Enemy )
{
	OBJECT * SObject;
	VECTOR		TempVector;
	VECTOR		TempUpVector;
	VECTOR		TempOffset = { 0.0F, 0.0F, 0.0F };
	BYTE		Weapon;

	SObject = &Enemy->Object;

	// Is it time to think???

	AI_THINK( Enemy , false , false);

	Enemy->Timer -= framelag;
	if( Enemy->Timer < 0.0F )
		Enemy->Timer = 0.0F;

	Enemy->PrimaryFireTimer -= framelag;
	if( Enemy->PrimaryFireTimer < 0.0F )
		Enemy->PrimaryFireTimer = 0.0F;

	if( !(Enemy->AIFlags &AI_MINEAVOID) )
	{
		Enemy->AIFlags &= ~(AI_MINEAVOID + AI_MINEISCLOSE);
		AI_SetFOLLOWPATH( Enemy );
		return;
	}
	
	Enemy->AIFlags &= ~AI_MINEAVOID;

	
	AI_UPDATEGUNS( Enemy );
	// Aim at target

	AI_AimAtTarget( &SObject->InvMat , &SObject->Pos, &Enemy->MinePos );
	
	if( !( EnemyTypes[Enemy->Type].Behave.Flags & AI_BEHAVIOUR_ICANTPITCH ) )
	{
		Enemy->AIMoveFlags |= AimData.Flags;
	}else{
		//This enemy cant look up or down so it has to move up and down to compensate....
		Enemy->AIMoveFlags |= ( AimData.Flags & ( AI_CONTROL_TURNLEFT + AI_CONTROL_TURNRIGHT ) );
		if( AimData.Flags & AI_CONTROL_TURNUP )
		{
			Enemy->AIMoveFlags |=  AI_CONTROL_UP;
		}else if( AimData.Flags & AI_CONTROL_TURNDOWN )
		{
			Enemy->AIMoveFlags |=  AI_CONTROL_DOWN;
		}
		
	}
	if( Enemy->AvoidTimer && Enemy->AvoidType )
	{
		Enemy->AIMoveFlags = Enemy->AvoidType;
	}else{
		Enemy->AvoidTimer = 0.0F;
		ApplyMatrix( &SObject->Mat, &Forward, &TempVector );
		ApplyMatrix( &SObject->Mat, &SlideUp, &TempUpVector );
		  
		if( !(Enemy->Object.Flags & SHIP_Scattered ) && (Enemy->PrimaryFireTimer == 0.0F) && ( EnemyTypes[Enemy->Type].PrimaryWeaponType != NO_PRIMARY )  )
		{
			Enemy->PrimaryFireTimer = EnemyTypes[Enemy->Type].PrimaryFireRate + (float) Random_Range( (u_int16_t) EnemyTypes[Enemy->Type].PrimaryFireRate );

			Weapon = BodgePrimaryWeapon( EnemyTypes[Enemy->Type].PrimaryWeaponType, Enemy->PickupHeld );

			EnemyFirePrimary( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Weapon,
				Enemy->Object.Group, &Enemy->Object.Pos, &TempOffset, &TempVector, &TempUpVector,
				EnemyTypes[Enemy->Type].PowerLevel, 0.0F, false, NULL );
		}
	}
	Enemy->AI_Angle = AimData.Angle;
}
