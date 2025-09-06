// Copyright 2025 Birdee
#ifndef SRC_PARSE_KEYS_H_
#define SRC_PARSE_KEYS_H_

#include <math.h>
#include "./parse_str.h"
#include "./types.h"

static inline bool is_identifier_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '_') || (c == '-');
}

// consumes whitespace until end of whitespace, line, or file.
// returns true if the end of the line or file was found
// 0 for whitespace ending before end of line
// 1 for end of line, 2 for end of file
// (both 1 and 2 will be read as true in if statements)
static inline int consume_whitespace_to_line(str_iter *src) {
    // skips through the end of the line on trailing comments and failed parses
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '#') {
            iter_skip(src);
            iter_result curr = iter_next(src);
            while (curr.ok) {
                if (curr.v == '\n') {
                    return true;
                } else if (curr.v == '\r' && iter_peek(src).v == '\n') {
                    iter_skip(src);
                    return true;
                }
                curr = iter_next(src);
            }
            // reached EOF in a comment
            return true;
        } else if (d == '\n') {
            iter_skip(src);
            return true;
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            return true;
        } else if (d == ' ' || d == '\t') {
            iter_skip(src);
        } else {
            // read non-whitespace
            return false;
        }
    }
    // read whitespace until EOF
    return 2;
}

static inline bool parse_key(lua_State *L, str_iter *src, str_buf *res) {
    iter_result current = iter_peek(src);
    if (!current.ok) {
        set_err_upval(L, false, 32, "expected key, got end of content");
        return false;
    }
    char c = current.v;
    if (c == '"') {
        iter_skip(src);
        if (!parse_basic_string(L, res, src)) return false;
    } else if (c == '\'') {
        iter_skip(src);
        if (!parse_literal_string(L, res, src)) return false;
    } else if (is_identifier_char(c)) {
        current = iter_peek(src);
        while (is_identifier_char(current.v)) {
            if (!buf_push(res, current.v)) {
                set_err_upval(L, false, 3, "OOM");
                return false;
            }
            iter_skip(src);
            current = iter_peek(src);
        }
    } else {
        set_err_upval(L, false, 42, "called parse_key with invalid first char: ");
        err_push(L, c);
        return false;
    }
    return true;
}

static inline int parse_keys(lua_State *L, str_iter *src, str_buf *buf, bool int_keys) {
    int i = 0;
    while (iter_peek(src).ok) {
        i++;
        if (consume_whitespace_to_line(src)) {
            set_err_upval(L, false, 33, "newlines not allowed between keys");
            return false;
        }
        buf_soft_reset(buf);
        if (!parse_key(L, src, buf)) {
            return false;
        }
        if (!push_buf_to_lua_string(L, buf)) {
            set_err_upval(L, false, 3, "OOM");
            return false;
        }
        if (int_keys && lua_isnumber(L, -1)) {
            lua_Number n = lua_tonumber(L, -1);
            if (n == (lua_Number)(int64_t)n) {
                lua_pop(L, 1);
                lua_pushnumber(L, n);
            }
        }
        if (consume_whitespace_to_line(src)) {
            set_err_upval(L, false, 68, "newlines not allowed between keys and their terminators: =, ], or ]]");
            return false;
        }
        iter_result next = iter_peek(src);
        if (next.ok) {
            if (next.v == '=' || next.v == ']') {
                break;
            } else if (next.v == '.') {
                iter_skip(src);
            }
        } else {
            set_err_upval(L, false, 13, "trailing key!");
            return false;
        }
    }
    if (!i) set_err_upval(L, false, 14, "empty key list");
    return i;
}

#endif  // SRC_PARSE_KEYS_H_
