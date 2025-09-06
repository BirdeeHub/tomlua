// Copyright 2025 Birdee
#ifndef SRC_ENCODE_LIB_H_
#define SRC_ENCODE_LIB_H_

#include <lua.h>
#include "./types.h"

static int is_lua_array(lua_State *L) {
    // probably dont need this check:
    int idx = lua_gettop(L);
    if (!lua_istable(L, idx)) {
        lua_settop(L, 0);
        lua_pushboolean(L, false);
        return 1;
    }

    // getmetatable(stack_top).array to allow overriding of representation
    if (lua_getmetatable(L, idx)) {
        // stack: ... table mt
        lua_getfield(L, -1, "array");
        if (!lua_isnil(L, -1)) {
            lua_settop(L, 0);
            if (lua_toboolean(L, -1)) {
                lua_pushboolean(L, true);
                return 1;
            } else {
                lua_pushboolean(L, false);
                return 1;
            }
        }
        lua_pop(L, 2);  // pop value + mt
    }
    bool is_array = true;
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, idx) != 0) {
        // now at stack: key value
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(int64_t)(key)) {
                is_array = false;
                break;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            is_array = false;
            break;
        }
    }
    if (highest_int_key != count || count == 0) is_array = false;
    lua_settop(L, 0);
    lua_pushboolean(L, is_array);
    return 1;
}

#endif  // SRC_ENCODE_LIB_H_
