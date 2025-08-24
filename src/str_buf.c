#include "str_buf.h"
#include "lua.h"
#include <stddef.h>
#include <stdlib.h>

struct str_buf new_str_buf() {
    struct str_buf buf;
    buf.capacity = 16;
    buf.len = 0;
    buf.data = malloc(buf.capacity * sizeof(char));
    return buf;
}

struct str_buf new_buf_from_str(const char *str, size_t len) {
    struct str_buf buf;
    // set capacity to at least len, rounded up to power-of-2
    size_t cap = 16;
    while (cap < len) {
        cap *= 2;
    }
    buf.capacity = cap;
    buf.len = len;
    buf.data = malloc(buf.capacity * sizeof(char));
    for (size_t i = 0; i < len; i++) {
        buf.data[i] = str[i];
    }
    return buf;
}

bool buf_push(struct str_buf *buf, char c) {
    if (!buf) return false;
    if (buf->len >= buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        char *tmp = realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) {
            return false;
        }
        buf->data = tmp;
        buf->capacity = new_capacity;
    }
    buf->data[buf->len++] = c;
    return true;
}

bool buf_push_str(struct str_buf *buf, const char *str, size_t len) {
    if (!buf || !str) return false;
    size_t required_len = buf->len + len;
    if (required_len > buf->capacity) {
        size_t new_capacity = buf->capacity;
        while (new_capacity < required_len) {
            new_capacity *= 2;
        }
        char *tmp = realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->capacity = new_capacity;
    }

    for (size_t i = 0; i < len; i++) {
        buf->data[buf->len + i] = str[i];
    }
    buf->len += len;

    return true;
}

bool buf_append(struct str_buf *dst, const struct str_buf *src) {
    if (!dst || !src) return false;
    size_t required_len = dst->len + src->len;
    if (required_len > dst->capacity) {
        size_t new_capacity = dst->capacity;
        while (new_capacity < required_len) {
            new_capacity *= 2;
        }
        char *tmp = realloc(dst->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        dst->data = tmp;
        dst->capacity = new_capacity;
    }

    for (size_t i = 0; i < src->len; i++) {
        dst->data[dst->len + i] = src->data[i];
    }
    dst->len += src->len;

    return true;
}

bool buf_equals(const struct str_buf *a, const struct str_buf *b) {
    if (!a || !b) return false;
    if (a->len != b->len) return false;

    for (size_t i = 0; i < a->len; i++) {
        if (a->data[i] != b->data[i]) return false;
    }
    return true;
}

void free_str_buf(struct str_buf *buf) {
    if (buf) {
        free(buf->data);
        buf->data = NULL;
        buf->len = buf->capacity = 0;
    }
}

bool push_buf_to_lua_string(lua_State *L, const struct str_buf *buf) {
    if (!buf || !buf->data) return false;
    lua_pushlstring(L, buf->data, buf->len);
    return true;
}

struct str_iter buf_to_iter(struct str_buf buf) {
    struct str_iter iter;
    iter.buf = buf.data;
    iter.len = buf.len;
    iter.pos = 0;
    return iter;
}
struct str_iter str_to_iter(const char *str, size_t len) {
    struct str_iter iter;
    iter.buf = str;
    iter.len = len;
    iter.pos = 0;
    return iter;
}

bool iter_starts_with(const struct str_iter *a, char *b, size_t len) {
    if (!a || !a->buf || !b) return false;
    if (a->pos + len > a->len) return false;

    for (size_t i = 0; i < len; i++) {
        if (a->buf[a->pos + i] != b[i]) return false;
    }
    return true;
}

struct iter_result iter_next(struct str_iter *iter) {
    struct iter_result res;
    if (!iter || !iter->buf || iter->pos >= iter->len) {
        res.v = '\0';
        res.ok = false;
        return res;
    }
    res.v = iter->buf[iter->pos++];
    res.ok = true;
    return res;
}

struct iter_result iter_peek(struct str_iter *iter) {
    struct iter_result res;
    if (!iter || !iter->buf || iter->pos >= iter->len) {
        res.v = '\0';
        res.ok = false;
        return res;
    }
    res.v = iter->buf[iter->pos];
    res.ok = true;
    return res;
}

void free_str_iter(struct str_iter *iter) {
    free(iter);
}
