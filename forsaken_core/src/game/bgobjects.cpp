/* bgobjects.cpp -- background objects: doors, lifts and animated level parts; see bgobjects.h. */

/*===================================================================
	Includes
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
#include "net/networking.h"
#include "game/ships.h"
#include "rendering/2dpolys.h"
#include "rendering/lights.h"
#include "game/models.h"
#include "rendering/text.h"
#include "game/primary.h"
#include "game/secondary.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "game/trigarea.h"
#include "world/bsp.h"
#include "rendering/lines.h"

#include "world/visi.h"
#include "game/enemies.h"
#include "game/spotfx.h"
#include "world/node.h"
#include "input/controls.h"
#include "util/util.h"
#include "game/oct2.h"
#include "data/tload.h"

/*===================================================================
	Global Variables
===================================================================*/
BGO_FILE	*	BGOFilesPtr = nullptr;
BGOBJECT	*	FirstBGObjectUsed = nullptr;
BGOBJECT	*	FirstBGObjectFree = nullptr;
BGOBJECT		BGObjects[ MAXBGOBJECTS ];
bool			ShowColZones = false;

DOORTYPESFX DoorTypeSFX[] = {

{ -1, (int8_t*)"gndoorop",	-1, nullptr,		-1, (int8_t*)"gndoorst",	-1, nullptr,	-1, (int8_t*)"gndoorop",	-1, nullptr,		-1, (int8_t*)"gndoorcl",	-1, nullptr },
{ -1, (int8_t*)"stndrstr",	-1, (int8_t*)"stndrmov",	-1, (int8_t*)"stndrstp", -1, nullptr,	-1, (int8_t*)"stndrstr",	-1, (int8_t*)"stndrmov",	-1, (int8_t*)"stndrstp", -1, nullptr },
{ -1, (int8_t*)"ffdoropn",	-1, nullptr,		-1, nullptr,		-1, nullptr,	-1, (int8_t*)"ffdorcls",	-1, nullptr,		-1, (int8_t*)"ffdorcns",	-1, nullptr },
{ -1, (int8_t*)"lift1str",	-1, (int8_t*)"lift1lp",	-1, (int8_t*)"lift1stp",	-1, nullptr,	-1, (int8_t*)"lift1str",	-1, (int8_t*)"lift1lp",	-1, (int8_t*)"lift1stp", -1, nullptr },
{ -1, (int8_t*)"smkdorop",	-1, nullptr,		-1, nullptr,		-1, nullptr,	-1, (int8_t*)"smkdorcl",	-1, nullptr,		-1, nullptr,		-1, nullptr },
{ -1, (int8_t*)"eled1str",	-1, (int8_t*)"eled1mov",	-1, (int8_t*)"eled1stp",	-1, nullptr,	-1, (int8_t*)"eled1str",	-1, (int8_t*)"eled1mov",	-1, (int8_t*)"eled1stp",	-1, nullptr },
{ -1, (int8_t*)"eled2str",	-1, (int8_t*)"eled2mov",	-1, (int8_t*)"eled2stp",	-1, nullptr,	-1, (int8_t*)"eled2str",	-1, (int8_t*)"eled2mov",	-1, (int8_t*)"eled2stp",	-1, nullptr },
{ -1, (int8_t*)"eled3str",	-1, (int8_t*)"eled3mov",	-1, (int8_t*)"eled3stp",	-1, nullptr,	-1, (int8_t*)"eled3str",	-1, (int8_t*)"eled3mov",	-1, (int8_t*)"eled3stp",	-1, nullptr },
{ -1, (int8_t*)"eled4str",	-1, (int8_t*)"eled4mov",	-1, (int8_t*)"eled4stp",	-1, nullptr,	-1, (int8_t*)"eled4str",	-1, (int8_t*)"eled4mov",	-1, (int8_t*)"eled4stp",	-1, nullptr },

};

/*===================================================================
	Defines
===================================================================*/
constexpr int BGO_VERSION_NUMBER = 3;

/*===================================================================
	Procedure	:	Initialise all Background Objects
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void InitBGObjects( void )
{
	u_int16_t	i;

	FirstBGObjectUsed = nullptr;
	FirstBGObjectFree = &BGObjects[ 0 ];

	for( i = 0; i < MAXBGOBJECTS; i++ )
	{
		memset( &BGObjects[ i ], 0, sizeof( BGOBJECT ) );

		BGObjects[ i ].ModelIndex = INDEX_NONE;

		BGObjects[ i ].Index = i;
		BGObjects[ i ].NextUsed = nullptr;
		BGObjects[ i ].PrevUsed = nullptr;
		BGObjects[ i ].NextFree = &BGObjects[ i + 1 ];
		BGObjects[ i ].PrevFree = &BGObjects[ i - 1 ];
	}

	BGObjects[ 0 ].PrevFree = nullptr;
	BGObjects[ MAXBGOBJECTS - 1 ].NextFree = nullptr;
}

/*===================================================================
	Procedure	:	Find free Background Object
	Input		:	nothing
	Output		:	BGObject	*	Object ( NULL if not available )
===================================================================*/
BGOBJECT * FindFreeBGObject( void )
{
	BGOBJECT * Object;

	Object = FirstBGObjectFree;

	if( Object != nullptr )
	{
		FirstBGObjectFree = Object->NextFree;
		if( FirstBGObjectFree != nullptr )
		{
			FirstBGObjectFree->PrevFree = nullptr;
		}

		if( FirstBGObjectUsed != nullptr )
		{
			FirstBGObjectUsed->PrevUsed = Object;
			Object->NextUsed = FirstBGObjectUsed;
		}
		else
		{
			Object->NextUsed = nullptr;
		}

		Object->PrevUsed = nullptr;
		FirstBGObjectUsed = Object;
	}

	return( Object );
}

/*===================================================================
	Procedure	:	Kill a Background Object
	Input		:	BGObject	*	Object
	Output		:	nothing
===================================================================*/
void KillUsedBGObject( BGOBJECT * Object )
{
	BGOBJECT	*	PrevObject;
	BGOBJECT	*	NextObject;

	if( Object != nullptr )
	{
		if( Object == FirstBGObjectUsed )
		{
			NextObject = FirstBGObjectUsed->NextUsed;

			FirstBGObjectUsed = NextObject;

			if( FirstBGObjectUsed != nullptr )
			{
				FirstBGObjectUsed->PrevUsed = nullptr;
			}
		}
		else
		{
			PrevObject = Object->PrevUsed;
			NextObject = Object->NextUsed;

			if( PrevObject != nullptr )
			{
				PrevObject->NextUsed = NextObject;
			}

			if( NextObject != nullptr )
			{
				NextObject->PrevUsed = PrevObject;
			}
		}

		if( FirstBGObjectFree != nullptr )
		{
			FirstBGObjectFree->PrevFree = Object;
		}

		if( Object->ModelIndex != INDEX_NONE )
		{
			KillUsedModel( Object->ModelIndex );
			Object->ModelIndex = INDEX_NONE;
		}

		FreeCompObjChildren( Object->Children, Object->NumChildren );
		if( Object->Children )
		{
			free( Object->Children );
			Object->Children = nullptr;
			Object->NumChildren = 0;
		}

		Object->NextFree = FirstBGObjectFree;
		Object->PrevFree = nullptr;
		FirstBGObjectFree = Object;
	}
}

/*===================================================================
	Procedure	:	Process all Background Objects
	Input		:	bool	Collide with objects?
	Output		:	nothing
===================================================================*/
void ProcessBGObjects( bool Collide )
{
	float			Damage = 0.0F;
	BGOBJECT	*	Object;
	BGOBJECT	*	NextObject;
	VECTOR			PushVector;
	bool			UndoAnim = false;

	Object = FirstBGObjectUsed;

	while( Object != nullptr )
	{
		NextObject = Object->NextUsed;

		switch( Object->Type )
		{
/*===================================================================
			STATIC
===================================================================*/
			case BGOTYPE_Static:
				UpdateCompObjChildren( Object->Children, Object->NumChildren, &Object->Matrix, &Object->Pos, Object->Time, Object->Group, &Object->Center );
				break;

/*===================================================================
			DOOR
===================================================================*/
			case BGOTYPE_Door:
				if( Object->StateChangeDelay )
				{
					Object->StateChangeDelay -= framelag;
					if( Object->StateChangeDelay < 0.0F ) Object->StateChangeDelay = 0.0F;
				}

				Object->OldOldTime = Object->OldTime;
				Object->OldTime = Object->Time;

				switch( Object->State )
				{
					case DOOR_OPENING:
						Object->Time += ( framelag * Object->AnimSpeed );
						if( Object->Time > Object->MidTime )
						{
							Object->State = DOOR_OPEN;
							Object->Timer = ( ANIM_SECOND * 3.0F ) - ( Object->Time - Object->MidTime );
							Object->Time = Object->MidTime;
							StartOrChangeDoorSoundFX( Object );

							if( Object->OpenTrigModPtr != nullptr )
							{
								ApplyTriggerMod( Object->OpenTrigModPtr );
							}
						}
						else
						{
							break;
						}
						break;

					case DOOR_REVERSEOPENING:
						Object->Time -= ( framelag * Object->AnimSpeed );
						if( Object->Time < 0.0F )
						{
							Object->State = DOOR_CLOSED;
							Object->Time = 0.0F;
							StartOrChangeDoorSoundFX( Object );

							if( Object->CloseTrigModPtr != nullptr )
							{
								ApplyTriggerMod( Object->CloseTrigModPtr );
							}
						}
						else
						{
							break;
						}
						break;

					case DOOR_OPEN:
						Object->Timer -= ( framelag * Object->AnimSpeed );

						if( Object->Timer < 0.0F )
						{
							Object->Time = Object->MidTime + ( 0.0F - Object->Timer );
							Object->Timer = 0.0F;
							Object->State = DOOR_CLOSING;
							StartOrChangeDoorSoundFX( Object );
						}
						else
						{
							Object->Time = Object->MidTime;
							break;
						}
						break;

					case DOOR_CLOSING:
						Object->Time += ( framelag * Object->AnimSpeed );
						if( Object->Time > Object->OverallTime )
						{
							Object->State = DOOR_CLOSED;
							Object->Time = 0.0F;
							StartOrChangeDoorSoundFX( Object );

							if( Object->CloseTrigModPtr != nullptr )
							{
								ApplyTriggerMod( Object->CloseTrigModPtr );
							}
						}
						break;

					case DOOR_REVERSECLOSING:
						Object->Time -= ( framelag * Object->AnimSpeed );
						if( Object->Time < Object->MidTime )
						{
							Object->State = DOOR_OPEN;
							Object->Timer = ( ANIM_SECOND * 3.0F ) - ( Object->MidTime - Object->Time );
							Object->Time = Object->MidTime;
							StartOrChangeDoorSoundFX( Object );

							if( Object->OpenTrigModPtr != nullptr )
							{
								ApplyTriggerMod( Object->OpenTrigModPtr );
							}
						}
						break;

					case DOOR_CLOSED:
						Object->Time = 0.0F;
						break;

					default:
						break;
				}

				if( Object->UpdateCount || ( Object->Time != Object->OldTime ) )
				{
					UpdateCompObjChildren( Object->Children, Object->NumChildren, &Object->Matrix, &Object->Pos, Object->Time, Object->Group, &Object->Center );

					if( Object->Time != Object->OldTime )
					{
						Object->UpdateCount = 3;

						UndoAnim = false;

						if( Collide )
						{
							if( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE )
							{
								if( CheckBGObjectCollision( &Ships[ WhoIAm ].Object.Pos, Object, &PushVector, &Damage, SHIP_RADIUS ) )
								{
									ChangeBGState( Object, OWNER_SHIP, WhoIAm, BUMP, 0.0F );

									{
										HitMe( OWNER_NOBODY, 0, ( Damage * framelag ), WEPTYPE_Various, 0 );
									}
										ForceExternalOneOff( WhoIAm, &PushVector );
									UndoAnim = true;
								}
							}

							if( CheckBGObjectToEnemies( Object ) )
							{
								ChangeBGState( Object, OWNER_ENEMY, 0, BUMP, 0.0F );
								UndoAnim = true;
							}

							if( CheckBGObjectToPickups( Object ) )
							{
								UndoAnim = true;
							}

							CheckBGObjectToMines( Object );
						}

						if( UndoAnim )
						{
							UndoBGObjectAnim( Object );
							UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						}
					}
					else
					{
						if( Object->UpdateCount ) Object->UpdateCount--;
					}
				}
				break;

/*===================================================================
			LOOPING
===================================================================*/
			case BGOTYPE_Looping_Anim:
				Object->OldOldTime = Object->OldTime;
				Object->OldTime = Object->Time;

				switch( Object->State )
				{
					case LOOPANIM_WaitForDelay:
						Object->Delay -= framelag;
						if( Object->Delay < 0.0F )
						{
							Object->Delay = 0.0F;
							Object->State = LOOPANIM_Animating;
						}
						break;

					case LOOPANIM_WaitForTrigger:
						break;

					case LOOPANIM_Animating:
						if( Object->OverallTime > 0.001F )
						{
							Object->Time += framelag;

							if( Object->Time >= Object->OverallTime )
							{
								Object->Time -= ( (float) fast_floorf( Object->Time / Object->OverallTime ) * Object->OverallTime );
							}
						}
						break;
				}

				if( Object->UpdateCount || ( Object->Time != Object->OldTime ) )
				{
					UpdateCompObjChildren( Object->Children, Object->NumChildren, &Object->Matrix, &Object->Pos, Object->Time, Object->Group, &Object->Center );

					if( Object->Time != Object->OldTime )
					{
						Object->UpdateCount = 3;

						UndoAnim = false;

						if( Collide )
						{
							if( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE )
							{
								if( CheckBGObjectCollision( &Ships[ WhoIAm ].Object.Pos, Object, &PushVector, &Damage, SHIP_RADIUS ) )
								{
									{
										HitMe( OWNER_NOBODY, 0, ( Damage * framelag ), WEPTYPE_Various, 0 );
									}

									ForceExternalOneOff( WhoIAm, &PushVector );
									UndoAnim = true;
								}
							}

							if( CheckBGObjectToEnemies( Object ) )
							{
								UndoAnim = true;
							}

							if( CheckBGObjectToPickups( Object ) )
							{
								UndoAnim = true;
							}

							CheckBGObjectToMines( Object );
						}

						if( UndoAnim )
						{
							UndoBGObjectAnim( Object );
							UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						}
					}
					else
					{
						if( Object->UpdateCount ) Object->UpdateCount--;
					}
				}
				break;

/*===================================================================
			ANIMATE AND STOP AT END
===================================================================*/
			case BGOTYPE_OneOff_Anim:
				Object->OldOldTime = Object->OldTime;
				Object->OldTime = Object->Time;

				switch( Object->State )
				{
					case ONEOFFANIM_WaitForDelay:
						Object->Delay -= framelag;
						if( Object->Delay < 0.0F )
						{
							Object->Delay = 0.0F;
							Object->State = ONEOFFANIM_Animating;
						}
						break;

					case ONEOFFANIM_WaitForTrigger:
						break;

					case ONEOFFANIM_Animating:
						if( Object->Time < Object->OverallTime )
						{
							Object->Time += framelag;
#ifdef EDITION_REMASTER
							/* Sync any CargoDrone waiting at a lift-base node (0x40)
							 * with this BGO's animation progress so the drone rides
							 * the platform rather than being passed through.
							 *
							 * Gate on Object->IsCargoDroneLift so the Y-lerp only
							 * fires for actual lift BGOs (filename "lift*.cob") —
							 * not for lever/door/decoration BGOs that happen to be
							 * animating while the drone sits at a wait node. */
							if( Object->IsCargoDroneLift )
							{
								ENEMY * drone;
								for( drone = FirstEnemyUsed; drone; drone = drone->NextUsed )
								{
									float dx, dz;
									NODE * wait_node;
									NODE * upper_node;
									if( drone->Type != ENEMY_CargoDrone ) continue;
									if( !drone->TNode ) continue;
									wait_node = (NODE*)drone->TNode;
									if( !( wait_node->Flags & 0x40 ) )
									{
										continue;
									}
									/* Gate on drone-to-wait-node XZ distance — the drone is
									 * parked at the wait node when the lift fires, and the
									 * wait node lives in world coords.  BGO origins are
									 * level-designer-placed and can be far from their wait
									 * node (lift 2's BGO is at world origin (0,0,0); lift 5's
									 * BGO is 380u from its wait node; only lift 1 happens to
									 * be near its wait at 213u).  A tight wait-node-based
									 * gate works uniformly for every lift. */
									dx = drone->Object.Pos.x - wait_node->Pos.x;
									dz = drone->Object.Pos.z - wait_node->Pos.z;
									if( dx*dx + dz*dz > 200.0F*200.0F )
									{
										continue;
									}
									/* Lerp drone Y from wait node to its IMMEDIATE upper
									 * neighbor (NodeLink[0]) — Y only, not XZ.  Lerping
									 * X/Z too is geometrically correct for slanted shafts,
									 * but it breaks the wait suppressor's XZ-distance
									 * check (drone moves away from wait_node XZ → wait
									 * disengages → drone tries AI movement → chaos).
									 *
									 * Trade-off: drone Y-rises straight up at wait_node's
									 * XZ; lift platform mesh may visually pull away on
									 * slanted shafts.  Better to look slightly off than
									 * to break parking.  When the animation ends, the
									 * block below advances the drone's TNode to the
									 * upper node. */
									upper_node = ( wait_node->NumOfLinks > 0 ) ? wait_node->NodeLink[0] : nullptr;
									if( upper_node )
									{
										float progress = Object->Time / Object->OverallTime;
										if( progress > 1.0F ) progress = 1.0F;
										drone->Object.Pos.y = wait_node->Pos.y + ( upper_node->Pos.y - wait_node->Pos.y ) * progress;
									}
								}
							}
#endif
						}
						else
						{
							if( Object->EndTrigModPtr != nullptr )
							{
								ApplyTriggerMod( Object->EndTrigModPtr );
							}
#ifdef EDITION_REMASTER
							/* Lift just finished animating.  EndTrigMod fired the
							 * Lift<N>Done state change, which fires NodeEnable on
							 * the upper node and NodeDisable on the wait.  Drone is
							 * sitting at wait_node.XZ with Y at the lerped top; we
							 * need to (a) snap drone position to upper_node and
							 * (b) advance drone TNode to upper_node so its
							 * subsequent AI behavior aims at the next forward
							 * neighbor instead of the now-disabled wait below it.
							 *
							 * Without this, drone TNode stays at the (now 0x60-
							 * disabled) wait and AI tries to navigate back down
							 * to wait.Pos.y, undoing the lift visually. */
							if( Object->IsCargoDroneLift )
							{
								ENEMY * drone;
								for( drone = FirstEnemyUsed; drone; drone = drone->NextUsed )
								{
									NODE * wait_node;
									NODE * upper_node;
									float dx, dz;
									bool upper_is_intermediate_wait;
									if( drone->Type != ENEMY_CargoDrone ) continue;
									if( !drone->TNode ) continue;
									wait_node = (NODE*)drone->TNode;
									/* Drone must have been parked at wait associated
									 * with this lift (XZ-close). */
									dx = drone->Object.Pos.x - wait_node->Pos.x;
									dz = drone->Object.Pos.z - wait_node->Pos.z;
									if( dx*dx + dz*dz > 200.0F*200.0F ) continue;
									upper_node = ( wait_node->NumOfLinks > 0 ) ? wait_node->NodeLink[0] : nullptr;
									if( !upper_node ) continue;
									/* Always advance the drone's TNode forward — the post-
									 * lift NodeDisable(wait) + NodeEnable(upper) trigger pair
									 * has already fired by now, so wait_node is 0x60-disabled
									 * and upper_node is either 0x40 (intermediate shaft wait)
									 * or 0x00 (regular track). */
									drone->TNode         = upper_node;
									drone->NextTNode     = nullptr;
									drone->LastTNode     = wait_node;
									drone->PickNewNodeNow = true;

									/* Only teleport drone POSITION when upper_node is now an
									 * intermediate shaft wait (0x40 set, e.g. lift 2 shaft).
									 * For those, the drone must "be there" so the wait
									 * suppressor parks it and the next switch's
									 * Drone<N>Wait zone-enter fires.
									 *
									 * For single-segment lifts (1, 5, 6, 7, 8 + lift 2's top
									 * segment) upper_node ends up 0x00 — regular track.
									 * Don't teleport position there.  Just advancing TNode
									 * is enough; the AI will walk the drone from the
									 * wait's XZ over to upper_node naturally, no visible
									 * jump on lift end. */
									upper_is_intermediate_wait = (upper_node->Flags & 0x40) != 0;
									if( upper_is_intermediate_wait )
									{
										VECTOR oldPos = drone->Object.Pos;
										drone->Object.Pos.x = upper_node->SolidPos.x;
										drone->Object.Pos.y = upper_node->SolidPos.y;
										drone->Object.Pos.z = upper_node->SolidPos.z;
										drone->Object.Group = (u_int16_t)upper_node->Group;
										TriggerAreaEnemyCheck( &oldPos, &drone->Object.Pos, (u_int16_t)upper_node->Group );
									}
								}
							}
#endif
							Object->Time = Object->OverallTime;
							if( Object->DestroyAtEnd )
							{
								ExplodeChildren( Object->Children, 1 );
								KillUsedBGObject( Object );
								Object = nullptr;
							}
							else
							{
								Object->State = ONEOFFANIM_WaitForTrigger;
							}
						}
						break;
				}

				if( Object )
				{
					if( Object->UpdateCount || ( Object->Time != Object->OldTime ) )
					{
						UpdateCompObjChildren( Object->Children, Object->NumChildren, &Object->Matrix, &Object->Pos, Object->Time, Object->Group, &Object->Center );

						if( Object->Time != Object->OldTime )
						{
							Object->UpdateCount = 3;

							UndoAnim = false;

							if( Collide )
							{
								if( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE )
								{
									if( CheckBGObjectCollision( &Ships[ WhoIAm ].Object.Pos, Object, &PushVector, &Damage, SHIP_RADIUS ) )
									{
										{
											HitMe( OWNER_NOBODY, 0, ( Damage * framelag ), WEPTYPE_Various, 0 );
										}

										ForceExternalOneOff( WhoIAm, &PushVector );
										UndoAnim = true;
									}
								}

								if( CheckBGObjectToEnemies( Object ) )
								{
									UndoAnim = true;
								}

								if( CheckBGObjectToPickups( Object ) )
								{
									UndoAnim = true;
								}

								CheckBGObjectToMines( Object );
							}

							if( UndoAnim )
							{
								UndoBGObjectAnim( Object );
								UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
							}
						}
						else
						{
							if( Object->UpdateCount ) Object->UpdateCount--;
						}
					}
				}
				break;

/*===================================================================
			STEPPED ANIM CAUSED BY EXTERNAL CONDITIONS
===================================================================*/
			case BGOTYPE_MultipleStep_Anim:
				Object->OldOldTime = Object->OldTime;
				Object->OldTime = Object->Time;

				if( Object->Time < Object->WantedTime )
				{
					Object->Time += framelag;

					if( Object->Time > Object->WantedTime )
					{
						Object->Time = Object->WantedTime;
					}

					if( Object->Time >= Object->OverallTime )
					{
						Object->Time = Object->OverallTime;

						if( Object->EndTrigModPtr != nullptr )
						{
							ApplyTriggerMod( Object->EndTrigModPtr );
						}

						if( Object->DestroyAtEnd )
						{
							ExplodeChildren( Object->Children, 1 );
							KillUsedBGObject( Object );
							Object = nullptr;
						}
					}
				}

				if( Object != nullptr )
				{
					if( Object->UpdateCount || ( Object->Time != Object->OldTime ) )
					{
						UpdateCompObjChildren( Object->Children, Object->NumChildren, &Object->Matrix, &Object->Pos, Object->Time, Object->Group, &Object->Center );

						if( Object->Time != Object->OldTime )
						{
							Object->UpdateCount = 3;

							UndoAnim = false;

							if( Collide )
							{
								if( Ships[ WhoIAm ].Object.Mode != LIMBO_MODE )
								{
									if( CheckBGObjectCollision( &Ships[ WhoIAm ].Object.Pos, Object, &PushVector, &Damage, SHIP_RADIUS ) )
									{
										if( WouldObjectCollide( &Ships[ WhoIAm ].Object, &PushVector, SHIP_RADIUS, nullptr ) )
										{
											HitMe( OWNER_NOBODY, 0, ( Damage * framelag ), WEPTYPE_Various, 0 );
										}

										ForceExternalOneOff( WhoIAm, &PushVector );
										UndoAnim = true;
									}
								}

								if( CheckBGObjectToEnemies( Object ) )
								{
									UndoAnim = true;
								}

								if( CheckBGObjectToPickups( Object ) )
								{
									UndoAnim = true;
								}

								CheckBGObjectToMines( Object );
							}

							if( UndoAnim )
							{
								UndoBGObjectAnim( Object );
								UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
							}
						}
						else
						{
							if( Object->UpdateCount ) Object->UpdateCount--;
						}
					}
				}
				break;

/*===================================================================
			SEQUENCE ANIM
===================================================================*/
			case BGOTYPE_Sequences:
				break;
		}

		Object = NextObject;
	}
}

/*===================================================================
	Procedure	:	Kill All Background Objects
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void KillAllBGObjects( void )
{
	BGOBJECT	*	Object;
	BGOBJECT	*	NextObject;

	Object = FirstBGObjectUsed;

	while( Object != nullptr )
	{
		NextObject = Object->NextUsed;
		KillUsedBGObject( Object );
		Object = NextObject;
	}
}

/*===================================================================
	Procedure	:	Change BGObject State
	Input		:	BGOBJECT	*	Object
				:	u_int16_t			Owner Type
				:	u_int16_t			Owner
				:	int16_t			How ( BUMP / SHOT )
				:	float			Damage ( multi-step objects only )
	Output		:	Nothing
===================================================================*/
void ChangeBGState( BGOBJECT * Object, u_int16_t OwnerType, u_int16_t Owner, int16_t How, float Damage )
{
	bool	Activate;

	switch( Object->Type )
	{
/*===================================================================
			STATIC
===================================================================*/
		case BGOTYPE_Static:
			break;

/*===================================================================
			DOOR
===================================================================*/
		case BGOTYPE_Door:
			Activate = false;

			if( How == BUMP )
			{
				if( Object->BumpTrigModPtr != nullptr )
				{
					ApplyTriggerMod( Object->BumpTrigModPtr );
				}

				if( !Object->Locked )
				{
					if( ( OwnerType == OWNER_SHIP ) && ( Object->OpenedBy & WHEN_PlayerBump ) ) Activate = true;
					else if( ( OwnerType == OWNER_ENEMY ) && ( Object->OpenedBy & WHEN_EnemyBump ) ) Activate = true;
				}
				else
				{
					if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) )
					{
						if( Object->PickupNeeded != -1 )
						{
							if( PickupsGot[ Object->PickupNeeded ] )
							{
								Object->Locked = false;
								Activate = true;
								if( Object->PickupNeeded == PICKUP_SkeletonKey ) PickupsGot[ Object->PickupNeeded ]--;
								if( Object->PickupNeeded == PICKUP_GoldFigure ) PickupsGot[ Object->PickupNeeded ]--;
							}
						}
					}
				}
			}
			else
			{
				if( Object->ShotTrigModPtr != nullptr )
				{
					ApplyTriggerMod( Object->ShotTrigModPtr );
				}

				if( !Object->Locked )
				{
					if( ( OwnerType == OWNER_SHIP ) && ( Object->OpenedBy & WHEN_PlayerShot ) ) Activate = true;
					else if( ( OwnerType == OWNER_ENEMY ) && ( Object->OpenedBy & WHEN_EnemyShot ) ) Activate = true;
				}
				else
				{
					if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) )
					{
						if( Object->PickupNeeded != -1 )
						{
							if( PickupsGot[ Object->PickupNeeded ] )
							{
								Object->Locked = false;
								Activate = true;
								if( Object->PickupNeeded == PICKUP_SkeletonKey ) PickupsGot[ Object->PickupNeeded ]--;
								if( Object->PickupNeeded == PICKUP_GoldFigure ) PickupsGot[ Object->PickupNeeded ]--;
							}
						}
					}
				}
			}

			if( Activate && !Object->StateChangeDelay )
			{
				switch( Object->State )
				{
					case DOOR_CLOSED:
						Object->State = DOOR_OPENING;
						if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) ) UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						Object->StateChangeDelay = ANIM_SECOND;
						StartOrChangeDoorSoundFX( Object );
						break;

					case DOOR_OPEN:
						Object->State = DOOR_CLOSING;
						if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) ) UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						Object->StateChangeDelay = ANIM_SECOND;
						StartOrChangeDoorSoundFX( Object );
						break;

					case DOOR_OPENING:
						break;

					case DOOR_CLOSING:
						Object->State = DOOR_REVERSECLOSING;
						if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) ) UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						Object->StateChangeDelay = ANIM_SECOND;
						StartOrChangeDoorSoundFX( Object );
						break;

					case DOOR_REVERSEOPENING:
						Object->State = DOOR_OPENING;
						if( ( OwnerType == OWNER_SHIP ) && ( Owner == WhoIAm ) ) UpdateBGObjectSend( Object->Index, Object->State, Object->Time );
						Object->StateChangeDelay = ANIM_SECOND;
						StartOrChangeDoorSoundFX( Object );
						break;

					case DOOR_REVERSECLOSING:
						break;

					default:
						break;
				}
			}
			break;

/*===================================================================
			LOOPING ANIM
===================================================================*/
		case BGOTYPE_Looping_Anim:
			if( How == BUMP )
			{
				if( Object->BumpTrigModPtr != nullptr )
				{
					if( OwnerType == OWNER_SHIP )
					{
						ApplyTriggerMod( Object->BumpTrigModPtr );
					}
				}
			}
			else
			{
				if( Object->ShotTrigModPtr != nullptr )
				{
					if( OwnerType == OWNER_SHIP )
					{
						ApplyTriggerMod( Object->ShotTrigModPtr );
					}
				}
			}
			break;

/*===================================================================
			ONEOFF ANIM
===================================================================*/
		case BGOTYPE_OneOff_Anim:
			if( How == BUMP )
			{
				if( Object->State != ONEOFFANIM_Animating )
				{
					if( Object->BumpTrigModPtr != nullptr )
					{
						if( OwnerType == OWNER_SHIP )
						{
							ApplyTriggerMod( Object->BumpTrigModPtr );
						}
					}
				}
			}
			else
			{
				if( Object->State != ONEOFFANIM_Animating )
				{
					if( Object->ShotTrigModPtr != nullptr )
					{
						if( OwnerType == OWNER_SHIP )
						{
							ApplyTriggerMod( Object->ShotTrigModPtr );
						}
					}
				}
			}
			break;

/*===================================================================
			MULTI-STEP ANIM
===================================================================*/
		case BGOTYPE_MultipleStep_Anim:
			Activate = false;

			if( How == BUMP )
			{
				if( ( OwnerType == OWNER_SHIP ) && ( Object->OpenedBy & WHEN_PlayerBump ) ) Activate = true;
				else if( ( OwnerType == OWNER_ENEMY ) && ( Object->OpenedBy & WHEN_EnemyBump ) ) Activate = true;

				if( Object->BumpTrigModPtr != nullptr )
				{
					if( Activate )
					{
						ApplyTriggerMod( Object->BumpTrigModPtr );
					}
				}
			}
			else
			{
				if( ( OwnerType == OWNER_SHIP ) && ( Object->OpenedBy & WHEN_PlayerShot ) ) Activate = true;
				else if( ( OwnerType == OWNER_ENEMY ) && ( Object->OpenedBy & WHEN_EnemyShot ) ) Activate = true;

				if( Object->ShotTrigModPtr != nullptr )
				{
					if( Activate )
					{
						ApplyTriggerMod( Object->ShotTrigModPtr );
					}
				}
			}


			if( Activate )
			{
				Object->DamageTime += ( Damage * Object->TimePerDamageUnit );

				if( Object->DamageTime > Object->OverallTime )
				{
					Object->DamageTime = Object->OverallTime;
				}

				if( Object->StepInterval > 0.0001F )
				{
					Object->WantedTime = FDIV( Object->DamageTime, Object->StepInterval );
				}
				else
				{
					Object->WantedTime = 0.0F;
				}
			}
			break;

/*===================================================================
			SEQUENCE ANIM
===================================================================*/
		case BGOTYPE_Sequences:
			break;

/*===================================================================
			UNKNOWN ANIM
===================================================================*/
		default:
			break;

	}
}

/*===================================================================
	Procedure	:	PreLoad All Background Animating Objects
	Input		:	Nothing ( reads the current level's .BGO file )
	Output		:	bool		true/false ( true if there is no file )
===================================================================*/
bool PreLoadBGOFiles( void )
{
	BGO_FILE	*	FilePtr = nullptr;
	char			TempFilename[ 256 ];
	u_int16_t			BaseModel;
	u_int16_t			TempBaseModel = INDEX_NONE;
	FILE		*	fp;
	const char		*	NewExt = ".BGO";
	int16_t			NumBGObjects;
	int16_t			Count;
	int16_t			i;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;

	ModNames = &ModelNames[ 0 ];
	BaseModel = NextNewModel;

	Change_Ext( &LevelNames[ LevelNum ][ 0 ], &TempFilename[ 0 ], NewExt );

	fp = file_open( &TempFilename[ 0 ], "rb" );

	if( fp != nullptr )
	{
		fread( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		fread( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != BGO_VERSION_NUMBER  ) )
		{
			fclose( fp );
			Msg( "PreLoadBGOFiles() Incompatible BGObject (.BGO) file %s", &TempFilename[ 0 ] );
			return( false );
		}

		fread( &NumBGObjects, sizeof( int16_t ), 1, fp );

		if( NumBGObjects > MAXBGOBJECTS )
		{
			fclose( fp );
			Msg( "PreLoadBGOFiles() Too Many BGObjects\n" );
			return( false );
		}

		BGOFilesPtr = (BGO_FILE *) malloc( ( NumBGObjects + 1 ) * sizeof( BGO_FILE ) );

		if( BGOFilesPtr != nullptr )
		{
			FilePtr = BGOFilesPtr;

			for( Count = 0; Count < NumBGObjects; Count++ )
			{
				i = 0;

				do
				{
					fread( &TempFilename[ i ], sizeof( int8_t ), 1, fp );
					i++;
				}
				while( TempFilename[ ( i - 1 ) ] != 0 );

				sprintf( &FilePtr->Filename[ 0 ], "data\\levels\\%s\\bgobjects\\%s", &ShortLevelNames[ LevelNum ][ 0 ], &TempFilename[ 0 ] );

				fread( &FilePtr->Type, sizeof( int16_t ), 1, fp );

				fread( &FilePtr->Group, sizeof( int16_t ), 1, fp );

				fread( &FilePtr->Pos.x, sizeof( float ), 1, fp );
				fread( &FilePtr->Pos.y, sizeof( float ), 1, fp );
				fread( &FilePtr->Pos.z, sizeof( float ), 1, fp );

				fread( &FilePtr->Dir.x, sizeof( float ), 1, fp );
				fread( &FilePtr->Dir.y, sizeof( float ), 1, fp );
				fread( &FilePtr->Dir.z, sizeof( float ), 1, fp );

				fread( &FilePtr->Up.x, sizeof( float ), 1, fp );
				fread( &FilePtr->Up.y, sizeof( float ), 1, fp );
				fread( &FilePtr->Up.z, sizeof( float ), 1, fp );

				switch( FilePtr->Type )
				{
/*	STATIC ANIM */

					case BGOTYPE_Static:
						FilePtr->open_event = -1;
						FilePtr->CloseEvent = -1;
						break;

/*	DOOR */

					case BGOTYPE_Door:
						fread( &FilePtr->open_event, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->CloseEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->ShotEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->BumpEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->PickupNeeded, sizeof( int32_t ), 1, fp );
						fread( &FilePtr->OpenedBy, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->Locked, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->DoorSfxType, sizeof( int16_t ), 1, fp );
						RequestDoorTypeSFX( FilePtr->DoorSfxType );
						break;

/*	LOOPING ANIM */

					case BGOTYPE_Looping_Anim:
						fread( &FilePtr->ShotEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->BumpEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->EndEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->GenType, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->Delay, sizeof( float ), 1, fp );
						break;

/*	ONEOFF ANIM */

					case BGOTYPE_OneOff_Anim:
						fread( &FilePtr->ShotEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->BumpEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->EndEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->GenType, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->Delay, sizeof( float ), 1, fp );
						fread( &FilePtr->DestroyAtEnd, sizeof( int16_t ), 1, fp );
						break;

/*	MULTISTEP ANIM */

					case BGOTYPE_MultipleStep_Anim:
						fread( &FilePtr->ShotEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->BumpEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->EndEvent, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->DestroyAtEnd, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->OpenedBy, sizeof( int16_t ), 1, fp );
						fread( &FilePtr->NumIntervals, sizeof( u_int16_t ), 1, fp );
						fread( &FilePtr->DamagePerInterval, sizeof( float ), 1, fp );
						break;

/*	SEQUENCES ANIM */

					case BGOTYPE_Sequences:
						break;

/*	UNKNOWN ANIM */

					default:
						break;

				}
				FilePtr++;

			}

			FilePtr->Filename[ 0 ] = 0;

			FilePtr = BGOFilesPtr;

			while( FilePtr->Filename[ 0 ] != 0 )
			{
				TempBaseModel = INDEX_NONE;

				if( TempBaseModel != INDEX_NONE )
				{
					FilePtr->BaseModel = TempBaseModel;
				}
				else
				{
					FilePtr->BaseModel = BaseModel;

					if( !PreLoadCompObj( (int8_t*)&FilePtr->Filename[ 0 ], &BaseModel, LEVEL_SPECIFIC ) )
					{
						Msg( "PreloadBGOFiles() PreloadCompObj() failed on %s\n", &FilePtr->Filename[ 0 ] );
						return( false );
				}	}

				FilePtr++;
			}

		}
		else
		{
			Msg( "PreloadBGOFiles() alloc failed on %s\n", &TempFilename[ 0 ] );
			return( false );
		}

		NextNewModel = BaseModel;

		fclose( fp );
	}
	else
	{
		BGOFilesPtr = nullptr;
	}

	RequestDoorTypeSFX( 0 );

	return( true );
}

/*===================================================================
	Procedure	:	Load All Background Animating Objects
	Input		:	Nothing
	Output		:	bool		true/false
===================================================================*/
bool LoadBGOFiles( void )
{
	BGO_FILE	*	FilePtr;
	BGOBJECT	*	Object;
	u_int16_t			BaseModel = MODEL_ExtraModels;

	if( BGOFilesPtr )
	{
		FilePtr = BGOFilesPtr;

		while( FilePtr->Filename[ 0 ] != 0 )
		{
			BaseModel = FilePtr->BaseModel;

			Object = LoadBGObjectData( (int8_t*)&FilePtr->Filename[ 0 ], FilePtr->Type, &FilePtr->Pos,
									   &FilePtr->Dir, &FilePtr->Up, FilePtr->Group,
									   FilePtr->open_event, FilePtr->CloseEvent,
									   FilePtr->ShotEvent, FilePtr->BumpEvent, FilePtr->EndEvent,
									   FilePtr->OpenedBy, FilePtr->DestroyAtEnd, FilePtr->GenType,
									   FilePtr->Delay, FilePtr->Locked, FilePtr->PickupNeeded, &BaseModel,
									   FilePtr->NumIntervals, FilePtr->DamagePerInterval, FilePtr->DoorSfxType );
			if( Object == nullptr )
			{
				return( false );
			}

			GetCompObjBoundingBox( &Object->Matrix, &Object->Pos, Object->Children, Object->NumChildren,
								   Object->OverallTime, &Object->TopLeft, &Object->BottomRight );

			Object->Center.x = Object->TopLeft.x + ( ( Object->BottomRight.x - Object->TopLeft.x ) / 2.0F );
			Object->Center.y = Object->TopLeft.y + ( ( Object->BottomRight.y - Object->TopLeft.y ) / 2.0F );
			Object->Center.z = Object->TopLeft.z + ( ( Object->BottomRight.z - Object->TopLeft.z ) / 2.0F );

			Object->Radius = CalcObjectRadius( &Object->Center, &Object->TopLeft, &Object->BottomRight );

			GetCompObjColBoundingBox( &Object->Matrix, &Object->Pos, Object->Children, Object->NumChildren,
								   Object->OverallTime, &Object->ColTopLeft, &Object->ColBottomRight );

			Object->ColCenter.x = Object->ColTopLeft.x + ( ( Object->ColBottomRight.x - Object->ColTopLeft.x ) / 2.0F );
			Object->ColCenter.y = Object->ColTopLeft.y + ( ( Object->ColBottomRight.y - Object->ColTopLeft.y ) / 2.0F );
			Object->ColCenter.z = Object->ColTopLeft.z + ( ( Object->ColBottomRight.z - Object->ColTopLeft.z ) / 2.0F );

			Object->ColRadius = CalcObjectRadius( &Object->ColCenter, &Object->ColTopLeft, &Object->ColBottomRight );

			SetCompObjRealLighting( Object->Children, Object->NumChildren, Object->Radius );

			FilePtr++;
		}

		free( BGOFilesPtr );
		BGOFilesPtr = nullptr;
	}

	ProcessBGObjects( false );

	return( true );
}

/*===================================================================
	Procedure	:	Load Background Object Data
	Input		:	int8_t		*	Filename of the object's .cob
				:	int16_t			Type
				:	VECTOR		*	Pos
				:	VECTOR		*	DirVector
				:	VECTOR		*	UpVector
				:	u_int16_t			Group
				:	int16_t			OpenTrigMod
				:	int16_t			CloseTrigMod
				:	int16_t			ShotTrigMod
				:	int16_t			BumpTrigMod
				:	int16_t			EndTrigMod
				:	int16_t			Open/Activate By
				:	int16_t			Destroy At End
				:	int16_t			GenType
				:	float			Gen Delay ( seconds )
				:	int16_t			Locked
				:	int32_t			Pickup Needed
				:	u_int16_t		*	BaseModel
				:	u_int16_t			NumIntervals
				:	float			DamagePerInterval
				:	int16_t			DoorSfxType
	Output		:	BGOBJECT	*	Object ( NULL = Error )
===================================================================*/
BGOBJECT * LoadBGObjectData( int8_t * Filename, int16_t Type, VECTOR * Pos, VECTOR * DirVector,
							 VECTOR * UpVector, u_int16_t Group, int16_t OpenTrigMod, int16_t CloseTrigMod,
							 int16_t ShotTrigMod, int16_t BumpTrigMod, int16_t EndTrigMod, int16_t OpenedBy,
							 int16_t DestroyAtEnd, int16_t GenType, float Delay, int16_t Locked, int32_t PickupNeeded,
							 u_int16_t * BaseModel, u_int16_t NumIntervals, float DamagePerInterval, int16_t DoorSfxType )
{
	BGOBJECT	*	Object;
	QUAT			TempQuat;
	MATRIX			TempMatrix;

	Object = FindFreeBGObject();

	if( Object != nullptr )
	{
		Object->State = DOOR_CLOSED;

		switch( Type )
		{
/*	STATIC ANIM */

			case BGOTYPE_Static:
				Object->OpenTrigMod = -1;
				Object->CloseTrigMod = -1;
				Object->ShotTrigMod = -1;
				Object->BumpTrigMod = -1;
				Object->EndTrigMod = -1;
				Object->OpenTrigModPtr = nullptr;
				Object->CloseTrigModPtr = nullptr;
				Object->ShotTrigModPtr = nullptr;
				Object->BumpTrigModPtr = nullptr;
				Object->EndTrigModPtr = nullptr;
				break;

/*	DOOR */

			case BGOTYPE_Door:
				Object->OpenTrigMod = OpenTrigMod;
				Object->CloseTrigMod = CloseTrigMod;
				if( OpenTrigMod == -1 ) Object->OpenTrigModPtr = nullptr;
				else Object->OpenTrigModPtr = &TrigMods[ OpenTrigMod ];
				if( CloseTrigMod == -1 ) Object->CloseTrigModPtr = nullptr;
				else Object->CloseTrigModPtr = &TrigMods[ CloseTrigMod ];
				Object->ShotTrigMod = ShotTrigMod;
				Object->BumpTrigMod = BumpTrigMod;
				Object->Locked = Locked;
				Object->PickupNeeded = PickupNeeded;
				if( ShotTrigMod == -1 ) Object->ShotTrigModPtr = nullptr;
				else Object->ShotTrigModPtr = &TrigMods[ ShotTrigMod ];
				if( BumpTrigMod == -1 ) Object->BumpTrigModPtr = nullptr;
				else Object->BumpTrigModPtr = &TrigMods[ BumpTrigMod ];
				Object->EndTrigMod = -1;
				Object->EndTrigModPtr = nullptr;
				Object->OpenedBy = OpenedBy;
				Object->DoorSfxType = DoorSfxType;
				break;

/*	LOOPING ANIM */

			case BGOTYPE_Looping_Anim:
				Object->OpenTrigMod = -1;
				Object->CloseTrigMod = -1;
				Object->OpenTrigModPtr = nullptr;
				Object->CloseTrigModPtr = nullptr;
				Object->ShotTrigMod = ShotTrigMod;
				Object->BumpTrigMod = BumpTrigMod;
				Object->EndTrigMod = EndTrigMod;
				if( ShotTrigMod == -1 ) Object->ShotTrigModPtr = nullptr;
				else Object->ShotTrigModPtr = &TrigMods[ ShotTrigMod ];
				if( BumpTrigMod == -1 ) Object->BumpTrigModPtr = nullptr;
				else Object->BumpTrigModPtr = &TrigMods[ BumpTrigMod ];
				if( EndTrigMod == -1 ) Object->EndTrigModPtr = nullptr;
				else Object->EndTrigModPtr = &TrigMods[ EndTrigMod ];
				Object->GenType = GenType;
				Object->Delay = ( Delay * ANIM_SECOND );

				switch( GenType )
				{
					case BGOGENTYPE_Immediate:
						Object->State = LOOPANIM_Animating;
						break;
					case BGOGENTYPE_AfterTime:
						Object->State = LOOPANIM_WaitForDelay;
						break;
					case BGOGENTYPE_Triggered:
						Object->State = LOOPANIM_WaitForTrigger;
						break;
				}
				break;

/*	ONEOFF ANIM */

			case BGOTYPE_OneOff_Anim:
				Object->OpenTrigMod = -1;
				Object->CloseTrigMod = -1;
				Object->OpenTrigModPtr = nullptr;
				Object->CloseTrigModPtr = nullptr;
				Object->ShotTrigMod = ShotTrigMod;
				Object->BumpTrigMod = BumpTrigMod;
				Object->EndTrigMod = EndTrigMod;
				if( ShotTrigMod == -1 ) Object->ShotTrigModPtr = nullptr;
				else Object->ShotTrigModPtr = &TrigMods[ ShotTrigMod ];
				if( BumpTrigMod == -1 ) Object->BumpTrigModPtr = nullptr;
				else Object->BumpTrigModPtr = &TrigMods[ BumpTrigMod ];
				if( EndTrigMod == -1 ) Object->EndTrigModPtr = nullptr;
				else Object->EndTrigModPtr = &TrigMods[ EndTrigMod ];
				Object->GenType = GenType;
				Object->Delay = ( Delay * ANIM_SECOND );
				Object->DestroyAtEnd = DestroyAtEnd;

				switch( GenType )
				{
					case BGOGENTYPE_Immediate:
						Object->State = ONEOFFANIM_Animating;
						break;
					case BGOGENTYPE_AfterTime:
						Object->State = ONEOFFANIM_WaitForDelay;
						break;
					case BGOGENTYPE_Triggered:
						Object->State = ONEOFFANIM_WaitForTrigger;
						break;
				}
				break;

/*	MULTISTEP ANIM */

			case BGOTYPE_MultipleStep_Anim:
				Object->OpenTrigMod = -1;
				Object->CloseTrigMod = -1;
				Object->OpenTrigModPtr = nullptr;
				Object->CloseTrigModPtr = nullptr;
				Object->ShotTrigMod = ShotTrigMod;
				Object->BumpTrigMod = BumpTrigMod;
				Object->EndTrigMod = EndTrigMod;
				if( ShotTrigMod == -1 ) Object->ShotTrigModPtr = nullptr;
				else Object->ShotTrigModPtr = &TrigMods[ ShotTrigMod ];
				if( BumpTrigMod == -1 ) Object->BumpTrigModPtr = nullptr;
				else Object->BumpTrigModPtr = &TrigMods[ BumpTrigMod ];
				if( EndTrigMod == -1 ) Object->EndTrigModPtr = nullptr;
				else Object->EndTrigModPtr = &TrigMods[ EndTrigMod ];
				Object->DestroyAtEnd = DestroyAtEnd;
				Object->OpenedBy = OpenedBy;
				break;

/*	SEQUENCES ANIM */

			case BGOTYPE_Sequences:
				break;

/*	UNKNOWN ANIM */

			default:
				break;
		}

		Object->Type = Type;
		Object->Group = Group;
#ifdef EDITION_REMASTER
		/* Tag CargoDrone lift BGOs by filename so the Y-lerp gate can
		 * distinguish them from levers/doors/etc.  Filenames like
		 * "data\levels\defend2\bgobjects\lift1.cob" — check the last
		 * path component. */
		Object->IsCargoDroneLift = false;
		{
			const char * fn = (const char *)Filename;
			const char * base = fn;
			const char * p;
			for( p = fn; *p; p++ )
			{
				if( *p == '\\' || *p == '/' ) base = p + 1;
			}
			if( strncasecmp( base, "lift", 4 ) == 0 )
				Object->IsCargoDroneLift = true;
		}
#endif
		Object->StartPos = *Pos;
		Object->Pos = *Pos;
		Object->DirVector = *DirVector;
		Object->UpVector = *UpVector;
		CrossProduct( UpVector, DirVector, &Object->RightVector );
		Object->MoveOff.x = 0.0F;
		Object->MoveOff.y = 0.0F;
		Object->MoveOff.z = 0.0F;
		Object->CrushCount = 0;
		Object->Frame = 0.0F;
		Object->Offset = 0.0F;
		Object->OffsetInc = 0.0F;
		Object->MaxOffset = 0.0F;
		Object->Timer = 0.0F;
		Object->Width = 1.0F;
		Object->Height = 1.0F;
		Object->Shield = 0.0F;
		Object->OldOldTime = 0.0F;
		Object->OldTime = 0.0F;
		Object->Time = 0.0F;
		Object->OverallTime = 0.0F;
		Object->MidTime = 0.0F;
		Object->AnimSpeed = 1.0F;
		Object->UpdateCount = 3;
		Object->SoundFX_ID = 0;

		TempQuat.FromDirAndUp(DirVector, UpVector);
		TempQuat.ToMatrix(&Object->ColMatrix);
		MatrixTranspose( &Object->ColMatrix, &Object->InvColMatrix );
		TempMatrix = MATRIX_Identity;
		TempMatrix._11 = 1.0F;
		TempMatrix._22 = 1.0F;
		TempMatrix._33 = 1.0F;
		MatrixMultiply( &Object->ColMatrix, &TempMatrix, &Object->Matrix );
		MatrixTranspose( &Object->Matrix, &Object->InvMatrix );

		TempMatrix = MATRIX_Identity;
		TempMatrix._11 = 1.0F;
		TempMatrix._22 = 1.0F;
		TempMatrix._33 = 1.0F;
		Object->InvScaleMatrix = TempMatrix;

		TempMatrix = MATRIX_Identity;
		TempMatrix._11 = 1.0F;
		TempMatrix._22 = 1.0F;
		TempMatrix._33 = 1.0F;
		Object->ScaleMatrix = TempMatrix;

		Object->NumChildren = 1;
		Object->Children = LoadCompObj( Filename, Pos, DirVector, Group,
					&Object->OverallTime, &Object->MidTime, BaseModel, OWNER_BGOBJECT, Object->Index );

		if( !Object->Children )
		{
			Msg( "LoadBGObjectData() LoadCompObj() failded in %s\n", Filename );
			KillUsedBGObject( Object );	/* back to the free list; it has no children */
			return( nullptr );
		}

		switch( Type )
		{
			case BGOTYPE_MultipleStep_Anim:
				Object->StepInterval = ( Object->OverallTime / NumIntervals );
				Object->TimePerDamageUnit = ( Object->StepInterval / DamagePerInterval );
				break;

			case BGOTYPE_Static:
			case BGOTYPE_Door:
			case BGOTYPE_Looping_Anim:
			case BGOTYPE_OneOff_Anim:
			case BGOTYPE_Sequences:
			default:
				break;
		}

	}
	else
	{
		Msg( "LoadBGObjectData() unable to allocate bgobject in %s\n", Filename );
		return( nullptr );
	}

	return( Object );
}

/*===================================================================
	Procedure	:		Update BGObject State
	Input		:		u_int16_t		BGObjbect
				:		int16_t		State
				:		float		Time
	Output		:		Nothing
===================================================================*/
void UpdateBGObject( u_int16_t BGObject, int16_t State, float Time )
{
	int16_t	OldState;

	OldState = BGObjects[ BGObject ].State;

	BGObjects[ BGObject ].State = State;
	BGObjects[ BGObject ].Time = Time;

	switch( State )
	{
		case DOOR_OPENING:
			if( OldState != State )
			{
				StartOrChangeDoorSoundFX( &BGObjects[ BGObject ] );
			}
			break;

		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Resort Back to last anim pos
	Input		:	BGOBJECT	*	BGObject to process
	Output		:	Nothing
===================================================================*/
void UndoBGObjectAnim( BGOBJECT * Object )
{
	if( Object->NumChildren && Object->Children )
	{
		Object->Time = Object->OldTime;
		Object->OldTime = Object->OldOldTime;

		UndoCompObjAnim( Object->Children, Object->NumChildren );
	}
}

/*===================================================================
	Procedure	:	Lock Door
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void LockDoor( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];
	Object->Locked = true;
}

/*===================================================================
	Procedure	:	Unlock Door
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void UnlockDoor( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];
	Object->Locked = false;
}

/*===================================================================
	Procedure	:	Start Anim
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void StartBGOAnim( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];

	switch( Object->Type )
	{
/*	STATIC ANIM */

		case BGOTYPE_Static:
			break;

/*	DOOR */

		case BGOTYPE_Door:
			break;

/*	LOOPING ANIM */

		case BGOTYPE_Looping_Anim:
			switch( Object->State )
			{
				case LOOPANIM_WaitForDelay:
					Object->State = LOOPANIM_Animating;
					Object->Delay = 0.0F;
					break;

				case LOOPANIM_WaitForTrigger:
					if( Object->Delay )
					{
						Object->State = LOOPANIM_WaitForDelay;
					}
					else
					{
						Object->State = LOOPANIM_Animating;
						Object->Time = 0.0F;
					}
					break;

				case LOOPANIM_Animating:
				default:
					break;
			}
			break;

/*	ONEOFF ANIM */

		case BGOTYPE_OneOff_Anim:
			switch( Object->State )
			{
				case ONEOFFANIM_WaitForDelay:
					Object->State = ONEOFFANIM_Animating;
					Object->Delay = 0.0F;
					break;

				case ONEOFFANIM_WaitForTrigger:
					if( Object->Delay )
					{
						Object->State = ONEOFFANIM_WaitForDelay;
					}
					else
					{
						Object->State = ONEOFFANIM_Animating;
						Object->Time = 0.0F;
					}
					break;

				case ONEOFFANIM_Animating:
					break;

				default:
					break;
			}
			break;

/*	MULTISTEP ANIM */

		case BGOTYPE_MultipleStep_Anim:
			break;

/*	SEQUENCES ANIM */

		case BGOTYPE_Sequences:
			break;

/*	UNKNOWN ANIM */

		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Stop Anim
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void StopBGOAnim( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];

	switch( Object->Type )
	{
		case BGOTYPE_Looping_Anim:
			switch( Object->State )
			{
				case LOOPANIM_Animating:
					Object->State = LOOPANIM_WaitForTrigger;
					break;

				case LOOPANIM_WaitForDelay:
				case LOOPANIM_WaitForTrigger:
				default:
					break;
			}
			break;

		case BGOTYPE_OneOff_Anim:
			switch( Object->State )
			{
				case ONEOFFANIM_Animating:
					Object->State = ONEOFFANIM_WaitForTrigger;
					break;

				case ONEOFFANIM_WaitForDelay:
				case ONEOFFANIM_WaitForTrigger:
				default:
					break;
			}
			break;

		case BGOTYPE_Sequences:
			break;

		case BGOTYPE_Static:
		case BGOTYPE_Door:
		case BGOTYPE_MultipleStep_Anim:
		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Open Door
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void OpenDoor( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];

	if( Object->Locked ) return;

	switch( Object->State )
	{
		case DOOR_CLOSED:
			Object->State = DOOR_OPENING;
			break;

		case DOOR_OPEN:
			break;

		case DOOR_OPENING:
			break;

		case DOOR_CLOSING:
			Object->State = DOOR_REVERSECLOSING;
			break;

		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Close Door
	Input		:	u_int16_t			Index
	Output		:	Nothing
===================================================================*/
void CloseDoor( u_int16_t Index )
{
	BGOBJECT * Object;

	Object = &BGObjects[ Index ];

	switch( Object->State )
	{
		case DOOR_CLOSED:
			break;

		case DOOR_OPEN:
			Object->State = DOOR_CLOSING;
			break;

		case DOOR_OPENING:
			Object->State = DOOR_REVERSEOPENING;
			break;

		case DOOR_CLOSING:
			break;

		default:
			break;
	}
}

/*===================================================================
	Procedure	:	Display collision zones of objects and enemies in a group
	Input		:	u_int16_t	Group
	Output		:	nothing
===================================================================*/
void ShowAllColZones( u_int16_t Group )
{
	BGOBJECT	*	Object;
	BGOBJECT	*	NextObject;
	ENEMY		*	NMEObject;
	ENEMY		*	NextNMEObject;

	if( ShowColZones )
	{
		Object = FirstBGObjectUsed;

		while( Object != nullptr )
		{
			NextObject = Object->NextUsed;

			if( Object->Group == Group ) //Ships[ WhoIAm ].Object.Group )
			{
				if( Object->NumChildren && Object->Children )
				{
					ShowCompObjColZones( Object->Children, Object->NumChildren, Object->Group );
				}
			}

			Object = NextObject;
		}

		NMEObject = FirstEnemyUsed;

		while( NMEObject != nullptr )
		{
			NextNMEObject = NMEObject->NextUsed;

			if( NMEObject->Object.Group == Group ) //Ships[ WhoIAm ].NMEObject.Group )
			{
				if( NMEObject->Object.Components )
				{
					ShowCompObjColZones( NMEObject->Object.Components, 1, NMEObject->Object.Group );
				}
			}

			NMEObject = NextNMEObject;
		}
	}
}

/*===================================================================
	Procedure	:	Create Bounding Box ( Lines )
	Input		:	VECTOR	*	x1
				:	VECTOR	*	x2
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void CreateBoundingBox( VECTOR * x1, VECTOR * x2, u_int16_t Group )
{
	CreateLine( x1->x, x1->y, x1->z, x2->x, x1->y, x1->z, Group );
	CreateLine( x2->x, x1->y, x1->z, x2->x, x2->y, x1->z, Group );
	CreateLine( x2->x, x2->y, x1->z, x1->x, x2->y, x1->z, Group );
	CreateLine( x1->x, x2->y, x1->z, x1->x, x1->y, x1->z, Group );

	CreateLine( x1->x, x1->y, x2->z, x2->x, x1->y, x2->z, Group );
	CreateLine( x2->x, x1->y, x2->z, x2->x, x2->y, x2->z, Group );
	CreateLine( x2->x, x2->y, x2->z, x1->x, x2->y, x2->z, Group );
	CreateLine( x1->x, x2->y, x2->z, x1->x, x1->y, x2->z, Group );

	CreateLine( x1->x, x1->y, x1->z, x1->x, x1->y, x2->z, Group );
	CreateLine( x2->x, x1->y, x1->z, x2->x, x1->y, x2->z, Group );
	CreateLine( x2->x, x2->y, x1->z, x2->x, x2->y, x2->z, Group );
	CreateLine( x1->x, x2->y, x1->z, x1->x, x2->y, x2->z, Group );
}

/*===================================================================
	Procedure	:	Update BGObjects Clip Group
	Input		:	CAMERA	*	Camera
	Output		:	nothing
===================================================================*/
void UpdateBGObjectsClipGroup( CAMERA * Camera )
{
	BGOBJECT	*	Object;
	BGOBJECT	*	NextObject;
	u_int16_t			ClipGroup;

	Object = FirstBGObjectUsed;

	while( Object != nullptr )
	{
		NextObject = Object->NextUsed;

		if( IsGroupVisible[ Object->Group ] || (CAMERA_VIEW_IS_VALID && (
				IsGroupVisible[ Ships[ Current_Camera_View ].Object.Group ] ||
				VisibleOverlap( Ships[ Current_Camera_View ].Object.Group, Object->Group, nullptr )
		)))
		{
			ClipGroup = FindClipGroup( Camera, &Mloadheader, &Object->TopLeft, &Object->BottomRight );
			UpdateCompObjClipGroup( Object->Children, Object->NumChildren, ClipGroup );
		}

		Object = NextObject;
	}
}


/*===================================================================
	Procedure	:	Calc Largest Radius of Bounding Box
	Input		:	VECTOR	*	Pos
				:	VECTOR	*	TopLeft of bounding Box
				:	VECTOR	*	BottomRight of bounding Box
	Output		:	float		Distance from Pos to the farthest corner
===================================================================*/
float CalcObjectRadius( VECTOR * Pos, VECTOR * TopLeft, VECTOR * BottomRight )
{
	VECTOR	Vert;
	float	Distance = 0.0F;
	float	Length;

	Vert.x = TopLeft->x - Pos->x;
	Vert.y = TopLeft->y - Pos->y;
	Vert.z = TopLeft->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = BottomRight->x - Pos->x;
	Vert.y = TopLeft->y - Pos->y;
	Vert.z = TopLeft->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = TopLeft->x - Pos->x;
	Vert.y = BottomRight->y - Pos->y;
	Vert.z = TopLeft->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = TopLeft->x - Pos->x;
	Vert.y = TopLeft->y - Pos->y;
	Vert.z = BottomRight->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = BottomRight->x - Pos->x;
	Vert.y = BottomRight->y - Pos->y;
	Vert.z = TopLeft->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = TopLeft->x - Pos->x;
	Vert.y = BottomRight->y - Pos->y;
	Vert.z = BottomRight->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = BottomRight->x - Pos->x;
	Vert.y = TopLeft->y - Pos->y;
	Vert.z = BottomRight->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	Vert.x = BottomRight->x - Pos->x;
	Vert.y = BottomRight->y - Pos->y;
	Vert.z = BottomRight->z - Pos->z;
	Length = VectorLength( &Vert );
	if( Length > Distance ) Distance = Length;
	return( Distance );
}

/*===================================================================
	Procedure	:	Check if Background object hit enemy
	Input		:	BGOBJECT	*	Object
	Output		:	bool			True/False
===================================================================*/
bool CheckBGObjectToEnemies( BGOBJECT * Object )
{
	int16_t		Count;
	GROUPLIST * GroupsVisible;
	u_int16_t	  * GroupList;
	u_int16_t		CurrentGroup;
	u_int16_t		DebugCount = 0;
	ENEMY	*	Enemy;
	ENEMY	*	NextEnemy;
	bool		HitFlag = false;
	VECTOR		PushVector;
	float		Damage;

	GroupsVisible = VisibleGroups( Ships[ Current_Camera_View ].Object.Group );
	GroupList = GroupsVisible->group;

	for( Count = 0; Count < GroupsVisible->groups; Count++ )
	{
		CurrentGroup = GroupList[ Count ];

		Enemy = EnemyGroups[ CurrentGroup ];

		while( Enemy )
		{
			NextEnemy = Enemy->NextInGroup;

			if( ( Enemy->Status & ENEMY_STATUS_Enable ) && Enemy->Alive )
			{
				if( DebugCount > MAXENEMIES )
				{
					Msg( "CheckBGObjectToEnemies() Link list corrupt!!" );
					return( true );	/* treat a corrupt list as a hit: stop the object */
				}

				DebugCount++;

				if( CheckBGObjectCollision( &Enemy->Object.Pos, Object, &PushVector, &Damage, EnemyTypes[ Enemy->Type ].Radius ) )
				{
#ifdef EDITION_REMASTER
					/* CargoDrone riding a lift (0x40 wait node): suppress HitFlag so
					 * UndoBGObjectAnim is NOT called. Y position is driven by the
					 * lerp in the OneOff_Anim update block above; PushVector.y here
					 * would double-count and fight the lerp. */
					if( Enemy->Type == ENEMY_CargoDrone &&
						Enemy->TNode && ( ((NODE*)Enemy->TNode)->Flags & 0x40 ) )
					{
						/* intentionally empty — no HitFlag, no pos change */
					}
					else
#endif
					{
					HitFlag = true;

					{
						if( Damage != 0.0F )
						{
							Enemy->Object.Shield -= Damage;
							if( Enemy->Object.Shield < 0 ) Enemy->Object.Shield = 0;
							if( Enemy->Object.Shield == 0 )
							{
								CreateNewExplosion( &Enemy->Object.Pos, &PushVector, CurrentGroup );
								KillUsedEnemy( Enemy );
							}
							else
							{
								CreateColSparks( &Enemy->Object.Pos, &PushVector, CurrentGroup );
							}
						}
					}

					Enemy->Object.ExternalForce.x += PushVector.x;
					Enemy->Object.ExternalForce.y += PushVector.y;
					Enemy->Object.ExternalForce.z += PushVector.z;
					}
				}
			}

			Enemy = NextEnemy;
		}
	}

	return( HitFlag );
}

/*===================================================================
	Procedure	:	Check if Background object hit pickup
	Input		:	BGOBJECT	*	Object
	Output		:	bool			True/False
===================================================================*/
bool CheckBGObjectToPickups( BGOBJECT * Object )
{
	int16_t		Count;
	GROUPLIST * GroupsVisible;
	u_int16_t	  * GroupList;
	u_int16_t		CurrentGroup;
	u_int16_t		DebugCount = 0;
	PICKUP	*	Pickup;
	PICKUP	*	NextPickup;
	bool		HitFlag = false;
	VECTOR		PushVector;
	float		Damage;

	VECTOR		ColPoint;
	u_int16_t		ColGroup;
	NORMAL		ColNormal;

	GroupsVisible = VisibleGroups( Ships[ Current_Camera_View ].Object.Group );
	GroupList = GroupsVisible->group;

	for( Count = 0; Count < GroupsVisible->groups; Count++ )
	{
		CurrentGroup = GroupList[ Count ];

		Pickup = PickupGroups[ CurrentGroup ];

		while( Pickup )
		{
			NextPickup = Pickup->NextInGroup;

			if( DebugCount > MAXPICKUPS )
			{
				Msg( "CheckBGObjectToPickups() Link list corrupt!!" );
				return( true );	/* treat a corrupt list as a hit: stop the object */
			}

			DebugCount++;

			if( CheckBGObjectCollision( &Pickup->Pos, Object, &PushVector, &Damage, PICKUP_MODEL_RADIUS ) )
			{
				HitFlag = true;

				Pickup->ExternalSpeed = ( 2.0F * VectorLength( &PushVector ) );
				Pickup->Dir = PushVector;
				Pickup->ColFlag = 0;
				NormaliseVector( &Pickup->Dir );

				if( QCollide( &Pickup->Pos, Pickup->Group, &PushVector, PICKUP_MODEL_RADIUS,
							  &ColPoint, &ColGroup, &ColNormal ) )
				{
					if( MyGameStatus == STATUS_SinglePlayer )			// Pickup being crushed!!!
					{
						if( ReturnClosestNode( &Pickup->Pos, &ColPoint, &ColGroup, &Object->TopLeft, &Object->BottomRight ) >= 0.0F )
						{
							InitOnePickup( &ColPoint, ColGroup, &Forward, 0.0F, Pickup->Type,
										   Pickup->Owner, ++Ships[WhoIAm].PickupIdCount, -1,
										   true, -1.0F, INDEX_NONE );

							KillPickup( Pickup->Owner, Pickup->ID, PICKUPKILL_Immediate );
						}
					}
				}
			}

			Pickup = NextPickup;
		}
	}

	return( HitFlag );
}

/*===================================================================
	Procedure	:	Check if Background object hit Mine
	Input		:	BGOBJECT	*	Object
	Output		:	bool			True/False
===================================================================*/
bool CheckBGObjectToMines( BGOBJECT * Object )
{
	int16_t		Count;
	GROUPLIST * GroupsVisible;
	u_int16_t	  * GroupList;
	u_int16_t		CurrentGroup;
	SECONDARYWEAPONBULLET * SecBull;
	SECONDARYWEAPONBULLET * NextSecBull;
	bool		HitFlag = false;
	VECTOR		PushVector;
	float		Damage;

	GroupsVisible = VisibleGroups( Ships[ Current_Camera_View ].Object.Group );
	GroupList = GroupsVisible->group;

	for( Count = 0; Count < GroupsVisible->groups; Count++ )
	{
		CurrentGroup = GroupList[ Count ];

		SecBull = SecBullGroups[ CurrentGroup ];

		while( SecBull )
		{
			NextSecBull = SecBull->NextInGroup;

			switch( SecBull->SecType )
			{
				case SEC_MINE:
					if( CheckBGObjectCollision( &SecBull->Pos, Object, &PushVector, &Damage, MINE_RADIUS ) )
					{
						DestroySecondary( SecBull->Index, &SecBull->Pos );
					}
					break;

				case SEC_MISSILE:
				case SEC_XMISSILE:
				default:
					break;
			}

			SecBull = NextSecBull;
		}
	}

	return( HitFlag );
}

/*===================================================================
	Procedure	:	Explode Childern
	Input		:	COMP_OBJ	*	Children
				:	int16_t			Number of Children
	Output		:	Nothing
===================================================================*/
constexpr auto STEPSIZE = ( BLOCKSIZE * 0.5F );

void ExplodeChildren( COMP_OBJ * Children, int16_t NumChildren )
{
	int16_t	Count;
	VECTOR	Pos;
	VECTOR	TempPos;
	int16_t	XCount, YCount, ZCount;
	int16_t	XNum, YNum, ZNum;
	float	XOffset, YOffset, ZOffset;
	int16_t	Chance;
	float	Radius;
	u_int16_t	Group;

	int16_t	VisNum;
	u_int16_t	VisGroups[ MAXGROUPS ];
	int16_t	Count2;

	if( Children && NumChildren )
	{
		for( Count = 0; Count < NumChildren; Count++ )
		{
			if( Children->ModelIndex != INDEX_NONE )
			{
				ApplyMatrix( &Models[ Children->ModelIndex ].Mat,
							 &ModelHeaders[ Models[ Children->ModelIndex ].ModelNum ].Center, &Pos );
				Pos.x += Models[ Children->ModelIndex ].Pos.x;
				Pos.y += Models[ Children->ModelIndex ].Pos.y;
				Pos.z += Models[ Children->ModelIndex ].Pos.z;

				VisNum = VisibleOverlap( Ships[ Current_Camera_View ].Object.Group, Models[ Children->ModelIndex ].Group, &VisGroups[ 0 ] );
				Group = Models[ Children->ModelIndex ].Group;

				for( Count2 = 0; Count2 < VisNum; Count2++ )
				{
					if( PointInsideSkin( &Pos, VisGroups[ Count2 ] ) )
					{
						Group = VisGroups[ Count2 ];
						break;
					}
				}

				XNum = (int16_t) ( ModelHeaders[ Models[ Children->ModelIndex ].ModelNum ].Sizes.x / STEPSIZE );
				YNum = (int16_t) ( ModelHeaders[ Models[ Children->ModelIndex ].ModelNum ].Sizes.y / STEPSIZE );
				ZNum = (int16_t) ( ModelHeaders[ Models[ Children->ModelIndex ].ModelNum ].Sizes.z / STEPSIZE );
				if( !XNum ) XNum = 1;
				if( !YNum ) YNum = 1;
				if( !ZNum ) ZNum = 1;

				Chance = (int16_t) ( 5.0F + ( ( XNum * YNum * ZNum ) * 0.1F ) );

				Radius = VectorLength( &ModelHeaders[ Models[ Children->ModelIndex ].ModelNum ].Sizes ) + ( BLOCKSIZE * 0.5F );
				MissileShockWave( &Pos, Radius, WhoIAm, 20.0F, Models[ Children->ModelIndex ].Group, 0 );

				PlayPannedSfx( SFX_BangBang, Group, &Pos, 0.0F );

				ZOffset = Pos.z - ( ( ZNum / 2 ) * STEPSIZE );

				for( ZCount = 0; ZCount < ZNum; ZCount++ )
				{
					YOffset = Pos.y - ( ( YNum / 2 ) * STEPSIZE );

					for( YCount = 0; YCount < YNum; YCount++ )
					{
						XOffset = Pos.x - ( ( XNum / 2 ) * STEPSIZE );

						for( XCount = 0; XCount < XNum; XCount++ )
						{
							TempPos.x = XOffset;
							TempPos.y = YOffset;
							TempPos.z = ZOffset;

							if( PointInsideSkin( &TempPos, Group ) )
							{
								if( !Random_Range( Chance ) )
								{
									CreateSpotFXShrapnel( &TempPos, &Models[ Children->ModelIndex ].Dir, Group );
								}
								else
								{
									CreateBigExplosion( &TempPos, Group );
								}
							}
							else
							{
								DebugPrintf( "Attempt to generate shrapnel outside map\n" );
							}

							XOffset += STEPSIZE;
						}

						YOffset += STEPSIZE;
					}

					ZOffset += STEPSIZE;
				}
			}

			ExplodeChildren( Children->Children, Children->NumChildren );
			Children++;
		}
	}
}

/*===================================================================
	Procedure	:	Save BGObjects Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveBGObjects( FILE * fp )
{
	int		i;
	u_int16_t	TempIndex = INDEX_NONE;

	if( fp )
	{
		if( FirstBGObjectUsed != nullptr ) fwrite( &FirstBGObjectUsed->Index, sizeof( u_int16_t ), 1, fp );
		else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
		if( FirstBGObjectFree != nullptr ) fwrite( &FirstBGObjectFree->Index, sizeof( u_int16_t ), 1, fp );
		else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );

		for( i = 0; i < MAXBGOBJECTS; i++ )
		{
			fwrite( &BGObjects[ i ].State, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].Type, sizeof( u_int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].ModelIndex, sizeof( u_int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].StartPos, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].RightVector, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].MoveOff, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].CrushCount, sizeof( u_int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].Frame, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Matrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].InvMatrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].ColMatrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].InvColMatrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].ScaleMatrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].InvScaleMatrix, sizeof( MATRIX ), 1, fp );
			fwrite( &BGObjects[ i ].Offset, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].OffsetInc, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].MaxOffset, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Timer, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Width, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Height, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Shield, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].NumChildren, sizeof( int16_t ), 1, fp );

			if( BGObjects[ i ].Children && BGObjects[ i ].NumChildren )
			{
				fp = SaveAllCompObj( fp, BGObjects[ i ].Children, BGObjects[ i ].NumChildren );
			}

			fwrite( &BGObjects[ i ].OldOldTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].OldTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Time, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].OverallTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].MidTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].AnimSpeed, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].StateChangeDelay, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].OpenTrigMod, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].CloseTrigMod, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].ShotTrigMod, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].BumpTrigMod, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].EndTrigMod, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].Locked, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].OpenedBy, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].DestroyAtEnd, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].PickupNeeded, sizeof( int32_t ), 1, fp );
			fwrite( &BGObjects[ i ].GenType, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].Delay, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].WantedTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].DamageTime, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].StepInterval, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].TimePerDamageUnit, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].DamagePerInterval, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Center, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].TopLeft, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].BottomRight, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].Radius, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].ColCenter, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].ColTopLeft, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].ColBottomRight, sizeof( VECTOR ), 1, fp );
			fwrite( &BGObjects[ i ].ColRadius, sizeof( float ), 1, fp );
			fwrite( &BGObjects[ i ].Index, sizeof( u_int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].UpdateCount, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].DoorSfxType, sizeof( int16_t ), 1, fp );
			fwrite( &BGObjects[ i ].SoundFX_ID, sizeof( u_int32_t ), 1, fp );

			if( BGObjects[ i ].PrevUsed != nullptr ) fwrite( &BGObjects[ i ].PrevUsed->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( BGObjects[ i ].NextUsed != nullptr ) fwrite( &BGObjects[ i ].NextUsed->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( BGObjects[ i ].PrevFree != nullptr ) fwrite( &BGObjects[ i ].PrevFree->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( BGObjects[ i ].NextFree != nullptr ) fwrite( &BGObjects[ i ].NextFree->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load BGObjects Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadBGObjects( FILE * fp )
{
	int		i;
	u_int16_t	TempIndex;
	int16_t	NumChildren;

	if( fp )
	{
		fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
		if( TempIndex != INDEX_NONE ) FirstBGObjectUsed = &BGObjects[ TempIndex ];
		else FirstBGObjectUsed = nullptr;
		fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
		if( TempIndex != INDEX_NONE ) FirstBGObjectFree = &BGObjects[ TempIndex ];
		else FirstBGObjectFree = nullptr;

		for( i = 0; i < MAXBGOBJECTS; i++ )
		{
			fread( &BGObjects[ i ].State, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].Type, sizeof( u_int16_t ), 1, fp );
			fread( &BGObjects[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fread( &BGObjects[ i ].ModelIndex, sizeof( u_int16_t ), 1, fp );
			fread( &BGObjects[ i ].StartPos, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].RightVector, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].MoveOff, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].CrushCount, sizeof( u_int16_t ), 1, fp );
			fread( &BGObjects[ i ].Frame, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Matrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].InvMatrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].ColMatrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].InvColMatrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].ScaleMatrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].InvScaleMatrix, sizeof( MATRIX ), 1, fp );
			fread( &BGObjects[ i ].Offset, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].OffsetInc, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].MaxOffset, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Timer, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Width, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Height, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Shield, sizeof( float ), 1, fp );
			fread( &NumChildren, sizeof( int16_t ), 1, fp );

			if( NumChildren )
			{
				fp = LoadAllCompObj( fp, BGObjects[ i ].Children, BGObjects[ i ].NumChildren );
			}
			else
			{
				if( BGObjects[ i ].Children && BGObjects[ i ].NumChildren )
				{
					FreeCompObjChildren( BGObjects[ i ].Children, BGObjects[ i ].NumChildren );
					free( BGObjects[ i ].Children );
					BGObjects[ i ].Children = nullptr;
					BGObjects[ i ].NumChildren = 0;
				}
			}

			fread( &BGObjects[ i ].OldOldTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].OldTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Time, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].OverallTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].MidTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].AnimSpeed, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].StateChangeDelay, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].OpenTrigMod, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].CloseTrigMod, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].ShotTrigMod, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].BumpTrigMod, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].EndTrigMod, sizeof( int16_t ), 1, fp );
			if( BGObjects[ i ].OpenTrigMod == -1 ) BGObjects[ i ].OpenTrigModPtr = nullptr;
			else BGObjects[ i ].OpenTrigModPtr = &TrigMods[ BGObjects[ i ].OpenTrigMod ];
			if( BGObjects[ i ].CloseTrigMod == -1 ) BGObjects[ i ].CloseTrigModPtr = nullptr;
			else BGObjects[ i ].CloseTrigModPtr = &TrigMods[ BGObjects[ i ].CloseTrigMod ];
			if( BGObjects[ i ].ShotTrigMod == -1 ) BGObjects[ i ].ShotTrigModPtr = nullptr;
			else BGObjects[ i ].ShotTrigModPtr = &TrigMods[ BGObjects[ i ].ShotTrigMod ];
			if( BGObjects[ i ].BumpTrigMod == -1 ) BGObjects[ i ].BumpTrigModPtr = nullptr;
			else BGObjects[ i ].BumpTrigModPtr = &TrigMods[ BGObjects[ i ].BumpTrigMod ];
			if( BGObjects[ i ].EndTrigMod == -1 ) BGObjects[ i ].EndTrigModPtr = nullptr;
			else BGObjects[ i ].EndTrigModPtr = &TrigMods[ BGObjects[ i ].EndTrigMod ];
			fread( &BGObjects[ i ].Locked, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].OpenedBy, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].DestroyAtEnd, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].PickupNeeded, sizeof( int32_t ), 1, fp );
			fread( &BGObjects[ i ].GenType, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].Delay, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].WantedTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].DamageTime, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].StepInterval, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].TimePerDamageUnit, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].DamagePerInterval, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Center, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].TopLeft, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].BottomRight, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].Radius, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].ColCenter, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].ColTopLeft, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].ColBottomRight, sizeof( VECTOR ), 1, fp );
			fread( &BGObjects[ i ].ColRadius, sizeof( float ), 1, fp );
			fread( &BGObjects[ i ].Index, sizeof( u_int16_t ), 1, fp );
			fread( &BGObjects[ i ].UpdateCount, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].DoorSfxType, sizeof( int16_t ), 1, fp );
			fread( &BGObjects[ i ].SoundFX_ID, sizeof( u_int32_t ), 1, fp );

			fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) BGObjects[ i ].PrevUsed = &BGObjects[ TempIndex ];
			else  BGObjects[ i ].PrevUsed = nullptr;
			fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) BGObjects[ i ].NextUsed = &BGObjects[ TempIndex ];
			else  BGObjects[ i ].NextUsed = nullptr;
			fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) BGObjects[ i ].PrevFree = &BGObjects[ TempIndex ];
			else  BGObjects[ i ].PrevFree = nullptr;
			fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) BGObjects[ i ].NextFree = &BGObjects[ TempIndex ];
			else  BGObjects[ i ].NextFree = nullptr;
		}
	}

	return( fp );
}


/*===================================================================
	BIKE INFO
===================================================================*/
BIKEINFO BikeCompFiles[ MAXBIKETYPES ] = {

	{ "", 0, 0, 0.0F },				//"Lokasenna",
	{ "", 0, 0, 0.0F },				//"Beard",
	{ "", 0, 0, 0.0F },				//"L.A. Jay",
	{ "", 0, 0, 0.0F },				//"Ex-Cop",
	{ "", 0, 0, 0.0F },				//"Rex Hardy",
	{ "", 0, 0, 0.0F },				//"Foetoid",
	{ "", 0, 0, 0.0F },				//"Nim Soo Sun",
	{ "", 0, 0, 0.0F },				//"Nutta",
	{ "", 0, 0, 0.0F },				//"Sceptre",
	{ "", 0, 0, 0.0F },				//"Jo",
	{ "", 0, 0, 0.0F },				//"Cuvel Clark",
	{ "", 0, 0, 0.0F },				//"HK 5",
	{ "", 0, 0, 0.0F },				//"Nubia",
	{ "", 0, 0, 0.0F },				//"Mofisto",
	{ "", 0, 0, 0.0F },				//"Cerbero",
	{ "", 0, 0, 0.0F },				//"Slick",
	{ "FlyGirl.cob", 0, 0, 0.0F },	//"FlyGirl"
};

/*===================================================================
	Procedure	:	PreLoad All Components for Ships
	Input		:	Nothing
	Output		:	bool		true/false
===================================================================*/
bool PreLoadShips( void )
{
	int16_t	Count;
	u_int16_t	BaseModel;
	char	TempFilename[ 256 ];

	ModNames = &ModelNames[ 0 ];

	BaseModel = NextNewModel;

	for( Count = 0; Count < MAXBIKETYPES; Count++ )
	{
		if( BikeCompFiles[ Count ].Filename[ 0 ] )
		{
			sprintf( &TempFilename[ 0 ], "data\\bgobjects\\%s", BikeCompFiles[ Count ].Filename );

			BikeCompFiles[ Count ].BaseModel = BaseModel;
			BikeModels[ Count ] = BaseModel;

			if( !PreLoadCompObj( (int8_t*)&TempFilename[ 0 ], &BaseModel, NOT_LEVEL_SPECIFIC ) )
			{
				Msg( "PreLoadShips() failed on %s\n", &TempFilename[ 0 ] );
				return( false );
			}

			sprintf( &TempFilename[ 0 ], "data\\bgobjects\\s%s", BikeCompFiles[ Count ].Filename );

			BikeCompFiles[ Count ].StealthBaseModel = BaseModel;

			if( !PreLoadCompObj( (int8_t*)&TempFilename[ 0 ], &BaseModel, NOT_LEVEL_SPECIFIC ) )
			{
				Msg( "PreLoadShips() failed on %s\n", &TempFilename[ 0 ] );
				return( false );
			}
		}
	}

	NextNewModel = BaseModel;

	return( true );
}

/*===================================================================
	Procedure	:	Allocate all Components for Ship
	Input		:	u_int16_t	Ship
	Output		:	bool	true/false
===================================================================*/
bool AllocateCompShip( u_int16_t Ship )
{
	int16_t			Type;
	u_int16_t			StartModel, BaseModel;
	char			TempFilename[ 256 ];
	COMP_OBJ	*	CompObj;
	float			MidTime;
	VECTOR			DirVector;

	Type = ( Ships[ Ship ].BikeNum % MAXBIKETYPES );

	if( BikeCompFiles[ Type ].Filename[ 0 ] )
	{
		sprintf( &TempFilename[ 0 ], "data\\bgobjects\\%s", BikeCompFiles[ Type ].Filename );

		BaseModel = BikeCompFiles[ Type ].BaseModel;
		StartModel = BaseModel;

		ApplyMatrix( &Ships[ Ship ].Object.FinalMat, &Forward, &DirVector );

		CompObj = LoadCompObj( (int8_t*)&TempFilename[ 0 ], &Ships[ Ship ].Object.Pos, &DirVector,
							   Ships[ Ship ].Object.Group, &BikeCompFiles[ Type ].OverallTime,
							   &MidTime, &BaseModel, OWNER_SHIP, Ship );
		if( CompObj )
		{
			SetStealthOffset( CompObj, 1, (int16_t) ( BaseModel - StartModel ) );

			Ships[ Ship ].Object.Components = CompObj;
			Ships[ Ship ].Object.AnimSpeed = 1.0F;
			Ships[ Ship ].Object.Time = 0.0F;
			Ships[ Ship ].Object.OverallTime = BikeCompFiles[ Type ].OverallTime;
		}
		else
		{
			Msg( "AllocateCompShip() failded in %s\n", &TempFilename[ 0 ] );
			return( false );
		}
	}
	else
	{
		return( false );
	}

	return( true );
}

/*===================================================================
	Procedure	:	Request Door SFX
	Input		:	int16_t	Type
	Output		:	Nothing
===================================================================*/
void RequestDoorTypeSFX( int16_t Type )
{
	DoorTypeSFX[ Type ].StartOpeningSoundFX	= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].StartOpeningName );	// Start Opening
	DoorTypeSFX[ Type ].OpeningSoundFX		= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].OpeningName );		// Opening
	DoorTypeSFX[ Type ].StartOpenSoundFX	= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].StartOpenName );		// Start Open
	DoorTypeSFX[ Type ].OpenSoundFX			= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].OpenName );			// Open
	DoorTypeSFX[ Type ].StartClosingSoundFX	= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].StartClosingName );	// Start Closing
	DoorTypeSFX[ Type ].ClosingSoundFX		= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].ClosingName );		// Closing
	DoorTypeSFX[ Type ].StartClosedSoundFX	= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].StartClosedName );	// Start Closed
	DoorTypeSFX[ Type ].ClosedSoundFX		= ReturnSFXIndex( (char*)DoorTypeSFX[ Type ].ClosedName );			// Closed
}

/*===================================================================
	Procedure	:	Start Or Change Door SFX
	Input		:	BGOBJECt	*	Door
	Output		:	Nothing
===================================================================*/
void StartOrChangeDoorSoundFX( BGOBJECT * Object )
{
	int16_t	Sfx1, Sfx2;

	if( Object->SoundFX_ID )
	{
		StopSfx( Object->SoundFX_ID );
		Object->SoundFX_ID = 0;
	}

	switch( Object->State )
	{
		case DOOR_CLOSED:
			Sfx1 = DoorTypeSFX[ Object->DoorSfxType ].StartClosedSoundFX;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].ClosedSoundFX;
			break;

		case DOOR_OPEN:
			Sfx1 = DoorTypeSFX[ Object->DoorSfxType ].StartOpenSoundFX;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].OpenSoundFX;
			break;

		case DOOR_OPENING:
			Sfx1 = DoorTypeSFX[ Object->DoorSfxType ].StartOpeningSoundFX;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].OpeningSoundFX;
			break;

		case DOOR_CLOSING:
			Sfx1 = DoorTypeSFX[ Object->DoorSfxType ].StartClosingSoundFX;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].ClosingSoundFX;
			break;

		case DOOR_REVERSEOPENING:
			Sfx1 = -1;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].ClosingSoundFX;
			break;

		case DOOR_REVERSECLOSING:
			Sfx1 = -1;
			Sfx2 = DoorTypeSFX[ Object->DoorSfxType ].OpeningSoundFX;
			break;

		default:
			Sfx1 = -1;
			Sfx2 = -1;
			break;
	}

	if( Sfx1 != -1 )
	{
		Object->SoundFX_ID = PlayFixedSpotSfx( Sfx1, Object->Group, &Object->Center, 1.0F, 1.0F, SPOT_SFX_TYPE_Normal );
	}

	if( Sfx2 != -1 )
	{
		Object->SoundFX_ID = PlayFixedSpotSfx( Sfx2, Object->Group, &Object->Center, 1.0F, 1.0F, SPOT_SFX_TYPE_Normal );
	}
}

BIKEINFO FlyGirlCompFile = { "FlyGirl.cob", 0, 0, 0.0F };
COMP_OBJ * FlyGirlComps = nullptr;
float FlyGirlAnim;

/*===================================================================
	Procedure	:	PreLoad All Components for FlyGirl
	Input		:	Nothing
	Output		:	bool		true/false
===================================================================*/
bool PreLoadFlyGirl( void )
{
	u_int16_t	BaseModel;
	char	TempFilename[ 256 ];

	ModNames = &TitleModelNames[ 0 ];
	FlyGirlComps = nullptr;

	BaseModel = TITLE_MODEL_ExtraModels;

	if( FlyGirlCompFile.Filename[ 0 ] )
	{
		sprintf( &TempFilename[ 0 ], "data\\bgobjects\\%s", FlyGirlCompFile.Filename );

		FlyGirlCompFile.BaseModel = BaseModel;

		if( !PreLoadCompObj( (int8_t*)&TempFilename[ 0 ], &BaseModel, NOT_LEVEL_SPECIFIC ) )
		{
			Msg( "PreLoadFlyGirl() failed on %s\n", &TempFilename[ 0 ] );
			return( false );
		}
	}
	return( true );
}

/*===================================================================
	Procedure	:	Allocate all Components for FlyGirl
	Input		:	Nothing
	Output		:	bool	true/false
===================================================================*/
bool AllocateCompFlyGirl( void )
{
	u_int16_t			BaseModel;
	char			TempFilename[ 256 ];
	COMP_OBJ	*	CompObj;
	float			MidTime;
	VECTOR			TempVector = { 0.0F, 0.0F, 0.0F };
	VECTOR			DirVector;

	DebugPrintf("Allocating components for flygirl...\n");

	if( FlyGirlCompFile.Filename[ 0 ] )
	{
		sprintf( &TempFilename[ 0 ], "data\\bgobjects\\%s", FlyGirlCompFile.Filename );

		BaseModel = FlyGirlCompFile.BaseModel;

		DirVector = Forward;

		CompObj = LoadCompObj( (int8_t*)&TempFilename[ 0 ], &TempVector, &DirVector,
							   0, &FlyGirlCompFile.OverallTime,
							   &MidTime, &BaseModel, OWNER_SHIP, 0 );
		if( CompObj )
		{
			FlyGirlComps = CompObj;
		}
		else
		{
			Msg( "AllocateCompFlyGirl() failded in %s\n", &TempFilename[ 0 ] );
			return( false );
		}
	}
	else
	{
		return( false );
	}

	return( true );
}

/*===================================================================
	Procedure	:	Release Flygirl components and models
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ReleaseFlyGirl( void )
{
	if( FlyGirlComps )
	{
		FreeCompObjChildren( FlyGirlComps, 1 );

		if( FlyGirlComps )
		{
			free( FlyGirlComps );
			FlyGirlComps = nullptr;
		}
	}
}

/*===================================================================
	Procedure	:	Animate And Update Components
	Input		:	VECTOR	*	Pos
				:	MATRIX	*	Matrix
				:	bool		Visible
	Output		:	Nothing
===================================================================*/
void UpdateFlyGirl( VECTOR * Pos, MATRIX * Matrix, bool Visible )
{
	FlyGirlAnim += framelag;

	if( FlyGirlAnim >= FlyGirlCompFile.OverallTime )
	{
		FlyGirlAnim = FMOD( FlyGirlAnim, FlyGirlCompFile.OverallTime );
	}

	SetCompObjModelsState( FlyGirlComps, 1, Visible );
	UpdateCompObjChildren( FlyGirlComps, 1, Matrix, Pos, FlyGirlAnim, 0, Pos );
}
