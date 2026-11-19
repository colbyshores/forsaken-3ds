#pragma once

/* networking.h -- multiplayer game protocol: ship state, messages, events
 *
 * networking.cpp is the game side of net.h. It owns Ships[], WhoIAm (this
 * machine's ship number, UNASSIGNED_SHIP until the host gives one out),
 * GameStatus[] and Names. SendGameMessage packs one MSG_* message into
 * CommBuff and passes it to network_send, or to network_broadcast when `to`
 * is NULL; reliability and channel are chosen per message (ship position
 * updates go sequenced but unreliable, on a channel of their own).
 * network_event takes the backend's join / leave / host / name / data events,
 * and EvaluateMessage unpacks a received message. NetworkGameUpdate runs once
 * a frame in a network game: it pumps the network (ReceiveGameMessages, which
 * also refreshes the tracker while hosting) and sends this ship's updates.
 *
 * The *MSG structs are the wire format, sent as raw memory, so their layout
 * must not change without breaking play with other builds (net_bool_t keeps
 * bool fields int-sized for that reason). SendGameMessage sends nothing while
 * a demo plays back. Ships[] and most of these globals are used by
 * single-player code too, and on 3DS, where net.h is stubbed (net_3ds.cpp).
 */

// TODO in 1.19 we should change this to a byte and merge multiple bools into bit fields
typedef int net_bool_t; // backwards compatibility need bool's in pkt structures to be int's

//
// General Networking
//

#include "game/main.h"
#include "net/net.h"
#include "math/new3d.h"
#include "game/object.h"
#include "game/constants.h"
#include "util/timer.h"

// game tracker
extern char tracker_server[256];
extern int tracker_port;
extern bool tracker_enabled;

// my ship number
constexpr auto UNASSIGNED_SHIP = 0xff; // WhoIAm before host gives me number
extern BYTE	WhoIAm;  // which ship number i am
extern network_player_t * host_network_player; // the host .. NULL if I am host

// don't change these directly...
// instead use:
//   local_port_str.text = "2300"
//   host_port_str.text = "2300"
//   TCPAddress.text = "192.168.0.30"
// the values are parsed before calling network_join and network_setup
extern int local_port;
extern int host_port;
extern char host_address[256];

 /*
 * defines
 */

constexpr auto NOISEFALLOFFFACTOR = ( 1.0F / 120.0F ); // approx 2 seconds....
constexpr int NORMAL_MODE = 0;
constexpr int DEATH_MODE = 1;
constexpr int LIMBO_MODE = 2; // this is what the ship is in while viewing its own death....but has blown up...
// 3 was DEMO_MODE (demo playback, removed); the mode tables keep the slot
constexpr int GAMEOVER_MODE = 4; // this is the mode the player goes into when single player has finished...
constexpr int WATCH_MODE = 5;
constexpr float MAX_SHIELD = 255.0F;
constexpr float START_SHIELD = 128.0F;
constexpr float START_HULL = 128.0F;
constexpr int MAXTEXTMSG = 512; // the number of chars in a message you can send.....
constexpr int MAXGENPICKUPCOUNT = 5;
constexpr int MAXGENREGENSLOTCOUNT = 6;
constexpr int MAXGENMINECOUNT = 3; //4
constexpr int MAXGENTRIGGERCOUNT = 60;
constexpr int MAXGENTRIGVARCOUNT = 60;
constexpr int MAX_PICKUPFLAGS = 2;
constexpr int MAX_BUFFER_SIZE = 1024;
constexpr auto FRAMELAGED_RECOIL = true;
constexpr auto ONEOFF_RECOIL = false;
constexpr int NUMTITANBITS = 10;

extern bool PlayerReady[MAX_PLAYERS];
extern BYTE GameStatus[];

/*
 * structures
 */
/*===================================================================
	Bodge Structures
===================================================================*/
struct SHORTMINE{

	u_int16_t	OwnerType;
	u_int16_t	Owner;
	u_int16_t	BulletID;
	u_int16_t	Group;
	VECTOR	Pos;
	VECTOR	Offset;
	VECTOR	Dir;
	VECTOR	UpVector;
	VECTOR	DropDir;
	u_int16_t	Weapon;
	float	LifeCount;

};

/*===================================================================
	Bodge Structures
===================================================================*/
struct SHORTKILLMINE{
	u_int16_t	OwnerType;			// Owner ID
	u_int16_t	Owner;				// Owner ID
	u_int16_t	ID;					// ID
	float	ShockwaveSize;		// Size
};
/*===================================================================
	Bodge Structures
===================================================================*/

struct MISSEDMINEMSG{
	int16_t	Type;
	void *	Struct;
};

/*===================================================================
	Bodge Structures
===================================================================*/
struct SHORTPICKUP{
	int16_t	Type;
	int16_t	Group;
	u_int16_t	Owner;				// Owner ID
	u_int16_t	ID;					// ID
	VECTOR	Pos;
	VECTOR	Dir;
	int16_t	RegenSlot;
	float	Speed;
	net_bool_t	Sparkle;
	float	LifeCount;
	u_int16_t	TriggerMod;
};

struct SHORTTRIGVAR{
	int		State;
};

struct SHORTTRIGGER{
	int		Active;
};

/*===================================================================
	Bodge Structures
===================================================================*/
struct SHORTREGENSLOT{

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

/*===================================================================
	Bodge Structures
===================================================================*/
struct SHORTKILLPICKUP{
	u_int16_t	Owner;				// Owner ID
	u_int16_t	ID;					// ID
	int16_t	Style;				// Style
};

/*===================================================================
	Bodge Structures
===================================================================*/

struct MISSEDPICKUPMSG{
	int16_t	Type;
	void *	Struct;
};

constexpr int MAXSHORTNAME = 8;
typedef	char		SHORTNAMETYPE[MAX_PLAYERS][MAXSHORTNAME];

typedef struct _SHIPDIEDINFO
{
	BYTE		WeaponType;
	BYTE		Weapon;

} SHIPDIEDINFO;

typedef struct _PICKUPINFO
{
	u_int16_t		IDCount;
	int16_t		Type;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		Dir;
	float		Speed;
	int16_t		RegenSlot;
	net_bool_t		Sparkle;
	float		LifeCount;
	u_int16_t		TriggerMod;
}PICKUPINFO; // 46

typedef struct _VERYSHORTPICKUPINFO
{
	u_int16_t		IDCount;
	BYTE		Type;
	BYTE		Group;
	BYTE		RegenSlot;
	BYTE		Sparkle;
	SHORTVECTOR	Pos;
	SHORTVECTOR	Dir;
	float		Speed;
	float		LifeCount;
	u_int16_t		TriggerMod;
}VERYSHORTPICKUPINFO; // 28


typedef struct _KILLPICKUPINFO
{
	u_int16_t		Owner;
	u_int16_t		IDCount;
	int16_t		Style;
}KILLPICKUPINFO;



typedef struct _TEAMGOALSINFO
{
	u_int16_t		TeamGoals[ MAX_TEAMS ];

}TEAMGOALSINFO;

typedef struct _SHOCKWAVEINFO
{
	u_int16_t		Owner;
	u_int16_t		Group;
	VECTOR		Pos;
	BYTE		Weapon;
	float		ShockwaveSize;
}SHOCKWAVEINFO;

typedef struct _BGOUPDATEINFO
{
	u_int16_t		BGObject;
	int16_t		State;
	float		Time;
}BGOUPDATEINFO;

typedef struct _PRIMBULLPOSDIR
{
	u_int16_t		OwnerType;
	u_int16_t		OwnerID;
	u_int16_t		BulletID;
	int8_t		Weapon;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		Offset;
	VECTOR		Dir;
	VECTOR		Up;
	int16_t		PowerLevel;
	float		PLevel;

}PRIMBULLPOSDIR;

typedef struct _SECBULLPOSDIR
{
	u_int16_t		OwnerType;
	u_int16_t		Owner;
	u_int16_t		BulletID;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		Offset;
	VECTOR		Dir;
	VECTOR		UpVector;
	VECTOR		DropDir;
	int8_t		Weapon;

}SECBULLPOSDIR;

typedef struct _TITANBITS
{
	u_int16_t		OwnerType;
	u_int16_t		Owner;
	u_int16_t		BulletID;
	u_int16_t		Group;
	VECTOR		Pos;
	VECTOR		Offset;
	VECTOR		UpVector;
	VECTOR		DropDir;
	VECTOR		Directions[ NUMTITANBITS ];
	int8_t		Weapon;

}TITANBITS;

typedef struct _SHIPHIT
{
	float		Damage;
	VECTOR		Recoil;
	VECTOR		Point;
	VECTOR		Dir;
	BYTE		WeaponType;
	BYTE		Weapon;
	float		Force;
	net_bool_t		OneOffExternalForce;

}SHIPHIT;


typedef struct _SHORTSHIPHIT
{
	float		Damage;
	float		Force;
	SHORTVECTOR	Recoil;
	SHORTVECTOR	Point;
	SHORTVECTOR	Dir;
	BYTE		WeaponType;
	BYTE		Weapon;
	u_int16_t		OneOffExternalForce;
	u_int16_t		Recoil_Scalar;
}SHORTSHIPHIT;


typedef struct _SETTIME
{
	float		Time;

}SETTIME;

typedef struct _GLOBALSHIP
{
	OBJECT				Object;						//

	BYTE				enable;						// is this ship active?
	BYTE				ShipThatLastKilledMe;		// the ship who killed me last...
	BYTE				ShipThatLastHitMe;			// the ship who killed me last...
	BYTE				NumMultiples;				// Number of multiples
	float				StealthTime;				// Stealth Mode Time Limit
	float				Timer;						// General Purpose Mode Timer....How Long to stay dead ...etc..
	float				InvulTimer;					// HowLong am I Invulnerable
	net_bool_t				Invul;						// Am I Invulnerable...
	VECTOR				LastAngle;					// what my last Step Turn Angles Were...
	u_int16_t				PrimBullIdCount;			// Id count for every bullet I fire....
	u_int16_t				SecBullIdCount;				// Id count for every bullet I fire....
	u_int16_t				PickupIdCount;				// Id count for every pickup I generate.....
	float				Damage;						// how much Damege I Have Taken...
	BYTE				Primary;					// which weapon is my primary....
	BYTE				Secondary;					// which wepaon is my secondary....
	u_int16_t				ModelNum;					// which model to display for them...
	int16_t				BikeNum;					// which model to display for them...
	BYTE				Pickups;
	BYTE				RegenSlots;
	BYTE				Triggers;
	BYTE				TrigVars;
	BYTE				Mines;
	net_bool_t				JustRecievedPacket;			//
	VECTOR				LastMove;					// last movement vector (framelagged)
	VECTOR				Move_Off;					// Last MoveMent...x , y , z
	network_player_t *  network_player;
	u_int16_t				OrbModels[ MAXMULTIPLES ];	// Orbit Pulsar Model
	float				OrbAmmo[ MAXMULTIPLES ];	// Orbit Pulsar Ammo

	float				PrimPowerLevel;				// Trojax Powerlevel
	u_int16_t				PrimID;						// Bullet ID
	u_int16_t				SecID;						// Missile ID
	BYTE				SecWeapon;					// Weapon
	net_bool_t	headlights; // are my headlights on?

	net_bool_t				DemoInterpolate;

	VECTOR		OldPos;								//
	VECTOR		NextPos;							// what my next position will be
	QUAT		OldQuat;							//
	QUAT		NextQuat;							// what my next view angle will be
	float		OldBank;							//
	float		NextBank;							// what my next bank angle will be..


	float		SuperNashramTimer;					// HowLong have i left with the super nashram?
	u_int16_t		TempLines[ 12 ];

	float		ShakeTimer;
	float		ShakeDirTimer;
	float		ShakeForce;

	float		PacketDelay;

	u_int8_t		ShieldHullCount;

	VECTOR		RealPos;
	u_int16_t		RealGroup;

} GLOBALSHIP;


/*		this is all that really needs to be sent	*/
typedef struct _SHORTGLOBALSHIP
{
	u_int32_t				Flags;			// Flags Enable/Stealth/Turbo/PrimFire/SecFire/PrimToggle/SecToggle
	BYTE				Status;			// tells us constantly what this ship is doing...
	BYTE				GroupImIn;		// Group Im In

	VECTOR				Pos;			// x , y , z position
	VECTOR				Move_Off;		// Last MoveMent...x , y , z
	VECTOR				Angle;			// Last Rotation..
	int16_t				Bank;			// How much Am I banked....
	QUAT				Quat;			// Final Quat...
} SHORTGLOBALSHIP;

typedef struct _VERYSHORTGLOBALSHIP
{
	u_int32_t				Flags;			// Flags Enable/Stealth/Turbo/PrimFire/SecFire/PrimToggle/SecToggle
	BYTE				Status;			// tells us constantly what this ship is doing...
	BYTE				GroupImIn;		// Group Im In
	SHORTVECTOR			Pos;			// x , y , z position
	SHORTVECTOR			Move_Off;		// Last MoveMent...x , y , z
	SHORTVECTOR			Angle;			// Last Rotation..
	int16_t				Bank;			// How much Am I banked....
	u_int16_t				Move_Off_Scalar;
	SHORTQUAT			Quat;			// Final Quat...
} VERYSHORTGLOBALSHIP;


typedef struct _FSHORTGLOBALSHIP
{
	u_int32_t				Flags;			// Flags Enable/Stealth/Turbo/PrimFire/SecFire/PrimToggle/SecToggle
	BYTE				Primary;		// Primary weapon
	BYTE				Secondary;		// Secondary weapon
	BYTE				GroupImIn;		// Group Im In
	BYTE				PrimPowerLevel;	// Trojax Powerlevel
	VECTOR				Pos;			// x , y , z position
	VECTOR				Move_Off;		// Last MoveMent...x , y , z
	VECTOR				Angle;			// Last Rotation..
	int16_t				Bank;		// How much Am I banked....
	QUAT				Quat;		// Final Quat...
} FSHORTGLOBALSHIP;

typedef struct _FVERYSHORTGLOBALSHIP
{
	u_int32_t				Flags;			// Flags Enable/Stealth/Turbo/PrimFire/SecFire/PrimToggle/SecToggle
	BYTE				Primary;		// Primary weapon
	BYTE				Secondary;		// Secondary weapon
	BYTE				GroupImIn;		// Group Im In
	BYTE				PrimPowerLevel;	// Trojax Powerlevel
	SHORTVECTOR			Pos;			// x , y , z position
	SHORTVECTOR			Move_Off;		// Last MoveMent...x , y , z
	SHORTVECTOR			Angle;			// Last Rotation..
	int16_t				Bank;		// How much Am I banked....
	u_int16_t				Move_Off_Scalar;
	SHORTQUAT			Quat;		// Final Quat...
} FVERYSHORTGLOBALSHIP;

typedef struct _GROUPONLY_FVERYSHORTGLOBALSHIP
{
	u_int32_t				Flags;			// Flags Enable/Stealth/Turbo/PrimFire/SecFire/PrimToggle/SecToggle
	BYTE				Primary;		// Primary weapon
	BYTE				Secondary;		// Secondary weapon
	BYTE				GroupImIn;		// Group Im In
	BYTE				PrimPowerLevel;	// Trojax Powerlevel
	SHORTVECTOR			Pos;			// x , y , z position
	SHORTQUAT			Quat;		// Final Quat...
	int16_t				Bank;		// How much Am I banked....
} GROUPONLY_FVERYSHORTGLOBALSHIP;

//----------------------------------------------------------
// Mask Defines for .Flags u_int32_t
//----------------------------------------------------------

constexpr int SHIP_Light_Bit = 30;
constexpr int SHIP_CarryingFlag4_Bit = 29;
constexpr int SHIP_CarryingFlag3_Bit = 28;
constexpr int SHIP_CarryingFlag2_Bit = 27;
constexpr int SHIP_CarryingFlag1_Bit = 26;
constexpr int SHIP_CarryingFlag_Bit = 26;
constexpr int SHIP_CarryingBounty_Bit = 26;
constexpr int SHIP_Shield_Bit = 25;
constexpr int SHIP_Scattered_Bit = 24;
constexpr int SHIP_Litup_Bit = 23;
constexpr int SHIP_InWater_Bit = 22;
constexpr int SHIP_SuperNashram_Bit = 21;
constexpr int SHIP_Invul_Bit = 20;
constexpr int SHIP_MulFire_Bit = 19;
constexpr int SHIP_Charging_Bit = 18;
constexpr int SHIP_Turbo_Bit = 17;
constexpr int SHIP_Stealth_Bit = 16;
constexpr int SHIP_PrimFire_Bit = 15;
constexpr int SHIP_PrimToggle_Bit = 14;
constexpr int SHIP_SecFire_Bit = 13;
constexpr int SHIP_SecToggle_Bit = 12;
constexpr int SHIP_IsHost_Bit = 11;
constexpr int SHIP_Enable_Bit = 10;
constexpr int SHIP_Hull_Bit2 = 9;
constexpr int SHIP_Hull_Bit1 = 8;
constexpr int SHIP_Mode_Bit2 = 7;
constexpr int SHIP_Mode_Bit1 = 6;
constexpr int SHIP_PowerLevel_Bit2 = 5;
constexpr int SHIP_PowerLevel_Bit1 = 4;
constexpr int SHIP_NumMultiples_Bit4 = 3;
constexpr int SHIP_NumMultiples_Bit3 = 2;
constexpr int SHIP_NumMultiples_Bit2 = 1;
constexpr int SHIP_NumMultiples_Bit1 = 0;

constexpr auto SHIP_Light = ( 1 << SHIP_Light_Bit );
constexpr auto SHIP_CarryingFlag4 = ( 1 << SHIP_CarryingFlag4_Bit );
constexpr auto SHIP_CarryingFlag3 = ( 1 << SHIP_CarryingFlag3_Bit );
constexpr auto SHIP_CarryingFlag2 = ( 1 << SHIP_CarryingFlag2_Bit );
constexpr auto SHIP_CarryingFlag1 = ( 1 << SHIP_CarryingFlag1_Bit );
constexpr auto SHIP_CarryingFlag = ( 1 << SHIP_CarryingFlag_Bit );
constexpr auto SHIP_CarryingFlags = ( SHIP_CarryingFlag1
							| SHIP_CarryingFlag2
							| SHIP_CarryingFlag3
							| SHIP_CarryingFlag4 );
constexpr auto SHIP_NumMultiplesBit4 = ( 1 << SHIP_NumMultiples_Bit4 );
constexpr auto SHIP_NumMultiplesBit3 = ( 1 << SHIP_NumMultiples_Bit3 );
constexpr auto SHIP_NumMultiplesBit2 = ( 1 << SHIP_NumMultiples_Bit2 );
constexpr auto SHIP_NumMultiplesBit1 = ( 1 << SHIP_NumMultiples_Bit1 );
constexpr auto SHIP_Shield = ( 1 << SHIP_Shield_Bit );
constexpr auto SHIP_HullBit2 = ( 1 << SHIP_Hull_Bit2 );
constexpr auto SHIP_HullBit1 = ( 1 << SHIP_Hull_Bit1 );
constexpr auto SHIP_ModeBit2 = ( 1 << SHIP_Mode_Bit2 );
constexpr auto SHIP_ModeBit1 = ( 1 << SHIP_Mode_Bit1 );
constexpr auto SHIP_PowerLevelBit2 = ( 1 << SHIP_PowerLevel_Bit2 );
constexpr auto SHIP_PowerLevelBit1 = ( 1 << SHIP_PowerLevel_Bit1 );
constexpr auto SHIP_CarryingBounty = ( 1 << SHIP_CarryingBounty_Bit );
constexpr auto SHIP_Scattered = ( 1 << SHIP_Scattered_Bit);
constexpr auto SHIP_Litup = ( 1 << SHIP_Litup_Bit );
constexpr auto SHIP_InWater = ( 1 << SHIP_InWater_Bit );
constexpr auto SHIP_SuperNashram = ( 1 << SHIP_SuperNashram_Bit );
constexpr auto SHIP_Invul = ( 1 << SHIP_Invul_Bit );
constexpr auto SHIP_MulFire = ( 1 << SHIP_MulFire_Bit );
constexpr auto SHIP_Multiple = ( ( 1 << SHIP_NumMultiples_Bit4 )
							| ( 1 << SHIP_NumMultiples_Bit3 )
							| ( 1 << SHIP_NumMultiples_Bit2 )
							| ( 1 << SHIP_NumMultiples_Bit1 ) );
constexpr auto SHIP_Charging = ( 1 << SHIP_Charging_Bit );
constexpr auto SHIP_Turbo = ( 1 << SHIP_Turbo_Bit );
constexpr auto SHIP_Stealth = ( 1 << SHIP_Stealth_Bit );
constexpr auto SHIP_PrimFire = ( 1 << SHIP_PrimFire_Bit );
constexpr auto SHIP_PrimToggle = ( 1 << SHIP_PrimToggle_Bit );
constexpr auto SHIP_SecFire = ( 1 << SHIP_SecFire_Bit );
constexpr auto SHIP_SecToggle = ( 1 << SHIP_SecToggle_Bit );
constexpr auto SHIP_IsHost = ( 1 << SHIP_IsHost_Bit );
constexpr auto SHIP_Enable = ( 1 << SHIP_Enable_Bit );

//----------------------------------------------------------
// communication packet structures
//----------------------------------------------------------
constexpr auto MSG_UPDATE = 0x11;
constexpr auto MSG_HEREIAM = 0x22;
constexpr auto MSG_INIT = 0x33;
constexpr auto MSG_SHIPHIT = 0x44;
constexpr auto MSG_PRIMBULLPOSDIR = 0x55;
constexpr auto MSG_SECBULLPOSDIR = 0x66;
constexpr auto MSG_SHIPDIED = 0x77;
constexpr auto MSG_DROPPICKUP = 0x88;
constexpr auto MSG_KILLPICKUP = 0x99;
constexpr auto MSG_NETSETTINGS = 0xaa;
constexpr auto MSG_STATUS = 0xbb;
constexpr auto MSG_SHORTPICKUP = 0xee;
constexpr auto MSG_SHOCKWAVE = 0xff;
constexpr auto MSG_FUPDATE = 0x1f;
constexpr auto MSG_SHORTMINE = 0x2f;
constexpr auto MSG_TEXTMSG = 0x3f;
constexpr auto MSG_SHORTREGENSLOT = 0x4f;
constexpr auto MSG_SHORTTRIGGER = 0x5f;
constexpr auto MSG_SHORTTRIGVAR = 0x6f;
constexpr auto MSG_INTERPOLATE = 0x8f;
constexpr auto MSG_BGOUPDATE = 0xaf;
constexpr auto MSG_LONGSTATUS = 0xdf;
constexpr auto MSG_SETTIME = 0xef;
constexpr auto MSG_REQTIME = 0xf1;
constexpr auto MSG_BIKENUM = 0xf4;
constexpr auto MSG_VERYSHORTUPDATE = 0xf5;
constexpr auto MSG_VERYSHORTFUPDATE = 0xf6;
constexpr auto MSG_VERYSHORTINTERPOLATE = 0xf8;
constexpr auto MSG_TEAMGOALS = 0xf9;
constexpr auto MSG_YOUQUIT = 0xfa;
constexpr auto MSG_SHORTSHIPHIT = 0xfc;
constexpr auto MSG_TITANBITS = 0xfd;
constexpr auto MSG_GROUPONLY_VERYSHORTFUPDATE = 0xec;
constexpr auto MSG_VERYSHORTDROPPICKUP = 0xed;
constexpr auto MSG_SHIPHEALTH = 0xcc;

typedef struct _YOUQUITMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		You;
}YOUQUITMSG;

typedef struct _BIKENUMMSG
{
	BYTE     MsgCode;
	BYTE     WhoIAm;
	BYTE		BikeNum;		// which model to display for them...
}BIKENUMMSG;

typedef struct _SHORTPICKUPMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		HowManyPickups;
	BYTE		Pickups;
	struct SHORTPICKUP  ShortPickup[MAXGENPICKUPCOUNT];
} SHORTPICKUPMSG;

typedef struct _SHORTREGENSLOTMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		HowManyRegenSlots;
	BYTE		RegenSlots;
	struct SHORTREGENSLOT  ShortRegenSlot[MAXGENREGENSLOTCOUNT];
} SHORTREGENSLOTMSG;

typedef struct _SHORTTRIGGERMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		HowManyTriggers;
	BYTE		Triggers;
	struct SHORTTRIGGER  ShortTrigger[MAXGENTRIGGERCOUNT];
} SHORTTRIGGERMSG;

typedef struct _SHORTTRIGVARMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		HowManyTrigVars;
	BYTE		TrigVars;
	struct SHORTTRIGVAR  ShortTrigVar[MAXGENTRIGVARCOUNT];
} SHORTTRIGVARMSG;

typedef struct _SHORTMINEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BYTE		HowManyMines;
	BYTE		Mines;
	struct SHORTMINE  ShortMine[MAXGENMINECOUNT];
} SHORTMINEMSG;

typedef struct _UPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	SHORTGLOBALSHIP  ShortGlobalShip;
} UPDATEMSG;

typedef struct _VERYSHORTUPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	VERYSHORTGLOBALSHIP  ShortGlobalShip;
} VERYSHORTUPDATEMSG;

typedef struct _FUPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	FSHORTGLOBALSHIP  ShortGlobalShip;
} FUPDATEMSG;

typedef struct _VERYSHORTFUPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	FVERYSHORTGLOBALSHIP  ShortGlobalShip;
} VERYSHORTFUPDATEMSG;

typedef struct _GROUPONLY_VERYSHORTFUPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	GROUPONLY_FVERYSHORTGLOBALSHIP  ShortGlobalShip;
} GROUPONLY_VERYSHORTFUPDATEMSG;


typedef struct _HEREIAMMSG
{
	BYTE		MsgCode;
	BYTE		WhoIAm;
	BYTE		MPVersion;
} HEREIAMMSG;

// later we should probably shove these all into a bit stream
// for now this is much simplar and this message is only sent once to new players
// converting all the bool's to BYTE's (even with all the new added fields) saved 72 bits...

typedef struct _INITMSG
{
	BYTE	MsgCode;

	// boolEANS

	BYTE	PlayerReady[MAX_PLAYERS];
	BYTE	RandomPickups;
	BYTE	TeamGame;
	BYTE	CaptureTheFlag;
	BYTE	CTF;
	BYTE	BountyHunt;
	BYTE	ResetKillsPerLevel;
	BYTE	UseShortPackets;

	// others

	BYTE	WhoIAm;
	BYTE	YouAre;
	BYTE	OverallGameStatus;
	BYTE	NumPrimaryPickups;
	BYTE	MaxKills;
	BYTE	TeamNumber[MAX_PLAYERS];
	BYTE	GameStatus[MAX_PLAYERS];
	BYTE	ColPerspective;
	BYTE	CTF_Type;
	BYTE	TimeLimit;
	BYTE	KillStats[MAX_PLAYERS][MAX_PLAYERS];
	BYTE	BonusStats[MAX_PLAYERS];
	BYTE	KillCounter[MAX_PLAYERS];

	u_int16_t	RandomStartPosModify;
	u_int16_t	Seed1;
	u_int16_t	Seed2;

	int16_t	GoalScore;
	int16_t	BountyBonusInterval;

	u_int32_t	PickupFlags[MAX_PICKUPFLAGS];

	float	NetUpdateInterval;

	char	LevelName[32];


} INITMSG;

typedef struct _SHIPHITMSG
{
	BYTE     MsgCode;
	BYTE		WhoHitYou;
	BYTE     You;
	SHIPHIT	ShipHit;
} SHIPHITMSG;

typedef struct _SHORTSHIPHITMSG
{
	BYTE		MsgCode;
	BYTE		WhoHitYou;
	BYTE		You;
	SHORTSHIPHIT	ShipHit;
} SHORTSHIPHITMSG;


typedef struct _SHIPDIEDMSG
{
	BYTE     MsgCode;
	BYTE     WhoIAm;
	BYTE     WhoKilledMe;
	BYTE		Type;
	BYTE		WeaponType;
	BYTE		Weapon;
	VECTOR	Pos;
} SHIPDIEDMSG;


typedef struct _KILLPICKUPMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	KILLPICKUPINFO	KillPickupInfo;

} KILLPICKUPMSG;

typedef struct _TEAMGOALSMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	TEAMGOALSINFO	TeamGoalsInfo;

} TEAMGOALSMSG;

typedef struct _SHOCKWAVEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	SHOCKWAVEINFO	ShockwaveInfo;

} SHOCKWAVEMSG;

typedef struct _BGOUPDATEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	BGOUPDATEINFO	BGOUpdateInfo;

} BGOUPDATEMSG;

typedef struct _DROPPICKUPMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	PICKUPINFO	PickupInfo;

} DROPPICKUPMSG;

typedef struct _VERYSHORTDROPPICKUPMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	VERYSHORTPICKUPINFO	PickupInfo;

} VERYSHORTDROPPICKUPMSG;

typedef struct _PRIMBULLPOSDIRMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	PRIMBULLPOSDIR	PrimBullPosDir;
} PRIMBULLPOSDIRMSG;

typedef struct _SECBULLPOSDIRMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	SECBULLPOSDIR	SecBullPosDir;
} SECBULLPOSDIRMSG;

typedef struct _TITANBITSMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	TITANBITS	TitanBits;
} TITANBITSMSG;

typedef struct _STATUSMSG
{
	BYTE     MsgCode;
	BYTE     WhoIAm;
	net_bool_t		IsHost;			// from host ???
	BYTE		Status;
	BYTE		TeamNumber;
	net_bool_t		IAmReady;		// used for team game - game cannot start until everyone is ready
	BYTE		Pickups;			// tells how much of the pickup list I have recieved..
	BYTE		RegenSlots;		// tells how much of the pickup regen slots list I have recieved..
	BYTE		Mines;			// tells how much of the mine list I have recieved..
	BYTE		Triggers;			// tells how much of the mine list I have recieved..
	BYTE		TrigVars;		// tells how much of the mine list I have recieved..
} STATUSMSG;

typedef struct _NETSETTINGSMSG
{
	BYTE		MsgCode;
	BYTE		WhoIAm;
	net_bool_t		IsHost;
	float		PacketsPerSecond;
	net_bool_t		CollisionPerspective;
	net_bool_t		ShortPackets;
} NETSETTINGSMSG;

typedef struct _LONGSTATUSMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	STATUSMSG	Status;
	char		LevelName[32];
	u_int16_t		RandomStartPosModify;
} LONGSTATUSMSG;

enum{
	TEXTMSGTYPE_Taunt1,
	TEXTMSGTYPE_Taunt2,
	TEXTMSGTYPE_Taunt3,
	TEXTMSGTYPE_TitleMessage,
	TEXTMSGTYPE_JoiningTeamGame,
	TEXTMSGTYPE_EnteredWatchMode,
	TEXTMSGTYPE_ExitedWatchMode,
	TEXTMSGTYPE_QuickTaunt,
	TEXTMSGTYPE_CaptureFlagMessage,
	TEXTMSGTYPE_ScoredWithFlag,
	TEXTMSGTYPE_ReturningFlag,
	TEXTMSGTYPE_ReturnedFlag,
	TEXTMSGTYPE_FlagDriftedIn,
	TEXTMSGTYPE_FlagEscaped,
	TEXTMSGTYPE_BountyMessage,
	TEXTMSGTYPE_QuickTauntWhisper,
	TEXTMSGTYPE_SpeechTaunt,
};

typedef struct _TEXTMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	char		Text[MAXTEXTMSG];
	BYTE		TextMsgType;
} TEXTMSG;


typedef struct _SETTIMEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;
	SETTIME		TimeInfo;

} SETTIMEMSG;


typedef struct _REQTIMEMSG
{
	BYTE        MsgCode;
	BYTE        WhoIAm;

} REQTIMEMSG;

typedef struct _SHIPHEALTHMSG
{
	BYTE MsgCode;
	BYTE WhoIAm;
	u_int8_t Hull;
	u_int8_t Shield;
} SHIPHEALTHMSG;


// expose Names to all that care
extern SHORTNAMETYPE Names;

extern GLOBALSHIP Ships[MAX_PLAYERS+1];
extern BYTE Current_Camera_View; // which object is currently using the camera view....
constexpr auto INVALID_CAMERA_VIEW = MAX_PLAYERS + 2;
#define CAMERA_VIEW_IS_VALID (Current_Camera_View >= 0 && Current_Camera_View < MAX_PLAYERS+1)

/*
 * fn prototypes
 */
void	DestroyGame( void );
void	SendGameMessage( BYTE msg, network_player_t * to, BYTE row, BYTE col, BYTE mask );
void	EvaluateMessage( network_player_t * from, DWORD len , BYTE * MsgPnt );
void	ReceiveGameMessages( void );
void	initShip( u_int16_t i );
void	NetworkGameUpdate();
void	SetupNetworkGame();
void	PrimBullPosDirSend( u_int16_t OwnerType, u_int16_t OwnerID, u_int16_t BulletID, int8_t Weapon,
						u_int16_t Group, VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * Up,
						int16_t PowerLevel, float PLevel );
void SecBullPosDirSend( u_int16_t OwnerType, u_int16_t Owner, u_int16_t BulletID, u_int16_t Group,
						VECTOR * Pos, VECTOR * Offset, VECTOR * Dir, VECTOR * UpVector,
						VECTOR * DropDir, int8_t Weapon );
void TitanBitsSend( u_int16_t OwnerType, u_int16_t Owner, u_int16_t BulletID, u_int16_t Group,
						VECTOR * Pos, VECTOR * Offset, VECTOR * UpVector,
						VECTOR * DropDir, int8_t Weapon, VECTOR * Directions );
void	DropPickupSend( VECTOR * Pos, u_int16_t Group, VECTOR * Dir, float Speed, int16_t Type, u_int16_t IDCount, int16_t RegenSlot, bool Sparkle, float LifeCount, u_int16_t TriggerMod);
void	KillPickupSend( u_int16_t Owner, u_int16_t IDCount, int16_t Style );
void	CreateShockwaveSend( u_int16_t OwnerType, u_int16_t Owner, VECTOR * Pos, u_int16_t Group, float ShockwaveSize, BYTE Weapon );
void	IHitYou( BYTE you, float Damage, VECTOR * Recoil, VECTOR * Point, VECTOR * Dir, float Force, BYTE WeaponType, BYTE Weapon, bool FramelagRecoil );
void	ShipDiedSend( BYTE WeaponType, BYTE Weapon );
void	UpdateBGObjectSend( u_int16_t BGObject, int16_t State, float Time );
void	smallinitShip( u_int16_t i );
int FindSameLevel( char * Name );
void	RequestTime( void  );
void	SetTime( float Time );

void set_my_player_name( void );

/* Defined in networking.cpp; shared with the modules that use it. */
extern float PrevHull;
extern float PrevShield;
extern BYTE MyGameStatus;
extern int16_t Lives;
extern int16_t BikeModels[ MAXBIKETYPES ];
extern bool IsHost;
void TeamGoalsSend( u_int16_t * TeamGoals );
extern SHIPHEALTHMSG PlayerHealths[ MAX_PLAYERS+1 ];
extern u_int8_t ShipHealthColour[ MAX_PLAYERS+1 ];
extern u_int32_t RecPacketSize;
extern u_int32_t MaxRecPacketSize;
extern u_int32_t CurrentBytesPerSecRec;
extern u_int32_t CurrentBytesPerSecSent;
extern u_int32_t MaxCurrentBytesPerSecRec;
extern u_int32_t MaxCurrentBytesPerSecSent;
extern int16_t NextworkOldBikeNum;
extern int16_t MaxKills;
extern BYTE OverallGameStatus;
extern int16_t NamesAreLegal;
extern MISSEDPICKUPMSG MissedPickups[ MAXMISSEDPICKUPS ];
extern SHORTPICKUP MissedInitPickups[ MAXMISSEDPICKUPS ];
extern SHORTKILLPICKUP MissedKillPickups[ MAXMISSEDPICKUPS ];
extern int16_t NumMissedPickups;
extern int16_t NumMissedInitPickups;
extern int16_t NumMissedKillPickups;
extern MISSEDMINEMSG MissedMines[ MAXMISSEDMINES ];
extern SHORTMINE MissedInitMines[ MAXMISSEDMINES ];
extern int16_t NumMissedMines;
extern int16_t NumMissedInitMines;
extern MATRIX TempMatrix;
extern int OldPPSValue;
extern int OldColPerspective;
extern int OldUseShortPackets;
extern bool UseShortPackets;
extern float NetUpdateInterval;
extern bool MyUseShortPackets;
extern px_timer_t LastPacketTime[ MAX_PLAYERS+1 ];
extern bool HostDuties;
