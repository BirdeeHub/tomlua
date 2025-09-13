// Copyright 2025 Birdee
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

static int env__newindex(lua_State *L) {
    const char *key = luaL_checkstring(L, 2);
    switch (lua_type(L, 3)) {
        case LUA_TNIL:
#ifdef _WIN32
            if (SetEnvironmentVariable(key, NULL) == 0)
#else
            if (unsetenv(key) != 0)
#endif
                return luaL_error(L, "failed to unset env var");
            break;
        case LUA_TSTRING:
#ifdef _WIN32
            if (SetEnvironmentVariable(key, lua_tostring(L, 3)) == 0)
#else
            if (setenv(key, lua_tostring(L, 3), 1) != 0)
#endif
                return luaL_error(L, "failed to set env var");
            break;
        default: return luaL_error(L, "env values must be strings or nil");
    }
    return 0;
}
static int env__index(lua_State *L) {
    const char *val = getenv(luaL_checkstring(L, 2));
    if (val) lua_pushstring(L, val);
    else lua_pushnil(L);
    return 1;
}
int luaopen_tomlua_env(lua_State *L) {
    lua_newuserdata(L, 0);
    if (luaL_newmetatable(L, "LUA_ENV_VAR_HELPER")) {
        lua_pushcfunction(L, env__index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, env__newindex);
        lua_setfield(L, -2, "__newindex");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static inline int embed_run(lua_State *L) {
    // 1: output_file: string
    const char *output_file = luaL_checkstring(L, 1);
    // 2: to_append?: bool
    const int to_append = lua_toboolean(L, 2);
    // 3: header_name?: string
    const char *header_name = lua_tostring(L, 3);
#if LUA_VERSION_NUM == 501
    size_t num_inputs = lua_objlen(L, lua_upvalueindex(1));
#else
    size_t num_inputs = lua_rawlen(L, lua_upvalueindex(1));
#endif
    // TODO: This needs to identify items with the same c_func_name
    // and output them into the same function
    FILE *out = fopen(output_file, (to_append) ? "ab" : "wb");
    if (!out) return luaL_error(L, "failed to open output");
    if (header_name) {
        fprintf(out, "#ifndef %s\n#define %s\n\n", header_name, header_name);
    }
    fprintf(out, "#include <lua.h>\n#include <lauxlib.h>\n\n");
    for (size_t cidx = num_inputs; cidx > 0; cidx--) {
        lua_settop(L, 0);
        lua_rawgeti(L, lua_upvalueindex(1), cidx);
        lua_pushnil(L);
        lua_rawseti(L, lua_upvalueindex(1), cidx);
        lua_getfield(L, 1, "modname");
        const char *f_name = lua_tostring(L, -1);
        lua_getfield(L, 1, "c_fn_name");
        const char *c_func_name;
        int output_as_table = 0;
        if (lua_istable(L, -1)) {
            output_as_table = 1;
            lua_rawgeti(L, -1, 1);
            c_func_name = lua_tostring(L, -1);
        } else {
            c_func_name = lua_tostring(L, -1);
        }
        lua_getfield(L, 1, "chunk");
        size_t len = 0;
        const char *chunk = lua_tolstring(L, -1, &len);

        fprintf(out, "%sint %s(lua_State *L) {\n", (header_name) ? "static " : "", c_func_name);
        if (output_as_table) {
            fprintf(out, "  lua_newtable(L);\n");
            fprintf(out, "  int out_table_idx = lua_gettop(L);\n");
        }
        fprintf(out, "  {\n");
        fprintf(out, "    const unsigned char data[] = {\n      ");
        for (size_t i = 0; i < len; i++) {
            fprintf(out, "0x%02X", (const unsigned char)chunk[i]);
            if (i + 1 < len) fprintf(out, ",%s", ((i + 1) % 8 != 0) ? " " : "");
            if ((i + 1) % 8 == 0 && i + 1 < len) fprintf(out, "\n      ");
        }
        fprintf(out, "\n    };\n");
        fprintf(out, "    const size_t len = %zu;\n", len);
        fprintf(out, "    if (luaL_loadbuffer(L, (const char *)data, len, \"%s\")) {\n", f_name);
        fprintf(out, "        const char *err = lua_tostring(L, -1);\n");
        fprintf(out, "        lua_pop(L, 1);\n");
        fprintf(out, "        return luaL_error(L, \"Error loading embedded Lua for %s from function %s: %%s\", err);\n", f_name, c_func_name);
        fprintf(out, "    }\n");
        if (output_as_table) {
            fprintf(out, "    lua_setfield(L, out_table_idx, \"%s\");\n", f_name);
        }
        fprintf(out, "  }\n");
        fprintf(out, "  return %zu;\n", (output_as_table) ? 1 : num_inputs);
        fprintf(out, "}\n");
    }

    if (header_name) {
        fprintf(out, "\n#endif  // %s\n", header_name);
    }
    fflush(out);
    fclose(out);
    return 0;
}

static inline int embed_writer(lua_State *L, const void *p, size_t sz, void *ud) {
    if (!p || !ud) return LUA_ERRERR;
    luaL_addlstring((luaL_Buffer *)ud, (const char *)p, sz);
    return 0;
}

static inline int embed_add(lua_State *L) {
    static const char *EMBED_USEAGE_MESSAGE = "invalid argument #%d to embed.add!\n"
        "Expected add(modname: string, path: string, c_func_name?: string|string[1]?)\n";
    if (!lua_isstring(L, 1)) return luaL_error(L, EMBED_USEAGE_MESSAGE, 1);
    if (!lua_isstring(L, 2)) return luaL_error(L, EMBED_USEAGE_MESSAGE, 2);
    int type = lua_type(L, 3);
    if (type != LUA_TNIL && type != LUA_TNONE && type != LUA_TSTRING && type != LUA_TTABLE)
        return luaL_error(L, EMBED_USEAGE_MESSAGE, 3, "string|string[1]?");
    else if (type == LUA_TNONE) lua_pushnil(L);
    lua_newtable(L);
    int residx = lua_gettop(L);
    lua_pushvalue(L, 1);
    lua_setfield(L, residx, "modname");
    if (type == LUA_TSTRING || type == LUA_TTABLE) {
        lua_pushvalue(L, 3);
        lua_setfield(L, residx, "c_fn_name");
    } else {
        int og_type = lua_type(L, lua_upvalueindex(1));
        if (og_type == LUA_TSTRING || og_type == LUA_TTABLE) {
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_setfield(L, residx, "c_fn_name");
        } else {
            // calculate luaopen_mod_path from arg 1 mod.path
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            luaL_addstring(&b, "luaopen_");
            for (const char *p = lua_tostring(L, 1); *p; p++)
                luaL_addchar(&b, (*p == '.') ? '_' : *p);
            luaL_pushresult(&b);
            lua_setfield(L, residx, "c_fn_name");
        }
    }

    if (lua_isfunction(L, lua_upvalueindex(2))) {
        lua_pushvalue(L, lua_upvalueindex(2));
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        if (lua_pcall(L, 3, 1, 0)) {
            const char *err = lua_tostring(L, -1);
            return luaL_error(L, "Error calling loader\n%s", err);
        }
        if (!lua_isfunction(L, -1)) {
            return luaL_error(L, "Loader did not return a function (loaded chunk)");
        }
    } else {
        // load Lua chunk as a function
        if (luaL_loadfile(L, lua_tostring(L, 2)) != 0) {
            const char *err = lua_tostring(L, -1);
            return luaL_error(L, "failed to load Lua file %s at %s\n%s", err);
        }
    }
    luaL_Buffer buf;
    luaL_buffinit(L, &buf);
    if (lua_dump(L, embed_writer, &buf)) {
        const char *err = lua_tostring(L, -1);
        return luaL_error(L, "Failed to dump Lua bytecode%s at %s\n%s", err);
    }
    luaL_pushresult(&buf);
    lua_setfield(L, residx, "chunk");

    // push value for run
#if LUA_VERSION_NUM == 501
    size_t len = lua_objlen(L, lua_upvalueindex(3));
#else
    size_t len = lua_rawlen(L, lua_upvalueindex(3));
#endif
    lua_settop(L, residx);
    lua_rawseti(L, lua_upvalueindex(3), len + 1);
    lua_settop(L, 0);
    return 0;
}

static inline int embed_new(lua_State *L) {
    static const char *EMBED_USEAGE_MESSAGE = "invalid argument #%d, expected %s.\nUseage:\n"
        "local embed = require('tomlua.luaembed')(c_func_name?: string|string[1], loader?: fun(name, path) -> function)\n"
        "embed.add(modname: string, path: string, c_func_name?: string|string[1])\n"
        "embed.run(output_file: string, to_append?: bool, header_name?: string)\n";
    int type = lua_type(L, 2);
    if (type != LUA_TNIL && type != LUA_TNONE && type != LUA_TSTRING && type != LUA_TTABLE)
        return luaL_error(L, EMBED_USEAGE_MESSAGE, 1, "string|string[1]?");
    else if (type == LUA_TNONE) lua_pushnil(L);
    type = lua_type(L, 3);
    if (type != LUA_TNIL && type != LUA_TNONE && type != LUA_TFUNCTION)
        return luaL_error(L, EMBED_USEAGE_MESSAGE, 2, "function or nil");
    else if (type == LUA_TNONE) lua_pushnil(L);

    lua_remove(L, 1);
    lua_newtable(L); // module table
    lua_insert(L, 1);

    lua_newtable(L); // keeps the values from add for run

    lua_pushvalue(L, -1); // duplicate the table for run
    lua_pushcclosure(L, embed_run, 1);
    lua_setfield(L, 1, "run");

    lua_pushcclosure(L, embed_add, 3);
    lua_setfield(L, 1, "add");

    if (luaL_newmetatable(L, "C_LUA_EMBEDDER")) {
        lua_pushcfunction(L, env__index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, env__newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, embed_new);
        lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, 1);
    lua_settop(L, 1);
    return 1;
}

int luaopen_tomlua_embed(lua_State *L) {
    lua_newuserdata(L, 0);
    if (luaL_newmetatable(L, "C_LUA_EMBEDDER")) {
        lua_pushcfunction(L, env__index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, env__newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, embed_new);
        lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, -2);
    return 1;
}
