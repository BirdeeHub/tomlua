local cfg = string.gmatch(package.config, "(%S+)")
local dirsep, pathsep, pathmark = cfg() or "/", cfg() or ";", cfg() or "?"
local tomlua_test_dir = debug.getinfo(1, 'S').source:sub(2):match("(.*)"..dirsep..".-") or "."
package.path = ("%s%s%s.lua%s"):format(tomlua_test_dir, dirsep, pathmark, pathsep) .. package.path
package.cpath = ("%s%s%s.so%s"):format(arg[1], dirsep, pathmark, pathsep) .. package.cpath

local define = require('gambiarra')

local function run_test_file(path)
	local ok, run = pcall(require, path)
	if ok then assert(pcall(run, define, tomlua_test_dir)) end
end

run_test_file('decode_tests')
run_test_file('encode_tests')

define.report()
define.assert_passing()
