#include "./argus.h"
#include "./types.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int luaopen_tomlua(lua_State *L);
extern int luaopen_tomlua_env(lua_State *L);

static void inject_tomlua(lua_State *L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    lua_pushcfunction(L, luaopen_tomlua);
    lua_setfield(L, -2, "tomlua");

    lua_pushcfunction(L, luaopen_tomlua_env);
    lua_setfield(L, -2, "tomlua.env");

    lua_pop(L, 2);
}

typedef struct {
    lua_State *L;
    char dirsep, pathsep, pathmark;
    str_buf buf;
    char *cmd;
    char *script;
    char *outpath;
    char **files;
    int files_count;
    int files_cap;
    bool dont_read;
} ArgusCtx;

static void fancy_dates_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_pushlightuserdata(L, ctx);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, ctx);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
    if (has_arg)
        lua_pushboolean(L, strcmp(val, "true") == 0);
    else
        lua_pushboolean(L, true);
    lua_setfield(L, -2, "fancy_dates");
    lua_pop(L, 1);
}

static void ldir_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    size_t cur_len;
    const char *cur = lua_tolstring(L, -1, &cur_len);
    buf_soft_reset(&ctx->buf);
    buf_push_str(&ctx->buf, cur, cur_len);
    buf_push(&ctx->buf, ctx->pathsep);
    buf_push_str(&ctx->buf, val, strlen(val));
    buf_push(&ctx->buf, ctx->dirsep);
    buf_push(&ctx->buf, ctx->pathmark);
    buf_push_str(&ctx->buf, ".lua", 4);
    buf_push(&ctx->buf, ctx->pathsep);
    buf_push_str(&ctx->buf, val, strlen(val));
    buf_push(&ctx->buf, ctx->dirsep);
    buf_push(&ctx->buf, ctx->pathmark);
    buf_push(&ctx->buf, ctx->dirsep);
    buf_push_str(&ctx->buf, "init.lua", 8);
    push_buf_to_lua_string(L, &ctx->buf);
    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
}

static void cdir_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");
    size_t cur_len;
    const char *cur = lua_tolstring(L, -1, &cur_len);
    buf_soft_reset(&ctx->buf);
    buf_push_str(&ctx->buf, cur, cur_len);
    buf_push(&ctx->buf, ctx->pathsep);
    buf_push_str(&ctx->buf, val, strlen(val));
    buf_push(&ctx->buf, ctx->dirsep);
    buf_push(&ctx->buf, ctx->pathmark);
    buf_push_str(&ctx->buf, ".so", 3);
    push_buf_to_lua_string(L, &ctx->buf);
    lua_setfield(L, -3, "cpath");
    lua_pop(L, 2);
}

static void lpath_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    size_t cur_len;
    const char *cur = lua_tolstring(L, -1, &cur_len);
    buf_soft_reset(&ctx->buf);
    buf_push_str(&ctx->buf, cur, cur_len);
    buf_push(&ctx->buf, ctx->pathsep);
    buf_push_str(&ctx->buf, val, strlen(val));
    push_buf_to_lua_string(L, &ctx->buf);
    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
}

static void lcpath_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");
    size_t cur_len;
    const char *cur = lua_tolstring(L, -1, &cur_len);
    buf_soft_reset(&ctx->buf);
    buf_push_str(&ctx->buf, cur, cur_len);
    buf_push(&ctx->buf, ctx->pathsep);
    buf_push_str(&ctx->buf, val, strlen(val));
    push_buf_to_lua_string(L, &ctx->buf);
    lua_setfield(L, -3, "cpath");
    lua_pop(L, 2);
}

static void cmd_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    ctx->cmd = strdup(val);
}

static void script_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    ctx->script = strdup(val);
}

static void output_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    ctx->outpath = strdup(val);
}

static void file_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    if (ctx->files_count >= ctx->files_cap) {
        ctx->files_cap = ctx->files_cap ? ctx->files_cap * 2 : 8;
        ctx->files = realloc(ctx->files, ctx->files_cap * sizeof(char *));
    }
    ctx->files[ctx->files_count++] = strdup(val);
}

static void dont_read_cb(bool has_arg, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    if (has_arg)
        ctx->dont_read = strcmp(val, "false") != 0;
    else
        ctx->dont_read = true;
}

static void default_action_cb(bool has_arg, const char *name, const char *val, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    lua_State *L = ctx->L;
    lua_pushlightuserdata(L, ctx);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, ctx);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
    if (has_arg)
        lua_pushboolean(L, strcmp(val, "false") != 0);
    else
        lua_pushboolean(L, true);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static void positional_cb(const char *val, int index, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    ctx->script = strdup(val);
}

static void parse_end_cb(const char *val, int index, void *userdata) {
    ArgusCtx *ctx = (ArgusCtx *)userdata;
    if (ctx->files_count >= ctx->files_cap) {
        ctx->files_cap = ctx->files_cap ? ctx->files_cap * 2 : 8;
        ctx->files = realloc(ctx->files, ctx->files_cap * sizeof(char *));
    }
    ctx->files[ctx->files_count++] = strdup(val);
}

static bool read_file(const char *path, str_buf *buf) {
    buf_soft_reset(buf);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    buf_grow(buf, len);
    size_t nread = fread(buf->data, 1, (size_t)len, f);
    fclose(f);
    if (nread != (size_t)len) {
        buf_soft_reset(buf);
        return false;
    }
    buf->len = len;
    return true;
}

static int decode_all_cb(lua_State *L) {
    bool dont_read = lua_toboolean(L, lua_upvalueindex(1));
    str_buf *buf = (str_buf *)lua_touserdata(L, lua_upvalueindex(2));
    int n = lua_gettop(L);
    lua_newtable(L);

    for (int i = 1; i <= n; i++) {
        lua_pushvalue(L, lua_upvalueindex(3));

        if (dont_read) {
            const char *path = lua_tostring(L, i);
            if (!read_file(path, buf))
                return luaL_error(L, "failed to open file '%s'", path);
            push_buf_to_lua_string(L, buf);
        } else {
            lua_pushvalue(L, i);
        }
        lua_pushvalue(L, -3);

        if (lua_pcall(L, 2, 2, 0))
            return lua_error(L);
        if (!lua_isnil(L, -1))
            return luaL_error(L, "failed to decode: %s", lua_tostring(L, -1));

        lua_pop(L, 1);
        lua_replace(L, -2);
    }

    return 1;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    inject_tomlua(L);

    lua_createtable(L, argc, 0);
    for (int i = 0; i < argc; ++i) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");

    ArgusCtx ctx = { .L = L, .buf = new_str_buf() };
    lua_pushlightuserdata(L, &ctx);
    lua_newtable(L);
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "fancy_dates");
    lua_settable(L, LUA_REGISTRYINDEX);
    {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "config");
        const char *config = lua_tostring(L, -1);
        if (config && sscanf(config, "%c\n%c\n%c", &ctx.dirsep, &ctx.pathsep, &ctx.pathmark) < 3) {
            ctx.dirsep = '/'; ctx.pathsep = ':'; ctx.pathmark = '?';
        } else if (!config) {
            ctx.dirsep = '/'; ctx.pathsep = ':'; ctx.pathmark = '?';
        }
        lua_pop(L, 2);
    }

    ArgusFlag flags[] = {
        {"int_keys",         ARGUS_ARG_BOOL,     "Support reading mixed tables (Careful!)", NULL},
        {"fancy_dates",      ARGUS_ARG_BOOL,     "Enable proper date/time handling in input (default: true)\nUnlike the normal lua library, defaults to `true` to allow dates to survive round-trip encoding by default.", fancy_dates_cb},
        {"mark_inline",      ARGUS_ARG_BOOL,     "Mark tables as inline for output", NULL},
        {"multi_strings",    ARGUS_ARG_BOOL,     "Mark multi-line strings as multi-line for output", NULL},
        {"overflow_errors",  ARGUS_ARG_BOOL,     "Report overflow errors", NULL},
        {"underflow_errors", ARGUS_ARG_BOOL,     "Report underflow errors", NULL},
        {"fancy_tables",     ARGUS_ARG_BOOL,     "Allow reading inline toml tables that are multi-line and with optional trailing commas", NULL},
        {"ldir",             ARGUS_ARG_REQUIRED, "Add directory to Lua module search path", ldir_cb},
        {"cdir",             ARGUS_ARG_REQUIRED, "Add directory to Lua C module search path", cdir_cb},
        {"lpath",            ARGUS_ARG_REQUIRED, "Append to Lua module search path", lpath_cb},
        {"lcpath",           ARGUS_ARG_REQUIRED, "Append to Lua C module search path", lcpath_cb},
        {"cmd",              ARGUS_ARG_REQUIRED, "Execute Lua command string, exclusive with --script.\nReceives file content (or paths if --dont_read) via ... arguments, return a value to auto re-encode", cmd_cb},
        {"script",           ARGUS_ARG_REQUIRED, "Execute Lua script file, exclusive with --cmd.\nReceives file content (or paths if --dont_read) via ... arguments, return a value to auto re-encode\nPositional arguments passed before -- will also be parsed as if they were provided to this option to allow use of tomlua as a script shebang", script_cb},
        {"output",           ARGUS_ARG_REQUIRED, "Output file path (default: stdout)", output_cb},
        {"file",             ARGUS_ARG_REQUIRED, "Input file (can be specified multiple times)", file_cb},
        {"dont_read",        ARGUS_ARG_BOOL,     "Do not pre-read files before passing them to --cmd or --script options (default: false)", dont_read_cb},
        {NULL, 0, NULL, NULL}
    };

    ArgusConfig cfg = {
        .argc = argc,
        .argv = argv,
        .flags = flags,
        .padding = 35,
        .tail_usage_str = "[FILES...]",
        .default_action = default_action_cb,
        .parse_end_action = parse_end_cb,
        .positional_action = positional_cb,
        .userdata = &ctx,
    };

    int code = argus_parse(&cfg);
    buf_soft_reset(&ctx.buf);
    if (code == 1) {
        free_str_buf(&ctx.buf);
        lua_close(L);
        return 0;
    } else if (code == -1) {
        goto error_cleanup;
    }

    if (ctx.cmd && ctx.script) {
        fprintf(stderr, "error: cannot specify both --cmd and --script\n");
        fflush(stderr);
        goto error_cleanup;
    }

    lua_getglobal(L, "require");
    lua_pushstring(L, "tomlua");
    lua_call(L, 1, 1);
    lua_pushlightuserdata(L, &ctx);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_call(L, 1, 1);
    lua_setglobal(L, "tomlua");

    if (ctx.dont_read) {
        for (int i = 0; i < ctx.files_count; i++)
            lua_pushstring(L, ctx.files[i]);
    } else {
        for (int i = 0; i < ctx.files_count; i++) {
            if (!read_file(ctx.files[i], &ctx.buf)) {
                fprintf(stderr, "error: failed to open file '%s'\n", ctx.files[i]);
                fflush(stderr);
                goto error_cleanup;
            }
            push_buf_to_lua_string(L, &ctx.buf);
        }
    }

    if (ctx.cmd) {
        if (luaL_loadstring(L, ctx.cmd)) {
            fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
            fflush(stderr);
            goto error_cleanup;
        }
    } else if (ctx.script) {
        if (luaL_loadfile(L, ctx.script)) {
            fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
            fflush(stderr);
            goto error_cleanup;
        }
    } else {
        lua_pushboolean(L, ctx.dont_read);
        lua_pushlightuserdata(L, &ctx.buf);
        lua_getglobal(L, "tomlua");
        lua_getfield(L, -1, "decode");
        lua_remove(L, -2);
        lua_pushcclosure(L, decode_all_cb, 3);
    }

    if (ctx.files_count > 0)
        lua_insert(L, -(ctx.files_count + 1));

    if (lua_pcall(L, ctx.files_count, 1, 0)) {
        fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
        fflush(stderr);
        goto error_cleanup;
    }

    if (lua_istable(L, -1)) {
        lua_getglobal(L, "tomlua");
        lua_getfield(L, -1, "encode");
        lua_remove(L, -2);
        lua_insert(L, -2);
        if (lua_pcall(L, 1, 2, 0)) {
            fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
            fflush(stderr);
            goto error_cleanup;
        }
        if (!lua_isnil(L, -1)) {
            fprintf(stderr, "error: failed to encode result: %s\n", lua_tostring(L, -1));
            fflush(stderr);
            goto error_cleanup;
        }
        lua_pop(L, 1);

        if (ctx.outpath) {
            FILE *f = fopen(ctx.outpath, "wb");
            if (!f) {
                fprintf(stderr, "error: failed to open '%s' for writing\n", ctx.outpath);
                fflush(stderr);
                goto error_cleanup;
            }
            size_t slen;
            const char *s = lua_tolstring(L, -1, &slen);
            fwrite(s, 1, slen, f);
            fclose(f);
        } else {
            size_t slen;
            const char *s = lua_tolstring(L, -1, &slen);
            fwrite(s, 1, slen, stdout);
            fflush(stdout);
        }
    }

    free_str_buf(&ctx.buf);
    lua_close(L);
    return 0;

error_cleanup:
    free_str_buf(&ctx.buf);
    lua_close(L);
    return 1;
}
