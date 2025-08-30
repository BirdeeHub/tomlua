#include <stdlib.h>
#include <string.h>
#include "parse_keys.h"
#include "parse_str.h"
#include "str_buf.h"

static key_result parse_key(str_iter *src) {
    key_result dst = {
        .err = NULL,
        .v = new_str_buf(),
    };
    iter_result current = iter_peek(src);
    if (!current.ok) {
        dst.err = strdup("expected key, got end of content");
        return dst;
    }
    char c = current.v;
    if (c == '"') {
        iter_skip(src);
        dst.err = parse_basic_string(&dst.v, src);
    }else if (c == '\'') {
        iter_skip(src);
        dst.err = parse_literal_string(&dst.v, src);
    } else if (is_identifier_char(c)) {
        current = iter_peek(src);
        while (is_identifier_char(current.v)) {
            if (!buf_push(&dst.v, current.v)) {
                dst.err = strdup("OOM");
            }
            iter_skip(src);
            current = iter_peek(src);
        }
    } else {
        dst.err = strdup("called parse_key with invalid first char");
    }
    if (dst.err != NULL) free_str_buf(&dst.v);
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

keys_result parse_keys(str_iter *src) {
    keys_result dst = {
        .cap = 2,
        .len = 0,
        .err = NULL,
        .v = malloc(dst.cap * sizeof(str_buf)),
    };
    while (iter_peek(src).ok) {
        if (consume_whitespace_to_line(src)) {
            dst.err = strdup("newlines not allowed between keys");
            break;
        }
        key_result key = parse_key(src);
        if (key.err != NULL) {
            dst.err = key.err;
            key.err = NULL;
            break;
        }
        keys_push_move(&dst, key.v);
        if (consume_whitespace_to_line(src)) {
            dst.err = strdup("newlines not allowed between keys and their terminators: =, ], or ]]");
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
            dst.err = strdup("trailing key!");
            break;
        }
    }
    return dst;
}
