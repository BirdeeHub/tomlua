return function(define, test_dir)
    -- Additional tests for TOMLUA encode functionality

    ---@type Tomlua
    local tomlua_default = require("tomlua")
    ---@type Tomlua
    local tomlua_fancy_dates = require("tomlua")({ fancy_dates = true })
    ---@type Tomlua
    local tomlua_int_keys = require("tomlua")({ int_keys = true })
    ---@type Tomlua
    local tomlua_multi_strings = require("tomlua")({ multi_strings = true })
    ---@type Tomlua
    local tomlua_mark_inline = require("tomlua")({ mark_inline = true })
    ---@type Tomlua
    local tomlua_overflow_errors = require("tomlua")({ overflow_errors = true })
    ---@type Tomlua
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

    define("encode mixed array/table lua tables", function()
        local val = { c = 123456, b = { "hi", a = "b" } }
        local encoded_str, err = tomlua_default.encode(val)
        it(err ~= nil, "Should error during encoding mixed tables without int_keys")
        encoded_str, err = tomlua_int_keys.encode(val)
        it(err == nil, "Should not error during encoding mixed tables with int_keys")
        it(tomlua_default.type(val.b) == "TABLE", "Encode should detect as table type")
        it(function()
            if encoded_str:find("%[b]") == nil then
                error("b was not printed as a table heading")
            end
            if encoded_str:find([[1 = "hi"]]) == nil then
                error("array type value in mixed table was not encoded with integer key")
            end
        end, "Should print table b as a table heading with some integer keys.")
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

    define("encode basic string", function()
        local test_table = { key = "value" }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = \"value\"", nil, true) ~= nil, "Encoded string should contain correct key-value pair")
    end)

    define("encode basic integer", function()
        local test_table = { key = 123 }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = 123", nil, true) ~= nil, "Encoded integer should be correct")
    end)

    define("encode basic float", function()
        local test_table = { key = 1.23 }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = 1.23", nil, true) ~= nil, "Encoded float should be correct")
    end)

    define("encode basic boolean true", function()
        local test_table = { key = true }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = true", nil, true) ~= nil, "Encoded boolean true should be correct")
    end)

    define("encode basic boolean false", function()
        local test_table = { key = false }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = false", nil, true) ~= nil, "Encoded boolean false should be correct")
    end)

    define("encode array of integers", function()
        local test_table = { key = { 1, 2, 3 } }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = %[%s*1,%s*2,%s*3%s*]", nil) ~= nil, "Encoded array of integers should be correct")
    end)

    define("encode array of mixed types", function()
        local test_table = { key = { 1, "hello", true, 1.2 } }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, [=[key = %[%s*1,%s*"hello",%s*true,%s*1.2%s*]]=], nil) ~= nil, "Encoded array of mixed types should be correct")
    end)

    define("encode empty array", function()
        local test_table = { key = setmetatable({}, { toml_type = "ARRAY_INLINE"}) }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = []", nil, true) ~= nil, "Encoded empty array should be correct")
    end)

    define("encode nested tables", function()
        local test_table = {
            owner = {
                name = "Tom",
                dob = "1979-05-27T07:32:00-07:00"
            },
            database = {
                server = "192.168.1.1",
                ports = { 8001, 8002 },
                enabled = true
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[owner]", nil, true) ~= nil, "Should contain owner table heading")
        it(string.find(encoded_str, "name = \"Tom\"", nil, true) ~= nil, "Should contain owner name")
        it(string.find(encoded_str, "[database]", nil, true) ~= nil, "Should contain database table heading")
        it(string.find(encoded_str, "ports = %[%s*8001,%s*8002%s*]", nil) ~= nil, "Should contain database ports")
    end)

    define("encode dotted keys", function()
        local test_table = {
            ["name.first"] = "Tom",
            ["name.last"] = "Preston-Werner"
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "\"name.first\" = \"Tom\"", nil, true) ~= nil, "Should encode dotted key correctly")
    end)

    define("encode array of tables", function()
        local test_table = {
            products = {
                { name = "Laptop", price = 1200 },
                { name = "Mouse", price = 25 }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[[products]]", nil, true) ~= nil, "Should contain array of tables heading")
        it(string.find(encoded_str, "name = \"Laptop\"", nil, true) ~= nil, "Should contain first product name")
        it(string.find(encoded_str, "name = \"Mouse\"", nil, true) ~= nil, "Should contain second product name")
    end)

    define("encode fancy_dates: offset datetime", function()
        local date_obj = tomlua_default.new_date({
            toml_type = tomlua_default.types.OFFSET_DATETIME,
            year = 1979, month = 5, day = 27,
            hour = 7, minute = 32, second = 0, fractional = 0,
            offset_hour = -7, offset_minute = 0
        })
        local test_table = { event_date = date_obj }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "event_date = 1979-05-27T07:32:00-07:00", nil, true) ~= nil, "Encoded offset datetime should be correct")
    end)

    define("encode fancy_dates: local datetime", function()
        local date_obj = tomlua_default.new_date({
            toml_type = tomlua_default.types.LOCAL_DATETIME,
            year = 1979, month = 5, day = 27,
            hour = 7, minute = 32, second = 0, fractional = 0
        })
        local test_table = { event_date = date_obj }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "event_date = 1979-05-27T07:32:00", nil, true) ~= nil, "Encoded local datetime should be correct")
    end)

    define("encode fancy_dates: local date", function()
        local date_obj = tomlua_default.new_date({
            toml_type = tomlua_default.types.LOCAL_DATE,
            year = 1979, month = 5, day = 27
        })
        local test_table = { event_date = date_obj }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "event_date = 1979-05-27", nil, true) ~= nil, "Encoded local date should be correct")
    end)

    define("encode fancy_dates: local time", function()
        local date_obj = tomlua_default.new_date({
            toml_type = tomlua_default.types.LOCAL_TIME,
            hour = 7, minute = 32, second = 0, fractional = 0
        })
        local test_table = { event_date = date_obj }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "event_date = 07:32:00", nil, true) ~= nil, "Encoded local time should be correct")
    end)

    define("encode multi_strings: basic multiline string", function()
        local mul_str = tomlua_default.str_2_mul("Hello\nWorld")
        local test_table = { message = mul_str }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, [=[message = """Hello
World"""]=], nil, true) ~= nil, "Encoded multiline string should be correct")
    end)

    define("encode multi_strings: literal multiline string", function()
        local mul_str = tomlua_default.str_2_mul([[C:\\Users\\Tom]], true) -- true for literal
        local test_table = { path = mul_str }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, [=[path = """C:\\\\Users\\\\Tom"""]=], nil, true) ~= nil, "Encoded literal multiline string should be correct")
    end)

    define("encode mark_inline: inline table", function()
        local inline_tbl = setmetatable({ a = 1, b = 2 }, { toml_type = tomlua_default.types.TABLE_INLINE })
        local test_table = { config = inline_tbl }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "config = { %a = %d, %a = %d }", nil) ~= nil, "Encoded inline table should be correct")
    end)

    define("encode mark_inline: inline array", function()
        local inline_arr = setmetatable({ 1, 2, 3 }, { toml_type = tomlua_default.types.ARRAY_INLINE })
        local test_table = { data = inline_arr }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "data = %[%s*1,%s*2,%s*3%s*]", nil) ~= nil, "Encoded inline array should be correct")
    end)

    define("encode empty table with toml_type", function()
        local empty_toml_table = setmetatable({}, { toml_type = tomlua_default.types.TABLE })
        local test_table = { empty_tbl = empty_toml_table }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[empty_tbl]", nil, true) ~= nil, "Encoded string should contain empty table heading")
    end)

    define("encode empty array with toml_type", function()
        local empty_toml_array = setmetatable({}, { toml_type = tomlua_default.types.ARRAY })
        local test_table = { empty_arr = empty_toml_array }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "empty_arr = []", nil, true) ~= nil, "Encoded string should contain empty array")
    end)

    define("encode complex nested structure", function()
        local test_table = {
            general = {
                title = "TOML Example",
                enabled = true
            },
            owner = {
                name = "John Doe",
                email = "john.doe@example.com",
                address = setmetatable({ street = "123 Main St", city = "Anytown" }, { toml_type = tomlua_default.types.TABLE_INLINE })
            },
            servers = {
                { name = "alpha", ip = "192.168.1.1", ports = setmetatable({ 8001, 8002 }, { toml_type = tomlua_default.types.ARRAY_INLINE }) },
                { name = "beta", ip = "192.168.1.2", ports = setmetatable({ 8003 }, { toml_type = tomlua_default.types.ARRAY_INLINE }), config = setmetatable({ max_connections = 100, timeout = 30 }, { toml_type = tomlua_default.types.TABLE_INLINE }) }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding complex structure")
        it(string.find(encoded_str, "[general]", nil, true) ~= nil, "Should contain general table heading")
        it(string.find(encoded_str, "title = \"TOML Example\"", nil, true) ~= nil, "Should contain general title")
        it(string.find(encoded_str, "address = { %S+ = \".*\", %S+ = \".*\" }", nil) ~= nil, "Should contain inline address table")
        it(string.find(encoded_str, "[[servers]]", nil, true) ~= nil, "Should contain servers array of tables heading")
        it(string.find(encoded_str, "name = \"alpha\"", nil, true) ~= nil, "Should contain first server name")
        it(string.find(encoded_str, "ports = %[%s*8001,%s*8002%s*]", nil) ~= nil, "Should contain first server ports")
        it(string.find(encoded_str, "config = { %S+ = %d+, %S+ = %d+ }", nil) ~= nil, "Should contain second server config")
    end)

    define("encode table with mixed array and table values", function()
        local test_table = {
            data = {
                numbers = { 1, 2, 3 },
                info = { name = "test", version = 1.0 },
                flags = { true, false }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[data]", nil, true) ~= nil, "Should contain data table heading")
        it(string.find(encoded_str, "numbers = %[%s+1,%s+2,%s+3%s+]", nil) ~= nil, "Should contain numbers array")
        it(string.find(encoded_str, "name = \"test\"", nil, true) ~= nil, "Should contain info name")
        it(string.find(encoded_str, "flags = %[%s+true,%s+false%s+]", nil) ~= nil, "Should contain flags array")
    end)

    define("encode table with array of inline tables", function()
        local test_table = {
            points = setmetatable({
                { x = 1, y = 3 },
                { x = 3, y = 4 }
            }, {
                toml_type = "ARRAY_INLINE"
            })
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "points = %[%s+{ [xy] = [1234], [xy] = [1234] },%s+{ [xy] = [1234], [xy] = [1234] }%s+]") ~= nil, "Encoded array of inline tables should be correct")
    end)

    define("encode table with array of arrays", function()
        local test_table = {
            matrix = setmetatable({
                { 1, 2 },
                { 3, 4 }
            }, { toml_type = tomlua_default.types.ARRAY_INLINE })
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "matrix = %[%s+%[%s+1,%s+2%s+],%s+%[%s+3,%s+4%s+]%s+]", nil) ~= nil, "Encoded array of arrays should be correct")
    end)

    define("encode table with mixed array of tables and arrays", function()
        local test_table = {
            items = setmetatable({
                { id = 1, value = "A" },
                { 10, 20 },
                { id = 2, value = "B" },
            }, { toml_type = tomlua_default.types.ARRAY_INLINE })
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "items = %[%s+{ %S+ = %S+, %S+ = %S+ },%s+%[%s+10,%s+20%s+],%s+{ %S+ = %S+, %S+ = %S+ }%s+]", nil) ~= nil, "Encoded mixed array of tables and arrays should be correct")
    end)

    define("encode table with complex key names", function()
        local test_table = {
            ["key.with.dots"] = 1,
            ["key-with-dashes"] = 2,
            ["key with spaces"] = 3,
            ["123"] = 4
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "\"key.with.dots\" = 1", nil, true) ~= nil, "Key with dots should be encoded correctly")
        it(string.find(encoded_str, "key-with-dashes = 2", nil, true) ~= nil, "Key with dashes should be encoded correctly")
        it(string.find(encoded_str, "\"key with spaces\" = 3", nil, true) ~= nil, "Key with spaces should be encoded correctly")
        it(string.find(encoded_str, [["123" = 4]], nil, true) ~= nil, "Numeric key as string should be encoded correctly")
    end)

    define("encode table with unicode keys and values", function()
        local test_table = {
            unicode_key = "你好世界",
            ["emoji_key"] = "👋🌍"
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "unicode_key = \"你好世界\"", nil, true) ~= nil, "Unicode key and value should be encoded correctly")
        it(string.find(encoded_str, "emoji_key = \"👋🌍\"", nil, true) ~= nil, "Emoji key and value should be encoded correctly")
    end)

    define("encode table with escaped strings", function()
        local test_table = {
            escaped_quotes = "He said \"Hello!\"",
            escaped_backslash = "C:\\Users\\",
            escaped_newline = "Line1\nLine2",
            escaped_tab = "Col1\tCol2"
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "escaped_quotes = \"He said \\\"Hello!\\\"\"", nil, true) ~= nil, "Escaped quotes should be encoded correctly")
        it(string.find(encoded_str, "escaped_backslash = \"C:\\\\Users\\\\\"", nil, true) ~= nil, "Escaped backslash should be encoded correctly")
        it(string.find(encoded_str, "escaped_newline = \"Line1\\nLine2\"", nil, true) ~= nil, "Escaped newline should be encoded correctly")
        it(string.find(encoded_str, [=[escaped_tab = "Col1\tCol2"]=], nil, true) ~= nil, "Escaped tab should be encoded correctly")
    end)

    define("encode table with empty values", function()
        local test_table = {
            empty_string = "",
            empty_array = setmetatable({}, { toml_type = "ARRAY_INLINE" }),
            -- TODO: this doesnt make a heading. Figure out what to do here
            empty_array_heading = setmetatable({}, { toml_type = "ARRAY" }),
            empty_table = setmetatable({}, { toml_type = tomlua_default.types.TABLE_INLINE }),
            empty_table_heading = {},
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "empty_string = \"\"", nil, true) ~= nil, "Empty string should be encoded correctly")
        it(string.find(encoded_str, "empty_array = []", nil, true) ~= nil, "Empty array should be encoded correctly")
        it(string.find(encoded_str, "empty_table = {}", nil, true) ~= nil, "Empty inline table should be encoded correctly")
    end)

    define("encode table with comments (not preserved)", function()
        local test_table = {
            key = "value" -- This comment will not be preserved
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "key = \"value\"", nil, true) ~= nil, "Key-value pair should be encoded correctly")
        it(string.find(encoded_str, "-- This comment will not be preserved", nil, true) == nil, "Comment should not be preserved")
    end)

    -- TODO:
    define("encode table with array of tables and nested arrays of tables", function()
        local test_table = {
            companies = {
                { name = "Company A", departments = { { name = "Sales", employees = 10 }, { name = "Marketing", employees = 5 } } },
                { name = "Company B", departments = { { name = "HR", employees = 3 } } }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[[companies]]", nil, true) ~= nil, "Should contain companies array of tables heading")
        it(string.find(encoded_str, "name = \"Company A\"", nil, true) ~= nil, "First company name should be correct")
        it(string.find(encoded_str, "[[companies.departments]]", nil, true) ~= nil, "Should contain nested array of tables heading")
        it(string.find(encoded_str, "name = \"Sales\"", nil, true) ~= nil, "First department name should be correct")
        it(string.find(encoded_str, "name = \"HR\"", nil, true) ~= nil, "Second company department name should be correct")
    end)

    define("encode table with array of tables and mixed key types", function()
        local test_table = {
            settings = {
                { ["key-with-dash"] = 1, ["key with spaces"] = 2 },
                { bare_key = 3, ["123"] = 4 }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[[settings]]", nil, true) ~= nil, "Should contain settings array of tables heading")
        it(string.find(encoded_str, "key-with-dash = 1", nil, true) ~= nil, "First setting key-with-dash should be correct")
        it(string.find(encoded_str, "\"key with spaces\" = 2", nil, true) ~= nil, "First setting key with spaces should be correct")
        it(string.find(encoded_str, "bare_key = 3", nil, true) ~= nil, "Second setting bare_key should be correct")
        it(string.find(encoded_str, "\"123\" = 4", nil, true) ~= nil, "Second setting numeric key as string should be correct")
    end)

    define("encode table with array of tables and unicode keys and values", function()
        local test_table = {
            products = {
                { name = "Laptop", ["型号"] = "XPS 15" },
                { name = "Mouse", ["颜色"] = "黑色" }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[[products]]", nil, true) ~= nil, "Should contain products array of tables heading")
        it(string.find(encoded_str, "name = \"Laptop\"", nil, true) ~= nil, "First product name should be correct")
        it(string.find(encoded_str, "\"型号\" = \"XPS 15\"", nil, true) ~= nil, "First product unicode key should be correct")
        it(string.find(encoded_str, "\"颜色\" = \"黑色\"", nil, true) ~= nil, "Second product unicode key should be correct")
    end)

    define("encode table with array of tables and escaped strings in values", function()
        local test_table = {
            messages = {
                { sender = "Alice", content = "Hello\nWorld" },
                { sender = "Bob", content = "He said \"Hi!\" again." }
            }
        }
        local encoded_str, err = tomlua_default.encode(test_table)
        it(err == nil, "Should not error during encoding")
        it(string.find(encoded_str, "[[messages]]", nil, true) ~= nil, "Should contain messages array of tables heading")
        it(string.find(encoded_str, "content = \"Hello\\nWorld\"", nil, true) ~= nil, "First message content with newline should be correct")
        it(string.find(encoded_str, "content = \"He said \\\"Hi!\\\" again.\"", nil, true) ~= nil, "Second message content with escaped quotes should be correct")
    end)
end
