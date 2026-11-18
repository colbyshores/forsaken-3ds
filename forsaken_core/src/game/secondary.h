
/*==========================================================================
 *
 *
 *  File:       Secondary.h
 *  Content:    Secondary include file
 *
 *
 ***************************************************************************/

#pragma once
#include "game/constants.h"
#include "game/main.h"
#include "util/timer.h"

/*===================================================================
	Defines
===================================================================*/

constexpr int MIS_ACCELERATE = 0; // Missile Sequences
constexpr int MIS_DROP = 1;
constexpr int MIS_STRAIGHT = 2;
constexpr int MIS_HOMING = 3;
constexpr int MIS_GRAVITY = 4;
constexpr int MIS_SPIRALING = 5;
constexpr int MIS_RETRACTTENTACLE = 6;
									
constexpr int MISMOVE_STRAIGHT = 0; // Movement types
constexpr int MISMOVE_HOMING = 1;

constexpr int SEC_MISSILE = 0; // Secondary Weapon Type
constexpr int SEC_MINE = 1;
constexpr int SEC_XMISSILE = 2;

constexpr int MAXSECONDARYWEAPONBULLETS = 256;

constexpr int MAXENEMYSECONDARYWEAPONS = 6;
constexpr auto TOTALSECONDARYWEAPONS = ( MAXSECONDARYWEAPONS + 2 + MAXENEMYSECONDARYWEAPONS );

constexpr int MAXSCATTERED = 9;

constexpr int MUGMISSILE = 0; // Straight
constexpr int SOLARISMISSILE = 1; // Homing
constexpr int THIEFMISSILE = 2; // Straight ( Grab best weapon )
constexpr int SCATTERMISSILE = 3; // Scatter ( Grab all weapons )
constexpr int GRAVGONMISSILE = 4; // Straight ( Gravity effecting )
constexpr int MULTIPLEMISSILE = 5; // Multiple Rocket Launcher
constexpr int TITANSTARMISSILE = 6; // Titan Star ( Create shrapnel on contact )
constexpr int PURGEMINE = 7; // Purge Mine ( Proximity detonated )
constexpr int PINEMINE = 8; // Pine Mine ( Aim and fire )
constexpr int QUANTUMMINE = 9; // Quantum Mine ( Proximity detonated )
constexpr int SPIDERMINE = 10; // Spider Mine ( Proximity detonated, Moving )
constexpr int PINEMISSILE = 11; // Pine Mine Missile
constexpr int TITANSTARSHRAPNEL = 12; // Titan Star Shrapnel
constexpr int ENEMYSPIRALMISSILE = 13; // Straight Spiralling destructable missile
constexpr int ENEMYHOMINGMISSILE = 14; // Homing destructable missile
constexpr int ENEMYBLUEHOMINGMISSILE = 15; // Homing destructable missile ( BLUE )
constexpr int ENEMYFIREBALL = 16; // FireBall
constexpr int ENEMYTENTACLE = 17; // Tentacle
constexpr int ENEMYDEPTHCHARGE = 18; // DepthCharge

constexpr int NO_SECONDARY = 128; // No Secondary weapon to speak of....

constexpr float COS_CONE = -2.0F; //0.7071F
constexpr int MAX_ENTRYS = 10;
constexpr auto MINE_RADIUS = ( 256.0F * GLOBAL_SCALE ); // Mine Radius
constexpr auto MINE_TRIGGER_RADIUS = ( 800.0F * GLOBAL_SCALE ); // Detonation Radius

constexpr float PURGE_SHOCKWAVE = 4.0F;
constexpr float PINE_SHOCKWAVE = 4.0F;
constexpr float QUANTUM_SHOCKWAVE = 20.0F;
constexpr float SPIDER_SHOCKWAVE = 4.0F;
constexpr auto MISSILE_SHOCKWAVE = ( 512 * GLOBAL_SCALE ); // Missile Shockwave ( Actual size )
constexpr float TITANSTAR_SHOCKWAVE = 15.0F;
constexpr float TENTACLE_SHOCKWAVE = 10.0F;
constexpr float DEPTHCHARGE_SHOCKWAVE = 10.0F;

constexpr int MAXOLDPOS = 10;
constexpr int MAXTENTOLDPOS = 20;
constexpr int NUMSCATTERPOINTS = 3;

constexpr auto SEC_LIGHTSIZE = ( 1536.0F * GLOBAL_SCALE );

constexpr int MM_TYPE_KILL = 0;
constexpr int MM_TYPE_INIT = 1;

constexpr int SECFLAGS_Nothing = 0;
constexpr int SECFLAGS_CanShoot = 1; // Can be shot.
constexpr int SECFLAGS_CanTarget = 2; // Can be targeted
constexpr int SECFLAGS_NoIncoming = 4; // Dont Play Incoming

constexpr auto SPIRAL_RADIUS = ( 100 * GLOBAL_SCALE );

/*===================================================================
	Structures
===================================================================*/
typedef	struct OLDPOS {

	VECTOR		Pos;			// Old Position
	VECTOR		Dir;			// Old Direction
	u_int8_t		R;				// Red
	u_int8_t		G;				// Green
	u_int8_t		B;				// Blue

} OLDPOS;

typedef struct SECONDARYWEAPONATTRIB {

	u_int16_t	SecType;			// Missile/Mine
	u_int16_t	State;				// Starting State
	u_int16_t	MoveType;			// Movement Type ( Straight/Homeing )
	float	StartSpeed;			// how fast do I move to start with
	float	SpeedInc;			// how fast do I Accelerate
	float	Speed;				// how fast do I move
	float	Size;				// how big is my collide sphere
	float	TurnSpeed;			// how quickly can I turn
	u_int16_t	FireDelay;			// how fast can I fire
	u_int16_t	ViewCone;			// how wide is my view
	u_int16_t	ModelNum;			// if 3D model which one....
	float	lightsize;			// how big is the light
	float	r;					// what color does it emit....
	float	g;					// what color does it emit....
	float	b;					// what color does it emit....
	float	Shield;				// How much shield do I have....
	float	Damage;				// How much Damage do I do.....
	bool	Lensflare;			// Lensflare......
	float	ColRadius;			// how big is my collide sphere
	u_int16_t	Flags;				// Various Flags;

} SECONDARYWEAPONATTRIB;

typedef struct SECONDARYWEAPONBULLET {
	bool	Used;

	u_int16_t	Next;				// link list.....	
	u_int16_t	Prev;				// link list.....
	struct SECONDARYWEAPONBULLET * NextInGroup;// Next in same group ....
	struct SECONDARYWEAPONBULLET * PrevInGroup;// Previous in same group ....
	u_int16_t	Flags;				// Flags
	u_int16_t	Index;				// Index
	u_int16_t	SecType;			// Missile/Mine
	u_int16_t	Type;				// which type of bullet am I
	u_int16_t	MoveType;			// Movement Type ( Straight/Homeing )
	u_int16_t	State;				// which State of bullet am I
	u_int16_t	OwnerType;			// Owner Type
	u_int16_t	Owner;				// who fired me...
	u_int16_t	ID;					// Id from that ship..
	int8_t	Weapon;				// Weapon that fired me..
	bool	Lensflare;			// Do Lensflare
	float	LifeCount;			// how long have i lived.....
	float	LifeSpan;			// how long can i live
	float	SpeedInc;			// how fast do I accelerate...
	float	SpeedWanted;		// how fast do I want to move...
	float	Speed;				// how fast do I move
	float	DropCount;			// Count to drop for..
	VECTOR	DropVector;			// Drop Direction
	float	Size;				// how big is my collide sphere
	VECTOR	Pos;				// where am I
	VECTOR	Offset;
	VECTOR	StartPos;			// where was i when initialised
	VECTOR	StartDir;			// where was i looking when initialised
	MATRIX	StartMat;		
	VECTOR	ColStart;			// where I was when started going straight.
	float	ColDist;			// Length from Collision to ColStart.
	u_int16_t	ColFlag;			// have I checked if im going to collide with something
	u_int16_t	ColGroup;			// Collision Group
	VERT	ColPoint;			// where am I going to collide
	NORMAL	ColPointNormal;		// if I reflect use this when I do
	u_int16_t	GroupImIn;			// which group am I in...
	u_int16_t	ModelNum;			// if Im a 3D model then I need a model description...
	u_int16_t	ModelIndex;			// if Im a 3D model then I need a model description...
	MATRIX	Mat;				// and a Matrix for when Im displayed...
	u_int16_t	fmpoly;				// if Im 2D then I need a Face Me Poly..
	int16_t	numfmpolys;			// Number of attached fmpolys
	u_int16_t	poly;				// first attached poly
	int16_t	numpolys;			// Number of attached polys
	float	xsize;				// x size..
	float	ysize;				// y size..
	u_int16_t	light;				// if I produce Light then I Need A Light
	float	lightsize;			// how big is the light
	float	r;					// what color does it emit....
	float	g;					// what color does it emit....
	float	b;					// what color does it emit....
	float	TurnSpeed;			// how quickly can I turn
	float	ViewCone;			// Visibility cone
	u_int16_t	TargetType;			// Ship i'm going for
	u_int16_t	Target;				// Ship i'm going for
	QUAT	DirQuat;			// Direction Quaternion
	VECTOR	DirVector;			// Forward Vector
	VECTOR	UpVector;			// Up Vector
	float	Shield;				// Shield
	float	Damage;				// Damage to do.
	u_int16_t	Ammo;				// Ammo
	int16_t	NumBounces;			// Bounce Count
	int16_t	NumOldPos;			// Number of old positions stored.
	float	RetractPos;			// retract pos
	float	ColRadius;			// how big is my collide sphere
	float	Interval;			// Scatter Capture Interval/smoke trail interval
	float	Time;				// Interval left
	OLDPOS	OldPos[ MAXTENTOLDPOS ];// Old positions array;
	float	FramelagAddition;

} SECONDARYWEAPONBULLET;		// sizes and colours of 2d polys and Lights are set up depoending on type...

typedef	struct ENTRY {

	struct ENTRY	*	Prev;	// Previous Entry ( Used / Free )
	struct ENTRY	*	Next;	// Next Entry ( Used / Free )

} ENTRY;

/*===================================================================
	Prototypes
===================================================================*/
void	InitSecBulls(void);
u_int16_t	FindFreeSecBull(void);
void	KillUsedSecBull( u_int16_t i );
void	FireSecondary(void);
void	ProcessSecondaryBullets( void );
void	CleanUpSecBull( u_int16_t i );
u_int16_t	InitOneSecBull( u_int16_t OwnerType, u_int16_t Owner, u_int16_t BulletID, u_int16_t Group,
					    VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * UpVector,
						VECTOR * DropDir, int8_t Weapon, bool NetFlag );
bool	PointToSphere(	VERT * TargPos, float TargRadius, VERT * BulPos );
void	CreateExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateSmallExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateSingleExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateTrail( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateBloodTrail( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateBloodSplat( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateGravgonTrail( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateSolarisTrail( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNewTrail( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateGravgonField( u_int16_t i );
void	CreateShockwave( u_int16_t OwnerType, u_int16_t Owner, VECTOR * Pos, u_int16_t Group, float ShockwaveSize, BYTE Weapon );
void	ExplodeSecondary( VECTOR * Pos, u_int16_t Group, u_int16_t OwnerType, u_int16_t Owner, u_int16_t ID, float ShockwaveSize );
bool	ProcessMines( u_int16_t i );
bool	ProcessMissiles( u_int16_t i, u_int16_t * NextMissile );
void	KillOwnersSecBulls( u_int16_t Owner );
void	ScatterWeapons( VECTOR * Dir, int16_t MaxPickupsAllowed );
int16_t	GenerateSecondaryPickups( int16_t NumAllowed );
int16_t	GetCurSecAmmo( void );
void	SecondarySelectControl( void );
u_int16_t	GetBestMine( void );
void	DropMine( void );
u_int16_t	TargetClosestShip( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int16_t MeType, u_int16_t Me, float ViewConeCos, float * ClosestCos );
bool	StillGotTarget( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int16_t TargetType, u_int16_t Target, float ViewConeCos );
void	CreateScatterTrail( u_int16_t i );
void	CreateBigFlash( VECTOR * Pos, u_int16_t Group, float Size );
void	CreateMuzzleFlash( VECTOR * Pos, u_int16_t Group, float Size );
void	FireSecondaryWeapons( u_int8_t Ship );
void	DestroySecondary( u_int16_t i, VECTOR * Int_Point );
void	GenMineList( u_int16_t Ship, SHORTMINE * MineSlots, BYTE * NumMines, BYTE Section );
void	RegenMineList( SHORTMINE * Slots, BYTE Num );
void	CopyMines( u_int16_t Player );
void	RegenerateQuedMines( void );
void	KillOwnerIDSecBulls( u_int16_t OwnerType, u_int16_t Owner, u_int16_t ID );
bool	CheckForMines( VECTOR * Pos );
void	SyncMines( void );
void	DoDamagedEffects( u_int16_t i );

void	GetFinalMatrix( MATRIX * FinalMatrix );
void	TargetingComputer( u_int16_t ClosestShip, u_int16_t ClosestEnemy );
void	HeatseakerTarget( u_int16_t ClosestShip, u_int16_t ClosestEnemy );
void	HUDNames( void );
void	DispHUDNames( void );
void	DropSecondary( void );
void	CreateTitanStarShrapnel( u_int16_t i, VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateSmokeRing( VECTOR * Pos, VECTOR * DirVector, VECTOR * UpVector, u_int16_t Group );
u_int16_t	TargetClosestEnemy( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int16_t MeType, u_int16_t Me, float ViewConeCos, float * ClosestCos );
u_int16_t	TargetClosestSecondary( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int16_t MeType, u_int16_t Me, float ViewConeCos, float * ClosestCos );
bool	CheckProximityToShips( VECTOR * Pos, u_int16_t Group, float Radius, u_int16_t OwnerType, u_int16_t Owner );
u_int16_t	GetClosestEnemy( VECTOR * Pos, u_int16_t Group, float * Radius, u_int16_t OwnerType, u_int16_t Owner );
void	CreateBigExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateSplash( VECTOR * Pos, VECTOR * DirVector, u_int16_t Group );

void	SetupSecBullGroups( void );
void	AddSecBullToGroup( u_int16_t i, u_int16_t Group );
void	RemoveSecBullFromGroup( u_int16_t i, u_int16_t Group );
void	MoveSecBullToGroup( u_int16_t i, u_int16_t OldGroup, u_int16_t NewGroup );
void	DispInGameText( VECTOR * Pos, int8_t * Text );
void	CreateFireBall( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateTentacle( u_int16_t i );
u_int16_t	GetSecondaryBulletID( u_int16_t OwnerType, u_int16_t Owner );
FILE * SaveSecBulls( FILE * fp );
FILE * LoadSecBulls( FILE * fp );
u_int8_t GetNextValidSeconadaryWeapon( void );
u_int8_t GetPrevValidSecondaryWeapon( void );
int8_t ConvSecToNormWeapon( int8_t Secondary );
void ToggleSecondaryWeapons( int8_t Secondary );

int16_t HostGenerateSecondaryPickups( u_int16_t Ship, int16_t NumAllowed );

/* Defined in secondary.cpp; shared with the modules that use it. */
extern SECONDARYWEAPONATTRIB SecondaryWeaponAttribs[ TOTALSECONDARYWEAPONS ];
extern SECONDARYWEAPONBULLET SecBulls[ MAXSECONDARYWEAPONBULLETS ];
extern u_int16_t FirstSecBullUsed;
extern SECONDARYWEAPONBULLET * SecBullGroups[ MAXGROUPS ];
extern int16_t SecondaryAmmo[ MAXSECONDARYWEAPONS ];
extern int16_t SecAmmoUsed[ MAXSECONDARYWEAPONS ];
extern u_int16_t GlobalSecBullsID;
extern bool TargetComputerOn;
extern int16_t SecondaryWeaponsGot[ MAXSECONDARYWEAPONS ];
extern int16_t NumSecBullsPerGroup[ MAXGROUPS ];
extern int8_t SecondaryToFireLookup[ MAXSECONDARYWEAPONS ];
extern bool SecondaryWeaponCheat;
extern const char *SecondaryDescription[ MAXSECONDARYWEAPONS ];
extern px_timer_t last_mine_timer;
extern u_int16_t HitBox;
extern u_int16_t TargScrPolys[ 4 ];
extern int8_t * DebugSecStrings[];
extern bool ShowNamesAnyway;
extern bool SecondaryValid[ MAXSECONDARYWEAPONS ];
