#include <stdlib.h>
#include <string.h>
#include "parse_keys.h"
#include "parse_str.h"
#include "str_buf.h"

static struct key_result parse_key(struct str_iter *src) {
    struct key_result dst = {
        .err = NULL,
        .v = new_str_buf(),
    };
    struct iter_result current = iter_peek(src);
    if (!current.ok) {
        dst.err = strdup("expected key, got end of content");
        return dst;
    }
    char c = current.v;
    if (c == '"') {
        iter_next(src);
        dst.err = parse_basic_string(&dst.v, src);
    }else if (c == '\'') {
        iter_next(src);
        dst.err = parse_literal_string(&dst.v, src);
    } else if (is_identifier_char(c)) {
        current = iter_peek(src);
        while (is_identifier_char(current.v)) {
            if (!buf_push(&dst.v, current.v)) {
                dst.err = strdup("OOM");
            }
            iter_next(src);
            current = iter_peek(src);
        }
    } else {
        dst.err = strdup("called parse_key with invalid first char");
    }
    if (dst.err != NULL) free_str_buf(&dst.v);
    return dst;
}

static bool keys_push_move(struct keys_result *dst, struct str_buf buf) {
    if (dst->len >= dst->cap) {
        dst-> cap = dst->cap > 0 ? dst->cap * 2 : 1;
        struct str_buf *tmp = realloc(dst->v, dst->cap * sizeof(struct str_buf));
        if (!tmp) return false;
        dst->v = tmp;
    }
    dst->v[dst->len++] = (struct str_buf) {
        .data = buf.data,
        .len = buf.len,
        .capacity = buf.capacity
    };
    buf.data = NULL;
    buf.len = buf.capacity = 0;
    return true;
}

struct keys_result parse_keys(struct str_iter *src) {
    struct keys_result dst = {
        .cap = 2,
        .len = 0,
        .err = NULL,
        .v = malloc(dst.cap * sizeof(struct str_buf)),
    };
    while (iter_peek(src).ok) {
        if (consume_whitespace_to_line(src)) {
            dst.err = strdup("newlines not allowed between keys");
            break;
        }
        struct key_result key = parse_key(src);
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
        struct iter_result next = iter_peek(src);
        if (next.ok) {
            if (next.v == '=' || next.v == ']') {
                break;
            } else if (next.v == '.') {
                iter_next(src);
            }
        } else {
            dst.err = strdup("trailing key!");
            break;
        }
    }
    return dst;
}
