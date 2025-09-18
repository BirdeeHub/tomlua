#include <lua.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "dates.h"
#include "types.h"
#include "encode.h"
#include "error_context.h"

#define ENCODE_VISITED_IDX 2

typedef struct {
    size_t len;
    char * str;
} KeyEntry;

typedef struct {
    int len;
    int cap;
    KeyEntry *keys;
} Keys;

static inline Keys new_keys() {
    Keys k = {
        .len = 0,
        .cap = 2,
        .keys = malloc(sizeof(KeyEntry) * k.cap),
    };
    if (k.keys == NULL) k.cap = 0;
    return k;
}

static inline bool push_lua_key(lua_State *L, Keys *keys, int idx) {
    size_t klen = 0;
    const char *str = lua_tolstring(L, idx, &klen);
    if (str == NULL) return false;
    KeyEntry k = {
        .str = malloc(klen + 1),
        .len = klen,
    };
    memcpy(k.str, str, klen);
    k.str[klen] = '\0';
    size_t required_len = keys->len + 1;
    if (required_len > keys->cap) {
        size_t new_capacity = keys->cap > 0 ? keys->cap : 1;
        while (new_capacity < required_len) new_capacity *= 2;
        KeyEntry *tmp = (KeyEntry *)realloc(keys->keys, new_capacity * sizeof(KeyEntry));
        if (!tmp) return false;
        keys->keys = tmp;
        keys->cap = new_capacity;
    }
    keys->keys[keys->len++] = k;
    return true;
}

static inline void free_keys(Keys *k) {
    if (k && k->keys) {
        for (int i = 0; i < k->len; i++) {
            free(k->keys[i].str);
        }
        free(k->keys);
        k->keys = NULL;
        k->len = 0;
        k->cap = 0;
    }
}

static inline void pop_key(Keys *k) {
    free(k->keys[k->len - 1].str);
    k->len--;
}

// works for char or uint32_t
static inline bool buf_push_toml_escaped_char(str_buf *buf, const uint32_t c, const bool esc_non_ascii) {
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

// 0 for not an array, 1 for array, 2 for heading array
// does not verify that the value is a table for optimization reasons
static inline int is_lua_heading_array(lua_State *L, int idx) {
    int old_top = lua_gettop(L);
    idx = absindex(old_top, idx);
    if (lua_arraylen(L, idx) == 0) {
        if (get_meta_toml_type(L, idx) == TOML_ARRAY) return 1;
        else return 0;
    }
    bool is_heading = true;
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, idx) != 0) {
        // now at stack: key value
        if (!lua_istable(L, -1)) is_heading = false;
        lua_pop(L, 1);  // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != (lua_Number)(lua_Integer)(key)) {
                lua_settop(L, old_top);
                return 0;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_settop(L, old_top);
            return 0;
        }
    }
    lua_settop(L, old_top);
    if (highest_int_key != count || count == 0) return 0;
    return (is_heading) ? 2 : 1;
}

static bool buf_push_esc_multi(str_buf *dst, str_iter *src) {
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
        iter_reset_pos(iter);
        if (!buf_push_esc_simple(buf, iter)) return false;
    }
    return true;
}

static bool buf_push_keys(str_buf *buf, const Keys *keys) {
    for (int i = 0; i < keys->len; i++) {
        KeyEntry k = keys->keys[i];
        str_iter src = {
            .buf = k.str,
            .pos = 0,
            .len = k.len,
        };
        if (!buf_push_esc_key(buf, &src)) return false;
        if (i != keys->len - 1) {
            if (!buf_push(buf, '.')) return false;
        }
    }
    return true;
}

static inline bool buf_push_heading(lua_State *L, str_buf *buf, const Keys *keys, const bool is_array) {
    if (is_array) {
        if (!buf_push_str(buf, "[[", 2)) {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            set_tmlerr(err, false, 18, "failed to push to ");
            tmlerr_push_str(err, (is_array) ? "array" : "table", 5);
            tmlerr_push_str(err, " heading", 8);
            return false;
        }
    } else {
        if (!buf_push(buf, '[')) {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            set_tmlerr(err, false, 18, "failed to push to ");
            tmlerr_push_str(err, (is_array) ? "array" : "table", 5);
            tmlerr_push_str(err, " heading", 8);
            return false;
        }
    }
    if (!buf_push_keys(buf, keys)) {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            set_tmlerr(err, false, 18, "failed to push to ");
            tmlerr_push_str(err, (is_array) ? "array" : "table", 5);
            tmlerr_push_str(err, " heading", 8);
            return false;
        }
    if (is_array) {
        if (!buf_push_str(buf, "]]\n", 3)) {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            set_tmlerr(err, false, 18, "failed to push to ");
            tmlerr_push_str(err, (is_array) ? "array" : "table", 5);
            tmlerr_push_str(err, " heading", 8);
            return false;
        }
    } else {
        if (!buf_push_str(buf, "]\n", 2)) {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            set_tmlerr(err, false, 18, "failed to push to ");
            tmlerr_push_str(err, (is_array) ? "array" : "table", 5);
            tmlerr_push_str(err, " heading", 8);
            return false;
        }
    }
    return true;
}

static bool buf_push_inline_value(lua_State *L, str_buf *buf, int visited_idx, int level) {
    int val_idx = lua_gettop(L);
    int vtype = lua_type(L, val_idx);
    switch (vtype) {
        case LUA_TSTRING: {
            str_iter src = lua_str_to_iter(L, val_idx);
            if (src.buf == NULL || !buf_push_esc_simple(buf, &src)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 36, "failed to push escaped simple string");
        } break;
        case LUA_TNUMBER: {
            size_t len;
            const char *str = lua_tolstring(L, val_idx, &len);
            if (!buf_push_str(buf, str, len)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 21, "failed to push number");
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
            if (!lua_isnil(L, -1)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 27, "Circular reference in table");
            lua_pop(L, 1);
            lua_pushvalue(L, val_idx);
            lua_pushboolean(L, true);
            lua_rawset(L, visited_idx);

            if (is_lua_array(L, val_idx)) {
                int len = lua_arraylen(L, val_idx);
                if (!buf_push(buf, '[')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 26, "failed to push array start");
                if (len > 0) {
                    int inlen = (level >= 0) ? (level + 1) * 2 + 1 : 1;
                    char indent[inlen];
                    indent[0] = (level >= 0) ? '\n' : ' ';
                    for (int i = 1; i < inlen; i++) indent[i] = ' ';
                    if (!buf_push_str(buf, indent, inlen)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 30, "failed to push indent to array");
                    for (int i = 1; i <= len; i++) {
                        lua_rawgeti(L, val_idx, i);
                        if(!buf_push_inline_value(L, buf, visited_idx, (level >= 0) ? level + 1 : -1)) return false;
                        if (i != len) {
                            if (!buf_push(buf, ',')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 30, "failed to push array separator");
                            if (!buf_push_str(buf, indent, inlen)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 30, "failed to push indent to array");
                        }
                    }
                    inlen = (level >= 0) ? level * 2 + 1 : 1;
                    if (!buf_push_str(buf, indent, inlen)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 30, "failed to push indent to array");
                }
                if (!buf_push(buf, ']')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 24, "failed to push array end");
            } else {
                if (!buf_push(buf, '{')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 26, "failed to push table start");
                lua_pushnil(L); // for lua_next
                bool first = true;
                while (lua_next(L, val_idx) != 0) {
                    // stack: key value
                    if (!first) {
                        if (!buf_push(buf, ',')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 30, "failed to push table separator");
                    }
                    first = false;
                    if (!buf_push(buf, ' ')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 37, "failed to push table space before key");
                    str_iter src = lua_str_to_iter(L, -2);
                    if (src.buf == NULL || !buf_push_esc_key(buf, &src)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 24, "failed to push table key");
                    if (!buf_push_str(buf, " = ", 3)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 27, "failed to push table equals");
                    // pop and push value to buffer (-1 because no newlines allowed)
                    if (!buf_push_inline_value(L, buf, visited_idx, -1)) return false;
                }
                if (!first) {
                    if (!buf_push(buf, ' ')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 35, "failed to push table trailing space");
                }
                if (!buf_push(buf, '}')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 24, "failed to push table end");
            }
            lua_settop(L, val_idx);
            lua_pushnil(L);
            lua_rawset(L, visited_idx);
        } break;
        case LUA_TUSERDATA:
            if (udata_is_of_type(L, val_idx, "TomluaDate")) {
                if (!buf_push_toml_date(buf, (TomlDate *)lua_touserdata(L, val_idx)))
                    return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 19, "failed to push date");
                break;
            } else if (udata_is_of_type(L, val_idx, "TomluaMultiStr")) {
                str_buf * arg = (str_buf *)lua_touserdata(L, val_idx);
                str_iter argiter = {
                    .buf = arg->data,
                    .len = arg->len,
                    .pos = 0
                };
                if (!buf_push_esc_multi(buf, &argiter)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 21, "failed to push string");
                break;
            }
        default: {
            TMLErr *err = new_tmlerr(L, ENCODE_VISITED_IDX);
            const char *tname = lua_typename(L, vtype);
            // NOTE: This cast is ok because if I say it is static, it copies it before modifying
            set_tmlerr(err, false, strlen(tname), (char *)tname);
            tmlerr_push_str(err, " is not a valid type for push_inline_value", 42);
            return false;
        }
    }
    lua_settop(L, val_idx - 1);
    return true;
}

//---@class Tomlua.Deferred_Heading
//---@field is_array boolean
//---@field key string
//---@field value any
// leaves stack how it found it, writes entries to residx list
static inline bool buf_push_heading_table(lua_State *L, str_buf *buf, const int validx, const int visited_idx, Keys *keys) {
    lua_newtable(L);
    int residx = lua_gettop(L);
    int result_len = 0;
    if (keys->len != 0) buf_push_heading(L, buf, keys, false);
    lua_pushnil(L);  // next(nil) // get first kv pair on stack
    while (lua_next(L, validx) != 0) {
        int vidx = lua_gettop(L);
        int key_idx = vidx - 1;
        if (lua_istable(L, vidx)) {
            int table_type = is_lua_heading_array(L, vidx);
            if (table_type == 2 || table_type == 0) {
                // create Deferred_Heading
                lua_newtable(L);
                lua_pushboolean(L, table_type);
                lua_setfield(L, -2, "is_array");
                lua_pushvalue(L, key_idx);
                lua_setfield(L, -2, "key");
                lua_pushvalue(L, vidx);
                lua_setfield(L, -2, "value");
                lua_rawseti(L, residx, ++result_len);
            } else if (table_type == 1) {
                str_iter lstr = lua_str_to_iter(L, key_idx);
                if (!lstr.buf) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 34, "invalid key in table heading entry");
                if (!buf_push_esc_key(buf, &lstr)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 32, "failed to push table heading key");
                if (!buf_push_str(buf, " = ", 3)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 44, "failed to push equals in table heading entry");
                if (!buf_push_inline_value(L, buf, visited_idx, 0)) return false;
                if (!buf_push(buf, '\n')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 48, "failed to push newline after table heading entry");
            }
        } else {
            str_iter lstr = lua_str_to_iter(L, key_idx);
            if (!lstr.buf) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 34, "invalid key in table heading entry");
            if (!buf_push_esc_key(buf, &lstr)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 32, "failed to push table heading key");
            if (!buf_push_str(buf, " = ", 3)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 44, "failed to push equals in table heading entry");
            if (!buf_push_inline_value(L, buf, visited_idx, 0)) return false;
            if (!buf_push(buf, '\n')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 48, "failed to push newline after table heading entry");
        }
        lua_settop(L, key_idx);
    }
    lua_settop(L, residx);
    return true;
}

static bool flush_q(lua_State *L, str_buf *buf, int visited_idx, Keys *keys) {
    int input_idx = lua_gettop(L);
    size_t inlen = lua_arraylen(L, input_idx);
    for (size_t i = 1; i <= inlen; i++) {
        lua_rawgeti(L, input_idx, i);
        int deferred = lua_gettop(L);
        lua_getfield(L, deferred, "is_array");
        bool is_heading_array = lua_toboolean(L, -1);
        lua_settop(L, deferred);
        lua_getfield(L, deferred, "key");
        // NOTE: because you pop these, you have to copy them, or they may get GC'd
        if (!push_lua_key(L, keys, -1)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 20, "failed to store key!");
        lua_settop(L, deferred);
        lua_getfield(L, deferred, "value");
        lua_replace(L, deferred);
        if (is_heading_array) {
            size_t array_len = lua_arraylen(L, deferred);
            if (!buf_push(buf, '\n')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 54, "failed to push newline before processing array heading");
            for (size_t i = 1; i <= array_len; i++) {
                buf_push_heading(L, buf, keys, true);
                lua_rawgeti(L, deferred, i);
                int tidx = lua_gettop(L);
                lua_pushnil(L);
                while (lua_next(L, tidx) != 0) {
                    lua_pushvalue(L, -2);
                    str_iter lstr = lua_str_to_iter(L, -1);
                    if (!lstr.buf) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 34, "invalid key in array heading entry");
                    if (!buf_push_esc_key(buf, &lstr)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 32, "failed to push array heading key");
                    lua_pop(L, 1);
                    if (!buf_push_str(buf, " = ", 3)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 44, "failed to push equals in array heading entry");
                    // pops value, leaves key for next lua_next
                    if (!buf_push_inline_value(L, buf, visited_idx, 0)) return false;
                    if (!buf_push(buf, '\n')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 48, "failed to push newline after array heading entry");
                }
            }
            lua_settop(L, deferred - 1);
        } else {
            if (!buf_push(buf, '\n')) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 54, "failed to push newline before processing table heading");
            // cycle detection
            lua_pushvalue(L, deferred);
            lua_rawget(L, visited_idx);
            if (!lua_isnil(L, -1)) return set_tmlerr(new_tmlerr(L, ENCODE_VISITED_IDX), false, 27, "Circular reference in table");
            lua_pop(L, 1);
            lua_pushvalue(L, deferred);
            lua_pushboolean(L, true);
            lua_rawset(L, visited_idx);

            if(!buf_push_heading_table(L, buf, deferred, visited_idx, keys)) return false;
            if(!flush_q(L, buf, ENCODE_VISITED_IDX, keys)) return false;

            lua_settop(L, deferred);
            lua_pushnil(L);
            lua_rawset(L, visited_idx);
        }
        // frees and removes last key
        pop_key(keys);
    }
    lua_settop(L, input_idx - 1);
    return true;
}

int encode(lua_State *L) {
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "Argument must be a table");
        return 2;
    }
    lua_settop(L, 1);
    str_buf buf = new_str_buf();
    if (buf.data == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "Unable to allocate memory for output buffer");
        return 2;
    }
    Keys keys = new_keys();
    if (keys.keys == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "Unable to allocate memory for keys struct");
        return 2;
    }
    // NOTE: ENCODE_VISITED_IDX; // 2
    // This will also be where our error ends up if we get one.
    lua_newtable(L);
    if(!buf_push_heading_table(L, &buf, 1, ENCODE_VISITED_IDX, &keys)) goto fail;
    if(!flush_q(L, &buf, ENCODE_VISITED_IDX, &keys)) goto fail;
    free_keys(&keys);
    lua_settop(L, 0);
    push_buf_to_lua_string(L, &buf);
    free_str_buf(&buf);
    return 1;
fail:
    free_str_buf(&buf);
    free_keys(&keys);
    lua_settop(L, ENCODE_VISITED_IDX);
    lua_pushnil(L);
    push_tmlerr_string(L, get_err_val(L, ENCODE_VISITED_IDX));
    return 2;
}
