// Copyright 2025 Birdee
#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "dates.h"
#include "opts.h"
#include "decode.h"
#include "encode.h"

static inline TomlType toml_table_type(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
    bool is_inline = false;
    switch (get_meta_toml_type(L, idx)) {
        case TOML_ARRAY: if (lua_arraylen(L, idx) == 0) return TOML_ARRAY; else break;
        case TOML_ARRAY_INLINE:
            is_inline = true;
            if (lua_arraylen(L, idx) == 0) return TOML_ARRAY_INLINE; else break;
        case TOML_TABLE_INLINE: return TOML_TABLE_INLINE;
        default: return TOML_TABLE;
    }
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, idx) != 0) {
        // now at stack: key value
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(lua_Integer)(key)) {
                lua_settop(L, old_top);
                return TOML_TABLE;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_settop(L, old_top);
            return TOML_TABLE;
        }
    }
    lua_settop(L, old_top);
    if (highest_int_key != count || count == 0) return TOML_TABLE;
    return is_inline ? TOML_ARRAY_INLINE : TOML_ARRAY;
}

static int tomlua_type(lua_State *L) {
    switch (lua_type(L, 1)) {
        case LUA_TSTRING:
            lua_pushstring(L, toml_type_names[TOML_STRING]);
            return 1;
        case LUA_TNUMBER: {
            lua_Number num = lua_tonumber(L, 1);
            if (num == (lua_Number)(lua_Integer)num) {
                lua_pushstring(L, toml_type_names[TOML_INTEGER]);
            } else {
                lua_pushstring(L, toml_type_names[TOML_FLOAT]);
            }
            return 1;
        }
        case LUA_TBOOLEAN:
            lua_pushstring(L, toml_type_names[TOML_BOOL]);
            return 1;
        case LUA_TTABLE:
            lua_pushstring(L, toml_type_names[toml_table_type(L, 1)]);
            return 1;
        case LUA_TUSERDATA:
            if(udata_is_of_type(L, 1, "TomluaDate")) {
                TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
                lua_pushstring(L, toml_type_names[(*date)[TOMLDATE_TOML_TYPE]]);
                return 1;
            } else if (udata_is_of_type(L, 1, "TomluaMultiStr")) {
                lua_pushstring(L, toml_type_names[TOML_STRING_MULTI]);
                return 1;
            }
        default:
            lua_pushstring(L, toml_type_names[TOML_UNTYPED]);
            return 1;
    }
}
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
            lua_pushinteger(L, toml_table_type(L, 1));
            return 1;
        case LUA_TUSERDATA:
            if(udata_is_of_type(L, 1, "TomluaDate")) {
                TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
                lua_pushnumber(L, (*date)[TOMLDATE_TOML_TYPE]);
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

static int str_2_mul(lua_State *L) {
    size_t len = 0;
    const char *data = lua_tolstring(L, 1, &len);
    if (data == NULL) return luaL_error(L, "tomlua.str_2_mul takes a string as its only argument!");
    str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
    *s = new_buf_from_str(data, len);
    push_multi_string_mt(L);
    lua_setmetatable(L, -2);
    return 1;
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
            opts_parse(L, *uopts, 2, argtop + 1);
        } else {
            opts_parse(L, *uopts, -1, argtop + 1);
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
    lua_setfield(L, 1, "type_of");
    lua_pushcfunction(L, tomlua_type);
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
