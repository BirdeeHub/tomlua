#ifndef SRC_OUTPUT_TABLE_LIB_H_
#define SRC_OUTPUT_TABLE_LIB_H_

#include <lua.h>
#include "./types.h"

static inline void push_to_output_table(lua_State *L) {
    return lua_replace(L, lua_upvalueindex(3));
}

static inline void push_output_table(lua_State *L) {
    return lua_pushvalue(L, lua_upvalueindex(3));
}

// pops keys, leaves new root on top
static inline bool heading_nav(lua_State *L, int keys_len, bool array_type) {
    if (keys_len <= 0) return set_err_upval(L, false, 28, "no keys provided to navigate");
    int keys_start = absindex(lua_gettop(L), -keys_len);
    push_output_table(L);
    for (int key_idx = keys_start; key_idx < keys_start + keys_len; key_idx++) {
        int parent_idx = lua_gettop(L);
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
            return set_err_upval(L, false, 33, "cannot navigate through non-table");
        }
        lua_remove(L, parent_idx);  // remove parent table, keep child on top
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
        lua_rawset(L, -4);           // t[len+1] = new element

        // remove parent array table, leave new element on top
        lua_remove(L, -2);
    }
    lua_insert(L, keys_start);
    lua_settop(L, keys_start);
    return true;
}

// pops value and keys, leaves root on top
static inline bool set_kv(lua_State *L, int keys_len) {
    if (keys_len <= 0) return set_err_upval(L, false, 22, "no key provided to set");
    int value_idx = lua_gettop(L) - keys_len;
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
            return set_err_upval(L, false, 18, "key is not a table");
        }
        lua_remove(L, parent_idx);
    }

    lua_pushvalue(L, -2);         // push last key
    lua_pushvalue(L, value_idx);  // push value
    lua_rawset(L, -3);            // parent[last_key] = value

    lua_settop(L, root_idx);
    return true;
}

// NOTE: FOR STRICT MODE ONLY!!
static inline void create_defined_table(lua_State *L) {
    lua_newtable(L);
    lua_replace(L, lua_upvalueindex(4));
}

// NOTE: FOR STRICT MODE ONLY!!
static inline void reset_defined_table(lua_State *L) {
    lua_pushnil(L);
    lua_replace(L, lua_upvalueindex(4));
}

// NOTE: FOR STRICT MODE ONLY!!
// does not remove table added
// returns false for already defined
static inline bool add_defined(lua_State *L, int idx) {
    lua_pushvalue(L, idx);
    lua_rawget(L, lua_upvalueindex(4));  // use table as key for lookup
    bool was = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (was) return false;
    lua_pushvalue(L, idx);
    lua_newtable(L);
    lua_rawset(L, lua_upvalueindex(4));  // register this heading as created
    return true;
}

// NOTE: FOR STRICT MODE ONLY!!
// does not remove table added
static inline void add_inline(lua_State *L, int idx) {
    lua_pushvalue(L, idx);
    lua_pushboolean(L, true);
    lua_rawset(L, lua_upvalueindex(4));  // register this heading as created
}

// NOTE: FOR STRICT MODE ONLY!!
// does not pop table, adds defined table to stack
// returns err == true if inline or already defined
// 1 for already defined 2 for inline
static inline int set_defined_key(lua_State *L, int t_idx, int k_idx) {
    t_idx = absindex(lua_gettop(L), t_idx);
    k_idx = absindex(lua_gettop(L), k_idx);
    lua_pushvalue(L, t_idx);
    lua_rawget(L, lua_upvalueindex(4));  // use table as key for lookup
    int vtype = lua_type(L, -1);
    if (vtype == LUA_TBOOLEAN && lua_toboolean(L, -1)) {
        lua_pop(L, 1);
        return 2;  // return error, code inline
    } else if (vtype != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, t_idx);
        lua_pushvalue(L, -2);
        lua_rawset(L, lua_upvalueindex(4));
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
// pops keys, leaves new root on top
static inline bool heading_nav_strict(lua_State *L, int keys_len, bool array_type) {
    if (keys_len <= 0) return set_err_upval(L, false, 28, "no keys provided to navigate");
    int keys_start = absindex(lua_gettop(L), -keys_len);
    push_output_table(L);
    for (int key_idx = keys_start; key_idx < keys_start + keys_len; key_idx++) {
        int parent_idx = lua_gettop(L);
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
            return set_err_upval(L, false, 33, "cannot navigate through non-table");
        }
        lua_remove(L, parent_idx);  // remove parent table, keep child on top
    }
    if (!array_type) {
        if (!add_defined(L, -1)) {
            return set_err_upval(L, false, 32, "cannot set the same table twice!");
        }
    } else {
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
        lua_rawset(L, -4);           // t[len+1] = new element

        // remove parent array table, leave new element on top
        lua_remove(L, -2);
        add_defined(L, -1);
    }
    lua_insert(L, keys_start);
    lua_settop(L, keys_start);
    return true;
}

// NOTE: FOR STRICT MODE ONLY!!
// pops value and keys, leaves root on top
// returns ok == false if it sets an existing value directly, or into an inline table
static inline bool set_kv_strict(lua_State *L, int keys_len) {
    if (keys_len <= 0) return set_err_upval(L, false, 22, "no key provided to set");
    int value_idx = lua_gettop(L) - keys_len;
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
            lua_pushvalue(L, -2);
            lua_rawset(L, parent_idx);   // t[key] = new table
        } else if (vtype != LUA_TTABLE) {
            return set_err_upval(L, false, 18, "key is not a table");
        }
        if (set_defined_key(L, parent_idx, key_idx) == 2) {
            return set_err_upval(L, false, 49, "key is part of a table defined as an inline value");
        }
        lua_remove(L, parent_idx);
        add_defined(L, -1);
    }

    if (set_defined_key(L, -1, -2)) {
        return set_err_upval(L, false, 23, "key was already defined");
    }
    if (lua_istable(L, value_idx)) add_inline(L, value_idx);
    lua_pushvalue(L, -2);
    lua_pushvalue(L, value_idx);
    lua_rawset(L, -3);          // t[last_key] = value

    lua_settop(L, root_idx);
    return true;
}

#endif  // SRC_OUTPUT_TABLE_LIB_H_
