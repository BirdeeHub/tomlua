return function(define, test_dir)

    -- TODO: A more tests

    local tomlua_default = require("tomlua")
    local tomlua_strict = require("tomlua")({ strict = true })
    local tomlua_fancy_dates = require("tomlua")({ fancy_dates = true })
    local tomlua_int_keys = require("tomlua")({ int_keys = true })
    local tomlua_multi_strings = require("tomlua")({ multi_strings = true })
    local tomlua_mark_inline = require("tomlua")({ mark_inline = true })
    local tomlua_overflow_errors = require("tomlua")({ overflow_errors = true })
    local tomlua_underflow_errors = require("tomlua")({ underflow_errors = true })

    define("decode example.toml", function()
        local f = io.open(("%s/example.toml"):format(test_dir), "r")
        local contents
        if f then
            contents = f:read("*a")
            f:close()
        end
        local data, err = tomlua_strict.decode(contents)
        it(data ~= nil, "Data should not be nil")
        it(err == nil, "Should not error on valid TOML")
    end)

    define("decode with wrong type", function()
        local data, err = tomlua_default.decode({ bleh = "haha" })
        it(err ~= nil, "Should error on table input")
    end)

    define("decode extra args ignored", function()
        local data, err = tomlua_default.decode("hehe = 'haha'", "ignored")
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

    local data, err = tomlua_default.decode(testtoml, testdata)
    it(data.a.b[1] == "3b", "a.b should be overwritten")
    it(data.d == "hahaha", "d should be added")
    it(err == nil, "Should not error")
end)

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
        it(data.date.hour == 7, "Date hour should be correct")
        it(data.date.day == 27, "Date day should be correct")
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
text3 = """"This," she said, "is just a pointless statement.""""
text4 = """""five quotes?!?!"""""
text5 = ''''That,' she said, 'is still pointless.''''
text6 = '''''five apostrophes??!?!'''''
]]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(type(data.text) == "userdata", "Multiline string should be userdata with multi_strings")
        it(tostring(data.text) == "Hello\nWorld\n", "Multiline string content 1 should be correct")
        it(tostring(data.text2) == "Hello\nWorld\n", "Multiline string content 2 should be correct")
        it(tostring(data.text3) == [["This," she said, "is just a pointless statement."]], "Multiline string content 3 should be correct")
        it(tostring(data.text4) == [[""five quotes?!?!""]], "Multiline string content 4 should be correct")
        it(tostring(data.text5) == [['That,' she said, 'is still pointless.']], "Multiline string content 5 should be correct")
        it(tostring(data.text6) == [[''five apostrophes??!?!'']], "Multiline string content 6 should be correct")
    end)

    -- Mark Inline
    define("mark_inline option for inline table", function()
        local toml_str = "table = { a = 1 }"
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(tomlua_default.type(data.table) == tomlua_mark_inline.types.TABLE_INLINE, "Inline table should be marked as TABLE_INLINE")
    end)

    define("mark_inline option for inline array", function()
        local toml_str = "array = [1, 2]"
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(tomlua_default.type(data.array) == tomlua_mark_inline.types.ARRAY_INLINE, "Inline array should be marked as ARRAY_INLINE")
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

    define("underflow_errors: decode float underflow", function()
        -- A very small number smaller than the smallest normal double
        local tiny = "tiny = 1.00000000999999999999999999999000000001123213212131e-316"
        local data, err = tomlua_underflow_errors.decode(tiny)
        it(err ~= nil, "Should error on underflow when underflow_errors")
        data, err = tomlua_underflow_errors.decode("normal = 1004.44")
        it(err == nil, "Should not error on normal values when underflow_errors")
        data, err = tomlua_default.decode(tiny)
        it(err == nil, "Should not error on underflow otherwise")
        it(data.tiny ~= 0.0, "Underflow should preserve subnormal value or 0 for extreme underflow")
    end)

    define("overflow_errors: decode float overflow", function()
        -- A number too big to fit in a double
        local huge = "huge = 1e400"
        local data, err = tomlua_overflow_errors.decode(huge)
        it(err ~= nil, "Should error on overflow when overflow_errors")
        data, err = tomlua_default.decode(huge)
        it(err == nil, "Should not error on overflow otherwise")
        it(data.huge == math.huge, "Overflow should be converted to +INFINITY")
    end)

    define("overflow_errors: decode negative float overflow", function()
        -- Negative number too small to fit in a double
        local neghuge = "neghuge = -1e400"
        local data, err = tomlua_overflow_errors.decode(neghuge)
        it(err ~= nil, "Should error on negative overflow when overflow_errors")
        data, err = tomlua_overflow_errors.decode("normal = 1004.44")
        it(err == nil, "Should not error on normal values when overflow_errors")
        data, err = tomlua_default.decode(neghuge)
        it(err == nil, "Should not error on negative overflow otherwise")
        it(data.neghuge == -math.huge, "Negative overflow should be converted to -INFINITY")
    end)


    define("overflow_errors: decode integer overflow", function()
        local huge_int = [[
huge_int = 999999999999999999999999999999999999999999999999999999999999999999999999999
neg_huge_int = -999999999999999999999999999999999999999999999999999999999999999999999999999
]]
        local data, err = tomlua_overflow_errors.decode(huge_int)
        it(err ~= nil, "Should error on extremely large integer when overflow_errors")
        data, err = tomlua_overflow_errors.decode("normal = 1004")
        it(err == nil, "Should not error on normal values when overflow_errors")
        data, err = tomlua_default.decode(huge_int)
        it(err == nil, "Should not error on extremely large integer otherwise")
        -- Lua converts integers that don't fit into float or integer types, so expect it as a float
        it(type(data.huge_int) == "number", "Value should be treated as number (float) when exceeding integer range")
        it(data.huge_int == math.huge, "Value should be very large")
        it(data.neg_huge_int == -math.huge, "Value should be very large")
    end)

end
