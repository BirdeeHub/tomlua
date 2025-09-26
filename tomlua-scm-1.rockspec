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
   build_variables = {
      LUA_INCDIR="$(LUA_INCDIR)",
   },
   install_variables = {
      LIBDIR="$(LIBDIR)",
      LUADIR="$(LUADIR)",
   },
}
-- -- How do I specify CFLAGS and make them compile in 1 command?
-- build = {
--    type = "builtin",
--    modules = {
--       tomlua = {
--           "./src/tomlua.c",
--           "./src/decode_str.c",
--           "./src/decode_inline_value.c",
--           "./src/decode.c",
--           "./src/encode.c",
--           "./src/dates.c",
--       },
--    }
-- }
