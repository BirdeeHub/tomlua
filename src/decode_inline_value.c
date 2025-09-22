// Copyright 2025 Birdee
#include <lua.h>
#include <limits.h>
#include <float.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#include "types.h"
#include "opts.h"
#include "dates.h"
#include "decode_keys.h"
#include "error_context.h"
#include "decode_str.h"
#include "decode_inline_value.h"

// pops value and keys, leaves root on top
static inline bool set_kv(lua_State *L, int keys_len, int value_idx, int erridx) {
    if (keys_len <= 0) return set_tmlerr(new_tmlerr(L, erridx), false, 22, "no key provided to set");
    int keys_start = value_idx + 1;
    int root_idx = value_idx - 1;
    lua_pushvalue(L, root_idx);     // copy root table to top

    // Navigate through all keys except the last
    for (int key_idx = keys_start; key_idx < keys_start + keys_len - 1; key_idx++) {
        int parent_idx = lua_gettop(L);
        lua_pushvalue(L, key_idx);
        lua_rawget(L, parent_idx);
        int vtype = lua_type(L, -1);
        if (vtype == LUA_TNIL) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);  // copy so we can continue with it after rawset
            lua_rawset(L, parent_idx);   // t[key] = new table
        } else if (vtype != LUA_TTABLE) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 18, "key is not a table");
        }
        lua_remove(L, parent_idx);
    }

    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        return set_tmlerr(new_tmlerr(L, erridx), false, 18, "key already defined!");
    }
    lua_pop(L, 1);

    lua_pushvalue(L, -2);         // push last key
    lua_pushvalue(L, value_idx);  // push value
    lua_rawset(L, -3);            // parent[last_key] = value

    lua_settop(L, root_idx);
    return true;
}

// adds a table to the lua stack and return NULL or error
static inline bool parse_inline_table(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts, int erridx) {
    lua_newtable(L);
    if (opts[TOMLOPTS_MARK_INLINE]) {
        lua_newtable(L);
        lua_pushliteral(L, "TABLE_INLINE");
        lua_setfield(L, -2, "toml_type");
        lua_setmetatable(L, -2);
    }
    bool last_was_comma = false;
    const bool int_keys = opts[TOMLOPTS_INT_KEYS];
    const bool fancy_tables = opts[TOMLOPTS_FANCY_TABLES];
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '}') {
            iter_skip(src);
            if (last_was_comma && !fancy_tables) {
                lua_pop(L, 1);
                return set_tmlerr(new_tmlerr(L, erridx), false, 42, "trailing comma in inline table not allowed");
            }
            return true;
        } else if (iter_peek(src).v == '\n') {
            iter_skip(src);
            if (!fancy_tables) return set_tmlerr(new_tmlerr(L, erridx), false, 35, "inline tables can not be multi-line");
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            if (!fancy_tables) return set_tmlerr(new_tmlerr(L, erridx), false, 35, "inline tables can not be multi-line");
        } else if (d == ',') {
            iter_skip(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return set_tmlerr(new_tmlerr(L, erridx), false, 18, "2 commas in a row!");
            }
            last_was_comma = true;
            continue;
        } else if (d == ' ' || d == '\t') {
            iter_skip(src);
            continue;
        }
        last_was_comma = false;
        lua_pushnil(L);
        int val_spacer = lua_gettop(L);
        int keys_len = parse_keys(L, src, buf, int_keys, erridx);
        if (!keys_len) return false;
        if (iter_peek(src).ok && iter_peek(src).v != '=') {
            return set_tmlerr(new_tmlerr(L, erridx), false, 35, "keys for assignment must end with =");
        }
        iter_skip(src);
        if (consume_whitespace_to_line(src)) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 76, "the value in key = value expressions must begin on the same line as the key!");
        }
        if (!decode_inline_value(L, src, buf, opts, erridx)) return false;
        lua_replace(L, val_spacer);
        if (!set_kv(L, keys_len, val_spacer, erridx)) return false;
        if (fancy_tables) {
            while (consume_whitespace_to_line(src)) {}
        } else if (consume_whitespace_to_line(src)) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 39, "toml inline tables cannot be multi-line");
        }
        iter_result next = iter_peek(src);
        if (next.ok && (next.v != ',' && next.v != '}')) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 65, "toml inline table values must be separated with , or ended with }");
        }
    }
    return set_tmlerr(new_tmlerr(L, erridx), false, 17, "missing closing }");
}

static bool push_integer_or_handle(lua_State *L, str_buf *s, int base, bool throw_on_overflow) {
    errno = 0;
    buf_null_terminate(s);
    int64_t val = strtoll(s->data, NULL, base);
    // Check if conversion failed or overflowed
    if (errno == ERANGE || val > LUA_MAXINTEGER || val < LUA_MININTEGER) {
        if (throw_on_overflow) {
            return false;
        } else {
            // push +/- inf depending on sign
            lua_pushnumber(L, (val < 0) ? -INFINITY : INFINITY);
            return true;
        }
    }
    lua_pushinteger(L, (lua_Integer)val);
    return true;
}

static inline bool push_float_or_handle(lua_State *L, str_buf *s, bool throw_on_overflow, bool throw_on_underflow) {
    errno = 0;
    buf_null_terminate(s);
    lua_Number val = strtod(s->data, NULL);
    if (errno == ERANGE) {
        if (throw_on_overflow && !isfinite(val)) return false;
        if (throw_on_underflow && val != 0.0 && fabs(val) < DBL_MIN) return false;
    }
    lua_pushnumber(L, val);
    return true;
}

// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
bool decode_inline_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts, int erridx) {
    iter_result curr = iter_peek(src);
    if (!curr.ok) return set_tmlerr(new_tmlerr(L, erridx), false, 34, "expected value, got end of content");
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
        if (!parse_multi_basic_string(L, buf, src, erridx)) {
            return false;
        }
        if (opts[TOMLOPTS_MULTI_STRINGS]) {
            str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
            if (!s || !buf || !buf->data) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 59, "tomlua.decode failed to push multi-line string to lua stack");
            }
            *s = new_buf_from_str(buf->data, buf->len);
            push_multi_string_mt(L);
            lua_setmetatable(L, -2);
        } else {
            if (!push_buf_to_lua_string(L, buf)) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 48, "tomlua.decode failed to push string to lua stack");
            }
        }
        return true;
    } else if (curr.v == '"') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_basic_string(L, buf, src, erridx)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 48, "tomlua.decode failed to push string to lua stack");
        }
        return true;
    } else if (iter_starts_with(src, "'''", 3)) {
        buf_soft_reset(buf);
        iter_skip_n(src, 3);
        if (!parse_multi_literal_string(L, buf, src, erridx)) {
            return false;
        }
        if (opts[TOMLOPTS_MULTI_STRINGS]) {
            str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
            if (!s || !buf || !buf->data) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 59, "tomlua.decode failed to push multi-line string to lua stack");
            }
            *s = new_buf_from_str(buf->data, buf->len);
            push_multi_string_mt(L);
            lua_setmetatable(L, -2);
        } else {
            if (!push_buf_to_lua_string(L, buf)) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 48, "tomlua.decode failed to push string to lua stack");
            }
        }
        return true;
    } else if (curr.v == '\'') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_literal_string(L, buf, src, erridx)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_tmlerr(new_tmlerr(L, erridx), false, 48, "tomlua.decode failed to push string to lua stack");
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
        if (iter_starts_with(src, "0x", 2)) {
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
                        return set_tmlerr(new_tmlerr(L, erridx), false, 51, "consecutive underscores not allowed in hex literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 53, "hex literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, erridx), false, 17, "empty hex literal");
            // Convert buffer to integer
            if (!push_integer_or_handle(L, buf, 16, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, erridx), false, 41, "Parse error: hex literal integer overflow");
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
                        return set_tmlerr(new_tmlerr(L, erridx), false, 53, "consecutive underscores not allowed in octal literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 55, "octal literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, erridx), false, 19, "empty octal literal");
            if (!push_integer_or_handle(L, buf, 8, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, erridx), false, 43, "Parse error: octal literal integer overflow");
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
                        return set_tmlerr(new_tmlerr(L, erridx), false, 54, "consecutive underscores not allowed in binary literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, erridx), false, 56, "binary literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, erridx), false, 20, "empty binary literal");
            if (!push_integer_or_handle(L, buf, 2, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, erridx), false, 44, "Parse error: binary literal integer overflow");
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
            if (curr.v == '+') {
                if (iter_starts_with(src, "+inf", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, INFINITY);
                    return true;
                } else if (iter_starts_with(src, "+nan", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, NAN);
                    return true;
                } else {
                    buf_push(buf, curr.v);
                    iter_skip(src);
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
                } else {
                    buf_push(buf, curr.v);
                    iter_skip(src);
                }
            }
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if (ch == '_') {
                    iter_skip(src);
                    if (was_underscore) {
                        return set_tmlerr(new_tmlerr(L, erridx), false, 46, "consecutive underscores not allowed in numbers");
                    }
                    was_underscore = true;
                } else if (ch == 'e' || ch == 'E') {
                    is_float = true;
                    buf_push(buf, ch);
                    iter_skip(src);
                    iter_result next = iter_peek(src);
                    if (next.ok && (next.v == '+' || next.v == '-')) {
                        buf_push(buf, next.v);
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
                return set_tmlerr(new_tmlerr(L, erridx), false, 56, "number literals not allowed to have trailing underscores");
            }
            if (buf->len > 0) {
                if (is_date) {
                    if (opts[TOMLOPTS_FANCY_DATES]) {
                        str_iter date_src = (str_iter) {
                            .len = buf->len,
                            .pos = 0,
                            .buf = buf->data
                        };
                        TomlDate date;
                        if (!parse_toml_date(&date_src, date))
                            return set_tmlerr(new_tmlerr(L, erridx), false, 29, "Invalid date format provided!");
                        if (!push_new_toml_date(L, date))
                            return set_tmlerr(new_tmlerr(L, erridx), false, 51, "tomlua.decode failed to push date type to lua stack");
                    } else if (!push_buf_to_lua_string(L, buf)) {
                        return set_tmlerr(new_tmlerr(L, erridx), false, 53, "tomlua.decode failed to push date string to lua stack");
                    }
                } else if (is_float) {
                    if (!push_float_or_handle(L, buf, opts[TOMLOPTS_OVERFLOW_ERRORS], opts[TOMLOPTS_UNDERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, erridx), false, 35, "Parse error: float literal overflow");
                } else {
                    if (!push_integer_or_handle(L, buf, 10, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, erridx), false, 37, "Parse error: integer literal overflow");
                }
                return true;
            }
        }
    // --- array --- allows trailing comma and multiline
    } else if (curr.v == '[') {
        iter_skip(src);
        lua_newtable(L);
        if (opts[TOMLOPTS_MARK_INLINE]) {
            lua_newtable(L);
            lua_pushliteral(L, "ARRAY_INLINE");
            lua_setfield(L, -2, "toml_type");
            lua_setmetatable(L, -2);
        }
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
            if (!decode_inline_value(L, src, buf, opts, erridx)) return false;
            lua_rawseti(L, -2, idx++);
        }
        return set_tmlerr(new_tmlerr(L, erridx), false, 17, "missing closing ]");
    // --- inline table --- does NOT support multiline or trailing comma (without fancy_tables)
    } else if (curr.v == '{') {
        iter_skip(src);
        return parse_inline_table(L, src, buf, opts, erridx);
    }
    return set_tmlerr(new_tmlerr(L, erridx), false, 13, "invalid value");
}
