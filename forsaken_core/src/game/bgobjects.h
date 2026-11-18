/*===================================================================
	File	:	bgobjects.h
	Content	:	BGObjects.c include file
===================================================================*/
#pragma once
#include "game/main.h"
#include "game/camera.h"
#include "game/constants.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAXBGOBJECTS = 256;

constexpr int BGOTYPE_Static = 0;
constexpr int BGOTYPE_Door = 1;
constexpr int BGOTYPE_Looping_Anim = 2;
constexpr int BGOTYPE_MultipleStep_Anim = 3;
constexpr int BGOTYPE_OneOff_Anim = 4;
constexpr int BGOTYPE_Sequences = 5;

constexpr int DOOR_CLOSED = 0;
constexpr int DOOR_OPENING = 1;
constexpr int DOOR_OPEN = 2;
constexpr int DOOR_CLOSING = 3;
constexpr int DOOR_REVERSEOPENING = 4;
constexpr int DOOR_REVERSECLOSING = 5;

constexpr int WHEN_PlayerBump = 1;
constexpr int WHEN_PlayerShot = 2;
constexpr int WHEN_EnemyBump = 4;
constexpr int WHEN_EnemyShot = 8;

constexpr int BUMP = 0;
constexpr int SHOT = 1;

constexpr int BGOGENTYPE_Immediate = 0;
constexpr int BGOGENTYPE_AfterTime = 1;
constexpr int BGOGENTYPE_Triggered = 2;

constexpr int LOOPANIM_WaitForDelay = 0;
constexpr int LOOPANIM_WaitForTrigger = 1;
constexpr int LOOPANIM_Animating = 2;

constexpr int ONEOFFANIM_WaitForDelay = 0;
constexpr int ONEOFFANIM_WaitForTrigger = 1;
constexpr int ONEOFFANIM_Animating = 2;


/*===================================================================
	Structures
===================================================================*/
typedef struct BGOBJECT {

	int16_t		State;
	u_int16_t		Type;
	u_int16_t		Group;
	u_int16_t		ModelIndex;
	VECTOR		StartPos;
	VECTOR		Pos;
	VECTOR		DirVector;
	VECTOR		UpVector;
	VECTOR		RightVector;
	VECTOR		MoveOff;
	u_int16_t		CrushCount;
	float		Frame;
	MATRIX		Matrix;
	MATRIX		InvMatrix;
	MATRIX		ColMatrix;
	MATRIX		InvColMatrix;
	MATRIX		ScaleMatrix;
	MATRIX		InvScaleMatrix;
	float		Offset;
	float		OffsetInc;
	float		MaxOffset;
	float		Timer;
	float		Width;
	float		Height;
	float		Shield;
	int16_t		NumChildren;
	struct COMP_OBJ	* Children;

	float		OldOldTime;
	float		OldTime;
	float		Time;
	float		OverallTime;
	float		MidTime;
	float		AnimSpeed;

	float		StateChangeDelay;

	int16_t		OpenTrigMod;		// Trigger Mod pointer index
	int16_t		CloseTrigMod;		// Trigger Mod pointer index
	struct TRIGGERMOD * OpenTrigModPtr;		// Trigger Mod Array
	struct TRIGGERMOD * CloseTrigModPtr;		// Trigger Mod Array
	int16_t		ShotTrigMod;		// Trigger Mod pointer index
	int16_t		BumpTrigMod;		// Trigger Mod pointer index
	int16_t		EndTrigMod;			// Trigger Mod pointer index
	struct TRIGGERMOD * ShotTrigModPtr;		// Trigger Mod Array
	struct TRIGGERMOD * BumpTrigModPtr;		// Trigger Mod Array
	struct TRIGGERMOD * EndTrigModPtr;		// Trigger Mod Array
	int16_t		Locked;
	int16_t		OpenedBy;
	int16_t		DestroyAtEnd;
	int32_t		PickupNeeded;
	int16_t		GenType;
	float		Delay;

	float		WantedTime;
	float		DamageTime;
	float		StepInterval;
	float		TimePerDamageUnit;
	float		DamagePerInterval;

	VECTOR		ColTopLeft;
	VECTOR		ColBottomRight;
	VECTOR		ColCenter;
	float		ColRadius;

	VECTOR		TopLeft;
	VECTOR		BottomRight;
	VECTOR		Center;
	float		Radius;
	u_int16_t		Index;
	int16_t		UpdateCount;

	int16_t		DoorSfxType;
	u_int32_t		SoundFX_ID;

#ifdef EDITION_REMASTER
	/* Tagged at LoadBGObjectData when the filename matches "lift*.cob".
	 * The defend2 CargoDrone Y-lerp gate uses this to distinguish lift
	 * BGOs from lever/door BGOs that happen to be animating while the
	 * drone is parked at a wait node.  Geometric gates don't suffice:
	 * some lift BGOs are placed at world origin (0,0,0). */
	bool		IsCargoDroneLift;
#endif

	struct	BGOBJECT	*	PrevUsed;
	struct	BGOBJECT	*	NextUsed;
	struct	BGOBJECT	*	PrevFree;
	struct	BGOBJECT	*	NextFree;

} BGOBJECT;

typedef struct BGO_FILE {

	char		Filename[ 256 ];
	u_int16_t		BaseModel;
	u_int16_t		Type;
	int16_t		open_event;	// renamed cause OpenEvent is part of winapi
	int16_t		CloseEvent;
	int16_t		ShotEvent;
	int16_t		BumpEvent;
	int16_t		EndEvent;
	int32_t		PickupNeeded;		// -1 if none needed
	int16_t		OpenedBy;
	int16_t		Locked;
	int16_t		DestroyAtEnd;
	int16_t		GenType;
	float		Delay;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		Dir;
	VECTOR		Up;
	u_int16_t		NumIntervals;
	float		DamagePerInterval;
	int16_t		DoorSfxType;

} BGO_FILE;

typedef struct BIKEINFO {

	const char *	Filename;
	u_int16_t		BaseModel;
	u_int16_t		StealthBaseModel;
	float		OverallTime;

} BIKEINFO;

typedef struct DOORTYPESFX {

	int16_t		StartOpeningSoundFX;
	int8_t	*	StartOpeningName;
	int16_t		OpeningSoundFX;
	int8_t	*	OpeningName;
	int16_t		StartOpenSoundFX;
	int8_t	*	StartOpenName;
	int16_t		OpenSoundFX;
	int8_t	*	OpenName;
	int16_t		StartClosingSoundFX;
	int8_t	*	StartClosingName;
	int16_t		ClosingSoundFX;
	int8_t	*	ClosingName;
	int16_t		StartClosedSoundFX;
	int8_t	*	StartClosedName;
	int16_t		ClosedSoundFX;
	int8_t	*	ClosedName;

} DOORTYPESFX;

/*===================================================================
	Prototypes
===================================================================*/
void InitBGObjects( void );
BGOBJECT * FindFreeBGObject( void );
void KillUsedBGObject( BGOBJECT * Object );
void ProcessBGObjects( bool Collide );
void KillAllBGObjects( void );
void ChangeBGState( BGOBJECT * Object, u_int16_t OwnerType, u_int16_t Owner, int16_t How, float Damage );
bool PreLoadBGOFiles( void );
bool LoadBGOFiles( void );
BGOBJECT * LoadBGObjectData( int8_t * Filename, int16_t Type, VECTOR * Pos, VECTOR * DirVector,
							 VECTOR * UpVector, u_int16_t Group, int16_t OpenTrigMod, int16_t CloseTrigMod,
							 int16_t ShotTrigMod, int16_t BumpTrigMod, int16_t EndTrigMod, int16_t OpenedBy,
							 int16_t DestroyAtEnd, int16_t GenType, float Delay, int16_t Locked, int32_t PickupNeeded,
							 u_int16_t * BaseModel, u_int16_t NumIntervals, float DamagePerInterval, int16_t DoorSfxType );
void UpdateBGObject( u_int16_t BGObject, int16_t State, float Time );
void UndoBGObjectAnim( BGOBJECT * Object );
void OpenDoor( u_int16_t Index );
void CloseDoor( u_int16_t Index );
void LockDoor( u_int16_t Index );
void UnlockDoor( u_int16_t Index );
void StartBGOAnim( u_int16_t Index );
void StopBGOAnim( u_int16_t Index );
void ShowAllColZones( u_int16_t Group );
void CreateBoundingBox( VECTOR * x1, VECTOR * x2, u_int16_t Group );
float CalcObjectRadius( VECTOR * Pos, VECTOR * TopLeft, VECTOR * BottomRight );
bool CheckBGObjectToEnemies( BGOBJECT * Object );
bool CheckBGObjectToPickups( BGOBJECT * Object );
bool CheckBGObjectToMines( BGOBJECT * Object );
void ExplodeChildren( COMP_OBJ * Children, int16_t NumChildren );
FILE * SaveBGObjects( FILE * fp );
FILE * LoadBGObjects( FILE * fp );
bool PreLoadShips( void );
bool AllocateCompShip( u_int16_t Ship );
void RequestDoorTypeSFX( int16_t Type );
void StartOrChangeDoorSoundFX( BGOBJECT * Object );

bool PreLoadFlyGirl( void );
bool AllocateCompFlyGirl( void );
void UpdateFlyGirl( VECTOR * Pos, MATRIX * Matrix, bool Visible );
void ReleaseFlyGirl( void );

/* Defined in bgobjects.cpp; shared with the modules that use it. */
extern BIKEINFO BikeCompFiles[ MAXBIKETYPES ];
extern bool ShowColZones;
void UpdateBGObjectsClipGroup( CAMERA * Camera );
extern BGOBJECT * FirstBGObjectUsed;
extern BGOBJECT BGObjects[ MAXBGOBJECTS ];
