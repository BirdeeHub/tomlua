#include <lua.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "dates.h"
#include "types.h"
#include "encode.h"

#define STR(x) #x
#define XSTR(x) STR(x)
#ifndef EMBEDDED_LUA
#define EMBEDDED_LUA ../tmp/embedded.h
#endif
#include XSTR(EMBEDDED_LUA)

// works for char or uint32_t
static inline bool buf_push_toml_escaped_char(str_buf *buf, uint32_t c, bool esc_non_ascii) {
    switch (c) {
        case '"':  return buf_push_str(buf, "\\\"", 2); break;
        case '\n': return buf_push_str(buf, "\\n", 2); break;
        case '\r': return buf_push_str(buf, "\\r", 2); break;
        case '\\': return buf_push_str(buf, "\\\\", 2); break;
        case '\b': return buf_push_str(buf, "\\b", 2); break;
        case '\t': return buf_push_str(buf, "\\t", 2); break;
        case '\f': return buf_push_str(buf, "\\f", 2); break;
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

// 0 for neither, 1 for str, 2 for buf.
static inline int is_str_or_buf(lua_State *L, int idx) {
    int type = lua_type(L, idx);
    if (type == LUA_TSTRING) {
        return 1;
    } else if (type == LUA_TUSERDATA && udata_is_of_type(L, idx, "TomluaStrBuf")) {
        return 2;
    }
    return 0;
}

static inline int lbuf_push_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    switch (is_str_or_buf(L, 2)) {
        case 1: {
            size_t len;
            const char *str = lua_tolstring(L, 2, &len);
            if (!buf_push_str(buf, str, len)) return luaL_error(L, "failed to push string");
            lua_settop(L, 1);
        } break;
        case 2: {
            str_buf * arg = (str_buf *)lua_touserdata(L, 2);
            if (!buf_push_str(buf, arg->data, arg->len)) return luaL_error(L, "failed to push string");
            lua_settop(L, 1);
        } break;
        default: return luaL_error(L, "expected string or string buffer");
    }
    return 1;
}

// TODO: improve this
static inline bool buf_push_num(str_buf *buf, lua_Number n) {
    if (isnan(n)) {
        if (!buf_push_str(buf, "nan", 3)) return false;
    } else if (isinf(n)) {
        if (!buf_push_str(buf, n > 0 ? "+inf" : "-inf", 4)) return false;
    } else if (n == (lua_Number)(lua_Integer)n) {
        lua_Integer i = (lua_Integer)n;
        char tmp[32];
        int len = snprintf(tmp, sizeof(tmp), "%lld", (long long)i);
        if (len < 0 || !buf_push_str(buf, tmp, len)) return false;
    } else {
        char tmp[64];
        int len = snprintf(tmp, sizeof(tmp), "%.17g", n); // high-precision float
        if (len < 0 || !buf_push_str(buf, tmp, len)) return false;
    }
    return true;
}

static inline bool buf_push_esc_multi(str_buf *dst, str_iter *src) {
    if (!buf_push_str(dst, "\"\"\"", 3)) return false;
    while (iter_peek(src).ok) {
        char c = iter_peek(src).v;
        if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            if (!buf_push_str(dst, "\r\n", 2)) return false;
        } else if (iter_starts_with(src, "\"\"\"", 3)) {
            iter_skip_n(src, 3);
            if (!buf_push_str(dst, "\"\"\\\"", 4)) return false;
        } else if (c == '"') {
            iter_skip(src);
            if (!buf_push(dst, '"')) return false;
        } else if (c == '\n') {
            iter_skip(src);
            if (!buf_push(dst, '\n')) return false;
        } else {
            iter_utf8_result res = iter_next_utf8(src);
            if (!res.ok) break;
            if (!buf_push_toml_escaped_char(dst, res.v, false)) return false;
        }
    }
    if (!buf_push_str(dst, "\"\"\"", 3)) return false;
    return true;
}

static bool buf_push_esc_simple(str_buf *dst, str_iter *src) {
    if (!buf_push(dst, '"')) return false;
    while (true) {
        iter_utf8_result res = iter_next_utf8(src);
        if (!res.ok) break;
        if (!buf_push_toml_escaped_char(dst, res.v, false)) return false;
    }
    if (!buf_push(dst, '"')) return false;
    return true;
}

static inline bool buf_push_esc_key(str_buf *buf, str_iter *iter) {
    bool is_safe = false;
    {
        iter_result curr = iter_next(iter);
        while (curr.ok) {
            if (is_identifier_char(curr.v)) {
                is_safe = true;
            } else {
                is_safe = false;
                break;
            }
            curr = iter_next(iter);
        }
    }
    if (is_safe) {
        if (!buf_push_str(buf, iter->buf, iter->len)) return false;
    } else {
        iter->pos = 0;
        if (!buf_push_esc_simple(buf, iter)) return false;
    }
    return true;
}

static inline int lbuf_push_keys(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    int top = lua_gettop(L);
    for (int i = 2; i <= top; i++) {
        str_iter src = lua_str_to_iter(L, i);
        if (src.buf == NULL || !buf_push_esc_key(buf, &src)) return luaL_error(L, "failed to push escaped key");
        if (i != top) {
            if (!buf_push(buf, '.')) return luaL_error(L, "failed to push escaped key");
        }
    }
    lua_settop(L, 1);
    return 1;
}

static inline int lbuf_push_heading(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    bool is_array = lua_toboolean(L, 2);
    int top = lua_gettop(L);
    if (is_array) {
        if (!buf_push_str(buf, "[[", 2)) return luaL_error(L, "failed to push to %s heading", (is_array) ? "array" : "table");
    } else {
        if (!buf_push(buf, '[')) return luaL_error(L, "failed to push to %s heading", (is_array) ? "array" : "table");
    }
    for (int i = 3; i <= top; i++) {
        str_iter src = lua_str_to_iter(L, i);
        if (src.buf == NULL || !buf_push_esc_key(buf, &src)) return luaL_error(L, "failed to push escaped key to %s heading", (is_array) ? "array" : "table");
        if (i != top) {
            if (!buf_push(buf, '.')) return luaL_error(L, "failed to push escaped key to %s heading", (is_array) ? "array" : "table");
        }
    }
    if (is_array) {
        if (!buf_push_str(buf, "]]\n", 3)) return luaL_error(L, "failed to push to %s heading", (is_array) ? "array" : "table");
    } else {
        if (!buf_push_str(buf, "]\n", 2)) return luaL_error(L, "failed to push to %s heading", (is_array) ? "array" : "table");
    }
    lua_settop(L, 1);
    return 1;
}

static int buf_push_inline_value(lua_State *L, str_buf *buf, int visited_idx, int level) {
    int val_idx = lua_gettop(L);
    int vtype = lua_type(L, val_idx);
    switch (vtype) {
        case LUA_TSTRING: {
            str_iter src = lua_str_to_iter(L, val_idx);
            if (src.buf == NULL || !buf_push_esc_simple(buf, &src)) return luaL_error(L, "failed to push escaped simple string");
        } break;
        case LUA_TNUMBER: {
            if (!lua_isnumber(L, val_idx)) return luaL_error(L, "expected number");
            lua_Number n = lua_tonumber(L, val_idx);
            if (!buf_push_num(buf, n)) return luaL_error(L, "failed to push number");
        } break;
        case LUA_TBOOLEAN: if (lua_toboolean(L, val_idx)) {
                buf_push_str(buf, "true", 4);
            } else {
                buf_push_str(buf, "false", 5);
            } break;
        case LUA_TTABLE: {
            // cycle detection
            lua_pushvalue(L, val_idx);
            lua_rawget(L, visited_idx);
            if (!lua_isnil(L, -1)) return luaL_error(L, "cyclic reference");
            lua_pop(L, 1);
            lua_pushvalue(L, val_idx);
            lua_pushboolean(L, true);
            lua_rawset(L, visited_idx);

            if (is_lua_array(L, val_idx)) {
                int len = lua_arraylen(L, val_idx);
                if (!buf_push(buf, '[')) return luaL_error(L, "failed to push array start");
                if (len > 0) {
                    int inlen = (level >= 0) ? (level + 1) * 2 + 1 : 1;
                    char indent[inlen];
                    indent[0] = (level >= 0) ? '\n' : ' ';
                    for (int i = 1; i < inlen; i++) indent[i] = ' ';
                    if (!buf_push_str(buf, indent, inlen)) return luaL_error(L, "failed to push indent to array");
                    for (int i = 1; i <= len; i++) {
                        lua_rawgeti(L, val_idx, i);
                        buf_push_inline_value(L, buf, visited_idx, (level >= 0) ? level + 1 : -1);
                        if (i != len) {
                            if (!buf_push(buf, ',')) return luaL_error(L, "failed to push array separator");
                            if (!buf_push_str(buf, indent, inlen)) return luaL_error(L, "failed to push indent to array");
                        }
                    }
                    inlen = (level >= 0) ? level * 2 + 1 : 1;
                    if (!buf_push_str(buf, indent, inlen)) return luaL_error(L, "failed to push indent to array");
                }
                if (!buf_push(buf, ']')) return luaL_error(L, "failed to push array end");
            } else {
                if (!buf_push(buf, '{')) return luaL_error(L, "failed to push table start");
                lua_pushnil(L); // for lua_next
                bool first = true;
                while (lua_next(L, val_idx) != 0) {
                    // stack: key value
                    if (!first) {
                        if (!buf_push(buf, ',')) return luaL_error(L, "failed to push table separator");
                    }
                    first = false;
                    if (!buf_push(buf, ' ')) return luaL_error(L, "failed to push table space before key");
                    str_iter src = lua_str_to_iter(L, -2);
                    if (src.buf == NULL || !buf_push_esc_key(buf, &src)) return luaL_error(L, "failed to push table key");
                    if (!buf_push_str(buf, " = ", 3)) return luaL_error(L, "failed to push table equals");
                    // pop and push value to buffer (-1 because no newlines allowed)
                    buf_push_inline_value(L, buf, visited_idx, -1);
                }
                if (!first) {
                    if (!buf_push(buf, ' ')) return luaL_error(L, "failed to push table trailing space");
                }
                if (!buf_push(buf, '}')) return luaL_error(L, "failed to push table end");
            }
        } break;
        case LUA_TUSERDATA:
            if(udata_is_of_type(L, val_idx, "TomluaDate")) {
                if (!buf_push_toml_date(buf, (TomlDate *)lua_touserdata(L, val_idx)))
                    return luaL_error(L, "failed to push date");
            } else if (udata_is_of_type(L, val_idx, "TomluaStrBuf")) {
                str_buf * arg = (str_buf *)lua_touserdata(L, val_idx);
                if (!buf_push_str(buf, arg->data, arg->len)) return luaL_error(L, "failed to push string");
            } break;
        default: return luaL_error(L, "%s is not a valid type for push_inline_value", lua_typename(L, vtype));
    }
    lua_settop(L, val_idx - 1);
    return 1;
}

static inline int lbuf_push_inline_value(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    lua_Integer level = ((lua_isnumber(L, 4)) ? lua_tonumber(L, 4) : 0);
    lua_settop(L, 3);
    if (!buf_push_inline_value(L, buf, 2, level)) return luaL_error(L, "failed to push inline value");
    lua_settop(L, 1);
    return 1;
}

static inline int lbuf_index(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, lbuf_push_str);
    lua_setfield(L, -2, "push");
    lua_pushcfunction(L, lbuf_push_inline_value);
    lua_setfield(L, -2, "push_inline_value");
    lua_pushcfunction(L, lbuf_push_heading);
    lua_setfield(L, -2, "push_heading");
    lua_pushcfunction(L, lbuf_push_keys);
    lua_setfield(L, -2, "push_keys");
    return 1;
}

static inline int lbuf_gc(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->cap = buf->len = 0;
    return 0;
}

static inline int lbuf_tostring(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "TomluaStrBuf");
    if (!buf->data) lua_pushliteral(L, "");
    else lua_pushlstring(L, buf->data, buf->len);
    return 1;
}

static inline int lbuf_new(lua_State *L) {
    str_buf *buf = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
    *buf = (str_buf){0};
    if (luaL_newmetatable(L, "TomluaStrBuf")) {
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

static inline int lis_lua_heading_array(lua_State *L) {
    int type = is_lua_heading_array(L, 1);
    if (type == 2) {
        lua_pushboolean(L, true);
        lua_pushboolean(L, true);
    } else if (type == 1) {
        lua_pushboolean(L, false);
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
        lua_pushboolean(L, false);
    }
    return 2;
}

void push_encode(lua_State *L) {
    push_embedded_encode(L);
    lua_pushcfunction(L, lis_lua_heading_array);
    lua_pushcfunction(L, lbuf_new);
    lua_call(L, 2, 1);
}
