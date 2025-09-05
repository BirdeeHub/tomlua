#include <stddef.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

#include "types.h"
#include "parse_keys.h"
#include "parse_val.h"
#include "strict_utils.h"

static inline bool heading_nav(lua_State *L, keys_result *keys, bool array_type, int top) {
    if (!keys->ok) return false;
    if (keys->len <= 0) return false;
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    for (size_t i = 0; i < keys->len; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);  // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);  // remove non-table
            lua_newtable(L);  // create table
            lua_pushvalue(L, -1);
            lua_insert(L, -3);
            lua_rawset(L, -4);   // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            return set_err_upval(L, false, 33, "cannot navigate through non-table");
        } else {
            lua_remove(L, -3);
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
        lua_rawset(L, -4);           // t[len+1] = new element

        // remove parent array table, leave new element on top
        lua_remove(L, -2);
    }
    return true;
}

int tomlua_decode(lua_State *L) {
    // process arguments and options
    if (!lua_isstring(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "tomlua.decode first argument must be a string! tomlua.decode(string, table|bool?) -> table?, err?");
        return 2;
    }
    TomluaUserOpts *uopts = get_opts_upval(L);
    const bool strict = uopts->strict;
    if (strict) create_defined_table(L);
    if (lua_istable(L, 2)) {
        lua_settop(L, 2);
    } else {
        lua_settop(L, 1);
        lua_newtable(L);
    }
    // pop and store top, this will be our result at the end
    const int top = luaL_ref(L, LUA_REGISTRYINDEX);

    size_t len;
    const char *s = lua_tolstring(L, 1, &len);
    lua_pop(L, 1); // pop the string
    str_iter src = str_to_iter(s, len);

    // set top as the starting location
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    // avoid allocations by making every parse_value use the same scratch buffer
    str_buf scratch = new_str_buf();
    keys_result keys = {0};
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
            keys = parse_keys(L, &src);
            if (!keys.ok) goto fail;
            if (!iter_starts_with(&src, "]]", 2)) {
                set_err_upval(L, false, 30, "table heading must end with ]]");
                goto fail;
            }
            iter_skip_n(&src, 2);  // consume ]]
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 56, "array [[headers]] must have a new line before new values");
                goto fail;
            }
            if (strict) {
                if (!heading_nav_strict(L, &keys, true, top)) goto fail;
            } else {
                if (!heading_nav(L, &keys, true, top)) goto fail;
            }
        } else if (iter_peek(&src).v == '[') {
            iter_skip(&src);
            lua_pop(L, 1);  // pop current location, we are moving
            keys = parse_keys(L, &src);
            if (!keys.ok) goto fail;
            if (iter_peek(&src).v != ']') {
                set_err_upval(L, false, 29, "table heading must end with ]");
                goto fail;
            }
            iter_skip(&src);  // consume ]
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 54, "table [headers] must have a new line before new values");
                goto fail;
            }
            if (strict) {
                if (!heading_nav_strict(L, &keys, false, top)) goto fail;
            } else {
                if (!heading_nav(L, &keys, false, top)) goto fail;
            }
        } else {
            keys = parse_keys(L, &src);
            if (!keys.ok) goto fail;
            if (iter_peek(&src).v != '=') {
                set_err_upval(L, false, 35, "keys for assignment must end with =");
                goto fail;
            }
            iter_skip(&src);  // consume =
            if (consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 76, "the value in key = value expressions must begin on the same line as the key!");
                goto fail;
            }
            if (!parse_value(L, &src, &scratch, uopts)) goto fail;
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 66, "key value pairs must be followed by a new line (or end of content)");
                goto fail;
            }
            // [1] value
            // [2] current root table
            if (strict) {
                if (!set_kv_strict(L, &keys)) goto fail;
            } else {
                if (!set_kv(L, &keys)) goto fail;
            }
            // [1] current root table
        }
        // all branches get a key from parse_keys so clear it here
        clear_keys_result(&keys);
    }

    lua_settop(L, 0);
    if (strict) reset_defined_table(L);
    free_str_buf(&scratch);
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    luaL_unref(L, LUA_REGISTRYINDEX, top);
    return 1;

fail:
    lua_settop(L, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, top);
    if (strict) reset_defined_table(L);
    free_str_buf(&scratch);
    clear_keys_result(&keys);
    lua_pushnil(L);
    push_err_upval(L);
    return 2;
}
