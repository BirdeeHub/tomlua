// Copyright 2025 Birdee
#ifndef SRC_PARSE_VAL_H_
#define SRC_PARSE_VAL_H_

#include <lua.h>
#include "./types.h"

struct parse_value_opts {
    int has_defaults;
    int top;
    bool strict;
    bool enhanced_tables;
};

bool parse_value(lua_State *L, str_iter *src, str_buf *buf, const struct parse_value_opts *opts);

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
