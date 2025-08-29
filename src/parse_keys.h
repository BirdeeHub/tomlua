// Copyright 2025 Birdee
#ifndef SRC_PARSE_KEYS_H_
#define SRC_PARSE_KEYS_H_

#include "./str_buf.h"

struct key_result {
    struct str_buf v;
    char *err;
};
struct keys_result {
    size_t cap;
    size_t len;
    char *err;
    struct str_buf *v;
};

struct keys_result parse_keys(struct str_iter *src);

static inline bool is_identifier_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '_') || (c == '-');
}

// consumes whitespace until end of whitespace, line, or file.
// returns true if the end of the line or file was found
// 1 for end of line, 2 for end of file
static inline int consume_whitespace_to_line(struct str_iter *src) {
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
    return 2;  // 2 will also be read as true in if statements
}

static inline void clear_keys_result(struct keys_result *dst) {
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

#endif  // SRC_PARSE_KEYS_H_
