// Copyright 2025 Birdee
#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "dates.h"
#include "decode.h"
#include "encode.h"

static int tomlua_type_of(lua_State *L) {
    return luaL_error(L, "ERROR(TODO): tomlua.type NOT YET IMPLEMENTED");
}

static int tomlua_types(lua_State *L) {
    lua_newtable(L);
    for (int i = 0; i < TOML_MAX_TYPES; i++) {
        lua_pushnumber(L, i);
        lua_setfield(L, -2, toml_type_to_lua_name(i));
    }
    return 1;
}

static int tomlua_typename(lua_State *L) {
    if (!lua_isnumber(L, 1)) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, toml_type_to_lua_name(lua_tonumber(L, 1)));
    }
    return 1;
}

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    TomluaUserOpts opts = {0};
    if (lua_istable(L, 2)) {
        lua_getfield(L, 1, "strict");
        opts.strict = lua_toboolean(L, -1);
        lua_getfield(L, 1, "fancy_tables");
        opts.fancy_tables = lua_toboolean(L, -1);
        lua_getfield(L, 1, "int_keys");
        opts.int_keys = lua_toboolean(L, -1);
        lua_getfield(L, 1, "fancy_dates");
        opts.fancy_dates = lua_toboolean(L, -1);
    }

    lua_settop(L, 0);
    lua_newtable(L); // module table with decode
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, 1);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_setfield(L, 1, "encode");
    lua_pushvalue(L, lua_upvalueindex(3));
    lua_setfield(L, 1, "types");
    lua_pushvalue(L, lua_upvalueindex(4));
    lua_setfield(L, 1, "typename");
    lua_pushvalue(L, lua_upvalueindex(5));
    lua_setfield(L, 1, "new_date");
    lua_pushvalue(L, lua_upvalueindex(6));
    lua_setfield(L, 1, "type");

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
    lua_settop(L, 0);
    lua_newtable(L); // module table
    lua_newtable(L); // meta table
    lua_pushvalue(L, -1);
    push_encode(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "encode");
    tomlua_types(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "types");
    lua_pushcfunction(L, tomlua_typename);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "typename");
    lua_pushcfunction(L, lnew_date);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "new_date");
    lua_pushcfunction(L, tomlua_type_of);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "type");
    lua_pushcclosure(L, tomlua_new, 6);
    lua_setfield(L, 2, "__call");
    lua_setmetatable(L, 1);
    return 1;
}
