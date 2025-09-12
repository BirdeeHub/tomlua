package.cpath = "./lib/?.so;" .. package.cpath
local tomlua = require("tomlua") { fancy_tables = false, strict = false, fancy_dates = false }
local tomlua_strict = require("tomlua") { fancy_tables = false, strict = true, fancy_dates = false }

local inspect = require('inspect')

local f = io.open("./tests/example.toml", "r")
local contents
if f then
    contents = f:read("*a")
    f:close()
end
-- print(contents)

local last_result
local last_error

do
    local data, err = tomlua.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
    end
end

print("Last result:", inspect(last_result))
print("Last error:", last_error)

do
    local data, err = tomlua_strict.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
    end
end

print("Last result:", inspect(last_result))
print("Last error:", last_error)

print()
print("TODO remove junk test output")

print()
print("will this error (sorta)")
local data, err = tomlua.decode({ bleh = "haha", })
print(inspect(data), "  :  ", inspect(err))
print("will this error (no)")
data, err = tomlua.decode("hehe = 'haha'", "Im the wrong type and will be ignored!")
print(inspect(data), "  :  ", inspect(err))
print("will this error (sorta)")
data, err = tomlua.decode()
print(inspect(data), "  :  ", inspect(err))
print("will this error (no)")
data, err = tomlua.decode("hehe = 'haha'", { bleh = "haha", }, "does it ignore extra args?")
print(inspect(data), "  :  ", inspect(err))

print()
print("OVERLAY DEFAULTS TEST")

local testdata = {
    a = {
        b = { "1b", "2b" },
    },
    c = "hello",
    e = {
        f = { "1f", "2f" },
    },
    g = { "1g", "2g", "3g", "4g" },
}

local testtoml = [=[
a.b = [ "3b", "4b" ]
d = "hahaha"
[[e.f]]
testtable.value = true
[a]
newval = "nope"
]=]

data, err = tomlua.decode(testtoml, testdata)
print(inspect(data))
print(inspect(err))

print()
print("TODO: ENCODE")
print()
print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
data.value = testdata
print(tomlua.encode(data))
print()

tomlua = require("tomlua") { fancy_tables = false, strict = false, fancy_dates = true }
local to_encode = tomlua.decode(contents)

do
    local str, e = tomlua.encode(to_encode)
    last_result = str
    if e then
        print("ERROR:", e)
    end
end

print("Last result:", last_result)
