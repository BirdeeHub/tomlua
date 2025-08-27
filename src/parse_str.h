// Copyright 2025 Birdee
#ifndef SRC_PARSE_STR_H_
#define SRC_PARSE_STR_H_

#include "./str_buf.h"

char *parse_basic_string(struct str_buf *dst, struct str_iter *src);
// char *parse_multi_basic_string(struct str_buf *dst, struct str_iter *src);
char *parse_literal_string(struct str_buf *dst, struct str_iter *src);
char *parse_multi_literal_string(struct str_buf *dst, struct str_iter *src);

#endif  // SRC_PARSE_STR_H_
