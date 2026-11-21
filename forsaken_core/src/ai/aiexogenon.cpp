/* aiexogenon.cpp -- Exogenon boss brain (ENEMY_CONTROLTYPE_EXOGENON). IDLE
 * parks it 2560 units above its first spawn point; it then drops in at one
 * of its Exogenon_StartPos spawn points away from the player (MOVEDOWN).
 * Without LOS, or by chance, it rises again firing homing missiles (MOVEUP);
 * otherwise it SCANs, charges and FIREs its primary, SWEEPs, then rises back
 * to IDLE.
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
#include "game/compobjects.h"
#include "game/enemies.h"
#include "ai/ai.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/oct2.h"

void ExogenonAim( ENEMY * Enemy );


void	ExogenonFireLeftRight( ENEMY * Enemy );

namespace {

/* The Exogenon's sweep aim (one instance, g_exogenon_sweep): the fire state
 * remembers the move flags and angle it aimed with, and each sweep tick
 * replays them before re-aiming. */
class ExogenonSweep {
	public:
	void remember( const ENEMY * Enemy )
	{
		ExogenonSweepDir = Enemy->AIMoveFlags;
		ExogenonSweepAngle = Enemy->AI_Angle;
	}
	void replay( ENEMY * Enemy ) const
	{
		Enemy->AIMoveFlags = ExogenonSweepDir;
		Enemy->AI_Angle = ExogenonSweepAngle;
	}

	private:
	VECTOR    ExogenonSweepAngle = {};
	u_int32_t ExogenonSweepDir = 0;
};

ExogenonSweep g_exogenon_sweep;

} // namespace

/*===================================================================
	Procedure	:	Exogenon Idle
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_IDLE( ENEMY * Enemy )
{
	int i;
	int startpos = 0;
	float	Distance = 0.0F;
	AI_THINK( Enemy , true , true );

	if( Enemy->Object.CurAnimSeq != EXOGENONSEQ_Stop_Up )
	{
		SetCurAnimSeq( EXOGENONSEQ_Stop_Up, &Enemy->Object );
		return;
	}

	Enemy->Object.Pos = EnemyState.exogenon_start( 0 );
	Enemy->Object.Pos.y += 256.0F * 10.0F;
	if( !(Enemy->AIFlags & AI_ANYPLAYERINRANGE) )
		return;

	if( Enemy->Timer )
	{
		Enemy->Timer -= framelag;
		if( Enemy->Timer <= 0.0F )
		{
			Enemy->Timer = 0.0F;
		}
		return;
	}

	startpos = Random_Range( (u_int16_t) EnemyState.exogenon_starts());
	i = EnemyState.exogenon_starts();

	while( Distance < (SHIP_RADIUS * 3.0F ) && i >= 0 )
	{
		Enemy->Object.Pos = EnemyState.exogenon_start( startpos );
		startpos++;
		if( startpos >= EnemyState.exogenon_starts() )
			startpos = 0;
		Distance = DistanceVector2Vector( &Enemy->Object.Pos,&Ships[WhoIAm].Object.Pos );
		i--;
	}


	SetCurAnimSeq( EXOGENONSEQ_Move_Down, &Enemy->Object );
	Enemy->Object.AI_Mode = AIMODE_EXOGENON_MOVEDOWN;
}
/*===================================================================
	Procedure	:	Exogenon Move Down
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_MOVEDOWN( ENEMY * Enemy )
{
	VECTOR	TempUpVector;
	VECTOR	FireDir;

	bool	ClearLos = true;
	VECTOR	TempOffset = { 0.0F, 0.0F, 180.0F };
	ExogenonAim( Enemy );

	Enemy->Object.AnimSpeed = 1.5F;
	if( Enemy->Object.Animating )
	{
		return;
	}

	if( !(ClearLos = AI_ClearLOSNonZeroNonObject( &Enemy->Object.Pos,Enemy->Object.Group, &Ships[WhoIAm].Object.Pos , SHIP_RADIUS))
		|| (Random_Range(10) > 5 ) )
	{
		Enemy->Object.AI_Mode = AIMODE_EXOGENON_MOVEUP;
		SetCurAnimSeq( EXOGENONSEQ_Move_Up, &Enemy->Object );

		if( ClearLos )
		{
			ApplyMatrix( &Enemy->Object.Mat, &SlideUp, &TempUpVector );
			ApplyMatrix( &Enemy->Object.Mat, &Forward, &FireDir );
			// Fire a homing missile Forward...
			InitOneSecBull( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Enemy->Object.Group,
				&Enemy->Object.Pos, &TempOffset, &FireDir, &TempUpVector,
				&TempOffset, ENEMYBLUEHOMINGMISSILE, false );
		}
		ExogenonFireLeftRight( Enemy );
	}else{
		Enemy->Object.AI_Mode = AIMODE_EXOGENON_SCAN;
		SetCurAnimSeq( EXOGENONSEQ_Idle, &Enemy->Object );
	}
}
/*===================================================================
	Procedure	:	Exogenon Scan
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_SCAN( ENEMY * Enemy )
{
	ExogenonAim( Enemy );

	Enemy->Object.AnimSpeed = 2.0F;
	if( Enemy->Object.Animating )
	{
		return;
	}
	if( !AI_ClearLOS( &Enemy->Object.Pos,Enemy->Object.Group, &Ships[WhoIAm].Object.Pos ) )
	{
		Enemy->Object.AI_Mode = AIMODE_EXOGENON_MOVEUP;
		SetCurAnimSeq( EXOGENONSEQ_Move_Up, &Enemy->Object );
		ExogenonFireLeftRight( Enemy );
		return;
	}

	Enemy->Object.AI_Mode = AIMODE_EXOGENON_FIRE;
	SetCurAnimSeq( EXOGENONSEQ_Fire, &Enemy->Object );
	Enemy->Timer = 60.0F * 2.5F;

}

/*===================================================================
	Procedure	:	Exogenon Fire
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_FIRE( ENEMY * Enemy )
{
	VECTOR		TempVector;
	VECTOR		TempUpVector;
	VECTOR		TempOffset = { 0.0F, 0.0F, 180.0F };
	float		Distance;


	Enemy->Object.AnimSpeed = 1.0F;

	if( Enemy->Timer )
	{
		Enemy->Timer -= framelag;

		if( Enemy->Timer <= 0.0F )
		{
			// This is where the main Weapon would fire....
			Enemy->Timer = 0.0F;

			if( EnemyTypes[Enemy->Type].PrimaryWeaponType != NO_PRIMARY )
			{
				ApplyMatrix( &Enemy->Object.Mat, &Forward, &TempVector );
				Distance = DistanceVector2Vector( &Ships[WhoIAm].Object.Pos ,&Enemy->Object.Pos );

				TempVector.x *= Distance;
				TempVector.y *= Distance;
				TempVector.z *= Distance;
				TempVector.y -= Enemy->Object.Pos.y - Ships[WhoIAm].Object.Pos.y;
				NormaliseVector(&TempVector);

				ApplyMatrix( &Enemy->Object.Mat, &SlideUp, &TempUpVector );

				EnemyFirePrimary( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, EnemyTypes[Enemy->Type].PrimaryWeaponType,
					Enemy->Object.Group, &Enemy->Object.Pos, &TempOffset, &TempVector, &TempUpVector,
					EnemyTypes[Enemy->Type].PowerLevel, (EnemyTypes[Enemy->Type].PowerLevel +1) * 33.0F, false, nullptr );
			}

			Enemy->Object.AI_Mode = AIMODE_EXOGENON_SWEEP;
			ExogenonAim( Enemy );
			g_exogenon_sweep.remember( Enemy );
			Enemy->Timer = 60.0F * 1.0F;
		}
	}
}
/*===================================================================
	Procedure	:	Exogenon Move Up
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_MOVEUP( ENEMY * Enemy )
{
	Enemy->Object.AnimSpeed = 2.0F;
	if( Enemy->Object.Animating )
	{
		return;
	}

	Enemy->Object.AI_Mode = AIMODE_EXOGENON_IDLE;
	Enemy->Timer = 60.0F * ( 0.5F + Random_Range_Float( 0.5F ) );
}


/*===================================================================
	Procedure	:	Exogenon Aim
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void ExogenonAim( ENEMY * Enemy )
{
	AI_AimAtTarget( &Enemy->Object.InvMat , &Enemy->Object.Pos, &Ships[WhoIAm].Object.Pos );
	Enemy->AIMoveFlags |= AimData.Flags;
	Enemy->AI_Angle = AimData.Angle;
}

/*===================================================================
	Procedure	:	Exogenon Sweep
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void AI_EXOGENON_SWEEP( ENEMY * Enemy )
{
	if( Enemy->Timer )
	{
		Enemy->Timer -= framelag;

		if( Enemy->Timer <= 0.0F )
		{
			Enemy->Timer = 0.0F;
		}
		g_exogenon_sweep.replay( Enemy );
		ExogenonAim( Enemy );

	}

	if( Enemy->Object.Animating )
	{
		return;
	}
	Enemy->Object.AI_Mode = AIMODE_EXOGENON_MOVEUP;
	SetCurAnimSeq( EXOGENONSEQ_Move_Up, &Enemy->Object );
}


/*===================================================================
	Procedure	:	Exogenon Fire homing missiles to the left and right
	Input		:	ENEMY * Enemy
	Output		:	Nothing
===================================================================*/
void	ExogenonFireLeftRight( ENEMY * Enemy )
{
	VECTOR	FireDir;
	VECTOR	TempUpVector;
	VECTOR	TempOffset;

	ApplyMatrix( &Enemy->Object.Mat, &SlideUp, &TempUpVector );

	ApplyMatrix( &Enemy->Object.Mat, &SlideLeft, &FireDir );
	// Fire a homing missile left
	TempOffset.x = FireDir.x * ( 1200.0F * GLOBAL_SCALE );
	TempOffset.y = FireDir.y * ( 1200.0F * GLOBAL_SCALE );
	TempOffset.z = FireDir.z * ( 1200.0F * GLOBAL_SCALE );

	InitOneSecBull( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Enemy->Object.Group,
		&Enemy->Object.Pos, &TempOffset, &FireDir, &TempUpVector,
		&TempOffset, ENEMYBLUEHOMINGMISSILE, false );

	ApplyMatrix( &Enemy->Object.Mat, &SlideRight, &FireDir );
	TempOffset.x = FireDir.x * ( 1200.0F * GLOBAL_SCALE );
	TempOffset.y = FireDir.y * ( 1200.0F * GLOBAL_SCALE );
	TempOffset.z = FireDir.z * ( 1200.0F * GLOBAL_SCALE );
	// Fire a homing missile right
	InitOneSecBull( OWNER_ENEMY, Enemy->Index, ++Enemy->BulletID, Enemy->Object.Group,
		&Enemy->Object.Pos, &TempOffset, &FireDir, &TempUpVector,
		&TempOffset, ENEMYBLUEHOMINGMISSILE, false );
}
