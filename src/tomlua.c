// Copyright 2025 Birdee
#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "dates.h"
#include "decode.h"
#include "encode.h"

static int tomlua_type_of(lua_State *L) {
    switch (lua_type(L, 1)) {
        case LUA_TSTRING:
            lua_pushnumber(L, TOML_STRING);
            return 1;
        case LUA_TNUMBER: {
            lua_Number num = lua_tonumber(L, 1);
            if (num == (lua_Number)(lua_Integer)num) {
                lua_pushnumber(L, TOML_INTEGER);
            } else {
                lua_pushnumber(L, TOML_FLOAT);
            }
            return 1;
        }
        case LUA_TBOOLEAN:
            lua_pushnumber(L, TOML_BOOL);
            return 1;
        case LUA_TTABLE:
            if (is_lua_array(L, 1)) {
                lua_pushnumber(L, TOML_ARRAY);
                return 1;
            } else {
                lua_pushnumber(L, TOML_TABLE);
                return 1;
            }
        case LUA_TUSERDATA:
            if(udata_is_of_type(L, 1, "TomluaDate")) {
                TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
                lua_pushnumber(L, date->toml_type);
                return 1;
            } else if (udata_is_of_type(L, 1, "TomluaMultiStr")) {
                lua_pushnumber(L, TOML_STRING_MULTI);
                return 1;
            }
        default:
            lua_pushnumber(L, TOML_UNTYPED);
            return 1;
    }
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

int luaopen_tomlua_date(lua_State *L) {
    lua_pushcfunction(L, lnew_date);
    return 1;
}
int luaopen_embed_date(lua_State *L) { return luaopen_tomlua_date(L); }
int luaopen_env_date(lua_State *L) { return luaopen_tomlua_date(L); }

static inline int str_2_mul(lua_State *L) {
    size_t len = 0;
    const char *data = lua_tolstring(L, 1, &len);
    if (data == NULL) return luaL_error(L, "tomlua.str_2_mul takes a string as its only argument!");
    str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
    *s = new_buf_from_str(data, len);
    push_multi_string_mt(L);
    lua_setmetatable(L, -2);
    return 1;
}

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    TomluaUserOpts opts = {0};
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "strict");
        opts.strict = lua_toboolean(L, -1);
        lua_getfield(L, 2, "fancy_tables");
        opts.fancy_tables = lua_toboolean(L, -1);
        lua_getfield(L, 2, "int_keys");
        opts.int_keys = lua_toboolean(L, -1);
        lua_getfield(L, 2, "fancy_dates");
        opts.fancy_dates = lua_toboolean(L, -1);
        lua_getfield(L, 2, "multi_strings");
        opts.multi_strings = lua_toboolean(L, -1);
    }

    lua_settop(L, 0);
    lua_newtable(L); // module table with encode and decode
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
    lua_pushvalue(L, lua_upvalueindex(7));
    lua_setfield(L, 1, "str_2_mul");

    lua_settop(L, 1);
    {
        TomluaUserOpts *uopts = lua_newuserdata(L, sizeof(TomluaUserOpts));
        *uopts = opts;
    }
    lua_pushcclosure(L, tomlua_decode, 1);
    lua_setfield(L, 1, "decode");
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    lua_settop(L, 0);
    lua_newtable(L); // module table
    lua_newtable(L); // meta table (and upvalue 1)
    lua_pushvalue(L, -1);
    push_encode(L);
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
    lua_pushcfunction(L, str_2_mul);
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "str_2_mul");
    lua_pushcclosure(L, tomlua_new, 7);
    lua_setfield(L, 2, "__call");
    lua_setmetatable(L, 1);
    return 1;
}
int luaopen_embed_tomlua(lua_State *L) { return luaopen_tomlua(L); }
int luaopen_env_tomlua(lua_State *L) { return luaopen_tomlua(L); }
