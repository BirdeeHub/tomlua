local _MODREV, _SPECREV = 'scm', '-1'
rockspec_format = '3.0'
package = "tomlua"
version = _MODREV .. _SPECREV

source = {
   url = "https://github.com/BirdeeHub/"..package,
}

description = {
   summary = "TOML parser for Lua implemented in C",
   detailed = "High-performance TOML parser for Lua using Lua C API",
   homepage = "https://github.com/BirdeeHub/"..package,
   license = "MIT"
}

dependencies = {
   "lua >= 5.1"
}

build = {
   type = "make",
   install_pass = false,
   build_variables = {
      LUA_INCDIR="$(LUA_INCDIR)",
   },
   install = {
       lib = {
           ['tomlua.so'] = 'lib/tomlua.so',
       },
   },
}
