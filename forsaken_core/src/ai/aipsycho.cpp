
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

// Externals
extern	float framelag;
extern	AIMDATA AimData;
extern	VECTOR	Forward;
extern	VECTOR	SlideUp;
extern	PRIMARYWEAPONATTRIB	PrimaryWeaponAttribs[];
extern	SECONDARYWEAPONATTRIB	SecondaryWeaponAttribs[];
extern	MLOADHEADER Mloadheader;
/*===================================================================
	Procedure	:	AIR Crash and burn...
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_AIR_DEATH_PSYCHO( register ENEMY * Enemy )
{
	OBJECT * SObject;
	VECTOR		NewDir = { 0.0F, 1.0F, 0.0F };
	VECTOR		TempVector;
	VECTOR		TempUpVector;
	VECTOR		TempOffset = { 0.0F, 0.0F, 0.0F };
	BYTE	Weapon;

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
	Enemy->Object.Angle.y += (3.0F - ( 3.0F * ( ( 1.0F / ( 1.5F * 60.0F ) ) * Enemy->Timer ) ) ) * framelag;
	Enemy->Object.Angle.x += (0.5F - ( 0.5F * ( ( 1.0F / ( 1.5F * 60.0F ) ) * Enemy->Timer ) ) ) * framelag;

	Enemy->PrimaryFireTimer -= framelag;
	if( Enemy->PrimaryFireTimer < 0.0F )
		Enemy->PrimaryFireTimer = 0.0F;

	if( !(Enemy->Object.Flags & SHIP_Scattered) )
	{
		ApplyMatrix( &SObject->Mat, &Forward, &TempVector );
		ApplyMatrix( &SObject->Mat, &SlideUp, &TempUpVector );
		
		if( (Enemy->PrimaryFireTimer == 0.0F) && ( EnemyTypes[Enemy->Type].PrimaryWeaponType != NO_PRIMARY )  )
		{
			Enemy->PrimaryFireTimer = EnemyTypes[Enemy->Type].PrimaryFireRate * 0.5F;
			
			Weapon = BodgePrimaryWeapon( EnemyTypes[Enemy->Type].PrimaryWeaponType, Enemy->PickupHeld );
		
			EnemyFirePrimary( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Weapon,
				Enemy->Object.Group, &Enemy->Object.Pos, &TempOffset, &TempVector, &TempUpVector,
				EnemyTypes[Enemy->Type].PowerLevel, (EnemyTypes[Enemy->Type].PowerLevel +1) * 33.0F, false, NULL );
		}
	}
	
}





