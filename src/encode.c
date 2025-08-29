#include <math.h>
#include <string.h>
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

static bool top_is_lua_array(lua_State *L) {
    if (!lua_istable(L, -1)) return false;
    // getmetatable(stack_top).array to allow overriding of representation
    if (lua_getmetatable(L, -1)) {
        // stack: ... table mt
        lua_getfield(L, -1, "array");
        if (!lua_isnil(L, -1)) {
            if (lua_toboolean(L, -1)) {
                lua_pop(L, 2); // pop value + mt
                return true;
            } else {
                lua_pop(L, 2); // pop value + mt
                return false;
            }
        }
        lua_pop(L, 2); // pop value + mt
    }
    bool is_array = true;
    int count = 0;
    lua_Number highest_int_key = 0;
    lua_pushnil(L); // next(nil) // get first kv pair on stack
    while (lua_next(L, -2) != 0) {
        // now at stack: ... table key value
        lua_pop(L, 1); // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != floor(key)) {
                is_array = false;
                break;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            is_array = false;
            break;
        }
    }
    if (highest_int_key != count || count == 0) is_array = false;
    return is_array;
}

static char *encode_table(lua_State *L, struct str_buf *output, bool is_inline, bool is_array) {
    // TODO: iterate recursively through tables pushing to output as you go
    // using `bool buf_push(struct str_buf *buf, char c)` and `bool buf_push_str(struct str_buf *buf, const char *str, size_t len)`
    // I need to figure out how to output [table] and [[array]] vs doing it inline within nested lists
    // this is because in toml, once you do [[array]] you cant make arrays within the tables in that array in that manner
    // so once you are within [[oneofthese]] you now have to do arrays and tables as inline...
    // any errors should be returned as heap allocated strings so that free gets set up to handle dynamic error messages later
    // pass errors along to the caller wherever possible.
    // for clarity of output, when !is_inline delay outputting headings of [[array]] and [table] types until after the others have been added to the current heading.
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        // now at stack: ... table key value
        int keytype = lua_type(L, -2);
        if (keytype != LUA_TSTRING && keytype != LUA_TNUMBER) {
            return strdup("tomlua.encode only supports strings and numbers as table keys");
        }

        // values
        if (!lua_istable(L, -1)) {
            // TODO: use the lua tostring function to get the string representation of the non-table values for safety and to respect metamethods
        } else if (is_array) {
            // TODO: implement array output
        } else {
            // TODO: implement table output
        }
        lua_pop(L, 1);
    }
    return NULL;
}

int tomlua_encode(lua_State *L) {
    // process arguments
    int argno = lua_gettop(L);
    if (argno < 1 || argno > 1) {
        return luaL_error(L, "tomlua.encode expects 1 argument! tomlua.encode(table)");
    }
    if (!lua_istable(L, -1)) {
        return luaL_error(L, "tomlua.encode expects a table as its only argument! tomlua.encode(table)");
    }
    struct str_buf output = new_str_buf();

    char *err = encode_table(L, &output, false, false);
    if (err != NULL) {
        lua_pushstring(L, err);
        free(err);
        free_str_buf(&output);
        return lua_error(L);
    };
    lua_pop(L, 1);

    if (!push_buf_to_lua_string(L, &output)) {
        free_str_buf(&output);
        return luaL_error(L, "tomlua.encode failed to push result string to lua stack");
    }
    free_str_buf(&output);
    return 1;
}
