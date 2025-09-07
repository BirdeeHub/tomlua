// Copyright 2025 Birdee
#ifndef SRC_ENCODE_LIB_H_
#define SRC_ENCODE_LIB_H_

#include <lua.h>
#include <stddef.h>
#include <stdio.h>
#include "./types.h"

#define STR(x) #x
#define XSTR(x) STR(x)
#ifndef EMBEDDED_LUA
#define EMBEDDED_LUA ../tmp/embedded.h
#endif
#include XSTR(EMBEDDED_LUA)

// getmetatable(stack_top).toml_type to allow overriding of representation
static inline TomlType get_meta_toml_type(lua_State *L, int idx) {
    if (luaL_getmetafield(L, idx, "toml_type")) {
        if (lua_isnumber(L, -1)) {
            lua_Number n = lua_tonumber(L, -1);
            if (is_valid_toml_type(n)) {
                lua_pop(L, 1);
                return (TomlType)n;
            }
        }
        lua_pop(L, 1);
    }
    return TOML_UTI;
}

static bool is_lua_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, idx)) {
        lua_settop(L, old_top);
        return false;
    }
    switch (get_meta_toml_type(L, idx)) {
        case TOML_ARRAY: {
            lua_settop(L, old_top);
            return true;
        }
        case TOML_TABLE: {
            lua_settop(L, old_top);
            return false;
        }
        default: break;
    }
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, idx) != 0) {
        // now at stack: key value
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(int64_t)(key)) {
                lua_settop(L, old_top);
                return false;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_settop(L, old_top);
            return false;
        }
    }
    lua_settop(L, old_top);
    if (highest_int_key != count || count == 0) {
        return false;
    }
    return true;
}

// works for char or uint32_t
static inline bool buf_push_toml_escaped_char(str_buf *buf, uint32_t c, bool esc_non_ascii) {
    switch (c) {
        case '\\': return buf_push_str(buf, "\\\\", 2); break;
        case '"':  return buf_push_str(buf, "\\\"", 2); break;
        case '\b': return buf_push_str(buf, "\\b", 2); break;
        case '\t': return buf_push_str(buf, "\\t", 2); break;
        case '\n': return buf_push_str(buf, "\\n", 2); break;
        case '\f': return buf_push_str(buf, "\\f", 2); break;
        case '\r': return buf_push_str(buf, "\\r", 2); break;
        default:
            if (c <= 0x7F) {
                return buf_push(buf, c);  // normal ASCII
            } else if (esc_non_ascii) {
                if (c <= 0xFFFF) {
                    char out[7];
                    int len = snprintf(out, sizeof(out), "\\u%04X", c);
                    return buf_push_str(buf, out, len);
                } else {
                    char out[11];
                    int len = snprintf(out, sizeof(out), "\\U%08X", c);
                    return buf_push_str(buf, out, len);
                }
            } else {
                return buf_push_utf8(buf, c);
            }
    }
}

static int lbuf_push_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    size_t len;
    const char *str = lua_tolstring(L, 2, &len);
    buf_push_str(buf, str, len);
    return 0;
}

// TODO: make this push an escaped "simple" string to dst
static inline bool buf_push_esc_simple(str_buf *dst, str_iter *src) {
    buf_push(dst, '"');
    buf_push_str(dst, "TODO: escape simple string contents", 35);
    buf_push(dst, '"');
    return true;
}

static int lbuf_push_simple_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    str_iter src = lua_str_to_iter(L, 2);
    if (!buf_push_esc_simple(buf, &src)) return luaL_error(L, "failed to push escaped simple string");
    return 0;
}

// TODO: make this push an escaped toml key (varargs, don't push =)
static int lbuf_push_keys(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    size_t len;
    const char *str = lua_tolstring(L, 2, &len);
    buf_push_str(buf, str, len);
    return 0;
}

static int lbuf_index(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, lbuf_push_str);
    lua_setfield(L, -2, "push_str");
    lua_pushcfunction(L, lbuf_push_simple_str);
    lua_setfield(L, -2, "push_esc_simple");
    lua_pushcfunction(L, lbuf_push_keys);
    lua_setfield(L, -2, "push_keys");
    return 1;
}

static int lbuf_gc(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    if (buf->cap) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->cap = buf->len = 0;
    return 0;
}

static int lbuf_tostring(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    if (!buf->data) lua_pushliteral(L, "");
    else lua_pushlstring(L, buf->data, buf->len);
    return 1;
}

static int lbuf_new(lua_State *L) {
    str_buf *buf = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
    if (luaL_newmetatable(L, "LStrBuf")) {
        lua_pushcfunction(L, lbuf_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, lbuf_gc);
        lua_setfield(L, -2, "__gc");
        lbuf_index(L);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static int lis_lua_array(lua_State *L) {
    lua_pushboolean(L, is_lua_array(L, 1));
    return 1;
}

static inline void push_encode(lua_State *L, int opts_idx, int types_idx) {
    int top = lua_gettop(L);
    opts_idx = absindex(top, opts_idx);
    types_idx = absindex(top, types_idx);
    push_embedded_encode(L);
    lua_pushvalue(L, opts_idx);
    lua_newtable(L);
    lua_pushcfunction(L, lis_lua_array);
    lua_setfield(L, -2, "is_array");
    lua_pushcfunction(L, lbuf_new);
    lua_setfield(L, -2, "new_buf");
    lua_pushvalue(L, types_idx);
    lua_setfield(L, -2, "types");
    lua_call(L, 2, 1);
}

#endif  // SRC_ENCODE_LIB_H_
