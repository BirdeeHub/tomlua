// Copyright 2025 Birdee
#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode_lib.h"

static int tomlua_types(lua_State *L) {
    lua_newtable(L);
    lua_pushnumber(L, TOML_STRING);
    lua_setfield(L, -2, "TOML_STRING");
    lua_pushnumber(L, TOML_INTEGER);
    lua_setfield(L, -2, "TOML_INTEGER");
    lua_pushnumber(L, TOML_FLOAT);
    lua_setfield(L, -2, "TOML_FLOAT");
    lua_pushnumber(L, TOML_BOOL);
    lua_setfield(L, -2, "TOML_BOOL");
    lua_pushnumber(L, TOML_ARRAY);
    lua_setfield(L, -2, "TOML_ARRAY");
    lua_pushnumber(L, TOML_TABLE);
    lua_setfield(L, -2, "TOML_TABLE");
    lua_pushnumber(L, TOML_LOCAL_DATE);
    lua_setfield(L, -2, "TOML_LOCAL_DATE");
    lua_pushnumber(L, TOML_LOCAL_TIME);
    lua_setfield(L, -2, "TOML_LOCAL_TIME");
    lua_pushnumber(L, TOML_LOCAL_DATETIME);
    lua_setfield(L, -2, "TOML_LOCAL_DATETIME");
    lua_pushnumber(L, TOML_OFFSET_DATETIME);
    lua_setfield(L, -2, "TOML_OFFSET_DATETIME");
    return 1;
}

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
    lua_insert(L, 1);
    lua_pushvalue(L, lua_upvalueindex(1));
    push_encode(L, 2, 3);
    lua_setfield(L, 1, "encode");
    lua_setfield(L, 1, "types");
    lua_settop(L, 1);
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
    tomlua_types(L);
    lua_pushcclosure(L, tomlua_new, 1);
    return 1;
}
