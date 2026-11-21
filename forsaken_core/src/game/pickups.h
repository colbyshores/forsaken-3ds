#pragma once

/* pickups.h -- pickups: types, the live pickup pool and regeneration points
 *
 * PickupAttribs[] (pickups.cpp) gives each PICKUP_* type its model and light.
 * Pickups[MAXPICKUPS] is the live pool, reset by InitPickups at scene init;
 * the used list (whose head is private to PickupPool, below) is walked
 * through Prev, and each pickup is also on a per-group list (PickupGroups[]). LoadPickupsPositions
 * reads the level .PIC into RegenPoints[] (malloc'd per level, plus one copy
 * per player in RegenSlotsCopy[]) and seeds the level's pickups.
 *
 * Once per frame ProcessPickups moves and animates them and RegeneratePickups
 * (skipped during demo playback) counts down timed regen points; only the
 * host creates regenerated pickups. CheckPickup, called from the local ship's
 * movement, collects any pickup the ship's path passed through.
 *
 * Pickups are identified across the network by (Owner, ID). KillPickup on a
 * pair not yet present queues it in a failed-kill list that ProcessFailedKills
 * retries for FAILEDKILLTIMEOUT frames. The Copy* / Gen*List / Regen*List
 * functions snapshot and rebuild pickups and regen slots for a joining player.
 */

#include "game/main.h"
#include "game/constants.h"
/*===================================================================
	Defines
===================================================================*/

#ifdef DEBUG_ON
#define DEBUG_PICKUPS			1
#else
#define DEBUG_PICKUPS			0
#endif

constexpr int PIC_VERSION_NUMBER = 1;

constexpr int MAXPICKUPS = 256;
/* Bumped 46 -> 128 to fit Remaster N64 pickup IDs at slots 100-105
 * (StablizerCrystal + ComputerCapsule + 4 BlackHoleGun parts). Slots
 * 46-99 + 106-127 stay zero-initialised. Cost: 82 extra entries in
 * PickupAttribs[] and the per-type arrays (counters, valid flags).
 *
 * Counterpart to the MAX_ENEMY_TYPES bump in enemies.h — same minimal-
 * correct pattern: extend the cap, add enum entries with explicit
 * values, populate the new slots at runtime via template-copy from a
 * tonally-similar 1998 pickup (light / glow), with the model swapped
 * for the Remaster mesh when PreLoadPickups found it. The pickup
 * spawns into a real slot rather than being substituted to
 * PICKUP_GeneralAmmo. */
constexpr int MAXPICKUPTYPES = 128;
constexpr auto PICKUP_RADIUS = ( 562.5F * GLOBAL_SCALE );
constexpr auto PICKUPLIGHT_RADIUS = ( 1536.0F * GLOBAL_SCALE );
constexpr auto PICKUP_SPEED = ( 40.0F * GLOBAL_SCALE );
constexpr auto PICKUP_DEC = ( 0.20F * GLOBAL_SCALE );
constexpr float COLOUR_SCALE = 0.5F;
constexpr auto PICKUP_MODEL_RADIUS = ( 310.0F * GLOBAL_SCALE );

constexpr int MPU_TYPE_KILL = 0;
constexpr int MPU_TYPE_INIT = 1;

constexpr auto INVULNERABILITY_TIME = ( 30.0F * 60.0F );

constexpr int MAXFAILEDKILLS = 100;
constexpr auto FAILEDKILLTIMEOUT = ( 15.0F * 60.0F );

enum {							// Pickup Mode

	PICKUPMODE_Appear,
	PICKUPMODE_Normal,
	PICKUPMODE_Disappear,
};

enum {							// Pickup Kill Modes

	PICKUPKILL_Immediate,
	PICKUPKILL_Disappear,
};

enum {

	PICKUP_Trojax,				// 0 Primary Weapons
	PICKUP_Pyrolite,			// 1
	PICKUP_Transpulse,			// 2
	PICKUP_SussGun,				// 3
	PICKUP_Laser,				// 4
	PICKUP_Mug,					// 5 Secondary weapons
	PICKUP_Mugs,				// 6
	PICKUP_Heatseaker,			// 7
	PICKUP_HeatseakerPickup,	// 8
	PICKUP_Thief,				// 9
	PICKUP_Scatter,				// 10
	PICKUP_Gravgon,				// 11
	PICKUP_Launcher,			// 12
	PICKUP_TitanStar,			// 13
	PICKUP_PurgePickup,			// 14
	PICKUP_PinePickup,			// 15
	PICKUP_QuantumPickup,		// 16
	PICKUP_SpiderPod,			// 17
	PICKUP_Parasite,			// 18
	PICKUP_Flare,				// 19
	PICKUP_GeneralAmmo,			// 20 Ammo
	PICKUP_PyroliteAmmo,		// 21
	PICKUP_SussGunAmmo,			// 22
	PICKUP_PowerPod,			// 23 Extras
	PICKUP_Shield,				// 24
	PICKUP_Inv,					// 25
	PICKUP_ExtraLife,			// 26
	PICKUP_Computer,			// 27
	PICKUP_Smoke,				// 28
	PICKUP_Nitro,				// 29
	PICKUP_Goggles,				// 30
	PICKUP_Gold,				// 31
	PICKUP_Mantle,				// 32
	PICKUP_Crystal,				// 33
	PICKUP_Orb,					// 34
	PICKUP_GoldenPowerPod,		// 35
	PICKUP_DNA,					// 36
	PICKUP_SkeletonKey,			// 37
	PICKUP_Bomb,				// 38
	PICKUP_GoldFigure,			// 39
	PICKUP_Flag,				// 40
	PICKUP_Bounty,				// 41
	PICKUP_Flag1,				// 42
	PICKUP_Flag2,				// 43
	PICKUP_Flag3,				// 44
	PICKUP_Flag4,				// 45

	/* Remaster N64 pickup IDs imported from defs/n64Pickups.txt.
	 * Slots 100-105 populated at runtime in LoadPickupsPositions
	 * by template-copying from tonally-similar 1998 pickups. */
	PICKUP_StablizerCrystal      = 100,	// 100  (template: PICKUP_Crystal)
	PICKUP_ComputerCapsule       = 101,	// 101  (template: PICKUP_Computer)
	PICKUP_BlackHoleGunPart1     = 102,	// 102  (template: PICKUP_PowerPod)
	PICKUP_BlackHoleGunPart2     = 103,	// 103  (template: PICKUP_PowerPod)
	PICKUP_BlackHoleGunPart3     = 104,	// 104  (template: PICKUP_PowerPod)
	PICKUP_BlackHoleGunPart4     = 105,	// 105  (template: PICKUP_PowerPod)
};

/*===================================================================
	Regeneration point defines
===================================================================*/
enum {							// Pickup Regeneration Status

	PU_REGENSTAT_Free,
	PU_REGENSTAT_Used,
	PU_REGENSTAT_TimeGen,
	PU_REGENSTAT_TrigGen,
};

enum {

	PU_GENTYPE_Initialised,		// Initialised immediatly
	PU_GENTYPE_Time,			// Initialised after time
	PU_GENTYPE_Trigger,			// Initialised by trigger
};

enum {

	PU_REGENTYPE_Random,		// Random each time
	PU_REGENTYPE_Const,			// Same each time
	PU_REGENTYPE_Onceonly,		// Once only ( Never used again )
};


/*===================================================================
	Structures
===================================================================*/
struct PICKUPATTRIB {

	u_int16_t ModelType;	// if 3D model which one....
	u_int16_t Light;			// do I produce Light
	float	Lightsize;		// how big is the light
	float	R;					// what color does it emit....
	float	G;					// what color does it emit....
	float	B;					// what color does it emit....

};

struct PICKUP {

	u_int16_t	Next;				// link list.....
	u_int16_t	Prev;				// link list.....
	struct PICKUP * NextInGroup;		// Next in same group ....
	struct PICKUP *	PrevInGroup;	// Previous in same group ....
	u_int16_t	Index;
	u_int16_t	Type;				// which type of bullet am I
	u_int16_t	Owner;			// Owner ID
	u_int16_t	ID;				// ID
	int16_t	Mode;				// Mode Pickup is currently in ...
	float	LifeCount;			// How long do i live ...
	float	PickupCount;		// Cannot pickup unless zero...
	VECTOR	Rot;				// Rotation..
	VECTOR	Pos;				// where am I
	VECTOR	Dir;				// where am I looking
	float	Speed;				// how fast am I moving...
	float	ExternalSpeed;	// Speed created by external force
	u_int16_t	Group;			// which group am I in...
	u_int16_t	ModelType;		// if Im a 3D model then I need a model type
	u_int16_t	ModelNum;		// Index into models...
	MATRIX	Mat;				// and a Matrix for when Im displayed...
	u_int16_t	Fmpoly;			// if Im 2D then I need a Face Me Poly..
	float	Xsize;				// x size..
	float	Ysize;				// y size..
	u_int16_t	Light;				// if I produce Light then I Need A Light
	float	Lightsize;			// how big is the light
	float	R;						// what color does it emit....
	float	G;						// what color does it emit....
	float	B;						// what color does it emit....
	QUAT	DirQuat;				// Direction Quaternion
	VECTOR	DirVector;		// Forward Vector
	VECTOR	UpVector;		// Up Vector
	int16_t	RegenSlot;			// Slot from original pickup file ( -1 anywhere )
	u_int16_t	TriggerMod;		// Trigger Mod pointer index
	TRIGGERMOD * TriggerModPtr;	// Trigger Mod Array

	VECTOR	ColStart;			// where I was when started going straight.
	float	ColDist;			// Length from Collision to ColStart.
	u_int16_t	ColFlag;			// have I checked if im going to collide with something
	u_int16_t	ColGroup;			// Collision Group
	VERT	ColPoint;			// where am I going to collide
	NORMAL	ColPointNormal;		// if I reflect use this when I do
	bool	CouldNotPickup;		// Could not pickup last time collided

	float	PickupTime;			// Life count so far for pickup

};

struct REGENPOINT {

	u_int16_t	GenType;			// Generation Type
	u_int16_t	RegenType;			// Regeneration Type
	float	GenDelay;			// Generation Delay ( 60hz )
	float	Lifespan;			// Lifespan of pickup ( 60hz )
	float	Wait;				// Global count
	int16_t	Status;				// Status of slot
	int16_t	Group;				// Group to generate pickup
	VECTOR	Pos;				// Position to generate pickup
	u_int16_t	Type;				// Type of pickup
	u_int16_t	TriggerMod;			// Trigger Mod Index
	u_int16_t	PickupIndex;		// Pickup Index
	u_int16_t	PickupID;			// Pickup ID

};

struct QUEDPICKUP {

	int16_t	Type;
	struct	QUEDPICKUP	*	PrevUsed;
	struct	QUEDPICKUP	*	NextUsed;
	struct	QUEDPICKUP	*	PrevFree;
	struct	QUEDPICKUP	*	NextFree;

};

struct FAILEDKILL {

	int16_t	Next;				// link list.....
	int16_t	Prev;				// link list.....
	float	Life;
	u_int16_t	Owner;
	u_int16_t	ID;
	int16_t	Style;
	u_int16_t	NewOwner;

};

/*===================================================================
	Prototypes
===================================================================*/
void	InitPickups( void );
u_int16_t	FindFreePickup( void );
void	KillUsedPickup( u_int16_t i );
void	ProcessPickups( void );
void	CleanUpPickup( u_int16_t i );
u_int16_t	InitOnePickup( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type,
					   u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod );
u_int16_t	InitJoinPickup( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type,
					   u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod );
void	KillPickup( u_int16_t Owner, u_int16_t ID, int16_t Style );
bool	CollectPickup( u_int16_t i );
void	CheckPickup( void );
void	SavePickupsPositions( void );
bool	LoadPickupsPositions( void );
void	RegeneratePickups( void );
void	RegenerateQuedPickups( void );
bool	RegeneratePickup( u_int16_t Type );
void	GenPickupList( u_int16_t Ship, SHORTPICKUP * PickupSlots, BYTE * NumPickups, BYTE Section );
void	RegenPickupList( SHORTPICKUP * Slots, BYTE Num );
void	GenRegenSlotList( u_int16_t Ship, SHORTREGENSLOT * RegenSlots, BYTE * NumRegenSlots, BYTE Section );
void	RegenRegenSlotList( SHORTREGENSLOT * Slots, BYTE Num );
void	KillAllPickups( void );
void	NewSecondaryWeapon( int16_t Weapon );
void	NewPrimaryWeapon( int16_t Weapon );
void	CopyPickups( u_int16_t Player );
void	AddPickupToRegen( u_int16_t Type );
u_int16_t	InitSlotPickup( u_int16_t Slot );
void	CopyRegenSlots( u_int16_t Player );
u_int16_t	FindClosestShip( void );
void	SetupPickupGroups( void );
void	AddPickupToGroup( u_int16_t i, u_int16_t Group );
void	RemovePickupFromGroup( u_int16_t i, u_int16_t Group );
void	MovePickupToGroup( u_int16_t i, u_int16_t OldGroup, u_int16_t NewGroup );
void	ClearPickupsGot( void );
bool	FilterPickup( u_int16_t PickupType );
void	PackPickupInfo( u_int32_t *packed );
void	UnpackPickupInfo( u_int32_t *packed );
FILE * SaveAllPickups( FILE * fp );
FILE * LoadAllPickups( FILE * fp );
void KillAllPickupsOfType( u_int16_t Type, int16_t Style );

void PickupModelValid( void );

void InitFailedKillSlots( void );
bool AddFailedKillToQue( u_int16_t Owner, u_int16_t ID, int16_t Style, u_int16_t NewOwner);
int16_t FindFreeFailedKillSlot( void );
void ReleaseFailedKillSlot( int16_t i );
void ProcessFailedKills( void );
u_int16_t FindPickup( u_int16_t Owner, u_int16_t ID );

/* Defined in pickups.cpp; shared with the modules that use it. */
bool PreLoadPickups( void );
extern PICKUP * PickupGroups[ MAXGROUPS ];
extern u_int16_t CopyOfSeed1;
extern u_int16_t CopyOfSeed2;
extern bool RandomPickups;
extern int8_t TeamFlagPickup[ MAX_TEAMS ];
extern int16_t NumPickupType[ MAXPICKUPTYPES ];
extern PICKUP Pickups[ MAXPICKUPS ];
extern int16_t NumPickupsPerGroup[ MAXGROUPS ];
extern REGENPOINT * RegenPoints;
extern int16_t NumRegenPoints;
extern REGENPOINT * RegenSlotsCopy[ MAX_PLAYERS ];
extern bool PickupValid[ MAXPICKUPTYPES ];
extern bool MyPickupValid[ MAXPICKUPTYPES ];
extern bool MyRandomPickups;
extern int16_t NumPrimaryPickups;
extern char CTFMessage[ MAXTEXTMSG ];
void SfxForCollectPickup( u_int16_t Owner, u_int16_t ID );
extern const char *Messages[];
void InitValidPickups();

/* The pickup pool's bookkeeping and the pickups owed back to the level (one
 * instance, PickupState):
 * - the used and free list heads through Pickups[], changed whenever a
 *   pickup is placed or killed and walked by the per-frame processing;
 * - the counts still to regenerate (primaries, power pods, golden power
 *   pods, stealth mantles, invulnerabilities, orbit pulsars, flags and the
 *   bounty), raised when one leaves play and drained by the per-frame
 *   regeneration, and the delay that holds that drain off after a drop;
 * - the per-type caps the level's .PIC sets (MaxPickupType), loaded with
 *   the level and lowered when a once-only pickup goes;
 * - what the local player carries (PickupsGot: keys, idols, flags, the
 *   bounty), the gold bars found and whether the running invulnerability
 *   came from a pickup;
 * - in the Remaster, the model slots PreLoadPickups gives the N64 meshes.
 * Other files use the intent members below; the C API forwards to the rest.
 * Pickups[] and its per-group and per-type counts stay global with it. */
class PickupPool {
	public:
	/* The intent members are always_inline so each caller compiles as the
	 * raw global access did. As plain inline, the x86 -O3 build inlined
	 * lose() later and then fully unrolled ProcessShips' four-team CTF loop.
	 *
	 * What the local player carries, by pickup type: doors and bgobjects
	 * test for a key, spend a skeleton key or idol, and the flag and bounty
	 * games drop what was carried. */
	[[gnu::always_inline]] int16_t carrying( int Type ) const { return PickupsGot[ Type ]; }
	[[gnu::always_inline]] void carry( int Type ) { PickupsGot[ Type ]++; }
	[[gnu::always_inline]] void use_up( int Type ) { PickupsGot[ Type ]--; }
	[[gnu::always_inline]] void lose( int Type ) { PickupsGot[ Type ] = 0; }

	/* A pickup of this kind left play without being dropped, so the
	 * regeneration owes the level another one. */
	[[gnu::always_inline]] void owe_golden_pod() { NumSuperNashrams++; }
	[[gnu::always_inline]] void owe_stealth() { NumStealths++; }
	[[gnu::always_inline]] void owe_invul() { NumInvuls++; }
	[[gnu::always_inline]] void owe_orb() { NumOrbs++; }
	[[gnu::always_inline]] void owe_flag() { FlagsToGenerate++; }
	/* Hold the regeneration off for this many frames (after a drop). */
	[[gnu::always_inline]] void delay_regen( float Frames ) { RegenDelay = Frames; }

	/* Whether the running invulnerability came from a pickup (and so is
	 * owed back when it ends early). */
	[[gnu::always_inline]] bool invul_from_pickup() const { return PickupInvulnerability; }
	[[gnu::always_inline]] void start_pickup_invul() { PickupInvulnerability = true; }
	[[gnu::always_inline]] void end_pickup_invul() { PickupInvulnerability = false; }

	/* Gold bars found this game (single player). */
	[[gnu::always_inline]] int16_t gold_bars() const { return NumGoldBars; }
	[[gnu::always_inline]] void add_gold_bar() { NumGoldBars ++; }
	[[gnu::always_inline]] void reset_gold_bars() { NumGoldBars = 0; }

	/* The C API (InitPickups, ProcessPickups and the rest) forwards to
	 * these, one forwarder each, and other files call the C API. They are
	 * always_inline so each forwarder keeps the body, with no this pointer,
	 * as when the class was private to pickups.cpp. */
	[[gnu::always_inline]] inline void init();
	[[gnu::always_inline]] inline void clear_got();
	[[gnu::always_inline]] inline void kill( u_int16_t Owner, u_int16_t ID, int16_t Style );
	[[gnu::always_inline]] inline u_int16_t find_free();
	[[gnu::always_inline]] inline void kill_used( u_int16_t i );
	[[gnu::always_inline]] inline u_int16_t init_one( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod );
	[[gnu::always_inline]] inline void process();
	[[gnu::always_inline]] inline void save_positions();
#ifdef EDITION_REMASTER
	[[gnu::always_inline]] inline bool preload();
#endif
	[[gnu::always_inline]] inline bool load_positions();
	[[gnu::always_inline]] inline void regenerate();
	[[gnu::always_inline]] inline void kill_all();
	[[gnu::always_inline]] inline void copy( u_int16_t Player );
	[[gnu::always_inline]] inline void add_to_regen( u_int16_t Type );
	[[gnu::always_inline]] inline void sfx_for_collect( u_int16_t Owner, u_int16_t ID );
	[[gnu::always_inline]] inline FILE * save( FILE * fp );
	[[gnu::always_inline]] inline FILE * restore( FILE * fp );
	[[gnu::always_inline]] inline void kill_all_of_type( u_int16_t Type, int16_t Style );
	[[gnu::always_inline]] inline void model_valid();
	[[gnu::always_inline]] inline u_int16_t find( u_int16_t Owner, u_int16_t ID );

	private:
#ifdef EDITION_REMASTER
	[[gnu::always_inline]] inline void LoadPickupsN64Templates();
#endif
	[[gnu::always_inline]] inline void LoadPickupsSetupRegenPoints();
	[[gnu::always_inline]] inline void LoadPickupsSetMinimums();

	/* The list heads and the counts the per-frame regeneration reads come
	 * first, within ldrh range of the object's base. MaxPickupType stays at
	 * an 8-byte offset, so its level-load fill keeps its strd; the gold bars
	 * and the invulnerability flag (ldrb reaches further) follow it. */
	u_int16_t FirstPickupUsed = 0;
	u_int16_t FirstPickupFree = 0;
	int16_t   NumPowerPods = 0;
	int16_t   NumPrimWeapons[ MAXPRIMARYWEAPONS ] = {};
	int       BountyToGenerate = 0;
	float     RegenDelay = 0.0F;
	int       FlagsToGenerate = 0;
	int16_t   NumSuperNashrams = 0;
	int16_t   NumStealths = 0;
	int16_t   NumInvuls = 0;
	int16_t   NumOrbs = 0;
	int16_t   MaxPickupType[ MAXPICKUPTYPES ] = {};
	int16_t   NumGoldBars = 0;
	bool      PickupInvulnerability = false;
	/* Static, so it stays its own symbol: as an instance member the other
	 * files' indexed stores (flag and bounty drops, door keys) lose their
	 * register-offset strh to an add from the object's base. */
	static int16_t PickupsGot[ MAXPICKUPTYPES ];
#ifdef EDITION_REMASTER
	/* Slot indices (into ModelNames[]) where PreLoadPickups registers each
	 * N64 pickup mesh. PickupAttribs[].ModelType is a ModelNames[] index, so
	 * once the slots are reserved we point the new pickup IDs at them. -1
	 * means the slot wasn't allocated (mesh file missing, or PreLoadPickups
	 * has not run). */
	int16_t   s_n64_pickup_model_idx[5] = { -1, -1, -1, -1, -1 };
#endif
};

extern PickupPool PickupState;
