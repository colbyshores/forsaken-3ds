#pragma once

/* primary.h -- primary weapons: weapon table, bullet pool, firing and hits
 *
 * PrimaryWeaponAttribs[TOTALPRIMARYWEAPONS] is indexed by weapon number: the
 * player weapons PULSAR..LASER, ORBITPULSAR, then the enemy-only weapons
 * NME_BULLET1..NME_PULSAR_BLUE (FLAMES included). PrimBulls[] is the live
 * bullet pool; its used list starts at FirstPrimBullUsed and is walked through
 * Prev. InitPrimBulls clears it at scene init and also resets the local
 * player's weapons and ammo (RestoreWeapons / RestoreAmmo).
 *
 * Once per frame FirePrimary turns the local player's fire input into bullets
 * and spends ammo, and ProcessPrimaryBullets moves every bullet and tests it
 * against the level, ships, enemies and secondary weapons. All bullets are
 * created by InitOnePrimBull, which sends the shot to other players only when
 * NetFlag is set; player shots are not sent that way, and other machines
 * recreate them in FirePrimaryWeapons from the fire flags in ship updates.
 * Enemies and model spot FX fire through EnemyFirePrimary, which records the
 * firing GUNOBJECT on the bullet.
 *
 * Ship hits follow ColPerspective: COLPERS_Forsaken lets the machine that
 * owns the bullet resolve the hit (IHitYou for a remote victim), while
 * COLPERS_Descent only resolves hits on the local ship. The ammo, PowerLevel
 * and LaserTemperature (in PrimaryWeapons, below) describe the local player
 * only.
 * NmeDamageModifier (set from the difficulty level by PreLoadEnemies) scales
 * the damage enemy weapons do to ships.
 */

#include "game/constants.h"
#include "game/main.h"
#include "rendering/2dtextures.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAXPRIMARYWEAPONBULLETS = 256;

constexpr int MAXENEMYPRIMARYWEAPONS = 11; /* +1 for NME_PULSAR_BLUE */
constexpr auto TOTALPRIMARYWEAPONS = ( MAXPRIMARYWEAPONS + 1 + MAXENEMYPRIMARYWEAPONS );

constexpr int MAXPOWERLEVELS = 3;

constexpr float MAXGENERALAMMO = 2000.0F;
constexpr float MAXPYROLITEAMMO = 3000.0F;
constexpr float MAXSUSSGUNAMMO = 3000.0F;
constexpr float MAXMULTIPLEAMMO = 1000.0F;

constexpr float MINIMUM_TOTAL_AMMO = 100.0F;

constexpr float GENERALAMMOPERPOD = 500.0F;
constexpr float PYROLITEAMMOPERPOD = 500.0F;
constexpr float SUSSGUNAMMOPERPOD = 500.0F;

constexpr int PULSAR = 0;
constexpr int TROJAX = 1;
constexpr int PYROLITE_RIFLE = 2;
constexpr int TRANSPULSE_CANNON = 3;
constexpr int SUSS_GUN = 4;
constexpr int LASER = 5;
constexpr int ORBITPULSAR = 6;
constexpr int NME_BULLET1 = 7;
constexpr int NME_PULSAR = 8;
constexpr int NME_TROJAX = 9;
constexpr int NME_PYROLITE = 10;
constexpr int NME_TRANSPULSE = 11;
constexpr int NME_SUSS_GUN = 12;
constexpr int NME_LASER = 13;
constexpr int NME_LIGHTNING = 14;
constexpr int FLAMES = 15;
constexpr int NME_POWERLASER = 16;
constexpr int NME_PULSAR_BLUE = 17; /* KEX Pulsar_Enemy_Blue — blue plasma (ShieldTurret, Ghost) */

constexpr int NO_PRIMARY = 128;

constexpr auto ORBITPULSAR_FMSIZE = ( ( 2.0F * GLOBAL_SCALE ) * 0.5F );

constexpr auto PULSAR_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto PULSAR_FMSIZE = ( ( 5.0F * GLOBAL_SCALE ) * 0.5F );
constexpr auto PULSAR_TAILSTART = ( ( 40.0F * GLOBAL_SCALE ) * 0.5F );
constexpr auto PULSAR_TAILEND = ( ( 20.0F * GLOBAL_SCALE ) * 0.5F );
constexpr auto PULSAR_TAILPOWERMUL = ( ( 240.0F * GLOBAL_SCALE ) * 0.5F );
constexpr auto PULSAR_TAILADDMIN = ( ( 376.0F * GLOBAL_SCALE ) * 0.5F );
constexpr auto PULSAR_EXPSIZE = ( ( 8.0F * GLOBAL_SCALE ) * 0.5F );

constexpr auto TROJAX_COLRADIUS = ( ( 512.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TROJAX_EXPSIZE = ( ( 4.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TROJAX_EXPMAXSIZE = ( ( 16.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TROJAX_EXPINC = ( ( 0.333F * GLOBAL_SCALE ) * 1.0F );

constexpr auto PYROLITE_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto PYROLITE_POWERMUL = ( ( 0.5F * GLOBAL_SCALE ) * 1.0F );
constexpr auto PYROLITE_ADDMIN = ( ( 0.75F * GLOBAL_SCALE ) * 1.0F );

constexpr auto TRANSPULSE_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_POWERMUL = ( ( 2.4F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_ADDMIN = ( ( 4.2F * GLOBAL_SCALE ) * 1.0F );
constexpr auto ARC_POWERMUL = ( ( 3.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto ARC_ADDMIN = ( ( 6.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto ARC_LENGTH = ( ( 12.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_SPARKSIZE = ( ( 4.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_EXP_INC = ( ( 0.333F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_EXP_MINSIZE = ( ( 1.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_EXP_MAXSIZE = ( ( 16.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto TRANSPULSE_FLARE_SIZE = ( ( 10.0F * GLOBAL_SCALE ) * 1.0F );
constexpr int TRANSPULSE_BOUNCES = ( 1 );
constexpr auto TRANSPULSE_VIEWCONE = ( 160.0F / 2.0F );

constexpr auto ARCTRAIL_POWERMUL = ( ( 360.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto ARCTRAIL_ADDMIN = ( ( 580.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SUSSGUN_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SUSSGUN_POWERMUL = ( ( 0.5F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SUSSGUN_ADDMIN = ( ( 0.5F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SUSSGUN_FLARESIZE = ( ( 4.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SUSSGUN_EXPSIZE = ( ( 8.0F * GLOBAL_SCALE ) * 1.0F );
constexpr float SUSSGUNLIFE_POWERMUL = ( 10.0F );
constexpr float SUSSGUNLIFE_ADDMIN = ( 10.0F );

constexpr auto LASER_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto LASER_WIDTH_POWERMUL = ( ( 8.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto LASER_WIDTH_ADDMIN = ( ( 24.0F * GLOBAL_SCALE ) * 1.0F );
constexpr float LASER_TEMP_POWERMUL = ( 0.5F );
constexpr float LASER_TEMP_ADDMIN = ( 0.5F );

constexpr auto NMEBUL1_COLRADIUS = ( ( 0.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto NMEBUL1_FMSIZE = ( ( 5.0F * GLOBAL_SCALE ) * 2.0F );

constexpr auto SPARKSIZE = ( ( 8.0F * GLOBAL_SCALE ) * 1.0F );


constexpr auto NORM_EXPLO_SIZE = ( ( 16.0F * GLOBAL_SCALE ) * 1.0F );

constexpr auto SMOKE_TRAIL_SIZE = ( ( 2.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto GRAVGON_TRAIL_SIZE = ( ( 7.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto GRAVGON_TRAIL_INC = ( ( 0.4F * GLOBAL_SCALE ) * 1.0F );
constexpr auto SOLARIS_TRAIL_SIZE = ( ( 7.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto NEW_TRAIL_SIZE = ( ( 7.0F * GLOBAL_SCALE ) * 1.0F );

constexpr auto PLASER_COLRADIUS = ( ( 200.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto PLASER_WIDTH_POWERMUL = ( ( 16.0F * GLOBAL_SCALE ) * 1.0F );
constexpr auto PLASER_WIDTH_ADDMIN = ( ( 48.0F * GLOBAL_SCALE ) * 1.0F );

constexpr float GRAVGON_SCALE_DEC = 0.075F;

constexpr auto PICKUP_REGEN_SIZE = ( ( 8.0F * GLOBAL_SCALE ) * 1.0F );

constexpr auto BLOODSIZE = ( ( 4.0F * GLOBAL_SCALE ) * 1.0F );

constexpr auto FLAMES_COLRADIUS = ( ( 500.0F * GLOBAL_SCALE ) * 1.0F );

constexpr int PULSAR2SHOTPOWERLEVEL = ( 1 );

constexpr int COLTYPE_Point = 0;
constexpr int COLTYPE_Sphere = 1;
constexpr int COLTYPE_Trojax = 2;
constexpr int COLTYPE_Transpulse = 3;

constexpr int WEPTYPE_Primary = 0;
constexpr int WEPTYPE_Secondary = 1;
constexpr int WEPTYPE_Various = 2;

constexpr int OWNER_NOBODY = 0;
constexpr int OWNER_SHIP = 1;
constexpr int OWNER_ENEMY = 2;
constexpr int OWNER_BGOBJECT = 3;
constexpr int OWNER_MINE = 4;
constexpr int OWNER_MODELSPOTFX = 5;

constexpr int COLPERS_Forsaken = 0; // Forsaken Collision ( shooter Perspective )
constexpr int COLPERS_Descent = 1; // Descent Collision ( target Perspective )

/*===================================================================
	Structures
===================================================================*/
struct PRIMARYWEAPONATTRIB {

	float		Speed[ MAXPOWERLEVELS ];		// how fast do I move
	float		LifeCount;						// how long can I last..
	u_int16_t		FireDelay[ MAXPOWERLEVELS ];	// how fast can I fire
	float		AmmoUsage[ MAXPOWERLEVELS ];	// how much ammo do i use per shot...
	float		Damage[ MAXPOWERLEVELS ];		// how much damage can i inflict...
	float		ColRadius[ MAXPOWERLEVELS ];	// Collision Radius...
	u_int16_t		ColType;						// Collision Type...
	float		lightsize;						// how big is the light
	float		r[ MAXPOWERLEVELS ];			// what color does it emit....
	float		g[ MAXPOWERLEVELS ];			// what color does it emit....
	float		b[ MAXPOWERLEVELS ];			// what color does it emit....
	int16_t		FmSeq;							// Faceme Sequence
	FRAME_INFO ** FmFrmInfo;					// Faceme FrameInfo

};

struct PRIMARYWEAPONBULLET {
	bool		Used;
	u_int16_t		Next;							// link list.....
	u_int16_t		Prev;							// link list.....
	u_int16_t		Type;							// which type of bullet am I
	u_int16_t		OwnerType;						// who fired me...
	u_int16_t		Owner;							// who fired me...
	void *		EnemyGun;						// Only used by enemies.
	u_int16_t		ID;								// Id from that ship..
	int8_t		Weapon;							// Weapon type that fired me..
	int16_t		PowerLevel;						// Power level the weapon has..
	float		TrojPower;						// Trojax Power
	float		LifeCount;						// how long do I live.....
	float		Speed;							// how fast do I move
	float		ColRadius;						// how big is my collide sphere
	u_int16_t		ColType;						// what type of collision
	VECTOR		Offset;							// offset relative to ship that fired me...
	VECTOR		Pos;							// where am I
	VECTOR		Dir;							// where am i going
	VECTOR		LocalDir;						// Local Direction
	VECTOR		UpVector;						// UpVector
	VECTOR		ColStart;						// where I was when started going straight.
	float		ColDist;						// Length from Collision to ColStart.
	u_int16_t		ColFlag;						// have I checked if im going to collide with something
	VERT		ColPoint;						// where am I going to collide
	NORMAL		ColPointNormal;					// if I reflect use this when I do
	u_int16_t		ColGroup;						// where am I going to collide
	u_int16_t		GroupImIn;						// which group am I in...
	MATRIX		Mat;							// and a Matrix for when Im displayed...
	u_int16_t		line;							// attached line
	u_int16_t		fmpoly;							// if Im 2D then I need a Face Me Poly..
	int16_t		numfmpolys;						// Number of linked fmpolys
	u_int16_t		poly;							// first attached poly
	int16_t		numpolys;						// Number of attached polys
	u_int16_t		light;							// if I produce Light then I Need A Light
	float		lightsize;						// how big is the light
	float		r;								// what color does it emit....
	float		g;								// what color does it emit....
	float		b;								// what color does it emit....
	u_int16_t		Bounces;						// Number of bounces
	float		TimeInterval;
	float		TimeCount;
	int16_t		FirePoint;						// gun...
	int16_t		SpotFX;
	float		FramelagAddition;

};							// sizes and colours of 2d polys and Lights are set up depoending on type...


/*===================================================================
	Prototypes
===================================================================*/
void	InitPrimBulls(void);
u_int16_t	FindFreePrimBull(void);
void	KillUsedPrimBull( u_int16_t i );
void	FirePrimary(void);
void	ProcessPrimaryBullets( void );
void	CleanUpPrimBull( u_int16_t i, bool LightFlag  );
u_int16_t InitOnePrimBull( u_int16_t OwnerType, u_int16_t OwnerID, u_int16_t BulletID, int8_t Weapon,
						u_int16_t Group, VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * Up,
						int16_t PowerLevel, float PLevel, bool NetFlag );
void	CreateTrojaxExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t FmPoly, int16_t Type, u_int16_t Group );
void	CreateNmeTrojaxExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t FmPoly, int16_t Type, u_int16_t Group );
void	CreatePulsarExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateNmePulsarExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateNmeShrapnelExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateShrapnelExplosion( VECTOR * Pos, u_int16_t Group );
void	CreateNmeArcExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateArcExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNmeLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
void	CreateNmePowerLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
void	CreateNmeLightningPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
void	CreateLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
void	CreateTransTrail( u_int16_t i );
void	RestoreAmmo( void );
void	RestoreWeapons( void );
void	InitAmmoUsed( void );
void	CreateShieldEffect( VECTOR * Pos, VECTOR * IntPos, u_int16_t ShipHit, int16_t Num,
							 u_int8_t Red, u_int8_t Green, u_int8_t Blue );
u_int16_t	CheckHitShip( u_int16_t OwnerType, u_int16_t Owner, VECTOR * Pos, u_int16_t Group, VECTOR * Dir, VECTOR * UpDir, float DirLength, VECTOR * Int_Point, VECTOR * Int_Point2, float * Dist, float WeaponRadius, u_int16_t ColType );
u_int16_t	CheckHitSecondary( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, VECTOR * UpDir, float DirLength, VECTOR * Int_Point, VECTOR * Int_Point2, float * Dist, float WeaponRadius, u_int16_t ColType );
void	PrimarySelectControl( void );
void	CreateDebugLine( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int8_t Red, u_int8_t Green, u_int8_t Blue );
void	FirePrimaryWeapons( u_int8_t Ship );
void	CreateColSparks( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateSmallColSparks( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CheckCharging( u_int16_t Ship, u_int32_t OldFlags, u_int32_t NewFlags );
void	GivemeAllWeapons( void );
void	LoseAllWeapons( void );
void	DropPrimary( void );
void	GetDeathString( BYTE WeaponType, BYTE Weapon, char * DstString, size_t size );
void	DropShield( void );
void	DropAmmo( void );
bool ChangeTranspulseDir( u_int16_t i, VECTOR * Pos, VECTOR * MoveOffset, VECTOR * Dir );
void	ChangePulsarDir( u_int16_t i, VECTOR * Dir );
void	ChangeTrojaxDir( u_int16_t i, VECTOR * Dir );
void	ChangeSussgunDir( u_int16_t i, VECTOR * Dir );
void	ChangeOrbitPulsarDir( u_int16_t i, VECTOR * Dir );
void	GivemeSuperNashram( void );
void	LoseSuperNashram( void );
void	CreateInvEffect( u_int16_t ShipHit, int16_t Num, u_int8_t Red, u_int8_t Green, u_int8_t Blue );
void	CreateSparks( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNewExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateSteam( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
u_int16_t	EnemyFirePrimary( u_int16_t OwnerType, u_int16_t OwnerID, u_int16_t BulletID, int8_t Weapon,
						u_int16_t Group, VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * Up,
						int16_t PowerLevel, float PLevel, bool NetFlag, void * Gun );
void CreatePowerLaserSparks( VECTOR * Pos, VECTOR * Dir, u_int16_t Group, u_int8_t Red, u_int8_t Green, u_int8_t Blue );
void GetLaserLocalVector( u_int16_t i, VECTOR * LocalVector );
void CreateScaleExplosion( VECTOR * Pos, u_int16_t Group, float GlobalScale );
bool ValidGroupCollision( VECTOR * StartPos, u_int16_t StartGroup, VECTOR * IntPoint, VECTOR * ObjPos, u_int16_t ObjGroup );
int16_t GetAmmoPickupToGen( void );
FILE * SavePrimBulls( FILE * fp );
FILE * LoadPrimBulls( FILE * fp );
u_int8_t GetNextValidPrimaryWeapon( void );
u_int8_t GetPrevValidPrimaryWeapon( void );
int8_t ConvPrimToNormWeapon( int8_t Primary );
void TogglePrimaryWeapons( int8_t Primary );
void KillPrimBullsByOwner(  u_int16_t OwnerType, u_int16_t Owner );

/* Defined in primary.cpp; shared with the modules that use it. */
extern PRIMARYWEAPONATTRIB PrimaryWeaponAttribs[ TOTALPRIMARYWEAPONS ];
extern PRIMARYWEAPONBULLET PrimBulls[ MAXPRIMARYWEAPONBULLETS ];
extern float NmeDamageModifier;
extern int32_t ColPerspective;
extern float GlobalFramelagAddition;
extern int8_t PrimaryToFireLookup[ MAXPRIMARYWEAPONS ];
extern bool PrimaryWeaponCheat;
extern int32_t MyColPerspective;
extern const char *PrimaryDescription[ MAXPRIMARYWEAPONS ];
extern bool PrimaryValid[ MAXPRIMARYWEAPONS ];

/* The primary weapons' state (one instance, Primaries):
 * - the PrimBulls[] used and free list heads, reset with the scene and
 *   changed as each bullet is made and dies, and the ID counter for the
 *   bullets no ship owns (spot FX lightning);
 * - the local ship's firing state: its fire delays, the pulsar/pyrolite side
 *   it fires from next (Toggle), the trojax charge (PowerLevel) and its
 *   sound, the laser temperature and overheat latch, changed as it fires
 *   each frame;
 * - the local player's primary weapons (PrimaryWeaponsGot) and ammo, and the
 *   ammo it has used (InitAmmoUsed clears it);
 * - the loadout Super Nashram replaced (the CopyOf* bank), given back when
 *   it ends;
 * - the per-ship trojax charge sounds of the other ships (TrojaxOwnersID),
 *   started and stopped as their ship updates arrive;
 * - the pyrolite light and laser width shared by bullet creation and the
 *   laser beam builders, and the pyrolite flame timer the per-frame
 *   processing advances;
 * - the invulnerability sparkle timer, whether GodMode still has to arm the
 *   ship (GodModeOnceOnly), and the weapons and ammo GodMode copies on
 *   GivemeAllWeapons and gives back on LoseAllWeapons;
 * - the constant tables only primary.cpp reads, as static members.
 * Other files use the intent members below; the C API forwards to the rest.
 * PrimBulls[] and PrimaryWeaponAttribs[] stay global with it. */
class PrimaryWeapons {
	public:
	/* The intent members are always_inline so each caller compiles as the
	 * raw global access did.
	 *
	 * The primary weapons the local player holds (PULSAR..LASER): the HUD
	 * and the death scatter test them, a weapon pickup gives one and the
	 * scatter takes one away. */
	[[gnu::always_inline]] int16_t got_weapon( int Weapon ) const { return PrimaryWeaponsGot[ Weapon ]; }
	[[gnu::always_inline]] void give_weapon( int Weapon ) { PrimaryWeaponsGot[ Weapon ] = 1; }
	[[gnu::always_inline]] void take_weapon( int Weapon ) { PrimaryWeaponsGot[ Weapon ] = 0; }

	/* The local player's ammo, by pool: the HUD shows it, an ammo pod adds
	 * to it and a weapon pickup fills it. */
	[[gnu::always_inline]] float general_ammo() const { return GeneralAmmo; }
	[[gnu::always_inline]] float pyrolite_ammo() const { return PyroliteAmmo; }
	[[gnu::always_inline]] float sussgun_ammo() const { return SussGunAmmo; }
	[[gnu::always_inline]] void add_general_ammo( float Ammo ) { GeneralAmmo += Ammo; }
	[[gnu::always_inline]] void add_pyrolite_ammo( float Ammo ) { PyroliteAmmo += Ammo; }
	[[gnu::always_inline]] void add_sussgun_ammo( float Ammo ) { SussGunAmmo += Ammo; }
	[[gnu::always_inline]] void fill_pyrolite_ammo() { PyroliteAmmo = MAXPYROLITEAMMO; }
	[[gnu::always_inline]] void fill_sussgun_ammo() { SussGunAmmo = MAXSUSSGUNAMMO; }

	/* While Super Nashram runs, the loadout it replaced is banked
	 * (GivemeSuperNashram) and given back when it ends (LoseSuperNashram),
	 * so what the player collects meanwhile goes into the bank. */
	[[gnu::always_inline]] float banked_general_ammo() const { return CopyOfGeneralAmmo; }
	[[gnu::always_inline]] float banked_pyrolite_ammo() const { return CopyOfPyroliteAmmo; }
	[[gnu::always_inline]] float banked_sussgun_ammo() const { return CopyOfSussGunAmmo; }
	[[gnu::always_inline]] void bank_general_ammo( float Ammo ) { CopyOfGeneralAmmo += Ammo; }
	[[gnu::always_inline]] void bank_pyrolite_ammo( float Ammo ) { CopyOfPyroliteAmmo += Ammo; }
	[[gnu::always_inline]] void bank_sussgun_ammo( float Ammo ) { CopyOfSussGunAmmo += Ammo; }
	[[gnu::always_inline]] void fill_banked_pyrolite_ammo() { CopyOfPyroliteAmmo = MAXPYROLITEAMMO; }
	[[gnu::always_inline]] void fill_banked_sussgun_ammo() { CopyOfSussGunAmmo = MAXSUSSGUNAMMO; }
	[[gnu::always_inline]] int16_t banked_power_level() const { return CopyOfPowerLevel; }
	[[gnu::always_inline]] void bank_power_pod() { CopyOfPowerLevel++; }
	[[gnu::always_inline]] float banked_nitro() const { return CopyOfNitroFuel; }
	[[gnu::always_inline]] void bank_nitro( float Fuel ) { CopyOfNitroFuel += Fuel; }
	[[gnu::always_inline]] void cap_banked_nitro( float Max ) { CopyOfNitroFuel = Max; }
	[[gnu::always_inline]] BYTE banked_orbitals() const { return CopyOfOrbitals; }
	[[gnu::always_inline]] void bank_orbital() { CopyOfOrbAmmo[ CopyOfOrbitals ] = MAXMULTIPLEAMMO; CopyOfOrbitals++; }

	/* The HUD's energy bar: the trojax charge and the laser temperature
	 * (each 0 to 100). */
	[[gnu::always_inline]] float trojax_charge() const { return PowerLevel; }
	[[gnu::always_inline]] float laser_temperature() const { return LaserTemperature; }

	/* On a level change: if GodMode is on, the next FirePrimary gives the
	 * ship every weapon again. */
	[[gnu::always_inline]] void rearm_god_mode() { GodModeOnceOnly = true; }

	/* The ID for the next bullet no ship owns (spot FX lightning). */
	[[gnu::always_inline]] u_int16_t next_bullet_id() { return GlobalPrimBullsID++; }

	/* The C API (InitPrimBulls, FirePrimary and the rest) forwards to these,
	 * one forwarder each, and other files and primary.cpp itself call the C
	 * API. They are always_inline so each forwarder keeps the body, with no
	 * this pointer, as when the class was private to primary.cpp. */
	[[gnu::always_inline]] inline void init();
	[[gnu::always_inline]] inline void restore_weapons();
	[[gnu::always_inline]] inline void restore_ammo();
	[[gnu::always_inline]] inline void init_ammo_used();
	[[gnu::always_inline]] inline u_int16_t find_free();
	[[gnu::always_inline]] inline void kill_used( u_int16_t i );
	[[gnu::always_inline]] inline void fire();
	[[gnu::always_inline]] inline void process();
	[[gnu::always_inline]] inline u_int16_t init_one( u_int16_t OwnerType, u_int16_t OwnerID, u_int16_t BulletID, int8_t Weapon,
						u_int16_t Group, VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * Up,
						int16_t PowerLevel, float PLevel, bool NetFlag );
	[[gnu::always_inline]] inline void create_laser_pulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
	[[gnu::always_inline]] inline void create_nme_laser_pulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
	[[gnu::always_inline]] inline void create_nme_power_laser_pulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t EndGroup );
	[[gnu::always_inline]] inline void auto_select();
	[[gnu::always_inline]] inline void fire_weapons( u_int8_t Ship );
	[[gnu::always_inline]] inline void check_charging( u_int16_t Ship, u_int32_t OldFlags, u_int32_t NewFlags );
	[[gnu::always_inline]] inline void give_all_weapons();
	[[gnu::always_inline]] inline void lose_all_weapons();
	[[gnu::always_inline]] inline void drop_primary();
	[[gnu::always_inline]] inline void drop_ammo();
	[[gnu::always_inline]] inline void give_super_nashram();
	[[gnu::always_inline]] inline void lose_super_nashram();
	[[gnu::always_inline]] inline void inv_effect( u_int16_t ShipHit, int16_t Num, u_int8_t RVal, u_int8_t GVal, u_int8_t BVal );
	[[gnu::always_inline]] inline int16_t ammo_pickup_to_gen();
	[[gnu::always_inline]] inline FILE * save( FILE * fp );
	[[gnu::always_inline]] inline FILE * restore( FILE * fp );
	[[gnu::always_inline]] inline u_int8_t next_valid_weapon();
	[[gnu::always_inline]] inline u_int8_t prev_valid_weapon();
	[[gnu::always_inline]] inline void kill_by_owner( u_int16_t OwnerType, u_int16_t Owner );

	/* Public only so the count check after its definition can read it. */
	static int8_t * DebugPrimStrings[];

	private:
	[[gnu::always_inline]] inline void FireShootPulsar( VECTOR & DownVector, VECTOR & LeftVector, VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector );
	[[gnu::always_inline]] inline void FireShootTrojax( VECTOR & DownVector, VECTOR & LeftVector, VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector, float & Ammo );
	[[gnu::always_inline]] inline void FireShootPyroliteRifle( VECTOR & DownVector, VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector, float & Speed );
	[[gnu::always_inline]] inline void FireShootSussGun( VECTOR & DownVector, VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector, int16_t & Count );
	[[gnu::always_inline]] inline void FireShootLaser( VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector );
	[[gnu::always_inline]] inline void FireShootNmeLightning( VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector );
	[[gnu::always_inline]] inline void FirePrimaryShoot( VECTOR & DownVector, VECTOR & LeftVector, VECTOR & Pos, VECTOR & Dir, VECTOR & UpVector, float & Ammo, int16_t & Count, float & Speed );
	[[gnu::always_inline]] inline void FirePrimarySelect( int16_t & OnceOnlyFlag, float & Ammo );
	[[gnu::always_inline]] inline void FirePrimaryTrojaxCharge( float & Old_PowerLevel, float & Freq );
	[[gnu::always_inline]] inline void FirePrimaryOrbitPulsars( VECTOR & Pos, VECTOR & UpVector, VECTOR & DirVector, VECTOR & Rotation, int16_t & Count, u_int16_t & Model );

	/* The per-ship charge sounds first (indexed by ship), then the list
	 * heads and halfword scalars, so every halfword access keeps an
	 * ldrh/strh immediate; then the firing state the per-frame firing
	 * reads; the GodMode copies (cold) last. */
	u_int32_t TrojaxOwnersID[ MAX_PLAYERS ] = {};
	u_int16_t FirstPrimBullUsed = 0;
	u_int16_t FirstPrimBullFree = 0;
	int16_t   Toggle = 0;
	int16_t   LaserOverheated = 0;
	u_int16_t GlobalPrimBullsID = 0;
	u_int32_t TrojaxChargeID = 0;
	float     PrimaryFireDelay = 0.0F;
	float     OrbitFireDelay = 0.0F;
	float     PowerLevel = 0.0F;
	float     LaserTemperature = 0.0F;
	float     PyroCount = 0.0F;
	float     LaserDiameter = ( 40.0F * GLOBAL_SCALE );
	float     GeneralAmmoUsed = 0.0F;
	float     PyroliteAmmoUsed = 0.0F;
	float     SussGunAmmoUsed = 0.0F;
	float     InvulInterval = 1.0F;
	float     InvulCount = 0.0F;
	bool      PyroLightFlag = true;
	bool      GodModeOnceOnly = true;
	BYTE      GodModeCopyOfPrimary = PULSAR;
	BYTE      GodModeCopyOfSecondary = 0;	/* MUGMISSILE (checked in primary.cpp) */
	int16_t   GodModeCopyOfPowerLevel = 0;
	int16_t   GodModeCopyOfSecondaryAmmo[ MAXSECONDARYWEAPONS ] = {};
	int16_t   GodModeCopyOfSecondaryWeaponsGot[ MAXSECONDARYWEAPONS ] = {};
	int16_t   GodModeCopyOfWeaponsGot[ MAXPRIMARYWEAPONS ] = {};
	float     GodModeCopyOfGeneralAmmo = 0.0F;
	float     GodModeCopyOfSussGunAmmo = 0.0F;
	float     GodModeCopyOfPyroliteAmmo = 0.0F;
	float     GodModeCopyOfNitroFuel = 0.0F;

	/* Static, so each stays its own symbol:
	 * - the HUD, pickups and the death scatter index PrimaryWeaponsGot by
	 *   weapon, as the per-frame weapon select does;
	 * - DropAmmo and the pickup collection pick the live ammo or its Super
	 *   Nashram bank and then share one compare-and-add on the selected
	 *   address. As instance members the two differ only in offset, and
	 *   GCC keeps a copy of that code for each (DropAmmo 136 -> 152,
	 *   CollectPickup 1469 -> 1502). */
	static int16_t  PrimaryWeaponsGot[ MAXPRIMARYWEAPONS ];
	static float    GeneralAmmo;
	static float    PyroliteAmmo;
	static float    SussGunAmmo;
	static int16_t  CopyOfPowerLevel;
	static float    CopyOfGeneralAmmo;
	static float    CopyOfSussGunAmmo;
	static float    CopyOfPyroliteAmmo;
	static float    CopyOfNitroFuel;
	static BYTE     CopyOfOrbitals;
	static float    CopyOfOrbAmmo[ MAXMULTIPLES ];
	static int16_t  PrimarySfxLookup[ MAXPRIMARYWEAPONS ];
	static float    SussgunTable[ MAXPOWERLEVELS ];
	static float    TrojaxFreqTable[];
	static float    TrojaxChargeRateTable[];
};

extern PrimaryWeapons Primaries;
