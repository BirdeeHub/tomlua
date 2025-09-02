#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode.h"

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "strict");
        bool strict = lua_toboolean(L, -1);
        lua_getfield(L, 1, "enhanced_tables");
        bool enhanced_tables = lua_toboolean(L, -1);

        lua_settop(L, 0);
        lua_newtable(L); // module table
        // upvalue 1: error object
        new_TMLErr(L);
        // upvalue 2: options
        TomluaUserOpts *opts = lua_newuserdata(L, sizeof(TomluaUserOpts));
        opts->strict = strict;
        opts->enhanced_tables = enhanced_tables;

        // duplicate pointers for encode
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_pushcclosure(L, tomlua_encode, 2); // pops err+opts, adds function
        lua_setfield(L, -4, "encode");         // pops function

        if (strict) {
            lua_pushnil(L);                        // init upvalue 3 for storing defined values
            lua_pushcclosure(L, tomlua_decode, 3); // pops err+opts+defined, adds function
            lua_setfield(L, -2, "decode");         // pops function
        } else {
            lua_pushcclosure(L, tomlua_decode, 2); // pops err+opts, adds function
            lua_setfield(L, -2, "decode");         // pops function
        }
        return 1;
    }
    lua_settop(L, 0);
    lua_newtable(L); // module table
    // upvalue 1: error object
    new_TMLErr(L);
    // upvalue 2: options
    TomluaUserOpts *opts = lua_newuserdata(L, sizeof(TomluaUserOpts));
    opts->strict = false;
    opts->enhanced_tables = false;
    // duplicate pointers for encode
    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    // stack: module, err, opts, err, opts
    lua_pushcclosure(L, tomlua_encode, 2); // pops err+opts, adds function
    lua_setfield(L, -4, "encode");         // pops function
    lua_pushcclosure(L, tomlua_decode, 2); // pops err+opts, adds function
    lua_setfield(L, -2, "decode");         // pops function
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    if (sizeof(lua_Integer) < 8) return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    lua_pushcfunction(L, tomlua_new);
    return 1;
}

