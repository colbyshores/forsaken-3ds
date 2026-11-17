#pragma once

/*===================================================================
	lua_api -- the Lua 5.1 C API, for every translation unit that
	talks to Lua.

	Lua is a C library and devkitPro's lua5.1 headers carry no
	extern "C" guards (nor the upstream lua.hpp), so they are included
	here and only here, wrapped once. Including <lua.h> directly from a
	.cpp would declare the API with C++ linkage and fail to link -- or
	worse, work only by include order, because an earlier wrapped
	include had already set the header's include guard.
===================================================================*/

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
