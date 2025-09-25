// Copyright 2025 Birdee
#include <stddef.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "types.h"
#include "opts.h"
#include "decode_keys.h"
#include "error_context.h"
#include "decode_inline_value.h"

#define DECODE_RESULT_IDX 2
// @type { [table]: len if array or -1 for defined table }
// or error userdata type on error
#define DECODE_DEFINED_IDX 3

// pops value and keys, leaves root on top
static inline bool set_kv(lua_State *L, int keys_len, int value_idx) {
    if (keys_len <= 0) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 22, "no key provided to set");
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
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 18, "key is not a table");
        }
        lua_remove(L, parent_idx);
        // NOTE: erridx is secretly also our defined table when no error
        lua_pushvalue(L, -1);
        lua_rawget(L, DECODE_DEFINED_IDX);
        if (lua_tointeger(L, -1) == -2) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 36, "Keys may not redefine inline tables!");
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushinteger(L, -1);
        lua_rawset(L, DECODE_DEFINED_IDX);
    }

    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 20, "key already defined!");
    }
    lua_pop(L, 1);

    lua_pushvalue(L, -2);         // push last key
    lua_pushvalue(L, value_idx);  // push value
    lua_rawset(L, -3);            // parent[last_key] = value

    lua_settop(L, root_idx);
    return true;
}

// pops keys, leaves new root on top
static inline bool heading_nav(lua_State *L, int keys_len, bool array_type) {
    if (keys_len <= 0) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 28, "no keys provided to navigate");
    int keys_start = absindex(lua_gettop(L), -keys_len);
    lua_pushvalue(L, DECODE_RESULT_IDX);
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
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 33, "cannot navigate through non-table");
        }
        lua_remove(L, parent_idx);  // remove parent table, keep child on top
        lua_pushvalue(L, -1);
        lua_rawget(L, DECODE_DEFINED_IDX);
        // NOTE: if DECODE_DEFINED_IDX has no entry, lua_tointeger returns 0
        // we only care about -1 or > 1
        lua_Integer len = lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (key_idx == keys_start + keys_len - 1) {
            if (len >= 0 && array_type) {
                len++;
                parent_idx = lua_gettop(L);
                lua_pushvalue(L, parent_idx);
                lua_pushinteger(L, len);
                lua_rawset(L, DECODE_DEFINED_IDX);
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_rawseti(L, parent_idx, len);
                lua_remove(L, parent_idx);  // remove parent table, keep child on top
            } else if (len != 0) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 22, "table already defined!");
            } else {
                lua_pushvalue(L, -1);
                lua_pushinteger(L, -1);
                lua_rawset(L, DECODE_DEFINED_IDX);
            }
        } else if (len > 0) { // it was an array, grab latest
            lua_rawgeti(L, -1, len);
            lua_remove(L, -2);
            if (!lua_istable(L, -1))
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 33, "cannot navigate through non-table");
        }
    }
    lua_insert(L, keys_start);
    lua_settop(L, keys_start);
    return true;
}

int tomlua_decode(lua_State *L) {
    // process arguments and options
    str_iter src = lua_str_to_iter(L, 1);
    if (src.buf == NULL) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, "tomlua.decode first argument must be a string! tomlua.decode(string) -> table?, err?");
        return 2;
    }
    TomluaUserOpts uopts;
    toml_user_opts_copy(uopts, *get_opts_upval(L));
    const bool int_keys = uopts[TOMLOPTS_INT_KEYS];
    lua_settop(L, 1);
    // DECODE_RESULT_IDX == 2 == here
    lua_newtable(L);
    // DECODE_DEFINED_IDX == 3 == here
    // @type { [table]: len if array or -1 for defined table }
    // or error if error
    lua_newtable(L);

    // set top as the starting location
    lua_pushvalue(L, DECODE_RESULT_IDX);
    // avoid allocations by making every parse_value use the same scratch buffer
    str_buf scratch = new_str_buf();
    if (scratch.data == NULL) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, "Unable to allocate memory for scratch buffer");
        return 2;
    }
    while (iter_peek(&src).ok) {
        {
            // consume until non-blank line, consume initial whitespace, then end loop
            int end_line = consume_whitespace_to_line(&src);
            while (end_line == 1) end_line = consume_whitespace_to_line(&src);
            if (end_line == 2) break;
        }
        if (iter_starts_with(&src, "[[", 2)) {
            iter_skip_n(&src, 2);
            lua_pop(L, 1); // pop current location, we are moving
            int keys_len = parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX);
            if (!keys_len) goto fail;
            if (!iter_starts_with(&src, "]]", 2)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 30, "table heading must end with ]]");
                goto fail;
            }
            iter_skip_n(&src, 2);  // consume ]]
            if (!consume_whitespace_to_line(&src)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 56, "array [[headers]] must have a new line before new values");
                goto fail;
            }
            if (!heading_nav(L, keys_len, true)) goto fail;
        } else if (iter_peek(&src).v == '[') {
            iter_skip(&src);
            lua_pop(L, 1);  // pop current location, we are moving
            int keys_len = parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX);
            if (!keys_len) goto fail;
            if (iter_peek(&src).v != ']') {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "table heading must end with ]");
                goto fail;
            }
            iter_skip(&src);  // consume ]
            if (!consume_whitespace_to_line(&src)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 54, "table [headers] must have a new line before new values");
                goto fail;
            }
            if (!heading_nav(L, keys_len, false)) goto fail;
        } else {
            lua_pushnil(L);
            int val_spacer = lua_gettop(L);
            int keys_len = parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX);
            if (!keys_len) goto fail;
            if (iter_peek(&src).v != '=') {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 35, "keys for assignment must end with =");
                goto fail;
            }
            iter_skip(&src);  // consume =
            if (consume_whitespace_to_line(&src)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 76, "the value in key = value expressions must begin on the same line as the key!");
                goto fail;
            }
            if (!decode_inline_value(L, &src, &scratch, uopts, DECODE_DEFINED_IDX)) goto fail;
            lua_replace(L, val_spacer);
            // [-1?] keys
            // [-?] value
            // [-?-1] current root table
            if (!set_kv(L, keys_len, val_spacer)) goto fail;
            // [-1] current root table
            if (!consume_whitespace_to_line(&src)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 66, "key value pairs must be followed by a new line (or end of content)");
                goto fail;
            }
        }
    }

    lua_settop(L, DECODE_RESULT_IDX);
    free_str_buf(&scratch);
    return 1;

fail:
    lua_settop(L, DECODE_DEFINED_IDX);
    tmlerr_push_ctx_from_iter(get_err_val(L, DECODE_DEFINED_IDX), 7, &src);
    free_str_buf(&scratch);
    lua_pushnil(L);
    push_tmlerr_string(L, get_err_val(L, DECODE_DEFINED_IDX));
    return 2;
}
