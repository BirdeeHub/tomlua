// Copyright 2025 Birdee
#ifndef SRC_TYPES_H_
#define SRC_TYPES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifndef LUA_MAXINTEGER
#define LUA_MAXINTEGER LLONG_MAX
#endif

#ifndef LUA_MININTEGER
#define LUA_MININTEGER LLONG_MIN
#endif

// NOTE: just for debugging
#include <stdio.h>
static void print_lua_stack(lua_State *L, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("=== Stack: ");
    vprintf(fmt, args);
    printf(" ===\n");
    va_end(args);
    const size_t MAX_DISPLAY_LEN = 256;
    int top = lua_gettop(L);
    for (int i = top; i >= 1; i--) {
        int t = lua_type(L, i);
        const char *type_name = lua_typename(L, t);
        int neg_index = i - top - 1;  // e.g. i=top => -1, i=top-1 => -2
        printf("%d (%d): %s", i, neg_index, type_name);
        if (t == LUA_TSTRING || t == LUA_TNUMBER) {
            size_t len;
            const char *s = lua_tolstring(L, i, &len);
            if (t == LUA_TSTRING && len > MAX_DISPLAY_LEN) {
                printf(" -> 'string[%zu]'", len);  // just show length for long strings
            } else {
                printf(" -> '%.*s'", (int)len, s);
            }
        }
        printf("\n");
    }
    printf("===================\n");
}

typedef enum {
    TOML_UNTYPED,  // Untyped TOML Item
    TOML_STRING,  // lua string
    TOML_STRING_MULTI,  // lua string
    TOML_INTEGER,  // lua number
    TOML_FLOAT,  // lua number
    TOML_BOOL,  // lua bool
    TOML_ARRAY,  // lua table
    TOML_TABLE,  // lua table
    TOML_ARRAY_INLINE,  // same, except display forced to be inline
    TOML_TABLE_INLINE,  // same, except display forced to be inline
    TOML_LOCAL_DATE,  // string for now
    TOML_LOCAL_TIME,  // string for now
    TOML_LOCAL_DATETIME,  // string for now
    TOML_OFFSET_DATETIME,  // string for now
    TOML_MAX_TYPES  // contains the length of this enum
} TomlType;
static const char *toml_type_names[TOML_MAX_TYPES] = {
    "UNTYPED",
    "STRING",
    "STRING_MULTI",
    "INTEGER",
    "FLOAT",
    "BOOL",
    "ARRAY",
    "TABLE",
    "ARRAY_INLINE",
    "TABLE_INLINE",
    "LOCAL_DATE",
    "LOCAL_TIME",
    "LOCAL_DATETIME",
    "OFFSET_DATETIME",
};
static inline bool is_valid_toml_type(lua_Number t) {
    return (t >= 0 && t < TOML_MAX_TYPES && t == (lua_Number)(lua_Integer)t);
}
static const char *toml_type_to_lua_name(int t) {
    if (t > 0 && t < TOML_MAX_TYPES) {
        return toml_type_names[t];
    } else {
        return "UNTYPED";
    }
}
static inline TomlType toml_type_from_name(const char * name) {
    for (int i = 1; i < TOML_MAX_TYPES; i++) {
        if (strcmp(name, toml_type_names[i]) == 0) return i;
    }
    return TOML_UNTYPED;
}

static inline int absindex(int top, int idx) {
    if (idx > 0 || idx <= LUA_REGISTRYINDEX)  // already absolute or special
        return idx;
    return top + idx + 1;
}

static inline bool is_hex_char(uint32_t c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

static inline bool is_identifier_char(uint32_t c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '_') || (c == '-');
}

static inline size_t lua_arraylen(lua_State *L, int idx) {
#if LUA_VERSION_NUM == 501
    return lua_objlen(L, idx);
#else
    return lua_rawlen(L, idx);
#endif
}

// Assumes you know it is a udata
static inline bool udata_is_of_type(lua_State *L, int idx, const char* name) {
    if (lua_getmetatable(L, idx)) {
        luaL_getmetatable(L, name);
        if (lua_rawequal(L, -1, -2)) {
            lua_pop(L, 2);
            return true;
        }
        lua_pop(L, 2);
    }
    return false;
}

typedef struct {
    size_t len;
    size_t cap;
    char *data;
} str_buf;

// NOTE: str_iter does NOT own its string
// make sure whatever resource you initialize str_iter with
// is still valid for the lifetime required of the iterator.
// This lack of ownership allows us to avoid copying strings from lua unnecessarily.
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
    bool ok;
    uint32_t v;
} iter_utf8_result;

static inline void free_str_buf(str_buf *buf) {
    if (buf) {
        if (buf->data) free(buf->data);
        buf->data = NULL;
        buf->len = buf->cap = 0;
    }
}

static int lbuf_gc(lua_State *L) {
    free_str_buf((str_buf *)lua_touserdata(L, 1));
    return 0;
}

static int lbuf_tostring(lua_State *L) {
    str_buf *buf = (str_buf *)lua_touserdata(L, 1);
    if (!buf || !buf->data) lua_pushliteral(L, "");
    else lua_pushlstring(L, buf->data, buf->len);
    return 1;
}

static inline int push_multi_string_mt(lua_State *L) {
    if (luaL_newmetatable(L, "TomluaMultiStr")) {
        lua_pushcfunction(L, lbuf_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, lbuf_gc);
        lua_setfield(L, -2, "__gc");
    }
    return 1;
}

static inline void buf_soft_reset(str_buf *buf) {
    buf->len = 0;
}

static inline str_buf new_str_buf() {
    str_buf res = (str_buf) {
        .len = 0,
        .cap = 16,
        .data = (char *)malloc(16 * sizeof(char))
    };
    if (!res.data) res.cap = 0;
    return res;
}

static inline str_buf new_buf_from_str(const char *str, size_t len) {
    // set cap to at least len, rounded up to power-of-2
    size_t cap = 16;
    while (cap < len) cap *= 2;
    char * tmp = (char *)malloc(cap * sizeof(char));
    if (!tmp) {
        return ((str_buf) {0});
    }
    memcpy(tmp, str, len);
    return ((str_buf) {
        .len = len,
        .cap = cap,
        .data = tmp,
    });
}

static bool buf_push(str_buf *buf, char c) {
    if (!buf) return false;
    if (buf->len >= buf->cap) {
        size_t new_capacity = buf->cap > 0 ? buf->cap * 2 : 1;
        char *tmp = (char *)realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->cap = new_capacity;
    }
    buf->data[buf->len++] = c;
    return true;
}

static bool buf_push_str(str_buf *buf, const char *str, size_t len) {
    if (!buf || !str) return false;
    size_t required_len = buf->len + len;
    if (required_len > buf->cap) {
        size_t new_capacity = buf->cap > 0 ? buf->cap : 1;
        while (new_capacity < required_len) new_capacity *= 2;
        char *tmp = (char *)realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->cap = new_capacity;
    }

    memcpy(buf->data + buf->len, str, len);
    buf->len += len;

    return true;
}

static bool buf_null_terminate(str_buf *buf) {
    if (!buf || buf->len == 0) return false;
    if (buf->len >= buf->cap) {
        size_t new_capacity = buf->cap > 0 ? buf->cap * 2 : 1;
        char *tmp = (char *)realloc(buf->data, new_capacity * sizeof(char));
        if (!tmp) return false;
        buf->data = tmp;
        buf->cap = new_capacity;
    }
    buf->data[buf->len] = '\0';
    return true;
}

static inline bool push_buf_to_lua_string(lua_State *L, const str_buf *buf) {
    if (!buf || !buf->data) return false;
    lua_pushlstring(L, buf->data, buf->len);
    return true;
}

static inline str_iter lua_str_to_iter(lua_State *L, int idx) {
    str_iter res = {0};
    res.buf = lua_tolstring(L, idx, &res.len);
    return res;
}

static inline str_iter str_to_iter(const char *str, size_t len) {
    return ((str_iter) { .len = len, .pos = 0, .buf = str });
}

static bool iter_starts_with(const str_iter *a, char *b, size_t len) {
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

static inline void iter_reset_pos(str_iter *iter) {
    iter->pos = 0;
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

static inline bool buf_push_utf8(str_buf *dst, uint32_t cp) {
    char buf[4];
    size_t len = 0;

    if (cp <= 0x7F) {
        buf[0] = cp & 0x7F;
        len = 1;
    } else if (cp <= 0x7FF) {
        buf[0] = 0xC0 | ((cp >> 6) & 0x1F);
        buf[1] = 0x80 | (cp & 0x3F);
        len = 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = 0xE0 | ((cp >> 12) & 0x0F);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        len = 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = 0xF0 | ((cp >> 18) & 0x07);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        len = 4;
    } else {
        // replacement character U+FFFD
        buf[0] = (char)0xEF;
        buf[1] = (char)0xBF;
        buf[2] = (char)0xBD;
        len = 3;
    }

    return buf_push_str(dst, buf, len);
}

static inline iter_utf8_result iter_next_utf8(str_iter *iter) {
    iter_utf8_result res = {0};
    if (!iter || !iter->buf || iter->pos >= iter->len) {
        return res;
    }

    // pointer to subsection of buffer
    const unsigned char *s = (const unsigned char *)iter->buf + iter->pos;
    size_t remaining = iter->len - iter->pos;

    unsigned char c = s[0];
    uint32_t cp;
    size_t nbytes;

    if (c < 0x80) {
        cp = c;
        nbytes = 1;
    } else if ((c & 0xE0) == 0xC0) {
        if (remaining < 2 || (s[1] & 0xC0) != 0x80) return res;
        cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        nbytes = 2;
        if (cp < 0x80) return res;  // overlong
    } else if ((c & 0xF0) == 0xE0) {
        if (remaining < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return res;
        cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        nbytes = 3;
        if (cp < 0x800) return res;  // overlong
    } else if ((c & 0xF8) == 0xF0) {
        if (remaining < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return res;
        cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
             ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        nbytes = 4;
        if (cp < 0x10000 || cp > 0x10FFFF) return res;  // overlong or out of range
    } else {
        return res;  // invalid first byte
    }

    iter->pos += nbytes;
    res.ok = true;
    res.v = cp;
    return res;
}

// consumes whitespace until end of whitespace, line, or file.
// returns true if the end of the line or file was found
// 0 for whitespace ending before end of line
// 1 for end of line, 2 for end of file
// (both 1 and 2 will be read as true in if statements)
static inline int consume_whitespace_to_line(str_iter *src) {
    // skips through the end of the line on trailing comments and failed parses
    while (iter_peek(src).ok) {
        char d = iter_peek(src).v;
        if (d == '#') {
            iter_skip(src);
            iter_result curr = iter_next(src);
            while (curr.ok) {
                if (curr.v == '\n') {
                    return true;
                } else if (curr.v == '\r' && iter_peek(src).v == '\n') {
                    iter_skip(src);
                    return true;
                }
                curr = iter_next(src);
            }
            // reached EOF in a comment
            return 2;
        } else if (d == '\n') {
            iter_skip(src);
            return true;
        } else if (iter_starts_with(src, "\r\n", 2)) {
            iter_skip_n(src, 2);
            return true;
        } else if (d == ' ' || d == '\t') {
            iter_skip(src);
        } else {
            // read non-whitespace
            return false;
        }
    }
    // read whitespace until EOF
    return 2;
}

#endif  // SRC_TYPES_H_
