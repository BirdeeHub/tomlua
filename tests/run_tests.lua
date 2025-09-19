local tomlua_test_dir = arg[2]
local tomlua_dir = arg[3]
if tomlua_dir == nil then
	print("Usage: lua ./tests/init.lua -- <test_dir> <tomlua_lib_dir>")
	os.exit(1)
end
if tomlua_test_dir == nil then
	print("Usage: lua ./tests/init.lua -- <test_dir> <tomlua_lib_dir>")
	os.exit(1)
end
package.path = ("%s/?.lua;%s/?/init.lua;"):format(tomlua_test_dir, tomlua_test_dir) .. package.path
package.cpath = ("%s/?.so;"):format(tomlua_dir) .. package.cpath
local define = require('gambiarra')

local function run_test_file(path)
	local ok, run = pcall(require, path)
	if ok then assert(pcall(run, define, tomlua_test_dir)) end
end

run_test_file('tests')

define.end_tests()
