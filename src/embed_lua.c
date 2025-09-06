#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>

int embed_run(lua_State *L) {
    const char *input_file = lua_tostring(L, 1);
    const char *output_file = lua_tostring(L, 2);
    const char *header_name = lua_tostring(L, 3);
    const char *var_name = lua_tostring(L, 4);
    if (!(input_file && output_file && header_name && var_name)) return luaL_error(L, "expected 4 string arguments, input_file, output_file, header_name, var_name");

    FILE *in = fopen(input_file, "rb");
    if (!in) {
        perror("fopen input");
        return luaL_error(L, "");
    }

    fseek(in, 0, SEEK_END);
    size_t size = ftell(in);
    if (size < 0) { perror("ftell"); fclose(in); return luaL_error(L, ""); }
    if (size == 0) { fprintf(stderr, "Input file is empty\n"); fclose(in); return luaL_error(L, ""); }
    fseek(in, 0, SEEK_SET);

    unsigned char *buffer = malloc(size);
    if (!buffer) { perror("malloc"); fclose(in); return luaL_error(L, ""); }

    if (fread(buffer, 1, size, in) != (size_t)size) {
        perror("fread");
        free(buffer);
        fclose(in);
        return luaL_error(L, "");
    }

    FILE *out = fopen(output_file, "w");
    if (!out) {
        perror("fopen output");
        free(buffer);
        return luaL_error(L, "");
    }

    fprintf(out, "#ifndef %s_H_\n#define %s_H_\n\n#include <lua.h>\n#include <lauxlib.h>\n\n", header_name, header_name);
    fprintf(out, "static inline int push_embedded_%s(lua_State *L) {\n", var_name);
    fprintf(out, "\tconst unsigned char data[] = {\n\t\t");

    for (size_t i = 0; i < size; i++) {
        fprintf(out, "0x%02x", buffer[i]);
        if (i + 1 < size) fprintf(out, ", ");
        if ((i + 1) % 8 == 0 && i + 1 < size) fprintf(out, "\n\t\t");
    }
    fprintf(out, "\n\t};\n");

    fprintf(out, "\tconst size_t len = %zu;\n", size);
    fprintf(out, "\tif (luaL_loadbuffer(L, (const char *)data, len, \"%s\")) {\n", var_name);
    fprintf(out, "\t\tconst char *err = lua_tostring(L, -1);\n");
    fprintf(out, "\t\tlua_pop(L, 1);\n");
    fprintf(out, "\t\treturn luaL_error(L, \"Error loading embedded Lua code: %%s\", err);\n");
    fprintf(out, "\t}\n");
    fprintf(out, "\treturn 1;\n");
    fprintf(out, "}\n");

    fprintf(out, "\n#endif  // %s_H_\n", header_name);

    fclose(out);
    free(buffer);

    printf("Generated C array %s in %s\n", var_name, output_file);
    return 0;
}
int luaopen_embed_lua(lua_State *L) {
    lua_pushcfunction(L, embed_run);
    return 1;
}
