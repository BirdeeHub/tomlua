// Copyright 2025 Birdee
#ifndef SRC_TYPES_H_
#define SRC_TYPES_H_

#include <stddef.h>
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
    for (size_t i = 0; i < len; i++) {
        buf.data[i] = str[i];
    }
    return buf;
}

static inline bool buf_push(str_buf *buf, char c) {
    if (!buf) return false;
    if (buf->len >= buf->capacity) {
        size_t new_capacity = buf->capacity > 0 ? buf->capacity * 2 : 1;
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

    for (size_t i = 0; i < len; i++) {
        buf->data[buf->len + i] = str[i];
    }
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

    for (size_t i = 0; i < len; i++) {
        if (a->buf[a->pos + i] != b[i]) return false;
    }
    return true;
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
    char *msg;
    size_t len;
    bool heap;
} TMLErr;

static int tomlua_gc(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    if (errorval->heap) {
        free(errorval->msg);
    }
    return 0;
}

static int tomlua_tostring(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    lua_pushlstring(L, errorval->msg, errorval->len);
    return 1;
}


static bool set_err_upval(lua_State *L, bool heap, size_t len, char *msg) {
    TMLErr *err = luaL_checkudata(L, lua_upvalueindex(1), "TomluaError");
    err->heap = heap;
    err->msg = msg;
    err->len = len;
    return false;  // returns `ok == true` as a convenience
}

#endif  // SRC_TYPES_H_
