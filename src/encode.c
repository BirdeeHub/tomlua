#include <lua.h>
#include <lauxlib.h>
#include "str_buf.h"

// TODO: delete this, just for debugging
static void print_lua_stack(lua_State *L, const char *label) {
    int top = lua_gettop(L);
    printf("=== Stack: %s ===\n", label);
    for (int i = top; i >= 1; i--) {
        int t = lua_type(L, i);
        const char *type_name = lua_typename(L, t);
        int neg_index = i - top - 1; // e.g. i=top => -1, i=top-1 => -2
        printf("%d (%d): %s", i, neg_index, type_name);
        if (t == LUA_TSTRING || t == LUA_TNUMBER) {
            size_t len;
            const char *s = lua_tolstring(L, i, &len);
            printf(" -> '%.*s'", (int)len, s);
        }
        printf("\n");
    }
    printf("===================\n");
}

int tomlua_encode(lua_State *L) {
    // process arguments
    int argno = lua_gettop(L);
    if (argno < 1 || argno > 1) {
        return luaL_error(L, "tomlua.encode expects 1 argument! tomlua.encode(target)");
    } else if (!lua_istable(L, -1)) {
        return luaL_error(L, "tomlua.encode expects a table as its only argument! tomlua.encode(target)");
    }
    struct str_buf output = new_str_buf();

    // TODO: iterate through tables collecting values to string as you go.

    if (!push_buf_to_lua_string(L, &output)) {
        free_str_buf(&output);
        return luaL_error(L, "tomlua.encode failed to push string to lua stack");
    }
    free_str_buf(&output);
    return 1;
}
