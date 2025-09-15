// Copyright 2025 Birdee
#ifndef SRC_DECODE_KEYS_H_
#define SRC_DECODE_KEYS_H_

#include "./decode_str.h"
#include "./types.h"
#include "./error_context.h"
#include <stddef.h>

static inline bool parse_key(lua_State *L, str_iter *src, str_buf *buf, bool int_keys) {
    buf_soft_reset(buf);
    iter_result current = iter_peek(src);
    if (!current.ok) {
        set_tmlerr(new_tmlerr(L), false, 32, "expected key, got end of content");
        return false;
    }
    char c = current.v;
    bool non_string = false;
    if (c == '"') {
        iter_skip(src);
        if (!parse_basic_string(L, buf, src)) return false;
    } else if (c == '\'') {
        iter_skip(src);
        if (!parse_literal_string(L, buf, src)) return false;
    } else if (is_identifier_char(c)) {
        non_string = true;
        current = iter_peek(src);
        while (is_identifier_char(current.v)) {
            if (!buf_push(buf, current.v)) {
                set_tmlerr(new_tmlerr(L), false, 3, "OOM");
                return false;
            }
            iter_skip(src);
            current = iter_peek(src);
        }
    } else {
        TMLErr *err = new_tmlerr(L);
        set_tmlerr(err, false, 42, "called parse_key with invalid first char: ");
        tmlerr_push(err, c);
        return false;
    }
    if (!push_buf_to_lua_string(L, buf)) {
        set_tmlerr(new_tmlerr(L), false, 3, "OOM");
        return false;
    }
    if (int_keys && non_string && lua_isnumber(L, -1)) {
        lua_Number n = lua_tonumber(L, -1);
        if (n == (lua_Number)(lua_Integer)n) {
            lua_pop(L, 1);
            lua_pushnumber(L, n);
        }
    }
    return true;
}

static int parse_keys(lua_State *L, str_iter *src, str_buf *buf, bool int_keys) {
    int i = 0;
    while (iter_peek(src).ok) {
        i++;
        if (consume_whitespace_to_line(src)) {
            set_tmlerr(new_tmlerr(L), false, 33, "newlines not allowed between keys");
            return false;
        }
        if (!parse_key(L, src, buf, int_keys)) {
            return false;
        }
        if (consume_whitespace_to_line(src)) {
            set_tmlerr(new_tmlerr(L), false, 68, "newlines not allowed between keys and their terminators: =, ], or ]]");
            return false;
        }
        iter_result next = iter_peek(src);
        if (next.ok) {
            if (next.v == '=' || next.v == ']') {
                break;
            } else if (next.v == '.') {
                iter_skip(src);
            } else {
                set_tmlerr(new_tmlerr(L), false, 43, "keys may not contain spaces without quotes!");
                return false;
            }
        } else {
            set_tmlerr(new_tmlerr(L), false, 13, "trailing key!");
            return false;
        }
    }
    if (!i) set_tmlerr(new_tmlerr(L), false, 14, "empty key list");
    return i;
}

#endif  // SRC_DECODE_KEYS_H_
