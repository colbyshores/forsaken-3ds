#pragma once

/* lua_vecmat.h -- Lua userdata wrappers for VECTOR and MATRIX
 *
 * luaopen_vecmat (lua_vecmat.cpp) would install the global constructors vec,
 * mat, matid, mateuler, mataa and matview plus the "VECTOR" / "MATRIX"
 * metatables; lua_pushvector / lua_pushmatrix push copies by value. Compiled
 * but never registered: nothing calls luaopen_vecmat, so these types do not
 * exist in the running Lua VM.
 */

#include "scripting/lua_api.h"
#include "math/new3d.h"

/* [-0, +1, m] */
void lua_pushvector(lua_State *L, VECTOR *v);

/* [-0, +1, m] */
void lua_pushmatrix(lua_State *L, MATRIX *m);

int luaopen_vecmat(lua_State *L);
