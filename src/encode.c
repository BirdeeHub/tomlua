#include <lua.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "types.h"
#include "encode.h"

#define STR(x) #x
#define XSTR(x) STR(x)
#ifndef EMBEDDED_LUA
#define EMBEDDED_LUA ../tmp/embedded.h
#endif
#include XSTR(EMBEDDED_LUA)

// getmetatable(idx).toml_type to allow overriding of representation
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
    return TOML_UNTYPED;
}

static inline bool is_lua_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
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
            if (key < 1 || key != (lua_Number)(lua_Integer)(key)) {
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
    } else if (type == LUA_TUSERDATA) {
        if (lua_getmetatable(L, idx)) {
            luaL_getmetatable(L, "LStrBuf");
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return 2;
            }
            lua_pop(L, 2);
        }
    }
    return 0;
}

static inline int lbuf_push_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
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
static inline int lbuf_push_num(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    if (!lua_isnumber(L, 2)) return luaL_error(L, "expected number");
    lua_Number n = lua_tonumber(L, 2);
    if (isnan(n)) {
        if (!buf_push_str(buf, "nan", 3))
            return luaL_error(L, "failed to push NaN");
    } else if (isinf(n)) {
        if (!buf_push_str(buf, n > 0 ? "+inf" : "-inf", 4))
            return luaL_error(L, "failed to push infinity");
    } else if (n == (lua_Number)(lua_Integer)n) {
        lua_Integer i = (lua_Integer)n;
        char tmp[32];
        int len = snprintf(tmp, sizeof(tmp), "%lld", (long long)i);
        if (len < 0 || !buf_push_str(buf, tmp, len))
            return luaL_error(L, "failed to push integer");
    } else {
        char tmp[64];
        int len = snprintf(tmp, sizeof(tmp), "%.17g", n); // high-precision float
        if (len < 0 || !buf_push_str(buf, tmp, len))
            return luaL_error(L, "failed to push float");
    }
    lua_settop(L, 1);
    return 1;
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
            if (!buf_push_toml_escaped_char(dst, iter_next(src).v, false)) return false;
        }
    }
    if (!buf_push_str(dst, "\"\"\"", 3)) return false;
    return true;
}

static inline int lbuf_push_multi_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    str_iter src = lua_str_to_iter(L, 2);
    if (!buf_push_esc_multi(buf, &src)) return luaL_error(L, "failed to push escaped simple string");
    lua_settop(L, 1);
    return 1;
}

static inline bool buf_push_esc_simple(str_buf *dst, str_iter *src) {
    if (!buf_push(dst, '"')) return false;
    while (iter_peek(src).ok) {
        if (!buf_push_toml_escaped_char(dst, iter_next(src).v, false)) return false;
    }
    if (!buf_push(dst, '"')) return false;
    return true;
}

static inline int lbuf_push_simple_str(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    str_iter src = lua_str_to_iter(L, 2);
    if (!buf_push_esc_simple(buf, &src)) return luaL_error(L, "failed to push escaped simple string");
    lua_settop(L, 1);
    return 1;
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
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    int top = lua_gettop(L);
    for (int i = 2; i <= top; i++) {
        str_iter src = lua_str_to_iter(L, i);
        if (!buf_push_esc_key(buf, &src)) return luaL_error(L, "failed to push escaped key");
        if (i != top) {
            if (!buf_push(buf, '.')) return luaL_error(L, "failed to push escaped key");
        }
    }
    lua_settop(L, 1);
    return 1;
}

typedef struct {
    size_t len;
    const char * str;
} lstr_tmp;

static inline int lbuf_push_sep(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    int top = lua_gettop(L);
    if (!lua_isstring(L, 2)) return luaL_error(L, "expected string");
    if (top - 2 <= 0) return luaL_error(L, "no strings to join"); // nothing to push
    lstr_tmp sep = {0};
    sep.str = lua_tolstring(L, 2, &sep.len);
    int tmp_len = 2 * (top - 2) - 1;
    int pos = 0;
    size_t total_len = 0;
    lstr_tmp tmp_res[tmp_len];
    for (int i = 3; i <= top; i++) {
        lstr_tmp v = {0};
        switch (is_str_or_buf(L, i)) {
            case 1: v.str = lua_tolstring(L, i, &v.len); break;
            case 2: {
                str_buf * arg = (str_buf *)lua_touserdata(L, i);
                v.len = arg->len;
                v.str = arg->data;
            } break;
            default: return luaL_error(L, "expected string or string buffer");
        }
        total_len += v.len;
        tmp_res[pos++] = v;
        if (i != top) {
            total_len += sep.len;
            tmp_res[pos++] = sep;
        }
    }
    size_t new_capacity = buf->cap > 0 ? buf->cap : 1;
    while (new_capacity < buf->len + total_len) new_capacity *= 2;
    char *tmp = (char *)realloc(buf->data, new_capacity * sizeof(char));
    if (!tmp) return false;
    buf->data = tmp;
    buf->cap = new_capacity;
    for (int i = 0; i < tmp_len; i++) {
        memcpy(buf->data + buf->len, tmp_res[i].str, tmp_res[i].len);
        buf->len += tmp_res[i].len;
    }
    lua_settop(L, 1);
    return 1;
}

static inline int lbuf_reset(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    buf_soft_reset(buf);
    lua_settop(L, 1);
    return 1;
}

static inline int lbuf_index(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, lbuf_push_str);
    lua_setfield(L, -2, "push");
    lua_pushcfunction(L, lbuf_push_sep);
    lua_setfield(L, -2, "push_sep");
    lua_pushcfunction(L, lbuf_push_simple_str);
    lua_setfield(L, -2, "push_str");
    lua_pushcfunction(L, lbuf_push_multi_str);
    lua_setfield(L, -2, "push_multi_str");
    lua_pushcfunction(L, lbuf_push_num);
    lua_setfield(L, -2, "push_num");
    lua_pushcfunction(L, lbuf_push_keys);
    lua_setfield(L, -2, "push_keys");
    lua_pushcfunction(L, lbuf_reset);
    lua_setfield(L, -2, "reset");
    return 1;
}

static inline int lbuf_gc(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->cap = buf->len = 0;
    return 0;
}

static inline int lbuf_tostring(lua_State *L) {
    str_buf *buf = (str_buf *)luaL_checkudata(L, 1, "LStrBuf");
    if (!buf->data) lua_pushliteral(L, "");
    else lua_pushlstring(L, buf->data, buf->len);
    return 1;
}

static inline int lbuf_new(lua_State *L) {
    str_buf *buf = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
    *buf = (str_buf){0};
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

static inline int lis_lua_array(lua_State *L) {
    lua_pushboolean(L, is_lua_array(L, 1));
    return 1;
}

void push_encode(lua_State *L, int opts_idx, int types_idx) {
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
