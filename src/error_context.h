// Copyright 2025 Birdee
#ifndef SRC_ERROR_CONTEXT_H_
#define SRC_ERROR_CONTEXT_H_

#include <lua.h>
#include <lauxlib.h>

#include "./types.h"

// TODO: make a nice feature that allows us to give string and position and get start and end of surrounding X lines or something
// That way we can give errors on decode that include source context
// make the context stuff separate from anything which grabs an upvalue so that we can use it everywhere in the codebase

typedef struct {
    size_t len;
    size_t heap;
    char *msg;
} TMLErr;

static inline void push_err_upval(lua_State *L) {
    lua_pushvalue(L, lua_upvalueindex(1));
}

static inline TMLErr *get_err_upval(lua_State *L) {
    return (TMLErr *)lua_touserdata(L, lua_upvalueindex(1));
}

static inline void free_tmlerr(TMLErr *err) {
    if (err) {
        if (err->heap) {
            free(err->msg);
            err->msg = NULL;
        }
        err->heap = err->len = 0;
    }
}

static inline int push_tmlerr_string(lua_State *L, TMLErr *err) {
    if (!err || !err->msg) {
        lua_pushliteral(L, "Error: (no message)");
    } else {
        lua_pushlstring(L, err->msg, err->len);
    }
    return 1;
}

static int tmlerr_gc(lua_State *L) {
    free_tmlerr((TMLErr *)luaL_checkudata(L, 1, "TomluaError"));
    return 0;
}
static int tmlerr_tostring_meta(lua_State *L) {
    return push_tmlerr_string(L, (TMLErr *)luaL_checkudata(L, 1, "TomluaError"));
}
static int new_TMLErr(lua_State *L) {
    TMLErr *lasterr = (TMLErr *)lua_newuserdata(L, sizeof(TMLErr));
    *lasterr = (TMLErr){0};
    if (luaL_newmetatable(L, "TomluaError")) {
        lua_pushcfunction(L, tmlerr_tostring_meta);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, tmlerr_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static inline bool set_tmlerr(TMLErr *err, size_t heap_size, size_t len, char *msg) {
    if (err->heap) free(err->msg);  // clears previous message if heap allocated
    err->heap = heap_size;
    err->msg = msg;
    err->len = len;
    return false;  // returns `not ok` as a convenience
}

static bool set_err_upval(lua_State *L, size_t heap_size, size_t len, char *msg) {
    return set_tmlerr(get_err_upval(L), heap_size, len, msg);
}

static inline bool tmlerr_push(TMLErr *err, char c) {
    if (!err) return false;
    if (!err->heap) {
        if (err->len == 0) {
            int cap = 16;
            char *tmp = (char *)malloc(cap * sizeof(char));
            if (!tmp) return false;
            err->heap = cap;
            err->msg = tmp;
        } else {
            // was static string, copy into a heap buffer
            size_t cap = 16;
            while (cap < err->len + 1) cap *= 2;
            char *tmp = (char *)malloc(cap * sizeof(char));
            if (!tmp) return false;
            memcpy(tmp, err->msg, err->len);
            err->msg = tmp;
            err->heap = cap;
        }
    }
    if (err->len >= err->heap) {
        size_t new_capacity = err->heap > 0 ? err->heap * 2 : 1;
        char *tmp = (char *)realloc(err->msg, new_capacity * sizeof(char));
        if (!tmp) {
            return false;
        }
        err->msg = tmp;
        err->heap = new_capacity;
    }
    err->msg[err->len++] = c;
    return true;
}

static inline bool err_push(lua_State *L, char c) {
    return tmlerr_push(get_err_upval(L), c);
}

static inline bool tmlerr_push_str(TMLErr *err, const char *str, size_t len) {
    if (!err || !str) return false;

    if (!err->heap) {
        if (err->len == 0) {
            size_t cap = 16;
            while (cap < len) cap *= 2;
            char *tmp = (char *)malloc(cap * sizeof(char));
            if (!tmp) return false;
            err->msg = tmp;
            err->heap = cap;
        } else {
            // was static string, copy into a new heap buffer
            size_t cap = 16;
            while (cap < err->len + len) cap *= 2;
            char *tmp = (char *)malloc(cap * sizeof(char));
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
        char *tmp = (char *)realloc(err->msg, new_capacity * sizeof(char));
        if (!tmp) return false;
        err->msg = tmp;
        err->heap = new_capacity;
    }

    memcpy(err->msg + err->len, str, len);
    err->len += len;

    return true;
}
static inline bool err_push_str(lua_State *L, const char *str, size_t len) {
    return tmlerr_push_str(get_err_upval(L), str, len);
}

static inline bool tmlerr_push_buf(TMLErr *err, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    return tmlerr_push_str(err, buf->data, buf->len);
}

static inline bool err_push_buf(lua_State *L, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    return err_push_str(L, buf->data, buf->len);
}

#endif  // SRC_ERROR_CONTEXT_H_
