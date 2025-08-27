#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "str_buf.h"
#include "parse_str.h"
#include "parse_keys.h"

// TODO: delete this, just for debugging
static void print_lua_stack(lua_State *L, const char *label) {
    int top = lua_gettop(L);
    printf("=== Stack: %s ===\n", label);
    for (int i = top; i >= 1; i--) {
        int t = lua_type(L, i);
        const char *type_name = lua_typename(L, t);
        int neg_index = i - top - 1; // e.g. i=top => -1, i=top-1 => -2
        printf("%d (%d): %s", i, neg_index, type_name);
        if (t == LUA_TSTRING || t == LUA_TNUMBER) {
            size_t len;
            const char *s = lua_tolstring(L, i, &len);
            printf(" -> '%.*s'", (int)len, s);
        }
        printf("\n");
    }
    printf("===================\n");
}

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

static bool push_buf_to_lua_string(lua_State *L, const struct str_buf *buf) {
    if (!buf || !buf->data) return false;
    lua_pushlstring(L, buf->data, buf->len);
    return true;
}

static bool heading_nav(lua_State *L, struct keys_result *keys, bool array_type, int top) {
    if (keys->err != NULL) { return false; }
    if (keys->len <= 0) { return false; }
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    for (size_t i = 0; i < keys->len; i++) {
        push_buf_to_lua_string(L, &keys->v[i]); // push key
        lua_gettable(L, -2); // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // remove non-table
            lua_newtable(L); // create table
            push_buf_to_lua_string(L, &keys->v[i]);
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4); // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            lua_pop(L, 1); // remove non-table
            return false;
        }
        lua_remove(L, -2); // remove parent table, keep child on top
    }
    if (array_type) {
        // We’re at the table that should act as an array
        if (!lua_istable(L, -1)) {
            return false;
        }
        size_t len = lua_objlen(L, -1);
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
static bool set_kv(lua_State *L, struct keys_result *keys) {
    if (keys->err != NULL) { return false; }
    if (keys->len <= 0) {
        keys->err = "no key provided to set";
        return false;
    }
    int value_idx = lua_gettop(L);  // value is on top
    int root_idx = value_idx - 1;   // the table to start navigation from
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (size_t i = 0; i < keys->len - 1; i++) {
        push_buf_to_lua_string(L, &keys->v[i]); // push key
        lua_gettable(L, -2);                        // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            push_buf_to_lua_string(L, &keys->v[i]);
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4);  // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            lua_pop(L, 1); // pop the table
            lua_pop(L, 1); // pop the value
            keys->err = "key is not a table";
            return false;
        }
        lua_remove(L, -2); // remove parent table
    }

    // Now top table is where we want to set the value
    push_buf_to_lua_string(L, &keys->v[keys->len - 1]); // push last key
    lua_pushvalue(L, value_idx); // push value
    lua_settable(L, -3);         // t[last_key] = value

    lua_pop(L, 1); // pop the table
    lua_pop(L, 1); // pop the value
    return true;
}

static char *parse_value(lua_State *L, struct str_iter *src);

// adds a table to the lua stack and return NULL or error
static char *parse_table(lua_State *L, struct str_iter *src) {
    lua_newtable(L);
    bool last_was_comma = false;
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '}') {
            iter_next(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return "trailing comma in inline table not allowed";
            }
            return NULL;
        } else if (iter_peek(src).v == '\n') {
            iter_next(src);
            return "inline tables can not be multi-line";
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_next(src);
            iter_next(src);
            return "inline tables can not be multi-line";
        } else if (d == ',') {
            iter_next(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return "2 commas in a row!";
            }
            last_was_comma = true;
            continue;
        } else if (d == ' ' || d == '\t') {
            iter_next(src);
            continue;
        }
        last_was_comma = false;
        struct keys_result keys = parse_keys(src);
        if (keys.err != NULL) {
            char *err = keys.err;
            keys.err = NULL;
            clear_keys_result(&keys);
            return err;
        }
        if (iter_peek(src).ok && iter_peek(src).v != '=') {
            clear_keys_result(&keys);
            return "keys for assignment must end with =";
        }
        iter_next(src);
        if (consume_whitespace_to_line(src)) {
            clear_keys_result(&keys);
            return "the value in key = value expressions must begin on the same line as the key!";
        }
        char *err = parse_value(L, src);
        if (err != NULL) {
            char *err = keys.err;
            keys.err = NULL;
            clear_keys_result(&keys);
            return err;
        }
        if (!set_kv(L, &keys)) {
            char *err = keys.err;
            keys.err = NULL;
            clear_keys_result(&keys);
            return err;
        }
        clear_keys_result(&keys);
        if (consume_whitespace_to_line(src)) {
            return "toml inline tables cannot be multi-line";
        }
        struct iter_result next = iter_peek(src);
        if (next.ok && (next.v != ',' && next.v != '}')) {
            return "toml inline table values must be separated with , or ended with }";
        }
    }
    return "missing closing }";
}

// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
static char *parse_value(lua_State *L, struct str_iter *src) {
    struct iter_result curr = iter_peek(src);
    if (!curr.ok) return "expected value, got end of content";
    // --- boolean ---
    if (iter_starts_with(src, "true", 4)) {
        for (int i = 0; i < 4; i++) iter_next(src);
        lua_pushboolean(L, 1);
        return NULL;
    } else if (iter_starts_with(src, "false", 5)) {
        for (int i = 0; i < 5; i++) iter_next(src);
        lua_pushboolean(L, 0);
        return NULL;
    // --- strings ---
    } else if (iter_starts_with(src, "\"\"\"", 3)) {
        struct str_buf buf = new_str_buf();
        iter_next(src);
        iter_next(src);
        iter_next(src);
        char *err = parse_multi_basic_string(&buf, src);
        if (err != NULL) {
            free_str_buf(&buf);
            return err;
        }
        push_buf_to_lua_string(L, &buf);
        free_str_buf(&buf);
        return NULL;
    } else if (curr.v == '"') {
        struct str_buf buf = new_str_buf();
        iter_next(src);
        char *err = parse_basic_string(&buf, src);
        if (err != NULL) {
            free_str_buf(&buf);
            return err;
        }
        push_buf_to_lua_string(L, &buf);
        free_str_buf(&buf);
        return NULL;
    } else if (iter_starts_with(src, "'''", 3)) {
        struct str_buf buf = new_str_buf();
        iter_next(src);
        iter_next(src);
        iter_next(src);
        char *err = parse_multi_literal_string(&buf, src);
        if (err != NULL) {
            free_str_buf(&buf);
            return err;
        }
        push_buf_to_lua_string(L, &buf);
        free_str_buf(&buf);
        return NULL;
    } else if (curr.v == '\'') {
        struct str_buf buf = new_str_buf();
        iter_next(src);
        char *err = parse_literal_string(&buf, src);
        if (err != NULL) {
            free_str_buf(&buf);
            return err;
        }
        push_buf_to_lua_string(L, &buf);
        free_str_buf(&buf);
        return NULL;
    // --- numbers (and dates?) ---
    // TODO: add dates and fix base detection
    } else if ((curr.v >= '0' && curr.v <= '9') || curr.v == '-' || curr.v == '+') {
        struct str_buf buf = new_str_buf();
        bool is_float = false;
        while (iter_peek(src).ok) {
            char ch = iter_peek(src).v;
            if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
                if (ch == '.' || ch == 'e' || ch == 'E') is_float = true;
                buf_push(&buf, ch);
                iter_next(src);
            } else {
                break;
            }
        }
        if (buf.len > 0) {
            if (is_float) {
                lua_pushnumber(L, strtod(buf.data, NULL));
            } else {
                lua_pushinteger(L, strtoll(buf.data, NULL, 10));
            }
            free_str_buf(&buf);
            return NULL;
        }
        free_str_buf(&buf);
    // --- array --- allows trailing comma and multiline
    } else if (curr.v == '[') {
        iter_next(src);
        lua_newtable(L);
        int idx = 1;
        while (iter_peek(src).ok) {
            char d = iter_peek(src).v;
            if (d == ']') {
                iter_next(src);
                return NULL;
            } else if (d == ',' || d == ' ' || d == '\t' || d == '\n' || d == '\r') {
                iter_next(src);
                continue;
            }
            char *err = parse_value(L, src);
            if (err != NULL) return err;
            lua_rawseti(L, -2, idx++);
        }
        return "missing closing ]";
    // --- inline table --- does NOT support multiline or trailing comma (in strict mode)
    } else if (curr.v == '{') {
        iter_next(src);
        return parse_table(L, src);
    }
    return "invalid value";
}

static int tomlua_parse(lua_State *L) {
    size_t len;
    const char *s = lua_tolstring(L, 1, &len);
    lua_pop(L, 1); // pop the string
    struct str_iter src = str_to_iter(s, len);

    lua_newtable(L);
    int top = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);

    while (iter_peek(&src).ok) {
        // consume until non-blank line, consume initial whitespace, then end loop
        while (consume_whitespace_to_line(&src)) {
            if (iter_peek(&src).ok == false) break;
        }
        struct iter_result curr = iter_peek(&src);
        if (!curr.ok) break;
        char c = curr.v;
        if (iter_starts_with(&src, "[[", 2)) {
            iter_next(&src);
            iter_next(&src);
            struct keys_result keys = parse_keys(&src);
            if (keys.err != NULL) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushfstring(L, "%s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            if (iter_peek(&src).ok && !iter_starts_with(&src, "]]", 2)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table heading must end with ]]");
                clear_keys_result(&keys);
                return 2;
            }
            iter_next(&src);
            iter_next(&src);
            if (!consume_whitespace_to_line(&src)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "array [[headers]] must have a new line before new values");
                clear_keys_result(&keys);
                return 2;
            }
            lua_pop(L, 1);
            if (!heading_nav(L, &keys, true, top)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushfstring(L, "%s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            clear_keys_result(&keys);
        } else if (c == '[') {
            iter_next(&src);
            struct keys_result keys = parse_keys(&src);
            if (keys.err != NULL) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushfstring(L, "%s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            if (iter_peek(&src).ok && iter_peek(&src).v != ']') {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table heading must end with ]");
                clear_keys_result(&keys);
                return 2;
            }
            iter_next(&src);
            if (!consume_whitespace_to_line(&src)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "table [headers] must have a new line before new values");
                clear_keys_result(&keys);
                return 2;
            }
            lua_pop(L, 1);
            if (!heading_nav(L, &keys, false, top)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushfstring(L, "%s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            clear_keys_result(&keys);
        } else if (is_identifier_char(c) || c == '\'' || c == '"') {
            struct keys_result keys = parse_keys(&src);
            if (keys.err != NULL) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushfstring(L, "%s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            if (iter_peek(&src).ok && iter_peek(&src).v != '=') {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "keys for assignment must end with =");
                clear_keys_result(&keys);
                return 2;
            }
            iter_next(&src);
            if (consume_whitespace_to_line(&src)) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "the value in key = value expressions must begin on the same line as the key!");
                clear_keys_result(&keys);
                return 2;
            }
            char *err = parse_value(L, &src);
            if (err != NULL) {  // parse_value should push value on top of stack
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "failed to parse value for key due to error: %s", err);
                clear_keys_result(&keys);
                return 2;
            }
            if (!consume_whitespace_to_line(&src)) {
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "key value pairs must be followed by a new line (or end of content)");
                clear_keys_result(&keys);
                return 2;
            }
            // [1] value
            // [2] current root table
            if (!set_kv(L, &keys)) {
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "cannot set key due to: %s", keys.err);
                clear_keys_result(&keys);
                return 2;
            }
            // [1] current root table
            clear_keys_result(&keys);
        }
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    luaL_unref(L, LUA_REGISTRYINDEX, top);
    return 1; // on fail return 2; lua nil and an error
}

int luaopen_tomlua(lua_State *L) {
    // --- 64-bit integer check ---
    if (sizeof(lua_Integer) < 8) {
        return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    }

    lua_newtable(L); // module table
    lua_pushcfunction(L, tomlua_parse);
    lua_setfield(L, -2, "decode");
    return 1;
}
