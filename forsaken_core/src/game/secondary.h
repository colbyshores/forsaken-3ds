#pragma once

/* secondary.h -- secondary weapons: missiles, mines, their pool and targeting
 *
 * SecondaryWeaponAttribs[TOTALSECONDARYWEAPONS] is indexed by weapon number:
 * the player weapons MUGMISSILE..SPIDERMINE, the spawned PINEMISSILE and
 * TITANSTARSHRAPNEL, then the enemy-only weapons ENEMYSPIRALMISSILE..
 * ENEMYDEPTHCHARGE. SecBulls[] is the live missile/mine pool; its used list
 * starts at FirstSecBullUsed and is walked through Prev, and each entry is
 * also on a per-group list (SecBullGroups[]). InitSecBulls clears it at scene
 * init.
 *
 * Once per frame FireSecondary turns the local player's input into missiles
 * or mines, and ProcessSecondaryBullets runs ProcessMissiles or ProcessMines
 * on each entry by SecType. InitOneSecBull creates every entry and sends it
 * to other players only when NetFlag is set: mines and pine missiles are sent
 * that way, while other machines recreate the local player's missiles in
 * FireSecondaryWeapons from ship updates. Mines in play are snapshotted per
 * joining player (CopyMines / GenMineList) and rebuilt on the joiner
 * (RegenMineList; SyncMines then ages them by the time since the first mine
 * message arrived).
 *
 * The module also owns the HUD targeting box and heat-seeker lock
 * (TargetingComputer, HeatseakerTarget), the on-screen player names, explosion
 * and trail effects, ScatterWeapons (the local ship throws its weapons out as
 * pickups, on death or when hit by a scatter missile) and DoDamagedEffects
 * (smoke from a damaged ship). The ammo and weapons held (in
 * SecondaryWeapons, below) describe the local player only.
 */

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
struct OLDPOS {

	VECTOR		Pos;			// Old Position
	VECTOR		Dir;			// Old Direction
	u_int8_t		R;				// Red
	u_int8_t		G;				// Green
	u_int8_t		B;				// Blue

};

struct SECONDARYWEAPONATTRIB {

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

};

struct SECONDARYWEAPONBULLET {
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

};		// sizes and colours of 2d polys and Lights are set up depoending on type...

struct ENTRY {

	struct ENTRY	*	Prev;	// Previous Entry ( Used / Free )
	struct ENTRY	*	Next;	// Next Entry ( Used / Free )

};

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
void	ExplodeSecondary( u_int16_t OwnerType, u_int16_t Owner, u_int16_t ID, float ShockwaveSize );
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
u_int16_t	TargetClosestEnemy( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, float ViewConeCos, float * ClosestCos );
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


/* Defined in secondary.cpp; shared with the modules that use it. */
extern SECONDARYWEAPONATTRIB SecondaryWeaponAttribs[ TOTALSECONDARYWEAPONS ];
extern SECONDARYWEAPONBULLET SecBulls[ MAXSECONDARYWEAPONBULLETS ];
extern u_int16_t FirstSecBullUsed;
extern SECONDARYWEAPONBULLET * SecBullGroups[ MAXGROUPS ];
extern int8_t SecondaryToFireLookup[ MAXSECONDARYWEAPONS ];
extern const char *SecondaryDescription[ MAXSECONDARYWEAPONS ];
extern bool SecondaryValid[ MAXSECONDARYWEAPONS ];

struct VISISHIP {

	u_int16_t	Ship;
	int16_t	ScreenX;
	int16_t	ScreenY;
	int16_t	InfoX;
	int16_t	InfoY;

};

class QUATLERP;

/* The secondary weapons' state (one instance, Secondaries):
 * - the SecBulls[] free-list head (FirstSecBullFree) and the per-group bullet
 *   counts (NumSecBullsPerGroup), reset with the scene and changed as
 *   missiles and mines are made, die and change group;
 * - the incoming-missile warning (ImTargeted, TargetedDelay, MissileDistance,
 *   MissileNum), set by each homing missile and played by the per-frame
 *   processing;
 * - the local ship's fire delay and mug/solaris launch side (Toggle), changed
 *   as it fires;
 * - the local player's secondary weapons (SecondaryWeaponsGot) and ammo
 *   (SecondaryAmmo), and the ammo owed back to the level as pickups
 *   (SecAmmoUsed);
 * - the targeting computer (TargetComputerOn) and the screen polys of the HUD
 *   targeting box and heat-seeker lock (HitBox, TargScrPolys);
 * - the ID counter for the bullets no ship owns (spot FX), the clock of the
 *   first mine message (last_mine_timer), the per-ship damage smoke timers,
 *   the HUD name positions (each frame), the mine-sync tick rate, and the
 *   secondary pickups a dying ship drops (SecWeaponsGot);
 * - the constant tables only secondary.cpp reads, as static members.
 * The list head, group counts, warning, fire delay, the weapons and ammo,
 * the ID counter and the targeting computer are saved and restored with the
 * game. Other files use the intent members below; the C API forwards to the
 * rest. SecBulls[], FirstSecBullUsed, SecBullGroups[], MinesCopy[] (a data
 * pool) and SecondaryWeaponAttribs[] stay global. */
class SecondaryWeapons {
	public:
	/* The intent members are always_inline so each caller compiles as the
	 * raw global access did.
	 *
	 * The secondary weapons the local player holds and their ammo
	 * (MUGMISSILE..SPIDERMINE): the HUD shows them, a pickup adds ammo and
	 * gives the weapon, RestoreAmmo and GodMode load the whole set. */
	[[gnu::always_inline]] int16_t ammo( int Weapon ) const { return SecondaryAmmo[ Weapon ]; }
	[[gnu::always_inline]] void add_ammo( int Weapon, int16_t Ammo ) { SecondaryAmmo[ Weapon ] += Ammo; }
	[[gnu::always_inline]] void load_ammo( int Weapon, int16_t Ammo ) { SecondaryAmmo[ Weapon ] = Ammo; }
	[[gnu::always_inline]] int16_t got_weapon( int Weapon ) const { return SecondaryWeaponsGot[ Weapon ]; }
	[[gnu::always_inline]] void give_weapon( int Weapon ) { SecondaryWeaponsGot[ Weapon ] = 1; }
	[[gnu::always_inline]] void take_weapon( int Weapon ) { SecondaryWeaponsGot[ Weapon ] = 0; }
	[[gnu::always_inline]] void load_weapon( int Weapon, int16_t Got ) { SecondaryWeaponsGot[ Weapon ] = Got; }

	/* The ammo owed back to the level as pickups: a lost pickup adds to it,
	 * RegenerateQuedPickups pays it back, and a pickup's ammo over the
	 * maximum (or InitAmmoUsed's clear) replaces it. */
	[[gnu::always_inline]] int16_t ammo_owed( int Weapon ) const { return SecAmmoUsed[ Weapon ]; }
	[[gnu::always_inline]] void owe_ammo( int Weapon, int16_t Ammo ) { SecAmmoUsed[ Weapon ] += Ammo; }
	[[gnu::always_inline]] void ammo_regenerated( int Weapon, int16_t Ammo ) { SecAmmoUsed[ Weapon ] -= Ammo; }
	[[gnu::always_inline]] void reset_ammo_owed( int Weapon, int16_t Ammo ) { SecAmmoUsed[ Weapon ] = Ammo; }

	/* The targeting computer: the computer pickup fits it, the F8 key turns
	 * it on and off, and each frame the tracker is drawn while it is on. */
	[[gnu::always_inline]] bool has_targeting_computer() const { return TargetComputerOn; }
	[[gnu::always_inline]] void fit_targeting_computer() { TargetComputerOn = true; }
	[[gnu::always_inline]] void remove_targeting_computer() { TargetComputerOn = false; }

	/* The ID for the next bullet no ship owns (spot FX). */
	[[gnu::always_inline]] u_int16_t next_bullet_id() { return GlobalSecBullsID++; }

	/* The first mine message from the host has arrived: SyncMines ages the
	 * mines by the time since. */
	[[gnu::always_inline]] void mark_first_mine_message() { timer_run( &last_mine_timer ); }

	/* The screen-poly pool was reset: the targeting box and heat-seeker lock
	 * polys are gone. */
	[[gnu::always_inline]] void forget_target_polys() { u_int16_t i; HitBox = INDEX_NONE; for( i = 0; i < 4; i++ ) TargScrPolys[ i ] = INDEX_NONE; }

	/* The C API (InitSecBulls, FireSecondary and the rest) forwards to these,
	 * one forwarder each, and other files and secondary.cpp itself call the
	 * C API. They are always_inline so each forwarder keeps the body, with no
	 * this pointer, as when the class was private to secondary.cpp. */
	[[gnu::always_inline]] inline void init();
	[[gnu::always_inline]] inline u_int16_t find_free();
	[[gnu::always_inline]] inline void kill_used( u_int16_t i );
	[[gnu::always_inline]] inline void fire();
	[[gnu::always_inline]] inline void process();
	[[gnu::always_inline]] inline u_int16_t init_one( u_int16_t OwnerType, u_int16_t Owner, u_int16_t BulletID, u_int16_t Group,
						VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * UpVector,
						VECTOR * DropDir, int8_t Weapon, bool NetFlag );
	/* process_missile (each missile, each frame) stays out of line. Inlined
	 * into the ProcessMissiles forwarder, GCC sees the constant this before
	 * its early passes and the body grows by 8 instructions (2801 -> 2809);
	 * out of line it is 2802, as near the phase-1 clone as external linkage
	 * allows. */
	[[gnu::noinline]] bool process_missile( u_int16_t i, u_int16_t * NextMissile );
	[[gnu::always_inline]] inline void explode( u_int16_t OwnerType, u_int16_t Owner, u_int16_t ID, float ShockwaveSize );
	[[gnu::always_inline]] inline void kill_owners( u_int16_t Owner );
	[[gnu::always_inline]] inline void scatter_weapons( VECTOR * Dir, int16_t MaxPickupsAllowed );
	[[gnu::always_inline]] inline int16_t generate_pickups( int16_t NumAllowed );
	[[gnu::always_inline]] inline int16_t cur_ammo();
	[[gnu::always_inline]] inline void select_control();
	[[gnu::always_inline]] inline u_int16_t best_mine();
	[[gnu::always_inline]] inline void drop_mine();
	[[gnu::always_inline]] inline void targeting_computer( u_int16_t ClosestShip, u_int16_t ClosestEnemy );
	[[gnu::always_inline]] inline void heatseeker_target( u_int16_t ClosestShip, u_int16_t ClosestEnemy );
	[[gnu::always_inline]] inline void hud_names();
	[[gnu::always_inline]] inline void disp_hud_names();
	[[gnu::always_inline]] inline void create_scatter_trail( u_int16_t i );
	[[gnu::always_inline]] inline void create_tentacle( u_int16_t i );
	[[gnu::always_inline]] inline void gen_mine_list( u_int16_t Ship, SHORTMINE * MineSlots, BYTE * NumMines, BYTE Section );
	[[gnu::always_inline]] inline void regen_mine_list( SHORTMINE * Slots, BYTE Num );
	[[gnu::always_inline]] inline void sync_mines();
	[[gnu::always_inline]] inline void copy_mines( u_int16_t Player );
	[[gnu::always_inline]] inline void kill_owner_id( u_int16_t OwnerType, u_int16_t Owner, u_int16_t ID );
	[[gnu::always_inline]] inline void damaged_effects( u_int16_t i );
	[[gnu::always_inline]] inline void drop_secondary();
	[[gnu::always_inline]] inline void setup_groups();
	[[gnu::always_inline]] inline void add_to_group( u_int16_t i, u_int16_t Group );
	[[gnu::always_inline]] inline void remove_from_group( u_int16_t i, u_int16_t Group );
	[[gnu::always_inline]] inline u_int16_t bullet_id( u_int16_t OwnerType, u_int16_t Owner );
	[[gnu::always_inline]] inline FILE * save( FILE * fp );
	[[gnu::always_inline]] inline FILE * restore( FILE * fp );
	[[gnu::always_inline]] inline u_int8_t next_valid_weapon();
	[[gnu::always_inline]] inline u_int8_t prev_valid_weapon();

	private:
	[[gnu::always_inline]] inline void FireSecondaryMugMissile( VECTOR & UpVector, VECTOR & Pos, VECTOR & Dir, VECTOR & DropVector );
	[[gnu::always_inline]] inline void FireSecondarySolarisMissile( VECTOR & UpVector, VECTOR & Pos, VECTOR & Dir, VECTOR & DropVector );
	[[gnu::always_inline]] inline void FireSecondarySelect( int16_t & OnceOnlyFlag );
	[[gnu::always_inline]] inline void MissileHoming( u_int16_t & i, QUATLERP & qlerp, VECTOR & NormVector, float & Cos, float & Angle, float & DistToTarget, float & NewFramelag );
	[[gnu::always_inline]] inline bool MissileStateUpdate( u_int16_t & i, QUATLERP & qlerp, VECTOR & DirVector, VECTOR & TempVector, VECTOR & NormVector, float & Speed, float & Cos, float & Angle, float & DistToTarget, u_int16_t & Light, MATRIX & UpMatrix, MATRIX & InvUpMatrix, QUAT & TempQuat, float & ClosestCos, u_int16_t & Ship, u_int16_t & Enemy, u_int16_t & Secondary, u_int16_t & OldGroup, VECTOR & Rotation, VECTOR & UpVector, VECTOR & TempPoint1, VECTOR & TempPoint2, float & NewFramelag );

	/* The HUD name table first (the per-frame HUD stores index it), then the
	 * small scalars, so every halfword access keeps an ldrh/strh immediate
	 * (TargetComputerOn and HitBox fill the padding after ImTargeted); the
	 * larger arrays after them, and the cold mine clock last. */
	int16_t   NumVisibleShips = 0;
	VISISHIP  VisibleShipsPos[ MAX_PLAYERS ] = {};
	u_int16_t FirstSecBullFree = 0;
	u_int16_t MissileNum = 0;
	int16_t   Toggle = 0;
	bool      ImTargeted = false;
	bool      TargetComputerOn = false;
	u_int16_t HitBox = INDEX_NONE;
	float     TargetedDelay = 0.0F;
	float     MissileDistance = 0.0F;
	float     SecondaryFireDelay = 0.0F;
	float     ShipSmokeInterval = 1.0F;
	int16_t   NumSecBullsPerGroup[ MAXGROUPS ] = {};
	float     ShipSmokeTimes[ MAX_PLAYERS ] = {};
	float     ticksperframe = 14.0F;
	int8_t    SecWeaponsGot[ 256 ] = {};	/* MAXPICKUPS (checked in secondary.cpp) */
	px_timer_t last_mine_timer = {};

	/* Static, so each stays its own symbol:
	 * - the HUD, the pickups, GodMode and the per-frame targeting index the
	 *   ammo, weapon and target-poly tables by weapon or by poly;
	 * - GlobalSecBullsID: as an instance member (at +768) the per-mine
	 *   ProcessMines lost registers around its GetSecondaryBulletID call
	 *   (753 -> 756 instructions). */
	static int16_t   SecondaryAmmo[ MAXSECONDARYWEAPONS ];
	static int16_t   SecondaryWeaponsGot[ MAXSECONDARYWEAPONS ];
	static int16_t   SecAmmoUsed[ MAXSECONDARYWEAPONS ];
	static u_int16_t TargScrPolys[ 4 ];
	static u_int16_t GlobalSecBullsID;
	static int8_t *  DebugSecStrings[];
	static int16_t   SecondarySfxLookup[ MAXSECONDARYWEAPONS ];
	static int       TeamColours[ 4 ];
	static VECTOR    OutlineOffsets[ NUMSCATTERPOINTS ];
	static VECTOR    TentOutlineOffsets[ NUMSCATTERPOINTS ];
};

extern SecondaryWeapons Secondaries;
