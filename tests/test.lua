local cfg = string.gmatch(package.config, "(%S+)")
local dirsep, pathsep, pathmark = cfg() or "/", cfg() or ";", cfg() or "?"
local tomlua_test_dir = debug.getinfo(1, 'S').source:sub(2):match("(.*)"..dirsep..".-") or "."
package.path = ("%s%s%s.lua%s"):format(tomlua_test_dir, dirsep, pathmark, pathsep) .. package.path

if arg[1] then
	package.cpath = ("%s%s%s.so%s"):format(arg[1], dirsep, pathmark, pathsep) .. package.cpath
end

local function run_test_file(define, path)
	local ok, run = pcall(require, path)
	if ok then assert(pcall(run, define, tomlua_test_dir)) end
end

local run_toml_edit = not arg[4] -- if arg[4] then skip toml edit

if tonumber(arg[2]) == 1 then
	local define = require('gambiarra')
	run_test_file(define, 'scratch')
	define.report()
	define.assert_passing()
elseif tonumber(arg[2]) == 2 then
	require('bench')(tomlua_test_dir, arg[3] and tonumber(arg[3]) or 100, run_toml_edit)
else
	local define = require('gambiarra')
	run_test_file(define, 'decode_tests')
	run_test_file(define, 'encode_tests')
	define.report()
	define.assert_passing()
end
