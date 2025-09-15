// Copyright 2025 Birdee
#ifndef SRC_ERROR_CONTEXT_H_
#define SRC_ERROR_CONTEXT_H_

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef struct {
    size_t len;
    size_t heap;
    char *msg;
} TMLErr;

static void free_tmlerr(TMLErr *err) {
    if (err) {
        if (err->heap) {
            free(err->msg);
            err->msg = NULL;
        }
        err->heap = err->len = 0;
    }
}

static bool set_tmlerr(TMLErr *err, size_t heap_size, size_t len, char *msg) {
    if (err->heap) free(err->msg);  // clears previous message if heap allocated
    err->heap = heap_size;
    err->msg = msg;
    err->len = len;
    return false;  // returns `not ok` as a convenience
}

static bool tmlerr_push(TMLErr *err, char c) {
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

static bool tmlerr_push_str(TMLErr *err, const char *str, size_t len) {
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

// position 0 is the start, and otherwise, ends of lines
// if it reaches the end of src, final position is src_len
// thus, it is best to always read FROM the current position up UNTIL the next position.
// returns the number of positions written
static int get_err_context(size_t positions[], const int max_positions, const size_t pos, const char *src, const size_t src_len) {
    if (pos >= src_len || max_positions < 2) return 0;
    // find start of context
    size_t start = pos;
    int up = max_positions / 2 + max_positions % 2;
    while (start > 0 && up > 0) {
        if (src[start - 1] == '\n') up--;
        if (up > 0) start--;
    }
    // get ends of lines
    positions[0] = start;
    int count = 1;
    for (size_t i = start; i < src_len && count < max_positions; i++)
        if (src[i] == '\n') positions[count++] = i;
    // end was hit rather than max positions.
    // Add position of EOF (indexing may cause error)
    if (count < max_positions) positions[count++] = src_len;
    return count;
}

static bool tmlerr_push_positions(TMLErr *err, const size_t errpos, const size_t positions[], const int num_positions, const char *src, const size_t src_len) {
    if (!err || !positions || num_positions < 2 || !src) return false;
    if (errpos < positions[0] || errpos >= positions[num_positions - 1]) return false;
    if (!tmlerr_push(err, '\n')) return false;
    for (int i = 1; i < num_positions; i++) {
        size_t line_start = positions[i-1];
        size_t line_end   = positions[i];
        size_t line_len   = line_end - line_start;

        // push the line itself
        if (!tmlerr_push_str(err, src + line_start, line_len)) return false;

        // if this line contains the error position
        if (line_start < errpos && errpos <= line_end) {
            if (!tmlerr_push(err, '\n')) return false;

            size_t caret_pos = errpos - line_start - 1;
            for (size_t s = 0; s < line_len + 4; s++) {
                switch ((caret_pos < s) ? s - caret_pos : caret_pos - s) {
                    case 0:
                        if (!tmlerr_push(err, '^')) return false;
                        break;
                    case 1:
                        if (!tmlerr_push(err, ' ')) return false;
                        break;
                    case 2: case 3: case 4:
                        if (!tmlerr_push(err, '*')) return false;
                        break;
                    default:
                        if (s < caret_pos) {
                            if (!tmlerr_push(err, ' ')) return false;
                        } else {
                            s = src_len;
                        }
                }
            }
        }
    }
    if (!tmlerr_push(err, '\n')) return false;
    return true;
}

#include "./types.h"
static bool tmlerr_push_ctx_from_iter(TMLErr *err, int max_lines, const str_iter *src) {
    // this is an error helper. max_lines will always be known at compile time
    // and will always be small, so this warning is not relevant
    // NOLINTNEXTLINE(runtime/arrays)
    size_t positions[max_lines + 1];
    return tmlerr_push_positions(
        err,
        src->pos,
        positions,
        get_err_context(positions, max_lines + 1, src->pos, src->buf, src->len),
        src->buf,
        src->len
    );
}

static bool tmlerr_push_buf(TMLErr *err, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    return tmlerr_push_str(err, buf->data, buf->len);
}

#include <lua.h>
#include <lauxlib.h>
static int push_tmlerr_string(lua_State *L, TMLErr *err) {
    if (!err || !err->msg) {
        lua_pushliteral(L, "Error: (no message)");
    } else {
        lua_pushlstring(L, err->msg, err->len);
    }
    return 1;
}
static TMLErr *get_err_val(lua_State *L, int idx) {
    return (TMLErr *)lua_touserdata(L, idx);
}
static int tmlerr_gc(lua_State *L) {
    free_tmlerr((TMLErr *)luaL_checkudata(L, 1, "TomluaError"));
    return 0;
}
static int tmlerr_tostring_meta(lua_State *L) {
    return push_tmlerr_string(L, (TMLErr *)luaL_checkudata(L, 1, "TomluaError"));
}
static TMLErr *new_tmlerr(lua_State *L) {
    TMLErr *lasterr = (TMLErr *)lua_newuserdata(L, sizeof(TMLErr));
    lua_replace(L, DECODE_ERR_IDX);
    if (luaL_newmetatable(L, "TomluaError")) {
        lua_pushcfunction(L, tmlerr_tostring_meta);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, tmlerr_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return lasterr;
}

#endif  // SRC_ERROR_CONTEXT_H_
