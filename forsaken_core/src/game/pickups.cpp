/* pickups.cpp -- pickup pool, collection, regeneration points; see pickups.h. */
#include <stdio.h>

#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"

#include "rendering/2dtextures.h"
#include "data/mload.h"

#include "game/primary.h"
#include "game/secondary.h"

#include <math.h>
#include "game/ships.h"
#include "data/mload.h"
#include "world/collision.h"
#include "rendering/lights.h"
#include "rendering/2dpolys.h"
#include "math/quat.h"
#include "game/models.h"
#include "data/file.h"
#include "math/sphere.h"
#include "input/controls.h"
#include "world/visi.h"
#include "audio/sfx.h"

#include "game/triggers.h"
#include "game/pickups.h"
#include "rendering/lines.h"
#include "rendering/text.h"
#include "rendering/screenpolys.h"

#include "game/goal.h"
#include "ui/local.h"
#include "util/util.h"
#include "data/config.h"
#include "game/oct2.h"
#include "ui/title.h"

/*===================================================================
	Defines
===================================================================*/



/*===================================================================
	Externs
===================================================================*/

/*===================================================================
	Globals
===================================================================*/
bool			RandomPickups;
bool			MyRandomPickups;
REGENPOINT	*	RegenPoints = nullptr;
int16_t			NumRegenPoints = 0;
int16_t			NumPickupsPerGroup[ MAXGROUPS ];
int16_t			NumPickupType[ MAXPICKUPTYPES ];
PICKUP	*		PickupGroups[ MAXGROUPS ];
PICKUP			Pickups[ MAXPICKUPS ];
PICKUP			PickupsCopy[ MAX_PLAYERS ][ MAXPICKUPS ];
REGENPOINT	*	RegenSlotsCopy[ MAX_PLAYERS ];
int16_t			NumStealths = 0;
int16_t			NumInvuls = 0;
int16_t			NumSuperNashrams = 0;
int16_t			NumOrbs = 0;
bool			PickupInvulnerability = false;
int16_t			NumGoldBars = 0;
int16_t			PickupsGot[ MAXPICKUPTYPES ];
bool			MyPickupValid[ MAXPICKUPTYPES ];
bool			PickupValid[ MAXPICKUPTYPES ];
int				FlagsToGenerate = 0;
int8_t			TeamFlagPickup[ MAX_TEAMS ] = {
	PICKUP_Flag1,
	PICKUP_Flag2,
	PICKUP_Flag3,
	PICKUP_Flag4,
};


int16_t	NumPrimaryPickups = 1;

char			CTFMessage[ MAXTEXTMSG ];

const char *Messages[] = {

			LT_PICKUP_TROJAX				,//"Trojax",						// Primary Weapons
			LT_PICKUP_PYROLITE				,//"Pyrolite Rifle",
			LT_PICKUP_TRANSPULSE			,//"Transpulse Cannon",
			LT_PICKUP_SUSSGUN				,//"Suss-Gun",
			LT_PICKUP_LASER					,//"Beam Laser",
			LT_PICKUP_MUG					,//"Mug Missile",					// Secondary Weapons
			LT_PICKUP_MUG_PACK				,//"Mug Missiles",
			LT_PICKUP_SOLARIS				,//"Heatseeker Missile",
			LT_PICKUP_SOLARIS_PACK			,//"Heatseeker Missile Pack",
			LT_PICKUP_THIEF					,//"Thief Missile",
			LT_PICKUP_SCATTER				,//"Scatter Missile",
			LT_PICKUP_GRAVGON				,//"Gravgon Missile",
			LT_PICKUP_MFRL					,//"Rocket Launcher",
			LT_PICKUP_TITAN					,//"TitanStar Missile",
			LT_PICKUP_PURGE					,//"Purge Mine Pack",
			LT_PICKUP_PINE					,//"Pine Mine Pack",
			LT_PICKUP_QUANTUM				,//"Quantum Mine Pack",
			LT_PICKUP_SPIDER				,//"Spider Mine Pack",
			LT_PICKUP_PARASITE				,//"Parasite Mine",
			LT_PICKUP_FLARE					,//"Flare",
			LT_PICKUP_GENAMMO				,//"General Ammo",					// Ammo
			LT_PICKUP_PYROFUEL				,//"Pyrolite Fuel",
			LT_PICKUP_SUSSAMMO				,//"Suss-Gun Ammo",
			LT_PICKUP_POWER_POD				,//"Power Pod",					// Extras
			LT_PICKUP_SHIELD				,//"Shield",
			LT_PICKUP_INVULBERABILITY		,//"Invulnerability",
			LT_PICKUP_EXTRA_LIFE			,//"Extra Life",
			LT_PICKUP_TARGETING_COMPUTER	,//"Targeting Computer",
			LT_PICKUP_SMOKE					,//"Smoke Streamer",
			LT_PICKUP_NITRO					,//"Nitro",
			LT_PICKUP_GOGGLES				,//"Goggles",
			LT_PICKUP_GOLD_BARS				,//"Gold Bars",
			LT_PICKUP_STEALTH				,//"Stealth Mantle",
			LT_PICKUP_CRYSTAL				,//"Crystal",
			LT_PICKUP_ORBITAL				,//"Orbit Pulsar",
			LT_PICKUP_GOLDEN_POWER_POD		,//"Golden Power Pod",
			LT_PICKUP_DNA					,//"DNA",
			"",														// skeleton key
			LT_PICKUP_BOMB					,//"Bomb",
			LT_PICKUP_GOLDEN_IDOL			,//"Golden Idol",
			"",								// flag
			"",
			"",								// flag 1
			"",								// flag 2
			"",								// flag 3
			"",								// flag 4
};

/* Pickup-type → message string lookup with bounds-check. Messages[] is
 * sized by the static initializer above (~46 entries — 1998 pickup
 * count). MAXPICKUPTYPES is now 128 (Remaster N64 pickups 100..105 plus
 * potential future expansion). Death-drops or .pic spawns that land on
 * a high-ID type can index past the array; without this guard the
 * Messages[Type] read returns adjacent-BSS garbage that gets fed to
 * sprintf("%s", ...) → strcpy → fault. */
static const char *PickupMessage( u_int16_t Type )
{
	const int n = (int)(sizeof(Messages) / sizeof(Messages[0]));
	if ((int)Type < n && Messages[Type]) return Messages[Type];
	return "";
}

namespace {

/* The pickup pool's private bookkeeping (one instance, g_pickups):
 * - the used and free list heads through Pickups[], changed whenever a
 *   pickup is placed or killed and walked by the per-frame processing;
 * - the per-type caps the level's .PIC sets (MaxPickupType), loaded with
 *   the level and lowered when a once-only pickup goes;
 * - the counts still to regenerate (primaries and power pods), raised when
 *   one goes back to the regen list and drained by the per-frame
 *   regeneration, and BountyToGenerate, which only that drain touches;
 * - in the Remaster, the model slots PreLoadPickups gives the N64 meshes.
 * The list heads, caps, primary and power-pod counts are saved and restored
 * with the game. Pickups[] and the other counts stay global because other
 * files use them. */
class PickupPool {
	public:
	void      init();
	void      clear_got();
	void      kill( u_int16_t Owner, u_int16_t ID, int16_t Style );
	u_int16_t find_free();
	void      kill_used( u_int16_t i );
	u_int16_t init_one( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod );
	void      process();
	void      save_positions();
#ifdef EDITION_REMASTER
	bool      preload();
#endif
	bool      load_positions();
	void      regenerate();
	void      kill_all();
	void      copy( u_int16_t Player );
	void      add_to_regen( u_int16_t Type );
	void      sfx_for_collect( u_int16_t Owner, u_int16_t ID );
	FILE *    save( FILE * fp );
	FILE *    restore( FILE * fp );
	void      kill_all_of_type( u_int16_t Type, int16_t Style );
	void      model_valid();
	u_int16_t find( u_int16_t Owner, u_int16_t ID );

	private:
#ifdef EDITION_REMASTER
	[[gnu::always_inline]] inline void LoadPickupsN64Templates();
#endif
	[[gnu::always_inline]] inline void LoadPickupsSetupRegenPoints();
	[[gnu::always_inline]] inline void LoadPickupsSetMinimums();

	u_int16_t FirstPickupUsed = 0;
	u_int16_t FirstPickupFree = 0;
	int16_t   NumPowerPods = 0;
	int16_t   NumPrimWeapons[ MAXPRIMARYWEAPONS ] = {};
	int       BountyToGenerate = 0;
	int16_t   MaxPickupType[ MAXPICKUPTYPES ] = {};
#ifdef EDITION_REMASTER
	/* Slot indices (into ModelNames[]) where PreLoadPickups registers each
	 * N64 pickup mesh. PickupAttribs[].ModelType is a ModelNames[] index, so
	 * once the slots are reserved we point the new pickup IDs at them. -1
	 * means the slot wasn't allocated (mesh file missing, or PreLoadPickups
	 * has not run). */
	int16_t   s_n64_pickup_model_idx[5] = { -1, -1, -1, -1, -1 };
#endif
};

} // namespace

PICKUPATTRIB PickupAttribs[ MAXPICKUPTYPES ] = {

/*===================================================================
	0 Trojax
===================================================================*/
	{
		MODEL_Trojax,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;	   	// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	1 Pyrolite
===================================================================*/
	{
		MODEL_Pyrolite,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	2 Transpulse
===================================================================*/
	{
		MODEL_Transpulse,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	3 Sussgun
===================================================================*/
	{
		MODEL_SussGun,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	4 Beam Laser
===================================================================*/
	{
		MODEL_Laser,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},

/*===================================================================
	5 Mug Missile
===================================================================*/
	{
		MODEL_Mug,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 128.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	6 Mug Missiles
===================================================================*/
	{
		MODEL_Mugs,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	7 Solaris Heatseaker
===================================================================*/
	{
		MODEL_Heatseaker,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	8 Heatseaker Pickup
===================================================================*/
	{
		MODEL_HeatseakerPickup,		// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	9 Thief Missile
===================================================================*/
	{
//		MODEL_Thief,				// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	10 Scatter Missile
===================================================================*/
	{
		MODEL_Scatter,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	11 Gravgon Missile
===================================================================*/
	{
		MODEL_Gravgon,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	12 Rocket Launcher
===================================================================*/
	{
		MODEL_Launcher,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	13 TitanStar Missile
===================================================================*/
	{
		MODEL_TitanStar,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	14 Purge Mine Pickup
===================================================================*/
	{
		MODEL_PurgePickup,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	15 Pine Mine Pickup
===================================================================*/
	{
		MODEL_PinePickup,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	16 Quantum Mine Pickup
===================================================================*/
	{
		MODEL_QuantumPickup,		// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	17 Spider Pod New
===================================================================*/
	{
//		MODEL_SpiderPod,			// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	18 Parasite Mine
===================================================================*/
	{
//		MODEL_Parasite,				// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 128.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	19 Flare
===================================================================*/
	{
//		MODEL_Flare,				// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},

/*===================================================================
	20 General Ammo
===================================================================*/
	{
		MODEL_GeneralAmmo,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	21 Pyrolite Fuel
===================================================================*/
	{
		MODEL_PyroliteAmmo,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	22 SussGun Ammo
===================================================================*/
	{
		MODEL_SussGunAmmo,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},


/*===================================================================
	23 Power Pod
===================================================================*/
	{
		MODEL_PowerPod,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	24 Shield
===================================================================*/
	{
		MODEL_Shield,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	25 Invulnerability
===================================================================*/
	{
		MODEL_Inv,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	26 Extra Life
===================================================================*/
	{
		MODEL_ExtraLife,			// u_int16_t	ModelType;	// if 3D model which one....
		0, //1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	27 Targeting Computer
===================================================================*/
	{
		MODEL_Computer,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	28 Smoke Streamer
===================================================================*/
	{
//		MODEL_Smoke,				// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	29 Nitro
===================================================================*/
	{
		MODEL_Nitro,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	30 Goggles
===================================================================*/
	{
//		MODEL_Goggles,				// u_int16_t	ModelType;	// if 3D model which one....
		MODEL_Eyeball,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	31 Gold Bars
===================================================================*/
	{
		MODEL_Gold,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	32 Mantle ( Cloaking Device )
===================================================================*/
	{
		MODEL_Mantle,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	33 Crystal
===================================================================*/
	{
		MODEL_Crystal,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	34 Orb
===================================================================*/
	{
		MODEL_Orb,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},

/*===================================================================
	35 Golden Power Pod
===================================================================*/
	{
		MODEL_GoldenPowerPod,		// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 0.0F			// float	b;			// what color does it emit....
	},
/*===================================================================
	36 DNA Pickup
===================================================================*/
	{
		MODEL_DNA,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 255.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	37 Skeleton Key Pickup
===================================================================*/
	{
		MODEL_SkeletonKey,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 0.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 0.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 128.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	38 Bomb Pickup
===================================================================*/
	{
		MODEL_Bomb,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 64.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 64.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	39 Gold Figure
===================================================================*/
	{
		MODEL_GoldFigure,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 192.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	40 Flag
===================================================================*/
	{
		MODEL_Flag,					// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 192.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 128.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	41 Bounty
===================================================================*/
	{
		MODEL_Gold	,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 192.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 192.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	42 Flag1 (red)
===================================================================*/
	{
		MODEL_RedFlag,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 192.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 64.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	43 Flag2 (green)
===================================================================*/
	{
		MODEL_GreenFlag,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 64.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 192.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	44 Flag3 (blue)
===================================================================*/
	{
		MODEL_BlueFlag,				// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 64.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 64.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 192.0F		// float	b;			// what color does it emit....
	},
/*===================================================================
	45 Flag4 (yellow)
===================================================================*/
	{
		MODEL_YellowFlag,			// u_int16_t	ModelType;	// if 3D model which one....
		1,							// u_int16_t	light;		// do I produce Light
		PICKUPLIGHT_RADIUS,			// float	lightsize;	// how big is the light
		COLOUR_SCALE * 192.0F,		// float	r;			// what color does it emit....
		COLOUR_SCALE * 192.0F,		// float	g;			// what color does it emit....
		COLOUR_SCALE * 64.0F		// float	b;			// what color does it emit....
	},
};

/*===================================================================
	Procedure	:	Set up And Init all Pickups
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void PickupPool::init()
{
	u_int16_t		i;

	InitFailedKillSlots();

	FirstPickupUsed = INDEX_NONE;
	FirstPickupFree = 0;

	SetupPickupGroups();
	ClearPickupsGot();

	for( i = 0; i < MAXPICKUPS; i++ )
	{
		memset( &Pickups[ i ], 0, sizeof( PICKUP ) );
		Pickups[ i ].Next = i + 1;
		Pickups[ i ].Prev = INDEX_NONE;

		Pickups[ i ].NextInGroup = nullptr;
		Pickups[ i ].PrevInGroup = nullptr;

		Pickups[ i ].Index = i;
		Pickups[ i ].LifeCount = -1.0F;
		Pickups[ i ].Mode = PICKUPMODE_Normal;
		Pickups[ i ].Type = INDEX_NONE;
		Pickups[ i ].Rot.x = -1.2F;
		Pickups[ i ].Rot.y = 2.0F;
		Pickups[ i ].Rot.z = 1.0F;
		Pickups[ i ].Group = 0;
		Pickups[ i ].DirVector = Forward;
		Pickups[ i ].UpVector = SlideUp;
		Pickups[ i ].RegenSlot = -1;
		Pickups[ i ].TriggerModPtr = nullptr;
		Pickups[ i ].TriggerMod = INDEX_NONE;
		Pickups[ i ].DirQuat.From2Vectors(&Forward, &Pickups[ i ].DirVector);
		Pickups[ i ].DirQuat.ToMatrix(&Pickups[ i ].Mat);
	}
	Pickups[ MAXPICKUPS-1 ].Next = INDEX_NONE;
}


/*===================================================================
	Procedure	:	Clear Pickups Got
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void PickupPool::clear_got()
{
	int16_t	Count;

	/* clear all pickups held */
	for( Count = 0; Count < MAXPICKUPTYPES; Count++ )
		PickupsGot[ Count ] = 0;

	/* clear all primaries */
	for( Count = 1; Count < MAXPRIMARYWEAPONS; Count++ )
		NumPrimWeapons[ Count ] = 0;

	/* clear special pickups */
	NumStealths			= 0;
	NumInvuls			= 0;
	NumSuperNashrams	= 0;
	NumOrbs				= 0;
	NumPowerPods		= 0;

}

/*===================================================================
	Procedure	:	Collect Pickup from scene
	Input		:	u_int16_t	Pickup to get
	Output		:	bool	True/False
===================================================================*/
/* PICKUP_Trojax */
[[gnu::always_inline]] static inline void CollectTrojax( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, int16_t & TriggeredSFX )
{
	if( !PrimaryWeaponsGot[ TROJAX ] )
	{
		PrimaryWeaponsGot[ TROJAX ] = 1;
		MessageSFX = SFX_Select_Trojax;
		TriggeredSFX = SFX_BIKER_BW;
		NewPrimaryWeapon( TROJAX );
	}
	else
	{
		PickupEnable = false;
		MessageSFX = SFX_BIKECOMP_AP;
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_A, Messages[ Pickups[i].Type ] );
	}
}

/* PICKUP_Pyrolite */
[[gnu::always_inline]] static inline void CollectPyrolite( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !PrimaryWeaponsGot[ PYROLITE_RIFLE ] )
	{
		PrimaryWeaponsGot[ PYROLITE_RIFLE ] = 1;

		if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
		{
			CopyOfPyroliteAmmo = MAXPYROLITEAMMO;
		}
		else
		{
			PyroliteAmmo = MAXPYROLITEAMMO;
		}
		NewPrimaryWeapon( PYROLITE_RIFLE );
		MessageSFX = SFX_Select_Pyrolite;
	}
	else
	{
		PickupEnable = false;
		MessageSFX = SFX_BIKECOMP_AP;
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_A, Messages[ Pickups[i].Type ] );
	}

}

/* PICKUP_Transpulse */
[[gnu::always_inline]] static inline void CollectTranspulse( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, int16_t & TriggeredSFX )
{
	if( !PrimaryWeaponsGot[ TRANSPULSE_CANNON ] )
	{
		PrimaryWeaponsGot[ TRANSPULSE_CANNON ] = 1;
		NewPrimaryWeapon( TRANSPULSE_CANNON );
		MessageSFX = SFX_Select_Transpulse;
		TriggeredSFX = SFX_BIKER_BW;
	}
	else
	{
		PickupEnable = false;
		MessageSFX = SFX_BIKECOMP_AP;
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_A, Messages[ Pickups[i].Type ] );
	}

}

/* PICKUP_SussGun */
[[gnu::always_inline]] static inline void CollectSussGun( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !PrimaryWeaponsGot[ SUSS_GUN ] )
	{
		PrimaryWeaponsGot[ SUSS_GUN ] = 1;

		if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
		{
			CopyOfSussGunAmmo = MAXSUSSGUNAMMO;
		}
		else
		{
			SussGunAmmo = MAXSUSSGUNAMMO;
		}
		NewPrimaryWeapon( SUSS_GUN );
		MessageSFX = SFX_Select_SussGun;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_A, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_Laser */
[[gnu::always_inline]] static inline void CollectLaser( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !PrimaryWeaponsGot[ LASER ] )
	{
		PrimaryWeaponsGot[ LASER ] = 1;
		NewPrimaryWeapon( LASER );
		MessageSFX = SFX_Select_BeamLaser;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_A, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Mugs */
[[gnu::always_inline]] static inline void CollectMugs( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ MUGMISSILE ] < 10 )
	{
		SecondaryAmmo[ MUGMISSILE ] += 3;
		if( SecondaryAmmo[ MUGMISSILE ] > 10 )
		{
			Temp = ( SecondaryAmmo[ MUGMISSILE ] - 10 );
			SecAmmoUsed[ MUGMISSILE ] = Temp;
			SecondaryAmmo[ MUGMISSILE ] = 10;
		}
		NewSecondaryWeapon( MUGMISSILE );
		MessageSFX = SFX_Select_MugMissile;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_HeatseakerPickup */
[[gnu::always_inline]] static inline void CollectHeatseakerPickup( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ SOLARISMISSILE ] < 10 )
	{
		SecondaryAmmo[ SOLARISMISSILE ] += 3;
		if( SecondaryAmmo[ SOLARISMISSILE ] > 10 )
		{
			Temp = ( SecondaryAmmo[ SOLARISMISSILE ] - 10 );
			SecAmmoUsed[ SOLARISMISSILE ] = Temp;
			SecondaryAmmo[ SOLARISMISSILE ] = 10;
		}
		NewSecondaryWeapon( SOLARISMISSILE );
		MessageSFX = SFX_Select_SolarisMissile;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_Thief */
[[gnu::always_inline]] static inline void CollectThief( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !SecondaryAmmo[ THIEFMISSILE ] )
	{
		SecondaryAmmo[ THIEFMISSILE ] += 1;
		NewSecondaryWeapon( THIEFMISSILE );
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Scatter */
[[gnu::always_inline]] static inline void CollectScatter( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ SCATTERMISSILE ] < 3 )
	{
		SecondaryAmmo[ SCATTERMISSILE ] += 1;
		NewSecondaryWeapon( SCATTERMISSILE );
		MessageSFX = SFX_Select_ScatterMissile;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Gravgon */
[[gnu::always_inline]] static inline void CollectGravgon( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !SecondaryAmmo[ GRAVGONMISSILE ] )
	{
		SecondaryAmmo[ GRAVGONMISSILE ] += 1;
		NewSecondaryWeapon( GRAVGONMISSILE );
		MessageSFX = SFX_Select_GravgonMissile;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_Launcher */
[[gnu::always_inline]] static inline void CollectLauncher( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ MULTIPLEMISSILE ] < 100 )
	{
		SecondaryAmmo[ MULTIPLEMISSILE ] += 50;
		if( SecondaryAmmo[ MULTIPLEMISSILE ] > 100 )
		{
			Temp = ( SecondaryAmmo[ MULTIPLEMISSILE ] - 100 );
			SecAmmoUsed[ MULTIPLEMISSILE ] = Temp;
			SecondaryAmmo[ MULTIPLEMISSILE ] = 100;
		}
		NewSecondaryWeapon( MULTIPLEMISSILE );
		MessageSFX = SFX_Select_MFRL;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_TitanStar */
[[gnu::always_inline]] static inline void CollectTitanStar( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, int16_t & TriggeredSFX )
{
	if( SecondaryAmmo[ TITANSTARMISSILE ] < 3 )
	{
		SecondaryAmmo[ TITANSTARMISSILE ] += 1;
		NewSecondaryWeapon( TITANSTARMISSILE );
		MessageSFX = SFX_Select_TitanStarMissile;
		TriggeredSFX = SFX_BIKER_BW;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_PurgePickup */
[[gnu::always_inline]] static inline void CollectPurgePickup( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ PURGEMINE ] < 10 )
	{
		SecondaryAmmo[ PURGEMINE ] += 3;
		if( SecondaryAmmo[ PURGEMINE ] > 10 )
		{
			Temp = ( SecondaryAmmo[ PURGEMINE ] - 10 );
			SecAmmoUsed[ PURGEMINE ] = Temp;
			SecondaryAmmo[ PURGEMINE ] = 10;
		}
		NewSecondaryWeapon( PURGEMINE );
		MessageSFX = SFX_Select_PurgeMine;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_PinePickup */
[[gnu::always_inline]] static inline void CollectPinePickup( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ PINEMINE ] < 6 )
	{
		SecondaryAmmo[ PINEMINE ] += 3;
		if( SecondaryAmmo[ PINEMINE ] > 6 )
		{
			Temp = ( SecondaryAmmo[ PINEMINE ] - 6 );
			SecAmmoUsed[ PINEMINE ] = Temp;
			SecondaryAmmo[ PINEMINE ] = 6;
		}
		NewSecondaryWeapon( PINEMINE );
		MessageSFX = SFX_Select_PineMine;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_QuantumPickup */
[[gnu::always_inline]] static inline void CollectQuantumPickup( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !SecondaryAmmo[ QUANTUMMINE ] )
	{
		SecondaryAmmo[ QUANTUMMINE ] += 1;
		NewSecondaryWeapon( QUANTUMMINE );
		MessageSFX = SFX_Select_QuantumMine;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_SpiderPod */
[[gnu::always_inline]] static inline void CollectSpiderPod( u_int16_t & i, int16_t & PickupEnable, int16_t & Temp, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( SecondaryAmmo[ SPIDERMINE ] < 6 )
	{
		SecondaryAmmo[ SPIDERMINE ] += 3;
		if( SecondaryAmmo[ SPIDERMINE ] > 6 )
		{
			Temp = ( SecondaryAmmo[ SPIDERMINE ] - 6 );
			SecAmmoUsed[ SPIDERMINE ] = Temp;
			SecondaryAmmo[ SPIDERMINE ] = 6;
		}
		NewSecondaryWeapon( SPIDERMINE );
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_GeneralAmmo */
[[gnu::always_inline]] static inline void CollectGeneralAmmo( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfGeneralAmmo < MAXGENERALAMMO )
		{
			CopyOfGeneralAmmo += GENERALAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}
	else
	{
		if( GeneralAmmo < MAXGENERALAMMO )
		{
			GeneralAmmo += GENERALAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}
}

/* PICKUP_PyroliteAmmo */
[[gnu::always_inline]] static inline void CollectPyroliteAmmo( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfPyroliteAmmo < MAXPYROLITEAMMO )
		{
			CopyOfPyroliteAmmo += PYROLITEAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}
	else
	{
		if( PyroliteAmmo < MAXPYROLITEAMMO )
		{
			PyroliteAmmo += PYROLITEAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}

}

/* PICKUP_SussGunAmmo */
[[gnu::always_inline]] static inline void CollectSussGunAmmo( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfSussGunAmmo < MAXSUSSGUNAMMO )
		{
			CopyOfSussGunAmmo += SUSSGUNAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}
	else
	{
		if( SussGunAmmo < MAXSUSSGUNAMMO )
		{
			SussGunAmmo += SUSSGUNAMMOPERPOD;
			MessageSFX = SFX_Select_Ammo;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			PickupEnable = false;
			MessageSFX = SFX_BIKECOMP_MA;
		}
	}
}

/* PICKUP_PowerPod */
[[gnu::always_inline]] static inline void CollectPowerPod( int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, bool & ShowTextAnyway )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfPowerLevel != ( MAXPOWERLEVELS - 1 ) )
		{
			CopyOfPowerLevel++;
			if( CopyOfPowerLevel == ( MAXPOWERLEVELS - 1 ) )
			{
				snprintf( (char*)&Message[ 0 ], sizeof( Message ), FULL_POWER_LEVEL );
			}
			else
			{
				snprintf( (char*)&Message[ 0 ], sizeof( Message ), POWER_POD_LEVEL, Ships[ WhoIAm ].Object.PowerLevel+1 );
			}
			ShowTextAnyway = true;
			MessageSFX = SFX_Select_PowerPod;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX_POWER );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}
	else
	{
		if( Ships[ WhoIAm ].Object.PowerLevel != ( MAXPOWERLEVELS - 1 ) )
		{
			Ships[ WhoIAm ].Object.PowerLevel++;
			if( Ships[ WhoIAm ].Object.PowerLevel == ( MAXPOWERLEVELS - 1 ) )
			{
				snprintf( (char*)&Message[ 0 ], sizeof( Message ), FULL_POWER_LEVEL );
			}
			else
			{
				snprintf( (char*)&Message[ 0 ], sizeof( Message ), POWER_POD_LEVEL, Ships[ WhoIAm ].Object.PowerLevel+1 );
			}
			ShowTextAnyway = true;
			MessageSFX = SFX_Select_PowerPod;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX_POWER );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}
}

/* PICKUP_GoldenPowerPod */
[[gnu::always_inline]] static inline void CollectGoldenPowerPod( int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		GivemeSuperNashram();
		MessageSFX = SFX_Select_GoldenPowerPod;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_CANT_HANDLE_ANY_MORE );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Shield */
[[gnu::always_inline]] static inline void CollectShield( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( Ships[ WhoIAm ].Object.Shield != MAX_SHIELD )
	{
		Ships[ WhoIAm ].Object.Shield += 32.0F;
		if ( Ships[ WhoIAm ].Object.Shield > SHIELD_CRITICAL_LEVEL )
			ShieldCritical = false;
		if( Ships[ WhoIAm ].Object.Shield > MAX_SHIELD ) Ships[ WhoIAm ].Object.Shield = MAX_SHIELD;
		MessageSFX = SFX_Select_Shield;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}
}

/* PICKUP_Inv */
[[gnu::always_inline]] static inline void CollectInv( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( !Ships[ WhoIAm ].Invul )
	{
		Ships[ WhoIAm ].InvulTimer = INVULNERABILITY_TIME;
		Ships[ WhoIAm ].Invul = true;
		PickupInvulnerability = true;
		MessageSFX = SFX_Select_Invul;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Nitro */
[[gnu::always_inline]] static inline void CollectNitro( u_int16_t & i, int16_t & PickupEnable, float & TempFloat, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfNitroFuel < MAX_NITRO_FUEL )
		{
			CopyOfNitroFuel += 50.0F;
			if( CopyOfNitroFuel > MAX_NITRO_FUEL )
			{
				TempFloat = ( CopyOfNitroFuel - MAX_NITRO_FUEL );
				NitroFuelUsed += TempFloat;
				CopyOfNitroFuel = MAX_NITRO_FUEL;
			}
			MessageSFX = SFX_Select_Nitro;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}
	else
	{
		if( NitroFuel < MAX_NITRO_FUEL )
		{
			NitroFuel += 50.0F;
			if( NitroFuel > MAX_NITRO_FUEL )
			{
				TempFloat = ( NitroFuel - MAX_NITRO_FUEL );
				NitroFuelUsed += TempFloat;
				NitroFuel = MAX_NITRO_FUEL;
			}
			MessageSFX = SFX_Select_Nitro;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}

}

/* PICKUP_Gold */
[[gnu::always_inline]] static inline void CollectGold( u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, bool & ShowTextAnyway, bool & Speech )
{
	NumGoldBars ++;
	if( !( NumGoldBars % 10 ) )
	{
		Lives += 1;
		snprintf( (char*)&Message[0], sizeof( Message ), YOUVE_EARNED_EXTRA_LIFE );
		PlaySfx( SFX_ExtraLife, 1.0F );
	}
	else
	{
		if( NumGoldBars == 1 ) snprintf( (char*)&Message[0], sizeof( Message ), YOU_HAVE_ONE_GOLD_BAR, NumGoldBars );
		else snprintf( (char*)&Message[0], sizeof( Message ), YOU_HAVE_GOLD_BARS, NumGoldBars );
	}
	MessageSFX = SFX_GoldBarPickup;
	Speech = false;
	ShowTextAnyway = true;
}

/* PICKUP_Mantle */
[[gnu::always_inline]] static inline void CollectMantle( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, bool & Speech )
{
	if( !( Ships[ WhoIAm ].Object.Flags & SHIP_Stealth ) )
	{
		Ships[ WhoIAm ].Object.Flags |= SHIP_Stealth;
		Ships[ WhoIAm ].StealthTime = ( 60.0F * ANIM_SECOND );
		MessageSFX = SFX_Cloaking;
		Speech = false;
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE, Messages[ Pickups[i].Type ] );
		MessageSFX = SFX_BIKECOMP_AP;
		PickupEnable = false;
	}

}

/* PICKUP_Crystal */
[[gnu::always_inline]] static inline void CollectCrystal( u_int8_t (&Message)[ 128 ], int16_t & MessageSFX, bool & ShowTextAnyway, bool & Speech )
{
	CrystalsFound += 1;
	if( CrystalsFound == 1 )
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_HAVE_ONE_CRYSTAL, CrystalsFound );
	}
	else
	{
		snprintf( (char*)&Message[0], sizeof( Message ), YOU_HAVE_CRYSTALS, CrystalsFound );
	}
	MessageSFX = SFX_PickupCrystal;
	Speech = false;
	ShowTextAnyway = true;
}

/* PICKUP_Flag */
[[gnu::always_inline]] static inline void CollectFlag(  )
{
	if ( CaptureTheFlag )
	{
		// now I have the flag....?
		Ships[ WhoIAm ].Object.Flags |= SHIP_CarryingFlag;
		AddColourMessageToQue(FlagMessageColour, TAKE_FLAG_TO_GOAL,
			TeamName[ TeamNumber[ WhoIAm ] ] );

		snprintf( CTFMessage, sizeof( CTFMessage ), SOMEONE_HAS_GOT_THE_FLAG, Names[ WhoIAm ], TeamName[ TeamNumber[ WhoIAm ] ] );
		SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_CaptureFlagMessage, 0);

	}
}

/* PICKUP_Bounty */
[[gnu::always_inline]] static inline void CollectBounty(  )
{
	if ( BountyHunt )
	{
		// now I have the bounty....?
		BountyTime = 0.0F;
		Ships[ WhoIAm ].Object.Flags |= SHIP_CarryingBounty;
		AddColourMessageToQue(FlagMessageColour, YOU_HAVE_GOT_THE_BOUNTY );

		SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_BountyMessage, 0);

	}
}

/* PICKUP_Flag1, PICKUP_Flag2, PICKUP_Flag3, PICKUP_Flag4 */
[[gnu::always_inline]] static inline void CollectFlag1( u_int16_t & i, int16_t & PickupEnable )
{
	if ( CTF )
	{
		int team;

		for ( team = 0; team < MAX_TEAMS; team++ )
		{
			if ( Pickups[i].Type == TeamFlagPickup[ team ] )
			{
				if ( team == TeamNumber[ WhoIAm ] )
				{
					if ( TeamFlagAtHome[ team ] )
					{
						PickupEnable = false;
					}
					else if ( OwnFlagTeleportsHome )
					{
						GOAL *goal;

						// teleport flag back to own goal
						PickupEnable = false;
						goal = TeamGoal( team );
						if ( goal )
						{
							KillPickupSend( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
							KillPickup( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
							GenerateFlagAtHome( team );
							AddColourMessageToQue(FlagMessageColour, TEAM_FLAG_RETURNED,
								TeamName[ TeamNumber[ WhoIAm ] ] );
							SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_ReturnedFlag, 0);
						}
					}
					else if ( CanCarryOwnFlag )
					{
						Ships[ WhoIAm ].Object.Flags |= TeamFlagMask[ team ];
						AddColourMessageToQue(FlagMessageColour, RETURN_TEAM_FLAG,
							TeamName[ TeamNumber[ WhoIAm ] ] );
						SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_ReturningFlag, 0);
					}
					else
					{
						AddColourMessageToQue(FlagMessageColour, CANNOT_PICKUP_OWN_FLAG );
						PickupEnable = false;
					}
				}
				else
				{
					// now I have the flag....?
					Ships[ WhoIAm ].Object.Flags |= TeamFlagMask[ team ];
					AddColourMessageToQue(FlagMessageColour, TAKE_FLAG_TO_GOAL,
						TeamName[ TeamNumber[ WhoIAm ] ] );

					snprintf( CTFMessage, sizeof( CTFMessage ), SOMEONE_HAS_OTHER_TEAM_FLAG,
						Names[ WhoIAm ],
						TeamName[ TeamNumber[ WhoIAm ] ],
						TeamName[ team ] );
					SendGameMessage(MSG_TEXTMSG, 0, 0, TEXTMSGTYPE_CaptureFlagMessage, 0);
				}
				break;
			}
		}

	}
}

/* PICKUP_Orb */
[[gnu::always_inline]] static inline void CollectOrb( u_int16_t & i, int16_t & PickupEnable, u_int8_t (&Message)[ 128 ], int16_t & MessageSFX )
{
	if( ( Ships[ WhoIAm ].Object.Flags & SHIP_SuperNashram ) )
	{
		if( CopyOfOrbitals < MAXMULTIPLES )
		{
			CopyOfOrbAmmo[ CopyOfOrbitals ] = MAXMULTIPLEAMMO;
			CopyOfOrbitals++;
			MessageSFX = SFX_Orbital;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}
	else
	{
		if( Ships[ WhoIAm ].NumMultiples < MAXMULTIPLES )
		{
			Ships[ WhoIAm ].OrbAmmo[ Ships[ WhoIAm ].NumMultiples ] = MAXMULTIPLEAMMO;
			Ships[ WhoIAm ].NumMultiples++;
			CreateOrbitPulsar( WhoIAm );
			MessageSFX = SFX_Orbital;
		}
		else
		{
			snprintf( (char*)&Message[0], sizeof( Message ), YOU_ALREADY_HAVE_MAX, Messages[ Pickups[i].Type ] );
			MessageSFX = SFX_BIKECOMP_AP;
			PickupEnable = false;
		}
	}
}

bool CollectPickup( u_int16_t i )
{
	int16_t	PickupEnable = true;
	float	Sfx_Volume = 1.0F;
	int16_t	Temp;
	float	TempFloat;

	u_int8_t	Message[ 128 ];
	int16_t	MessageSFX = -1;
	int16_t	TriggeredSFX = -1;
	bool	ShowTextAnyway = false;
	bool	Speech = true;

	snprintf( (char*)&Message[0], sizeof( Message ), "%s", PickupMessage( Pickups[i].Type ) );

	if( i != INDEX_NONE )
	{
		switch( Pickups[i].Type )
		{
		case PICKUP_Trojax:
			CollectTrojax( i, PickupEnable, Message, MessageSFX, TriggeredSFX );
			break;

		case PICKUP_Pyrolite:
			CollectPyrolite( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Transpulse:
			CollectTranspulse( i, PickupEnable, Message, MessageSFX, TriggeredSFX );
			break;

		case PICKUP_SussGun:
			CollectSussGun( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Laser:
			CollectLaser( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Mug:
			break;

		case PICKUP_Mugs:
			CollectMugs( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_Heatseaker:
			break;

		case PICKUP_HeatseakerPickup:
			CollectHeatseakerPickup( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_Thief:
			CollectThief( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Scatter:
			CollectScatter( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Gravgon:
			CollectGravgon( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Launcher:
			CollectLauncher( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_TitanStar:
			CollectTitanStar( i, PickupEnable, Message, MessageSFX, TriggeredSFX );
			break;

		case PICKUP_PurgePickup:
			CollectPurgePickup( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_PinePickup:
			CollectPinePickup( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_QuantumPickup:
			CollectQuantumPickup( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_SpiderPod:
			CollectSpiderPod( i, PickupEnable, Temp, Message, MessageSFX );
			break;

		case PICKUP_Parasite:

			break;

		case PICKUP_Flare:

			MessageSFX = SFX_BIKECOMP_FL;

			break;

		case PICKUP_GeneralAmmo:
			CollectGeneralAmmo( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_PyroliteAmmo:
			CollectPyroliteAmmo( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_SussGunAmmo:
			CollectSussGunAmmo( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_PowerPod:
			CollectPowerPod( PickupEnable, Message, MessageSFX, ShowTextAnyway );
			break;

		case PICKUP_GoldenPowerPod:
			CollectGoldenPowerPod( PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Shield:
			CollectShield( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_Inv:
			CollectInv( i, PickupEnable, Message, MessageSFX );
			break;

		case PICKUP_ExtraLife:
			Lives += 1;
			MessageSFX = SFX_ResnicReanimator;
			PlaySfx( SFX_ExtraLife, 1.0F );
			break;

		case PICKUP_Computer:

			TargetComputerOn = true;

			break;

		case PICKUP_Smoke:
			break;

		case PICKUP_Nitro:
			CollectNitro( i, PickupEnable, TempFloat, Message, MessageSFX );
			break;

		case PICKUP_Goggles:
			break;

		case PICKUP_Gold:
			CollectGold( Message, MessageSFX, ShowTextAnyway, Speech );
			break;

		case PICKUP_Mantle:
			CollectMantle( i, PickupEnable, Message, MessageSFX, Speech );
			break;

		case PICKUP_Crystal:
			CollectCrystal( Message, MessageSFX, ShowTextAnyway, Speech );
			break;

		case PICKUP_DNA:

			MessageSFX = SFX_PickupGeneral;
			Speech = false;
			ShowTextAnyway = true;
			break;

		case PICKUP_SkeletonKey:

			MessageSFX = SFX_PickupGeneral;
			Speech = false;
			ShowTextAnyway = true;
			break;

		case PICKUP_Bomb:

			MessageSFX = SFX_PickupGeneral;
			Speech = false;
			ShowTextAnyway = true;
			break;

		case PICKUP_GoldFigure:

			MessageSFX = SFX_PickupGeneral;
			Speech = false;
			ShowTextAnyway = true;
			break;

		case PICKUP_Flag:
			CollectFlag(  );
			break;

		case PICKUP_Bounty:
			CollectBounty(  );
			break;

		case PICKUP_Flag1:
		case PICKUP_Flag2:
		case PICKUP_Flag3:
		case PICKUP_Flag4:
			CollectFlag1( i, PickupEnable );
			break;

		case PICKUP_Orb:
			CollectOrb( i, PickupEnable, Message, MessageSFX );
			break;
		}

		if( PickupEnable || no_collision )
		{

			if( ( MessageSFX != -1 ) && ( bSoundEnabled ) && ( BikeCompSpeechSlider.value || !Speech ) )
			{
				if ( ( TriggeredSFX != -1 ) && !Random_Range( 4 ) )
					PlaySfxWithTrigger( MessageSFX, TriggeredSFX );
				else
					PlaySfx( MessageSFX, Sfx_Volume );
				if( ShowTextAnyway )
					AddColourMessageToQue( PickupMessageColour, "%s", &Message[ 0 ] );
			}
			else
				if( Speech )
					PlaySfx( SFX_PickupGeneral, Sfx_Volume );

			PickupsGot[ Pickups[ i ].Type ]++;

			KillPickupSend( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
			KillPickup( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
			return true;
		}
		else
		{
			if( !Pickups[ i ].CouldNotPickup )
			{
				if( ( MessageSFX != -1 ) && ( bSoundEnabled ) && ( BikeCompSpeechSlider.value || !Speech ) )
				{
					if ( ( TriggeredSFX != -1 ) && !Random_Range( 4 ) )
						PlaySfxWithTrigger( MessageSFX, TriggeredSFX );
					else
						PlaySfx( MessageSFX, Sfx_Volume );

					if( ShowTextAnyway )
						AddColourMessageToQue( PickupMessageColour, "%s", &Message[ 0 ] );
				}
				Pickups[ i ].CouldNotPickup = true;
			}
			return false;
		}
	}
	else
		return false;
}

/*===================================================================
	Procedure	:	New Secondary Weapon ( AutoSelect )
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void NewSecondaryWeapon( int16_t Weapon )
{
	u_int16_t	Current_Type;

	SecondaryWeaponsGot[ Weapon ] = 1;

	Current_Type = SecondaryWeaponAttribs[ Ships[ WhoIAm ].Secondary ].SecType;
	if( SecondaryWeaponAttribs[ Weapon ].SecType != Current_Type ) return;

	if( player_config->secondary_priority[ Weapon ] != -1 )
	{
		if( player_config->secondary_priority[ Weapon ] < player_config->secondary_priority[ Ships[ WhoIAm ].Secondary ] )
		{
			Ships[ WhoIAm ].Secondary = (BYTE) Weapon;
		}
	}
}

/*===================================================================
	Procedure	:	New Primary Weapon ( AutoSelect )
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void NewPrimaryWeapon( int16_t Weapon )
{
	if( player_config->primary_priority[ Weapon ] < player_config->primary_priority[ Ships[ WhoIAm ].Primary ] )
	{
		PlaySfx( SFX_LoadWeapon, 1.0F );
		Ships[ WhoIAm ].Primary = (BYTE) Weapon;
	}
}

/*===================================================================
	Procedure	:	Ship to Pickups collision
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void CheckPickup( void )
{
	float		Length;
	VECTOR		Pos;
	VECTOR		Dir;
	int16_t		Count;
	GROUPLIST *	GroupsVisible;
	u_int16_t	  *	GroupList;
	u_int16_t		CurrentGroup;
	PICKUP	*	Pickup;
	PICKUP	*	NextPickup;

	if( Ships[ WhoIAm ].Object.Mode != NORMAL_MODE ) return;
	if( DebugInfo ) return;

	Dir.x = ( Ships[ WhoIAm ].Move_Off.x * framelag );
	Dir.y = ( Ships[ WhoIAm ].Move_Off.y * framelag );
	Dir.z = ( Ships[ WhoIAm ].Move_Off.z * framelag );

	Pos.x = ( Ships[ WhoIAm ].Object.Pos.x - Dir.x );
	Pos.y = ( Ships[ WhoIAm ].Object.Pos.y - Dir.y );
	Pos.z = ( Ships[ WhoIAm ].Object.Pos.z - Dir.z );

	Length = VectorLength( &Dir );
	NormaliseVector( &Dir );

	GroupsVisible = VisibleGroups( Ships[ WhoIAm ].Object.Group );

	GroupList = GroupsVisible->group;

	for( Count = 0; Count < GroupsVisible->groups; Count++ )
	{
		CurrentGroup = GroupList[ Count ];

		Pickup = PickupGroups[ CurrentGroup ];

		while( Pickup )
		{
			NextPickup = Pickup->NextInGroup;

			if( ( ( Pickup->Owner == WhoIAm ) && ( Pickup->Speed == 0.0F ) ) || ( Pickup->Owner != WhoIAm ) )
			{
				if( RaytoSphereShort( (VECTOR *) &Pickup->Pos, PICKUP_RADIUS, &Pos, &Dir, Length ) )
					CollectPickup( Pickup->Index );
				else
					Pickup->CouldNotPickup = false;
			}

			Pickup = NextPickup;
		}
	}
}

/*===================================================================
	Procedure	:	Kill a specific Pickup
	Input		:	u_int16_t	Owner
				:	u_int16_t	ID
				:	int16_t	Style to kill
	Output		:	nothing
===================================================================*/
void PickupPool::kill( u_int16_t Owner, u_int16_t ID, int16_t Style )
{
	u_int16_t	i;
	u_int16_t	NextPickup;
	u_int16_t	fmpoly;
	int16_t	Count = 0;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		NextPickup = Pickups[ i ].Prev;							/* Next Pickup */

		if( ( Pickups[ i ].Owner == Owner ) && ( Pickups[ i ].ID == ID ) )
		{
#if DEBUG_PICKUPS
			DebugPrintf( "pickups: Killed '%s' Owner '%s', ID %d\n", Messages[ Pickups[i].Type ], &Names[ Pickups[i].Owner ][ 0 ], Pickups[i].ID );
#endif

			switch( Style )
			{
			case PICKUPKILL_Immediate:
				if ( CTF )
				{
					int team;

					team = -1;
					switch ( Pickups[ i ].Type )
					{
					case PICKUP_Flag1:
						team = 0;
						break;
					case PICKUP_Flag2:
						team = 1;
						break;
					case PICKUP_Flag3:
						team = 2;
						break;
					case PICKUP_Flag4:
						team = 3;
						break;
					}
					if ( team >= 0 )
					{
						TeamFlagAtHome[ team ] = false;
					}
				}
				CleanUpPickup( i );
				Count++;
				if( Count > 1 )
				{
					Msg( "Killed more than 1 pickup with same type\n" );
				}
				break;

			case PICKUPKILL_Disappear:
				if( Pickups[ i ].Mode == PICKUPMODE_Disappear ) break;

				Pickups[ i ].LifeCount = 0.0F;
				Pickups[ i ].Mode = PICKUPMODE_Disappear;
				Pickups[ i ].PickupCount = 32.0F;

				fmpoly = FindFreeFmPoly();
				if( fmpoly != INDEX_NONE )
				{
					FmPolys[ fmpoly ].LifeCount = 1000.0F;
					FmPolys[ fmpoly ].Pos = Pickups[ i ].Pos;
					FmPolys[ fmpoly ].SeqNum = FM_PICKUP_REGEN;
					FmPolys[ fmpoly ].Frame = 0.0F;
					FmPolys[ fmpoly ].Frm_Info = &PickupRegen_Header;
					FmPolys[ fmpoly ].Flags = FM_FLAG_MOVEOUT;
					FmPolys[ fmpoly ].Trans = 192;
					FmPolys[ fmpoly ].Dir.x = 0.0F;
					FmPolys[ fmpoly ].Dir.y = 0.0F;
					FmPolys[ fmpoly ].Dir.z = 0.0F;
					FmPolys[ fmpoly ].R = 192;
					FmPolys[ fmpoly ].G = 192;
					FmPolys[ fmpoly ].B = 192;
					FmPolys[ fmpoly ].Speed = 0.0F;
					FmPolys[ fmpoly ].Rot = 0.0F;
					FmPolys[ fmpoly ].Group = Pickups[ i ].Group;
					FmPolys[ fmpoly ].RotSpeed = 0.0F;
					FmPolys[ fmpoly ].xsize = PICKUP_REGEN_SIZE;
					FmPolys[ fmpoly ].ysize = PICKUP_REGEN_SIZE;
					AddFmPolyToTPage( fmpoly, GetTPage( *FmPolys[ fmpoly ].Frm_Info, 0 ) );
				}
				break;
			}
			return;
		}

		i = NextPickup;											/* Next Pickup */
	}


	AddFailedKillToQue( Owner, ID, Style, INDEX_NONE );

#if DEBUG_PICKUPS
	if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Couldnt't kill pickup of Owner '%s', ID %d\n", &Names[ Owner ][ 0 ], ID );
	else DebugPrintf( "pickups: Couldnt't kill pickup of No Owner, ID %d\n", ID );
#endif
}


/*===================================================================
	Procedure	:	Find a free Pickup and move it from the free
				:	list to	the used list
	Input		:	nothing
	Output		:	u_int16_t number of Pickup free....
===================================================================*/
u_int16_t PickupPool::find_free()
{
	u_int16_t i;

	i = FirstPickupFree;

	if ( i == INDEX_NONE ) return i;

	Pickups[i].Prev = FirstPickupUsed;
	if( FirstPickupUsed != INDEX_NONE)
	{
		Pickups[ FirstPickupUsed ].Next = i;
	}

	Pickups[i].CouldNotPickup = false;

	FirstPickupUsed = i;
	FirstPickupFree = Pickups[ i ].Next;
	return i ;
}

/*===================================================================
	Procedure	:	Kill a used Pickup and move it from the used
				:	list to the free list
	Input		:	u_int16_t number of Pickup to kill....
	Output		:	nothing
===================================================================*/
void PickupPool::kill_used( u_int16_t i )
{
	u_int16_t	its_prev;
	u_int16_t	its_next;

	if( Pickups[ i ].TriggerModPtr != nullptr )
	{
		ApplyTriggerMod( Pickups[i].TriggerModPtr );
	}

	its_prev = Pickups[ i ].Prev;
	its_next = Pickups[ i ].Next;

	if ( i == FirstPickupUsed ) FirstPickupUsed = Pickups[ i ].Prev;
	if( its_prev != INDEX_NONE ) Pickups[ its_prev ].Next = its_next;
	if( its_next != INDEX_NONE ) Pickups[ its_next ].Prev = its_prev;

	if( Pickups[ i ].RegenSlot != -1 )
	{
		if( Pickups[ i ].RegenSlot < NumRegenPoints )
		{
			switch( RegenPoints[ Pickups[ i ].RegenSlot ].RegenType )
			{
			case PU_REGENTYPE_Random:		// Random each time
				RegenPoints[ Pickups[ i ].RegenSlot ].GenType = PU_GENTYPE_Initialised,		// Initialised immediatly
				RegenPoints[ Pickups[ i ].RegenSlot ].PickupIndex = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].TriggerMod = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].Status = PU_REGENSTAT_Free;
				RegenPoints[ Pickups[ i ].RegenSlot ].Wait = ( 5.0F * ANIM_SECOND );
				break;

			case PU_REGENTYPE_Const:		// Same each time
				RegenPoints[ Pickups[ i ].RegenSlot ].GenType = PU_GENTYPE_Initialised,		// Initialised immediatly
				RegenPoints[ Pickups[ i ].RegenSlot ].PickupIndex = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].TriggerMod = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].Status = PU_REGENSTAT_Free;
				RegenPoints[ Pickups[ i ].RegenSlot ].Wait = ( 5.0F * ANIM_SECOND );
				break;

			case PU_REGENTYPE_Onceonly:		// Once only ( Never used again )
				if( MaxPickupType[ Pickups[ i ].Type ] ) MaxPickupType[ Pickups[ i ].Type ]--;
				RegenPoints[ Pickups[ i ].RegenSlot ].GenType = PU_GENTYPE_Initialised,		// Initialised immediatly
				RegenPoints[ Pickups[ i ].RegenSlot ].PickupIndex = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].TriggerMod = INDEX_NONE;
				RegenPoints[ Pickups[ i ].RegenSlot ].Status = PU_REGENSTAT_Used;
				RegenPoints[ Pickups[ i ].RegenSlot ].Wait = 0.0F;
				break;
			}

#if DEBUG_PICKUPS
			if( Pickups[i].Owner != INDEX_NONE ) DebugPrintf( "pickups: Killed '%s' from slot %d, Owner '%s', ID %d\n", Messages[ Pickups[i].Type ], Pickups[i].RegenSlot, &Names[ Pickups[i].Owner ][ 0 ], Pickups[i].ID );
			else DebugPrintf( "pickups: Killed '%s' from slot %d, No Owner, ID %d\n", Messages[ Pickups[i].Type ], Pickups[i].RegenSlot, Pickups[i].ID );
#endif
		}
		else
		{
			Pickups[ i ].RegenSlot = -1;
#if DEBUG_PICKUPS
			if( Pickups[i].Owner != INDEX_NONE ) DebugPrintf( "pickups: Killed '%s'. Owner '%s', ID %d with slot grater than max slots\n", Messages[ Pickups[i].Type ], &Names[ Pickups[i].Owner ][ 0 ], Pickups[i].ID );
			else DebugPrintf( "pickups: Killed '%s'. No Owner, ID %d with slot grater than max slots\n", Messages[ Pickups[i].Type ], Pickups[i].ID );
#endif
		}
	}
	else
	{
#if DEBUG_PICKUPS
		if( Pickups[i].Owner != INDEX_NONE ) DebugPrintf( "pickups: Killed '%s' from slot %d, Owner '%s', ID %d\n", Messages[ Pickups[i].Type ], Pickups[i].RegenSlot, &Names[ Pickups[i].Owner ][ 0 ], Pickups[i].ID  );
		else DebugPrintf( "pickups: Killed '%s' from slot %d, No Owner, ID %d\n", Messages[ Pickups[i].Type ], Pickups[i].RegenSlot, Pickups[i].ID  );
#endif
	}

	Pickups[ i ].Type = INDEX_NONE;
	Pickups[ i ].Prev = INDEX_NONE;
	Pickups[ i ].Next = FirstPickupFree;
	FirstPickupFree	= i;

	RemovePickupFromGroup( i, Pickups[ i ].Group );
}

/*===================================================================
	Procedure	:	Clean up and kill a Pickup
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void CleanUpPickup( u_int16_t i )
{

	/* remove pickup from global counter */
	NumPickupType[ Pickups[ i ].Type ]--;

#if DEBUG_PICKUPS
	if( NumPickupType[ Pickups[ i ].Type ] < 0 )
	{
		DebugPrintf( "pickups: Killed more %s's than allowed\n", Messages[ Pickups[ i ].Type ] );
	}
#endif

	/* kill the pickups light */
	if( Pickups[ i ].Light != INDEX_NONE )
	{
		/* cause a light to go red and get smaller then die */
		SetLightDie( Pickups[ i ].Light );
		Pickups[ i ].Light = INDEX_NONE;
	}

	/* kill the pickups faceme poly */
	if ( Pickups[ i ].Fmpoly != INDEX_NONE )
	{
		/* Kill a used FmPoly and move it from the used list to the free list */
		KillUsedFmPoly( Pickups[ i ].Fmpoly );
		Pickups[ i ].Fmpoly = INDEX_NONE;
	}

	/* kill the pickups model */
	if ( Pickups[ i ].ModelNum != INDEX_NONE )
	{
		/* Kill a used Model and move it from the used list to the free list */
		KillUsedModel( Pickups[ i ].ModelNum );
		Pickups[ i ].ModelNum = INDEX_NONE;
	}

	/* kill and remove pickup from used list to free list */
	KillUsedPickup( i );

}

/*===================================================================
	Procedure	:	Init Pickup
	Input		:	VECTOR	*	Pos
				:	u_int16_t		Group
				:	VECTOR	*	Dir
				:	float		Speed
				:	int16_t		Type
				:	u_int16_t		Owner
				:	u_int16_t		ID
				:	int16_t		Regen Slot
				:	bool		Twinkle Into life?
				:	float		LifeCount
				:	u_int16_t		TriggerMod Index
	Output		:	u_int16_t		-1 if none free
				:				-2 if too many of same type
===================================================================*/
u_int16_t PickupPool::init_one( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod )
{
	u_int16_t	i;
	u_int16_t	light;
	u_int16_t	fmpoly;
	u_int16_t	model;

	if( !DebugInfo && ( MyGameStatus != STATUS_SinglePlayer ) )
	{
		if( NumPickupType[ Type ] >= MaxPickupType[ Type ] )
		{
			switch( Type )
			{
			case PICKUP_Mugs:
			case PICKUP_Shield:
			case PICKUP_GeneralAmmo:
			case PICKUP_SussGunAmmo:
			case PICKUP_PyroliteAmmo:
				break;

			case PICKUP_Bounty:
			case PICKUP_Flag:
			case PICKUP_Flag1:
			case PICKUP_Flag2:
			case PICKUP_Flag3:
			case PICKUP_Flag4:
				KillAllPickupsOfType( Type, PICKUPKILL_Immediate );
				break;

			default:
#if DEBUG_PICKUPS
				DebugPrintf( "pickups: Tried to generated more %s's than allowed\n", Messages[ Type ] );
#endif
				return( (u_int16_t) -2 );
				break;
			}
		}
	}

	i = FindFreePickup();

	if( i != INDEX_NONE )
	{
		if( RegenSlot != -1 )
		{
			if( RegenSlot < 0 || RegenSlot >= NumRegenPoints )
			{
				// EvaluateMessage() MSG_VERYSHORTDROPPICKUP sends 255
				RegenSlot = -1;	// if trying to use bad slot then use no slot.
			}
			else if ( RegenPoints[ RegenSlot ].Status == PU_REGENSTAT_Used )
			{
				RegenSlot = -1;	// if trying to use used slot then use no slot.
			}
		}

		AddPickupToGroup( i, Group );

		if( Sparkle )
		{
			PlayPannedSfx( SFX_Respawn, Group , Pos, 0.0F );
			Pickups[ i ].Mode = PICKUPMODE_Appear;
		}
		else Pickups[ i ].Mode = PICKUPMODE_Normal;
		Pickups[ i ].Speed = Speed;					//PICKUP_SPEED;
		Pickups[ i ].ExternalSpeed = 0.0F;
		Pickups[ i ].LifeCount = LifeCount;
		Pickups[ i ].PickupTime = 0.0F;
		Pickups[ i ].PickupCount = 0.0F;			//16.0F;
		Pickups[ i ].Type = (int8_t) Type;
		Pickups[ i ].Owner = Owner;
		Pickups[ i ].Dir = *Dir;
		Pickups[ i ].Rot.x = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].Rot.y = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].Rot.z = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].ID = ID;
		Pickups[ i ].Pos = *Pos;
		Pickups[ i ].DirVector.x = 0.0F;
		Pickups[ i ].DirVector.y = 0.0F;
		Pickups[ i ].DirVector.z = 1.0F;
		Pickups[ i ].ColFlag = 0;
		Pickups[ i ].Group = Group;
		Pickups[ i ].RegenSlot = RegenSlot;
		Pickups[ i ].ModelType = PickupAttribs[ Type ].ModelType;
		Pickups[ i ].TriggerMod = TriggerMod;
		if( TriggerMod == INDEX_NONE ) Pickups[ i ].TriggerModPtr = nullptr;
		else Pickups[ i ].TriggerModPtr = &TrigMods[ TriggerMod ];
		Pickups[ i ].DirQuat.From2Vectors(&Forward, &Pickups[ i ].DirVector);
		Pickups[ i ].DirQuat.ToMatrix(&Pickups[ i ].Mat);			// Create Dir Matrix

		NumPickupType[ Pickups[ i ].Type ]++;

		if( RegenSlot != -1 )
		{
			if( RegenSlot < NumRegenPoints )
			{
				RegenPoints[ RegenSlot ].Status = PU_REGENSTAT_Used;
				RegenPoints[ RegenSlot ].Wait = 0.0F;
				RegenPoints[ RegenSlot ].PickupIndex = i;
				RegenPoints[ RegenSlot ].PickupID = ID;
#if DEBUG_PICKUPS
				if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Initialised '%s' in slot %d. Owner '%s', ID %d\n", Messages[ Type ], RegenSlot, &Names[ Owner ][ 0 ], ID );
				else DebugPrintf( "pickups: Initialised '%s' in slot %d. No Owner, ID %d\n", Messages[ Type ], RegenSlot, ID );
#endif
			}
			else
			{
#if DEBUG_PICKUPS
				if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Tried to init '%s', Owner '%s', ID %d, in regen slot grater than max slots\n", Messages[ Type ], &Names[ Owner ][ 0 ], ID );
				else DebugPrintf( "pickups: Tried to init '%s', No Owner, ID %d, in regen slot grater than max slots\n", Messages[ Type ], ID );
#endif
			}
		}
		else
		{
#if DEBUG_PICKUPS
			if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Initialised '%s' in slot %d, Owner '%s', ID %d\n", Messages[ Type ], RegenSlot, &Names[ Owner ][ 0 ], ID );
			else DebugPrintf( "pickups: Initialised '%s' in slot %d, No Owner, ID %d\n", Messages[ Type ], RegenSlot, ID );
#endif
		}

		light = INDEX_NONE;
		model = INDEX_NONE;

		if( PickupAttribs[ Type ].Light != 0 )
		{
			light = FindFreeXLight();
			if( light != INDEX_NONE )
			{
				XLights[ light ].Pos = *Pos;
				XLights[ light ].Size = PickupAttribs[ Type ].Lightsize;
				XLights[ light ].SizeCount = 0.0F;
				XLights[ light ].r = PickupAttribs[ Type ].R;
				XLights[ light ].g = PickupAttribs[ Type ].G;
				XLights[ light ].b = PickupAttribs[ Type ].B;
				XLights[ light ].Group = Pickups[ i ].Group;
			}
		}
		Pickups[ i ].Light = light;

		model =	FindFreeModel();
		if( model != INDEX_NONE )
		{
			Models[ model ].OwnerType = OWNER_SHIP;
			Models[ model ].Owner = Pickups[ i ].Owner;
			Models[ model ].ModelNum = Pickups[ i ].ModelType;
			Models[ model ].Type = MODTYPE_Pickup;
			Models[ model ].Flags = MODFLAG_Clip;
			if( Sparkle ) Models[ model ].Visible = false;
			else Models[ model ].Visible = true;
			Models[ model ].Pos = *Pos;
			Models[ model ].Mat = Pickups[ i ].Mat;
			Models[ model ].Scale = 1.0F;
			Models[ model ].Func = MODFUNC_Nothing;
			Models[ model ].Group = Pickups[ i ].Group;

			MatrixTranspose( &Models[ model ].Mat, &Models[ model ].InvMat );
		}

		if( Sparkle )
		{
			Pickups[ i ].PickupCount = 32.0F;

			fmpoly = FindFreeFmPoly();
			if( fmpoly != INDEX_NONE )
			{
				FmPolys[ fmpoly ].LifeCount = 1000.0F;
				FmPolys[ fmpoly ].Pos = *Pos;
				FmPolys[ fmpoly ].SeqNum = FM_PICKUP_REGEN;
				FmPolys[ fmpoly ].Frame = 0.0F;
				FmPolys[ fmpoly ].Frm_Info = &PickupRegen_Header;
				FmPolys[ fmpoly ].Flags = FM_FLAG_MOVEOUT;
				FmPolys[ fmpoly ].Trans = 192;
				FmPolys[ fmpoly ].Dir.x = 0.0F;
				FmPolys[ fmpoly ].Dir.y = 0.0F;
				FmPolys[ fmpoly ].Dir.z = 0.0F;
				FmPolys[ fmpoly ].R = 192;
				FmPolys[ fmpoly ].G = 192;
				FmPolys[ fmpoly ].B = 192;
				FmPolys[ fmpoly ].Speed = 0.0F;
				FmPolys[ fmpoly ].Rot = 0.0F;
				FmPolys[ fmpoly ].Group = Pickups[ i ].Group;
				FmPolys[ fmpoly ].RotSpeed = 0.0F;
				FmPolys[ fmpoly ].xsize = PICKUP_REGEN_SIZE;
				FmPolys[ fmpoly ].ysize = PICKUP_REGEN_SIZE;
				AddFmPolyToTPage( fmpoly, GetTPage( *FmPolys[ fmpoly ].Frm_Info, 0 ) );
			}
		}

		Pickups[ i ].ModelNum = model;
		Pickups[ i ].Fmpoly = INDEX_NONE;

		// check flag position
		if ( CTF )
		{
			int team;

			team = -1;
			switch ( Pickups[ i ].Type )
			{
			case PICKUP_Flag1:
				team = 0;
				break;
			case PICKUP_Flag2:
				team = 1;
				break;
			case PICKUP_Flag3:
				team = 2;
				break;
			case PICKUP_Flag4:
				team = 3;
				break;
			}
			if ( team >= 0 )
			{
				Ships[ WhoIAm ].Object.Flags &= ~TeamFlagMask[ team ];
				switch ( GoalCheckTeam( &Pickups[ i ].Pos, &Pickups[ i ].Pos, Pickups[ i ].Group, team ) )
				{
				case GOAL_SCORED:
					TeamFlagAtHome[ team ] = true;
					break;
				default:
					TeamFlagAtHome[ team ] = false;
				}
			}
		}

		if ( BountyHunt && Pickups[ i ].Type == PICKUP_Bounty )
			Ships[ WhoIAm ].Object.Flags &= ~SHIP_CarryingBounty;

		if ( CaptureTheFlag && Pickups[ i ].Type == PICKUP_Flag )
			Ships[ WhoIAm ].Object.Flags &= ~SHIP_CarryingFlag;
	}
	else
	{
#if DEBUG_PICKUPS
		if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Couldn't Initialise '%s' in slot %d. Owner '%s', ID %d\n", Messages[ Type ], RegenSlot, &Names[ Owner ][ 0 ], ID );
		else DebugPrintf( "pickups: Couldn't Initialise '%s' in slot %d. No Owner, ID %d\n", Messages[ Type ], RegenSlot, ID );
#endif
	}

	return i;
}

/*===================================================================
	Procedure	:	Init Joining Pickup
	Input		:	VECTOR	*	Pos
				:	u_int16_t		Group
				:	VECTOR	*	Dir
				:	float		Speed
				:	int16_t		Type
				:	u_int16_t		Owner
				:	u_int16_t		ID
				:	int16_t		Regen Slot
				:	bool		Twinkle Into life?
				:	float		LifeCount
				:	u_int16_t		TriggerMod Index
	Output		:	u_int16_t		-1 if none free
				:				-2 if too many of same type
===================================================================*/
u_int16_t InitJoinPickup( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod )
{
	u_int16_t	i;
	u_int16_t	light;
	u_int16_t	fmpoly;
	u_int16_t	model;

	i = FindFreePickup();

	if( i != INDEX_NONE )
	{

		AddPickupToGroup( i, Group );

		if( Sparkle ) Pickups[ i ].Mode = PICKUPMODE_Appear;
		else Pickups[ i ].Mode = PICKUPMODE_Normal;
		Pickups[ i ].Speed = Speed;					//PICKUP_SPEED;
		Pickups[ i ].ExternalSpeed = 0.0F;
		Pickups[ i ].LifeCount = LifeCount;
		Pickups[ i ].PickupCount = 0.0F;			//16.0F;
		Pickups[ i ].Type = (int8_t) Type;
		Pickups[ i ].Owner = Owner;
		Pickups[ i ].Dir = *Dir;
		Pickups[ i ].Rot.x = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].Rot.y = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].Rot.z = ( ( (float) Random_Range( 256 ) ) / 64.0F ) - 2.0F;
		Pickups[ i ].ID = ID;
		Pickups[ i ].Pos = *Pos;
		Pickups[ i ].DirVector.x = 0.0F;
		Pickups[ i ].DirVector.y = 0.0F;
		Pickups[ i ].DirVector.z = 1.0F;
		Pickups[ i ].ColFlag = 0;
		Pickups[ i ].Group = Group;
		Pickups[ i ].RegenSlot = RegenSlot;
		Pickups[ i ].ModelType = PickupAttribs[ Type ].ModelType;
		Pickups[ i ].TriggerMod = TriggerMod;
		if( TriggerMod == INDEX_NONE ) Pickups[ i ].TriggerModPtr = nullptr;
		else Pickups[ i ].TriggerModPtr = &TrigMods[ TriggerMod ];
		Pickups[ i ].DirQuat.From2Vectors(&Forward, &Pickups[ i ].DirVector);
		Pickups[ i ].DirQuat.ToMatrix(&Pickups[ i ].Mat);			// Create Dir Matrix

		NumPickupType[ Pickups[ i ].Type ]++;
		if( RegenSlot != -1 )
		{
			RegenPoints[ RegenSlot ].Status = PU_REGENSTAT_Used;
			RegenPoints[ RegenSlot ].Wait = 0.0F;
			RegenPoints[ RegenSlot ].PickupIndex = i;
			RegenPoints[ RegenSlot ].PickupID = ID;
		}

		light = INDEX_NONE;
		model = INDEX_NONE;

		if( PickupAttribs[ Type ].Light != 0 )
		{
			light = FindFreeXLight();
			if( light != INDEX_NONE )
			{
				XLights[ light ].Pos = *Pos;
				XLights[ light ].Size = PickupAttribs[ Type ].Lightsize;
				XLights[ light ].SizeCount = 0.0F;
				XLights[ light ].r = PickupAttribs[ Type ].R;
				XLights[ light ].g = PickupAttribs[ Type ].G;
				XLights[ light ].b = PickupAttribs[ Type ].B;
				XLights[ light ].Group = Pickups[ i ].Group;
			}
		}
		Pickups[ i ].Light = light;

		model =	FindFreeModel();
		if( model != INDEX_NONE )
		{
			Models[ model ].OwnerType = OWNER_SHIP;
			Models[ model ].Owner = Pickups[ i ].Owner;
			Models[ model ].ModelNum = Pickups[ i ].ModelType;
			Models[ model ].Type = MODTYPE_Pickup;
			Models[ model ].Flags = MODFLAG_Clip;
			if( Sparkle ) Models[ model ].Visible = false;
			else Models[ model ].Visible = true;
			Models[ model ].Pos = *Pos;
			Models[ model ].Mat = Pickups[ i ].Mat;
			Models[ model ].Scale = 1.0F;
			Models[ model ].Func = MODFUNC_Nothing;
			Models[ model ].Group = Pickups[ i ].Group;

			MatrixTranspose( &Models[ model ].Mat, &Models[ model ].InvMat );
		}

		if( Sparkle )
		{
			Pickups[ i ].PickupCount = 32.0F;

			fmpoly = FindFreeFmPoly();
			if( fmpoly != INDEX_NONE )
			{
				FmPolys[ fmpoly ].LifeCount = 1000.0F;
				FmPolys[ fmpoly ].Pos = *Pos;
				FmPolys[ fmpoly ].SeqNum = FM_PICKUP_REGEN;
				FmPolys[ fmpoly ].Frame = 0.0F;
				FmPolys[ fmpoly ].Frm_Info = &PickupRegen_Header;
				FmPolys[ fmpoly ].Flags = FM_FLAG_MOVEOUT;
				FmPolys[ fmpoly ].Trans = 192;
				FmPolys[ fmpoly ].Dir.x = 0.0F;
				FmPolys[ fmpoly ].Dir.y = 0.0F;
				FmPolys[ fmpoly ].Dir.z = 0.0F;
				FmPolys[ fmpoly ].R = 192;
				FmPolys[ fmpoly ].G = 192;
				FmPolys[ fmpoly ].B = 192;
				FmPolys[ fmpoly ].Speed = 0.0F;
				FmPolys[ fmpoly ].Rot = 0.0F;
				FmPolys[ fmpoly ].Group = Pickups[ i ].Group;
				FmPolys[ fmpoly ].RotSpeed = 0.0F;
				FmPolys[ fmpoly ].xsize = PICKUP_REGEN_SIZE;
				FmPolys[ fmpoly ].ysize = PICKUP_REGEN_SIZE;
				AddFmPolyToTPage( fmpoly, GetTPage( *FmPolys[ fmpoly ].Frm_Info, 0 ) );
			}
		}

		Pickups[ i ].ModelNum = model;
		Pickups[ i ].Fmpoly = INDEX_NONE;
	}
	else
	{
#if DEBUG_PICKUPS
		if( Owner != INDEX_NONE ) DebugPrintf( "pickups: Couldn't Initialise '%s' in slot %d. Owner '%s', ID %d\n", Messages[ Type ], RegenSlot, &Names[ Owner ][ 0 ], ID );
		else DebugPrintf( "pickups: Couldn't Initialise '%s' in slot %d. No Owner, ID %d\n", Messages[ Type ], RegenSlot, ID );
#endif
	}
	return i;
}

/*===================================================================
	Procedure	:	Process Pickups
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void PickupPool::process()
{
	u_int16_t		i;
	u_int16_t		NextPickup;
	u_int16_t		OldGroup;
	float		Speed;
	QUAT		TempQuat;
	VECTOR		TempVector;
	VECTOR		NewDir;
	VECTOR		FrameLag_Rot;
	int16_t		WantedPowerPods;
	int16_t		EnabledPlayers = 0;
	int16_t		NumPowerPodsGot = 0;
	VECTOR		NewPos;
	VECTOR		OldPos;

	ProcessFailedKills();

	for( i = 0; i < MAX_PLAYERS; i++ )
	{
		if( ( Ships[ i ].enable ) && ( ( GameStatus[ i ] == STATUS_Normal ) || ( GameStatus[ i ] == STATUS_SinglePlayer) ) )
		{
			EnabledPlayers++;
			NumPowerPodsGot += ( Ships[ i ].Object.PowerLevel );
		}
	}

	WantedPowerPods = ( 2 + EnabledPlayers );
	if( IsHost ) NumPowerPods = ( WantedPowerPods - ( NumPickupType[ PICKUP_PowerPod ] + NumPowerPodsGot + NumPowerPods ) );
	MaxPickupType[ PICKUP_PowerPod ] = WantedPowerPods;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		NextPickup = Pickups[ i ].Prev;							/* Next Pickup */

		Pickups[ i ].PickupTime -= framelag;
		if( Pickups[ i ].PickupTime < 0.0F )
		{
			Pickups[ i ].PickupTime = 0.0F;
		}

		switch( Pickups[ i ].Mode )
		{
		case PICKUPMODE_Appear:
			Pickups[ i ].PickupCount -= framelag;
			if( Pickups[ i ].PickupCount <= 0.0F )
			{
				if( Pickups[ i ].ModelNum != INDEX_NONE )
				{
					Models[ Pickups[ i ].ModelNum ].Visible = true;
				}
				Pickups[ i ].PickupCount = 0.0F;
				Pickups[ i ].Mode = PICKUPMODE_Normal;
			}
			break;

		case PICKUPMODE_Normal:
			if( ( ( Pickups[ i ].Owner == WhoIAm ) ) ||
				( ( ( Pickups[ i ].Owner == INDEX_NONE ) && ( IsHost ) ) ) ||
				( ( ( Pickups[ i ].Owner != INDEX_NONE ) && !Ships[ Pickups[ i ].Owner ].enable && ( IsHost ) ) ) )
			{
				if( Pickups[ i ].LifeCount != -1.0F )
				{
					Pickups[ i ].LifeCount -= framelag;
					if( Pickups[ i ].LifeCount <= 0.0F )
					{
						{
#if DEBUG_PICKUPS
							DebugPrintf( "pickups: Tell %s to dissapear\n", Messages[ Pickups[ i ].Type ] );
#endif
						}

						KillPickupSend( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Disappear );
						KillPickup( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Disappear );

						i = NextPickup;
					}
				}
			}
			break;

		case PICKUPMODE_Disappear:
			Pickups[ i ].PickupCount -= framelag;
			if( Pickups[ i ].PickupCount <= 0.0F )
			{
				if( ( ( Pickups[ i ].Owner == WhoIAm ) ) ||
					( ( ( Pickups[ i ].Owner == INDEX_NONE ) && ( IsHost ) ) ) ||
					( ( ( Pickups[ i ].Owner != INDEX_NONE ) && !Ships[ Pickups[ i ].Owner ].enable && ( IsHost ) ) ) )
				{
					{
#if DEBUG_PICKUPS
						DebugPrintf( "pickups: Kill %s immediatly\n", Messages[ Pickups[ i ].Type ] );
#endif
					}

					AddPickupToRegen( Pickups[ i ].Type );
					KillPickupSend( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
					KillPickup( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
					i = NextPickup;
					break;
				}
				else
				{
					if( Pickups[ i ].PickupCount < -( ANIM_SECOND * 5.0F ) )
					{
						KillPickup( Pickups[ i ].Owner, Pickups[ i ].ID, PICKUPKILL_Immediate );
						i = NextPickup;
					}
				}
			}
			break;

		default:
			break;
		}

		if( i != NextPickup )
		{
			OldPos = Pickups[ i ].Pos;

			if( ( Pickups[ i ].Speed != 0.0F ) || ( Pickups[ i ].ExternalSpeed != 0.0F ) )
			{
				Speed = ( Pickups[ i ].Speed * framelag );
				Speed += ( Pickups[ i ].ExternalSpeed * framelag );

				NewDir.x = ( Pickups[ i ].Dir.x * Speed );
				NewDir.y = ( Pickups[ i ].Dir.y * Speed );
				NewDir.z = ( Pickups[ i ].Dir.z * Speed );

				OldGroup = Pickups[ i ].Group;

				/*===================================================================
					Do first collision, when initialised or changed direction
				===================================================================*/
				NewPos.x = ( Pickups[ i ].Pos.x + NewDir.x );
				NewPos.y = ( Pickups[ i ].Pos.y + NewDir.y );
				NewPos.z = ( Pickups[ i ].Pos.z + NewDir.z );

				if( QCollide( &Pickups[ i ].Pos, Pickups[ i ].Group, &NewDir, PICKUP_MODEL_RADIUS,
							  (VECTOR *) &Pickups[ i ].ColPoint, &Pickups[ i ].Group, &Pickups[ i ].ColPointNormal ) )
				{
					TempVector = Pickups[i].Dir;
					NormaliseVector( &TempVector );
					ReflectVector( &TempVector, &Pickups[i].ColPointNormal, &Pickups[i].Dir );

					Pickups[ i ].Speed *= 0.75F;
					Pickups[ i ].ExternalSpeed *= 0.75F;

					NewPos.x = Pickups[i].ColPoint.x;
					NewPos.y = Pickups[i].ColPoint.y;
					NewPos.z = Pickups[i].ColPoint.z;
				}

				if( Pickups[ i ].Speed > PICKUP_SPEED ) Pickups[ i ].Speed = PICKUP_SPEED;
				if( Pickups[ i ].ExternalSpeed > PICKUP_SPEED ) Pickups[ i ].ExternalSpeed = PICKUP_SPEED;

				Pickups[ i ].Pos = NewPos;
				if( Pickups[ i ].Speed > 0.0F ) Pickups[ i ].Speed -= ( PICKUP_DEC * framelag );
				if( Pickups[ i ].Speed < 0.0F ) Pickups[ i ].Speed = 0.0F;
				if( Pickups[ i ].ExternalSpeed > 0.0F ) Pickups[ i ].ExternalSpeed -= ( PICKUP_DEC * framelag );
				if( Pickups[ i ].ExternalSpeed < 0.0F ) Pickups[ i ].ExternalSpeed = 0.0F;

				if( OldGroup != Pickups[ i ].Group )
				{
					MovePickupToGroup( i, OldGroup, Pickups[ i ].Group );
				}

				if( Pickups[ i ].Light != INDEX_NONE )									/* Light Attached? */
				{
					XLights[ Pickups[ i ].Light ].Pos = Pickups[ i ].Pos;				/* Update Light Position */
					XLights[ Pickups[ i ].Light ].Group = Pickups[ i ].Group;
					if( XLights[ Pickups[ i ].Light ].Type == SPOT_LIGHT )
					{
						ApplyMatrix( &Pickups[ i ].Mat, &Forward, &XLights[ Pickups[ i ].Light ].Dir );
						NormaliseVector( &XLights[ Pickups[ i ].Light ].Dir );
					}
				}

				if( Pickups[ i ].Fmpoly != INDEX_NONE )								/* Faceme Poly Attached? */
				{
					FmPolys[ Pickups[ i ].Fmpoly ].Pos = Pickups[ i ].Pos;				/* Update FmPoly Position */
					FmPolys[ Pickups[ i ].Fmpoly ].Group = Pickups[ i ].Group;
				}

				if( Pickups[ i ].ModelNum != INDEX_NONE )								/* 3D Model Attached? */
				{
					Models[ Pickups[ i ].ModelNum ].Pos = Pickups[ i ].Pos;
					Models[ Pickups[ i ].ModelNum ].Group = Pickups[ i ].Group;
				}
			}

			switch( Pickups[ i ].Type )
			{
			case PICKUP_Flag:
			case PICKUP_Flag1:
			case PICKUP_Flag2:
			case PICKUP_Flag3:
			case PICKUP_Flag4:
				if( Pickups[ i ].ModelNum != INDEX_NONE )								/* 3D Model Attached? */
				{
					Speed = ( LevelTimeTaken * 4.0F );
					Models[ Pickups[ i ].ModelNum ].Frame = (int16_t) ( FMOD( Speed, ( 2.5F * ANIM_SECOND ) ) / ( 0.25F * ANIM_SECOND ) );
					Models[ Pickups[ i ].ModelNum ].InterpFrame = ( Models[ Pickups[ i ].ModelNum ].Frame + 1 );
					if( Models[ Pickups[ i ].ModelNum ].InterpFrame == 10 ) Models[ Pickups[ i ].ModelNum ].InterpFrame = 0;
					Models[ Pickups[ i ].ModelNum ].InterpTime = FMOD( ( FMOD( Speed, ( 2.5F * ANIM_SECOND ) ) / ( 0.25F * ANIM_SECOND ) ), 1.0F );
				}
				if ( CTF )
				{
					int team;

					team = -1;
					switch ( Pickups[ i ].Type )
					{
					case PICKUP_Flag1:
						team = 0;
						break;
					case PICKUP_Flag2:
						team = 1;
						break;
					case PICKUP_Flag3:
						team = 2;
						break;
					case PICKUP_Flag4:
						team = 3;
						break;
					}
					if ( team >= 0 )
					{
						VECTOR Move;
						float MoveDist;

						switch ( GoalCheckTeam( &OldPos, &Pickups[ i ].Pos, Pickups[ i ].Group, team ) )
						{
						case GOAL_SCORED:
							if ( !TeamFlagAtHome[ team ] )
							{
								Move.x = Pickups[ i ].Pos.x - OldPos.x;
								Move.y = Pickups[ i ].Pos.y - OldPos.y;
								Move.z = Pickups[ i ].Pos.z - OldPos.z;
								MoveDist = VectorLength( &Move );
								if ( MoveDist > 0.0F )
									AddColourMessageToQue(FlagMessageColour, FLAG_DRIFTED_BACK, TeamName[ team ] );
								TeamFlagAtHome[ team ] = true;
							}
							break;
						default:
							if ( TeamFlagAtHome[ team ] )
							{
								Move.x = Pickups[ i ].Pos.x - OldPos.x;
								Move.y = Pickups[ i ].Pos.y - OldPos.y;
								Move.z = Pickups[ i ].Pos.z - OldPos.z;
								MoveDist = VectorLength( &Move );
								if ( MoveDist > 0.0F )
									AddColourMessageToQue(FlagMessageColour, FLAG_ESCAPED, TeamName[ team ] );
								TeamFlagAtHome[ team ] = false;
							}
						}
					}
				}
				break;
			default:
				break;
			}

			if( IsGroupVisible[ Pickups[i].Group ] )								// Don't Rotate if not visible
			{
				FrameLag_Rot.x = ( Pickups[i].Rot.x * framelag );
				FrameLag_Rot.y = ( Pickups[i].Rot.y * framelag );
				FrameLag_Rot.z = ( Pickups[i].Rot.z * framelag );
				TempQuat.MakeEuler(FrameLag_Rot.x, FrameLag_Rot.y, FrameLag_Rot.z);
				TempQuat.Multiply(&Pickups[ i ].DirQuat, &Pickups[ i ].DirQuat);
				Pickups[ i ].DirQuat.ToMatrix(&Pickups[ i ].Mat);				// Create Dir Matrix

				if( Pickups[ i ].ModelNum != INDEX_NONE )								/* 3D Model Attached? */
				{
					Models[ Pickups[ i ].ModelNum ].Mat = Pickups[ i ].Mat;				/* Update Model Matrix */
					MatrixTranspose( &Pickups[ i ].Mat, &Models[ Pickups[ i ].ModelNum ].InvMat );
					Models[ Pickups[ i ].ModelNum ].Pos = Pickups[ i ].Pos;
					Models[ Pickups[ i ].ModelNum ].Group = Pickups[ i ].Group;
				}
			}

			i = NextPickup;											/* Next Pickup */
		}
	}
}

/*===================================================================
	Procedure	:	Save Pickup Positions
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
char		NewFilename[ 128 ];

void PickupPool::save_positions()
{
	const char	*	NewExt = ".PIC";
	FILE	*	fp;
	u_int16_t		i;
	int16_t		Num_Pickups;
	u_int16_t		GenType = PU_GENTYPE_Initialised;
	u_int16_t		ReGenType = PU_REGENTYPE_Random;
	float		GenDelay = -1.0F;
	float		LifeSpan = -1.0F;
	u_int16_t		TriggerMod = INDEX_NONE;

	Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NewFilename[ 0 ], sizeof( NewFilename ), NewExt );

	fp = file_open( &NewFilename[ 0 ], "wb" );

	if( fp != nullptr )
	{
		i = FirstPickupUsed;
		Num_Pickups = 0;

		while( i != INDEX_NONE )
		{
			Num_Pickups++;
			i = Pickups[ i ].Prev;					/* Next Pickup */
		}

		fwrite( &Num_Pickups, sizeof( int16_t ), 1, fp );

		i = FirstPickupUsed;

		while( i != INDEX_NONE )
		{
			fwrite( &GenType, sizeof( u_int16_t ), 1, fp );
			fwrite( &ReGenType, sizeof( u_int16_t ), 1, fp );
			fwrite( &GenDelay, sizeof( float ), 1, fp );
			fwrite( &LifeSpan, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Pos.x, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Pos.y, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Pos.z, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Type, sizeof( u_int16_t ), 1, fp );
			fwrite( &TriggerMod, sizeof( u_int16_t ), 1, fp );

			i = Pickups[ i ].Prev;					/* Next Pickup */
		}

		fclose( fp );

		AddColourMessageToQue(SystemMessageColour, "Saved Pickup Positions: %s", &NewFilename[ 0 ] );
	}
	else
	{
		AddColourMessageToQue(SystemMessageColour, "Error Saving Pickup Positions: %s", &NewFilename[ 0 ] );
	}
}

/*===================================================================
	Procedure	:		Randomize Pickup Regen Slots
	Input		:		Nothing
	Output		:		Nothing
===================================================================*/
u_int16_t	CopyOfSeed1;
u_int16_t	CopyOfSeed2;

void RandomizePickups( void )
{
	int16_t	Count;
	int16_t	Slot1, Slot2;
	VECTOR	TempPos;
	u_int16_t	TempGroup;

	/* Shuffle the regen points around x number of times */
	for( Count = 0; Count < 1000; Count++ )
	{
		/* pick 2 random regen points */
		Slot1 = Random_Range( NumRegenPoints );
		Slot2 = Random_Range( NumRegenPoints );

		/* make sure they are differen't points */
		if( Slot1 == Slot2 )
			continue;

		/* copy location of slot2 into a temporary holder */
		TempGroup = RegenPoints[ Slot2 ].Group;
		TempPos   = RegenPoints[ Slot2 ].Pos;

		/* assign the position of slot 1 to slot 2 */
		RegenPoints[ Slot2 ].Group = RegenPoints[ Slot1 ].Group;
		RegenPoints[ Slot2 ].Pos   = RegenPoints[ Slot1 ].Pos;

		/* assign the temporary position to slot 1 */
		RegenPoints[ Slot1 ].Group = TempGroup;
		RegenPoints[ Slot1 ].Pos   = TempPos;

	}
}


/*===================================================================
	Procedure	:		Check and correct valid regen slots
	Input		:		int16_t	Slot
	Output		:		bool	True/False
===================================================================*/
bool CheckValidRegenSlot( int16_t Slot )
{
	/* if the position is inside the group */
	if( PointInsideSkin( &RegenPoints[ Slot ].Pos, RegenPoints[ Slot ].Group ) )
		return( true );

	/* set it to be used even though its really not
	   so we don't assign a pickup to it again */

	/* if you are host add pickup thats in the slot to regen list */
	if( ( IsHost ) )
		AddPickupToRegen( RegenPoints[ Slot ].Type );

	RegenPoints[ Slot ].Status = PU_REGENSTAT_Used;
	RegenPoints[ Slot ].Wait = 0.0F;
	RegenPoints[ Slot ].Type = INDEX_NONE;

	/* let people know the pic file is messed up! */
	DebugPrintf("pickups: Bad regen point detected! The PIC file needs to be fixed!\n");

	/* failed */
	return( false );

}


#ifdef EDITION_REMASTER
/*===================================================================
	Procedure	:	PreLoadPickups
	Input		:	Nothing
	Output		:	bool	True/False
	Notes		:	Registers the Remaster's N64 single-mesh pickup .mx
				:	files into fresh ModelNames[] slots and bumps
				:	NextNewModel past them so subsequent BG-object /
				:	enemy / etc. registrations don't collide. Called
				:	from oct2.cpp's OnceOnlyChangeLevel block, after
				:	PreLoadEnemies (which also advances NextNewModel)
				:	and before PreInitModel loads the models.
===================================================================*/
bool PickupPool::preload()
{

	static const char * const n64_pickup_files[5] = {
		"n64\\stcrys.mx",   /* PICKUP_StablizerCrystal (100) */
		"n64\\bhgun1.mx",   /* PICKUP_BlackHoleGunPart1 (102) */
		"n64\\bhgun2.mx",   /* PICKUP_BlackHoleGunPart2 (103) */
		"n64\\bhgun3.mx",   /* PICKUP_BlackHoleGunPart3 (104) */
		"n64\\bhgun4.mx",   /* PICKUP_BlackHoleGunPart4 (105) */
	};
	static const char * const n64_pickup_check_paths[5] = {
		"data\\models\\n64\\stcrys.mx",
		"data\\models\\n64\\bhgun1.mx",
		"data\\models\\n64\\bhgun2.mx",
		"data\\models\\n64\\bhgun3.mx",
		"data\\models\\n64\\bhgun4.mx",
	};

	int i;
	for (i = 0; i < 5; i++)
	{
		/* Skip cleanly if the .mx isn't present (e.g., the device's
		 * SD romfs predates the N64 mesh extraction). The pickup ID
		 * stays mapped to the 1998 template via the existing
		 * LoadPickupsPositions block, which only overrides ModelType
		 * if s_n64_pickup_model_idx[i] >= 0. */
		if (!File_Exists((char *)n64_pickup_check_paths[i]))
			continue;
		if (NextNewModel >= MAXMODELHEADERS)
			return false;
		copy_string( &ModelNames[ NextNewModel ].Name[0], sizeof( ModelNames[ NextNewModel ].Name ), n64_pickup_files[i] );
		ModelNames[ NextNewModel ].LOD             = 0;
		ModelNames[ NextNewModel ].Panel           = false;
		ModelNames[ NextNewModel ].DoIMorph        = false;
		ModelNames[ NextNewModel ].ModelIndex      = NextNewModel;
		ModelNames[ NextNewModel ].StoreTriangles  = false;
		ModelNames[ NextNewModel ].AllocateTpage   = LOAD_TPAGES;
		ModelNames[ NextNewModel ].LevelSpecific   = NOT_LEVEL_SPECIFIC;
		ModelNames[ NextNewModel ].LoadEnable      = true;
		s_n64_pickup_model_idx[i] = NextNewModel;
		NextNewModel++;
	}
	return true;
}
#endif /* EDITION_REMASTER */

#ifdef EDITION_REMASTER
/* Populate Remaster N64 pickup slots (100-105) by template-copying
 * from a similar 1998 pickup (light / glow attribs) once, then point
 * ModelType at the N64 mesh PreLoadPickups registered. PreLoadPickups
 * runs on every level change and the slot indices it gets depend on
 * the level's background and enemy models, so the ModelType overrides
 * are reapplied on every load, not only the first. */
void PickupPool::LoadPickupsN64Templates()
{
	static bool _n64_pickup_init_done = false;
	if (!_n64_pickup_init_done)
	{
		_n64_pickup_init_done = true;

		/* StablizerCrystal (100): glowing crystal pickup → Crystal template. */
		PickupAttribs[ PICKUP_StablizerCrystal ] = PickupAttribs[ PICKUP_Crystal ];

		/* ComputerCapsule (101): pickup capsule → Computer template
		 * (no real mesh extracted — not used by any level in the SP
		 * mission set, kept for completeness). */
		PickupAttribs[ PICKUP_ComputerCapsule ] = PickupAttribs[ PICKUP_Computer ];

		/* BlackHoleGun parts (102-105): weapon-component pickups → PowerPod
		 * template for light/glow. */
		PickupAttribs[ PICKUP_BlackHoleGunPart1 ] = PickupAttribs[ PICKUP_PowerPod ];
		PickupAttribs[ PICKUP_BlackHoleGunPart2 ] = PickupAttribs[ PICKUP_PowerPod ];
		PickupAttribs[ PICKUP_BlackHoleGunPart3 ] = PickupAttribs[ PICKUP_PowerPod ];
		PickupAttribs[ PICKUP_BlackHoleGunPart4 ] = PickupAttribs[ PICKUP_PowerPod ];
	}

	/* This level's model slots for the real N64 meshes. */
	if (s_n64_pickup_model_idx[0] >= 0)
		PickupAttribs[ PICKUP_StablizerCrystal ].ModelType = s_n64_pickup_model_idx[0];
	if (s_n64_pickup_model_idx[1] >= 0)
		PickupAttribs[ PICKUP_BlackHoleGunPart1 ].ModelType = s_n64_pickup_model_idx[1];
	if (s_n64_pickup_model_idx[2] >= 0)
		PickupAttribs[ PICKUP_BlackHoleGunPart2 ].ModelType = s_n64_pickup_model_idx[2];
	if (s_n64_pickup_model_idx[3] >= 0)
		PickupAttribs[ PICKUP_BlackHoleGunPart3 ].ModelType = s_n64_pickup_model_idx[3];
	if (s_n64_pickup_model_idx[4] >= 0)
		PickupAttribs[ PICKUP_BlackHoleGunPart4 ].ModelType = s_n64_pickup_model_idx[4];
}
#endif /* EDITION_REMASTER */

/* validate each regen point (inside its group, a pickup type this game allows) and set it up by generation type: placed now, after a delay, or on a trigger */
void PickupPool::LoadPickupsSetupRegenPoints()
{
	int16_t   Count;
	VECTOR  Dir = { 0.0F, 0.0F, 0.0F };

	for( Count = 0; Count < NumRegenPoints; Count++ )
	{

		/* if the regen point is outside of its group
		   it will be decalred as used and forgetten about
		   the pickup it holds will be added to the regenerate list */

		if( ! CheckValidRegenSlot( Count ) )
			continue;

		/* if pickup is NOT valid */
		if(
			!FilterPickup( RegenPoints[ Count ].Type ) &&
				   (!
					   (( ( RegenPoints[ Count ].Type == PICKUP_Flag) && CaptureTheFlag) ||
				 ( (RegenPoints[ Count ].Type == PICKUP_Bounty) && BountyHunt ))
					  )
			)

			continue;

		/* setup regen point
		   based on its GenType */

		switch( RegenPoints[ Count ].GenType )
		{

			/* Initialised Regen Point */
			/* AKA: Regen Points that have their pickups loaded at start */
		case PU_GENTYPE_Initialised:

			/* add one to global count for this type of pickup */
			MaxPickupType[ RegenPoints[ Count ].Type ]++;

			/* try to create the pickup */
			InitOnePickup(
				  &RegenPoints[ Count ].Pos,
				  RegenPoints[ Count ].Group,
				  &Dir,
				  0.0F,
				  RegenPoints[ Count ].Type,
				  INDEX_NONE,
				  ++Ships[WhoIAm].PickupIdCount,
				  Count,
				  false,
				  RegenPoints[ Count ].Lifespan,
				  RegenPoints[ Count ].TriggerMod
				);

			break;

			/* timed regen point */
			/* AKA: The Regen point will continously generate the pickup ever N seconds */
		case PU_GENTYPE_Time:

			/* apply the generation delay */
			RegenPoints[ Count ].GenDelay = ( RegenPoints[ Count ].GenDelay * ANIM_SECOND );

			/* add one to global count for this type of pickup */
			MaxPickupType[ RegenPoints[ Count ].Type ]++;

			/* ? */
			RegenPoints[ Count ].PickupID = ++Ships[WhoIAm].PickupIdCount;

			/* set the status of the regen point */
			RegenPoints[ Count ].Status = PU_REGENSTAT_TimeGen;

			/* set timer to regen event */
			RegenPoints[ Count ].Wait = RegenPoints[ Count ].GenDelay;

			break;

		case PU_GENTYPE_Trigger:

			/* add one to global count for this type of pickup */
			MaxPickupType[ RegenPoints[ Count ].Type ]++;

			/* ? */
			RegenPoints[ Count ].PickupID = ++Ships[WhoIAm].PickupIdCount;

			/* set the status of the regen point */
			RegenPoints[ Count ].Status = PU_REGENSTAT_TrigGen;

			break;
		}
	}
}

/* outside the bomb/flag modes: the minimum counts of general, pyrolite and suss-gun ammo (and CTF flags per team) that must always exist */
void PickupPool::LoadPickupsSetMinimums()
{
	int16_t   Count;
	u_int16_t  i;

	/* if were not in the blow modes */
	if(
		!(
		( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_SinglePlayer )			  ||
		( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer)
		)
		)
	{
		for( Count = 1; Count < MAXPRIMARYWEAPONS; Count++ )
		{
			i = PICKUP_Trojax + ( Count - 1 );
			if( ! FilterPickup( i ) )
				continue;

			if( MaxPickupType[ i ] < NumPrimaryPickups && ( MaxPickupType[ i ] != 0 ) )
			{
				MaxPickupType[ i ] = NumPrimaryPickups;

				if( IsHost )
					NumPrimWeapons[ Count ] += ( NumPrimaryPickups - NumPickupType [ i ] );
			}
		}
	}

	/* if the minimum ammount of general ammo to allways exist is lower then 3 */
	if( MaxPickupType[ PICKUP_GeneralAmmo ] < 3 )
		/* set the minimum of general ammo to to 3 */
		MaxPickupType[ PICKUP_GeneralAmmo ] = 3;

	/* if the minimum ammount of pryolite ammo to allways exist is 0 */
	/* and we currently have a pyro on the level */
	if( ( MaxPickupType[ PICKUP_PyroliteAmmo ] == 0 ) &&
		( MaxPickupType[ PICKUP_Pyrolite ] != 0 ) )
		/* set the minimum of pyro ammo that always exists to 2 */
		MaxPickupType[ PICKUP_PyroliteAmmo ] = 2;

	/* if the minimum ammount of suss gun ammo to allways exist is 0 */
	/* and we currently have a suss gun on the level */
	if( ( MaxPickupType[ PICKUP_SussGunAmmo ] == 0 ) &&
		( MaxPickupType[ PICKUP_SussGun ] != 0 ) )
		/* set the minimum of suss gun ammot that allways exists to 2 */
		MaxPickupType[ PICKUP_SussGunAmmo ] = 2;

	/* if we are in ctf mode */
	if ( CTF )
		/* for each team */
		for ( Count = 0; Count < MAX_TEAMS; Count++ )
			/* set the minimum ammount of flags for this team that allways has to exist to 1 */
			MaxPickupType[ TeamFlagPickup[ Count ] ] = 1;
}

/*===================================================================
	Procedure	:	Load Pickup Positions ( level .PIC regen points )
	Input		:	Nothing
	Output		:	bool	True/False
===================================================================*/
bool PickupPool::load_positions()
{

	/* the extension of the file with pickup data */
	const char  * NewExt = ".PIC";

	FILE    * fp;
	int16_t   Count;
	u_int32_t  MagicNumber;
	u_int32_t  VersionNumber;

	/* wipes out all pickups, so we can load new ones */
	KillAllPickups();


#ifdef EDITION_REMASTER
	LoadPickupsN64Templates();
#endif

	/* convert the level_name into level_name.pic */
	Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NewFilename[ 0 ], sizeof( NewFilename ), NewExt );

	/* reset number of pickups in groups to 0 */
	for( Count = 0; Count < MAXGROUPS; Count++ )
		NumPickupsPerGroup[ Count ] = 0;

	/* reset the number of each pickup to 0
	   and the max number of each to 1000 */
	for( Count = 0; Count < MAXPICKUPTYPES; Count++ )
	{
		NumPickupType[ Count ] = 0;
		MaxPickupType[ Count ] = 1000;
	}

	/* counter */
	NumRegenPoints = 0;

	/* reset number of pickups in current biker */
	Ships[WhoIAm].PickupIdCount = (u_int16_t) 0;

	/* get the size of the file */

	/* open the file for reading */
	fp = file_open( &NewFilename[ 0 ], "rb" );

	/* if the file handle is valid */
	if( fp != nullptr )
	{

		/* read in the magic number */
		file_read( &MagicNumber, sizeof( u_int32_t ), 1, fp );

		/* read in the pic version number */
		file_read( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		/* fail if the magic/version numbers are not proper */
		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != PIC_VERSION_NUMBER  ) )
		{
			fclose( fp );
			Msg( "LoadPickupsPositions() Incompatible pickup (.PIC) file %s", &NewFilename[ 0 ] );
			return( false );
		}

		/* read in the number of regen points contained in the file */
		file_read( &NumRegenPoints, sizeof( int16_t ), 1, fp );

		/* if global RegenPoints list of regen points IS set */
		if( RegenPoints != nullptr )
		{
			/* free the memory */
			free( RegenPoints );

			/* for each possible player */
			for( Count = 0; Count < MAX_PLAYERS; Count++ )

				/* if regen slot copy IS set */
				if( RegenSlotsCopy[ Count ] != nullptr )

					/* free the memory */
					free( RegenSlotsCopy[ Count ] );

		}

		/* allocate memory for the global which holds the regen points */
		RegenPoints = (REGENPOINT*)malloc( sizeof( REGENPOINT ) * NumRegenPoints );

		/* allocate memory for each regen slot copy */
		for( Count = 0; Count < MAX_PLAYERS; Count++ )
			RegenSlotsCopy[ Count ] = (REGENPOINT*)malloc( sizeof( REGENPOINT ) * NumRegenPoints );

		/* if the global is not NULL */
		if( RegenPoints != nullptr )
		{
			/* loop each regen point in the file */
			for( Count = 0; Count < NumRegenPoints; Count++ )
			{
				/* reset the regen point */
				RegenPoints[ Count ].Status = PU_REGENSTAT_Free;
				RegenPoints[ Count ].Wait = 0.0F;

				/* extract each field for the regen point */
				file_read( &RegenPoints[ Count ].GenType,  sizeof( u_int16_t ), 1, fp );
				file_read( &RegenPoints[ Count ].RegenType,  sizeof( u_int16_t ), 1, fp );
				file_read( &RegenPoints[ Count ].GenDelay,   sizeof( float ),  1, fp );
				file_read( &RegenPoints[ Count ].Lifespan,   sizeof( float ),  1, fp );
				file_read( &RegenPoints[ Count ].Pos.x,    sizeof( float ),  1, fp );
				file_read( &RegenPoints[ Count ].Pos.y,    sizeof( float ),  1, fp );
				file_read( &RegenPoints[ Count ].Pos.z,    sizeof( float ),  1, fp );
				file_read( &RegenPoints[ Count ].Group,    sizeof( u_int16_t ), 1, fp );
				file_read( &RegenPoints[ Count ].Type,     sizeof( u_int16_t ), 1, fp );
				file_read( &RegenPoints[ Count ].TriggerMod, sizeof( u_int16_t ), 1, fp );

				/* if pickup is not valid */
				if(
					/* if pickup is NOT a valid type */
					( RegenPoints[ Count ].Type >= MAXPICKUPTYPES ) ||
					/* or */
					(
					 /* triggerMod is set */
					 /* and is NOT valid */
					( RegenPoints[ Count ].TriggerMod != INDEX_NONE ) &&
					( RegenPoints[ Count ].TriggerMod >= NumOfTrigMods )
					)
					)
				{
#ifdef EDITION_REMASTER
					/* A Remaster .PIC regen point with a Type >= MAXPICKUPTYPES or a
					 * TriggerMod past NumOfTrigMods makes the non-Remaster build
					 * (#else) reject the whole file. Keep the level loadable: the
					 * point becomes PICKUP_GeneralAmmo (even when only the trigger
					 * mod was bad) and an invalid TriggerMod is cleared. */
					RegenPoints[ Count ].Type = PICKUP_GeneralAmmo;
					if (RegenPoints[ Count ].TriggerMod != INDEX_NONE &&
						RegenPoints[ Count ].TriggerMod >= NumOfTrigMods) {
						RegenPoints[ Count ].TriggerMod = INDEX_NONE;
					}
#else
					/* quit */
					Msg( "Error loading pickups\nIllegal pickup or trigger\n" );
					fclose( fp );
					return( false );
#endif
				}
			}
		}

		/* done reading file */
		if( !file_read_ok( fp ) )
		{
			fclose( fp );
			Msg( "LoadPickupsPositions() Damaged pickup (.PIC) file for %s\n", &ShortLevelNames[ LevelNum ][ 0 ] );
			return( false );
		}

		fclose( fp );

		/* if not single player */
		if( !( ( ChangeLevel_MyGameStatus == STATUS_PostStartingSinglePlayer ) ||
			 ( ChangeLevel_MyGameStatus == STATUS_SinglePlayer ) ||
			 ( ChangeLevel_MyGameStatus == STATUS_TitleLoadGamePostStartingSinglePlayer) ) )
			/* if randomize pickups is set and we have more than one regen point */
			if( RandomPickups && ( NumRegenPoints > 1 ) )
				/* shuffle around regen points */
				RandomizePickups();

		/* for each regeneration point */
		LoadPickupsSetupRegenPoints();
	}

	/* remove the 1000 headroom added above; it kept InitOnePickup's
	   NumPickupType >= MaxPickupType cap from refusing pickups while
	   the level was being seeded */

	for( Count = 0; Count < MAXPICKUPTYPES; Count++ )
		MaxPickupType[ Count ] -= 1000;

	LoadPickupsSetMinimums();

	/* finally done */
	return( true );

}

/*===================================================================
	Procedure	:		Initialise a pickup given only regen slot
	Input		:		u_int16_t		Slot Number
	Output		:		u_int16_t		Pickup Index (-1 or -2 error)
===================================================================*/
u_int16_t InitSlotPickup( u_int16_t Slot )
{
	u_int16_t		i = INDEX_NONE;
	VECTOR		Dir = { 0.0F, 0.0F, 0.0F };

	if( RegenPoints[ Slot ].Status != PU_REGENSTAT_TrigGen )
		return( INDEX_NONE );

	if( RegenPoints[ Slot ].GenDelay )
	{
		RegenPoints[ Slot ].GenDelay = ( RegenPoints[ Slot ].GenDelay * ANIM_SECOND );
		RegenPoints[ Slot ].Status = PU_REGENSTAT_TimeGen;
		RegenPoints[ Slot ].Wait = RegenPoints[ Slot ].GenDelay;
	}
	else
	{
		i =	InitOnePickup( &RegenPoints[ Slot ].Pos, RegenPoints[ Slot ].Group, &Dir, 0.0F, RegenPoints[ Slot ].Type,
							INDEX_NONE, RegenPoints[ Slot ].PickupID, Slot, true, -1.0F, RegenPoints[ Slot ].TriggerMod );
#if DEBUG_PICKUPS
		if( ( i == INDEX_NONE ) || ( i == (u_int16_t) -2 ) )
		{
			DebugPrintf( "pickups: Unable to initialise pickup in slot %d\n", Slot );
		}
#endif
	}
	return( i );
}

/*===================================================================
	Procedure	:	Regenerate required pickups
	Input		:	nothing
	Output		:	nothing
===================================================================*/
float	RegenDelay = 0.0F;

void PickupPool::regenerate()
{
	int16_t				Count;
	VECTOR				Dir = { 0.0F, 0.0F, 1.0F };
	u_int16_t				i;

	if( RegenDelay != 0.0F )
	{
		RegenDelay -= framelag;
		if( RegenDelay < 0.0F ) RegenDelay = 0.0F;
	}

	RegenerateQuedPickups();

	/* for all regen locations
	   if location is a time based regen
	   check the time and regenerate it */

	for( Count = 0; Count < NumRegenPoints; Count++ )
	{
		if( ( RegenPoints[ Count ].Status == PU_REGENSTAT_Free ) &&
			( RegenPoints[ Count ].Wait > 0.0F ) )
		{
			RegenPoints[ Count ].Wait -= framelag;
			if( RegenPoints[ Count ].Wait < 0.0F ) RegenPoints[ Count ].Wait = 0.0F;
		}

		if( RegenPoints[ Count ].Status == PU_REGENSTAT_TimeGen )
		{
			RegenPoints[ Count ].Wait -= framelag;
			if( RegenPoints[ Count ].Wait < 0.0F )
			{
				if( ( IsHost ) )
				{
					i = InitOnePickup( &RegenPoints[ Count ].Pos, RegenPoints[ Count ].Group,
									   &Dir, 0.0F, RegenPoints[ Count ].Type, INDEX_NONE,
									   ++Ships[WhoIAm].PickupIdCount, Count, true,
									   RegenPoints[ Count ].Lifespan, RegenPoints[ Count ].TriggerMod );

					if( ( i != INDEX_NONE )  && ( i != (u_int16_t) -2 ) )
					{
						DropPickupSend( &RegenPoints[ Count ].Pos, RegenPoints[ Count ].Group,
										&Dir, 0.0F, RegenPoints[ Count ].Type, Ships[WhoIAm].PickupIdCount,
										Count, false, RegenPoints[ Count ].Lifespan, RegenPoints[ Count ].TriggerMod );
					}
				}

				RegenPoints[ Count ].Wait = 0.0F;
			}
		}
	}


	// if single player mode
	// or regen delay is at 0
	if(
		(MyGameStatus == STATUS_SinglePlayer) ||
		(RegenDelay != 0.0F)
		)
	{
		// quit
		return;
	}

	// regenerate ctf flags
	if ( CaptureTheFlag )
	{
		while (
			(FlagsToGenerate > 0) &&
			RegeneratePickup( PICKUP_Flag )
			)
			FlagsToGenerate--;
	}

	/* regenerate bounty */
	if ( BountyHunt )
	{
		while ( BountyToGenerate > 0 && RegeneratePickup( PICKUP_Bounty ) )
			BountyToGenerate--;
	}

	/* regenerate nitro */
	if( NitroFuelUsed >= ( 50.0F - 0.1F ) )	// Floating point inacuaracy !!!!!
	{
		if( RegeneratePickup( PICKUP_Nitro ) )
		{
			if( NitroFuelUsed <= 50.0F ) NitroFuelUsed = 0.0F;
			else NitroFuelUsed -= 50.0F;
		}
	}

	/* regen primaries */
	for( Count = 1; Count < MAXPRIMARYWEAPONS; Count++ )
	{
		// if this weapon should be regenerated
		if( NumPrimWeapons[ Count ] <= 0 ) continue;
		// get the weapon id
		u_int16_t weapon = PICKUP_Trojax + Count - 1;
		// regenerate weapon
		if ( ! RegeneratePickup( weapon ) ) continue;
		// decrement count to regenerate
		NumPrimWeapons[ Count ]--;
	}

	/* regen power pods */
	if(
		NumPowerPods > 0 &&
		RegeneratePickup( PICKUP_PowerPod )
		)
	{
		NumPowerPods--;
	}

	/* host controls regenerating ammo
	   if overall ammo is lower than the level minumum */

	if( IsHost )
	{
		if( NumPickupType[ PICKUP_GeneralAmmo ] < MaxPickupType[ PICKUP_GeneralAmmo ] )
		{
			RegeneratePickup( PICKUP_GeneralAmmo );
		}

		if( NumPickupType[ PICKUP_SussGunAmmo ] < MaxPickupType[ PICKUP_SussGunAmmo ] )
		{
			RegeneratePickup( PICKUP_SussGunAmmo );
		}

		if( NumPickupType[ PICKUP_PyroliteAmmo ] < MaxPickupType[ PICKUP_PyroliteAmmo ] )
		{
			RegeneratePickup( PICKUP_PyroliteAmmo );
		}
	}

	/* regenerate golden */
	if( NumSuperNashrams )
	{
		if( RegeneratePickup( PICKUP_GoldenPowerPod ) ) NumSuperNashrams--;
	}

	if( NumStealths )
	{
		if( RegeneratePickup( PICKUP_Mantle ) ) NumStealths--;
	}

	if( NumInvuls )
	{
		if( RegeneratePickup( PICKUP_Inv ) ) NumInvuls--;
	}

	if( NumOrbs )
	{
		if( RegeneratePickup( PICKUP_Orb ) ) NumOrbs--;
	}

	if( SecAmmoUsed[ SOLARISMISSILE ] >= 3 )
	{
		if( RegeneratePickup( PICKUP_HeatseakerPickup ) )
			SecAmmoUsed[ SOLARISMISSILE ] -= 3;
	}

	if( SecAmmoUsed[ THIEFMISSILE ] )
	{
		if( RegeneratePickup( PICKUP_Thief ) ) SecAmmoUsed[ THIEFMISSILE ] --;
	}

	if( SecAmmoUsed[ SCATTERMISSILE ] )
	{
		if( RegeneratePickup( PICKUP_Scatter ) ) SecAmmoUsed[ SCATTERMISSILE ]--;
	}

	if( SecAmmoUsed[ GRAVGONMISSILE ] )
	{
		if( RegeneratePickup( PICKUP_Gravgon ) ) SecAmmoUsed[ GRAVGONMISSILE ]--;
	}

	if( SecAmmoUsed[ MULTIPLEMISSILE ] >= 50  )
	{
		if( RegeneratePickup( PICKUP_Launcher ) ) SecAmmoUsed[ MULTIPLEMISSILE ] -= 50;
	}

	if( SecAmmoUsed[ TITANSTARMISSILE ] )
	{
		if( RegeneratePickup( PICKUP_TitanStar ) ) SecAmmoUsed[ TITANSTARMISSILE ]--;
	}

	if( SecAmmoUsed[ PURGEMINE ] >= 3 )
	{
		if( RegeneratePickup( PICKUP_PurgePickup ) ) SecAmmoUsed[ PURGEMINE ] -= 3;
	}

	if( SecAmmoUsed[ PINEMINE ] >= 3 )
	{
		if( RegeneratePickup( PICKUP_PinePickup ) ) SecAmmoUsed[ PINEMINE ] -= 3;
	}

	if( SecAmmoUsed[ QUANTUMMINE ] )
	{
		if( RegeneratePickup( PICKUP_QuantumPickup ) ) SecAmmoUsed[ QUANTUMMINE ]--;
	}

	if( SecAmmoUsed[ SPIDERMINE ] >= 3 )
	{
		if( RegeneratePickup( PICKUP_SpiderPod ) ) SecAmmoUsed[ SPIDERMINE ]--;
	}

}

/*===================================================================
	Procedure	:	Regenerate qued pickups
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void RegenerateQuedPickups( void )
{
	int16_t						Count;
	SHORTPICKUP				*	IP_Ptr;
	SHORTKILLPICKUP			*	KP_Ptr;

	if(
		(!Ships[ WhoIAm ].Pickups)		&&
		(!Ships[ WhoIAm ].RegenSlots)	&&
		(NumMissedPickups)
		)
	{
		for( Count = 0; Count < NumMissedPickups; Count++ )
		{
			switch( MissedPickups[ Count ].Type )
			{
			case MPU_TYPE_INIT:
				IP_Ptr = (SHORTPICKUP *) MissedPickups[ Count ].Struct;
#if DEBUG_PICKUPS
				DebugPrintf( "pickups: QUED INIT :\n" );
#endif
				InitOnePickup( &IP_Ptr->Pos, IP_Ptr->Group,
							   &IP_Ptr->Dir, IP_Ptr->Speed,
							   IP_Ptr->Type, IP_Ptr->Owner,
							   IP_Ptr->ID, IP_Ptr->RegenSlot,
							   IP_Ptr->Sparkle, IP_Ptr->LifeCount,
							   IP_Ptr->TriggerMod );
				break;

			case MPU_TYPE_KILL:
				KP_Ptr = (SHORTKILLPICKUP *) MissedPickups[ Count ].Struct;
#if DEBUG_PICKUPS
				DebugPrintf( "pickups: QUED KILL :\n" );
#endif
				KillPickup( KP_Ptr->Owner, KP_Ptr->ID, KP_Ptr->Style );
				break;
			}
		}

		NumMissedPickups = 0;
		NumMissedInitPickups = 0;
		NumMissedKillPickups = 0;
	}
}

/*===================================================================
	Procedure	:	Regenerate one pickup of the given type
	Input		:	u_int16_t		Pickup Type
	Output		:	bool		True/False
===================================================================*/

bool RegeneratePickup( u_int16_t Type )
{
	int16_t				Slot;
	u_int16_t				i;
	VECTOR				Dir = { 0.0F, 0.0F, 0.0F };
	int16_t				FreeSlots[ MAXPICKUPS ];
	int16_t				NumFreeSlots;
	int16_t				FreeTypeSlots[ MAXPICKUPS ];
	int16_t				NumFreeTypeSlots;

	NumFreeSlots = 0;
	NumFreeTypeSlots = 0;

	/* locate all free regen points
	   if any regen points cator specifically to the given pickup type
	   then exclusively use them instead of random points */

	/* for each regen point */
	for( Slot = 0; Slot < NumRegenPoints; Slot++ )
	{

		/* if the regen point is NOT free continue */

		if( !( ( RegenPoints[ Slot ].Status == PU_REGENSTAT_Free ) &&
			   ( RegenPoints[ Slot ].Wait == 0.0F ) ))
			continue;

		/* if regen point can only accept one type of pickup */

		if( RegenPoints[ Slot ].RegenType == PU_REGENTYPE_Const )
		{

			/* if given pickup type is alloud here */
			if( RegenPoints[ Slot ].Type == Type )
			{
				/* then add this regen point to the list of free slots */
				FreeTypeSlots[ NumFreeTypeSlots ] = Slot;
				NumFreeTypeSlots++;
			}
		}

		/* if regen location can accept any type of pickup */
		/* skip this if type specific regen locations found for this type */

		else if ( ! NumFreeTypeSlots )
		{

			/* add the location to the free slot list */
			FreeSlots[ NumFreeSlots ] = Slot;
			NumFreeSlots++;

		}

	}

	/* if there is regen points which only hold our specific type of pickup
	   then use them exclusively for this current pickup */

	if( NumFreeTypeSlots )
	{

		/* pick a random slot */
		Slot = FreeTypeSlots[ Random_Range( NumFreeTypeSlots ) ];

	}


	/* There are no regen points which only cator the current pickup
	   So use any pickup location that doesn't have a defined type */

	else
	{

		/* pick a random location */
		Slot = FreeSlots[ Random_Range( NumFreeSlots ) ];

		/* if slots less than 5 ditch out */
		if( NumFreeSlots < 5 )
			return false;

	}

	/* Try to init a pickup */
	i = InitOnePickup( &RegenPoints[ Slot ].Pos, RegenPoints[ Slot ].Group, &Dir,
					   0.0F, Type, WhoIAm, ++Ships[WhoIAm].PickupIdCount, Slot,
					   true, -1.0F, INDEX_NONE );

	/* the location given is occupied */
	if ( i == INDEX_NONE )
	{
		DebugPrintf("pickups: RegeneratePickup (FAILED) location given is occupied.\n");
		return false;
	}

	/* too many of same type */
	if( i == (u_int16_t) -2 ){
		DebugPrintf("pickups: RegeneratePickup (FAILED) too many of same type.\n");
		return true; /* dont complain */
	}

	/* tell others in networked game */
	RegenDelay = ( ANIM_SECOND * 0.5F );
	DropPickupSend( &RegenPoints[ Slot ].Pos, RegenPoints[ Slot ].Group, &Dir,
					0.0F, Type, Ships[WhoIAm].PickupIdCount, Slot, true, -1.0F,
					INDEX_NONE );

	/* Success ! */
	return true;

}

/*===================================================================
	Procedure	:	Generate pickup list for new player
	Input		:	u_int16_t			Ship
				:	SHORTPICKUP	*	Pickup Array to be filled in
				:	BYTE		*	Pickup Number to be filled in
				:	BYTE			Section of pickups
	Output		:	Nothing
===================================================================*/
void GenPickupList( u_int16_t Ship, SHORTPICKUP * PickupSlots, BYTE * NumPickups, BYTE Section )
{
	int16_t	Count;
	int16_t	i;
	int16_t	NumUsed = 0;

	Section = ( ( ( MAXPICKUPS + ( MAXGENPICKUPCOUNT - 1 ) ) / MAXGENPICKUPCOUNT ) - Section );

	for( Count = 0; Count < MAXGENPICKUPCOUNT; Count++ )
	{
		i = ( Count + ( Section * MAXGENPICKUPCOUNT ) );

		if( i < MAXPICKUPS )
		{
			if( PickupsCopy[ Ship ][ i ].Type != INDEX_NONE )
			{
				PickupSlots[ NumUsed ].Type = PickupsCopy[ Ship ][ i ].Type;
				PickupSlots[ NumUsed ].Group = PickupsCopy[ Ship ][ i ].Group;
				PickupSlots[ NumUsed ].Pos = PickupsCopy[ Ship ][ i ].Pos;
				PickupSlots[ NumUsed ].Owner = PickupsCopy[ Ship ][ i ].Owner;
				PickupSlots[ NumUsed ].ID = PickupsCopy[ Ship ][ i ].ID;
				PickupSlots[ NumUsed ].Dir = PickupsCopy[ Ship ][ i ].Dir;
				PickupSlots[ NumUsed ].Speed = PickupsCopy[ Ship ][ i ].Speed;
				PickupSlots[ NumUsed ].RegenSlot = PickupsCopy[ Ship ][ i ].RegenSlot;
				PickupSlots[ NumUsed ].Sparkle = false;
				PickupSlots[ NumUsed ].LifeCount = PickupsCopy[ Ship ][ i ].LifeCount;
				PickupSlots[ NumUsed ].TriggerMod = PickupsCopy[ Ship ][ i ].TriggerMod;
				NumUsed++;
			}
		}
	}

	*NumPickups = (BYTE) NumUsed;
}

/*===================================================================
	Procedure	:	Generate pickups from list for new player
	Input		:	SHORTPICKUP	*	Pickup Array
				:	BYTE			Number of Pickups
	Output		:	Nothing
===================================================================*/
void RegenPickupList( SHORTPICKUP * Slots, BYTE Num )
{
	int16_t	Count;

	for( Count = 0; Count < Num; Count++ )
	{
		InitJoinPickup( &Slots[ Count ].Pos, Slots[ Count ].Group, &Slots[ Count ].Dir, Slots[ Count ].Speed,
						Slots[ Count ].Type, Slots[ Count ].Owner, Slots[ Count ].ID, Slots[ Count ].RegenSlot,
						Slots[ Count ].Sparkle, Slots[ Count ].LifeCount, Slots[ Count ].TriggerMod );
	}
}

/*===================================================================
	Procedure	:	Generate RegenSlot list for new player
	Input		:	u_int16_t			Ship
				:	SHORTREGENSLOT*	RegenSlot Array to be filled in
				:	BYTE		*	RegenSlot Number to be filled in
				:	BYTE			Section of RegenSlot
	Output		:	Nothing
===================================================================*/
void GenRegenSlotList( u_int16_t Ship, SHORTREGENSLOT * RegenSlots, BYTE * NumRegenSlots, BYTE Section )
{
	int16_t	Count;
	int16_t	i;
	int16_t	NumUsed = 0;

#if DEBUG_PICKUPS
	DebugPrintf( "pickups: Generate RegenSlot List : %d Regen Slots\n", NumRegenPoints );
#endif

	for( Count = 0; Count < MAXGENREGENSLOTCOUNT; Count++ )
	{
		i = ( Count + ( ( Section - 1 ) * MAXGENREGENSLOTCOUNT ) );

		if( i < NumRegenPoints )
		{
			memmove( RegenSlots, ( RegenSlotsCopy[ Ship ] ) + i, sizeof( REGENPOINT ) );//memcpy
			RegenSlots++;
			NumUsed++;

#if DEBUG_PICKUPS
			DebugPrintf( "pickups: Section %d, Index %d\n", ( Section - 0 ), i );
#endif
		}
	}

	*NumRegenSlots = (BYTE) NumUsed;
}

/*===================================================================
	Procedure	:	Regeneralte RegenSlots from list for new player
	Input		:	SHORTREGENSLOT*	RegenSlot Array
				:	BYTE			Number of RegenSlots
	Output		:	Nothing
===================================================================*/
void RegenRegenSlotList( SHORTREGENSLOT * Slots, BYTE Num )
{
	REGENPOINT	*	Ptr;

#if DEBUG_PICKUPS
	DebugPrintf( "pickups: Regenerate RegenSlot List : %d Regen Slots\n", NumRegenPoints );
#endif

	if( RegenPoints )
	{
		Ptr = RegenPoints + ( ( Ships[ WhoIAm ].RegenSlots + 0 ) * MAXGENREGENSLOTCOUNT );

#if DEBUG_PICKUPS
		DebugPrintf( "pickups: Section %d, Number %d\n", ( Ships[ WhoIAm ].RegenSlots + 0 ), Num );
#endif
		memmove( Ptr, Slots, ( sizeof( REGENPOINT ) * Num ) );//memcpy
	}
}

/*===================================================================
	Procedure	:	Kill All Pickups
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/

void PickupPool::kill_all()
{
	u_int16_t	i;
	u_int16_t	NextPickup;

	/* get the first pickup */
	i = FirstPickupUsed;

	/* if pointer points to a valid pickup id */
	while( i != INDEX_NONE )
	{

		/* get the next pickup */
		NextPickup = Pickups[ i ].Prev;

		/* Reset the pickups triggerMod */
		Pickups[ i ].TriggerModPtr = nullptr;
		Pickups[ i ].TriggerMod    = INDEX_NONE;

		/* cleanup the pickup
		   remove any lighting or other affect the pickup does */

		CleanUpPickup( i );

		/* go to the next pickup */
		i = NextPickup;

	}
}

/*===================================================================
	Procedure	:	Copy Pickups into copy of pickups
	Input		:	u_int16_t		Player
	Output		:	Nothing
===================================================================*/
void PickupPool::copy( u_int16_t Player )
{
	u_int16_t	i;
	int16_t	Num = 0;

	for( i = 0; i < MAXPICKUPS; i++ )
	{
		PickupsCopy[ Player ][ i ].Type = INDEX_NONE;
	}

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		memmove( &PickupsCopy[ Player ][ Num ], &Pickups[i], sizeof( PICKUP ) );//memcpy
		Num++;

		i = Pickups[ i ].Prev;							/* Next Secondary Bullet */
	}
}

/*===================================================================
	Procedure	:	Copy RegenSlots into copy of regenslots
	Input		:	u_int16_t		Player
	Output		:	Nothing
===================================================================*/
void CopyRegenSlots( u_int16_t Player )
{
	REGENPOINT	*	Ptr;

	Ptr = RegenSlotsCopy[ Player ];

	if( Ptr != nullptr )
	{
		memmove( Ptr, RegenPoints, ( NumRegenPoints * sizeof( REGENPOINT ) ) );//memcpy
	}
	else
	{
#if DEBUG_PICKUPS
		DebugPrintf( "pickups: No regen point copy slot!\n" );
#endif
	}
}

/*===================================================================
	Procedure	:	Add Pickup to regeneration que
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void PickupPool::add_to_regen( u_int16_t Type )
{
	switch( Type )
	{
	case PICKUP_Trojax:
		NumPrimWeapons[ TROJAX ]++;
		break;

	case PICKUP_Pyrolite:
		NumPrimWeapons[ PYROLITE_RIFLE ]++;
		break;

	case PICKUP_Transpulse:
		NumPrimWeapons[ TRANSPULSE_CANNON ]++;
		break;

	case PICKUP_SussGun:
		NumPrimWeapons[ SUSS_GUN ]++;
		break;

	case PICKUP_Laser:
		NumPrimWeapons[ LASER ]++;
		break;

	case PICKUP_PowerPod:
		NumPowerPods++;
		break;

	case PICKUP_GoldenPowerPod:
		NumSuperNashrams++;
		break;

	case PICKUP_PurgePickup:
		SecAmmoUsed[ PURGEMINE ] += 3;
		break;

	case PICKUP_PinePickup:
		SecAmmoUsed[ PINEMINE ] += 3;
		break;

	case PICKUP_QuantumPickup:
		SecAmmoUsed[ QUANTUMMINE ] += 1;
		break;

	case PICKUP_SpiderPod:
		SecAmmoUsed[ SPIDERMINE ] += 3;
		break;

	case PICKUP_Gravgon:
		SecAmmoUsed[ GRAVGONMISSILE ] += 1;
		break;

	case PICKUP_TitanStar:
		SecAmmoUsed[ TITANSTARMISSILE ] += 1;
		break;

	case PICKUP_Scatter:
		SecAmmoUsed[ SCATTERMISSILE ] += 1;
		break;

	case PICKUP_Thief:
		SecAmmoUsed[ THIEFMISSILE ] += 1;
		break;

	case PICKUP_Launcher:
		SecAmmoUsed[ MULTIPLEMISSILE ] += 50;
		break;

	case PICKUP_HeatseakerPickup:
		SecAmmoUsed[ SOLARISMISSILE ] += 3;
		break;

	case PICKUP_Mugs:
		SecAmmoUsed[ MUGMISSILE ] += 3;
		break;

	case PICKUP_Mantle:
		NumStealths++;
		break;

	case PICKUP_Inv:
		NumInvuls++;
		break;

	case PICKUP_Orb:
		NumOrbs++;
		break;
	}
}


/*===================================================================
	Procedure	:	Find Closest Ship
	Input		:	Nothing
	Output		:	u_int16_t		Ship Index ( -1 If none hit )
===================================================================*/
u_int16_t FindClosestShip( void )
{
	u_int16_t		Count;
	VECTOR		TempVector;
	float		DistToSphere;
	u_int16_t		ClosestShip = INDEX_NONE;
	float		ClosestLength = 0.0f;

	for( Count = 0; Count < MAX_PLAYERS; Count++ )
	{
		if ( (Ships[Count].enable ) && (Ships[Count].Object.Mode != LIMBO_MODE) && ((GameStatus[Count] == STATUS_Normal )||(GameStatus[Count] == STATUS_SinglePlayer ) )&& ( Count != WhoIAm ) )
		{
			if( Ships[ Count ].Object.Mode == NORMAL_MODE )
			{
				TempVector.x = ( Ships[ Count ].Object.Pos.x - Ships[ WhoIAm ].Object.Pos.x );
				TempVector.y = ( Ships[ Count ].Object.Pos.y - Ships[ WhoIAm ].Object.Pos.y );
				TempVector.z = ( Ships[ Count ].Object.Pos.z - Ships[ WhoIAm ].Object.Pos.z );

				DistToSphere = VectorLength( &TempVector );

				if( ClosestShip != INDEX_NONE )
				{
					if( DistToSphere < ClosestLength )
					{
						ClosestLength = DistToSphere;
						ClosestShip = Count;
					}
				}
				else
				{
					ClosestLength = DistToSphere;
					ClosestShip = Count;
				}
			}
		}
	}

	return( ClosestShip );
}


/*===================================================================
	Procedure	:	Play Sfx for Collect Pickup from scene
	Input		:	u_int16_t	pickup to get
	Output		:	bool	True/False
===================================================================*/
void PickupPool::sfx_for_collect( u_int16_t Owner, u_int16_t ID )
{
	u_int16_t	i;
	u_int16_t	NextPickup;

	float	Sfx_Volume = 1.0F;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		NextPickup = Pickups[ i ].Prev;							/* Next Pickup */

		if( ( Pickups[i].Owner == Owner ) && ( Pickups[i].ID == ID ) )
		{
			switch( Pickups[i].Type )
			{
			case PICKUP_Trojax:
				PlaySfx( SFX_Select_Trojax, Sfx_Volume );
				return;
			case PICKUP_Pyrolite:
				PlaySfx( SFX_Select_Pyrolite, Sfx_Volume );
				return;
			case PICKUP_Transpulse:
				PlaySfx( SFX_Select_Transpulse, Sfx_Volume );
				return;
			case PICKUP_SussGun:
				PlaySfx( SFX_Select_SussGun, Sfx_Volume );
				return;
			case PICKUP_Laser:
				PlaySfx( SFX_Select_BeamLaser, Sfx_Volume );
				return;
			case PICKUP_Mug:
				return;
			case PICKUP_Mugs:
				PlaySfx( SFX_Select_MugMissile, Sfx_Volume );
				return;
			case PICKUP_Heatseaker:
				return;
			case PICKUP_HeatseakerPickup:
				PlaySfx( SFX_Select_SolarisMissile, Sfx_Volume );
				return;
			case PICKUP_Thief:
				return;
			case PICKUP_Scatter:
				PlaySfx( SFX_Select_ScatterMissile, Sfx_Volume );
				return;
			case PICKUP_Gravgon:
				PlaySfx( SFX_Select_GravgonMissile, Sfx_Volume );
				return;
			case PICKUP_Launcher:
				PlaySfx( SFX_Select_MFRL, Sfx_Volume );
				return;
			case PICKUP_TitanStar:
				PlaySfx( SFX_Select_TitanStarMissile, Sfx_Volume );
				return;
			case PICKUP_PurgePickup:
				PlaySfx( SFX_Select_PurgeMine, Sfx_Volume );
				return;
			case PICKUP_PinePickup:
				PlaySfx( SFX_Select_PineMine, Sfx_Volume );
				return;
			case PICKUP_QuantumPickup:
				PlaySfx( SFX_Select_QuantumMine, Sfx_Volume );
				return;
			case PICKUP_SpiderPod:
				return;
			case PICKUP_Parasite:
				return;
			case PICKUP_Flare:
				return;
			case PICKUP_GeneralAmmo:
				PlaySfx( SFX_Select_Ammo, Sfx_Volume );
				return;
			case PICKUP_PyroliteAmmo:
				PlaySfx( SFX_Select_Ammo, Sfx_Volume );
				return;
			case PICKUP_SussGunAmmo:
				PlaySfx( SFX_Select_Ammo, Sfx_Volume );
				return;
			case PICKUP_PowerPod:
				PlaySfx( SFX_Select_PowerPod, Sfx_Volume );
				return;
			case PICKUP_GoldenPowerPod:
				PlaySfx( SFX_Select_GoldenPowerPod, Sfx_Volume );
				return;
			case PICKUP_Shield:
				PlaySfx( SFX_Select_Shield, Sfx_Volume );
				return;
			case PICKUP_Inv:
				PlaySfx( SFX_Select_Invul, Sfx_Volume );
				return;
			case PICKUP_ExtraLife:
				return;
			case PICKUP_Computer:
				return;
			case PICKUP_Smoke:
				return;
			case PICKUP_Nitro:
				PlaySfx( SFX_Select_Nitro, Sfx_Volume );
				return;
			case PICKUP_Goggles:
				return;
			case PICKUP_Gold:
				return;
			case PICKUP_Mantle:
				PlaySfx( SFX_Cloaking, Sfx_Volume );
				return;
			case PICKUP_Crystal:
				return;
			case PICKUP_Orb:
				PlaySfx( SFX_Orbital, Sfx_Volume );
				return;
			case PICKUP_DNA:
				return;
			case PICKUP_SkeletonKey:
				return;
			case PICKUP_Bomb:
				return;
			case PICKUP_GoldFigure:
				return;
			}
		}

		i = NextPickup;											/* Next Pickup */
	}
}

/*===================================================================
	Procedure	:	Setup Pickup group link list
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void SetupPickupGroups( void )
{
	int16_t	Count;

	for( Count = 0; Count < MAXGROUPS; Count++ )
	{
		PickupGroups[ Count ] = nullptr;
		NumPickupsPerGroup[ Count ] = 0;
	}
}

/*===================================================================
	Procedure	:	Add Pickup to group link list
	Input		:	u_int16_t		Pickup Index
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void AddPickupToGroup( u_int16_t i, u_int16_t Group )
{
	Pickups[ i ].PrevInGroup = nullptr;
	Pickups[ i ].NextInGroup = PickupGroups[ Group ];
	if( Pickups[ i ].NextInGroup ) Pickups[ i ].NextInGroup->PrevInGroup = &Pickups[ i ];
	PickupGroups[ Group ] = &Pickups[ i ];
	NumPickupsPerGroup[ Group ]++;
}

/*===================================================================
	Procedure	:	Remove Pickup from group link list
	Input		:	u_int16_t		Pickup Index
				:	u_int16_t		Group
	Output		:	Nothing
===================================================================*/
void RemovePickupFromGroup( u_int16_t i, u_int16_t Group )
{
	if( Pickups[ i ].PrevInGroup ) Pickups[ i ].PrevInGroup->NextInGroup = Pickups[ i ].NextInGroup;
	else PickupGroups[ Group ] = Pickups[ i ].NextInGroup;
	if( Pickups[ i ].NextInGroup ) Pickups[ i ].NextInGroup->PrevInGroup = Pickups[ i ].PrevInGroup;
	Pickups[ i ].PrevInGroup = nullptr;
	Pickups[ i ].NextInGroup = nullptr;
	NumPickupsPerGroup[ Group ]--;
}

/*===================================================================
	Procedure	:	Move Pickup from 1 group to another
	Input		:	u_int16_t		Pickup Index
				:	u_int16_t		OldGroup
				:	u_int16_t		NewGroup
	Output		:	Nothing
===================================================================*/
void MovePickupToGroup( u_int16_t i, u_int16_t OldGroup, u_int16_t NewGroup )
{
	RemovePickupFromGroup( i, OldGroup );
	AddPickupToGroup( i, NewGroup );
}

void InitValidPickups()
{
	int i;

	for ( i = 0; i < MAXPICKUPTYPES; i++ )
	{
		PickupValid[ i ] = true;
	}
}

/*===================================================================
	Procedure	:	Filter Pickup
	Input		:	u_int16_t		Pickup Type
	Output		:	bool		( True/False ( Allowed/Not )
===================================================================*/
bool FilterPickup( u_int16_t PickupType )
{
	if ( PickupType >= MAXPICKUPTYPES )
		return false;

	// if in these modes
	if( ( ChangeLevel_MyGameStatus == STATUS_StartingMultiplayer ) ||
		( ChangeLevel_MyGameStatus == STATUS_WaitingAfterScore ) ||
		( ChangeLevel_MyGameStatus == STATUS_PreStartingMultiplayerSynch ) ||
		( ChangeLevel_MyGameStatus == STATUS_StartingMultiplayerSynch ) ||
		( ChangeLevel_MyGameStatus == STATUS_ChangeLevelPostInitView )
		)
	{
		// valid pickup type
		if( PickupValid[ PickupType ] )
		{
			// allow
			return( true );
		}
		else
		{
			// deny
			return( false );
		}
	}

	// allow
	return( true );
}

void PackPickupInfo( u_int32_t *packed )
{
	u_int8_t i;
	u_int32_t temp;
	int psize = sizeof( packed[ 0 ] ) * 8;

	for ( i = 0; i < MAX_PICKUPFLAGS; i++ )
	{
		packed[ i ] = 0;
	}
	for ( i = 0; i < MAXPICKUPTYPES; i++ )
	{
		if ( PickupValid[ i ] )
		{
			/* @vfp-exempt R11 — network pickup-state bit-packing, per network tick bounded by pickup count, not per frame. measured n/a. Verified 2026-10-25. */
			temp = ( 1 << ( i % psize ) );
			/* @vfp-exempt R11 — network pickup-state bit-packing, per network tick bounded by pickup count, not per frame. measured n/a. Verified 2026-10-25. */
			packed[ i / psize ] |= temp;
		}
	}
}

void UnpackPickupInfo( u_int32_t *packed )
{
	u_int8_t i;
	int psize = sizeof( packed[ 0 ] ) * 8;

	for ( i = 0; i < MAXPICKUPTYPES; i++ )
	{
		/* @vfp-exempt R11 — network pickup-state bit-packing, per network tick bounded by pickup count, not per frame. measured n/a. Verified 2026-10-25. */
		PickupValid[ i ] = ( packed[ i / psize ] & ( 1 << ( i % psize ) ) ) ? true : false;
	}
}

/*===================================================================
	Procedure	:	Save Pickups Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * PickupPool::save( FILE * fp )
{
	u_int16_t	i;
	u_int16_t	TempIndex = INDEX_NONE;

	if( fp )
	{
		for( i = 0; i < MAXGROUPS; i++ )
		{
			fwrite( &NumPickupsPerGroup[ i ], sizeof( int16_t ), 1, fp );
			if( PickupGroups[ i ] != nullptr ) fwrite( &PickupGroups[ i ]->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
		}

		for( i = 0; i < MAXPICKUPTYPES; i++ )
		{
			fwrite( &NumPickupType[ i ], sizeof( int16_t ), 1, fp );
			fwrite( &MaxPickupType[ i ], sizeof( int16_t ), 1, fp );
			fwrite( &PickupsGot[ i ], sizeof( int16_t ), 1, fp );
			fwrite( &PickupValid[ i ], sizeof( bool ), 1, fp );
		}

		for( i = 0; i < MAXPRIMARYWEAPONS; i++ )
		{
			fwrite( &NumPrimWeapons[ i ], sizeof( int16_t ), 1, fp );
		}

		fwrite( &NitroFuel, sizeof( NitroFuel ), 1, fp );
		fwrite( &NitroFuelUsed, sizeof( NitroFuelUsed ), 1, fp );
		fwrite( &NumStealths, sizeof( int16_t ), 1, fp );
		fwrite( &NumInvuls, sizeof( int16_t ), 1, fp );
		fwrite( &NumSuperNashrams, sizeof( int16_t ), 1, fp );
		fwrite( &NumOrbs, sizeof( int16_t ), 1, fp );
		fwrite( &NumPowerPods, sizeof( int16_t ), 1, fp );
		fwrite( &PickupInvulnerability, sizeof( bool ), 1, fp );
		fwrite( &NumGoldBars, sizeof( int16_t ), 1, fp );
		fwrite( &FirstPickupUsed, sizeof( u_int16_t ), 1, fp );
		fwrite( &FirstPickupFree, sizeof( u_int16_t ), 1, fp );
		fwrite( &NumRegenPoints, sizeof( int16_t ), 1, fp );
		fwrite( &CrystalsFound, sizeof( CrystalsFound ), 1, fp );

		for( i = 0; i < NumRegenPoints; i++ )
		{
			fwrite( &RegenPoints[ i ].GenType, sizeof( u_int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].RegenType, sizeof( u_int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].GenDelay, sizeof( float ), 1, fp );
			fwrite( &RegenPoints[ i ].Lifespan, sizeof( float ), 1, fp );
			fwrite( &RegenPoints[ i ].Wait, sizeof( float ), 1, fp );
			fwrite( &RegenPoints[ i ].Status, sizeof( int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].Group, sizeof( int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &RegenPoints[ i ].Type, sizeof( u_int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].TriggerMod, sizeof( u_int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].PickupIndex, sizeof( u_int16_t ), 1, fp );
			fwrite( &RegenPoints[ i ].PickupID, sizeof( u_int16_t ), 1, fp );
		}

		i = FirstPickupUsed;

		while( i != INDEX_NONE )
		{
			fwrite( &Pickups[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			if( Pickups[i].NextInGroup ) fwrite( &Pickups[ i ].NextInGroup->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( Pickups[i].PrevInGroup ) fwrite( &Pickups[ i ].PrevInGroup->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Index, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Type, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Owner, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ID, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Mode, sizeof( int16_t ), 1, fp );
			fwrite( &Pickups[ i ].LifeCount, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].PickupCount, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Rot, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].Speed, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].ExternalSpeed, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ModelType, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ModelNum, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Mat, sizeof( MATRIX ), 1, fp );
			fwrite( &Pickups[ i ].Fmpoly, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Xsize, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Ysize, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].Light, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].Lightsize, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].R, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].G, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].B, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].DirQuat, sizeof( QUAT ), 1, fp );
			fwrite( &Pickups[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].RegenSlot, sizeof( int16_t ), 1, fp );
			fwrite( &Pickups[ i ].TriggerMod, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ColStart, sizeof( VECTOR ), 1, fp );
			fwrite( &Pickups[ i ].ColDist, sizeof( float ), 1, fp );
			fwrite( &Pickups[ i ].ColFlag, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ColGroup, sizeof( u_int16_t ), 1, fp );
			fwrite( &Pickups[ i ].ColPoint, sizeof( VERT ), 1, fp );
			fwrite( &Pickups[ i ].ColPointNormal, sizeof( NORMAL ), 1, fp );
			fwrite( &Pickups[ i ].CouldNotPickup, sizeof( bool ), 1, fp );
			i = Pickups[ i ].Prev;
		}

		i = FirstPickupFree;

		while( i != INDEX_NONE )
		{
			fwrite( &Pickups[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = Pickups[ i ].Next;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load Pickups Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * PickupPool::restore( FILE * fp )
{
	u_int16_t	i;
	u_int16_t	TempIndex;

	if( fp )
	{
		for( i = 0; i < MAXGROUPS; i++ )
		{
			file_read( &NumPickupsPerGroup[ i ], sizeof( int16_t ), 1, fp );
			file_read( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) PickupGroups[ i ] = &Pickups[ TempIndex ];
			else PickupGroups[ i ] = nullptr;
		}

		for( i = 0; i < MAXPICKUPTYPES; i++ )
		{
			file_read( &NumPickupType[ i ], sizeof( int16_t ), 1, fp );
			file_read( &MaxPickupType[ i ], sizeof( int16_t ), 1, fp );
			file_read( &PickupsGot[ i ], sizeof( int16_t ), 1, fp );
			file_read( &PickupValid[ i ], sizeof( bool ), 1, fp );
		}

		for( i = 0; i < MAXPRIMARYWEAPONS; i++ )
		{
			file_read( &NumPrimWeapons[ i ], sizeof( int16_t ), 1, fp );
		}

		file_read( &NitroFuel, sizeof( NitroFuel ), 1, fp );
		file_read( &NitroFuelUsed, sizeof( NitroFuelUsed ), 1, fp );
		file_read( &NumStealths, sizeof( int16_t ), 1, fp );
		file_read( &NumInvuls, sizeof( int16_t ), 1, fp );
		file_read( &NumSuperNashrams, sizeof( int16_t ), 1, fp );
		file_read( &NumOrbs, sizeof( int16_t ), 1, fp );
		file_read( &NumPowerPods, sizeof( int16_t ), 1, fp );
		file_read( &PickupInvulnerability, sizeof( bool ), 1, fp );
		file_read( &NumGoldBars, sizeof( int16_t ), 1, fp );
		file_read( &FirstPickupUsed, sizeof( u_int16_t ), 1, fp );
		file_read( &FirstPickupFree, sizeof( u_int16_t ), 1, fp );
		file_read( &NumRegenPoints, sizeof( int16_t ), 1, fp );
		file_read( &CrystalsFound, sizeof( CrystalsFound ), 1, fp );

		for( i = 0; i < NumRegenPoints; i++ )
		{
			file_read( &RegenPoints[ i ].GenType, sizeof( u_int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].RegenType, sizeof( u_int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].GenDelay, sizeof( float ), 1, fp );
			file_read( &RegenPoints[ i ].Lifespan, sizeof( float ), 1, fp );
			file_read( &RegenPoints[ i ].Wait, sizeof( float ), 1, fp );
			file_read( &RegenPoints[ i ].Status, sizeof( int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].Group, sizeof( int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].Pos, sizeof( VECTOR ), 1, fp );
			file_read( &RegenPoints[ i ].Type, sizeof( u_int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].TriggerMod, sizeof( u_int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].PickupIndex, sizeof( u_int16_t ), 1, fp );
			file_read( &RegenPoints[ i ].PickupID, sizeof( u_int16_t ), 1, fp );
		}

		i = FirstPickupUsed;

		while( i != INDEX_NONE )
		{
			file_read( &Pickups[ i ].Next, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			file_read( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) Pickups[ i ].NextInGroup = &Pickups[ TempIndex ];
			else Pickups[ i ].NextInGroup = nullptr;
			file_read( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) Pickups[ i ].PrevInGroup = &Pickups[ TempIndex ];
			else Pickups[ i ].PrevInGroup = nullptr;
			file_read( &Pickups[ i ].Index, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Type, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Owner, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].ID, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Mode, sizeof( int16_t ), 1, fp );
			file_read( &Pickups[ i ].LifeCount, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].PickupCount, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].Rot, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].Pos, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].Dir, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].Speed, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].ExternalSpeed, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].Group, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].ModelType, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].ModelNum, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Mat, sizeof( MATRIX ), 1, fp );
			file_read( &Pickups[ i ].Fmpoly, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Xsize, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].Ysize, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].Light, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].Lightsize, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].R, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].G, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].B, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].DirQuat, sizeof( QUAT ), 1, fp );
			file_read( &Pickups[ i ].DirVector, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].UpVector, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].RegenSlot, sizeof( int16_t ), 1, fp );
			file_read( &Pickups[ i ].TriggerMod, sizeof( u_int16_t ), 1, fp );
			if( Pickups[ i ].TriggerMod == INDEX_NONE ) Pickups[ i ].TriggerModPtr = nullptr;
			else Pickups[ i ].TriggerModPtr = &TrigMods[ Pickups[ i ].TriggerMod ];
			file_read( &Pickups[ i ].ColStart, sizeof( VECTOR ), 1, fp );
			file_read( &Pickups[ i ].ColDist, sizeof( float ), 1, fp );
			file_read( &Pickups[ i ].ColFlag, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].ColGroup, sizeof( u_int16_t ), 1, fp );
			file_read( &Pickups[ i ].ColPoint, sizeof( VERT ), 1, fp );
			file_read( &Pickups[ i ].ColPointNormal, sizeof( NORMAL ), 1, fp );
			file_read( &Pickups[ i ].CouldNotPickup, sizeof( bool ), 1, fp );
			i = Pickups[ i ].Prev;
		}

		i = FirstPickupFree;

		while( i != INDEX_NONE )
		{
			memset( &Pickups[ i ], 0, sizeof( PICKUP ) );
			Pickups[ i ].Prev = INDEX_NONE;
			Pickups[ i ].NextInGroup = nullptr;
			Pickups[ i ].PrevInGroup = nullptr;
			Pickups[ i ].Index = i;
			Pickups[ i ].LifeCount = -1.0F;
			Pickups[ i ].Mode = PICKUPMODE_Normal;
			Pickups[ i ].Type = INDEX_NONE;
			Pickups[ i ].Rot.x = -1.2F;
			Pickups[ i ].Rot.y = 2.0F;
			Pickups[ i ].Rot.z = 1.0F;
			Pickups[ i ].Group = 0;
			Pickups[ i ].DirVector = Forward;
			Pickups[ i ].UpVector = SlideUp;
			Pickups[ i ].RegenSlot = -1;
			Pickups[ i ].TriggerModPtr = nullptr;
			Pickups[ i ].TriggerMod = INDEX_NONE;
			Pickups[ i ].DirQuat.From2Vectors(&Forward, &Pickups[ i ].DirVector);
			Pickups[ i ].DirQuat.ToMatrix(&Pickups[ i ].Mat);

			file_read( &Pickups[ i ].Next, sizeof( u_int16_t ), 1, fp );
			i = Pickups[ i ].Next;
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Kill all pickups of a specific type
	Input		:	u_int16_t	Type
				:	int16_t	Style to kill
	Output		:	nothing
===================================================================*/
void PickupPool::kill_all_of_type( u_int16_t Type, int16_t Style )
{
	u_int16_t	i;
	u_int16_t	NextPickup;
	u_int16_t	fmpoly;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		NextPickup = Pickups[ i ].Prev;							/* Next Pickup */

		if( Pickups[ i ].Type == Type )
		{
			switch( Style )
			{
			case PICKUPKILL_Immediate:
				if ( CTF )
				{
					int team;

					team = -1;
					switch ( Pickups[ i ].Type )
					{
					case PICKUP_Flag1:
						team = 0;
						break;
					case PICKUP_Flag2:
						team = 1;
						break;
					case PICKUP_Flag3:
						team = 2;
						break;
					case PICKUP_Flag4:
						team = 3;
						break;
					}
					if ( team >= 0 )
					{
						TeamFlagAtHome[ team ] = false;
					}
				}
				CleanUpPickup( i );
				break;

			case PICKUPKILL_Disappear:
				if( Pickups[ i ].Mode == PICKUPMODE_Disappear ) break;

				Pickups[ i ].LifeCount = 0.0F;
				Pickups[ i ].Mode = PICKUPMODE_Disappear;
				Pickups[ i ].PickupCount = 32.0F;

				fmpoly = FindFreeFmPoly();
				if( fmpoly != INDEX_NONE )
				{
					FmPolys[ fmpoly ].LifeCount = 1000.0F;
					FmPolys[ fmpoly ].Pos = Pickups[ i ].Pos;
					FmPolys[ fmpoly ].SeqNum = FM_PICKUP_REGEN;
					FmPolys[ fmpoly ].Frame = 0.0F;
					FmPolys[ fmpoly ].Frm_Info = &PickupRegen_Header;
					FmPolys[ fmpoly ].Flags = FM_FLAG_MOVEOUT;
					FmPolys[ fmpoly ].Trans = 192;
					FmPolys[ fmpoly ].Dir.x = 0.0F;
					FmPolys[ fmpoly ].Dir.y = 0.0F;
					FmPolys[ fmpoly ].Dir.z = 0.0F;
					FmPolys[ fmpoly ].R = 192;
					FmPolys[ fmpoly ].G = 192;
					FmPolys[ fmpoly ].B = 192;
					FmPolys[ fmpoly ].Speed = 0.0F;
					FmPolys[ fmpoly ].Rot = 0.0F;
					FmPolys[ fmpoly ].Group = Pickups[ i ].Group;
					FmPolys[ fmpoly ].RotSpeed = 0.0F;
					FmPolys[ fmpoly ].xsize = PICKUP_REGEN_SIZE;
					FmPolys[ fmpoly ].ysize = PICKUP_REGEN_SIZE;
					AddFmPolyToTPage( fmpoly, GetTPage( *FmPolys[ fmpoly ].Frm_Info, 0 ) );
				}
				break;
			}
		}

		i = NextPickup;											/* Next Pickup */
	}
}


/*===================================================================
	Procedure	:	Check if model for pickup is valid
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void PickupPool::model_valid()
{
	u_int16_t	i;
	u_int16_t	NextPickup;
	u_int16_t	Model;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		NextPickup = Pickups[ i ].Prev;							/* Next Pickup */

		Model = Pickups[ i ].ModelNum;

		if( Model != INDEX_NONE )
		{
			if( Models[ Model ].ModelNum != PickupAttribs[ Pickups[ i ].Type ].ModelType )
			{
				Msg( "Model for Pickup %s invalid ( %s )\n", Messages[ Pickups[ i ].Type ], &ModelNames[ Models[ Model ].ModelNum ].Name[ 0 ] );
			}
		}

		i = NextPickup;											/* Next Pickup */
	}
}


int16_t		FirstFailedKillUsed = -1;
int16_t		FirstFailedKillFree = -1;
FAILEDKILL	FailedKill[ MAXFAILEDKILLS ];

/*===================================================================
	Procedure	:	Init FailedKill Slots
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void InitFailedKillSlots( void )
{
	int16_t	i;

	FirstFailedKillUsed = -1;
	FirstFailedKillFree = 0;

	for( i = 0; i < MAXFAILEDKILLS; i++ )
	{
		memset( &FailedKill[ i ], 0, sizeof( FAILEDKILL ) );
		FailedKill[ i ].Next = i + 1;
		FailedKill[ i ].Prev = -1;
	}
	FailedKill[ MAXFAILEDKILLS-1 ].Next = -1;
}

/*===================================================================
	Procedure	:	Add FailedKill to que
	Input		:	u_int16_t	Owner
				:	u_int16_t	ID
				:	int16_t	Style
				:	u_int16_t	NewOwner
	Output		:	bool	true/false
===================================================================*/
bool AddFailedKillToQue( u_int16_t Owner, u_int16_t ID, int16_t Style, u_int16_t NewOwner )
{
	int16_t	i;

	i = FindFreeFailedKillSlot();

	if( i != -1 )
	{
		DebugPrintf( "pickups: Added Failed KillPickup() Owner %d, ID %d to que\n", Owner, ID );
		FailedKill[ i ].ID = ID;
		FailedKill[ i ].Owner = Owner;
		FailedKill[ i ].NewOwner = NewOwner;
		FailedKill[ i ].Style = Style;
		FailedKill[ i ].Life = FAILEDKILLTIMEOUT;
		return( true );
	}

	return( false );
}

/*===================================================================
	Procedure	:	Find Free failed kill slot
	Input		:	Nothing
	Output		:	int16_t	Failed pickup index
===================================================================*/
int16_t FindFreeFailedKillSlot( void )
{
	int16_t i;

	i = FirstFailedKillFree;

	if ( i == -1 )
	{
		Msg( "Failed Kill Pickup Que Overflowed!!!\n" );
		return( i );
	}

	FailedKill[ i ].Prev = FirstFailedKillUsed;

	if( FirstFailedKillUsed != -1 )
	{
		FailedKill[ FirstFailedKillUsed ].Next = i;
	}

	FirstFailedKillUsed = i;
	FirstFailedKillFree = FailedKill[ i ].Next;
	return( i );
}

/*===================================================================
	Procedure	:	release a failed kill slot
	Input		:	int16_t	Slot number
	Output		:	Nothing
===================================================================*/
void ReleaseFailedKillSlot( int16_t i )
{
	int16_t	its_prev, its_next;

	its_prev = FailedKill[ i ].Prev;
	its_next = FailedKill[ i ].Next;

	if ( i == FirstFailedKillUsed ) FirstFailedKillUsed = FailedKill[ i ].Prev;
	if( its_prev != -1 ) FailedKill[ its_prev ].Next = its_next;
	if( its_next != -1 ) FailedKill[ its_next ].Prev = its_prev;

	FailedKill[ i ].Prev = -1;
	FailedKill[ i ].Next = FirstFailedKillFree;
	FirstFailedKillFree	= i;
}

/*===================================================================
	Procedure	:	Process Failed Kill Pickups
	Input		:	Nothing
	Output		:	Nothing
===================================================================*/
void ProcessFailedKills( void )
{
	u_int16_t		Pickup;
	int16_t		i, NextFailedKill;

	i = FirstFailedKillUsed;

	while( i != -1 )
	{
		NextFailedKill = FailedKill[ i ].Prev;

		FailedKill[ i ].Life -= framelag;

		if( FailedKill[ i ].Life < 0.0F )
		{
			DebugPrintf( "pickups: ************** Dropped Failed KillPickup() Owner %d, ID %d from que\n", FailedKill[ i ].Owner, FailedKill[ i ].ID );
			ReleaseFailedKillSlot( i );
		}
		else
		{
			Pickup = FindPickup( FailedKill[ i ].Owner, FailedKill[ i ].ID );

			if( Pickup != INDEX_NONE )
			{
				KillPickup( FailedKill[ i ].Owner, FailedKill[ i ].ID, FailedKill[ i ].Style );
				DebugPrintf( "pickups: Removed Failed KillPickup() Owner %d, ID %d from que\n", FailedKill[ i ].Owner, FailedKill[ i ].ID );
				ReleaseFailedKillSlot( i );
			}
		}

		i = NextFailedKill;
	}
}

/*===================================================================
	Procedure	:	Find Pickup from owner and id
	Input		:	u_int16_t		Owner
				:	u_int16_t		ID
	Output		:	u_int16_t		Index
===================================================================*/
u_int16_t PickupPool::find( u_int16_t Owner, u_int16_t ID )
{
	u_int16_t	i;

	i = FirstPickupUsed;

	while( i != INDEX_NONE )
	{
		if( ( Pickups[ i ].Owner == Owner ) && ( Pickups[ i ].ID == ID ) )
		{
			return( i );
		}

		i = Pickups[ i ].Prev;
	}

	return( INDEX_NONE );
}

namespace { PickupPool g_pickups; }

void InitPickups( void ) { g_pickups.init(); }
void ClearPickupsGot( void ) { g_pickups.clear_got(); }
void KillPickup( u_int16_t Owner, u_int16_t ID, int16_t Style ) { g_pickups.kill( Owner, ID, Style ); }
u_int16_t FindFreePickup( void ) { return g_pickups.find_free(); }
void KillUsedPickup( u_int16_t i ) { g_pickups.kill_used( i ); }
u_int16_t InitOnePickup( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t Owner, u_int16_t ID, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod ) { return g_pickups.init_one( Pos, Group, Dir, Speed, Type, Owner, ID, RegenSlot, Sparkle, LifeCount, TriggerMod ); }
void ProcessPickups( void ) { g_pickups.process(); }
void SavePickupsPositions( void ) { g_pickups.save_positions(); }
#ifdef EDITION_REMASTER
bool PreLoadPickups( void ) { return g_pickups.preload(); }
#endif
bool LoadPickupsPositions( void ) { return g_pickups.load_positions(); }
void RegeneratePickups( void ) { g_pickups.regenerate(); }
void KillAllPickups( void ) { g_pickups.kill_all(); }
void CopyPickups( u_int16_t Player ) { g_pickups.copy( Player ); }
void AddPickupToRegen( u_int16_t Type ) { g_pickups.add_to_regen( Type ); }
void SfxForCollectPickup( u_int16_t Owner, u_int16_t ID ) { g_pickups.sfx_for_collect( Owner, ID ); }
FILE * SaveAllPickups( FILE * fp ) { return g_pickups.save( fp ); }
FILE * LoadAllPickups( FILE * fp ) { return g_pickups.restore( fp ); }
void KillAllPickupsOfType( u_int16_t Type, int16_t Style ) { g_pickups.kill_all_of_type( Type, Style ); }
void PickupModelValid( void ) { g_pickups.model_valid(); }
u_int16_t FindPickup( u_int16_t Owner, u_int16_t ID ) { return g_pickups.find( Owner, ID ); }
