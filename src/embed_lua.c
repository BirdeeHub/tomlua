#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include "types.h" // provides str_buf type

int writer(lua_State *L, const void *p, size_t sz, void *ud) {
    str_buf *buf = (str_buf*)ud;
    buf_push_str(buf, p, sz);
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

    str_buf buf = new_str_buf();
    int err = lua_dump(L, writer, &buf);

    fprintf(out, "#ifndef %s_H_\n#define %s_H_\n\n#include <lua.h>\n#include <lauxlib.h>\n\n", header_name, header_name);
    fprintf(out, "static inline int push_embedded_%s(lua_State *L) {\n", var_name);
    fprintf(out, "\tconst unsigned char data[] = {\n\t\t");

    for (size_t i = 0; i < buf.len; i++) {
        fprintf(out, "0x%02x", (unsigned char)buf.data[i]);
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
    free_str_buf(&buf);

    printf("Generated C array %s in %s\n", var_name, output_file);
    return 0;
}
int luaopen_embed_lua(lua_State *L) {
    lua_pushcfunction(L, embed_run);
    return 1;
}
