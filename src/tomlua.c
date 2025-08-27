#include <lua.h>
#include <lauxlib.h>
#include "parse.h"

int luaopen_tomlua(lua_State *L) {
    // --- 64-bit integer check ---
    if (sizeof(lua_Integer) < 8) {
        return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    }

    lua_newtable(L); // module table
    lua_pushcfunction(L, tomlua_parse);
    lua_setfield(L, -2, "decode");
    return 1;
}
