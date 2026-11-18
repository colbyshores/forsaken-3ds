#pragma once

/* constants.h -- game-wide size limits
 *
 * The fixed capacities that size arrays in more than one module. They sit at
 * the bottom of the include graph (no includes of their own), so a header can
 * size an array by another module's limit without including that module's
 * header.
 *
 * Most of these are baked into level data, savegames or the network protocol
 * (MAXGROUPS bounds group indices in level files, MAX_PLAYERS sizes network
 * packets and scoreboards). Changing one is a format change, not a tweak.
 */

/* Players and teams (network protocol + multiplayer scoreboards). */
constexpr int MAX_PLAYERS = 16; /* was 24, 12 in earlier builds */
constexpr int MAX_TEAMS = 4;
constexpr int MAXMULTIPLES = 8; /* orbital pods (Orb pickups) per ship */

/* World. */
constexpr int MAXGROUPS = 128; /* visibility groups per level */
constexpr int MAXBIKETYPES = 17; /* selectable bikes (ship models) */

/* Weapons. */
constexpr int MAXPRIMARYWEAPONS = 6;
constexpr int MAXSECONDARYWEAPONS = 11;

/* Network resync queues: pickups / mines a client missed. */
constexpr int MAXMISSEDPICKUPS = 256;
constexpr int MAXMISSEDMINES = 100;
