#pragma once

/* lua_games.h -- C accessors for the Lua `games` server list
 *
 * Not a Lua binding: each call runs a function on the global `games` table that
 * scripts/init.lua loads from games.lua (a hosted-game list fetched over HTTP,
 * empty when socket.http is unavailable). title.cpp uses it for the join-game
 * list. Indices are 1-based; games_index_at returns 0 when the name is absent.
 * Returned strings belong to the Lua VM and are already popped, so copy them
 * at once.
 */

void games_update( void );
int games_length( void );
int games_index_at( char * name );
const char * games_port_at( int index );
const char * games_name_at( int index );
const char * games_ip_at( int index );
