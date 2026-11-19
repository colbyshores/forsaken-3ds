#pragma once

/* ships.h -- player ships: per-frame movement, modes, collision and damage
 *
 * The ships themselves live in Ships[MAX_PLAYERS + 1] (GLOBALSHIP, defined in
 * networking.cpp); slot MAX_PLAYERS is the remote / demo-playback camera.
 * InitShipsChangeLevel resets every slot at level load, enables only the
 * local ship (WhoIAm), places each ship at a start position and restores the
 * local player's weapons and ammo.
 *
 * ProcessShips runs once per frame. For the local ship it calls the mode
 * handler ModeControl[Object.Mode] (normal, death, limbo, demo, game over,
 * watch), then applies movement and external forces and collides with the
 * level, enemies and other ships, handles water entry, and collects pickups
 * (CheckPickup). Between network updates other players' ships are
 * dead-reckoned from their last received velocity and turn rate, which decay
 * once PacketDelay runs out. DoDamage applies Ships[WhoIAm].Damage to the
 * local ship only and reports whether that hit killed it.
 *
 * The movement tunables (MaxMoveSpeed, TurnAccell, ...) are globals loaded by
 * SetBikeMods from BikeMods[]: index 0 is the multiplayer default, 1 the
 * in-water set, and 2 + SelectedBike the single-player bike. ProcessShips
 * switches between them as the local ship enters and leaves water.
 */

#include "game/main.h"
#include <math.h>
#include "math/new3d.h"
#include "data/mload.h"
#include "math/quat.h"
#include "world/collision.h"
#include "world/visi.h"
#include "audio/sfx.h"
#include "game/models.h"
#include "data/mxload.h"
#include "input/controls.h"

extern bool SwitchedToWatchMode;

#define DEG2RAD(D)				((D) * PI / 180.0F)

constexpr float RESPAWN_TIMER = -3.5F; // = -250.0F / 71.0f - framelag hack reversal

constexpr auto SHIP_RADIUS = ( 300.0F * GLOBAL_SCALE );
constexpr auto MOVE_TOLERANCE = ( 2.0F * GLOBAL_SCALE );
constexpr float MAX_NITRO_FUEL = ( 100.0F );
constexpr float NITROLOSS = ( 0.08333F );

constexpr float SHIELD_CRITICAL_LEVEL = ( 10.0F );
constexpr float HULL_CRITICAL_LEVEL = ( 64.0F );

#define	OVERRIDE_INVUL		( true )
#define	DONT_OVERRIDE_INVUL	( false )
/*
 * Structures...
 */
struct BIKEMOD
{
	float	MaxMoveSpeed;
	float	MoveAccell;
	float	MoveDecell;
	float	MaxTurboSpeed;
	float	TurboAccell;
	float	TurboDecell;
	float	MaxTurnSpeed;
	float	TurnAccell;
	float	TurnDecell;
	float	MaxRollSpeed;
	float	RollAccell;
	float	RollDecell;
	float	MaxBankAngle;
	float	BankAccell;
	float	BankDecell;
	float	Start_Shield;
	float	Start_Hull;
	float	AutoLevel;

};


/*
 * fn prototypes
 */

bool ProcessShips();
void	InitShipsChangeLevel( MLOADHEADER * Mloadheader );
bool	ENV( MXLOADHEADER * Mloadheader , MATRIX * Mat ,VECTOR * Pos);
int16_t DoDamage( bool OverrideInvul );

void SetBikeMods( u_int16_t Bike );

void AddKill( void );
void InitMultiSfxHandle( void );

// force ship to move
void ForceExternal( u_int16_t ship, VECTOR *force );
void ForceExternalOneOff( u_int16_t ship, VECTOR *force );
void ForceInternalOneOff( u_int16_t ship, VECTOR *force );

void StartShipScreenShake( float Force );

// force ship to rotate
void RotateExternal( u_int16_t ship, VECTOR *point, VECTOR *dir, float force );
void RotateInternal( u_int16_t ship, float xrot, float yrot, float zrot );


void InitShipRandomPos( int16_t i );
void InitShipStartPos( int16_t ship, int16_t pos );
bool GotoRoom( int16_t i, char *roomname );

void Process_Remote_Camera( void );
//	Mode Functions...
void ShipMode0( GLOBALSHIP * ShipPnt , BYTE i );
void ShipMode1( GLOBALSHIP * ShipPnt , BYTE i );
void ShipMode2( GLOBALSHIP * ShipPnt , BYTE i );
void ShipMode4( GLOBALSHIP * ShipPnt , BYTE i );
void WatchMode5( GLOBALSHIP * ShipPnt , BYTE i );

//	Mode Functions...
void RemoteCameraMode0( GLOBALSHIP * ShipPnt , BYTE i );
void RemoteCameraMode1( GLOBALSHIP * ShipPnt , BYTE i );

void ReleaseComponentedShips( void );

FILE * SaveShips( FILE * fp );
FILE * LoadShips( FILE * fp );

/* Defined in ships.cpp; shared with the modules that use it. */
extern RENDERMATRIX TempWorld;
extern SHIPCONTROL control;
extern float MaxMoveSpeed;
extern float MoveAccell;
extern float MoveDecell;
extern float MaxTurboSpeed;
extern float TurboAccell;
extern float TurboDecell;
extern float MaxTurnSpeed;
extern float TurnAccell;
extern float TurnDecell;
extern float MaxRollSpeed;
extern float RollAccell;
extern float RollDecell;
extern float MaxBankAngle;
extern float BankAccell;
extern float BankDecell;
extern int outside_map;
extern u_int32_t TeamFlagMask[ MAX_TEAMS ];
extern bool TeamFlagAtHome[ MAX_TEAMS ];
extern bool RearCameraDisable;
extern float WhiteOut;
extern float NitroFuel;
extern int HullHit;
extern int ShieldHit;
extern float BountyTime;
extern bool ShieldCritical;
extern float NitroFuelUsed;
extern float Start_Shield;
extern float Start_Hull;
void ObjForceExternalOneOff( OBJECT *Obj, VECTOR *force );
