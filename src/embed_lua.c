#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

typedef struct {
    size_t len;
    size_t capacity;
    unsigned char *data;
} embed_buf;

static inline void free_embed_buf(embed_buf *buf) {
    if (buf) {
        if (buf->data) free(buf->data);
        buf->data = NULL;
        buf->len = buf->capacity = 0;
    }
}

static inline embed_buf new_embed_buf() {
    return ((embed_buf) {
        .len = 0,
        .capacity = 16,
        .data = (unsigned char *)malloc(16 * sizeof(char))
    });
}

static inline int embed_buf_push_str(embed_buf *buf, const unsigned char *str, size_t len) {
    if (!buf || !str) return 0;
    size_t required_len = buf->len + len;
    if (required_len > buf->capacity) {
        size_t new_capacity = buf->capacity > 0 ? buf->capacity : 1;
        while (new_capacity < required_len) {
            new_capacity *= 2;
        }
        unsigned char *tmp = (unsigned char *)realloc(buf->data, new_capacity * sizeof(unsigned char));
        if (!tmp) return 0;
        buf->data = tmp;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->len, str, len);
    buf->len += len;

    return 1;
}

int writer(lua_State *L, const void *p, size_t sz, void *ud) {
    embed_buf *buf = (embed_buf*)ud;
    embed_buf_push_str(buf, p, sz);
    return 0;
}

int embed_run(lua_State *L) {
    const char *input_file = lua_tostring(L, 1);
    const char *output_file = lua_tostring(L, 2);
    const char *header_name = lua_tostring(L, 3);
    const char *var_name = lua_tostring(L, 4);
    if (!(input_file && output_file && header_name && var_name)) return luaL_error(L, "expected 4 string arguments, input_file, output_file, header_name, var_name");

    // load Lua chunk as a function first
    if (luaL_loadfile(L, input_file) != 0) {
        const char *err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return luaL_error(L, "failed to load Lua file: %s", err);
    }

    // now the function is on top of the stack
    FILE *out = fopen(output_file, "wb");
    if (!out) return luaL_error(L, "failed to open output");

    embed_buf buf = new_embed_buf();
    if (lua_dump(L, writer, &buf)) {
        free_embed_buf(&buf);
        fclose(out);
        return luaL_error(L, "Failed to dump Lua bytecode");
    }

    fprintf(out, "#ifndef %s_H_\n#define %s_H_\n\n#include <lua.h>\n#include <lauxlib.h>\n\n", header_name, header_name);
    fprintf(out, "static inline int push_embedded_%s(lua_State *L) {\n", var_name);
    fprintf(out, "\tconst unsigned char data[] = {\n\t\t");

    for (size_t i = 0; i < buf.len; i++) {
        fprintf(out, "0x%02x", buf.data[i]);
        if (i + 1 < buf.len) fprintf(out, ", ");
        if ((i + 1) % 8 == 0 && i + 1 < buf.len) fprintf(out, "\n\t\t");
    }
    fprintf(out, "\n\t};\n");

    fprintf(out, "\tconst size_t len = %zu;\n", buf.len);
    fprintf(out, "\tif (luaL_loadbuffer(L, (const char *)data, len, \"%s\")) {\n", var_name);
    fprintf(out, "\t\tconst char *err = lua_tostring(L, -1);\n");
    fprintf(out, "\t\tlua_pop(L, 1);\n");
    fprintf(out, "\t\treturn luaL_error(L, \"Error loading embedded Lua code: %%s\", err);\n");
    fprintf(out, "\t}\n");
    fprintf(out, "\treturn 1;\n");
    fprintf(out, "}\n");

    fprintf(out, "\n#endif  // %s_H_\n", header_name);

    fclose(out);
    free_embed_buf(&buf);

    printf("Generated C array %s in %s\n", var_name, output_file);
    return 0;
}
int luaopen_embed_lua(lua_State *L) {
    lua_pushcfunction(L, embed_run);
    return 1;
}
