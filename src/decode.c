// Copyright 2025 Birdee
#include <stddef.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
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

#define DECODE_RESULT_IDX 2
// @type { [table]: table<key, bool?> | len if array or -1 for inline tables or -2 for inline arrays }
// or error userdata type on error
// Yes, I know, this is very overloaded.
// It is the best structure I thought of to achieve the desired behavior and performance together.
#define DECODE_DEFINED_IDX 3

static const int DEFINED_MARK;

// NOTE: Assumes root is less than keys_start, does not pop root
static bool recursive_lua_nav(
    lua_State *L,
    int keys_start,
    int root_idx,
    bool had_defaults,
    bool is_array
) {
    int keys_end = lua_gettop(L);
    if (keys_end - keys_start < 0) {
        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 28, "no keys provided to navigate");
    }
    lua_pushvalue(L, root_idx);
    lua_pushvalue(L, root_idx);
    lua_rawget(L, DECODE_DEFINED_IDX);
    for (int key_idx = keys_start; key_idx <= keys_end; key_idx++) {
        int defidx = lua_gettop(L);
        int validx = defidx - 1;
        if (lua_isnil(L, defidx)) {
            lua_settop(L, validx);
            lua_newtable(L);
            lua_pushvalue(L, validx);
            lua_pushvalue(L, -2);
            lua_rawset(L, DECODE_DEFINED_IDX);
        }
        lua_pushvalue(L, key_idx);
        lua_pushboolean(L, true);
        lua_rawset(L, defidx);
        lua_settop(L, validx); // remove defidx, as we are done with it for now.
        lua_pushvalue(L, key_idx);
        lua_rawget(L, validx);
        int vtype = lua_type(L, -1);
        if (vtype == LUA_TNIL) {
            lua_pop(L, 1);      // remove nil
            lua_newtable(L);    // create new table
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);
            lua_rawset(L, validx);   // t[key] = new table
        } else if (vtype != LUA_TTABLE) {
            TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
            set_tmlerr(err, false, 44, "cannot navigate through non-table! Key was: ");
            return err_push_keys(L, err, keys_start, keys_end);
        }
        lua_remove(L, validx);  // will remove validx
        lua_pushvalue(L, validx);  // and the new value will be shifted down into its place
        lua_rawget(L, DECODE_DEFINED_IDX);
        int deftype = lua_type(L, defidx); // get the type of the new defidx
        if (key_idx < keys_end) {  // NOTE: not last key
            lua_Integer len = lua_tointeger(L, defidx);
            lua_pop(L, 1);
            if (len < 0) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 33, "value already defined inline at: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            if (deftype == LUA_TNUMBER) {
                lua_rawgeti(L, validx, len);
                lua_replace(L, validx);
                lua_settop(L, validx);
                lua_pushvalue(L, validx);
                lua_rawget(L, DECODE_DEFINED_IDX);  // update defidx for next iteration
            } else if (deftype != LUA_TTABLE && deftype != LUA_TNIL) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 33, "value already defined inline at: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            if (!lua_istable(L, validx)) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 44, "cannot navigate through non-table! Key was: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            lua_settop(L, defidx);
        } else {  // NOTE: Last key
            if (is_array) {
                if (lua_istable(L, defidx)) {
                    TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                    set_tmlerr(err, false, 26, "table already defined at: ");
                    return err_push_keys(L, err, keys_start, keys_end);
                }
                lua_Integer len = lua_tointeger(L, defidx);
                if (had_defaults && deftype != LUA_TNUMBER) {
                    len = lua_arraylen(L, validx);
                }
                lua_pop(L, 1);
                if (len < 0) {
                    TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                    set_tmlerr(err, false, 33, "array already defined inline at: ");
                    return err_push_keys(L, err, keys_start, keys_end);
                }
                len++;
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_rawseti(L, validx, len);
                lua_pushvalue(L, validx);
                lua_pushinteger(L, len);
                lua_rawset(L, DECODE_DEFINED_IDX);
                lua_replace(L, validx);
            } else if (deftype == LUA_TTABLE) {
                lua_pushlightuserdata(L, (void *)&DEFINED_MARK);
                lua_rawget(L, defidx);
                if (lua_toboolean(L, -1)) {
                    TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                    set_tmlerr(err, false, 32, "table already defined! Key was: ");
                    return err_push_keys(L, err, keys_start, keys_end);
                }
                lua_pushlightuserdata(L, (void *)&DEFINED_MARK);
                lua_pushboolean(L, true);
                lua_rawset(L, defidx);
            } else if (deftype == LUA_TNIL) {
                lua_pushvalue(L, validx);
                lua_newtable(L);
                lua_pushlightuserdata(L, (void *)&DEFINED_MARK);
                lua_pushboolean(L, true);
                lua_rawset(L, -3);
                lua_rawset(L, DECODE_DEFINED_IDX);
            } else {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 32, "table already defined! Key was: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            lua_settop(L, validx);
        }
    }
    // Top is validx after loop
    // grab validx and put it at keys_start
    lua_replace(L, keys_start);
    // pop the rest of the keys
    lua_settop(L, keys_start);
    return true;
}

// pops keys, does not pop root
// does the checks for set, but just returns final table and last key onto the stack
// with the last key on top and final table below it
// This allows decode_inline_value to set directly into it as well.
static bool recursive_lua_set_nav(lua_State *L, int keys_start, int root_idx) {
    int keys_end = lua_gettop(L);
    if (keys_end - keys_start < 0) {
        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 28, "no keys provided to navigate");
    }
    lua_pushvalue(L, root_idx);
    for (int key_idx = keys_start; key_idx <= keys_end; key_idx++) {
        int parent_idx = lua_gettop(L);
        if (key_idx < keys_end) {  // NOTE: not last key
            lua_pushvalue(L, parent_idx);
            lua_rawget(L, DECODE_DEFINED_IDX);
            int deftype = lua_type(L, parent_idx + 1);
            if (deftype == LUA_TNIL) {
                lua_pop(L, 1);
                lua_pushvalue(L, parent_idx);
                lua_newtable(L);
                lua_pushvalue(L, key_idx);
                lua_pushboolean(L, true);
                lua_rawset(L, -3);
                lua_pushlightuserdata(L, (void *)&DEFINED_MARK);
                lua_pushboolean(L, true);
                lua_rawset(L, -3);
                lua_rawset(L, DECODE_DEFINED_IDX);
            } else if (deftype == LUA_TTABLE) {
                lua_pushvalue(L, key_idx);
                lua_pushboolean(L, true);
                lua_rawset(L, parent_idx + 1);
            } else {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 68, "Tried to use key indexing to set value in inline value or array at: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            lua_settop(L, parent_idx);
            lua_pushvalue(L, key_idx);
            lua_rawget(L, parent_idx);
            if (lua_istable(L, -1)) {
                lua_remove(L, parent_idx);
            } else {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushvalue(L, key_idx);
                lua_pushvalue(L, -2);
                lua_rawset(L, parent_idx);
                lua_remove(L, parent_idx);
            }
        } else {  // NOTE: last key
            lua_pushvalue(L, parent_idx);
            lua_rawget(L, DECODE_DEFINED_IDX);
            int pdef = parent_idx + 1;
            int pdef_type = lua_type(L, pdef);
            if (pdef_type == LUA_TTABLE) {
                lua_pushvalue(L, key_idx);
                lua_rawget(L, pdef);
                if (lua_isboolean(L, -1)) {
                    TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                    set_tmlerr(err, false, 30, "key already defined! Key was: ");
                    return err_push_keys(L, err, keys_start, keys_end);
                }
                lua_pop(L, 1);
                lua_pushvalue(L, key_idx);
                lua_pushboolean(L, true);
                lua_rawset(L, pdef);
                lua_settop(L, parent_idx);
            } else if (pdef_type == LUA_TNIL) {
                lua_pop(L, 1);  // remove nil pdef
                lua_pushvalue(L, parent_idx);  // used to set back into def_err_idx
                lua_newtable(L);  // new pdef
                lua_pushvalue(L, key_idx);
                lua_pushboolean(L, true);
                lua_rawset(L, -3);  // add initial key to new pdef
                lua_pushlightuserdata(L, (void *)&DEFINED_MARK);
                lua_pushboolean(L, true);
                lua_rawset(L, -3);
                // stack: new pdef, parent_idx copy (key), parent_idx
                lua_rawset(L, DECODE_DEFINED_IDX);
            } else {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 52, "Tried to use key indexing to set value in array at: ");
                return err_push_keys(L, err, keys_start, keys_end);
            }
            // stack: parent_idx, keys...
            // NOTE: push both first so you dont overwrite yourself
            lua_pushvalue(L, parent_idx);
            lua_pushvalue(L, keys_end);
            lua_replace(L, keys_start + 1);
            lua_replace(L, keys_start);
            lua_settop(L, keys_start + 1);
        }
    }
    return true;
}

// function is to recieve src iterator starting after the first `=`,
// it is also to recieve the table to set into, and the key to use to do it on the top of the stack, with the key on top and table below it.
static bool decode_inline_value(
    lua_State *L,
    str_iter *src,
    str_buf *buf,
    const TomluaUserOpts opts
);

// adds values to table on top of the lua stack and returns NULL or error
static bool parse_inline_table(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts) {
    int root_idx = lua_gettop(L);
    bool last_was_comma = false;
    const bool int_keys = opts[TOMLOPTS_INT_KEYS];
    const bool fancy_tables = opts[TOMLOPTS_FANCY_TABLES];
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '}') {  // NOTE: SUCCESSFUL EXIT
            iter_skip(src);
            if (last_was_comma && !fancy_tables) {
                lua_pop(L, 1);
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 42, "trailing comma in inline table not allowed");
            }
            return true;
        } else if (iter_peek(src).v == '\n') {
            iter_skip(src);
            if (!fancy_tables) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 35, "inline tables can not be multi-line");
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            if (!fancy_tables) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 35, "inline tables can not be multi-line");
        } else if (d == ',') {
            iter_skip(src);
            if (last_was_comma) {
                lua_pop(L, 1);
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 18, "2 commas in a row!");
            }
            last_was_comma = true;
            continue;
        } else if (d == ' ' || d == '\t') {
            iter_skip(src);
            continue;
        }
        last_was_comma = false;
        if (!parse_keys(L, src, buf, int_keys, DECODE_DEFINED_IDX)) return false;
        if (iter_peek(src).ok && iter_peek(src).v != '=') {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 35, "keys for assignment must end with =");
        }
        iter_skip(src);
        if (consume_whitespace_to_line(src)) {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 76, "the value in key = value expressions must begin on the same line as the key!");
        }
        if (!recursive_lua_set_nav(L, root_idx + 1, root_idx)) return false;
        if (!decode_inline_value(L, src, buf, opts)) return false;
        if (fancy_tables) {
            while (consume_whitespace_to_line(src)) {}
        } else if (consume_whitespace_to_line(src)) {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 39, "toml inline tables cannot be multi-line");
        }
        iter_result next = iter_peek(src);
        if (next.ok && (next.v != ',' && next.v != '}')) {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 65, "toml inline table values must be separated with , or ended with }");
        }
    }
    return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 17, "missing closing }");
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
// it is also to recieve the table to set into, and the key to use to do it on the top of the stack, with the key on top and table below it.
bool decode_inline_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts) {
    // stack is currently: target_key, dest_table (already checked and made ready to be set by recursive_lua_set_nav)
    int key_idx = lua_gettop(L);
    int dest_idx = key_idx - 1;
    iter_result curr = iter_peek(src);
    if (!curr.ok) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 34, "expected value, got end of content");
    // --- boolean ---
    if (iter_starts_with(src, "true", 4)) {
        iter_skip_n(src, 4);
        lua_pushboolean(L, 1);
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    } else if (iter_starts_with(src, "false", 5)) {
        iter_skip_n(src, 5);
        lua_pushboolean(L, 0);
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    // --- strings ---
    } else if (iter_starts_with(src, "\"\"\"", 3)) {
        buf_soft_reset(buf);
        iter_skip_n(src, 3);
        if (!parse_multi_basic_string(L, buf, src, DECODE_DEFINED_IDX)) {
            return false;
        }
        if (opts[TOMLOPTS_MULTI_STRINGS]) {
            str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
            if (!s || !buf || !buf->data) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 59, "tomlua.decode failed to push multi-line string to lua stack");
            }
            *s = new_buf_from_str(buf->data, buf->len);
            push_multi_string_mt(L);
            lua_setmetatable(L, -2);
        } else {
            if (!push_buf_to_lua_string(L, buf)) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 48, "tomlua.decode failed to push string to lua stack");
            }
        }
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    } else if (curr.v == '"') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_basic_string(L, buf, src, DECODE_DEFINED_IDX)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    } else if (iter_starts_with(src, "'''", 3)) {
        buf_soft_reset(buf);
        iter_skip_n(src, 3);
        if (!parse_multi_literal_string(L, buf, src, DECODE_DEFINED_IDX)) {
            return false;
        }
        if (opts[TOMLOPTS_MULTI_STRINGS]) {
            str_buf *s = (str_buf *)lua_newuserdata(L, sizeof(str_buf));
            if (!s || !buf || !buf->data) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 59, "tomlua.decode failed to push multi-line string to lua stack");
            }
            *s = new_buf_from_str(buf->data, buf->len);
            push_multi_string_mt(L);
            lua_setmetatable(L, -2);
        } else {
            if (!push_buf_to_lua_string(L, buf)) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 48, "tomlua.decode failed to push string to lua stack");
            }
        }
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    } else if (curr.v == '\'') {
        buf_soft_reset(buf);
        iter_skip(src);
        if (!parse_literal_string(L, buf, src, DECODE_DEFINED_IDX)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 48, "tomlua.decode failed to push string to lua stack");
        }
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    // --- numbers (and dates) ---
    } else if (iter_starts_with(src, "inf", 3)) {
        iter_skip_n(src, 3);
        lua_pushnumber(L, INFINITY);
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
        return true;
    } else if (iter_starts_with(src, "nan", 3)) {
        iter_skip_n(src, 3);
        lua_pushnumber(L, NAN);
        lua_rawset(L, dest_idx);
        lua_settop(L, dest_idx - 1);
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
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 51, "consecutive underscores not allowed in hex literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 53, "hex literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 17, "empty hex literal");
            // Convert buffer to integer
            if (!push_integer_or_handle(L, buf, 16, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 41, "Parse error: hex literal integer overflow");
            lua_rawset(L, dest_idx);
            lua_settop(L, dest_idx - 1);
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
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 53, "consecutive underscores not allowed in octal literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 55, "octal literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 19, "empty octal literal");
            if (!push_integer_or_handle(L, buf, 8, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 43, "Parse error: octal literal integer overflow");
            lua_rawset(L, dest_idx);
            lua_settop(L, dest_idx - 1);
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
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else if (ch == '_') {
                    if (was_underscore) {
                        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 54, "consecutive underscores not allowed in binary literals");
                    }
                    was_underscore = true;
                    iter_skip(src);
                } else break;
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 56, "binary literals not allowed to have trailing underscores");
            }
            if (buf->len == 0) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 20, "empty binary literal");
            if (!push_integer_or_handle(L, buf, 2, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 44, "Parse error: binary literal integer overflow");
            lua_rawset(L, dest_idx);
            lua_settop(L, dest_idx - 1);
            return true;
        } else {
            // detect dates and pass on as strings, and numbers are allowed to have underscores in them (only 1 consecutive underscore at a time)
            // is date if it has a - in it not immediately preceded by e or E
            // is date if it has a : in it
            buf_soft_reset(buf);
            bool is_float = false;
            bool is_date = false;
            bool t_used = false;
            bool last_was_T_space = false;
            bool z_used = false;
            bool was_underscore = true;
            if (curr.v == '+') {
                if (iter_starts_with(src, "+inf", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, INFINITY);
                    lua_rawset(L, dest_idx);
                    lua_settop(L, dest_idx - 1);
                    return true;
                } else if (iter_starts_with(src, "+nan", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, NAN);
                    lua_rawset(L, dest_idx);
                    lua_settop(L, dest_idx - 1);
                    return true;
                } else {
                    if (!buf_push(buf, curr.v)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 51, "failed to push leading + character to number buffer");
                    iter_skip(src);
                }
            } else if (curr.v == '-') {
                if (iter_starts_with(src, "-inf", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, -INFINITY);
                    lua_rawset(L, dest_idx);
                    lua_settop(L, dest_idx - 1);
                    return true;
                } else if (iter_starts_with(src, "-nan", 4)) {
                    iter_skip_n(src, 4);
                    lua_pushnumber(L, -NAN);
                    lua_rawset(L, dest_idx);
                    lua_settop(L, dest_idx - 1);
                    return true;
                } else {
                    if (!buf_push(buf, curr.v)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 51, "failed to push leading - character to number buffer");
                    iter_skip(src);
                }
            }
            while (iter_peek(src).ok) {
                char ch = iter_peek(src).v;
                if (ch == '_' && !last_was_T_space) {
                    if (is_date) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 44, "date literal not allowed to have underscores");
                    iter_skip(src);
                    if (was_underscore) {
                        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 46, "consecutive underscores not allowed in numbers");
                    }
                    was_underscore = true;
                } else if ((ch == 'e' || ch == 'E') && !last_was_T_space) {
                    if (is_date) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 41, "date literal not allowed to have exponent");
                    is_float = true;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                    iter_result next = iter_peek(src);
                    if (next.ok && (next.v == '+' || next.v == '-')) {
                        if (!buf_push(buf, next.v)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                        iter_skip(src);
                    }
                    was_underscore = false;
                } else if (ch == ':' && !last_was_T_space) {
                    is_date = true;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "failed to push date character");
                    iter_skip(src);
                } else if (ch == '-' && !last_was_T_space) {
                    is_date = true;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else if (is_date && !t_used && ch == 'T' && !last_was_T_space) {
                    t_used = true;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "failed to push date character");
                    iter_skip(src);
                } else if (is_date && !t_used && ch == ' ' && !last_was_T_space) {
                    t_used = true;
                    was_underscore = false;
                    last_was_T_space = true;
                    iter_skip(src);
                } else if (is_date && !z_used && ch == 'Z' && !last_was_T_space) {
                    z_used = true;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "failed to push date character");
                    iter_skip(src);
                } else if (ch == '.' && !last_was_T_space) {
                    is_float = true;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else if (ch >= '0' && ch <= '9') {
                    if (last_was_T_space) {
                        if (!buf_push(buf, ' ')) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "failed to push date character");
                    }
                    last_was_T_space = false;
                    was_underscore = false;
                    if (!buf_push(buf, ch)) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 31, "failed to push number character");
                    iter_skip(src);
                } else {
                    was_underscore = false;
                    last_was_T_space = false;
                    break;
                }
            }
            if (was_underscore) {
                return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 56, "number literals not allowed to have trailing underscores");
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
                            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 29, "Invalid date format provided!");
                        if (!push_new_toml_date(L, date))
                            return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 51, "tomlua.decode failed to push date type to lua stack");
                    } else if (!push_buf_to_lua_string(L, buf)) {
                        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 53, "tomlua.decode failed to push date string to lua stack");
                    }
                } else if (is_float) {
                    if (!push_float_or_handle(L, buf, opts[TOMLOPTS_OVERFLOW_ERRORS], opts[TOMLOPTS_UNDERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 35, "Parse error: float literal overflow");
                } else {
                    if (!push_integer_or_handle(L, buf, 10, opts[TOMLOPTS_OVERFLOW_ERRORS])) return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 37, "Parse error: integer literal overflow");
                }
                lua_rawset(L, dest_idx);
                lua_settop(L, dest_idx - 1);
                return true;
            }
        }
    // --- array --- allows trailing comma and multiline
    } else if (curr.v == '[') {
        iter_skip(src);
        lua_pushvalue(L, key_idx);
        lua_rawget(L, dest_idx);
        int thearray = lua_gettop(L);
        int idx;
        if (!lua_istable(L, thearray)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);
            lua_rawset(L, dest_idx);
            if (opts[TOMLOPTS_MARK_INLINE]) {
                lua_newtable(L);
                lua_pushliteral(L, "ARRAY_INLINE");
                lua_setfield(L, -2, "toml_type");
                lua_setmetatable(L, thearray);
            }
            idx = 1;
        } else {
            idx = lua_arraylen(L, thearray) + 1;
            if (opts[TOMLOPTS_MARK_INLINE]) {
                lua_getmetatable(L, thearray);
                if (!lua_istable(L, -1)) {
                    lua_settop(L, thearray);
                    lua_newtable(L);
                    lua_pushliteral(L, "ARRAY_INLINE");
                    lua_setfield(L, -2, "toml_type");
                    lua_setmetatable(L, thearray);
                } else {
                    lua_pushliteral(L, "ARRAY_INLINE");
                    lua_setfield(L, -2, "toml_type");
                    lua_settop(L, thearray);
                }
            }
        }
        lua_pushvalue(L, thearray);
        lua_pushinteger(L, -2);
        lua_rawset(L, DECODE_DEFINED_IDX);
        while (iter_peek(src).ok) {
            char d = iter_peek(src).v;
            if (d == ']') {
                iter_skip(src);
                lua_settop(L, dest_idx - 1);
                return true;
            } else if (d == ',' || d == ' ' || d == '\t' || d == '\n' || d == '\r') {
                iter_skip(src);
                continue;
            }
            lua_pushvalue(L, thearray);
            lua_pushinteger(L, idx++);
            if (!decode_inline_value(L, src, buf, opts)) return false;
        }
        return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 17, "missing closing ]");
    // --- inline table --- does NOT support multiline or trailing comma (without fancy_tables)
    } else if (curr.v == '{') {
        iter_skip(src);
        lua_pushvalue(L, key_idx); // push key
        lua_rawget(L, dest_idx);   // get current value
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, key_idx);
            lua_pushvalue(L, -2);
            lua_rawset(L, dest_idx);
            if (opts[TOMLOPTS_MARK_INLINE]) {
                lua_newtable(L);
                lua_pushliteral(L, "TABLE_INLINE");
                lua_setfield(L, -2, "toml_type");
                lua_setmetatable(L, -2);
            }
        } else {
            if (opts[TOMLOPTS_MARK_INLINE]) {
                lua_getmetatable(L, -1);
                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_pushliteral(L, "TABLE_INLINE");
                    lua_setfield(L, -2, "toml_type");
                    lua_setmetatable(L, -2);
                } else {
                    lua_pushliteral(L, "TABLE_INLINE");
                    lua_setfield(L, -2, "toml_type");
                    lua_pop(L, 1);
                }
            }
        }
        if (!parse_inline_table(L, src, buf, opts)) return false;
        lua_pushvalue(L, -1);
        lua_pushinteger(L, -1);
        lua_rawset(L, DECODE_DEFINED_IDX);
        lua_settop(L, dest_idx - 1);
        return true;

    }
    return set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 13, "invalid value");
}

int tomlua_decode(lua_State *L) {
    // process arguments and options
    str_iter src = lua_str_to_iter(L, 1);
    if (src.buf == NULL) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, "tomlua.decode first argument must be a string! tomlua.decode(string) -> table?, err?");
        return 2;
    }
    TomluaUserOpts uopts;
    toml_user_opts_copy(uopts, *get_opts_upval(L));
    const bool int_keys = uopts[TOMLOPTS_INT_KEYS];
    // DECODE_RESULT_IDX == 2 == here
    bool had_defaults = false;
    if (lua_istable(L, 2)) {
        had_defaults = true;
        lua_settop(L, 2);
    } else {
        lua_settop(L, 1);
        lua_newtable(L);
    }
    // DECODE_DEFINED_IDX == 3 == here
    // @type { [table]: len if array or -1 for defined table }
    // or error if error
    lua_newtable(L);

    // set top as the starting location
    lua_pushvalue(L, DECODE_RESULT_IDX);
    int root_idx = lua_gettop(L);
    // avoid allocations by making every parse_value use the same scratch buffer
    str_buf scratch = new_str_buf();
    if (scratch.data == NULL) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushstring(L, "Unable to allocate memory for scratch buffer");
        return 2;
    }
    while (iter_peek(&src).ok) {
        {
            // consume until non-blank line, consume initial whitespace, then end loop
            int end_line = consume_whitespace_to_line(&src);
            while (end_line == 1) end_line = consume_whitespace_to_line(&src);
            if (end_line == 2) break;
        }
        if (iter_starts_with(&src, "[[", 2)) {
            iter_skip_n(&src, 2);
            lua_settop(L, DECODE_DEFINED_IDX);  // pop current location, we are moving
            if(!parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX)) goto fail;
            if (!iter_starts_with(&src, "]]", 2)) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 14, "array heading ");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx, top);
                tmlerr_push_str(err, " must end with ]]", 17);
                goto fail;
            }
            iter_skip_n(&src, 2);  // consume ]]
            if (!consume_whitespace_to_line(&src)) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 8, "array [[");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx, top);
                tmlerr_push_str(err, "]] must have a new line before new values", 41);
                goto fail;
            }
            if (!recursive_lua_nav(L, root_idx, DECODE_RESULT_IDX, had_defaults, true)) goto fail;
        } else if (iter_peek(&src).v == '[') {
            iter_skip(&src);
            lua_settop(L, DECODE_DEFINED_IDX);  // pop current location, we are moving
            if (!parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX)) goto fail;
            if (iter_peek(&src).v != ']') {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 14, "table heading ");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx, top);
                tmlerr_push_str(err, " must end with ]", 16);
                goto fail;
            }
            iter_skip(&src);  // consume ]
            if (!consume_whitespace_to_line(&src)) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 7, "table [");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx, top);
                tmlerr_push_str(err, "] must have a new line before new values", 40);
                goto fail;
            }
            if (!recursive_lua_nav(L, root_idx, DECODE_RESULT_IDX, had_defaults, false)) goto fail;
        } else {
            if (!parse_keys(L, &src, &scratch, int_keys, DECODE_DEFINED_IDX)) goto fail;
            if (iter_peek(&src).v != '=') {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 20, "keys for assignment ");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx + 1, top);
                tmlerr_push_str(err, " must end with =", 16);
                goto fail;
            }
            iter_skip(&src);  // consume =
            if (consume_whitespace_to_line(&src)) {
                TMLErr *err = new_tmlerr(L, DECODE_DEFINED_IDX);
                set_tmlerr(err, false, 86, "the value in key = value expressions must begin on the same line as the key! Key was: ");
                int top = lua_gettop(L);
                err_push_keys(L, err, root_idx + 1, top);
                goto fail;
            }
            if (!recursive_lua_set_nav(L, root_idx + 1, root_idx)) goto fail;
            if (!decode_inline_value(L, &src, &scratch, uopts)) goto fail;
            if (!consume_whitespace_to_line(&src)) {
                set_tmlerr(new_tmlerr(L, DECODE_DEFINED_IDX), false, 66, "key value pairs must be followed by a new line (or end of content)");
                goto fail;
            }
        }
        lua_settop(L, root_idx);
    }

    lua_settop(L, DECODE_RESULT_IDX);
    free_str_buf(&scratch);
    return 1;

fail:
    lua_settop(L, DECODE_DEFINED_IDX);
    free_str_buf(&scratch);
    src.pos = (src.pos >= src.len) ? src.len - 1 : src.pos;
    tmlerr_push_ctx_from_iter(get_err_val(L, DECODE_DEFINED_IDX), 7, &src);
    lua_pushnil(L);
    push_tmlerr_string(L, get_err_val(L, DECODE_DEFINED_IDX));
    return 2;
}
