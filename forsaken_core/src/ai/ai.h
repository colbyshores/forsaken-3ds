#pragma once

/* ai.h -- enemy AI: brains, modes, perception flags, targeting helpers
 *
 * Each frame ProcessEnemies (enemies.cpp) calls EnemyControlType[
 * Object.ControlType ], set from EnemyTypes[].ControlType. The *UnderAiControl
 * wrappers there run one brain -- AI_AIR (FLY_AI), AI_CRAWL, AI_TURRET,
 * AI_SPLINE, AI_EXOGENON, AI_FLESHMORPH, AI_LITTLEGEEK (aimain.cpp) or
 * AI_JUMP (aijump.cpp, Remaster) -- then turn the AIMoveFlags / AI_Angle the
 * brain wrote into movement. The fly, crawl, Exogenon, Fleshmorph and
 * LittleGeek wrappers apply movement only while AI_ANYPLAYERINRANGE is set
 * (Boss_Avatar excepted).
 *
 * A brain is a table of handlers indexed by Object.AI_Mode; a mode is one
 * behaviour (follow path, scan, dogfight, ...), mostly one ai<mode>.cpp
 * each. Modes switch by writing AI_Mode, usually via the AI_Set* helpers in
 * ailib.cpp, which also reset Timer / TShip / TNode. The first mode is
 * EnemyTypes[].AiMode (FORMATION for formation-linked enemies); a killed
 * FLY_AI enemy may get an AIMODE_DEATH_* mode instead of dying at once.
 * The AIMODE_* enums index the tables, so their order is load-bearing; the
 * tables are not padded to the whole enum, so a brain must only be put in
 * modes its table covers. Exogenon and Fleshmorph use their own mode enums.
 *
 * AI_THINK (aimain.cpp) is the perception pass: every ThinkTime it rebuilds
 * ENEMY.AIFlags (AI_* bits: player in range, can see / hear, targeted,
 * friendly fire) from range, view cone and LOS, and AI_AVOIDCHECK sets
 * AvoidTimer / AvoidType when the target fires at the enemy. Called with
 * JustCheckPlayerRange it only refreshes AI_ANYPLAYERINRANGE. Most modes
 * call AI_THINK first and AI_UPDATEGUNS (aifire.cpp) to aim and fire the
 * enemy's guns. AI_BEHAVIOUR_* bits come from the enemy type, AI_CONTROL_*
 * bits are per-frame steering requests.
 *
 * ailib.cpp holds the shared helpers (target search, AI_AimAtTarget, LOS
 * and view-cone tests, look-ahead) and their scratch results (TargetInfo
 * behind Tinfo, AimData); every call overwrites them, so read them straight
 * after. Target search only finds the local ship, Ships[WhoIAm]. Timers
 * count framelag ticks (ONE_SECOND = 60). AimData.Angle is in degrees in
 * the source's local frame: y is yaw, x is pitch.
 */

#include "game/main.h"
#include "util/util.h"

//--------------------------------------------------------------------------
// Define AI modes
//--------------------------------------------------------------------------

enum
{
	AIMODE_PLAYER,					// Controlled by PLAYER
	AIMODE_FOLLOWPATH,				// Follow node path
	AIMODE_IDLE,					// Do nothing for a while
	AIMODE_SCAN,					// Look around for a target
	AIMODE_MOVETOTARGET,			// Move towards selected target   
	AIMODE_FIRE,					// Fire at PLAYER
	AIMODE_DOGFIGHT,				// Spar with the player		Move in swrily motion...
	AIMODE_KILLMINE,				// Attack And kill a Mine....
	AIMODE_RETREAT,					// Try and get away from PLAYER
	AIMODE_FORMATION,				// Stay in Formation and attack whan necessary...

	AIMODE_DEATH_CRASHNBURN,		// Fall to the ground and Shatter...
	AIMODE_DEATH_PINGOFF,			// Ping off in the dir of the bullet that killed me spinning as I go...
	AIMODE_DEATH_PSYCHO,			// Twist and turn and shoot and explode...

	// No brain implements the modes from here down; nothing sets them.
	AIMODE_DEATH_DEADINTHEWATER,	// Float or stop occasionally Spark or explode...
	
	AIMODE_STRAFE,					// Do a passing Run at the player firing as I go..
	AIMODE_GETBEHIND,				// Get behind the player then start dog fighting him again...
	AIMODE_AVOIDHOMING,				// Avoid a Homing Missile..
	AIMODE_FINDCOVER,				// Find a 'safe' spot for a while
	AIMODE_UNLOAD,					// Unload all at PLAYER
	AIMODE_BACKOFF,					// Back off firing at PLAYER
	AIMODE_STALK,					// Stalk PLAYER from a distance
	AIMODE_FLOCK,					// Flock around a point
};
enum
{
	AIMODE_EXOGENON_IDLE,
	AIMODE_EXOGENON_MOVEDOWN,
	AIMODE_EXOGENON_FIRE,
	AIMODE_EXOGENON_MOVEUP,
	AIMODE_EXOGENON_SCAN,
	AIMODE_EXOGENON_SWEEP,
};

enum
{
	AIMODE_FLESHMORPH_IDLE,
	AIMODE_FLESHMORPH_MOVEFORWARDS,
	AIMODE_FLESHMORPH_STOPFORWARD,
	AIMODE_FLESHMORPH_MOVEBACKWARDS,
	AIMODE_FLESHMORPH_STOPBACKWARD,
};



// Ai Status.... Enemy->AIFlags
constexpr auto AI_PLAYERINRANGE = (1<<1);
constexpr auto AI_ANYPLAYERINRANGE = (1<<2);
constexpr auto AI_ICANSEEPLAYER = (1<<3);
constexpr auto AI_IHAVESEENPLAYER = (1<<4);
constexpr auto AI_FRIENDLYFIRE = (1<<5); // If I fire at my target I might cause friendly fire....
constexpr auto AI_PLAYERCANSEEME = (1<<6);
constexpr auto AI_PLAYERISTARGETINGME = (1<<7);
constexpr auto AI_IVEBEENHITBYPLAYER = (1<<9);
constexpr auto AI_ICANHEARPLAYER = (1<<10);
constexpr auto AI_PLAYERHASFIREDPRIMARY = (1<<11);
constexpr auto AI_PLAYERHASFIREDSECONDARY = (1<<12);
constexpr auto AI_TARGETED = (1<<13);
constexpr auto AI_TARGETED_ONEOFF = (1<<14);
constexpr auto AI_MINEISCLOSE = (1<<15);
constexpr auto AI_MINEAVOID = (1<<16);



// Behaviour
constexpr auto AI_BEHAVIOUR_LEAVENETWORK = (1<<0); // Can I Leave My Network to persue Target...
constexpr auto AI_BEHAVIOUR_NOFRIENDLYFIRE = (1<<1); // Do I Care About Friendly Fire...
constexpr auto AI_BEHAVIOUR_AVOID_PRIMARY = (1<<2);
constexpr auto AI_BEHAVIOUR_AVOID_SECONDARY = (1<<3);
constexpr auto AI_BEHAVIOUR_AVOID_MINES = (1<<4);
constexpr auto AI_BEHAVIOUR_ANTICIPATE_MOVE = (1<<5);
constexpr auto AI_BEHAVIOUR_ATTACK_ONSITE = (1<<6); // If I See the player Should I Attack him...
constexpr auto AI_BEHAVIOUR_ATTACK_PROVOKE = (1<<7); // If the Player Hits me should I Hit Back...
constexpr auto AI_BEHAVIOUR_ATTACK_FIND = (1<<8); // Can I Find the player if I have allready attacked him...
constexpr auto AI_BEHAVIOUR_NETWORKRANGE = (1<<9); // Do I Have a max range that I can Wander from my network???
constexpr auto AI_BEHAVIOUR_WEAPONKNOWLEDGE = (1<<10); // Can I Take advantage of certain weapon charcteristics..ie suss gun limited range??
constexpr auto AI_BEHAVIOUR_ICANTPITCH = (1<<11); // I cant turn in the X..So I must try and get level with the player by sliding up and down...
constexpr auto AI_BEHAVIOUR_ICANTYAW = (1<<12); // I cant turn in the Y..So I must try and get level with the player by sliding left and right...
constexpr auto AI_BEHAVIOUR_ATTACKMINES = (1<<13); // If I see a Mine Should I Attack It..
constexpr auto AI_BEHAVIOUR_AVOIDMINES = (1<<14); // If I see a Mine Should I Avoid It..
constexpr auto AI_BEHAVIOUR_DROPMINES = (1<<15); // If My Nearest Node is a mine dropping node then I can Drop Mines..
constexpr auto AI_BEHAVIOUR_RAMMINES = (1<<16); // If I find a mine ram the bastard....
constexpr auto AI_BEHAVIOUR_RETREAT = (1<<17); // If I Get hurt bad I should run away..
constexpr auto AI_BEHAVIOUR_DONTSTOPANDTURN = (1<<18); // should I stop when I turn...
constexpr auto AI_BEHAVIOUR_PRECISECONTROL = (1<<19); // No bouncy inertial shit...Robotic and Menacing movements...
constexpr auto AI_BEHAVIOUR_CLOAK = (1<<20); // I Can Cloak...
constexpr auto AI_BEHAVIOUR_NOBOB = (1<<21); // I Dont Bob...
constexpr auto AI_BEHAVIOUR_NOTURN = (1<<22); // I Dont Turn..
constexpr auto AI_BEHAVIOUR_RANDSECONDARYFIREPOS = (1<<23); // Fire my secondary bullets randomly...


constexpr auto AI_CONTROL_FORWARD = (1<<0);
constexpr auto AI_CONTROL_BACK = (1<<1);
constexpr auto AI_CONTROL_LEFT = (1<<2);
constexpr auto AI_CONTROL_RIGHT = (1<<3);
constexpr auto AI_CONTROL_UP = (1<<4);
constexpr auto AI_CONTROL_DOWN = (1<<5);
constexpr auto AI_CONTROL_TURNLEFT = (1<<6);
constexpr auto AI_CONTROL_TURNRIGHT = (1<<7);
constexpr auto AI_CONTROL_TURNUP = (1<<8);
constexpr auto AI_CONTROL_TURNDOWN = (1<<9);


constexpr auto AI_CONTROL_NO_FORWARD = (1<<11);
constexpr auto AI_CONTROL_NO_BACK = (1<<12);
constexpr auto AI_CONTROL_NO_LEFT = (1<<13);
constexpr auto AI_CONTROL_NO_RIGHT = (1<<14);
constexpr auto AI_CONTROL_NO_UP = (1<<15);
constexpr auto AI_CONTROL_NO_DOWN = (1<<16);

constexpr auto AI_CONTROL_COLLISION = (1<<17);




// Avoidance Types....
constexpr auto AVOID_LEFT = (AI_CONTROL_LEFT);
constexpr auto AVOID_RIGHT = (AI_CONTROL_RIGHT);
constexpr auto AVOID_UP = (AI_CONTROL_UP);
constexpr auto AVOID_DOWN = (AI_CONTROL_DOWN);
constexpr auto AVOID_FORWARD = (AI_CONTROL_FORWARD);



//--------------------------------------------------------------------------
// Define AI mode timer values
//--------------------------------------------------------------------------

constexpr float ONE_SECOND = (60.0F);

constexpr auto RESET_IDLE_TIME = (ONE_SECOND * 1.0F);
constexpr auto RESET_SCAN_TIME = (ONE_SECOND * 0.25F);
constexpr auto RESET_VALIDATE_TIME = (ONE_SECOND * 0.25F);
constexpr auto RESET_FIRE_TIME = (ONE_SECOND * 0.25F);

constexpr auto PINGOFFTIME = (ONE_SECOND * 3.0F);

//--------------------------------------------------------------------------
// Define Primary Weapon Data...
//--------------------------------------------------------------------------
typedef struct PRIMARYWEAPONKNOWLEDGE{
	float	MinDistance;		// How close should I get to make the weapon less effective....
	float	AccuracyAngle;		// How accurate does the player have to be to hit me.... 
}PRIMARYWEAPONKNOWLEDGE;



//--------------------------------------------------------------------------
// Define TARGETTING data structure
//--------------------------------------------------------------------------

typedef struct {

	VECTOR			Pos;									// Initial search position
	u_int16_t			Group;									// Initial Search Group..
	float			PosDist;								// Distance of 'Pos' from targetting SHIP
	float			Nearest;								// Distance to nearest target
	u_int32_t			Flags;								// Flags - see below
	void	*		SObject;								// Pointer to targetting SHIP   
	void	*		TObject;								// Pointer to target
	u_int32_t			TFlags;								// Flags - see below
} TARGETDATA;

//--------------------------------------------------------------------------
// Define Aim data structure
//--------------------------------------------------------------------------

typedef struct {
	int32_t	Flags;								// Flags - Say wether to turn left right etc...
	VECTOR	Angle;								// The difference in x y z...
}AIMDATA;
//--------------------------------------------------------------------------
// Define TARGETTING flags / macros
//--------------------------------------------------------------------------

constexpr auto _TARGET_CURRENT = (1 << 0); // Target in current GROUP only
constexpr auto _TARGET_ENEMIES = (1 << 1); // Target ENEMIES
constexpr auto _TARGET_PLAYERS = (1 << 2); // Target PLAYERS
constexpr auto _TARGET_NODES = (1 << 3); // Target NODES
constexpr auto _TARGET_PICKUPS = (1 << 4); // Target PICKUPS
constexpr auto _TARGET_TRIGGERS = (1 << 5); // Target TRIGGERS
constexpr auto _TARGET_MINES = (1 << 6); // Target MINES

#define	SET_TARGET_PLAYERS	(Tinfo->Flags |= _TARGET_PLAYERS)
#define	SET_TARGET_NODES	(Tinfo->Flags |= _TARGET_NODES)

#define	TARGET_PLAYERS			(Tinfo->Flags & _TARGET_PLAYERS)
#define	TARGET_NODES			(Tinfo->Flags & _TARGET_NODES)
#define	TARGET_MINES			(Tinfo->Flags & _TARGET_MINES)

constexpr float BIGDISTANCE = (32000.0F);


//--------------------------------------------------------------------------
// External data
//--------------------------------------------------------------------------

extern TARGETDATA *				Tinfo;
extern	GUNTYPE	GunTypes[];
extern	ENEMY_TYPES	EnemyTypes[ MAX_ENEMY_TYPES ];
extern	float	framelag;
extern	float	SoundInfo[MAXGROUPS][MAXGROUPS];
extern	PRIMARYWEAPONKNOWLEDGE PrimaryKnowledge[];

//--------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------

// ailib.cpp

void AI_GetDistToNearestTarget( ENEMY * Enemy );
bool AI_InViewCone( VECTOR * Pos, MATRIX * Mat ,VECTOR * TPos, float ViewConeCos );
bool AI_ClearLOS( VECTOR * SPos, u_int16_t Group , VECTOR * Pos );
bool AI_ClearLOSNonZero( OBJECT * SObject, VECTOR * Pos , float radius );
bool AI_ClearLOSNonZeroNonObject( VECTOR * SPos, u_int16_t Group , VECTOR * Pos , float radius );
void AI_SetIDLE( ENEMY * Enemy );
void AI_SetSCAN( ENEMY * Enemy );
void AI_SetMOVETOTARGET( ENEMY * Enemy );
void AI_SetFIREATTARGET( ENEMY * Enemy );
void AI_AimAtTarget( MATRIX * InvMat , VECTOR * SPos, VECTOR * TPos );
void AI_GetNearestNODETarget( OBJECT * Object );
void AI_LookAhead( float Accuracy, VECTOR * SPos ,OBJECT * TObject , VECTOR * NewPos , float SpeedOfBullet );
void AI_SetFOLLOWPATH( ENEMY * Enemy );
void AI_SetDOGFIGHT( ENEMY * Enemy );
void AI_SetKILLMINE( ENEMY * Enemy );
void AI_GetNearestMINETarget( void );
void AI_SetRETREAT( ENEMY * Enemy );

// aimain.cpp
void AI_AIR( ENEMY * Enemy );
void AI_TURRET( ENEMY * Enemy );
/* Aim solution shared by every brain (defined in ailib.cpp). */
extern AIMDATA AimData;
/* Fleshmorph's home position, set at level load (defined in aifleshmorph.cpp). */
extern VECTOR FleshmorphPos;

void AI_CRAWL( ENEMY * Enemy );
void AI_THINK( ENEMY * Enemy , bool OverideThinkTime , bool JustCheckPlayerRange );
void AI_AVOIDCHECK( ENEMY * Enemy );
void AI_SPLINE( ENEMY * Enemy );
void AI_EXOGENON( ENEMY * Enemy );
void AI_LITTLEGEEK( ENEMY * Enemy );
void AI_FLESHMORPH( ENEMY * Enemy );
/* The two AI_JUMP entries live in aijump.cpp, defined only in EDITION_REMASTER builds. */
void AI_JUMP( ENEMY * Enemy );
void AI_JUMP_FOLLOWPATH( ENEMY * Enemy );
bool Enemy2EnemyFriendlyFireCheck( ENEMY * SEnemy );

// aiidle.cpp
void AI_AIR_IDLE( ENEMY * Enemy );
void AI_TURRET_IDLE( ENEMY * Enemy );
void AI_CRAWL_IDLE( ENEMY * Enemy );

// aiscan.cpp
void AI_AIR_SCAN( ENEMY * Enemy );
void AI_TURRET_SCAN( ENEMY * Enemy );
void AI_CRAWL_SCAN( ENEMY * Enemy );
void AI_DO_SCAN( ENEMY * Enemy );

// aimovet.cpp
void AI_AIR_MOVETOTARGET( ENEMY * Enemy );

// aidogfight.cpp
void AI_AIR_DOGFIGHT( ENEMY * Enemy );

// aikillmine.cpp
void AI_AIR_KILLMINE( ENEMY * Enemy );


// aifire.cpp
void AI_TURRET_FIREATTARGET( ENEMY * Enemy );
void AI_UPDATEGUNS( ENEMY * Enemy );

// aifollow.cpp
void AI_AIR_FOLLOWPATH( ENEMY * Enemy );
void AI_CRAWL_FOLLOWPATH( ENEMY * Enemy );


// airetreat.cpp
void AI_AIR_RETREAT( ENEMY * Enemy );

// aiformation.cpp
void AI_AIR_FORMATION( ENEMY * Enemy );
void AI_LITTLEGEEK_FORMATION( ENEMY * Enemy );

// aispline.cpp
void AI_SPLINE_FOLLOWPATH( ENEMY * Enemy );


// aicrashandburn.cpp
void AI_AIR_DEATH_CRASHANDBURN( ENEMY * Enemy );
// aipingoff.cpp
void AI_AIR_DEATH_PINGOFF( ENEMY * Enemy );
// aipsycho.cpp
void AI_AIR_DEATH_PSYCHO( ENEMY * Enemy );


// aiexogenon.cpp
void AI_EXOGENON_IDLE( ENEMY * Enemy );
void AI_EXOGENON_MOVEDOWN( ENEMY * Enemy );
void AI_EXOGENON_FIRE( ENEMY * Enemy );
void AI_EXOGENON_MOVEUP( ENEMY * Enemy );
void AI_EXOGENON_SCAN( ENEMY * Enemy );
void AI_EXOGENON_SWEEP( ENEMY * Enemy );

// aifleshmorph.cpp
void AI_FLESHMORPH_IDLE( ENEMY * Enemy );
void AI_FLESHMORPH_MOVEFORWARDS( ENEMY * Enemy );
void AI_FLESHMORPH_STOPFORWARD( ENEMY * Enemy );
void AI_FLESHMORPH_MOVEBACKWARDS( ENEMY * Enemy );
void AI_FLESHMORPH_STOPBACKWARD( ENEMY * Enemy );



//--------------------------------------------------------------------------
 
 
