#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "types.h"
#include "decode.h"
#include "encode.h"

static int tomlua_gc(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    if (errorval->heap) {
        free(errorval->msg);
    }
    return 0;
}
static int tomlua_tostring(lua_State *L) {
    TMLErr *errorval = luaL_checkudata(L, 1, "TomluaError");
    lua_pushlstring(L, errorval->msg, errorval->len);
    return 1;
}

int luaopen_tomlua(lua_State *L) {
    if (sizeof(lua_Integer) < 8) return luaL_error(L, "tomlua requires Lua integers to be 64-bit");
    lua_newtable(L); // module table

    // upvalue for returning errors without unnecessary runtime allocation.
    TMLErr *lasterr = lua_newuserdata(L, sizeof(TMLErr));
    lasterr->msg = "OOM";
    lasterr->len = 3;
    lasterr->heap = false;
    luaL_newmetatable(L, "TomluaError");
    lua_pushcfunction(L, tomlua_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, tomlua_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);

    // copy it, give to both, each lua_State is single threaded and has a different copy
    lua_pushvalue(L, -1);

    lua_pushcclosure(L, tomlua_decode, 1);
    lua_setfield(L, -3, "decode");

    lua_pushcclosure(L, tomlua_encode, 1);
    lua_setfield(L, -2, "encode");
    return 1;
}
