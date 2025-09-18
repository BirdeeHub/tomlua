// Copyright 2025 Birdee
#ifndef SRC_ENCODE_H_
#define SRC_ENCODE_H_

#include <lua.h>
#include <stddef.h>
#include "./types.h"

int encode(lua_State *L);

// getmetatable(idx).toml_type to allow overriding of representation
static TomlType get_meta_toml_type(lua_State *L, int idx) {
    if (luaL_getmetafield(L, idx, "toml_type")) {
        if (lua_isnumber(L, -1)) {
            lua_Number n = lua_tonumber(L, -1);
            if (is_valid_toml_type(n)) {
                lua_pop(L, 1);
                return (TomlType)n;
            }
        }
        lua_pop(L, 1);
    }
    return TOML_UNTYPED;
}

static bool is_lua_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
    if (!lua_istable(L, idx)) return false;
    if (lua_arraylen(L, idx) == 0) {
        if (get_meta_toml_type(L, idx) == TOML_ARRAY) return true;
        else return 0;
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
                return false;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_settop(L, old_top);
            return false;
        }
    }
    lua_settop(L, old_top);
    if (highest_int_key != count || count == 0) return false;
    return true;
}

#endif  // SRC_ENCODE_H_
