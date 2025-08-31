-- TODO: turn this into actual tests

local inspect = require('inspect')
local tomlua = require("tomlua")({ enhanced_tables = false, strict = true })

local f = io.open("./example.toml", "r")
local contents
if f then
    contents = f:read("*a")
    f:close()
end
local data, err = tomlua.decode(contents)
if err then
    print("DATA:\n", inspect(data), "\nERROR:\n", err)
end

print()
print("will this error (sorta)")
data, err = tomlua.decode({ bleh = "haha", })
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
print(tomlua.encode(testdata))
