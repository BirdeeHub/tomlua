#include <stdlib.h>
#include "parse_keys.h"
#include "parse_str.h"
#include "types.h"

static bool parse_key(lua_State *L, str_iter *src, str_buf *res) {
    iter_result current = iter_peek(src);
    if (!current.ok) {
        set_err_upval(L, false, 32, "expected key, got end of content");
        return false;
    }
    char c = current.v;
    if (c == '"') {
        iter_skip(src);
        if (!parse_basic_string(L, res, src)) return false;
    }else if (c == '\'') {
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

static bool keys_push_move(keys_result *dst, str_buf buf) {
    if (dst->len >= dst->cap) {
        dst-> cap = dst->cap > 0 ? dst->cap * 2 : 1;
        str_buf *tmp = realloc(dst->v, dst->cap * sizeof(str_buf));
        if (!tmp) return false;
        dst->v = tmp;
    }
    dst->v[dst->len++] = (str_buf) {
        .data = buf.data,
        .len = buf.len,
        .capacity = buf.capacity
    };
    buf.data = NULL;
    buf.len = buf.capacity = 0;
    return true;
}

int parse_keys(lua_State *L, str_iter *src, str_buf *buf) {
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
