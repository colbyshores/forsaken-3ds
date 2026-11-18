#pragma once

/* lua_api.h -- the Lua 5.1 C API, with C linkage
 *
 * Every translation unit that talks to Lua includes this instead of <lua.h>.
 * Lua is a C library and its 5.1 headers (devkitPro portlibs lua5.1 on 3DS,
 * the pkg-config lua5.1 / lua package on PC) carry no extern "C" guards, so
 * they are included here and only here, wrapped once. Including <lua.h>
 * directly from a .cpp would declare the API with C++ linkage and fail to
 * link -- or worse, work only by include order, because an earlier wrapped
 * include had already set the header's include guard.
 */

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
