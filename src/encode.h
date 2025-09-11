// Copyright 2025 Birdee
#ifndef SRC_ENCODE_H_
#define SRC_ENCODE_H_

#include <lua.h>
#include <stddef.h>
#include "./types.h"

void push_encode(lua_State *L);

// getmetatable(idx).toml_type to allow overriding of representation
static inline TomlType get_meta_toml_type(lua_State *L, int idx) {
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

static inline bool is_lua_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
    if (!lua_istable(L, idx)) return false;
    switch (get_meta_toml_type(L, idx)) {
        case TOML_TABLE: return false;
        case TOML_ARRAY: if (lua_arraylen(L, idx) == 0) return true;
        default: break;
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

// 0 for not an array, 1 for array, 2 for heading array
static inline int is_lua_heading_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
    if (!lua_istable(L, idx)) return false;
    switch (get_meta_toml_type(L, idx)) {
        case TOML_TABLE: return 0;
        case TOML_ARRAY: if (lua_arraylen(L, idx) == 0) return 1;
        default: break;
    }
    bool is_heading = true;
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, idx) != 0) {
        // now at stack: key value
        if (!lua_istable(L, -1)) is_heading = false;
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(lua_Integer)(key)) {
                lua_settop(L, old_top);
                return 0;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_settop(L, old_top);
            return 0;
        }
    }
    lua_settop(L, old_top);
    if (highest_int_key != count || count == 0) return 0;
    return (is_heading) ? 2 : 1;
}

#endif  // SRC_ENCODE_H_
