#pragma once

/* triggers.h -- level scripting: trigger variables, conditions and events
 *
 * triggers.cpp loads the level's .trg file (Triggerload); all tables are per
 * level. A TRIGGERMOD applies an Op to a TRIGGERVAR's integer State, at once
 * or after Time framelag ticks via the mod queue. A changed State re-tests
 * each TRIGGER on that var against ActiveState; a CONDITION whose triggers
 * are all Active is queued, and ProcessActiveConditions runs its EVENTs.
 * EVENT.Data is raw bytes decoded per event type. Flag ops and tests number
 * bits from 1. ApplyTriggerMod does nothing while framelag is 0 (paused).
 * Gen/Regen/Copy* serialise trigger state for players joining a game.
 */

#include "net/networking.h"

/*
 * Defines
 */



constexpr int MAXDATAPEREVENT = 64; // in bytes

constexpr int TRIGGEROP_Set = 0;
constexpr int TRIGGEROP_Reset = 1;
constexpr int TRIGGEROP_Inc = 2;
constexpr int TRIGGEROP_Dec = 3;
constexpr int TRIGGEROP_Or = 4;
constexpr int TRIGGEROP_And = 5;
constexpr int TRIGGEROP_Xor = 6;
constexpr int TRIGGEROP_Multiply = 7;
constexpr int TRIGGEROP_Divide = 8;
constexpr int TRIGGEROP_Random = 9;
constexpr int TRIGGEROP_Setflag = 10;
constexpr int TRIGGEROP_Clearflag = 11;


constexpr int TRIGGERTYPE_Equal = 0;
constexpr int TRIGGERTYPE_NotEqual = 1;
constexpr int TRIGGERTYPE_Less = 2;
constexpr int TRIGGERTYPE_Greater = 3;
constexpr int TRIGGERTYPE_LessEqual = 4;
constexpr int TRIGGERTYPE_GreaterEqual = 5;
constexpr int TRIGGERTYPE_FlagCount = 6;
constexpr int TRIGGERTYPE_FlagTest = 7;


 
/*
 * Structures
 */

typedef struct CONDITION{
	int		NumOfTriggers;
	struct	TRIGGER ** Triggers;
	int		NumOfEvents;
	struct	EVENT ** Events;
}CONDITION;

typedef struct EVENT{
	BYTE	Type;				// Which type of event to trigger..
	alignas( 2 ) BYTE	Data[MAXDATAPEREVENT];	// event arguments; the handlers read them as u_int16_t
}EVENT;

typedef struct TRIGGERVAR{
	int		State;				// true / false ...
	int		InitState;
	int		NumOfTriggers;
	char	Name[32];
	struct	TRIGGER ** Triggers;
}TRIGGERVAR;

typedef struct TRIGGER{
	BYTE	Type;
	TRIGGERVAR *	TrigVar;	// true / false ...
	int		ActiveState;		// true / false ...
	int		Active;				// false...true...
	int		NumOfConditions;
	CONDITION ** Conditions;
}TRIGGER;

typedef struct TRIGGERMOD{
	int		Op;
	int		Val;
	float	Time;
	TRIGGERVAR *	TrigVar;
	struct TRIGGERMOD	*Next;
}TRIGGERMOD;


typedef struct TRIGGERMODQUE{
	float	Time;
	struct TRIGGERMOD	*TriggerMod;
}TRIGGERMODQUE;




/*
 * fn prototypes
 */
bool Triggerload( char * Filename );
void ReleaseTriggers( void );
void ApplyTriggerMod( TRIGGERMOD * TrigMod );
void ModifyTriggerVar( TRIGGERVAR * TrigVar , int Op , int Val );
void TestAllConditions( TRIGGER * Trig );
void AddCondition( CONDITION * Con );
void ProcessActiveConditions( void );
void ProcessEvent( EVENT * Event );
void GenTriggerList( u_int16_t Ship, SHORTTRIGGER * TriggerSlots, BYTE * NumTriggerSlots, BYTE Section );
void RegenTriggerList( SHORTTRIGGER * Slots, BYTE Num );
void CopyTriggers( u_int16_t Player );
void GenTrigVarList( u_int16_t Ship, SHORTTRIGVAR * TrigVarSlots, BYTE * NumTrigVarSlots, BYTE Section );
void RegenTrigVarList( SHORTTRIGVAR * Slots, BYTE Num );
void CopyTrigVars( u_int16_t Player );
void HowManySecrets( int * NumOfSecrets , int * SecretsFound );
bool HasLevelEnded( void );
bool IsAvatarActive( void );
bool CheckDecreaseTemperature( void );

// Event Prototypes...
void EVENT_PickupGenerate( u_int8_t * Data );
void EVENT_EnemyGenerate( u_int8_t * Data );
void EVENT_OpenDoor( u_int8_t * Data );
void EVENT_CloseDoor( u_int8_t * Data );
void EVENT_LockDoor( u_int8_t * Data );
void EVENT_UnlockDoor( u_int8_t * Data );
void EVENT_StartBGOAnim( u_int8_t * Data );
void EVENT_StopBGOAnim( u_int8_t * Data );
void EVENT_StartTexAnim( u_int8_t * Data );
void EVENT_StopTexAnim( u_int8_t * Data );
void EVENT_WaterFill( u_int8_t * Data );
void EVENT_WaterDrain( u_int8_t * Data );
void EVENT_StartSpotFX( u_int8_t * Data );
void EVENT_StopSpotFX( u_int8_t * Data );
void EVENT_ShowMessages( u_int8_t * Data );
void EVENT_StartExternalForce( u_int8_t * Data );
void EVENT_StopExternalForce( u_int8_t * Data );
void EVENT_EnableLight( u_int8_t * Data );
void EVENT_DisableLight( u_int8_t * Data );
void EVENT_TriggerAreaEnable( u_int8_t * Data );
void EVENT_TriggerAreaDisable( u_int8_t * Data );
void EVENT_CameraEnable( u_int8_t * Data );
void EVENT_CameraDisable( u_int8_t * Data );
void EVENT_TeleportEnable( u_int8_t * Data );
void EVENT_TeleportDisable( u_int8_t * Data );

FILE * SaveAllTriggers( FILE * fp );
FILE * LoadAllTriggers( FILE * fp );


/* Defined in triggers.cpp; shared with the modules that use it. */
extern TRIGGERMOD * TrigMods;
extern int NumOfTrigMods;
void CheckTimeLimit( void );
extern int NumOfTrigVars;
extern int NumOfTriggers;
extern TRIGGERVAR * DecreaseTemperature;
extern TRIGGERVAR * TimeLimitTrigger;
