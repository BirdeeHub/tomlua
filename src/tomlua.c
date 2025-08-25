#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <string.h>
#include <lauxlib.h>
#include <stdint.h>
#include "str_buf.h"
#include "parse_str.h"

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

static bool is_identifier_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '_') || (c == '-');
}

enum ExprType {
    EXPR_K_V,
    HEADING_TABLE,
    HEADING_ARRAY,
};

struct Keys {
    enum ExprType type;
    struct str_buf *vals;
    size_t capacity;
    size_t len;
    char *err;
};

static bool push_buf_to_keys(struct Keys *dst, struct str_buf buf) {
    if (dst->len >= dst->capacity) {
        dst->capacity *= 2;
        dst->vals = realloc(dst->vals, dst->capacity * sizeof(struct str_buf));
        if (dst->vals == NULL) {
            dst->err = "OOM";
            return false;
        }
    }
    dst->vals[dst->len++] = buf;
    return true;
}

static void free_keys(struct Keys *keys) {
    if (!keys || !keys->vals) return;
    for (size_t i = 0; i < keys->len; i++) {
        free_str_buf(&keys->vals[i]);
    }
    free(keys->vals);
    keys->vals = NULL;
    keys->len = keys->capacity = 0;
}

static struct Keys parse_keys(struct str_iter *src) {
    struct Keys dst = {
        .type = EXPR_K_V,
        .capacity = 4,
        .len = 0,
        .vals = malloc(4 * sizeof(struct str_buf)),
        .err = NULL,
    };
    struct str_buf res = new_str_buf();
    while (iter_peek(src).ok) {
        char c = iter_peek(src).v;
        if (c == '#') {
            if (res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V) {
                dst.err = "keys must be followed by ] ]] or =";
                return dst;
            }
            iter_next(src);
            while (iter_peek(src).ok) {
                if (iter_peek(src).v == '\n') {
                    iter_next(src);
                    break;
                } else if (iter_starts_with(src, "\r\n", 2)) {
                    iter_next(src);
                    iter_next(src);
                    break;
                }
                iter_next(src);
            }
            continue;
        }
        if (c == ' ' || c == '\t') {
            iter_next(src);
            continue;
        } else if (iter_starts_with(src, "[[", 2)) {
            iter_next(src);
            iter_next(src);
            if (res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V) {
                dst.err = "[[ headings may not start in the middle of keys";
                return dst;
            } else {
                dst.type = HEADING_ARRAY;
            }
        } else if (c == '[') {
            iter_next(src);
            if (res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V) {
                dst.err = "[ headings may not start in the middle of keys";
                return dst;
            } else {
                dst.type = HEADING_TABLE;
            }
        } else if (c == '"') {
            iter_next(src);
            char *err = parse_basic_string(&res, src);
            if (err != NULL) {
                dst.err = err;
                return dst;
            }
            if (!push_buf_to_keys(&dst, res)) {
                return dst;
            }
            res = new_str_buf();
            continue;
        } else if (c == '.') {
            iter_next(src);
            if (!(res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V)) {
                dst.err = "key cannot start with a dot";
                return dst;
            }
            if (res.len > 0) {
                if (!push_buf_to_keys(&dst, res)) {
                    return dst;
                }
                res = new_str_buf();
            }
            continue;
        } else if (is_identifier_char(c)) {
            iter_next(src);
            if (!buf_push(&res, c)) {
                dst.err = "OOM";
                return dst;
            }
            continue;
        } else if (c == '\n') {
            iter_next(src);
            if (res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V) {
                dst.err = "literal newlines not allowed in keys without being in strings";
                return dst;
            }
            continue;
        } else if(iter_starts_with(src, "\r\n", 2)) {
            iter_next(src);
            iter_next(src);
            if (res.len > 0 || dst.len > 0 || dst.type != EXPR_K_V) {
                dst.err = "literal newlines not allowed in keys without being in strings";
                return dst;
            }
            continue;
        } else if (dst.type == EXPR_K_V && c == '=') {
            iter_next(src);
            if (res.len != 0) {
                if (!push_buf_to_keys(&dst, res)) {
                    return dst;
                }
            }
            if (dst.len == 0) {
                dst.err = "you must have a key before the = or [in] [[headers]]";
                return dst;
            } else {
                return dst;
            }
        } else if (dst.type == HEADING_ARRAY && iter_starts_with(src, "]]", 2)) {
            iter_next(src);
            iter_next(src);
            if (res.len != 0) {
                if (!push_buf_to_keys(&dst, res)) {
                    return dst;
                }
            }
            if (dst.len == 0) {
                dst.err = "you must have a key in [[headers]]";
                return dst;
            } else {
                return dst;
            }
        } else if (dst.type == HEADING_TABLE && c == ']') {
            iter_next(src);
            if (res.len != 0) {
                if (!push_buf_to_keys(&dst, res)) {
                    return dst;
                }
            }
            if (dst.len == 0) {
                dst.err = "you must have a key in [headers]";
                return dst;
            } else {
                return dst;
            }
        }
    }
    if (res.len != 0) {
        if (!push_buf_to_keys(&dst, res)) {
            return dst;
        }
    }
    if (!iter_peek(src).ok) {
        dst.err = "EOF";
    }
    return dst;
}

static bool heading_nav(lua_State *L, struct Keys *keys, bool array_type, int top) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);
    for (size_t i = 0; i < keys->len; i++) {
        push_buf_to_lua_string(L, &keys->vals[i]); // push key
        lua_gettable(L, -2); // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // remove non-table
            lua_newtable(L); // create table
            push_buf_to_lua_string(L, &keys->vals[i]);
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4); // t[key] = new table
        } else if (!lua_istable(L, -1)) {
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
static bool set_kv(lua_State *L, struct Keys *keys) {
    int value_idx = lua_gettop(L);  // value is on top
    int root_idx = value_idx - 1;   // the table to start navigation from
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (size_t i = 0; i < keys->len - 1; i++) {
        push_buf_to_lua_string(L, &keys->vals[i]); // push key
        lua_gettable(L, -2);                        // get t[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            push_buf_to_lua_string(L, &keys->vals[i]);
            lua_pushvalue(L, -2); // push new table
            lua_settable(L, -4);  // t[key] = new table
        } else if (!lua_istable(L, -1)) {
            // can't navigate through non-table
            return false;
        }
        lua_remove(L, -2); // remove parent table
    }

    // Now top table is where we want to set the value
    push_buf_to_lua_string(L, &keys->vals[keys->len - 1]); // push last key
    lua_pushvalue(L, value_idx); // push value
    lua_settable(L, -3);         // t[last_key] = value

    lua_pop(L, 1); // pop the table
    lua_pop(L, 1); // pop the value
    return true;
}

enum ValueType {
    VALUE_STRING, // lua string
    VALUE_INTEGER, // lua number
    VALUE_FLOAT, // lua number
    VALUE_BOOL, // lua bool
    VALUE_ARRAY, // lua table
    VALUE_TABLE, // lua table
    LOCAL_DATE, // string for now
    LOCAL_TIME, // string for now
    LOCAL_DATETIME, // string for now
    OFFSET_DATETIME, // string for now
};

// TODO: parse_value { // (it starts here)
// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
// this function does not work very well yet
static bool parse_value(lua_State *L, struct str_iter *src) {
    // skip leading whitespace
    while (iter_peek(src).ok) {
        char c = iter_peek(src).v;
        if (c == ' ' || c == '\t') {
            iter_next(src);
            continue;
        }
        break;
    }
    struct iter_result curr = iter_peek(src);
    if (!curr.ok) return false;
    // --- boolean ---
    if (iter_starts_with(src, "true", 4)) {
        for (int i = 0; i < 4; i++) iter_next(src);
        lua_pushboolean(L, 1);
        return true;
    } else if (iter_starts_with(src, "false", 5)) {
        for (int i = 0; i < 5; i++) iter_next(src);
        lua_pushboolean(L, 0);
        return true;
    }
    // --- string ---
    if (curr.v == '"') {
        struct str_buf buf = new_str_buf();
        iter_next(src);
        char *err = parse_basic_string(&buf, src);
        if (err != NULL) {
            free_str_buf(&buf);
            return false;
        }
        push_buf_to_lua_string(L, &buf);
        free_str_buf(&buf);
        return true;
    }

    // --- number (integer or float) --- Should date be done here too?
    if ((curr.v >= '0' && curr.v <= '9') || curr.v == '-' || curr.v == '+') {
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
            return true;
        }
        free_str_buf(&buf);
    }

    // --- array --- should allow trailing comma
    if (curr.v == '[') {
        iter_next(src);
        lua_newtable(L);
        int idx = 1;
        while (iter_peek(src).ok) {
            char d = iter_peek(src).v;
            if (d == ']') {
                iter_next(src);
                return true;
            } else if (d == ',' || d == ' ' || d == '\t' || d == '\n' || d == '\r') {
                iter_next(src);
                continue;
            }
            if (!parse_value(L, src)) return false;
            lua_rawseti(L, -2, idx++);
        }
        return false; // missing closing ]
    }

    // --- inline table --- should not support multiline or trailing comma
    if (curr.v == '{') {
        iter_next(src);
        lua_newtable(L);
        while (iter_peek(src).ok) {
            char d = iter_peek(src).v;
            if (d == '}') {
                iter_next(src);
                return true;
            } else if (d == ',' || d == ' ' || d == '\t') {
                iter_next(src);
                continue;
            }
            struct Keys k = parse_keys(src);
            if (k.err != NULL) { free_keys(&k); return false; }
            if (!parse_value(L, src)) { free_keys(&k); return false; }
            if (!set_kv(L, &k)) { free_keys(&k); return false; }
            free_keys(&k);
        }
        return false; // missing closing }
    }

    // skips through the end of the line on trailing comments and failed parses
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '#') {
            iter_next(src);
            while (iter_peek(src).ok) {
                if (iter_peek(src).v == '\n') {
                    iter_next(src);
                    break;
                } else if (iter_starts_with(src, "\r\n", 2)) {
                    iter_next(src);
                    iter_next(src);
                    break;
                }
                iter_next(src);
            }
            break;
        }
        if (d == '\n') {
            iter_next(src);
            break;
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_next(src);
            iter_next(src);
            break;
        }
        iter_next(src);
    }
    return false;
}
// TODO: parse_value } // (it goes until here)

static int tomlua_parse(lua_State *L) {
    size_t len;
    const char *s = lua_tolstring(L, 1, &len);
    lua_pop(L, 1); // pop the table
    struct str_iter src = str_to_iter(s, len);

    lua_newtable(L);
    int top = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawgeti(L, LUA_REGISTRYINDEX, top);

    while (iter_peek(&src).ok) {
        size_t depth = 0;
        enum ExprType *type;
        struct Keys keys = parse_keys(&src);
        if (keys.err != NULL && strcmp(keys.err, "EOF") == 0) { // doing literal string compare here works but strcmp doesnt?
            free_keys(&keys);
            break;
        }
        if (keys.err != NULL) {
            lua_pop(L, 1); // pop the table
            lua_pushnil(L);
            lua_pushfstring(L, "%s", keys.err);
            free_keys(&keys);
            return 2;
        }
        if (keys.type == EXPR_K_V) {
            // [1] current root table
            if (!parse_value(L, &src)) {  // parse_value should push value on top of stack
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "failed to parse value for key %s", keys.vals[keys.len - 1].data);
                free_keys(&keys);
                return 2;
            }
            // [1] value
            // [2] current root table
            if (!set_kv(L, &keys)) {
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "cannot set key %s, path blocked by non-table", keys.vals[keys.len - 1].data);
                free_keys(&keys);
                return 2;
            }
            // [1] current root table
        } else if (keys.type == HEADING_TABLE) {
            lua_pop(L, 1); // pop the table
            if (!heading_nav(L, &keys, false, top)) {
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "unable to heading_nav to %s", keys.vals[keys.len - 1].data);
                free_keys(&keys);
                return 2;
            }
        } else if (keys.type == HEADING_ARRAY) {
            lua_pop(L, 1); // pop the table
            if (!heading_nav(L, &keys, true, top)) {
                lua_pop(L, 1); // pop the table
                lua_pushnil(L);
                lua_pushfstring(L, "unable to heading_nav to %s", keys.vals[keys.len - 1].data);
                free_keys(&keys);
                return 2;
            }
        }
        free_keys(&keys);
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
