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

bool is_identifier_char(char c);
bool consume_whitespace_to_line(struct str_iter *src);
struct key_result parse_key(struct str_iter *src);
void clear_keys_result(struct keys_result *dst);
struct keys_result parse_keys(struct str_iter *src);

#endif  // SRC_PARSE_KEYS_H_
