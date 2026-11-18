/*===================================================================
	checkenemy.cpp — CheckHitEnemy in its own translation unit.

	Moved verbatim from enemies.cpp (same code, new home) so the host
	test (tests/test_checkhitenemy.cpp) can link the REAL function against
	stubbed data-layer deps. Game behavior unchanged by the move; the
	only functional delta is the SoundInfo group pre-filter (see below),
	which mirrors CheckHitShip's gate in primary.cpp.

	Picked up automatically: both Makefiles compile every .cpp in the
	source trees (no per-file list to update).
===================================================================*/
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
#include "rendering/2dpolys.h"
#include "rendering/lights.h"
#include "game/models.h"
#include "rendering/text.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/triggers.h"
#include "game/trigarea.h"
#include "math/sphere.h"
#include "game/pickups.h"
#include "game/enemies.h"
#include "input/controls.h"
#include "ai/ai.h"

#include "game/extforce.h"
#include "game/teleport.h"
#include "game/spotfx.h"
#include "audio/sfx.h"
#include "ui/title.h"
#include "util/util.h"
#include "game/oct2.h"

/*===================================================================
	Procedure	:	Check if hit Enemy
	Input		:	u_int16_t		OwnerType
				:	u_int16_t		Owner of weapon
				:	VECTOR	*	Pos
				:	u_int16_t	Bullet group (SoundInfo pre-filter)
				:	VECTOR	*	Dir
				:	VECTOR	*	UpDir
				:	float		Length of Ray
				:	VECTOR	*	Int_Point;
				:	VECTOR	*	Int_Point2;
				:	float	*	Dist to Int_Point
				:	float		Weapon Radius;
				:	u_int16_t		Colision Type
	Output		:	ENEMY	*	Enemy Address ( NULL If none hit )
===================================================================*/
ENEMY * CheckHitEnemy( u_int16_t OwnerType, u_int16_t Owner, VECTOR * Pos, u_int16_t Group, VECTOR * Dir, VECTOR * UpDir, float DirLength, VECTOR * Int_Point,
						VECTOR * Int_Point2, float * Dist, float WeaponRadius, u_int16_t ColType )
{
	VECTOR		TempVector;
	VECTOR		Int_Temp;
	VECTOR		Int_Temp2;
	float		DistToSphere;
	ENEMY	*	ClosestEnemy;
	float		ClosestLength;
	float		Cos;
	float		ColRadius = 0.0f;
	ENEMY	*	Enemy;
	ENEMY	*	NextEnemy;

	ClosestEnemy = NULL;
	ClosestLength = *Dist;

	Enemy = FirstEnemyUsed;

	while( Enemy != NULL )
	{
		NextEnemy = Enemy->NextUsed;

		if( !( ( OwnerType == OWNER_ENEMY ) && ( Owner == Enemy->Index ) ) )
		{
			if( ( Enemy->Status & ENEMY_STATUS_Enable ) && ( Enemy->CompCollision == SPHERECOLLISION ) )
			{
				/* SoundInfo group pre-filter (mirrors CheckHitShip, primary.cpp):
				 * skip enemies in groups with no direct visibility from the
				 * bullet's group before any per-enemy vector math. */
				/* @vfp-exempt R3 — protected hit-set pre-filter mirroring the shipped ship gate; fidelity A/B + multi-trajectory in perf/OPTIMIZATION-LOG.md. measured -7.7ms/frame wall (ramqan mono). Verified 2026-11-11. */
				if( SoundInfo[ Enemy->Object.Group ][ Group ] )
					goto SkipIt;

				if( Enemy->Type == ENEMY_Boss_Avatar )
				{
					if( !IsAvatarActive() )
						goto SkipIt;
				}
			
				switch( ColType )
				{
					case COLTYPE_Trojax:
						TempVector.x = ( Enemy->Object.Pos.x - Pos->x );			// Bul - Enemy
						TempVector.y = ( Enemy->Object.Pos.y - Pos->y );
						TempVector.z = ( Enemy->Object.Pos.z - Pos->z );
						NormaliseVector( &TempVector );
						Cos = (float) ( 1.0F - fabsf( DotProduct( &TempVector, Dir ) ) );
						ColRadius = ( EnemyTypes[Enemy->Type].Radius + ( WeaponRadius * Cos ) );
						break;

					case COLTYPE_Transpulse:
						TempVector.x = ( Enemy->Object.Pos.x - Pos->x );			// Bul - Enemy
						TempVector.y = ( Enemy->Object.Pos.y - Pos->y );
						TempVector.z = ( Enemy->Object.Pos.z - Pos->z );
						NormaliseVector( &TempVector );
						Cos = (float) ( 1.0F - fabsf( DotProduct( &TempVector, Dir ) ) );
						Cos = (float) ( Cos * ( 1.0F - fabsf( DotProduct( &TempVector, UpDir ) ) ) );
						ColRadius = ( EnemyTypes[Enemy->Type].Radius + ( WeaponRadius * Cos ) );
						break;

					case COLTYPE_Sphere:
						ColRadius = ( EnemyTypes[Enemy->Type].Radius + WeaponRadius );
						break;

					case COLTYPE_Point:
						ColRadius = EnemyTypes[Enemy->Type].Radius;
						break;
				}

/*===================================================================
	Does Ray hit target directly?
===================================================================*/
				if( RaytoSphere2( (VECTOR *) &Enemy->Object.Pos, EnemyTypes[Enemy->Type].Radius, Pos, Dir,
									&Int_Temp, &Int_Temp2 ) )
				{
					TempVector.x = ( Int_Temp.x - Pos->x );
					TempVector.y = ( Int_Temp.y - Pos->y );
					TempVector.z = ( Int_Temp.z - Pos->z );
					
					DistToSphere = VectorLength( &TempVector );
					
					if( ( DistToSphere <= DirLength ) && ( DistToSphere < ClosestLength ) )
					{
						ClosestLength = DistToSphere;
						ClosestEnemy = Enemy;
						*Int_Point = Int_Temp;
						*Int_Point2 = Int_Temp2;
					}
				}
				else
				{
/*===================================================================
	Is Starting pos inside sphere already?
===================================================================*/
					if( PointToSphere( (VERT *) &Enemy->Object.Pos, EnemyTypes[Enemy->Type].Radius, (VERT *) Pos ) )
					{
						ClosestLength = 0.0F;
						ClosestEnemy = Enemy;
						*Int_Point = *Pos;
						*Int_Point2 = *Pos;
					}
   					else
   					{
   						if( ColRadius > EnemyTypes[Enemy->Type].Radius )
   						{
/*===================================================================
	Do i graze sphere?
===================================================================*/
   							if( RaytoSphere2( (VECTOR *) &Enemy->Object.Pos, ColRadius, Pos, Dir,
   											  &Int_Temp, &Int_Temp2 ) )
   							{
   								TempVector.x = ( Int_Temp.x - Pos->x );
   								TempVector.y = ( Int_Temp.y - Pos->y );
   								TempVector.z = ( Int_Temp.z - Pos->z );
   						
   								DistToSphere = VectorLength( &TempVector );
   						
   								if( ( DistToSphere <= DirLength ) && ( DistToSphere < ClosestLength ) )
   								{
   									ClosestLength = DistToSphere;
   									ClosestEnemy = Enemy;
   									*Int_Point = Int_Temp;
   									*Int_Point2 = Int_Temp2;
   								}
   							}
   							else
   							{
   								if( PointToSphere( (VERT *) &Enemy->Object.Pos, ColRadius, (VERT *) Pos ) )
   								{
   									ClosestLength = 0.0F;
   									ClosestEnemy = Enemy;
   									*Int_Point = *Pos;
   									*Int_Point2 = *Pos;
   								}
   							}
   						}
   					}
				}
			}
		}
SkipIt:
		Enemy = NextEnemy;
	}

	if( ClosestEnemy != NULL ) *Dist = ClosestLength;

	return( ClosestEnemy );
}
