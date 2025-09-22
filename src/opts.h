// Copyright 2025 Birdee
#ifndef SRC_OPTS_H_
#define SRC_OPTS_H_

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef enum {
    TOMLOPTS_INT_KEYS,
    TOMLOPTS_FANCY_DATES,
    TOMLOPTS_FANCY_TABLES,
    TOMLOPTS_MULTI_STRINGS,
    TOMLOPTS_MARK_INLINE,
    TOMLOPTS_OVERFLOW_ERRORS,
    TOMLOPTS_UNDERFLOW_ERRORS,
    TOMLOPTS_LENGTH
} TOMLOPTS;
static const char *toml_opts_names[TOMLOPTS_LENGTH] = {
    "int_keys",
    "fancy_dates",
    "fancy_tables",
    "multi_strings",
    "mark_inline",
    "overflow_errors",
    "underflow_errors"
};
typedef bool TomluaUserOpts[TOMLOPTS_LENGTH];

static inline TomluaUserOpts *get_opts_upval(lua_State *L) {
    return (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
}
static inline TomluaUserOpts *toml_user_opts_copy(TomluaUserOpts dst, TomluaUserOpts src) {
    return memcpy(dst, src, sizeof(TomluaUserOpts));
}

// negative taridx means all false
static bool opts_parse(lua_State *L, TomluaUserOpts dst, int taridx, int optidx) {
    if (taridx > 0) {
        luaL_checktype(L, taridx, LUA_TTABLE);
        for (int i = 0; i < TOMLOPTS_LENGTH; i++) {
            lua_getfield(L, taridx, toml_opts_names[i]);
            dst[i] = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }
    } else {
        memset(dst, false, sizeof(TomluaUserOpts));
    }
    for (int i = 0; i < TOMLOPTS_LENGTH; i++) {
        lua_pushboolean(L, dst[i]);
        lua_setfield(L, optidx, toml_opts_names[i]);
    }
    return false;
}
static int opts_call(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    opts_parse(L, *opts, 2, 1);
    return 0;
}
static int opts_index(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = luaL_checkstring(L, 2);
    for (int i = 0; i < TOMLOPTS_LENGTH; i++) {
        if (strcmp(key, toml_opts_names[i]) == 0) {
            lua_pushboolean(L, (*opts)[i]);
            return 1;
        }
    }
    lua_pushnil(L);  // unknown key
    return 1;
}
static int opts_newindex(lua_State *L) {
    TomluaUserOpts *opts = (TomluaUserOpts *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = luaL_checkstring(L, 2);
    int value = lua_toboolean(L, 3);
    lua_pop(L, 1);
    lua_pushboolean(L, value);
    int i = 0;
    while (i < TOMLOPTS_LENGTH) {
        if (strcmp(key, toml_opts_names[i]) == 0) {
            (*opts)[i] = value;
            break;
        }
        i++;
    }
    if (i >= TOMLOPTS_LENGTH) return luaL_error(L, "invalid option '%s'", key);
    // mirror into table so `print(inspect(opts))` shows it
    lua_rawset(L, 1);
    return 0;
}


#endif  // SRC_OPTS_H_
