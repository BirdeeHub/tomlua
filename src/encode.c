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

static char *encode_table(lua_State *L, struct str_buf *output) {
    // TODO: iterate recursively through tables collecting values to string as you go.
    // This iterates through 1 table, but its not gonna work because of detecting array vs table from lua
    // This first loop probably needs to leave the values on the stack and just check if the keys are all integers with no holes
    // Then a second loop can go ahead and push all those to the output once it knows how to do so.
    // Although maybe a big enough table would then overflow the stack? IDK.
    // This is because I want to detect if table without actually iterating through the whole table twice.
    // I also need to figure out how to output [table] and [[array]] vs doing it inline within nested lists
    // this is because in toml, once you do [[array]] you cant make arrays within the tables in that array in that manner
    // so once you are within [[oneofthese]] you now have to do arrays and tables as inline...
    lua_pushnil(L); // next(nil) // get first kv pair on stack
    while (lua_next(L, -2) != 0) {
        // now at stack: ... table key value
        int key_type = lua_type(L, -2);
        int table_type = lua_type(L, -1);

        if (key_type == LUA_TSTRING) {
            size_t klen;
            const char *k = lua_tolstring(L, -2, &klen);
            buf_push_str(output, k, klen);
        } else if (key_type == LUA_TNUMBER) {
            lua_Number n = lua_tonumber(L, -2);
            char tmp[64];
            int len = snprintf(tmp, sizeof tmp, "%g", n);
            buf_push_str(output, tmp, len);
        }
        buf_push(output, ' ');
        buf_push(output, '=');
        buf_push(output, ' ');

        if (table_type == LUA_TSTRING) {
            size_t vlen;
            const char *v = lua_tolstring(L, -1, &vlen);
            buf_push_str(output, v, vlen);
        } else if (table_type == LUA_TNUMBER) {
            lua_Number n = lua_tonumber(L, -1);
            char tmp[64];
            int len = snprintf(tmp, sizeof tmp, "%g", n);
            buf_push_str(output, tmp, len);
        }
        buf_push(output, '\n');

        lua_pop(L, 1); // pop value, keep key for next lua_next
    }
    return NULL;
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

    char *err = encode_table(L, &output);
    if (err != NULL) {
        lua_pushstring(L, err);
        free(err);
        free_str_buf(&output);
        return lua_error(L);
    };
    lua_pop(L, 1); // pop input table

    if (!push_buf_to_lua_string(L, &output)) {
        free_str_buf(&output);
        return luaL_error(L, "tomlua.encode failed to push result string to lua stack");
    }
    free_str_buf(&output);
    return 1;
}
