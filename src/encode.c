#include <math.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "types.h"

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

static bool is_lua_array(lua_State *L, int idx) {
    // probably dont need this check:
    // if (!lua_istable(L, -1)) return false;

    // getmetatable(stack_top).array to allow overriding of representation
    if (lua_getmetatable(L, idx)) {
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
    while (lua_next(L, idx - 1) != 0) {
        // now at stack: key value
        lua_pop(L, 1); // pop value, keep key to check and for next lua_next
        if (lua_isnumber(L, -1)) {
            lua_Number key = lua_tonumber(L, -1);
            if (key < 1 || key != floor(key)) {
                lua_pop(L, 1);  // normally lua_next leaves no values when done, so if we break early, pop
                is_array = false;
                break;
            }
            count++;
            if (key > highest_int_key) highest_int_key = key;
        } else {
            lua_pop(L, 1);
            is_array = false;
            break;
        }
    }
    if (highest_int_key != count || count == 0) is_array = false;
    return is_array;
}

// escapes top of stack value properly to be placed in a toml file
// static char *encode_value(lua_State *L, struct str_buf *output) {
// }

// writes a non-table value to output, escaping properly to be placed in a toml file
// static char *encode_key(lua_State *L, struct str_buf *output) {
// }

typedef struct {
    str_buf prefix;
    size_t prefix_len;
    size_t capacity;
    bool is_inline;
    bool is_array;
} EncodeOpts;

static bool encode_table(lua_State *L, str_buf *output, EncodeOpts opts) {
    // TODO: iterate recursively through tables pushing to output as you go
    // using `bool buf_push(struct str_buf *buf, char c)` and `bool buf_push_str(struct str_buf *buf, const char *str, size_t len)`
    // I need to figure out how to output [table] and [[array]] vs doing it inline within nested lists
    // this is because in toml, once you do [[array]] you cant make arrays within the tables in that array in that manner
    // so once you are within [[oneofthese]] you now have to do arrays and tables as inline...
    // any errors should be returned as heap allocated strings so that free gets set up to handle dynamic error messages later
    // pass errors along to the caller wherever possible.
    // for clarity of output, when !is_inline delay outputting headings of [[array]] and [table] types until after the others have been added to the current heading.
    int headingno = 0;
    // TODO: change iteration strategy if opts.is_array to iterate sequential integer keys instead of unordered
    lua_pushnil(L);
    while (lua_next(L, -2 - headingno * 2) != 0) {
        // now at stack: ... table key value
        int keytype = lua_type(L, -2);
        if (keytype != LUA_TSTRING && keytype != LUA_TNUMBER) {
            return set_err_upval(L, false, 61, "tomlua.encode only supports strings and numbers as table keys");
        }

        // values
        if (!lua_istable(L, -1)) {
            // TODO: use the lua tostring function to get the string representation of the non-table values for safety and to respect metamethods
            // print properly escaped key = value line
            lua_pop(L, 1); // pop value, leave key for next
        } else if (opts.is_inline) {
            if (is_lua_array(L, -1)) {
                // TODO: inline array, encode_table(L, output, is_inline, true) to the recursive call
            } else {
                // TODO: inline table, encode_table(L, output, is_inline, false) to the recursive call
            }
            lua_pop(L, 1); // pop value, leave key for next
        } else {
            headingno++;
            // TODO: maybe instead of saving kv here, we can do is_lua_array and then
            // parse it and push a string onto the stack or store a reference to the string in a c array
            // because currently we are pushing them on, and theyre gonna come off in reverse order
            // so at the very least something is off here
            lua_pushvalue(L, -2); // copy key to top for lua_next, saving k-v on the stack for making headings out of
        }
    }
    printf("%d\n", headingno);
    print_lua_stack(L, "TEST");
    // TODO: go through table and arrays left on stack, number of pairs given by headingno, and output them
    // output them using a heading, if it is an [[array.heading]] you will then pass in is_inline to the recursive call, which will cause the internal lists and tables to be inline instead
    // then finally, pop 1 more to pop off current table
    return true;
}

int tomlua_encode(lua_State *L) {
    // process arguments
    int argno = lua_gettop(L);
    if (argno < 1) {
        return luaL_error(L, "tomlua.encode expects 1 argument! tomlua.encode(table)");
    } else if (argno > 1) {
        lua_pop(L, argno - 1);
    }
    if (!lua_istable(L, -1)) {
        return luaL_error(L, "tomlua.encode expects a table as its only argument! tomlua.encode(table)");
    }
    str_buf output = new_str_buf();

    if(!encode_table(L, &output, (EncodeOpts) {
            .is_inline = false,
            .is_array = false,
            .prefix = NULL,
            .prefix_len = 0,
            .capacity = 0
    })) {
        free_str_buf(&output);
        lua_pushvalue(L, lua_upvalueindex(1));
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
