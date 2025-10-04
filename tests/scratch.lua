return function(define, test_dir)
    ---@type Tomlua
    local tomlua = require("tomlua")
    local inspect = require("inspect")

    -- local f = io.open("./tests/example.toml", "r")
    -- local contents
    -- if f then
    --     contents = f:read("*a")
    --     f:close()
    -- end

    local tomlua_default = tomlua()
    define("decode nested tables", function()
        local toml_str = [[
[owner]
name = "Tom Preston-Werner"
dob = 1979-05-27T07:32:00-07:00

[database]
server = "192.168.1.1"
ports = [ 8001, 8001, 8002 ]
connection_max = 5000
enabled = true
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.owner.name == "Tom Preston-Werner", "Nested table owner.name should be correct")
        it(data.database.server == "192.168.1.1", "Nested table database.server should be correct")
        it(eq(data.database.ports, { 8001, 8001, 8002 }), "Nested table database.ports should be correct")
    end)

    define("SCRATCH TEST PRINT", function()
        it(true, "DOING RANDOM STUFF FOR DEBUGGING")

        -- local toml = tomlua { fancy_tables = false, fancy_dates = true, multi_strings = true }
        -- print()
        -- print("will this error (sorta)")
        -- local data, err = toml.decode({ bleh = "haha", })
        -- print(inspect(data), "  :  ", inspect(err))
        -- print("will this error (no)")
        -- data, err = toml.decode("hehe = 'haha'", "Im the wrong type and will be ignored!")
        -- print(inspect(data), "  :  ", inspect(err))
        -- print("will this error (sorta)")
        -- data, err = toml.decode()
        -- print(inspect(data), "  :  ", inspect(err))
        -- print("will this error (no)")
        -- data, err = toml.decode("hehe = 'haha'", { bleh = "haha", }, "does it ignore extra args?")
        -- print(inspect(data), "  :  ", inspect(err))
        -- print()
        -- print("ENCODE")
        -- print()
        -- print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
        -- data = {
        --     g = { "1g", "2g", "3g", "4g" },
        -- }
        -- table.insert(data.g, data)
        -- print(toml.encode(data))
        -- print("THIS SHOULD RETURN ERROR AS SECOND RETURN VALUE FOR CYCLE DETECTION")
        -- data = {}
        -- data.value = data
        -- print(toml.encode(data))
        -- print()
        --
        -- local val = { c = 123456, b = { "hi", a = "b" } }
        -- print(tomlua.type(val.b))
        -- print(tomlua.encode(val))
        -- print()

            local appendtoml = [=[
        xmplstr = "this is a string"
        xmplfloat = 12.34
        xmplinteger = 1234
        xmplarray = [ "this", "is", "an", "array" ]
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
            xmplarray = { "1", "2", "3" },
        }

        local toml = tomlua { int_keys = true }
        local d, e = toml.decode(appendtoml, defaults)
        print(inspect(d), e)
        print(toml.encode(d))
    end)

end
