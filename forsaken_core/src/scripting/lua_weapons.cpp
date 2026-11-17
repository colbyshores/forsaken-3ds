/* Functions for accessing weapon attribute data from Lua.
 *
 * print(weapons.Pulsar.Speed[me.Object.PowerLevel + 1])
 * print(weapons["Purge Mine"].Flags.CanTarget)
 * me = Ships[WhoIAm()]
 * print(me.Secondary.index, me.Secondary.name)
 *
 * see also primary.h and secondary.h
 */
#include <lua.h>
#include <lauxlib.h>
#include "game/main.h" /* FIXME -- needed for primary.h and secondary.h */
#include "rendering/2dtextures.h" /* FIXME -- needed for primary.h and secondary.h */
#include "math/new3d.h" /* FIXME -- needed for primary.h and secondary.h */
#include "game/object.h" /* FIXME -- needed for networking.h */
#include "net/networking.h" /* FIXME -- needed for primary.h and secondary.h */
#include "game/primary.h"
#include "game/secondary.h"
#include "scripting/lua_weapons.h"

#define FIELD(field, table, index, type) do { \
	lua_push ## type(L, table[index].field);  \
	lua_setfield(L, -2, #field);              \
} while (0)

#define FIELDPRIM(field, index, type) \
	    FIELD(field, PrimaryWeaponAttribs, index, type)

#define FIELDPRIMPOW(field, index, type) do {                      \
	lua_createtable(L, 3, 0);                                      \
	for (p=0; p<MAXPOWERLEVELS; p++)                               \
	{                                                              \
		lua_pushinteger(L, p+1);                                   \
		lua_push ## type(L, PrimaryWeaponAttribs[index].field[p]); \
		lua_settable(L, -3);                                       \
	}                                                              \
	lua_setfield(L, -2, #field);                                   \
} while (0)

#define FIELDSEC(field, index, type) \
	    FIELD(field, SecondaryWeaponAttribs, index, type)

#define SECFLAG(name, index) do {                               \
	lua_pushboolean(L,                                          \
		SecondaryWeaponAttribs[index].Flags & SECFLAGS_ ## name \
	);                                                          \
	lua_setfield(L, -2, #name);                                 \
} while (0)

int luaopen_weapons(lua_State *L)
{
	int i, p;

	lua_createtable(L,
		TOTALPRIMARYWEAPONS + TOTALSECONDARYWEAPONS,
		TOTALPRIMARYWEAPONS + TOTALSECONDARYWEAPONS
	);
	for (i=0; i<TOTALPRIMARYWEAPONS; i++)
	{
		lua_pushinteger(L, i+1);
		lua_createtable(L, 0, 15);
		lua_pushliteral(L, "primary");
		lua_setfield(L, -2, "category");
		lua_pushinteger(L, i+1);
		lua_setfield(L, -2, "index");
		lua_pushstring(L, (char*)DebugPrimStrings[i]);
		lua_setfield(L, -2, "name");
		FIELDPRIMPOW(Speed, i, number);
		FIELDPRIM(LifeCount, i, number);
		FIELDPRIMPOW(FireDelay, i, integer);
		FIELDPRIMPOW(AmmoUsage, i, number);
		FIELDPRIMPOW(Damage, i, number);
		FIELDPRIMPOW(ColRadius, i, number);
		FIELDPRIM(ColType, i, integer);
		FIELDPRIM(lightsize, i, number);
		FIELDPRIMPOW(r, i, number);
		FIELDPRIMPOW(g, i, number);
		FIELDPRIMPOW(b, i, number);
		FIELDPRIM(FmSeq, i, integer);
		lua_pushvalue(L, -1);
		lua_setfield(L, -4, (char*)DebugPrimStrings[i]);
		lua_settable(L, -3);
	}
	for (i=0; i<TOTALSECONDARYWEAPONS; i++)
	{
		lua_pushinteger(L, 1 + i + SECOFFSET);
		lua_createtable(L, 0, 22);
		lua_pushliteral(L, "secondary");
		lua_setfield(L, -2, "category");
		lua_pushinteger(L, 1 + i + SECOFFSET);
		lua_setfield(L, -2, "index");
		if (i < 17)
		{
			lua_pushstring(L, (char*)DebugSecStrings[i]);
			lua_setfield(L, -2, "name");
		}
		FIELDSEC(SecType, i, integer);
		FIELDSEC(State, i, integer);
		FIELDSEC(MoveType, i, integer);
		FIELDSEC(StartSpeed, i, number);
		FIELDSEC(Speed, i, number);
		FIELDSEC(Size, i, number);
		FIELDSEC(TurnSpeed, i, number);
		FIELDSEC(FireDelay, i, integer);
		FIELDSEC(ViewCone, i, integer);
		FIELDSEC(ModelNum, i, integer);
		FIELDSEC(lightsize, i, number);
		FIELDSEC(r, i, number);
		FIELDSEC(g, i, number);
		FIELDSEC(b, i, number);
		FIELDSEC(Shield, i, number);
		FIELDSEC(Damage, i, number);
		FIELDSEC(Lensflare, i, boolean);
		FIELDSEC(ColRadius, i, number);
		/* Flags */
		lua_createtable(L, 0, 3);
		SECFLAG(CanShoot, i);
		SECFLAG(CanTarget, i);
		SECFLAG(NoIncoming, i);
		lua_setfield(L, -2, "Flags");
		if (i < 17)
		{
			lua_pushvalue(L, -1);
			lua_setfield(L, -4, (char*)DebugSecStrings[i]);
		}
		lua_settable(L, -3);
	}
	lua_setglobal(L, "weapons");
	return 0;
}
