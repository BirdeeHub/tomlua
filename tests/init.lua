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
local test = require('gambiarra')

local tests_passed = 0
local tests_failed = 0
require('gambiarra')(function(e, desc, msg)
	if e == 'pass' then
		print("[32m✔[0m " .. desc .. ': ' .. msg)
		tests_passed = tests_passed + 1
	elseif e == 'fail' then
		print("[31m✘[0m " .. desc .. ': ' .. msg)
		tests_failed = tests_failed + 1
	elseif e == 'except' then
		print("[31m✘[0m " .. desc .. ': ' .. msg)
		tests_failed = tests_failed + 1
	end
end)

local tomlua = require("tomlua")({ enhanced_tables = false, strict = true })

-- TODO: write some tests
test('test name', function()
	ok(true, 'message')
end)

if tests_failed > 0 then os.exit(1) end
