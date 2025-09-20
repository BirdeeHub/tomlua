return function(define, test_dir)
    -- TODO: more tests

    local tomlua_default = require("tomlua")
    local tomlua_strict = require("tomlua")({ strict = true })
    local tomlua_fancy_dates = require("tomlua")({ fancy_dates = true })
    local tomlua_int_keys = require("tomlua")({ int_keys = true })
    local tomlua_multi_strings = require("tomlua")({ multi_strings = true })
    local tomlua_mark_inline = require("tomlua")({ mark_inline = true })
    local tomlua_overflow_errors = require("tomlua")({ overflow_errors = true })
    local tomlua_underflow_errors = require("tomlua")({ underflow_errors = true })

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
