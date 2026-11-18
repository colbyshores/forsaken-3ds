#pragma once

/*===================================================================
	constants.h -- game-wide size limits

	The fixed capacities that size arrays in more than one module.
	They live here, at the bottom of the include graph, so a header
	can declare an array sized by another module's limit without
	including that module's header -- which is how the old layout
	ended up with headers that only compiled in one include order.

	Plain #defines, no includes: safe from C and C++, and every
	module that used to define one of these still includes this
	header, so nothing that used them lost visibility.

	These are capacities baked into level data, savegames and the
	network protocol. Changing one is a format change, not a tweak.
===================================================================*/

/* Players and teams (network protocol + multiplayer scoreboards). */
constexpr int MAX_PLAYERS = 16; /* was 24, 12 in earlier builds */
constexpr int MAX_TEAMS = 4;
constexpr int MAXMULTIPLES = 8; /* multiple-fire mount points per ship */

/* World. */
constexpr int MAXGROUPS = 128; /* visibility groups per level */
#define MAXBIKETYPES			17		/* selectable bikes (ship models) */

/* Weapons. */
constexpr int MAXPRIMARYWEAPONS = 6;
constexpr int MAXSECONDARYWEAPONS = 11;

/* Network resync queues: pickups / mines a client missed. */
constexpr int MAXMISSEDPICKUPS = 256;
constexpr int MAXMISSEDMINES = 100;
