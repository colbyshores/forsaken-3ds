/* aifire.cpp -- turret fire mode and the shared gun driver.
 * AI_TURRET_FIREATTARGET is AIMODE_FIRE for TURRET_AI enemies, entered from
 * AI_TURRET_SCAN; every RESET_VALIDATE_TIME it rechecks range and LOS and
 * closes back to SCAN when the player is lost. AI_UPDATEGUNS, called by most
 * modes, aims each GUNOBJECT at the target with look-ahead and fires it on
 * reload (single shots or bursts); it fires only while AI_ICANSEEPLAYER is
 * set, except for SPLINE and JUMP_AI enemies.
 */

#include	<stdio.h>
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
#include "game/compobjects.h"
#include "game/enemies.h"
#include "ai/ai.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/oct2.h"

/*===================================================================
	Procedure	:	TURRET Fire At Target..
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_TURRET_FIREATTARGET( ENEMY * Enemy )
{
	OBJECT * TObject;

	TObject = (OBJECT*) Enemy->TShip;


	if( (Enemy->Type != ENEMY_MissileTurret) )
	{
		if( Enemy->Object.Animating && !(Enemy->Object.CurAnimSeq == TURRETSEQ_Fire) )
		{
			return;
		}
	}else{
		if( !Enemy->Object.Animating )
		{
			SetCurAnimSeq( TURRETSEQ_Open, &Enemy->Object );
		}
	}
	
	
	
	AI_THINK( Enemy , true , true );


	// Is it time to validate target ?
	if ( Enemy->Timer == 0.0F )
	{
		if( !( Enemy->AIFlags & AI_ANYPLAYERINRANGE ) || !AI_ClearLOS( &Enemy->Object.Pos ,Enemy->Object.Group , &TObject->Pos ) )
		{
			Enemy->AIFlags &= ~AI_ICANSEEPLAYER;
			SetCurAnimSeq( TURRETSEQ_Closing, &Enemy->Object );
			AI_SetSCAN( Enemy );
			return;
		}
		Enemy->Timer  =	RESET_VALIDATE_TIME + (float) Random_Range( (u_int16_t) RESET_VALIDATE_TIME );

	}else{
		Enemy->Timer -= framelag;
		if( Enemy->Timer < 0.0F )
			Enemy->Timer = 0.0F;
	}

	Enemy->AIFlags |= AI_ICANSEEPLAYER;
	AI_UPDATEGUNS( Enemy );
}

/*===================================================================
	Procedure	:	Update an Enemies Guns...
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_UPDATEGUNS( ENEMY * Enemy )
{
	OBJECT * TObject;
	GUNOBJECT * GObject;
	VECTOR		TempVector;
	VECTOR		TempUpVector;
	VECTOR		TempOffset = { 0.0F, 0.0F, 0.0F };
	VECTOR		NewPos;
	VECTOR		FireOffset;
	FIREPOS *	FirePosPnt;
	VECTOR		AimOffset;
	u_int16_t		i;
	BYTE		Weapon;

	TObject = (OBJECT*) Enemy->TShip;

	if( !TObject || TObject->Mode != NORMAL_MODE )
		return;
	
	GObject = Enemy->Object.FirstGun;

	
	while( GObject )
	{
		
		if( TObject && ( (Enemy->AIFlags & AI_ICANSEEPLAYER) || (Enemy->Object.ControlType == ENEMY_CONTROLTYPE_SPLINE) || (Enemy->Object.ControlType == ENEMY_CONTROLTYPE_JUMP_AI) ) )
		{
			FirePosPnt = EnemyTypes[Enemy->Type].GunFirePoints[GObject->GunNum>>1];
			ApplyMatrix( &GObject->Mat , &FirePosPnt->Points[GObject->FirePosCount] , &FireOffset );
			FireOffset.x += GObject->FirePos.x;
			FireOffset.y += GObject->FirePos.y;
			FireOffset.z += GObject->FirePos.z;
			FireOffset.x -= Enemy->Object.Pos.x;
			FireOffset.y -= Enemy->Object.Pos.y;
			FireOffset.z -= Enemy->Object.Pos.z;
			
			ApplyMatrix( &GObject->Mat, EnemyTypes[ Enemy->Type ].GunAimPos[GObject->GunNum>>1], &AimOffset );
			AimOffset.x += Enemy->Object.Pos.x;
			AimOffset.y += Enemy->Object.Pos.y;
			AimOffset.z += Enemy->Object.Pos.z;
			
			if( !GunTypes[GObject->Type].BurstMasterCount )
			{
				// Normal gun that just fires when it can..
				
				if( GunTypes[GObject->Type].PrimarySecondary )
				{
					AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , SecondaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed);
				}else{
					AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , PrimaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed[GunTypes[GObject->Type].PowerLevel]);
				}
				
				
				AI_AimAtTarget( &GObject->InvMat , &AimOffset, &NewPos );
				GObject->AIMoveFlags |= AimData.Flags;
				GObject->AI_Angle = AimData.Angle;

				if( GObject->Type == GUN_MetaTankMain )
				{
					if( (GObject->ReloadTime <= (0.5F*60.0F) ) && !Enemy->Object.Animating )
					{
						SetCurAnimSeq( 0, &Enemy->Object );
					}
				}
				if( GObject->ReloadTime <= 0.0F && ( (( ( GObject->AI_Angle.x < GunTypes[GObject->Type].BurstAngle && GObject->AI_Angle.x > -GunTypes[GObject->Type].BurstAngle )
					&& ( GObject->AI_Angle.y < GunTypes[GObject->Type].BurstAngle && GObject->AI_Angle.y > -GunTypes[GObject->Type].BurstAngle ) )  ) || GObject->Type == GUN_MetaTankMain ) )
				{
					ApplyMatrix( &GObject->Mat, &Forward, &TempVector );
					ApplyMatrix( &GObject->Mat, &SlideUp, &TempUpVector );
					
					if( !(Enemy->Object.Flags & SHIP_Scattered ) && (!GunTypes[GObject->Type].Range || ( DistanceVector2Vector( &TObject->Pos , &Enemy->Object.Pos) < GunTypes[GObject->Type].Range ) ))
					{

						if(Enemy->Object.ControlType == ENEMY_CONTROLTYPE_TURRET_AI)
						{
							if( !Enemy->Object.Animating && (Enemy->Type != ENEMY_MissileTurret) )
							{
								SetCurAnimSeq( TURRETSEQ_Fire, &Enemy->Object );
							}
						}
						
						if( GunTypes[GObject->Type].PrimarySecondary )
						{
							InitOneSecBull( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Enemy->Object.Group,
								&Enemy->Object.Pos, &FireOffset, &TempVector, &TempUpVector,
								&TempOffset, GunTypes[GObject->Type].WeaponType, false );
							
						}else{
							/* BodgePrimaryWeapon rewrites NME_* weapons to player
							 * pickups when PickupHeld <= PICKUP_Laser. For NME_*
							 * enemy weapons (Pulsar etc.) the rewrite produces
							 * a player weapon that EnemyFirePrimary silently
							 * drops — skip it for already-NME slots. */
							if( GunTypes[GObject->Type].WeaponType >= NME_PULSAR )
								Weapon = GunTypes[GObject->Type].WeaponType;
							else
								Weapon = BodgePrimaryWeapon( GunTypes[GObject->Type].WeaponType, Enemy->PickupHeld );

							i = EnemyFirePrimary( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Weapon,
										Enemy->Object.Group, &Enemy->Object.Pos, &FireOffset, &TempVector, &TempUpVector,
										GunTypes[GObject->Type].PowerLevel, (GunTypes[GObject->Type].PowerLevel +1) * 33.0F, false, GObject );
							if( i != (u_int16_t) -1 )
							{
								PrimBulls[i].FirePoint = GObject->FirePosCount;
							}
						}
						GObject->FirePosCount++;
						if( GObject->FirePosCount >= FirePosPnt->NumPoints )
							GObject->FirePosCount = 0;
						
						
						GObject->ReloadTime =GunTypes[GObject->Type].ReloadTime;
					}
				}
			}else{
				// A Gun That Does a Burst...
				if( GObject->BurstCount )
				{
					
					if( GunTypes[GObject->Type].PrimarySecondary )
					{
						AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , SecondaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed);
					}else{
						AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , PrimaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed[GunTypes[GObject->Type].PowerLevel]);
					}
					AI_AimAtTarget( &GObject->InvMat , &AimOffset, &NewPos );
					GObject->AIMoveFlags |= AimData.Flags;
					GObject->AIMoveFlags &= ~(AI_CONTROL_TURNLEFT + AI_CONTROL_TURNRIGHT );
					GObject->AI_Angle = AimData.Angle;
					if( GObject->BurstStartSign	< 0.0F )
					{
						GObject->AIMoveFlags |= AI_CONTROL_TURNLEFT;
					}else{
						GObject->AIMoveFlags |= AI_CONTROL_TURNRIGHT;
					}
					if( !(Enemy->Object.Flags & SHIP_Scattered ) &&GObject->ReloadTime <= 0.0F )
					{
						ApplyMatrix( &GObject->Mat, &Forward, &TempVector );
						ApplyMatrix( &GObject->Mat, &SlideUp, &TempUpVector );

						if(Enemy->Object.ControlType == ENEMY_CONTROLTYPE_TURRET_AI)
						{
							if( !Enemy->Object.Animating && (Enemy->Type != ENEMY_MissileTurret) )
							{
								SetCurAnimSeq( TURRETSEQ_Fire, &Enemy->Object );
							}
						}

						if( GunTypes[GObject->Type].PrimarySecondary )
						{
							InitOneSecBull( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Enemy->Object.Group,
								&Enemy->Object.Pos, &FireOffset, &TempVector, &TempUpVector,
								&TempOffset, GunTypes[GObject->Type].WeaponType, false );
							
						}else{
							/* BodgePrimaryWeapon rewrites NME_* weapons to player
							 * pickups when PickupHeld <= PICKUP_Laser. For NME_*
							 * enemy weapons (Pulsar etc.) the rewrite produces
							 * a player weapon that EnemyFirePrimary silently
							 * drops — skip it for already-NME slots. */
							if( GunTypes[GObject->Type].WeaponType >= NME_PULSAR )
								Weapon = GunTypes[GObject->Type].WeaponType;
							else
								Weapon = BodgePrimaryWeapon( GunTypes[GObject->Type].WeaponType, Enemy->PickupHeld );

							i = EnemyFirePrimary( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Weapon,
										Enemy->Object.Group, &Enemy->Object.Pos, &FireOffset, &TempVector, &TempUpVector,
										GunTypes[GObject->Type].PowerLevel, (GunTypes[GObject->Type].PowerLevel +1) * 33.0F, false, GObject );
							if( i != (u_int16_t) -1 )
							{
								PrimBulls[i].FirePoint = GObject->FirePosCount;
							}
						}
						
						GObject->FirePosCount++;
						if( GObject->FirePosCount >= FirePosPnt->NumPoints )
							GObject->FirePosCount = 0;
						
						GObject->ReloadTime = GunTypes[GObject->Type].ReloadTime;
						GObject->BurstCount--;
					}
				}
				
				if( !GObject->BurstCount && GunTypes[GObject->Type].BurstMasterCount )
				{
					
					if( GunTypes[GObject->Type].PrimarySecondary )
					{
						AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , SecondaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed);
					}else{
						AI_LookAhead( EnemyTypes[Enemy->Type].Behave.Anticipate_Move ,&AimOffset, TObject , &NewPos , PrimaryWeaponAttribs[GunTypes[GObject->Type].WeaponType].Speed[GunTypes[GObject->Type].PowerLevel]);
					}
					
					AI_AimAtTarget( &GObject->InvMat , &AimOffset, &NewPos );
					GObject->AIMoveFlags |= AimData.Flags;
					GObject->AI_Angle = AimData.Angle;
					if( GObject->BurstTime == 0.0F )
					{
						if( (GObject->AI_Angle.y <= GunTypes[GObject->Type].BurstAngle) && (GObject->AI_Angle.y >= -GunTypes[GObject->Type].BurstAngle) )
						{
							GObject->BurstCount = GunTypes[GObject->Type].BurstMasterCount;
							GObject->BurstTime = GunTypes[GObject->Type].BurstMasterTime;
							GObject->BurstStartSign = GObject->AI_Angle.y;
						}
					}
				}
				
			}
		}
		GObject = GObject->Next;
	}
}
