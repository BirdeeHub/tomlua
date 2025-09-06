// Copyright 2025 Birdee
#ifndef SRC_TYPES_H_
#define SRC_TYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

#ifndef __cplusplus
typedef uint8_t bool;
#define true 1
#define false 0
#endif

// TODO: delete this, just for debugging
static void print_lua_stack(lua_State *L, const char *label) {
    int top = lua_gettop(L);
    printf("=== Stack: %s ===\n", label);
    for (int i = top; i >= 1; i--) {
        int t = lua_type(L, i);
        const char *type_name = lua_typename(L, t);
        int neg_index = i - top - 1;  // e.g. i=top => -1, i=top-1 => -2
        printf("%d (%d): %s", i, neg_index, type_name);
        if (t == LUA_TSTRING || t == LUA_TNUMBER) {
            size_t len;
            const char *s = lua_tolstring(L, i, &len);
            printf(" -> '%.*s'", (int)len, s);
        }
        printf("\n");
    }
    printf("===================\n");
}

static inline int absindex(int top, int idx) {
    if (idx > 0 || idx <= LUA_REGISTRYINDEX)  // already absolute or special
        return idx;
    return top + idx + 1;
}

static inline bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

typedef struct {
    size_t len;
    size_t capacity;
    char *data;
} str_buf;

typedef struct {
    size_t len;
    size_t pos;
    const char *buf;
} str_iter;
typedef struct {
    bool ok;
    char v;
} iter_result;

typedef struct {
    bool strict;
    bool enhanced_tables;
} TomluaUserOpts;

static inline TomluaUserOpts *get_opts_upval(lua_State *L) {
    return lua_touserdata(L, lua_upvalueindex(2));
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
    return lua_touserdata(L, lua_upvalueindex(1));
}

static int tomlua_gc(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    if (errorval->heap) {
        free(errorval->msg);
        errorval->msg = NULL;
    }
    errorval->heap = 0;
    errorval->len = 0;
    return 0;
}
static int tomlua_tostring(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    if (!errorval->msg) {
        lua_pushliteral(L, "Error: (no message)");
    } else {
        lua_pushlstring(L, errorval->msg, errorval->len);
    }
    return 1;
}

static int new_TMLErr(lua_State *L) {
    TMLErr *lasterr = lua_newuserdata(L, sizeof(TMLErr));
    lasterr->msg = NULL;
    lasterr->len = 0;
    lasterr->heap = 0;
    if (luaL_newmetatable(L, "TomluaError")) {
        lua_pushcfunction(L, tomlua_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, tomlua_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static inline bool set_err_upval(lua_State *L, size_t heap_size, size_t len, char *msg) {
    TMLErr *err = get_err_upval(L);
    if (err->heap) free(err->msg);  // clears previous message if heap allocated
    err->heap = heap_size;
    err->msg = msg;
    err->len = len;
    return false;  // returns `not ok` as a convenience
}

static inline void move_buf_to_err_upval(lua_State *L, str_buf *buf) {
    set_err_upval(L, buf->capacity, buf->len, buf->data);
    buf->data = NULL;
    buf->len = buf->capacity = 0;
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

static inline bool err_push_buf(lua_State *L, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    return err_push_str(L, buf->data, buf->len);
}

// NOTE: misc str buf and iter functions

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

static inline str_buf new_str_buf() {
    return ((str_buf) {
        .capacity = 16,
        .len = 0,
        .data = malloc(16 * sizeof(char))
    });
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

static inline str_iter str_to_iter(const char *str, size_t len) {
    return ((str_iter) { .buf = str, .len = len, .pos = 0 });
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

#endif  // SRC_TYPES_H_
