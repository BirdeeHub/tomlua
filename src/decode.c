#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>

#include "types.h"
#include "parse_str.h"
#include "parse_keys.h"

static inline bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

static bool heading_nav(lua_State *L, keys_result *keys, bool array_type, int top) {
    if (!keys->ok) { return false; }
    if (keys->len <= 0) { return false; }
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    for (size_t i = 0; i < keys->len; i++) {
        if (!push_buf_to_lua_string(L, &keys->v[i])) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_gettable(L, -2); // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // remove non-table
            lua_newtable(L); // create table
            if (!push_buf_to_lua_string(L, &keys->v[i])) {
                return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
            }
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4); // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            lua_pop(L, 1); // remove non-table
            return set_err_upval(L, false, 33, "cannot navigate through non-table");
        }
        lua_remove(L, -2); // remove parent table, keep child on top
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
    if (!keys->ok) { return false; }
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
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4);  // t[key] = new table
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
    lua_pushvalue(L, value_idx); // push value
    lua_settable(L, -3);         // t[last_key] = value

    lua_pop(L, 2);
    return true;
}

static bool parse_value(lua_State *L, str_iter *src, str_buf *buf);

// adds a table to the lua stack and return NULL or error
static inline bool parse_inline_table(lua_State *L, str_iter *src, str_buf *buf) {
    lua_newtable(L);
    bool last_was_comma = false;
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '}') {
            iter_skip(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return set_err_upval(L, false, 42, "trailing comma in inline table not allowed");
            }
            return true;
        } else if (iter_peek(src).v == '\n') {
            iter_skip(src);
            return set_err_upval(L, false, 35, "inline tables can not be multi-line");
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            return set_err_upval(L, false, 35, "inline tables can not be multi-line");
        } else if (d == ',') {
            iter_skip(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return set_err_upval(L, false, 18, "2 commas in a row!");
            }
            last_was_comma = true;
            continue;
        } else if (d == ' ' || d == '\t') {
            iter_skip(src);
            continue;
        }
        last_was_comma = false;
        keys_result keys = parse_keys(L, src);
        if (!keys.ok) {
            clear_keys_result(&keys);
            return false;
        }
        if (iter_peek(src).ok && iter_peek(src).v != '=') {
            clear_keys_result(&keys);
            return set_err_upval(L, false, 35, "keys for assignment must end with =");
        }
        iter_skip(src);
        if (consume_whitespace_to_line(src)) {
            clear_keys_result(&keys);
            return set_err_upval(L, false, 76, "the value in key = value expressions must begin on the same line as the key!");
        }
        if (!parse_value(L, src, buf)) {
            clear_keys_result(&keys);
            return false;
        }
        if (!set_kv(L, &keys)) {
            clear_keys_result(&keys);
            return false;
        }
        clear_keys_result(&keys);
        if (consume_whitespace_to_line(src)) {
            return set_err_upval(L, false, 39, "toml inline tables cannot be multi-line");
        }
        iter_result next = iter_peek(src);
        if (next.ok && (next.v != ',' && next.v != '}')) {
            return set_err_upval(L, false, 65, "toml inline table values must be separated with , or ended with }");
        }
    }
    return set_err_upval(L, false, 17, "missing closing }");
}

// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
static bool parse_value(lua_State *L, str_iter *src, str_buf *buf) {
    iter_result curr = iter_peek(src);
    if (!curr.ok) return set_err_upval(L, false, 34, "expected value, got end of content");
    // --- boolean ---
    if (iter_starts_with(src, "true", 4)) {
        iter_skip_n(src, 4);
        lua_pushboolean(L, 1);
        return true;
    } else if (iter_starts_with(src, "false", 5)) {
        iter_skip_n(src, 5);
        lua_pushboolean(L, 0);
        return true;
    // --- strings ---
    } else if (iter_starts_with(src, "\"\"\"", 3)) {
        buf_soft_reset(buf);
        iter_skip_n(src, 3);
        if (!parse_multi_basic_string(L, buf, src)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        return true;
    } else if (curr.v == '"') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_basic_string(L, buf, src)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        return true;
    } else if (iter_starts_with(src, "'''", 3)) {
        buf_soft_reset(buf);
        iter_skip_n(src, 3);
        if (!parse_multi_literal_string(L, buf, src)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        return true;
    } else if (curr.v == '\'') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_literal_string(L, buf, src)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_err_upval(L, false, 48, "tomlua.decode failed to push string to lua stack");
        }
        return true;
    // --- numbers (and dates) ---
    } else if (iter_starts_with(src, "inf", 3)) {
        iter_skip_n(src, 3);
        lua_pushnumber(L, INFINITY);
        return true;
    } else if (iter_starts_with(src, "nan", 3)) {
        iter_skip_n(src, 3);
        lua_pushnumber(L, NAN);
        return true;
    } else if ((curr.v >= '0' && curr.v <= '9') || curr.v == '-' || curr.v == '+') {
        if (curr.v == '+') {
            if (iter_starts_with(src, "+inf", 4)) {
                iter_skip_n(src, 4);
                lua_pushnumber(L, INFINITY);
                return true;
            } else if (iter_starts_with(src, "+nan", 4)) {
                iter_skip_n(src, 4);
                lua_pushnumber(L, NAN);
                return true;
            }
        } else if (curr.v == '-') {
            if (iter_starts_with(src, "-inf", 4)) {
                iter_skip_n(src, 4);
                lua_pushnumber(L, -INFINITY);
                return true;
            } else if (iter_starts_with(src, "-nan", 4)) {
                iter_skip_n(src, 4);
                lua_pushnumber(L, -NAN);
                return true;
            }
        } else if (iter_starts_with(src, "0x", 2)) {
            // Hex integer
            buf_soft_reset(buf);
            iter_skip_n(src, 2);
            bool was_underscore = false;
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if (is_hex_char(ch)) {
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_err_upval(L, false, 51, "consecutive underscores not allowed in hex literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_err_upval(L, false, 53, "hex literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) {
                return set_err_upval(L, false, 17, "empty hex literal");
            }
            // Convert buffer to integer
            long long val = strtoll(buf->data, NULL, 16);
            lua_pushinteger(L, val);
            return true;
        } else if (iter_starts_with(src, "0o", 2)) {
            // Octal integer
            buf_soft_reset(buf);
            iter_skip_n(src, 2);
            bool was_underscore = false;
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if ((ch >= '0' && ch <= '7')) {
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_err_upval(L, false, 53, "consecutive underscores not allowed in octal literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_err_upval(L, false, 55, "octal literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) {
                return set_err_upval(L, false, 19, "empty octal literal");
            }
            long long val = strtoll(buf->data, NULL, 8);
            lua_pushinteger(L, val);
            return true;
        } else if (iter_starts_with(src, "0b", 2)) {
            // binary integer
            buf_soft_reset(buf);
            iter_skip_n(src, 2);
            bool was_underscore = false;
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if ((ch == '0' || ch == '1')) {
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_err_upval(L, false, 54, "consecutive underscores not allowed in binary literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_err_upval(L, false, 56, "binary literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) {
                return set_err_upval(L, false, 20, "empty binary literal");
            }
            long long val = strtoll(buf->data, NULL, 2);
            lua_pushinteger(L, val);
            return true;
        } else {
            // detect dates and pass on as strings, and numbers are allowed to have underscores in them (only 1 consecutive underscore at a time)
            // is date if it has a - in it not immediately preceded by e or E
            // is date if it has a : in it
            buf_soft_reset(buf);
            bool is_float = false;
            bool is_date = false;
            bool t_used = false;
            bool z_used = false;
            bool was_underscore = true;
            if (curr.v == '-' || curr.v == '+') buf_push(buf, curr.v);
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if (ch == '_') {
                    iter_skip(src);
                    if (was_underscore) {
                        return set_err_upval(L, false, 46, "consecutive underscores not allowed in numbers");
                    }
                    was_underscore = true;
                } else if (ch == 'e' || ch == 'E') {
                    is_float = true;
                    buf_push(buf, ch);
                    iter_skip(src);
                    iter_result next = iter_peek(src);
                    if (next.ok && (next.v == '+' || next.v == '-')) {
                        buf_push(buf, ch);
                        iter_skip(src);
                    }
                    was_underscore = false;
                } else if (ch == ':') {
                    is_date = true;
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch == '-') {
                    is_date = true;
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (is_date && !t_used && (ch == 'T' || ch == ' ')) {
                    t_used = true;
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (is_date && !z_used && ch == 'Z') {
                    z_used = true;
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch == '.') {
                    is_float = true;
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else if (ch >= '0' && ch <= '9') {
                    was_underscore = false;
                    buf_push(buf, ch);
                    iter_skip(src);
                } else {
                    was_underscore = false;
                    break;
                }
            }
            if (was_underscore) {
                return set_err_upval(L, false, 56, "number literals not allowed to have trailing underscores");
            }
            if (buf->len > 0) {
                if (is_float) {
                    lua_pushnumber(L, strtod(buf->data, NULL));
                } else if (is_date) {
                    if (!push_buf_to_lua_string(L, buf)) {
                        return set_err_upval(L, false, 53, "tomlua.decode failed to push date string to lua stack");
                    }
                } else {
                    lua_pushinteger(L, strtoll(buf->data, NULL, 10));
                }
                return true;
            }
        }
    // --- array --- allows trailing comma and multiline
    } else if (curr.v == '[') {
        iter_skip(src);
        lua_newtable(L);
        int idx = 1;
        while (iter_peek(src).ok) {
            char d = iter_peek(src).v;
            if (d == ']') {
                iter_skip(src);
                return true;
            } else if (d == ',' || d == ' ' || d == '\t' || d == '\n' || d == '\r') {
                iter_skip(src);
                continue;
            }
            if (!parse_value(L, src, buf)) return false;
            lua_rawseti(L, -2, idx++);
        }
        return set_err_upval(L, false, 17, "missing closing ]");
    // --- inline table --- does NOT support multiline or trailing comma (in strict mode)
    } else if (curr.v == '{') {
        iter_skip(src);
        return parse_inline_table(L, src, buf);
    }
    return set_err_upval(L, false, 13, "invalid value");
}

int tomlua_decode(lua_State *L) {
    // TODO: process strict mode.
    // this likely involves making a heading_nav_strict() and a set_kv_strict()
    bool strict = get_opts_upval(L)->strict;

    // process arguments
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
            if (!parse_value(L, &src, &scratch)) {  // parse_value should push value on top of stack
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
