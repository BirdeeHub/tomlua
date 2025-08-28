#include <lua.h>
#include <lauxlib.h>
#include "str_buf.h"

int tomlua_encode(lua_State *L) {
    // process arguments
    int argno = lua_gettop(L);
    if (argno < 1 || argno > 1) {
        return luaL_error(L, "tomlua.encode expects 1 argument! tomlua.encode(target)");
    } else if (!lua_istable(L, -1)) {
        return luaL_error(L, "tomlua.encode expects a table as its only argument! tomlua.encode(target)");
    }
    struct str_buf output = new_str_buf();
    // pops and saves the table
    int top = luaL_ref(L, LUA_REGISTRYINDEX);
    // TODO: iterate through tables collecting values to string as you go.
    luaL_unref(L, LUA_REGISTRYINDEX, top);
    if (!push_buf_to_lua_string(L, &output)) {
        free_str_buf(&output);
        return luaL_error(L, "tomlua.encode failed to push string to lua stack");
    }
    free_str_buf(&output);
    return 1;
}
