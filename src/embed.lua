local lib = arg[1] or error("No path to library provided")
local src = arg[2] or error("No source directory provided")
local out = arg[3] or error("No out path provided")

local luaembed = package.loadlib(lib, [[luaopen_luaembed]])()
local embed = luaembed(out, [[push_embedded_encode]], [[ENCODE_H_]])
embed.add('encode', src..[[/encode.lua]])
embed.run(true)
