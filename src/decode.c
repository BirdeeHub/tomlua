#include <stddef.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

#include "types.h"
#include "parse_keys.h"
#include "parse_val.h"

int tomlua_decode(lua_State *L) {
    // TODO: process strict mode. (default false)
    // this likely involves making a heading_nav_strict() and a set_kv_strict()
    const bool strict = get_opts_upval(L)->strict;
    // allows multi line tables with trailing commas (default false)
    const bool enhanced_tables = get_opts_upval(L)->enhanced_tables;

    // verify arguments
    if (!lua_isstring(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "tomlua.decode first argument must be a string! tomlua.decode(string, table|bool?) -> table?, err?");
        return 2;
    }
    if (lua_istable(L, 2)) {
        lua_settop(L, 2);
    } else {
        lua_settop(L, 1);
        lua_newtable(L);
    }

    // pops and saves the table
    int top = luaL_ref(L, LUA_REGISTRYINDEX);

    size_t len;
    const char *s = lua_tolstring(L, 1, &len);
    lua_pop(L, 1); // pop the string
    str_iter src = str_to_iter(s, len);

    // set top as the starting location
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    // avoid allocations by making every parse_value use the same scratch buffer
    str_buf scratch = new_str_buf();
    while (iter_peek(&src).ok) {
        {
            // consume until non-blank line, consume initial whitespace, then end loop
            int end_line = consume_whitespace_to_line(&src);
            while (end_line == 1) {
                end_line = consume_whitespace_to_line(&src);
            }
            if (end_line == 2) break;
        }
        iter_result curr = iter_peek(&src);
        if (iter_starts_with(&src, "[[", 2)) {
            iter_skip_n(&src, 2);
            keys_result keys = parse_keys(L, &src);
            if (!keys.ok) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            if (iter_peek(&src).ok && !iter_starts_with(&src, "]]", 2)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table heading must end with ]]");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            iter_skip_n(&src, 2);
            if (!consume_whitespace_to_line(&src)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "array [[headers]] must have a new line before new values");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            lua_pop(L, 1);
            if (!heading_nav(L, &keys, true, top)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            clear_keys_result(&keys);
        } else if (curr.v == '[') {
            iter_skip(&src);
            keys_result keys = parse_keys(L, &src);
            if (!keys.ok) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            if (iter_peek(&src).ok && iter_peek(&src).v != ']') {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table heading must end with ]");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            iter_skip(&src);
            if (!consume_whitespace_to_line(&src)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table [headers] must have a new line before new values");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            lua_pop(L, 1);
            if (!heading_nav(L, &keys, false, top)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            clear_keys_result(&keys);
        } else {
            keys_result keys = parse_keys(L, &src);
            if (!keys.ok) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            if (iter_peek(&src).ok && iter_peek(&src).v != '=') {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "keys for assignment must end with =");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            iter_skip(&src);
            if (consume_whitespace_to_line(&src)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "the value in key = value expressions must begin on the same line as the key!");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            if (!parse_value(L, &src, &scratch, strict, enhanced_tables)) {  // parse_value should push value on top of stack
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            if (!consume_whitespace_to_line(&src)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushstring(L, "key value pairs must be followed by a new line (or end of content)");
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            // [1] value
            // [2] current root table
            if (!set_kv(L, &keys)) {
                free_str_buf(&scratch);
                clear_keys_result(&keys);
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                push_err_upval(L);
                luaL_unref(L, LUA_REGISTRYINDEX, top);
                return 2;
            }
            // [1] current root table
            clear_keys_result(&keys);
        }
    }

    free_str_buf(&scratch);
    lua_pop(L, 1); // pop current target, add top back
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    luaL_unref(L, LUA_REGISTRYINDEX, top);
    return 1; // on fail return 2; lua nil and an error
}
