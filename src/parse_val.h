// Copyright 2025 Birdee
#ifndef SRC_PARSE_VAL_H_
#define SRC_PARSE_VAL_H_

#include <lua.h>
#include "./types.h"

bool parse_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts *opts);

// gets [-1] value and [-2] root table from top of stack but leaves root table on top of stack, and sets value at place indexed to by keys
static inline bool set_kv(lua_State *L, keys_result *keys) {
    if (!keys->ok) return false;
    if (keys->len <= 0) return set_err_upval(L, false, 22, "no key provided to set");
    lua_pushvalue(L, -2);     // copy root table to top

    // Navigate through all keys except the last
    for (size_t i = 0; i < keys->len - 1; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);    // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            lua_pushvalue(L, -1);
            lua_insert(L, -3);
            lua_rawset(L, -4);   // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            return set_err_upval(L, false, 18, "key is not a table");
        } else {
            lua_remove(L, -3);
        }
        lua_remove(L, -2); // remove parent table
    }

    // Now top table is where we want to set the value
    if (!push_buf_to_lua_string(L, &keys->v[keys->len - 1])) {
        return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
    }
    lua_pushvalue(L, -3);  // push value
    lua_rawset(L, -3);          // t[last_key] = value

    lua_pop(L, 2);
    return true;
}

#endif  // SRC_PARSE_VAL_H_
