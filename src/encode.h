// Copyright 2025 Birdee
#ifndef SRC_ENCODE_H_
#define SRC_ENCODE_H_

#include <lua.h>
#include <stddef.h>
#include "./types.h"

int encode(lua_State *L);

// getmetatable(idx).toml_type to allow overriding of representation
static inline TomlType get_meta_toml_type(lua_State *L, int idx) {
    if (luaL_getmetafield(L, idx, "toml_type")) {
        if (lua_isnumber(L, -1)) {
            lua_Number n = lua_tonumber(L, -1);
            if (is_valid_toml_type(n)) {
                lua_pop(L, 1);
                return (TomlType)n;
            }
        } else if (lua_isstring(L, -1)) {
            TomlType t = toml_type_from_name(lua_tostring(L, -1));
            lua_pop(L, 1);
            return t;
        }
        lua_pop(L, 1);
    }
    return TOML_UNTYPED;
}

#endif  // SRC_ENCODE_H_
