// Copyright 2025 Birdee
#ifndef SRC_STR_BUF_H_
#define SRC_STR_BUF_H_

#include <stddef.h>

#ifndef __cplusplus
typedef int bool;
#define true 1
#define false 0
#endif

struct str_buf {
    char *data;
    size_t len;
    size_t capacity;
};
struct str_buf new_str_buf(void);
struct str_buf new_buf_from_str(const char *str, size_t len);
void free_str_buf(struct str_buf *buf);
bool buf_push(struct str_buf *buf, const char c);
bool buf_push_str(struct str_buf *buf, const char *str, const size_t len);
bool buf_append(struct str_buf *dst, const struct str_buf *src);
bool buf_equals(const struct str_buf *a, const struct str_buf *b);

struct str_iter {
    const char *buf;
    size_t len;
    size_t pos;
};
struct iter_result {
    char v;
    bool ok;
};
struct str_iter buf_to_iter(struct str_buf buf);
struct str_iter str_to_iter(const char *str, size_t len);
bool iter_starts_with(const struct str_iter *a, char *b, size_t len);
bool iter_is_empty(const struct str_iter *a);
struct iter_result iter_next(struct str_iter *iter);
struct iter_result iter_peek(struct str_iter *iter);
#endif  // SRC_STR_BUF_H_
