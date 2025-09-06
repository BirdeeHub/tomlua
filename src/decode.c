#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>

#include "types.h"
#include "parse_str.h"
#include "parse_keys.h"
#include "output_table_lib.h"

static inline bool parse_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts *opts);

// adds a table to the lua stack and return NULL or error
static inline bool parse_inline_table(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts *opts) {
    lua_newtable(L);
    bool last_was_comma = false;
    const bool strict = opts->strict;
    const bool enhanced_tables = opts->enhanced_tables;
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '}') {
            iter_skip(src);
            if (last_was_comma && !enhanced_tables) {
                lua_pop(L, 1);
                return set_err_upval(L, false, 42, "trailing comma in inline table not allowed");
            }
            return true;
        } else if (iter_peek(src).v == '\n') {
            iter_skip(src);
            if (!enhanced_tables) return set_err_upval(L, false, 35, "inline tables can not be multi-line");
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            if (!enhanced_tables) return set_err_upval(L, false, 35, "inline tables can not be multi-line");
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
        int keys_len = parse_keys(L, src, buf);
        if (!keys_len) return false;
        if (iter_peek(src).ok && iter_peek(src).v != '=') {
            return set_err_upval(L, false, 35, "keys for assignment must end with =");
        }
        iter_skip(src);
        if (consume_whitespace_to_line(src)) {
            return set_err_upval(L, false, 76, "the value in key = value expressions must begin on the same line as the key!");
        }
        if (!parse_value(L, src, buf, opts)) {
            return false;
        }
        lua_insert(L, -keys_len - 1);
        if (strict) {
            if (!set_kv_strict(L, keys_len)) {
                return false;
            }
        } else {
            if (!set_kv(L, keys_len)) {
                return false;
            }
        }
        if (enhanced_tables) {
            while (consume_whitespace_to_line(src)) {}
        } else if (consume_whitespace_to_line(src)) {
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
bool parse_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts *opts) {
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
            if (!parse_value(L, src, buf, opts)) return false;
            lua_rawseti(L, -2, idx++);
        }
        return set_err_upval(L, false, 17, "missing closing ]");
    // --- inline table --- does NOT support multiline or trailing comma (without enhanced_tables)
    } else if (curr.v == '{') {
        iter_skip(src);
        return parse_inline_table(L, src, buf, opts);
    }
    return set_err_upval(L, false, 13, "invalid value");
}

int tomlua_decode(lua_State *L) {
    // process arguments and options
    if (!lua_isstring(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "tomlua.decode first argument must be a string! tomlua.decode(string, table|bool?) -> table?, err?");
        return 2;
    }
    TomluaUserOpts *uopts = get_opts_upval(L);
    const bool strict = uopts->strict;
    if (strict) create_defined_table(L);
    if (lua_istable(L, 2)) {
        lua_settop(L, 2);
    } else {
        lua_settop(L, 1);
        lua_newtable(L);
    }
    // pop and store top, this will be our result at the end
    push_to_output_table(L);

    str_iter src;
    {
        size_t len;
        const char *s = lua_tolstring(L, 1, &len);
        lua_pop(L, 1); // pop the string
        src = str_to_iter(s, len);
    }

    // set top as the starting location
    push_output_table(L);
    // avoid allocations by making every parse_value use the same scratch buffer
    str_buf scratch = new_str_buf();
    while (iter_peek(&src).ok) {
        {
            // consume until non-blank line, consume initial whitespace, then end loop
            int end_line = consume_whitespace_to_line(&src);
            while (end_line == 1) end_line = consume_whitespace_to_line(&src);
            if (end_line == 2) break;
        }
        if (iter_starts_with(&src, "[[", 2)) {
            iter_skip_n(&src, 2);
            lua_pop(L, 1); // pop current location, we are moving
            int keys_len = parse_keys(L, &src, &scratch);
            if (!keys_len) goto fail;
            if (!iter_starts_with(&src, "]]", 2)) {
                set_err_upval(L, false, 30, "table heading must end with ]]");
                goto fail;
            }
            iter_skip_n(&src, 2);  // consume ]]
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 56, "array [[headers]] must have a new line before new values");
                goto fail;
            }
            if (strict) {
                if (!heading_nav_strict(L, keys_len, true)) goto fail;
            } else {
                if (!heading_nav(L, keys_len, true)) goto fail;
            }
        } else if (iter_peek(&src).v == '[') {
            iter_skip(&src);
            lua_pop(L, 1);  // pop current location, we are moving
            int keys_len = parse_keys(L, &src, &scratch);
            if (!keys_len) goto fail;
            if (iter_peek(&src).v != ']') {
                set_err_upval(L, false, 29, "table heading must end with ]");
                goto fail;
            }
            iter_skip(&src);  // consume ]
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 54, "table [headers] must have a new line before new values");
                goto fail;
            }
            if (strict) {
                if (!heading_nav_strict(L, keys_len, false)) goto fail;
            } else {
                if (!heading_nav(L, keys_len, false)) goto fail;
            }
        } else {
            int keys_len = parse_keys(L, &src, &scratch);
            if (!keys_len) goto fail;
            if (iter_peek(&src).v != '=') {
                set_err_upval(L, false, 35, "keys for assignment must end with =");
                goto fail;
            }
            iter_skip(&src);  // consume =
            if (consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 76, "the value in key = value expressions must begin on the same line as the key!");
                goto fail;
            }
            if (!parse_value(L, &src, &scratch, uopts)) goto fail;
            lua_insert(L, -keys_len -1);
            // [-1?] keys
            // [-?] value
            // [-?-1] current root table
            if (strict) {
                if (!set_kv_strict(L, keys_len)) goto fail;
            } else {
                if (!set_kv(L, keys_len)) goto fail;
            }
            // [-1] current root table
            if (!consume_whitespace_to_line(&src)) {
                set_err_upval(L, false, 66, "key value pairs must be followed by a new line (or end of content)");
                goto fail;
            }
        }
    }

    lua_settop(L, 0);
    if (strict) reset_defined_table(L);
    free_str_buf(&scratch);
    push_output_table(L);
    lua_pushnil(L);
    push_to_output_table(L);
    return 1;

fail:
    lua_settop(L, 0);
    lua_pushnil(L);
    push_to_output_table(L);
    if (strict) reset_defined_table(L);
    free_str_buf(&scratch);
    lua_pushnil(L);
    push_err_upval(L);
    return 2;
}
