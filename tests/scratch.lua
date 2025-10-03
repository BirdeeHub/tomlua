package.cpath = "./lib/?.so;" .. package.cpath
local inspect = require('inspect')
---@type Tomlua
local tomlua = require("tomlua")

print(inspect(tomlua))

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
    local toml = tomlua { fancy_tables = false, fancy_dates = true, multi_strings = false }
    local data, err = toml.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
    end
end

print("Last result:", inspect(last_result))
print("Last error:", last_error)

do
    local toml = tomlua { fancy_tables = false, fancy_dates = true, multi_strings = true }
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
    print("ENCODE")
    print()
    print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
    data = {
        g = { "1g", "2g", "3g", "4g" },
    }
    table.insert(data.g, data)
    print(toml.encode(data))
    print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
    data = {}
    data.value = data
    print(toml.encode(data))
    print()
end

do
    local toml = tomlua { fancy_tables = false, fancy_dates = true, multi_strings = true, mark_inline = true }
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
    fractional = 333333,
    offset_hour = 3,
    offset_minute = 3,
})
print("date1", date)
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

print(date.toml_type)
print(tomlua.typename(tomlua.type_of(date)))
date.toml_type = "LOCAL_TIME"
print(date.toml_type)
print(date)
date.toml_type = tomlua.types.OFFSET_DATETIME
print(date.toml_type)
print(date)


print("ERROR TEST")
do
    local toml = tomlua {
        fancy_tables = false,
        fancy_dates = true,
        multi_strings = true,
    }
    local errtoml = [=[

    time_now = 06:22:05
    time_next = 06:22:05.1234

    # INVALID TOML DOC
    fruits = []
    database = { duplicate = { hehe = "haha" }}

    release_date = 2022-08-24T12:00:00.666969696969696969Z
    next_release = 2028-08-24T12:00:00.666Z
    last_backup = 2025-08-23T23:45:12-07:00
    last_modified = 2025-08-24 12:00:00Z
    date_today = 2025-08-24

    [[fruits]]
    name = "apple"

    [fruits.physical]  # subtable
    color = "red"
    shape = "round"

    [[fruits.varieties]]  # nested array of tables
    name = "red delicious"

    [[fruits.varieties]]
    name = "granny smith"


    [[fruits]]
    name = "banana"

    [[fruits.varieties]]
    name = "plantain"
    [[test]]
    names.hello = "h\u1234i"
    key = "\U0001F600 value"
    [[test]]
    key = "value"
    "names"."boo" = "hi"
    names."boo2" = "hi2"
    "tk2-dsadas.com" = "value"
    "tk2-dsadas.com" = "vaaalue"

    [database]
    type = "postgres"
    host = "localhost"
    port = 5432
    username = "dbuser"
    password = "secret"
    pool_size = 10

    [database.replica]
    host = "replica1.local"
    port = 5433
    ]=]
    local d, e = toml.decode(errtoml)
    print(require('inspect')(d), e)
end

local val = { c = 123456, b = { "hi", a = "b" } }
print(tomlua.type(val.b))
print(tomlua.encode(val))

do
    local appendtoml = [=[
[example]
test = "this is a test"
[[example2]]
test = "this is a test"
]=]
    local defaults = {
        "test_mixed",
        ["1"] = "im different",
        mixed = {
            "test_mixed",
            ["1"] = "im different",
        },
        mixed_inline = setmetatable({
            "test_mixed",
            ["1"] = "im different",
        }, { toml_type = "TABLE_INLINE" }),
        example = {
            if_defined = "as a heading",
            the_values_here = "will be recursively updated",
        },
        example2 = {
            { test = "if defined as a heading", },
            { test = "it will append", }
        },
    }

    local toml = tomlua { int_keys = true }
    local d, e = toml.decode(appendtoml, defaults)
    print(require('inspect')(d), e)
    print(toml.encode(d))
end
