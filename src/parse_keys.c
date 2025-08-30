#include <stdlib.h>
#include <string.h>
#include "parse_keys.h"
#include "parse_str.h"
#include "types.h"

static key_result parse_key(lua_State *L, str_iter *src) {
    key_result dst = {
        .ok = true,
        .v = new_str_buf(),
    };
    iter_result current = iter_peek(src);
    if (!current.ok) {
        dst.ok = false;
        set_err_upval(L, false, 32, "expected key, got end of content");
        return dst;
    }
    char c = current.v;
    if (c == '"') {
        iter_skip(src);
        dst.ok = parse_basic_string(L, &dst.v, src);
    }else if (c == '\'') {
        iter_skip(src);
        dst.ok = parse_literal_string(L, &dst.v, src);
    } else if (is_identifier_char(c)) {
        current = iter_peek(src);
        while (is_identifier_char(current.v)) {
            if (!buf_push(&dst.v, current.v)) {
                dst.ok = false;
                set_err_upval(L, false, 3, "OOM");
            }
            iter_skip(src);
            current = iter_peek(src);
        }
    } else {
        dst.ok = false;
        set_err_upval(L, false, 40, "called parse_key with invalid first char");
    }
    if (!dst.ok) free_str_buf(&dst.v);
    return dst;
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

keys_result parse_keys(lua_State *L, str_iter *src) {
    keys_result dst = {
        .cap = 2,
        .len = 0,
        .ok = true,
        .v = malloc(dst.cap * sizeof(str_buf)),
    };
    while (iter_peek(src).ok) {
        if (consume_whitespace_to_line(src)) {
            dst.ok = false;
            set_err_upval(L, false, 33, "newlines not allowed between keys");
            break;
        }
        key_result key = parse_key(L, src);
        if (!key.ok) {
            dst.ok = false;
            break;
        }
        keys_push_move(&dst, key.v);
        if (consume_whitespace_to_line(src)) {
            dst.ok = false;
            set_err_upval(L, false, 68, "newlines not allowed between keys and their terminators: =, ], or ]]");
            break;
        }
        iter_result next = iter_peek(src);
        if (next.ok) {
            if (next.v == '=' || next.v == ']') {
                break;
            } else if (next.v == '.') {
                iter_skip(src);
            }
        } else {
            dst.ok = false;
            set_err_upval(L, false, 13, "trailing key!");
            break;
        }
    }
    return dst;
}
