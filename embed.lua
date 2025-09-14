local lib = arg[1] or error("No path to library provided")
local src = arg[2] or error("No source directory provided")
local out = arg[3] or error("No out path provided")

local luaembed = package.loadlib(lib, [[luaopen_tomlua_embed]])()
local embed = luaembed([[push_embedded_encode]])
embed.add('encode', src..[[/encode.lua]])
embed.run(out, [[ENCODE_H_]])
