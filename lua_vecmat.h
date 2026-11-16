#ifndef LUA_VECMAT_INCLUDED
#define LUA_VECMAT_INCLUDED

#include <lua.h>
#include "new3d.h"
#ifdef __cplusplus
extern "C" {
#endif

/* [-0, +1, m] */
void lua_pushvector(lua_State *L, VECTOR *v);

/* [-0, +1, m] */
void lua_pushmatrix(lua_State *L, MATRIX *m);

int luaopen_vecmat(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif /* LUA_VECMAT_INCLUDED */
