#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode.h"

static int tomlua_new(lua_State *L) {
    // arg 1 = options or nil
    lua_newtable(L); // module table

    // upvalue 1: error object
    new_TMLErr(L);

    // upvalue 2: options
    if (lua_istable(L, 1)) {
        lua_pushvalue(L, 1);
    } else {
        lua_newtable(L);
    }
     // duplicate pointers for both closures
    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    // stack: module, err, err, opts, opts

    // decode
    lua_pushcclosure(L, tomlua_decode, 2); // pops err+opts, adds function
    lua_setfield(L, -4, "decode");

    // encode
    lua_pushcclosure(L, tomlua_encode, 2); // pops err+opts, adds function
    lua_setfield(L, -2, "encode");

    // module table is at -1 now
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    if (sizeof(lua_Integer) < 8) return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    lua_pushcfunction(L, tomlua_new);
    return 1;
}
