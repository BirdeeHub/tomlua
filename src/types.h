// Copyright 2025 Birdee
#ifndef SRC_TYPES_H_
#define SRC_TYPES_H_

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

#ifndef __cplusplus
typedef int bool;
#define true 1
#define false 0
#endif

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} str_buf;

typedef struct {
    const char *buf;
    size_t len;
    size_t pos;
} str_iter;
typedef struct {
    char v;
    bool ok;
} iter_result;

static inline str_buf new_str_buf() {
    str_buf buf;
    buf.capacity = 16;
    buf.len = 0;
    buf.data = malloc(buf.capacity * sizeof(char));
    return buf;
}

static inline str_buf new_buf_from_str(const char *str, size_t len) {
    str_buf buf;
    // set capacity to at least len, rounded up to power-of-2
    size_t cap = 16;
    while (cap < len) {
        cap *= 2;
    }
    buf.capacity = cap;
    buf.len = len;
    buf.data = malloc(buf.capacity * sizeof(char));
    memcpy(buf.data, str, len);
    return buf;
}

static inline bool buf_push(str_buf *buf, char c) {
    if (!buf) return false;
    if (buf->len >= buf->capacity) {
        size_t new_capacity = buf->capacity > 0 ? buf->capacity * 2 : 1;
        char *tmp = realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->capacity = new_capacity;
    }
    buf->data[buf->len++] = c;
    return true;
}

static inline bool buf_push_str(str_buf *buf, const char *str, size_t len) {
    if (!buf || !str) return false;
    size_t required_len = buf->len + len;
    if (required_len > buf->capacity) {
        size_t new_capacity = buf->capacity > 0 ? buf->capacity : 1;
        while (new_capacity < required_len) {
            new_capacity *= 2;
        }
        char *tmp = realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->len, str, len);
    buf->len += len;

    return true;
}

static inline bool push_buf_to_lua_string(lua_State *L, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    lua_pushlstring(L, buf->data, buf->len);
    return true;
}

static inline void free_str_buf(str_buf *buf) {
    if (buf) {
        if (buf->data) free(buf->data);
        buf->data = NULL;
        buf->len = buf->capacity = 0;
    }
}

static inline void buf_soft_reset(str_buf *buf) {
    if (buf) {
        buf->data[0] = '\0';
        buf->len = 0;
    }
}

static inline str_iter str_to_iter(const char *str, size_t len) {
    str_iter iter;
    iter.buf = str;
    iter.len = len;
    iter.pos = 0;
    return iter;
}

static inline bool iter_starts_with(const str_iter *a, char *b, size_t len) {
    if (!a || !a->buf || !b) return false;
    if (a->pos + len > a->len) return false;
    return memcmp(a->buf + a->pos, b, len) == 0;
}

static inline iter_result iter_next(str_iter *iter) {
    iter_result res;
    if (!iter || !iter->buf || iter->pos >= iter->len) {
        res.v = '\0';
        res.ok = false;
        return res;
    }
    res.v = iter->buf[iter->pos++];
    res.ok = true;
    return res;
}

static inline void iter_skip(str_iter *iter) {
    iter->pos++;
}

static inline void iter_skip_n(str_iter *iter, unsigned int n) {
    iter->pos += n;
}

static inline iter_result iter_peek(str_iter *iter) {
    iter_result res;
    if (!iter || !iter->buf || iter->pos >= iter->len) {
        res.v = '\0';
        res.ok = false;
        return res;
    }
    res.v = iter->buf[iter->pos];
    res.ok = true;
    return res;
}

typedef struct {
    size_t len;
    size_t heap;
    char *msg;
} TMLErr;

static inline void push_err_upval(lua_State *L) {
    lua_pushvalue(L, lua_upvalueindex(1));
}

static inline TMLErr *get_err_upval(lua_State *L) {
    return luaL_checkudata(L, lua_upvalueindex(1), "TomluaError");
}

static inline bool set_err_upval(lua_State *L, size_t heap_size, size_t len, char *msg) {
    TMLErr *err = get_err_upval(L);
    if (err->heap) free(err->msg);  // clears previous message if heap allocated
    err->heap = heap_size;
    err->msg = msg;
    err->len = len;
    return false;  // returns `not ok` as a convenience
}

static inline bool err_push(lua_State *L, char c) {
    TMLErr *err = get_err_upval(L);
    if (!err) return false;
    if (!err->heap) {
        if (err->len == 0) {
            int cap = 16;
            char *tmp = malloc(cap * sizeof(char));
            if (!tmp) return false;
            err->heap = cap;
            err->msg = tmp;
        } else {
            // was static string, copy into a heap buffer
            size_t cap = 16;
            while (cap < err->len + 1) cap *= 2;
            char *tmp = malloc(cap * sizeof(char));
            if (!tmp) return false;
            memcpy(tmp, err->msg, err->len);
            err->msg = tmp;
            err->heap = cap;
        }
    }
    if (err->len >= err->heap) {
        size_t new_capacity = err->heap > 0 ? err->heap * 2 : 1;
        char *tmp = realloc(err->msg, new_capacity * sizeof(char));
        if (!tmp) {
            return false;
        }
        err->msg = tmp;
        err->heap = new_capacity;
    }
    err->msg[err->len++] = c;
    return true;
}

static inline bool err_push_str(lua_State *L, const char *str, size_t len) {
    TMLErr *err = get_err_upval(L);
    if (!err || !str) return false;

    if (!err->heap) {
        if (err->len == 0) {
            size_t cap = 16;
            while (cap < len) cap *= 2;
            char *tmp = malloc(cap * sizeof(char));
            if (!tmp) return false;
            err->msg = tmp;
            err->heap = cap;
        } else {
            // was static string, copy into a new heap buffer
            size_t cap = 16;
            while (cap < err->len + len) cap *= 2;
            char *tmp = malloc(cap * sizeof(char));
            if (!tmp) return false;
            memcpy(tmp, err->msg, err->len);
            err->msg = tmp;
            err->heap = cap;
        }
    }

    // grow if necessary
    if (err->len + len > err->heap) {
        size_t new_capacity = err->heap;
        while (new_capacity < err->len + len) new_capacity *= 2;
        char *tmp = realloc(err->msg, new_capacity * sizeof(char));
        if (!tmp) return false;
        err->msg = tmp;
        err->heap = new_capacity;
    }

    memcpy(err->msg + err->len, str, len);
    err->len += len;

    return true;
}

#endif  // SRC_TYPES_H_
