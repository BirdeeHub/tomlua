// Copyright 2025 Birdee
#ifndef SRC_DECODE_INLINE_VALUE_H_
#define SRC_DECODE_INLINE_VALUE_H_

#include <lua.h>
#include "./types.h"
#include "./opts.h"
#include "./error_context.h"

// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
bool decode_inline_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts, int erridx);

// pops value and keys, leaves root on top
static inline bool set_kv(lua_State *L, int keys_len, int value_idx, int erridx) {
    if (keys_len <= 0) return set_tmlerr(new_tmlerr(L, erridx), false, 22, "no key provided to set");
    int keys_start = value_idx + 1;
    int root_idx = value_idx - 1;
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (int key_idx = keys_start; key_idx < keys_start + keys_len - 1; key_idx++) {
        int parent_idx = lua_gettop(L);
        lua_pushvalue(L, key_idx);
        lua_rawget(L, parent_idx);
        int vtype = lua_type(L, -1);
        if (vtype == LUA_TNIL) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);  // copy so we can continue with it after rawset
            lua_rawset(L, parent_idx);   // t[key] = new table
        } else if (vtype != LUA_TTABLE) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 18, "key is not a table");
        }
        lua_remove(L, parent_idx);
        // NOTE: erridx is secretly also our defined table when no error
        lua_pushvalue(L, -1);
        lua_pushinteger(L, -1);
        lua_rawset(L, erridx);
    }

    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        return set_tmlerr(new_tmlerr(L, erridx), false, 18, "key already defined!");
    }
    lua_pop(L, 1);

    lua_pushvalue(L, -2);         // push last key
    lua_pushvalue(L, value_idx);  // push value
    lua_rawset(L, -3);            // parent[last_key] = value

    lua_settop(L, root_idx);
    return true;
}

#endif  // SRC_DECODE_INLINE_VALUE_H_
