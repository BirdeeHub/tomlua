local tomlua_test_dir = arg[2]
local tomlua_dir = arg[3]
if tomlua_dir == nil then
	print("Usage: lua ./tests/test.lua -- <test_dir> <tomlua_lib_dir>")
	os.exit(1)
end
if tomlua_test_dir == nil then
	print("Usage: lua ./tests/test.lua -- <test_dir> <tomlua_lib_dir>")
	os.exit(1)
end
package.path = package.path .. (";%s/?.lua"):format(tomlua_test_dir)
package.cpath = package.cpath .. (";%s/?.so"):format(tomlua_dir)
local define = require('gambiarra')

local tests_passed = 0
local tests_failed = 0
require('gambiarra')(function(e, desc, msg, err)
	local suffix = desc .. ': ' .. tostring(msg) .. (err and "\n(with error: " .. err .. ")" or "")
	if e == 'pass' then
		print("[32m✔[0m " .. suffix)
		tests_passed = tests_passed + 1
	elseif e == 'fail' then
		print("[31m✘[0m " .. suffix)
		tests_failed = tests_failed + 1
	elseif e == 'except' then
		print("[31m✘[0m " .. suffix)
		tests_failed = tests_failed + 1
	end
end)

local ok, val = pcall(require, 'tests')
if ok then val(define, tomlua_test_dir) end

if tests_failed > 0 then os.exit(1) end
