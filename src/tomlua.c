#include <lua.h>
#include <lauxlib.h>
#include "decode.h"
#include "encode.h"

int luaopen_tomlua(lua_State *L) {
    // --- 64-bit integer check ---
    if (sizeof(lua_Integer) < 8) {
        return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    }

    lua_newtable(L); // module table
    lua_pushcfunction(L, tomlua_decode);
    lua_setfield(L, -2, "decode");
    lua_pushcfunction(L, tomlua_encode);
    lua_setfield(L, -2, "encode");
    return 1;
}
