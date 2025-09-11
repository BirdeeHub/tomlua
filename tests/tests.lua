return function(define, test_dir)

define("decode example.toml", function()
    local tomlua_s = require("tomlua")({ fancy_tables = false, strict = true })
    local f = io.open(("%s/example.toml"):format(test_dir), "r")
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
    local tomlua = require("tomlua")({ fancy_tables = false, strict = false })
    local data, err = tomlua.decode({ bleh = "haha" })
    it(err ~= nil, "Should error on table input")
end)

define("decode extra args ignored", function()
    local tomlua = require("tomlua")({ fancy_tables = false, strict = false })
    local data, err = tomlua.decode("hehe = 'haha'", "ignored")
    it(data.hehe == "haha", "Value should be parsed correctly")
    it(err == nil, "Should ignore extra arguments")
end)

define("overlay defaults", function()
    local tomlua = require("tomlua")({ fancy_tables = false, strict = false })
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

end
