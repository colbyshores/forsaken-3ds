#pragma once

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
