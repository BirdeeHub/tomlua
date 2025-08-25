-- local f = io.open("./example.toml", "r")
-- local contents
-- if f then
--     contents = f:read("*a")
--     f:close()
-- end
local inspect = require('inspect')
local tomlua = require("tomlua")
-- print(contents)
-- print(inspect(tomlua))
local data, err = tomlua.decode([=[
    testkey = "value"
    testkey2 = true
    testkey3 = false
    testkey4 = 1
    testkey5 = 1.5
    testkey6 = [
        1, 2, 3, 4, 5
    ]
    testkey7 = {
        a.d = 1,
        b = [ 2, 3 ], # testing
        c = 3
    }
    # dasdsadsa = aaa2121dasssss22
    # dasdsadsa = 2121dasssss22 # these are giving me weird errors
    [[test]]
    names.hello = "hi"
    key = "value"
    [[test]]
    key = "value"
    "names"."boo" = "hi"
    names."boo2" = "hi2"
    "tk2-dsadas.com" = "value"
    [test2]
    key = "value"
    [test2]
    "tk1-assass.com" = "value"
    [[test2.key2]]
    das = "dasda"
]=])
print(inspect(data), inspect(err))
-- data = tomlua.decode(contents)
-- print(inspect(data))
