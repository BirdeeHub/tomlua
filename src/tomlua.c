#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode.h"

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    bool strict = false;
    bool enhanced_tables = false;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "strict");
        strict = lua_toboolean(L, -1);
        lua_getfield(L, 1, "enhanced_tables");
        enhanced_tables = lua_toboolean(L, -1);
    }

    lua_settop(L, 0);
    lua_newtable(L); // module table
    // upvalue 1: error object
    new_TMLErr(L);
    // upvalue 2: options
    TomluaUserOpts *opts = lua_newuserdata(L, sizeof(TomluaUserOpts));
    opts->strict = strict;
    opts->enhanced_tables = enhanced_tables;

    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, tomlua_encode, 2);
    lua_setfield(L, 1, "encode");

    if (strict) {
        lua_pushnil(L);                        // init upvalue 3 for storing defined values
        lua_pushnil(L);                        // init upvalue 4 for uniquness bookkeeping
        lua_pushcclosure(L, tomlua_decode, 4);
        lua_setfield(L, 1, "decode");
    } else {
        lua_pushnil(L);                        // init upvalue 3 for storing defined values
        lua_pushcclosure(L, tomlua_decode, 3);
        lua_setfield(L, 1, "decode");
    }
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    if (sizeof(lua_Integer) < 8) return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    lua_pushcfunction(L, tomlua_new);
    return 1;
}

