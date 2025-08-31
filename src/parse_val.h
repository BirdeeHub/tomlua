// Copyright 2025 Birdee
#ifndef SRC_PARSE_VAL_H_
#define SRC_PARSE_VAL_H_

#include <lua.h>
#include "./types.h"

bool parse_value(lua_State *L, str_iter *src, str_buf *buf);

static bool heading_nav(lua_State *L, keys_result *keys, bool array_type, int top) {
    if (!keys->ok) return false;
    if (keys->len <= 0) return false;
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    for (size_t i = 0; i < keys->len; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_gettable(L, -2);  // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);  // remove non-table
            lua_newtable(L);  // create table
            if (!push_buf_to_lua_string(L, &keys->v[i])) {
                return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
            }
            lua_pushvalue(L, -2);  // push new table
            lua_settable(L, -4);   // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            lua_pop(L, 1);  // remove non-table
            return set_err_upval(L, false, 33, "cannot navigate through non-table");
        }
        lua_remove(L, -2);  // remove parent table, keep child on top
    }
    if (array_type) {
        // We’re at the table that should act as an array
        if (!lua_istable(L, -1)) {
            return set_err_upval(L, false, 37, "target of array heading isn't a table");
        }
#if LUA_VERSION_NUM == 501
        size_t len = lua_objlen(L, -1);
#else
        size_t len = lua_rawlen(L, -1);
#endif
        // append new table at len+1
        lua_newtable(L);               // new element
        lua_pushinteger(L, len + 1);
        lua_pushvalue(L, -2);          // copy new element
        lua_settable(L, -4);           // t[len+1] = new element

        // remove parent array table, leave new element on top
        lua_remove(L, -2);
    }
    return true;
}

// gets [-1] value and [-2] root table from top of stack but leaves on top of stack, and sets value at place indexed to by keys
static bool set_kv(lua_State *L, keys_result *keys) {
    if (!keys->ok) return false;
    if (keys->len <= 0) {
        return set_err_upval(L, false, 22, "no key provided to set");
    }
    int value_idx = lua_gettop(L);  // value is on top
    int root_idx = value_idx - 1;   // the table to start navigation from
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (size_t i = 0; i < keys->len - 1; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_gettable(L, -2);                        // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            if (!push_buf_to_lua_string(L, &keys->v[i])) {
                return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
            }
            lua_pushvalue(L, -2);  // push new table
            lua_settable(L, -4);   // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return set_err_upval(L, false, 18, "key is not a table");
        }
        lua_remove(L, -2); // remove parent table
    }

    // Now top table is where we want to set the value
    if (!push_buf_to_lua_string(L, &keys->v[keys->len - 1])) {
        return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
    }
    lua_pushvalue(L, value_idx);  // push value
    lua_settable(L, -3);          // t[last_key] = value

    lua_pop(L, 2);
    return true;
}


#endif  // SRC_PARSE_VAL_H_
