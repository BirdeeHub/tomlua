#include "parse_keys.h"
#include "parse_str.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>

bool is_identifier_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '_') || (c == '-');
}

// returns true if the end of the line or file was found
bool consume_whitespace_to_line(struct str_iter *src) {
    // skips through the end of the line on trailing comments and failed parses
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '#') {
            iter_next(src);
            struct iter_result curr = iter_next(src);
            while (curr.ok) {
                if (curr.v == '\n') {
                    return true;
                } else if (curr.v == '\r' && iter_peek(src).v == '\n') {
                    iter_next(src);
                    return true;
                }
                curr = iter_next(src);
            }
            // reached EOF in a comment
            return true;
        } else if (d == '\n') {
            iter_next(src);
            return true;
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_next(src);
            iter_next(src);
            return true;
        } else if (d == ' ' || d == '\t') {
            iter_next(src);
        } else {
            // read non-whitespace
            return false;
        }
    }
    // read whitespace until EOF
    return true;
}

struct key_result parse_key(struct str_iter *src) {
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
        dst->cap *= 2;
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

void clear_keys_result(struct keys_result *dst) {
    if (dst) {
        if (dst->v) {
            for (size_t i = 0; i < dst->len; i++) {
                free_str_buf(&dst->v[i]);
            }
            free(dst->v);
        }
        if (dst->err) {
            free(dst->err);
        }
        dst->err = NULL;
        dst->v = NULL;
        dst->len = 0;
        dst->cap = 0;
    }
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
