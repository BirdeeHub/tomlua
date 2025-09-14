package.cpath = "./lib/?.so;" .. package.cpath
local inspect = require('inspect')
local tomlua = require("tomlua")


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
    local toml = tomlua { fancy_tables = false, strict = false, fancy_dates = true, multi_strings = false }
    local data, err = toml.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
    end
end

print("Last result:", inspect(last_result))
print("Last error:", last_error)

do
    local toml = tomlua { fancy_tables = false, strict = true, fancy_dates = true, multi_strings = true }
    local data, err = toml.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
    end
end

print("Last result:", inspect(last_result))
print("Last error:", last_error)

do
    local toml = tomlua { fancy_tables = false, strict = false, fancy_dates = true, multi_strings = true }
    print()
    print("will this error (sorta)")
    local data, err = toml.decode({ bleh = "haha", })
    print(inspect(data), "  :  ", inspect(err))
    print("will this error (no)")
    data, err = toml.decode("hehe = 'haha'", "Im the wrong type and will be ignored!")
    print(inspect(data), "  :  ", inspect(err))
    print("will this error (sorta)")
    data, err = toml.decode()
    print(inspect(data), "  :  ", inspect(err))
    print("will this error (no)")
    data, err = toml.decode("hehe = 'haha'", { bleh = "haha", }, "does it ignore extra args?")
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

    data, err = toml.decode(testtoml, testdata)
    print(inspect(data))
    print(inspect(err))

    print()
    print("ENCODE")
    print()
    print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
    data = {
        g = { "1g", "2g", "3g", "4g" },
    }
    table.insert(data.g, data)
    print(toml.encode(data))
    print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR STACK OVERFLOW")
    data = {}
    data.value = data
    print(toml.encode(data))
    print()
end

do
    local toml = tomlua { fancy_tables = false, strict = false, fancy_dates = true, multi_strings = true }
    local to_encode = toml.decode(contents)
    local str, e = toml.encode(to_encode)
    last_result = str
    if e then
        print("ERROR:", e)
    end
end
print("Last result:", last_result)


local date = tomlua.new_date({
    toml_type = tomlua.types.OFFSET_DATETIME,
    year = 3333,
    month = 3,
    day = 3,
    hour = 3,
    minute = 3,
    second = 3,
    fractional_second = 333333,
    offset_hour = 3,
    offset_minute = 3,
})
date.year = 2222 -- set values
for k, v in getmetatable(date).__pairs(date) do
    print(k, v)
end
local timestamp = date() -- call with no args to get timestamp
local date2 = tomlua.new_date(timestamp)
local date3 = tomlua.new_date("2028-08-24T12:00:00.666Z")
print("date1", date)
print("date2", date2)
print("date3", date3)
date(timestamp + 12345) -- set value from timestamp (__call takes same arg as new_date)
print(date > date2) -- true
print(date < date2) -- false
print("date1", date)

print(inspect(tomlua))

print(tomlua.typename(tomlua.type(date)))
