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

// NOTE: FOR STRICT MODE ONLY!!
// does not remove table added
// returns false for already defined
static inline bool add_defined(lua_State *L, int idx) {
    lua_pushvalue(L, idx);
    lua_rawget(L, lua_upvalueindex(3));  // use table as key for lookup
    bool was = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (was) return false;
    lua_pushvalue(L, idx);
    lua_newtable(L);
    lua_rawset(L, lua_upvalueindex(3));  // register this heading as created
    return true;
}
// NOTE: FOR STRICT MODE ONLY!!
// does not remove table added
static inline void add_inline(lua_State *L, int idx) {
    lua_pushvalue(L, idx);
    lua_pushboolean(L, true);
    lua_rawset(L, lua_upvalueindex(3));  // register this heading as created
}
// NOTE: FOR STRICT MODE ONLY!!
// does not pop table, adds defined table to stack
// returns err == true if inline or already defined
// 1 for already defined 2 for inline
static inline int set_defined_key(lua_State *L, int t_idx, int k_idx) {
    t_idx = absindex(lua_gettop(L), t_idx);
    k_idx = absindex(lua_gettop(L), k_idx);
    lua_pushvalue(L, t_idx);
    lua_rawget(L, lua_upvalueindex(3));  // use table as key for lookup
    int vtype = lua_type(L, -1);
    if (vtype == LUA_TBOOLEAN && lua_toboolean(L, -1)) {
        lua_pop(L, 1);
        return 2;  // return error, code inline
    } else if (vtype != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, t_idx);
        lua_pushvalue(L, -2);
        lua_rawset(L, lua_upvalueindex(3));
        lua_pushvalue(L, k_idx);
        lua_pushboolean(L, true);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        return false;  // return no error
    } else {
        lua_pushvalue(L, k_idx);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1)) {
            lua_pop(L, 2);
            return true;  // return error
        }
        lua_pop(L, 1);
        lua_pushvalue(L, k_idx);
        lua_pushboolean(L, true);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        return false;  // return no error
    }
}
// NOTE: FOR STRICT MODE ONLY!!
// returns ok == false if it sets an existing value directly, or into an inline table
// gets [-1] value and [-2] root table from top of stack but leaves root table on top of stack, and sets value at place indexed to by keys
static inline bool set_kv_strict(lua_State *L, keys_result *keys) {
    if (!keys->ok) return false;
    if (keys->len <= 0) return set_err_upval(L, false, 22, "no key provided to set");
    int value_idx = lua_gettop(L);
    int root_idx = value_idx - 1;
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (size_t i = 0; i < keys->len - 1; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        int key_idx = lua_gettop(L);
        int parent_idx = key_idx - 1;
        lua_pushvalue(L, key_idx);
        lua_rawget(L, parent_idx);
        int vtype = lua_type(L, -1);
        if (vtype == LUA_TNIL) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);
            lua_rawset(L, parent_idx);   // t[key] = new table
        } else if (vtype != LUA_TTABLE) {
            return set_err_upval(L, false, 18, "key is not a table");
        }
        if (set_defined_key(L, parent_idx, key_idx) == 2) {
            return set_err_upval(L, false, 49, "key is part of a table defined as an inline value");
        }
        lua_remove(L, key_idx);
        lua_remove(L, parent_idx);
        add_defined(L, -1);
    }

    // Now top table is where we want to set the value
    if (!push_buf_to_lua_string(L, &keys->v[keys->len - 1])) {
        return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
    }
    if (set_defined_key(L, -2, -1)) {
        return set_err_upval(L, false, 23, "key was already defined");
    }
    if (lua_istable(L, value_idx)) add_inline(L, value_idx);  // if value was an inline table, add it because we are going to add it
    lua_pushvalue(L, value_idx);  // push value
    lua_rawset(L, -3);          // t[last_key] = value

    lua_pop(L, 2);  // remove top table and value, leave root table
    return true;
}

#endif  // SRC_PARSE_VAL_H_
