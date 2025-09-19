return function(define, test_dir)

    -- TODO: A more tests

    local tomlua_default = require("tomlua")
    local tomlua_strict = require("tomlua")({ strict = true })
    local tomlua_fancy_dates = require("tomlua")({ fancy_dates = true })
    local tomlua_int_keys = require("tomlua")({ int_keys = true })
    local tomlua_multi_strings = require("tomlua")({ multi_strings = true })
    local tomlua_mark_inline = require("tomlua")({ mark_inline = true })

    -- Basic Types
    define("decode basic string", function()
        local data, err = tomlua_default.decode([[key = "value"]])
        it(err == nil, "Should not error")
        it(data.key == "value", "String value should be correct")
    end)

    define("decode basic integer", function()
        local data, err = tomlua_default.decode("key = 123")
        it(err == nil, "Should not error")
        it(data.key == 123, "Integer value should be correct")
    end)

    define("decode basic float", function()
        local data, err = tomlua_default.decode("key = 1.23")
        it(err == nil, "Should not error")
        it(data.key == 1.23, "Float value should be correct")
    end)

    define("decode basic boolean true", function()
        local data, err = tomlua_default.decode("key = true")
        it(err == nil, "Should not error")
        it(data.key == true, "Boolean true value should be correct")
    end)

    define("decode basic boolean false", function()
        local data, err = tomlua_default.decode("key = false")
        it(err == nil, "Should not error")
        it(data.key == false, "Boolean false value should be correct")
    end)

    define("decode basic datetime (offset)", function()
        local data, err = tomlua_default.decode("key = 1979-05-27T07:32:00-07:00")
        it(err == nil, "Should not error")
        it(data.key == "1979-05-27T07:32:00-07:00", "Offset datetime should be correct")
    end)

    define("decode basic datetime (local)", function()
        local data, err = tomlua_default.decode("key = 1979-05-27T07:32:00")
        it(err == nil, "Should not error")
        it(data.key == "1979-05-27T07:32:00", "Local datetime should be correct")
    end)

    define("decode basic date", function()
        local data, err = tomlua_default.decode("key = 1979-05-27")
        it(err == nil, "Should not error")
        it(data.key == "1979-05-27", "Local date should be correct")
    end)

    define("decode basic time", function()
        local data, err = tomlua_default.decode("key = 07:32:00")
        it(err == nil, "Should not error")
        it(data.key == "07:32:00", "Local time should be correct")
    end)

    -- Arrays
    define("decode array of integers", function()
        local data, err = tomlua_default.decode("key = [1, 2, 3]")
        it(err == nil, "Should not error")
        it(eq(data.key, { 1, 2, 3 }), "Array of integers should be correct")
    end)

    define("decode array of mixed types", function()
        local data, err = tomlua_default.decode("key = [1, \"hello\", true, 1.2]")
        it(err == nil, "Should not error")
        it(eq(data.key, { 1, "hello", true, 1.2 }), "Array of mixed types should be correct")
    end)

    define("decode empty array", function()
        local data, err = tomlua_default.decode("key = []")
        it(err == nil, "Should not error")
        it(eq(data.key, {}), "Empty array should be correct")
    end)

    -- Tables
    define("decode inline table", function()
        local data, err = tomlua_default.decode([[key = { name = "Tom", age = 30 }]])
        it(err == nil, "Should not error")
        it(eq(data.key, { name = "Tom", age = 30 }), "Inline table should be correct")
    end)

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

    -- Strict mode
    define("strict mode duplicate key error", function()
        local toml_str = [[
key = 1
key = 2
]]
        local data, err = tomlua_strict.decode(toml_str)
        it(err ~= nil, "Strict mode should error on duplicate key")
    end)

    define("strict mode duplicate table error", function()
        local toml_str = [[
[table]
key = 1
[table]
key = 2
]]
        local data, err = tomlua_strict.decode(toml_str)
        it(err ~= nil, "Strict mode should error on duplicate table")
    end)

    -- Fancy Dates
    define("fancy_dates option", function()
        local toml_str = "date = 1979-05-27T07:32:00Z"
        local data, err = tomlua_fancy_dates.decode(toml_str)
        it(err == nil, "Should not error")
        it(type(data.date) == "userdata", "Date should be userdata with fancy_dates")
        it(data.date.year == 1979, "Date year should be correct")
    end)

    -- Integer Keys
    define("int_keys option", function()
        local toml_str = [[123 = "value"]]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(data[123] == "value", "Key should be integer with int_keys")
    end)

    define("int_keys option with quoted key", function()
        local toml_str = [["123" = "value"]]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(data["123"] == "value", "Quoted key should remain string with int_keys")
    end)

    -- Multiline Strings
    define("multi_strings option", function()
        local toml_str = [[
text = """Hello
World
"""
text2 = """Hello\nWorld\n"""
]]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(type(data.text) == "userdata", "Multiline string should be userdata with multi_strings")
        it(tostring(data.text) == "Hello\nWorld\n", "Multiline string content should be correct")
        it(tostring(data.text2) == "Hello\nWorld\n", "Multiline string content2 should be correct")
    end)

    -- Mark Inline
    define("mark_inline option for inline table", function()
        local toml_str = "table = { a = 1 }"
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(getmetatable(data.table) and getmetatable(data.table).toml_type == tomlua_mark_inline.types.TABLE_INLINE, "Inline table should be marked as TABLE_INLINE")
    end)

    define("mark_inline option for inline array", function()
        local toml_str = "array = [1, 2]"
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(getmetatable(data.array) and getmetatable(data.array).toml_type == tomlua_mark_inline.types.ARRAY_INLINE, "Inline array should be marked as ARRAY_INLINE")
    end)

    -- Encoding tests (basic round-trip)
    define("encode basic table", function()
        local test_table = {
            name = "Test",
            value = 123,
            enabled = true
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        local decoded_data, decode_err = tomlua_default.decode(encoded_str)
        it(decode_err == nil, "Should not error during decoding encoded string")
        it(eq(test_table, decoded_data), "Encoded and decoded table should be equal")
    end)

    define("encode table with nested array", function()
        local test_table = {
            data = {
                items = { "a", "b", "c" },
                count = 3
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        local decoded_data, decode_err = tomlua_default.decode(encoded_str)
        it(decode_err == nil, "Should not error during decoding encoded string")
        it(eq(test_table, decoded_data), "Encoded and decoded table with nested array should be equal")
    end)

    define("encode table with fancy date", function()
        local date_obj = tomlua_default.new_date({
            toml_type = tomlua_default.types.OFFSET_DATETIME,
            year = 2024, month = 1, day = 1,
            hour = 12, minute = 30, second = 0, fractional = 0,
            offset_hour = 0, offset_minute = 0
        })
        local test_table = {
            event_date = date_obj
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        local decoded_data, decode_err = tomlua_fancy_dates.decode(encoded_str) -- Use fancy_dates to decode
        it(decode_err == nil, "Should not error during decoding encoded string")
        it(type(decoded_data.event_date) == "userdata", "Decoded date should be userdata")
        it(decoded_data.event_date.year == 2024, "Decoded date year should be correct")
    end)

    define("encode multiline string", function()
        local mul_str = tomlua_default.str_2_mul("line1\nline2")
        local test_table = {
            message = mul_str
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        local decoded_data, decode_err = tomlua_multi_strings.decode(encoded_str)
        it(decode_err == nil, "Should not error during decoding encoded string")
        it(type(decoded_data.message) == "userdata", "Decoded multiline string should be userdata")
        it(tostring(decoded_data.message) == "line1\nline2", "Decoded multiline string content should be correct")
    end)

    define("encode empty table and array with toml_type", function()
        local empty_toml_array = setmetatable({}, {
            toml_type = tomlua_default.types.ARRAY
        })
        local empty_toml_table = setmetatable({}, {
            toml_type = tomlua_default.types.TABLE
        })
        local test_table = {
            empty_arr = empty_toml_array,
            empty_tbl = empty_toml_table
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        -- Check if the output string contains the expected empty array/table format
        it(string.find(encoded_str, "empty_arr = []", nil, true) ~= nil, "Encoded string should contain empty array")
        it(string.find(encoded_str, "[empty_tbl]", nil, true) ~= nil, "Encoded string should contain empty table heading")
    end)

    define("encode inline table and array with toml_type", function()
        local inline_arr = setmetatable({1, 2}, {
            toml_type = tomlua_default.types.ARRAY_INLINE
        })
        local inline_tbl = setmetatable({a = 1, b = 2}, {
            toml_type = tomlua_default.types.TABLE_INLINE
        })
        local test_table = {
            inline_array = inline_arr,
            inline_table = inline_tbl
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str,
[=[inline_array = %[
  1,
  2
]]=]) ~= nil, "Encoded string should contain inline array")
        it(string.find(encoded_str, "inline_table = { %a = %d, %a = %d }") ~= nil, "Encoded string should contain inline table")
    end)

end
