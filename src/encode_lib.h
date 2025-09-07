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

static int is_lua_array(lua_State *L) {
    // probably dont need this check:
    if (!lua_istable(L, 1)) {
        lua_settop(L, 0);
        lua_pushboolean(L, false);
        return 1;
    }

    // getmetatable(stack_top).array to allow overriding of representation
    if (lua_getmetatable(L, 1)) {
        // stack: ... table mt
        lua_getfield(L, -1, "array");
        if (!lua_isnil(L, -1)) {
            if (lua_toboolean(L, -1)) {
                lua_settop(L, 0);
                lua_pushboolean(L, true);
                return 1;
            } else {
                lua_settop(L, 0);
                lua_pushboolean(L, false);
                return 1;
            }
        }
        lua_settop(L, 1);
    }
    bool is_array = true;
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, 1) != 0) {
        // now at stack: key value
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(int64_t)(key)) {
                is_array = false;
                break;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            is_array = false;
            break;
        }
    }
    if (highest_int_key != count || count == 0) is_array = false;
    lua_settop(L, 0);
    lua_pushboolean(L, is_array);
    return 1;
}

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

// TODO: make this push an escaped """multiline""" string
static int lbuf_push_multi_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    size_t len;
    const char *str = lua_tolstring(L, 2, &len);
    buf_push_str(buf, str, len);
    return 0;
}

// TODO: make this push an escaped "simple" string
static int lbuf_push_simple_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    size_t len;
    const char *str = lua_tolstring(L, 2, &len);
    buf_push_str(buf, str, len);
    return 0;
}

// TODO: make this push an escaped toml key (varargs)
static int lbuf_push_key(lua_State *L) {
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

static inline void push_encode(lua_State *L, int opts_idx) {
    push_embedded_encode(L);
    lua_pushvalue(L, opts_idx);
    lua_pushcfunction(L, is_lua_array);
    lua_newtable(L);
    lua_pushcfunction(L, lbuf_new);
    lua_setfield(L, -2, "new_buf");
    lua_call(L, 3, 1);
}

#endif  // SRC_ENCODE_LIB_H_
