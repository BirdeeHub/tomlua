// Copyright 2025 Birdee
#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode_lib.h"

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    TomluaUserOpts opts = {0};
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "strict");
        opts.strict = lua_toboolean(L, -1);
        lua_getfield(L, 1, "enhanced_tables");
        opts.enhanced_tables = lua_toboolean(L, -1);
        lua_getfield(L, 1, "int_keys");
        opts.int_keys = lua_toboolean(L, -1);
        lua_pop(L, 3);
    }

    lua_settop(L, 1);
    lua_newtable(L); // module table
    push_encode(L, 1);
    lua_remove(L, 1);
    lua_setfield(L, -2, "encode");
    // upvalue 1: error object
    new_TMLErr(L);
    // upvalue 2: options
    {
        TomluaUserOpts *uopts = lua_newuserdata(L, sizeof(TomluaUserOpts));
        *uopts = opts;
    }
    // upvalue 3: for storing output until it is returned
    lua_pushnil(L);

    if (opts.strict) {
        // upvalue 4: for uniquness bookkeeping
        lua_pushnil(L);
        lua_pushcclosure(L, tomlua_decode, 4);
        lua_setfield(L, 1, "decode");
    } else {
        lua_pushcclosure(L, tomlua_decode, 3);
        lua_setfield(L, 1, "decode");
    }
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    lua_pushcfunction(L, tomlua_new);
    return 1;
}
