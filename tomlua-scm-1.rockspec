local _MODREV, _SPECREV = 'scm', '-1'
rockspec_format = '3.0'
package = "tomlua"
version = _MODREV .. _SPECREV

source = {
   url = "https://github.com/BirdeeHub/"..package,
}

description = {
   summary = "TOML parser for Lua implemented in C",
   homepage = "https://github.com/BirdeeHub/"..package,
   license = "MIT"
}

dependencies = {
   "lua >= 5.1"
}

build = {
   type = "make",
   build_target = "build",
   build_variables = {
      LUA_INCDIR="$(LUA_INCDIR)",
   },
   install_target = "install",
   install_variables = {
      LIBDIR="$(LIBDIR)",
      LUADIR="$(LUADIR)",
   },
}

test = {
   type = "command",
   command = "make test",
}

-- How do I specify CFLAGS -O3 and maybe -flto here?
-- build = {
--    type = "builtin",
--    modules = {
--       tomlua = {
--           "./src/tomlua.c",
--           "./src/decode.c",
--           "./src/encode.c",
--           "./src/dates.c",
--       },
--    }
-- }
