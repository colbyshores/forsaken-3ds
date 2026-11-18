#pragma once

/* lua_weapons.h -- Lua table of primary and secondary weapon attributes
 *
 * luaopen_weapons (lua_weapons.cpp) would build the global `weapons` table as a
 * snapshot of PrimaryWeaponAttribs[] and SecondaryWeaponAttribs[], indexed
 * 1-based and by name, with secondaries starting at SECOFFSET + 1. Compiled but
 * never registered: nothing calls luaopen_weapons, so the table does not exist
 * at runtime.
 */

#include "scripting/lua_api.h"
#include "game/main.h" /* FIXME -- needed for primary.h and secondary.h */
#include "rendering/2dtextures.h" /* FIXME -- needed for primary.h and secondary.h */
#include "math/new3d.h" /* FIXME -- needed for primary.h and secondary.h */
#include "game/object.h" /* FIXME -- needed for networking.h */
#include "net/networking.h" /* FIXME -- needed for primary.h and secondary.h */
#include "game/primary.h"
#include "game/secondary.h"

#define SECOFFSET TOTALPRIMARYWEAPONS

int luaopen_weapons(lua_State *L);
