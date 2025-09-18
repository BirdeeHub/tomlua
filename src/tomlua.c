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

// negative taridx means all false
static TomluaUserOpts opts_parse(lua_State *L, int taridx, int optidx) {
    int top = lua_gettop(L);
    TomluaUserOpts opts = {0};
    if (taridx > 0) luaL_checktype(L, taridx, LUA_TTABLE);

    if (taridx > 0) {
        lua_getfield(L, taridx, "strict");
        opts.strict = lua_toboolean(L, -1);
    } else {
        opts.strict = false;
    }
    lua_pushboolean(L, opts.strict);
    lua_setfield(L, optidx, "strict");

    if (taridx > 0) {
        lua_getfield(L, taridx, "fancy_tables");
        opts.fancy_tables = lua_toboolean(L, -1);
    } else {
        opts.fancy_tables = false;
    }
    lua_pushboolean(L, opts.fancy_tables);
    lua_setfield(L, optidx, "fancy_tables");

    if (taridx > 0) {
        lua_getfield(L, taridx, "int_keys");
        opts.int_keys = lua_toboolean(L, -1);
    } else {
        opts.int_keys = false;
    }
    lua_pushboolean(L, opts.int_keys);
    lua_setfield(L, optidx, "int_keys");

    if (taridx > 0) {
        lua_getfield(L, taridx, "fancy_dates");
        opts.fancy_dates = lua_toboolean(L, -1);
    } else {
        opts.fancy_dates = false;
    }
    lua_pushboolean(L, opts.fancy_dates);
    lua_setfield(L, optidx, "fancy_dates");

    if (taridx > 0) {
        lua_getfield(L, taridx, "multi_strings");
        opts.multi_strings = lua_toboolean(L, -1);
    } else {
        opts.multi_strings = false;
    }
    lua_pushboolean(L, opts.multi_strings);
    lua_setfield(L, optidx, "multi_strings");

    lua_settop(L, top);
    return opts;
}
static int opts_call(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    *opts = opts_parse(L, 2, 1);
    return 0;
}
static int opts_index(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "strict") == 0) {
        lua_pushboolean(L, opts->strict);
    } else if (strcmp(key, "fancy_tables") == 0) {
        lua_pushboolean(L, opts->fancy_tables);
    } else if (strcmp(key, "int_keys") == 0) {
        lua_pushboolean(L, opts->int_keys);
    } else if (strcmp(key, "fancy_dates") == 0) {
        lua_pushboolean(L, opts->fancy_dates);
    } else if (strcmp(key, "multi_strings") == 0) {
        lua_pushboolean(L, opts->multi_strings);
    } else {
        lua_pushnil(L); // unknown key
    }
    return 1;
}
static int opts_newindex(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = luaL_checkstring(L, 2);
    int value = lua_toboolean(L, 3);
    if (strcmp(key, "strict") == 0) {
        opts->strict = value;
    } else if (strcmp(key, "fancy_tables") == 0) {
        opts->fancy_tables = value;
    } else if (strcmp(key, "int_keys") == 0) {
        opts->int_keys = value;
    } else if (strcmp(key, "fancy_dates") == 0) {
        opts->fancy_dates = value;
    } else if (strcmp(key, "multi_strings") == 0) {
        opts->multi_strings = value;
    } else {
        return luaL_error(L, "invalid option '%s'", key);
    }
    // mirror into table so `print(inspect(opts))` shows it
    lua_pushvalue(L, 2); // key
    lua_pushboolean(L, value);
    lua_rawset(L, 1);
    return 0;
}

int luaopen_tomlua(lua_State *L) {
    int argtop = lua_gettop(L);
    lua_newtable(L); // module table
    if (argtop > 0) lua_replace(L, 1);
    lua_newtable(L);  // options table (argtop + 1)
    lua_newtable(L);  // options table metatable (argtop + 2)
    {
        TomluaUserOpts *uopts = lua_newuserdata(L, sizeof(TomluaUserOpts));
        if (lua_istable(L, 2)) {
            *uopts = opts_parse(L, 2, argtop + 1);
        } else {
            *uopts = opts_parse(L, -1, argtop + 1);
        }
    }
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, tomlua_decode, 1);
    lua_setfield(L, 1, "decode");
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, opts_index, 1);
    lua_setfield(L, argtop + 2, "__index");
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, opts_newindex, 1);
    lua_setfield(L, argtop + 2, "__newindex");
    // NOTE: dont copy opts for last closure
    lua_pushcclosure(L, opts_call, 1);
    lua_setfield(L, argtop + 2, "__call");
    // pop opts meta
    lua_setmetatable(L, argtop + 1);
    // pop opts
    lua_setfield(L, 1, "opts");
    lua_settop(L, 1);
    tomlua_types(L);
    lua_setfield(L, 1, "types");
    lua_pushcfunction(L, tomlua_typename);
    lua_setfield(L, 1, "typename");
    lua_pushcfunction(L, lnew_date);
    lua_setfield(L, 1, "new_date");
    lua_pushcfunction(L, tomlua_type_of);
    lua_setfield(L, 1, "type");
    lua_pushcfunction(L, str_2_mul);
    lua_setfield(L, 1, "str_2_mul");
    lua_pushcfunction(L, encode);
    lua_setfield(L, 1, "encode");
    lua_newtable(L);
    lua_pushcfunction(L, luaopen_tomlua);
    lua_setfield(L, 2, "__call");
    lua_setmetatable(L, 1);
    return 1;
}
