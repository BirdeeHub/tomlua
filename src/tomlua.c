#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode.h"

int luaopen_tomlua(lua_State *L) {
    if (sizeof(lua_Integer) < 8) return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    lua_newtable(L); // module table

    // upvalue for returning errors without unnecessary runtime allocation.
    new_TMLErr(L);
    // copy it, give to both, each lua_State is single threaded and has a different copy
    lua_pushvalue(L, -1);

    lua_pushcclosure(L, tomlua_decode, 1);
    lua_setfield(L, -3, "decode");

    lua_pushcclosure(L, tomlua_encode, 1);
    lua_setfield(L, -2, "encode");
    return 1;
}
