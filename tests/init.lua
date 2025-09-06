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

local tomlua = require("tomlua")({ enhanced_tables = false, strict = false })

define("decode example.toml", function()
	local tomlua_s = require("tomlua")({ enhanced_tables = false, strict = true })
    local f = io.open(("%s/example.toml"):format(tomlua_test_dir), "r")
    local contents
    if f then
        contents = f:read("*a")
        f:close()
    end
    local data, err = tomlua_s.decode(contents)
    it(data ~= nil, "Data should not be nil")
    it(err == nil, "Should not error on valid TOML")
end)

define("decode with wrong type", function()
    local data, err = tomlua.decode({ bleh = "haha" })
    it(err ~= nil, "Should error on table input")
end)

define("decode extra args ignored", function()
    local data, err = tomlua.decode("hehe = 'haha'", "ignored")
    it(data.hehe == "haha", "Value should be parsed correctly")
    it(err == nil, "Should ignore extra arguments")
end)

define("overlay defaults", function()
    local testdata = {
        a = { b = { "1b", "2b" } },
        c = "hello",
        e = { f = { "1f", "2f" } },
    }

    local testtoml = [=[
a.b = [ "3b", "4b" ]
d = "hahaha"
[[e.f]]
testtable.value = true
[a]
newval = "nope"
]=]

    local data, err = tomlua.decode(testtoml, testdata)
    it(data.a.b[1] == "3b", "a.b should be overwritten")
    it(data.d == "hahaha", "d should be added")
    it(err == nil, "Should not error")
end)

if tests_failed > 0 then os.exit(1) end
