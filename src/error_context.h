// Copyright 2025 Birdee
#ifndef SRC_ERROR_CONTEXT_H_
#define SRC_ERROR_CONTEXT_H_

#include <lua.h>
#include <lauxlib.h>

#include "./types.h"

// TODO: make a nice feature that allows us to give string and position and get start and end of surrounding X lines or something
// That way we can give errors on decode that include source context
// make the context stuff separate from the TMLErr type so that we might be able to use it elsewhere

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

static int tomlua_gc(lua_State *L) {
    TMLErr *errorval = (TMLErr *)luaL_checkudata(L, 1, "TomluaError");
    if (errorval->heap) {
        free(errorval->msg);
        errorval->msg = NULL;
    }
    errorval->heap = errorval->len = 0;
    return 0;
}
static int tomlua_tostring(lua_State *L) {
    TMLErr *errorval = (TMLErr *)luaL_checkudata(L, 1, "TomluaError");
    if (!errorval->msg) {
        lua_pushliteral(L, "Error: (no message)");
    } else {
        lua_pushlstring(L, errorval->msg, errorval->len);
    }
    return 1;
}

static int new_TMLErr(lua_State *L) {
    TMLErr *lasterr = (TMLErr *)lua_newuserdata(L, sizeof(TMLErr));
    *lasterr = (TMLErr){0};
    if (luaL_newmetatable(L, "TomluaError")) {
        lua_pushcfunction(L, tomlua_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, tomlua_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static bool set_err_upval(lua_State *L, size_t heap_size, size_t len, char *msg) {
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

static inline bool err_push_str(lua_State *L, const char *str, size_t len) {
    TMLErr *err = get_err_upval(L);
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

static inline bool err_push_buf(lua_State *L, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    return err_push_str(L, buf->data, buf->len);
}

#endif  // SRC_ERROR_CONTEXT_H_
