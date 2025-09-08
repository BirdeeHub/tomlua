// Copyright 2025 Birdee
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

static inline size_t lua_arraylen(lua_State *L, int idx) {
#if LUA_VERSION_NUM == 501
    return lua_objlen(L, idx);
#else
    return lua_rawlen(L, idx);
#endif
}

typedef struct {
    size_t len;
    size_t cap;
    unsigned char *data;
} embed_buf;

static inline void free_embed_buf(embed_buf *buf) {
    if (buf) {
        if (buf->data) free(buf->data);
        buf->data = NULL;
        buf->len = buf->cap = 0;
    }
}

static inline void buf_soft_reset(embed_buf *buf) {
    if (buf) {
        buf->data[0] = '\0';
        buf->len = 0;
    }
}

static inline embed_buf new_embed_buf() {
    return ((embed_buf) {
        .len = 0,
        .cap = 16,
        .data = (unsigned char *)malloc(16 * sizeof(char))
    });
}

static inline int embed_buf_push_str(embed_buf *buf, const unsigned char *str, size_t len) {
    if (!buf || !str) return 0;
    size_t required_len = buf->len + len;
    if (required_len > buf->cap) {
        size_t new_capacity = buf->cap > 0 ? buf->cap : 1;
        while (new_capacity < required_len) {
            new_capacity *= 2;
        }
        unsigned char *tmp = (unsigned char *)realloc(buf->data, new_capacity * sizeof(unsigned char));
        if (!tmp) return 0;
        buf->data = tmp;
        buf->cap = new_capacity;
    }

    memcpy(buf->data + buf->len, str, len);
    buf->len += len;

    return 1;
}

static int writer(lua_State *L, const void *p, size_t sz, void *ud) {
    embed_buf *buf = (embed_buf*)ud;
    embed_buf_push_str(buf, p, sz);
    return 0;
}

static int embed_run(lua_State *L) {
    const int output_to_stack = lua_toboolean(L, 1);
    const char *output_file = lua_tostring(L, lua_upvalueindex(1));
    const char *c_func_name = lua_tostring(L, lua_upvalueindex(2));
    const char *header_name = lua_tostring(L, lua_upvalueindex(3));

    size_t num_inputs = lua_arraylen(L, lua_upvalueindex(4));

    embed_buf buf = new_embed_buf();
    FILE *out = fopen(output_file, "wb");
    if (!out) return luaL_error(L, "failed to open output");

    if (header_name) {
        fprintf(out, "#ifndef %s\n#define %s\n\n", header_name, header_name);
    }
    fprintf(out, "#include <lua.h>\n#include <lauxlib.h>\n\n");
    fprintf(out, "static inline int %s(lua_State *L) {\n", c_func_name);
    if (!output_to_stack) {
        fprintf(out, "  lua_newtable(L);\n");
        fprintf(out, "  int out_table_idx = lua_gettop(L);\n");
    }

    for (size_t cidx = num_inputs; cidx > 0; cidx--) {
        lua_rawgeti(L, lua_upvalueindex(4), cidx);
        lua_rawgeti(L, -1, 1);
        const char *f_name = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        const char *input_file = lua_tostring(L, -1);
        lua_pop(L, 2);

        // load Lua chunk as a function
        if (luaL_loadfile(L, input_file) != 0) {
            const char *err = lua_tostring(L, -1);
            free_embed_buf(&buf);
            fclose(out);
            return luaL_error(L, "failed to load Lua file: %s", err);
        }

        buf_soft_reset(&buf);
        if (lua_dump(L, writer, &buf)) {
            free_embed_buf(&buf);
            fclose(out);
            return luaL_error(L, "Failed to dump Lua bytecode");
        }
        lua_pop(L, 1);

        fprintf(out, "  {\n");
        fprintf(out, "    const unsigned char data[] = {\n      ");
        for (size_t i = 0; i < buf.len; i++) {
            fprintf(out, "0x%02X", buf.data[i]);
            if (i + 1 < buf.len) fprintf(out, ",%s", ((i + 1) % 8 != 0) ? " " : "");
            if ((i + 1) % 8 == 0 && i + 1 < buf.len) fprintf(out, "\n      ");
        }
        fprintf(out, "\n    };\n");
        fprintf(out, "    const size_t len = %zu;\n", buf.len);
        fprintf(out, "    if (luaL_loadbuffer(L, (const char *)data, len, \"%s\")) {\n", f_name);
        fprintf(out, "        const char *err = lua_tostring(L, -1);\n");
        fprintf(out, "        lua_pop(L, 1);\n");
        fprintf(out, "        return luaL_error(L, \"Error loading embedded Lua code for %s from function %s: %%s\", err);\n", f_name, c_func_name);
        fprintf(out, "    }\n");
    if (!output_to_stack) {
        fprintf(out, "    lua_setfield(L, out_table_idx, \"%s\");\n", f_name);
    }
        fprintf(out, "  }\n");
    }

    if (output_to_stack) {
        fprintf(out, "  return %zu;\n", num_inputs);
    } else {
        fprintf(out, "  return 1;\n");
    }
    fprintf(out, "}\n");
    if (header_name) {
        fprintf(out, "\n#endif  // %s\n", header_name);
    }
    free_embed_buf(&buf);
    fclose(out);
    return 0;
}

static int embed_add(lua_State *L) {
    if (!lua_isstring(L, 1)) return luaL_error(L,
        "invalid first argument to embed.add!\n"
        "Expected string `name` (used for output table and for debug info when calling luaL_loadbuffer)"
    );
    if (!lua_isstring(L, 2)) return luaL_error(L, "invalid second argument to embed.add!\nExpected string `input_path`");
    size_t len = lua_arraylen(L, lua_upvalueindex(1));
    lua_newtable(L);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, 2);
    lua_rawseti(L, lua_upvalueindex(1), len + 1);
    lua_settop(L, 0);
    return 0;
}

static const char *USEAGE_MESSAGE = "Useage:\n"
    "local embed = require('embed_lua')(output_header_file, c_func_name, header_name?)\n"
    "-- if header_name is nil, it will not be made into a header file\n"
    "embed.add('file1', 'path/to/file1.lua')\n"
    "embed.add('dir.file2', 'path/to/dir/file2.lua')\n"
    "embed.run()\n"
    "-- or run(true) for returning directly on the stack instead of table, first on top\n";

static int embed_new(lua_State *L) {
    for (int i = 1; i <= 3; i++) {
        int type = lua_type(L, i);
        if (i == 3 && type != LUA_TNIL && type != LUA_TSTRING) {
            return luaL_error(L, "invalid argument #%d, expected string or nil.\n%s", i, USEAGE_MESSAGE);
        } else if (type != LUA_TSTRING) {
            return luaL_error(L, "invalid argument #%d, expected string.\n%s", i, USEAGE_MESSAGE);
        };
    }
    lua_newtable(L);
    lua_insert(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, embed_add, 1);
    lua_setfield(L, 1, "add");
    lua_pushcclosure(L, embed_run, 4);
    lua_setfield(L, 1, "run");
    return 1;
}

int luaopen_embed_lua(lua_State *L) {
    lua_pushcfunction(L, embed_new);
    return 1;
}
