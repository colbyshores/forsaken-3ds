#pragma once

/* englishlocal.h -- English in-game message strings
 *
 * String-literal macros (team, pickup and death-method names; network,
 * flag, bounty, pickup and HUD messages), grouped under the .cpp that prints
 * them. Included only through ui/local.h; front-end menu strings are in
 * localtitle.h. Many are printf formats: the trailing comments give the
 * arguments, and the sprintf calls must pass matching types.
 */

// in the following text:
// <team name>=any one of LT_TEAM_??? defined below

// text.cpp
#define LT_TEAM_1		"Red"
#define LT_TEAM_2		"Green"
#define LT_TEAM_3		"Blue" 
#define LT_TEAM_4		"Yellow"

#define LIVES			"Lives"

// networking
#define IS_JOINING_THE_GAME "is joining the game"		// player is joining the games
#define YOU_HAVE_BECOME_THE_HOST "You Have Become The Host"
#define HAS_LEFT_THE_GAME "has left the game"
#define FROM_YOUR_OWN_TEAM "from your own team"
#define WITH_THE_BOUNTY "with the bounty"
#define NO_POINTS_FOR_KILLING_PLAYER_WITHOUT_THE_BOUNTY "no points for killing %s without the bounty" // %s= Name of Player
#define ON_HIS_OWN_TEAM "on his own team"
#define PACKETS_PER_SECOND_SET "Packets per second set"
#define THE_COLOUR_TEAM_HAVE_SCORED "the %s team have scored" // %s=<team name>
#define THE_COLOUR_TEAM_ARE_RETURNING_THEIR_FLAG "the %s team are returning their flag"	// %s=<team name>
#define THE_COLOUR_TEAM_FLAG_HAS_BEEN_RETURNED "the %s team flag has been returned"	// %s=<team name>
#define THE_COLOUR_TEAM_FLAG_HAS_DRIFTED_INTO_THEIR_GOAL "the %s team flag has drifted into their goal" // %s=<team name>
#define THE_COLOUR_TEAM_FLAG_HAS_ESCAPED_FROM_THEIR_GOAL "the %s team flag has escaped from their goal"	// %s=<team name>
#define HAS_GOT_THE_BOUNTY "has got the bounty"
#define ON_YOUR_OWN_TEAM "On Your Own Team"

// pickups.cpp
// in the following text:
// <player name>=any player's name (typed in by player)
// <pickup name>=any one of LT_PICKUP_??? defined below

#define LT_PICKUP_TROJAX				"Trojax"						// Primary Weapons
#define LT_PICKUP_PYROLITE				"Pyrolite Rifle"
#define LT_PICKUP_TRANSPULSE			"Transpulse Cannon"
#define LT_PICKUP_SUSSGUN				"Suss-Gun"
#define LT_PICKUP_LASER					"Beam Laser"
#define LT_PICKUP_MUG					"Mug Missile"					// Secondary Weapons
#define LT_PICKUP_MUG_PACK				"Mug Missiles"
#define LT_PICKUP_SOLARIS				"Heatseeker Missile"
#define LT_PICKUP_SOLARIS_PACK			"Heatseeker Missile Pack"
#define LT_PICKUP_THIEF					"Thief Missile"
#define LT_PICKUP_SCATTER				"Scatter Missile"
#define LT_PICKUP_GRAVGON				"Gravgon Missile"
#define LT_PICKUP_MFRL					"Rocket Launcher"
#define LT_PICKUP_TITAN					"TitanStar Missile"
#define LT_PICKUP_PURGE					"Purge Mine Pack"
#define LT_PICKUP_PINE					"Pine Mine Pack"
#define LT_PICKUP_QUANTUM				"Quantum Mine Pack"
#define LT_PICKUP_SPIDER				"Spider Mine Pack"
#define LT_PICKUP_PARASITE				"Parasite Mine"
#define LT_PICKUP_FLARE					"Flare"
#define LT_PICKUP_GENAMMO				"General Ammo"					// Ammo
#define LT_PICKUP_PYROFUEL				"Pyrolite Fuel"
#define LT_PICKUP_SUSSAMMO				"Suss-Gun Ammo"
#define LT_PICKUP_POWER_POD				"Power Pod"					// Extras
#define LT_PICKUP_SHIELD				"Shield"
#define LT_PICKUP_INVULBERABILITY		"Invulnerability"
#define LT_PICKUP_EXTRA_LIFE			"Extra Life"
#define LT_PICKUP_TARGETING_COMPUTER	"Targeting Computer"
#define LT_PICKUP_SMOKE					"Smoke Streamer"
#define LT_PICKUP_NITRO					"Nitro"
#define LT_PICKUP_GOGGLES				"Goggles"
#define LT_PICKUP_GOLD_BARS				"Gold Bars"
#define LT_PICKUP_STEALTH				"Stealth Mantle"
#define LT_PICKUP_CRYSTAL				"Crystal"
#define LT_PICKUP_ORBITAL				"Orbit Pulsar"
#define LT_PICKUP_GOLDEN_POWER_POD		"Golden Power Pod"
#define LT_PICKUP_DNA					"DNA"
#define LT_PICKUP_BOMB					"Bomb"
#define LT_PICKUP_GOLDEN_IDOL			"Golden Idol"

#define YOU_ALREADY_HAVE				"You already have %s" // %s=<pickup name>
#define YOU_ALREADY_HAVE_A				"You already have a %s" // %s=<pickup name>
#define YOU_ALREADY_HAVE_MAX			"You already have max %s" // %s=<pickup name>
#define FULL_POWER_LEVEL				"Full Power Level"
#define POWER_POD_LEVEL					"Power Pod Level %d" // %d=1, 2 or 3
#define YOU_ALREADY_HAVE_MAX_POWER		"You already have max power"
#define YOU_CANT_HANDLE_ANY_MORE		"You can't handle any more"
#define YOUVE_EARNED_EXTRA_LIFE			"You've earned yourself an extra life..."
#define YOU_HAVE_ONE_GOLD_BAR			"You now have %hd gold bar" // %hd=1
#define YOU_HAVE_GOLD_BARS				"You now have %hd gold bars" // %hd=2, 3, 4...
#define YOU_HAVE_ONE_CRYSTAL			"You now have %d crystal" // %d=1
#define YOU_HAVE_CRYSTALS				"You now have %d crystals" // %d=2, 3, 4...

#define TAKE_FLAG_TO_GOAL				"take the flag to the %s goal" // %s=<team name>
#define SOMEONE_HAS_GOT_THE_FLAG		"%s on the %s team has got the flag" // %s1=<player name>, %s2=<team name>
#define YOU_HAVE_GOT_THE_BOUNTY			"you have found the bounty" 
#define TEAM_FLAG_RETURNED				"the %s flag has been returned" // %s=<team name>
#define RETURN_TEAM_FLAG				"return the flag to the %s goal" // %s=<team name>
#define CANNOT_PICKUP_OWN_FLAG			"you cannot pick up your own team's flag"
#define SOMEONE_HAS_OTHER_TEAM_FLAG		"%s on the %s team has got the %s flag" // %s1=<player name>, %s2=<team name>, %s3=<team name>
#define FLAG_DRIFTED_BACK				"the %s flag drifted back into their goal" // %s=<team name>
#define FLAG_ESCAPED					"the %s flag escaped from their goal" // %s=<team name>

// models.cpp
// in the following text:
// <death method>=any one of the LT_DEATHMETHOD_??? defined below
#define ENEMY_KILLED_YOU					"enemy killed you with %s"		// %s=<death method>
#define YOU_KILLED_YOURSELF_HOW		"you killed yourself with %s"	// %s=<death method>
#define SOMEONE_KILLED_YOU				"%s killed you with %s %s"		// %s1=<player name> %s2=<death method>, %s3=either FROM_YOUR_OWN_TEAM (see following line) or empty
#define FROM_YOUR_OWN_TEAM			"from your own team"
#define YOU_DIED								"you died"

// primary.cpp
#define LT_DEATHMETHOD_KILLED					"a kamikaze"
#define LT_DEATHMETHOD_PURGE_MINE			"a purge mine"
#define LT_DEATHMETHOD_PINE_MINE				"a pine mine"
#define LT_DEATHMETHOD_QUANTUM_MINE		"a quantum mine"
#define LT_DEATHMETHOD_SPIDER_MINE			"a spider mine"
#define LT_DEATHMETHOD_TITAN					"a titan"
#define LT_DEATHMETHOD_TITAN_SHRAPNEL	"titan shrapnel"
#define LT_DEATHMETHOD_SCATTER				"a scatter"
#define LT_DEATHMETHOD_GRAVGON				"a gravgon"
#define LT_DEATHMETHOD_SOLARIS				"a solaris"
#define LT_DEATHMETHOD_MFRL					"the mfrl"
#define LT_DEATHMETHOD_MUG						"a mug"
#define LT_DEATHMETHOD_THEIF					"a theif missile"
#define LT_DEATHMETHOD_PINE_MISSILE			"a pine mine missile"
#define LT_DEATHMETHOD_PULSAR					"pulsars"
#define LT_DEATHMETHOD_TROJAX					"trojax"
#define LT_DEATHMETHOD_PYROLITE				"pyrolite"
#define LT_DEATHMETHOD_TRANSPULSE			"transpulse"
#define LT_DEATHMETHOD_SUSS_GUN				"suss-gun"
#define LT_DEATHMETHOD_LASER					"lasers"
#define LT_DEATHMETHOD_ORBITOR				"an orbitor"
#define LT_DEATHMETHOD_NO_PRIMARY			"no primary weapon rofl"
#define LT_DEATHMETHOD_NO_SECONDARY		"no secondary weapon rofl"
#define LT_DEATHMETHOD_NME_BULLET1			"an enemy bullet"
#define LT_DEATHMETHOD_NME_PULSAR			"enemy pulsars"
#define LT_DEATHMETHOD_NME_TROJAX			"enemy trojax"
#define LT_DEATHMETHOD_NME_PYROLITE		"enemy pyrolite"
#define LT_DEATHMETHOD_NME_TRANSPULSE	"enemy transpulse"
#define LT_DEATHMETHOD_NME_SUSS_GUN		"enemy suss-gun"
#define LT_DEATHMETHOD_NME_LASER			"enemy lasers"
#define LT_DEATHMETHOD_NME_LIGHTNING		"lightning lasers"
#define LT_DEATHMETHOD_FLAMES					"flames"
#define LT_DEATHMETHOD_NME_POWERLASER	"power laser"
#define LT_DEATHMETHOD_ENEMYSPIRALMISSILE		"enemy spiral missile"
#define LT_DEATHMETHOD_NMEHOMINGMISSILE			"enemy homing missile"
#define LT_DEATHMETHOD_ENEMYBLUEHOMINGMISSILE	"enemy blue homing missile"
#define LT_DEATHMETHOD_ENEMYFIREBALL			"enemy fireball"
#define LT_DEATHMETHOD_ENEMYTENTACLE			"enemy tentacle"
#define LT_DEATHMETHOD_ENEMYDEPTHCHARGE			"enemy depth charge"

#define YOU_DONT_HAVE_ANY_AMMO				"You don't have any ammo"
#define YOU_DONT_HAVE_THAT_WEAPON		"You don't have that weapon"
#define LASER_OVERHEATED							"Laser Overheated"

// ships.cpp
#define TEAM_SCORED						"the %s team have scored" // %s=<team name>
#define OTHER_TEAM_FLAG_RETURNED		"the %s team have returned their flag" // %s=<team name>
#define YOU_NEED_FLAG					"you need the %s flag to score" // %s=<team name>
#define YOU_KILLED_YOURSELF				"you killed yourself"
#define NO_LIVES_LEFT					"No Lives Left"
#define LAST_LIFE						"Last Life"
#define ONE_LIFE_LEFT					"%d Life Left" // %d=1
#define LIVES_LEFT						"%d Lives Left" // %d=2, 3, 4...
#define RESTART_ACTIVATED				"ReStart Position Activated"

#define SHIELD_CRITICAL					"shield critical"
#define HULL_CRITICAL					"hull critical"


// screenpolys.cpp
#define OUT_OF_TIME						"Out of time"
#define IF_YOU_CANT_TAKE_THE_HEAT		"If you can't take the heat"
#define GET_OUT_OF_THE_KITCHEN			"get out of the kitchen...."

// title.cpp: default pilot name





#define DEFAULT_NEWNAME "newname"
