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
 * COLPERS_Descent only resolves hits on the local ship. The ammo globals,
 * PowerLevel and LaserTemperature describe the local player only.
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
typedef struct PRIMARYWEAPONATTRIB {

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

} PRIMARYWEAPONATTRIB;

typedef struct PRIMARYWEAPONBULLET {
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

} PRIMARYWEAPONBULLET;							// sizes and colours of 2d polys and Lights are set up depoending on type...


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
void	CreatePulsarExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNmePulsarExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNmeShrapnelExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateShrapnelExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNmeArcExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateArcExplosion( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
void	CreateNmeLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t Group, u_int16_t EndGroup );
void	CreateNmePowerLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t Group, u_int16_t EndGroup );
void	CreateNmeLightningPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t Group, u_int16_t EndGroup );
void	CreateLaserPulse( u_int16_t i, float Distance, VECTOR * Dir, u_int16_t Group, u_int16_t EndGroup );
void	CreateTransTrail( u_int16_t i );
void	RestoreAmmo( void );
void	RestoreWeapons( void );
void	InitAmmoUsed( void );
void	CreateShieldEffect( VECTOR * Pos, VECTOR * IntPos, VECTOR * IntPos2, u_int16_t ShipHit, int16_t Num,
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
void	GetDeathString( BYTE WeaponType, BYTE Weapon, char * DstString );
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
extern u_int16_t GlobalPrimBullsID;
extern int32_t ColPerspective;
extern float PyroliteAmmo;
extern float SussGunAmmo;
extern float GeneralAmmo;
extern float PowerLevel;
extern float LaserTemperature;
extern bool GodModeOnceOnly;
extern int16_t PrimaryWeaponsGot[ MAXPRIMARYWEAPONS ];
extern float GeneralAmmoUsed;
extern float PyroliteAmmoUsed;
extern float SussGunAmmoUsed;
extern BYTE CopyOfOrbitals;
extern float CopyOfOrbAmmo[ MAXMULTIPLES ];
extern int16_t CopyOfPowerLevel;
extern float CopyOfNitroFuel;
extern float CopyOfGeneralAmmo;
extern float CopyOfSussGunAmmo;
extern float CopyOfPyroliteAmmo;
extern float GlobalFramelagAddition;
extern int8_t PrimaryToFireLookup[ MAXPRIMARYWEAPONS ];
extern bool PrimaryWeaponCheat;
extern int32_t MyColPerspective;
extern const char *PrimaryDescription[ MAXPRIMARYWEAPONS ];
extern int8_t * DebugPrimStrings[];
extern bool PrimaryValid[ MAXPRIMARYWEAPONS ];
