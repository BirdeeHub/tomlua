return function(define, test_dir)
    -- Additional tests for TOMLUA decode functionality

    local tomlua_default = require("tomlua")
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
        local data, err = tomlua_default.decode(contents)
        it(data ~= nil, "Data should not be nil")
        it(err == nil, "Should not error on valid TOML")
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

    define("duplicate key error", function()
        local toml_str = [[
key = 1
key = 2
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "should error on duplicate key")
    end)

    define("duplicate table error", function()
        local toml_str = [[
[table]
key = 1
[table]
key = 2
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "should error on duplicate table")
    end)

    define("table defined by key is defined", function()
        local data, err = tomlua_default.decode([=[
database.replica_backup = { host = "replica2.local", port = 5434 }
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
]=])
        it(err ~= nil, "Should error")
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
        it(tomlua_default.type(data.table) == "TABLE_INLINE", "Inline table should be marked as TABLE_INLINE")
    end)

    define("mark_inline option for inline array", function()
        local toml_str = "array = [1, 2]"
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(tomlua_default.type_of(data.array) == tomlua_mark_inline.types.ARRAY_INLINE, "Inline array should be marked as ARRAY_INLINE")
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

    define("decode with wrong type", function()
        local data, err = tomlua_default.decode({ bleh = "haha" })
        it(err ~= nil, "Should error on table input")
    end)

    define("decode extra args ignored", function()
        local data, err = tomlua_default.decode("hehe = 'haha'", "ignored")
        it(data.hehe == "haha", "Value should be parsed correctly")
        it(err == nil, "Should ignore extra arguments")
    end)

    -- Basic Types
    define("decode basic string", function()
        local data, err = tomlua_default.decode([[key = "value"]])
        it(err == nil, "Should not error")
        it(data.key == "value", "String value should be correct")

        data, err = tomlua_default.decode([[key = 'another value']])
        it(err == nil, "Should not error with single quotes")
        it(data.key == "another value", "String value should be correct with single quotes")

        data, err = tomlua_default.decode([[key = """Multiline
String"""
]])
        it(err == nil, "Should not error with basic multiline string")
        it(data.key == "Multiline\nString", "Multiline string value should be correct")

        data, err = tomlua_default.decode([[key = '''Literal
String'''
]])
        it(err == nil, "Should not error with basic literal multiline string")
        it(data.key == "Literal\nString", "Literal multiline string value should be correct")
    end)

    define("decode dotted keys", function()
        local data, err = tomlua_default.decode("name.first = \"Tom\"")
        it(err == nil, "Should not error")
        it(data.name.first == "Tom", "Dotted key should be correct")

        data, err = tomlua_default.decode("a.b.c = 1")
        it(err == nil, "Should not error on deeply nested dotted key")
        it(data.a.b.c == 1, "Deeply nested dotted key should be correct")

        data, err = tomlua_default.decode("a.b = 1\na.c = 2")
        it(err == nil, "Should not error on sibling dotted keys")
        it(data.a.b == 1 and data.a.c == 2, "Sibling dotted keys should be correct")
    end)

    define("decode array of tables", function()
        local toml_str = [=[
[[products]]
name = "Laptop"
price = 1200

[[products]]
name = "Mouse"
price = 25
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].name == "Laptop", "First product name should be correct")
        it(data.products[2].price == 25, "Second product price should be correct")
    end)

    define("decode inline table with mixed content", function()
        local toml_str = [[item = { name = "Book", id = 123, tags = ["fiction", "bestseller"] }]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.item.name == "Book", "Inline table string should be correct")
        it(data.item.id == 123, "Inline table integer should be correct")
        it(eq(data.item.tags, {"fiction", "bestseller"}), "Inline table array should be correct")
    end)

    define("decode various integer formats", function()
        local data, err = tomlua_default.decode("key1 = +99\nkey2 = 42\nkey3 = 0\nkey4 = -17\nkey5 = 1_000\nkey6 = 5_349_221\nkey7 = 1_2_3_4_5")
        it(err == nil, "Should not error")
        it(data.key1 == 99, "Positive integer should be correct")
        it(data.key2 == 42, "Basic integer should be correct")
        it(data.key3 == 0, "Zero integer should be correct")
        it(data.key4 == -17, "Negative integer should be correct")
        it(data.key5 == 1000, "Underscored integer should be correct")
        it(data.key6 == 5349221, "Multiple underscored integer should be correct")
        it(data.key7 == 12345, "Multiple underscored integer with single digit groups should be correct")
    end)

    define("decode various float formats", function()
        local data, err = tomlua_default.decode("key1 = +1.0\nkey2 = 3.14\nkey3 = -0.01\nkey4 = 5e+22\nkey5 = 1e6\nkey6 = -2E-2\nkey7 = 6.626e-34\nkey8 = 9_224_617.445_991_228")
        it(err == nil, "Should not error")
        it(data.key1 == 1.0, "Positive float should be correct")
        it(data.key2 == 3.14, "Basic float should be correct")
        it(data.key3 == -0.01, "Negative float should be correct")
        it(data.key4 == 5e+22, "Exponent float should be correct")
        it(data.key5 == 1e6, "Positive exponent float should be correct")
        it(data.key6 == -2E-2, "Negative exponent float should be correct")
        it(data.key7 == 6.626e-34, "Scientific notation float should be correct")
        it(data.key8 == 9224617.445991228, "Underscored float should be correct")
    end)

    define("decode boolean values", function()
        local data, err = tomlua_default.decode("key1 = true\nkey2 = false")
        it(err == nil, "Should not error")
        it(data.key1 == true, "True boolean should be correct")
        it(data.key2 == false, "False boolean should be correct")
    end)

    define("decode various datetime formats", function()
        local data, err = tomlua_default.decode("key1 = 1979-05-27T07:32:00Z\nkey2 = 1979-05-27T00:32:00-07:00\nkey3 = 1979-05-27T07:32:00.999999-07:00\nkey4 = 1979-05-27T07:32:00\nkey5 = 1979-05-27\nkey6 = 07:32:00\nkey7 = 07:32:00.123456")
        it(err == nil, "Should not error")
        it(data.key1 == "1979-05-27T07:32:00Z", "Datetime with Z should be correct")
        it(data.key2 == "1979-05-27T00:32:00-07:00", "Datetime with offset should be correct")
        it(data.key3 == "1979-05-27T07:32:00.999999-07:00", "Datetime with fractional seconds and offset should be correct")
        it(data.key4 == "1979-05-27T07:32:00", "Local datetime should be correct")
        it(data.key5 == "1979-05-27", "Local date should be correct")
        it(data.key6 == "07:32:00", "Local time should be correct")
        it(data.key7 == "07:32:00.123456", "Local time with fractional seconds should be correct")
    end)

    define("decode comments", function()
        local toml_str = [[
# This is a full-line comment
key = "value" # This is an end-of-line comment
# Another comment
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.key == "value", "Key-value pair with comments should be parsed correctly")
    end)

    -- TODO: this finds an actual bug. keep this test, highest priority
    define("decode table array of tables with dotted keys", function()
        local toml_str = [=[
[[fruit]]
  name = "apple"

  [fruit.physical]
    color = "red"
    shape = "round"

  [[fruit.varieties]]
    name = "red delicious"

  [[fruit.varieties]]
    name = "granny smith"

[[fruit]]
  name = "banana"

  [[fruit.varieties]]
    name = "plantain"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.fruit == 2, "Should have two fruit entries")
        it(data.fruit[1].name == "apple", "First fruit name should be correct")
        it(data.fruit[1].physical.color == "red", "First fruit physical color should be correct")
        it(#data.fruit[1].varieties == 2, "First fruit should have two varieties")
        it(data.fruit[2].name == "banana", "Second fruit name should be correct")
        it(#data.fruit[2].varieties == 1, "Second fruit should have one variety")
    end)

    define("inline stuff should block later headings", function()
        local errtoml = [=[
        fruits = []
        [[fruits]]
        name = "apple"
        ]=]
        local _, err = tomlua_default.decode(errtoml)
        it(err ~= nil, "Array version should error")
        errtoml = [=[
        database = {}
        [database]
        type = "postgres"
        ]=]
        _, err = tomlua_default.decode(errtoml)
        it(err ~= nil, "Table version should error")
    end)

    define("duplicate table array error", function()
        local toml_str = [=[
[[table]]
key = 1
[[table]]
key = 2
[table]
key = 3
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "should error on duplicate table array and then table")
    end)

    -- TODO: errors on time only date type
    define("fancy_dates option: various date types", function()
        local toml_str = [[
date_offset = 1979-05-27T07:32:00Z
date_local = 1979-05-27T07:32:00
date_only = 1979-05-27
time_only = 07:32:00
]]
        local data, err = tomlua_fancy_dates.decode(toml_str)
        it(err == nil, "Should not error")
        it(type(data.date_offset) == "userdata", "Offset datetime should be userdata")
        it(data.date_offset.year == 1979 and data.date_offset.month == 5 and data.date_offset.day == 27, "Offset datetime components should be correct")
        it(type(data.date_local) == "userdata", "Local datetime should be userdata")
        it(type(data.date_only) == "userdata", "Local date should be userdata")
        it(type(data.time_only) == "userdata", "Local time should be userdata")
    end)

    define("int_keys option: mixed integer and string keys", function()
        local toml_str = [[
123 = "value1"
"456" = "value2"
key = "value3"
]]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(data[123] == "value1", "Integer key should be correct")
        it(data["456"] == "value2", "Quoted integer key should remain string")
        it(data.key == "value3", "Normal string key should be correct")
    end)

    define("multi_strings option: complex multiline strings", function()
        local toml_str = [[
str1 = """The quick brown fox jumps over the lazy dog."""
str2 = """\
  The quick brown fox jumps over the lazy dog.\
  The quick brown fox jumps over the lazy dog.\
  """
str3 = '''The quick brown fox jumps over the lazy dog.'''
]]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(tostring(data.str1) == "The quick brown fox jumps over the lazy dog.", "Basic multiline string should be correct")
        it(tostring(data.str2) == "The quick brown fox jumps over the lazy dog.The quick brown fox jumps over the lazy dog.", "Trimmed multiline string should be correct")
        it(tostring(data.str3) == "The quick brown fox jumps over the lazy dog.", "Basic literal multiline string should be correct")
    end)

    define("mark_inline option: nested inline structures", function()
        local toml_str = [[data = { arr = [1, {a=1}, 3], tbl = { x = 1, y = [4,5] } }]]
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(tomlua_default.type(data.data) == "TABLE_INLINE", "Outer table should be marked inline")
        it(tomlua_default.type(data.data.arr) == "ARRAY_INLINE", "Nested array should be marked inline")
        it(tomlua_default.type(data.data.arr[2]) == "TABLE_INLINE", "Nested inline table in array should be marked inline")
        it(tomlua_default.type(data.data.tbl) == "TABLE_INLINE", "Nested table should be marked inline")
        it(tomlua_default.type(data.data.tbl.y) == "ARRAY_INLINE", "Nested array in table should be marked inline")
    end)

    define("overflow_errors: integer and float edge cases", function()
        local toml_str = [[
int_max = 9223372036854775807
int_min = -9223372036854775808
float_max = 1.7976931348623157e+308
float_min = -1.7976931348623157e+308
]]
        local data, err = tomlua_overflow_errors.decode(toml_str)
        it(err == nil, "Should not error on max/min values")
        it(data.int_max == 9223372036854775807, "Max integer should be correct")
        it(data.int_min == -9223372036854775808, "Min integer should be correct")
        it(data.float_max == 1.7976931348623157e+308, "Max float should be correct")
        it(data.float_min == -1.7976931348623157e+308, "Min float should be correct")

        local huge_int_plus_one = "int_overflow = 9223372036854775808" -- Max int + 1
        data, err = tomlua_overflow_errors.decode(huge_int_plus_one)
        it(err ~= nil, "Should error on integer overflow")

        local huge_float_plus_one = "float_overflow = 1.7976931348623157e+309" -- Max float * 10
        data, err = tomlua_overflow_errors.decode(huge_float_plus_one)
        it(err ~= nil, "Should error on float overflow")
    end)

    define("underflow_errors: float edge cases", function()
        local toml_str = [[
float_normal = 1.0e-300
float_subnormal = 1.0e-320
]]
        local data, err = tomlua_underflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on subnormal float when underflow_errors")

        data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error on subnormal float otherwise")
        it(data.float_normal == 1.0e-300, "Normal float should be correct")
        it(data.float_subnormal ~= 0, "Subnormal float should be preserved or become 0")
    end)

    define("decode empty document", function()
        local data, err = tomlua_default.decode("")
        it(err == nil, "Should not error on empty string")
        it(eq(data, {}), "Empty string should decode to empty table")
    end)

    define("decode only comments", function()
        local toml_str = [[
# Comment 1
# Comment 2
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error on only comments")
        it(eq(data, {}), "Only comments should decode to empty table")
    end)

    define("decode complex nested structures", function()
        local toml_str = [=[
[general]
  title = "TOML Example"
  enabled = true

[owner]
  name = "John Doe"
  email = "john.doe@example.com"
  address = { street = "123 Main St", city = "Anytown" }

[[servers]]
  name = "alpha"
  ip = "192.168.1.1"
  ports = [ 8001, 8002 ]

[[servers]]
  name = "beta"
  ip = "192.168.1.2"
  ports = [ 8003 ]
  config = { max_connections = 100, timeout = 30 }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error on complex nested structures")
        it(data.general.title == "TOML Example", "General title should be correct")
        it(data.owner.address.city == "Anytown", "Owner address city should be correct")
        it(#data.servers == 2, "Should have two server entries")
        it(data.servers[1].name == "alpha", "First server name should be correct")
        it(data.servers[2].config.max_connections == 100, "Second server max_connections should be correct")
    end)

    define("decode keys with special characters", function()
        local toml_str = [[
"key-with-dash" = 1
'key with spaces' = 2
"key.with.dots" = 3
"key/with/slash" = 4
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data["key-with-dash"] == 1, "Key with dash should be correct")
        it(data["key with spaces"] == 2, "Key with spaces should be correct")
        it(data["key.with.dots"] == 3, "Key with dots should be correct")
        it(data["key/with/slash"] == 4, "Key with slash should be correct")
    end)

    define("decode unicode strings", function()
        local toml_str = [[
unicode_key = "你好世界"
emoji_key = "👋🌍"
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.unicode_key == "你好世界", "Unicode string should be correct")
        it(data.emoji_key == "👋🌍", "Emoji string should be correct")
    end)

    define("decode escaped characters in strings", function()
        local toml_str = [[
escaped_quotes = "He said \"Hello!\""
escaped_backslash = "C:\\Users\\"
escaped_newline = "Line1\nLine2"
escaped_tab = "Col1\tCol2"
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.escaped_quotes == "He said \"Hello!\"", "Escaped quotes should be correct")
        it(data.escaped_backslash == "C:\\Users\\", "Escaped backslash should be correct")
        it(data.escaped_newline == "Line1\nLine2", "Escaped newline should be correct")
        it(data.escaped_tab == "Col1\tCol2", "Escaped tab should be correct")
    end)

    define("decode empty tables and arrays", function()
        local toml_str = [[
empty_table = {}
empty_array = []
[another_empty_table]
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(eq(data.empty_table, {}), "Empty inline table should be correct")
        it(eq(data.empty_array, {}), "Empty inline array should be correct")
        it(eq(data.another_empty_table, {}), "Empty standard table should be correct")
    end)

    define("decode table with mixed array and table values", function()
        local toml_str = [[
[data]
  numbers = [1, 2, 3]
  info = { name = "test", version = 1.0 }
  flags = [true, false]
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(eq(data.data.numbers, {1, 2, 3}), "Array of numbers should be correct")
        it(data.data.info.name == "test", "Nested table string should be correct")
        it(eq(data.data.flags, {true, false}), "Array of booleans should be correct")
    end)

    define("decode table with array of inline tables", function()
        local toml_str = [[
points = [ { x = 1, y = 2 }, { x = 3, y = 4 } ]
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.points == 2, "Should have two points")
        it(data.points[1].x == 1 and data.points[1].y == 2, "First point should be correct")
        it(data.points[2].x == 3 and data.points[2].y == 4, "Second point should be correct")
    end)

    define("decode table with array of arrays", function()
        local toml_str = [[
matrix = [ [1, 2], [3, 4] ]
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.matrix == 2, "Should have two rows")
        it(eq(data.matrix[1], {1, 2}), "First row should be correct")
        it(eq(data.matrix[2], {3, 4}), "Second row should be correct")
    end)

    define("decode table with mixed array of tables and arrays", function()
        local toml_str = [[
items = [ { id = 1, value = "A" }, [10, 20], { id = 2, value = "B" } ]
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 3, "Should have three items")
        it(data.items[1].id == 1, "First item (table) should be correct")
        it(eq(data.items[2], {10, 20}), "Second item (array) should be correct")
        it(data.items[3].value == "B", "Third item (table) should be correct")
    end)

    define("decode table with complex key names", function()
        local toml_str = [[
"key.with.dots" = 1
"key-with-dashes" = 2
"key with spaces" = 3
"123" = 4
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data["key.with.dots"] == 1, "Key with dots should be accessible")
        it(data["key-with-dashes"] == 2, "Key with dashes should be accessible")
        it(data["key with spaces"] == 3, "Key with spaces should be accessible")
        it(data["123"] == 4, "Numeric key as string should be accessible")
    end)

    define("decode table with bare keys and quoted keys", function()
        local toml_str = [[
bare_key = 1
"quoted_key" = 2
'another_quoted_key' = 3
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.bare_key == 1, "Bare key should be correct")
        it(data.quoted_key == 2, "Double-quoted key should be correct")
        it(data.another_quoted_key == 3, "Single-quoted key should be correct")
    end)

    define("decode table with mixed bare and dotted keys", function()
        local toml_str = [[
[section]
  key = 1
  sub.key = 2
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.section.key == 1, "Bare key in section should be correct")
        it(data.section.sub.key == 2, "Dotted key in section should be correct")
    end)

    define("decode table with array of tables and inline tables", function()
        local toml_str = [=[
[[products]]
  id = 1
  details = { name = "Laptop", price = 1200 }

[[products]]
  id = 2
  details = { name = "Mouse", price = 25, available = true }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].id == 1, "First product ID should be correct")
        it(data.products[1].details.name == "Laptop", "First product details name should be correct")
        it(data.products[2].details.available == true, "Second product details available should be correct")
    end)

    -- TODO: basically the same as the fruits test. Fix the array heading pickup
    define("decode table with array of arrays of tables", function()
        local toml_str = [=[
[[groups]]
  [[groups.users]]
    name = "Alice"
    id = 1
  [[groups.users]]
    name = "Bob"
    id = 2

[[groups]]
  [[groups.users]]
    name = "Charlie"
    id = 3
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.groups == 2, "Should have two groups")
        it(#data.groups[1].users == 2, "First group should have two users")
        it(data.groups[1].users[1].name == "Alice", "First group first user name should be correct")
        it(#data.groups[2].users == 1, "Second group should have one user")
        it(data.groups[2].users[1].name == "Charlie", "Second group first user name should be correct")
    end)

    define("decode table with empty heading array of tables", function()
        local toml_str = [=[
[[empty_array_of_tables]]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(eq(data.empty_array_of_tables, { {} }), "Empty array of tables should be correct")
    end)

    define("decode table with empty inline table", function()
        local toml_str = [[
empty_inline_table = {}
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(eq(data.empty_inline_table, {}), "Empty inline table should be correct")
    end)

    define("decode table with empty array", function()
        local toml_str = [[
empty_array = []
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(eq(data.empty_array, {}), "Empty array should be correct")
    end)

    define("decode table with mixed types in array", function()
        local toml_str = [=[
mixed_array = [1, "hello", true, 3.14, { a = 1 }, [5, 6]]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.mixed_array == 6, "Mixed array should have 6 elements")
        it(data.mixed_array[1] == 1, "First element should be integer")
        it(data.mixed_array[2] == "hello", "Second element should be string")
        it(data.mixed_array[3] == true, "Third element should be boolean")
        it(data.mixed_array[4] == 3.14, "Fourth element should be float")
        it(data.mixed_array[5].a == 1, "Fifth element (table) should be correct")
        it(eq(data.mixed_array[6], {5, 6}), "Sixth element (array) should be correct")
    end)

    define("decode table with complex nested inline tables and arrays", function()
        local toml_str = [[
config = { settings = { theme = "dark", font_size = 12 }, features = [ { name = "auth", enabled = true }, { name = "logging", level = "info" } ] }
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.config.settings.theme == "dark", "Nested inline table value should be correct")
        it(data.config.features[1].name == "auth", "Array of inline tables first element should be correct")
        it(data.config.features[2].level == "info", "Array of inline tables second element should be correct")
    end)

    define("decode table with multiple top-level tables", function()
        local toml_str = [[
[table1]
  key1 = "value1"

[table2]
  key2 = "value2"
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.table1.key1 == "value1", "First top-level table should be correct")
        it(data.table2.key2 == "value2", "Second top-level table should be correct")
    end)

    define("decode table with nested tables and array of tables", function()
        local toml_str = [=[
[server]
  ip = "127.0.0.1"
  port = 8080

  [server.database]
    type = "postgres"
    host = "localhost"

  [[server.clients]]
    id = 1
    name = "ClientA"

  [[server.clients]]
    id = 2
    name = "ClientB"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.server.ip == "127.0.0.1", "Server IP should be correct")
        it(data.server.database.type == "postgres", "Server database type should be correct")
        it(#data.server.clients == 2, "Server should have two clients")
        it(data.server.clients[1].name == "ClientA", "First client name should be correct")
    end)

    define("decode table with mixed bare and quoted keys at top level", function()
        local toml_str = [[
key1 = 1
"key.2" = 2
'key-3' = 3
]]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(data.key1 == 1, "Bare key should be correct")
        it(data["key.2"] == 2, "Quoted dotted key should be correct")
        it(data["key-3"] == 3, "Quoted dashed key should be correct")
    end)

    define("decode table with array of tables and nested inline tables", function()
        local toml_str = [=[
[[users]]
  name = "Alice"
  profile = { age = 30, city = "New York" }

[[users]]
  name = "Bob"
  profile = { age = 24, city = "London", active = true }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.users == 2, "Should have two users")
        it(data.users[1].name == "Alice", "First user name should be correct")
        it(data.users[1].profile.city == "New York", "First user profile city should be correct")
        it(data.users[2].profile.active == true, "Second user profile active status should be correct")
    end)

    define("decode table with array of tables and nested arrays", function()
        local toml_str = [=[
[[products]]
  name = "Shirt"
  sizes = ["S", "M", "L"]

[[products]]
  name = "Pants"
  sizes = ["M", "L", "XL"]
  colors = ["blue", "black"]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].name == "Shirt", "First product name should be correct")
        it(eq(data.products[1].sizes, {"S", "M", "L"}), "First product sizes should be correct")
        it(eq(data.products[2].colors, {"blue", "black"}), "Second product colors should be correct")
    end)

    define("decode table with array of tables and mixed types", function()
        local toml_str = [=[
[[events]]
  id = 1
  type = "login"
  timestamp = 1979-05-27T07:32:00Z

[[events]]
  id = 2
  type = "logout"
  success = true
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.events == 2, "Should have two events")
        it(data.events[1].type == "login", "First event type should be correct")
        it(data.events[1].timestamp == "1979-05-27T07:32:00Z", "First event timestamp should be correct")
        it(data.events[2].success == true, "Second event success should be correct")
    end)

    define("decode table with array of tables and dotted keys", function()
        local toml_str = [=[
[[servers]]
  name = "server1"
  network.ip = "192.168.1.1"

[[servers]]
  name = "server2"
  network.ip = "192.168.1.2"
  network.port = 8080
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.servers == 2, "Should have two servers")
        it(data.servers[1].name == "server1", "First server name should be correct")
        it(data.servers[1].network.ip == "192.168.1.1", "First server network IP should be correct")
        it(data.servers[2].network.port == 8080, "Second server network port should be correct")
    end)

    define("decode table with array of tables and comments", function()
        local toml_str = [=[
[[tasks]] # First task
  name = "Task A"
  priority = 1 # High priority

[[tasks]] # Second task
  name = "Task B"
  priority = 2
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.tasks == 2, "Should have two tasks")
        it(data.tasks[1].name == "Task A", "First task name should be correct")
        it(data.tasks[1].priority == 1, "First task priority should be correct")
        it(data.tasks[2].name == "Task B", "Second task name should be correct")
    end)

    define("decode table with array of tables and empty tables", function()
        local toml_str = [=[
[[sections]]
  name = "Section 1"
  data = {}

[[sections]]
  name = "Section 2"
  data = { key = "value" }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.sections == 2, "Should have two sections")
        it(data.sections[1].name == "Section 1", "First section name should be correct")
        it(eq(data.sections[1].data, {}), "First section data should be empty table")
        it(data.sections[2].data.key == "value", "Second section data key should be correct")
    end)

    define("decode table with array of tables and empty arrays", function()
        local toml_str = [=[
[[items]]
  name = "Item 1"
  tags = []

[[items]]
  name = "Item 2"
  tags = ["tag1", "tag2"]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(data.items[1].name == "Item 1", "First item name should be correct")
        it(eq(data.items[1].tags, {}), "First item tags should be empty array")
        it(eq(data.items[2].tags, {"tag1", "tag2"}), "Second item tags should be correct")
    end)

    -- TODO: same as the fruits thing
    define("decode table with array of tables and nested array of tables", function()
        local toml_str = [=[
[[companies]]
  name = "Company A"
  [[companies.departments]]
    name = "Sales"
    employees = 10
  [[companies.departments]]
    name = "Marketing"
    employees = 5

[[companies]]
  name = "Company B"
  [[companies.departments]]
    name = "HR"
    employees = 3
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.companies == 2, "Should have two companies")
        it(data.companies[1].name == "Company A", "First company name should be correct")
        it(#data.companies[1].departments == 2, "First company should have two departments")
        it(data.companies[1].departments[1].name == "Sales", "First company first department name should be correct")
        it(data.companies[2].departments[1].name == "HR", "Second company first department name should be correct")
    end)

    define("decode table with array of tables and nested inline array of tables", function()
        local toml_str = [=[
[[projects]]
  name = "Project X"
  tasks = [ { id = 1, desc = "Design" }, { id = 2, desc = "Develop" } ]

[[projects]]
  name = "Project Y"
  tasks = [ { id = 3, desc = "Test" } ]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.projects == 2, "Should have two projects")
        it(data.projects[1].name == "Project X", "First project name should be correct")
        it(#data.projects[1].tasks == 2, "First project should have two tasks")
        it(data.projects[1].tasks[1].desc == "Design", "First project first task description should be correct")
        it(data.projects[2].tasks[1].desc == "Test", "Second project first task description should be correct")
    end)

    define("decode table with array of tables and nested inline arrays", function()
        local toml_str = [=[
[[configs]]
  name = "Config A"
  values = [1, 2, 3]

[[configs]]
  name = "Config B"
  values = [4, 5]
  options = ["opt1", "opt2"]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.configs == 2, "Should have two configs")
        it(data.configs[1].name == "Config A", "First config name should be correct")
        it(eq(data.configs[1].values, {1, 2, 3}), "First config values should be correct")
        it(eq(data.configs[2].options, {"opt1", "opt2"}), "Second config options should be correct")
    end)

    define("decode table with array of tables and nested inline tables", function()
        local toml_str = [=[
[[settings]]
  id = 1
  params = { timeout = 10, retries = 3 }

[[settings]]
  id = 2
  params = { timeout = 20, enabled = true }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.settings == 2, "Should have two settings")
        it(data.settings[1].id == 1, "First setting ID should be correct")
        it(data.settings[1].params.timeout == 10, "First setting params timeout should be correct")
        it(data.settings[2].params.enabled == true, "Second setting params enabled should be correct")
    end)

    define("decode table with array of tables and mixed nested structures", function()
        local toml_str = [=[
[[items]]
  name = "Item 1"
  data = { value = 10, tags = ["tag1", "tag2"] }

[[items]]
  name = "Item 2"
  data = { value = 20, options = { a = true, b = false } }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(data.items[1].name == "Item 1", "First item name should be correct")
        it(data.items[1].data.value == 10, "First item data value should be correct")
        it(eq(data.items[1].data.tags, {"tag1", "tag2"}), "First item data tags should be correct")
        it(data.items[2].data.options.a == true, "Second item data options a should be correct")
    end)

    define("decode table with array of tables and comments and empty lines", function()
        local toml_str = [=[
# Global comment

[[users]] # User A
  name = "Alice"
  age = 30


[[users]] # User B
  name = "Bob"
  age = 25
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.users == 2, "Should have two users")
        it(data.users[1].name == "Alice", "First user name should be correct")
        it(data.users[2].age == 25, "Second user age should be correct")
    end)

    define("decode table with array of tables and mixed key types", function()
        local toml_str = [=[
[[settings]]
  "key-with-dash" = 1
  'key with spaces' = 2

[[settings]]
  bare_key = 3
  "123" = 4
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.settings == 2, "Should have two settings")
        it(data.settings[1]["key-with-dash"] == 1, "First setting key-with-dash should be correct")
        it(data.settings[1]["key with spaces"] == 2, "First setting key with spaces should be correct")
        it(data.settings[2].bare_key == 3, "Second setting bare_key should be correct")
        it(data.settings[2]["123"] == 4, "Second setting numeric key as string should be correct")
    end)

    define("decode table with array of tables and unicode keys", function()
        local toml_str = [=[
[[languages]]
  name = "English"
  "hello_world" = "Hello World"

[[languages]]
  name = "Japanese"
  "こんにちは世界" = "こんにちは世界"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.languages == 2, "Should have two languages")
        it(data.languages[1].name == "English", "First language name should be correct")
        it(data.languages[1].hello_world == "Hello World", "First language hello_world should be correct")
        it(data.languages[2]["こんにちは世界"] == "こんにちは世界", "Second language unicode key should be correct")
    end)

    define("decode table with array of tables and escaped strings", function()
        local toml_str = [=[
[[messages]]
  id = 1
  text = "Line1\nLine2"

[[messages]]
  id = 2
  text = "He said \"Hi!\""
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.messages == 2, "Should have two messages")
        it(data.messages[1].text == "Line1\nLine2", "First message text with newline should be correct")
        it(data.messages[2].text == "He said \"Hi!\"", "Second message text with escaped quotes should be correct")
    end)

    define("decode table with array of tables and empty values", function()
        local toml_str = [=[
[[config]]
  name = "Config A"
  value = ""

[[config]]
  name = "Config B"
  value = "some value"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.config == 2, "Should have two configs")
        it(data.config[1].value == "", "First config value should be empty string")
        it(data.config[2].value == "some value", "Second config value should be correct")
    end)

    define("decode table with array of tables and boolean values", function()
        local toml_str = [=[
[[features]]
  name = "Feature A"
  enabled = true

[[features]]
  name = "Feature B"
  enabled = false
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.features == 2, "Should have two features")
        it(data.features[1].enabled == true, "First feature enabled should be true")
        it(data.features[2].enabled == false, "Second feature enabled should be false")
    end)

    define("decode table with array of tables and integer/float values", function()
        local toml_str = [=[
[[metrics]]
  name = "Metric A"
  value = 100

[[metrics]]
  name = "Metric B"
  value = 10.5
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.metrics == 2, "Should have two metrics")
        it(data.metrics[1].value == 100, "First metric value should be integer")
        it(data.metrics[2].value == 10.5, "Second metric value should be float")
    end)

    define("decode table with array of tables and date/time values", function()
        local toml_str = [=[
[[logs]]
  timestamp = 1979-05-27T07:32:00Z
  message = "Log entry 1"

[[logs]]
  timestamp = 1979-05-27
  message = "Log entry 2"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.logs == 2, "Should have two logs")
        it(data.logs[1].timestamp == "1979-05-27T07:32:00Z", "First log timestamp should be correct")
        it(tostring(data.logs[2].timestamp) == "1979-05-27", "Second log timestamp should be correct")
    end)

    define("decode table with array of tables and multiline strings", function()
        local toml_str = [=[
[[docs]]
  title = "Doc 1"
  content = """Line 1
Line 2"""

[[docs]]
  title = "Doc 2"
  content = '''Another
Multiline
String'''
]=]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.docs == 2, "Should have two docs")
        it(tostring(data.docs[1].content) == "Line 1\nLine 2", "First doc content should be correct")
        it(tostring(data.docs[2].content) == "Another\nMultiline\nString", "Second doc content should be correct")
    end)

    define("decode table with array of tables and inline tables/arrays", function()
        local toml_str = [=[
[[items]]
  id = 1
  details = { name = "Item A", price = 10 }
  tags = ["tag1", "tag2"]

[[items]]
  id = 2
  details = { name = "Item B", price = 20, available = true }
  tags = ["tag3"]
]=]
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(tomlua_default.type(data.items[1].details) == "TABLE_INLINE", "First item details should be inline table")
        it(tomlua_default.type(data.items[1].tags) == "ARRAY_INLINE", "First item tags should be inline array")
        it(data.items[2].details.available == true, "Second item details available should be correct")
    end)

    define("decode table with array of tables and mixed options", function()
        local toml_str = [=[
[[data]]
  key = 1
  123 = "value"

[[data]]
  key = 2
  key-with-dash = true
]=]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].key == 1, "First data key should be correct")
        it(data.data[1][123] == "value", "First data numeric key should be correct")
        it(data.data[2]["key-with-dash"] == true, "Second data dashed key should be correct")
    end)

    define("decode table with array of tables and overflow/underflow errors", function()
        local toml_str = [=[
[[numbers]]
  value = 1e400

[[numbers]]
  value = 1.0e-320
]=]
        local data, err = tomlua_overflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on overflow in array of tables")

        data, err = tomlua_underflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on underflow in array of tables")
    end)

    define("decode table with array of tables and errors", function()
        local toml_str = [=[
[[items]]
  key = 1
  key = 2
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "Should error on duplicate key in array of tables")

        local toml_str_2 = [=[
[[items]]
  [items.sub]
    val = 1
  [items.sub]
    val = 2
]=]
        data, err = tomlua_default.decode(toml_str_2)
        it(err ~= nil, "Should error on duplicate table in array of tables")
    end)

    define("decode table with array of tables and mixed types and comments", function()
        local toml_str = [=[
[[data]] # First entry
  id = 1
  name = "Entry A" # Name of entry
  values = [1, 2, 3] # Some values

[[data]] # Second entry
  id = 2
  name = "Entry B"
  config = { enabled = true, retries = 5 } # Configuration
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].id == 1, "First data ID should be correct")
        it(data.data[1].name == "Entry A", "First data name should be correct")
        it(eq(data.data[1].values, {1, 2, 3}), "First data values should be correct")
        it(data.data[2].config.enabled == true, "Second data config enabled should be correct")
    end)

    define("decode table with array of tables and complex nested structures", function()
        local toml_str = [=[
[[servers]]
  name = "Web Server"
  config = { ip = "192.168.1.1", ports = [80, 443], admin = { user = "admin", pass = "secret" } }
  status = "running"

[[servers]]
  name = "DB Server"
  config = { ip = "192.168.1.2", port = 5432 }
  status = "stopped"
  logs = [ { timestamp = 1979-05-27T07:32:00Z, message = "Error" } ]
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.servers == 2, "Should have two servers")
        it(data.servers[1].name == "Web Server", "First server name should be correct")
        it(data.servers[1].config.admin.user == "admin", "First server admin user should be correct")
        it(data.servers[2].config.ip == "192.168.1.2", "Second server IP should be correct")
        it(data.servers[2].logs[1].message == "Error", "Second server log message should be correct")
    end)

    define("decode table with array of tables and mixed bare and quoted keys", function()
        local toml_str = [=[
[[users]]
  name = "Alice"
  "user-id" = 1

[[users]]
  name = "Bob"
  'user.id' = 2
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.users == 2, "Should have two users")
        it(data.users[1].name == "Alice", "First user name should be correct")
        it(data.users[1]["user-id"] == 1, "First user-id should be correct")
        it(data.users[2]["user.id"] == 2, "Second user.id should be correct")
    end)

    define("decode table with array of tables and unicode keys and values", function()
        local toml_str = [=[
[[products]]
  name = "Laptop"
  "型号" = "XPS 15"

[[products]]
  name = "Mouse"
  "颜色" = "黑色"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].name == "Laptop", "First product name should be correct")
        it(data.products[1]["型号"] == "XPS 15", "First product unicode key should be correct")
        it(data.products[2]["颜色"] == "黑色", "Second product unicode key should be correct")
    end)

    define("decode table with array of tables and escaped strings in values", function()
        local toml_str = [=[
[[messages]]
  sender = "Alice"
  content = "Hello\nWorld"

[[messages]]
  sender = "Bob"
  content = "He said \"Hi!\" again."
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.messages == 2, "Should have two messages")
        it(data.messages[1].content == "Hello\nWorld", "First message content with newline should be correct")
        it(data.messages[2].content == "He said \"Hi!\" again.", "Second message content with escaped quotes should be correct")
    end)

    define("decode table with array of tables and empty values and comments", function()
        local toml_str = [=[
[[config]] # Configuration A
  name = "Config A"
  value = "" # Empty value

[[config]] # Configuration B
  name = "Config B"
  value = "some value" # Non-empty value
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.config == 2, "Should have two configs")
        it(data.config[1].value == "", "First config value should be empty string")
        it(data.config[2].value == "some value", "Second config value should be correct")
    end)

    define("decode table with array of tables and boolean values and comments", function()
        local toml_str = [=[
[[features]] # Feature A
  name = "Feature A"
  enabled = true # Enabled

[[features]] # Feature B
  name = "Feature B"
  enabled = false # Disabled
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.features == 2, "Should have two features")
        it(data.features[1].enabled == true, "First feature enabled should be true")
        it(data.features[2].enabled == false, "Second feature enabled should be false")
    end)

    define("decode table with array of tables and integer/float values and comments", function()
        local toml_str = [=[
[[metrics]] # Metric A
  name = "Metric A"
  value = 100 # Integer value

[[metrics]] # Metric B
  name = "Metric B"
  value = 10.5 # Float value
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.metrics == 2, "Should have two metrics")
        it(data.metrics[1].value == 100, "First metric value should be integer")
        it(data.metrics[2].value == 10.5, "Second metric value should be float")
    end)

    -- TODO: fix this weird extra space thing
    define("date/time values in array headings with fancy dates", function()
        local toml_str = [=[
[[logs]] # Log entry 1
  timestamp = 1979-05-27T07:32:00Z # Offset datetime
  message = "Log entry 1"

[[logs]] # Log entry 2
  timestamp = 1979-05-27 # Local date
  message = "Log entry 2"
]=]
        local data, err = tomlua_fancy_dates.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.logs == 2, "Should have two logs")
        it(tostring(data.logs[1].timestamp) == "1979-05-27T07:32:00Z", "First log timestamp should be correct")
        it(tostring(data.logs[2].timestamp) == "1979-05-27", "Second log timestamp should be correct")
    end)

    -- TODO: fix this error for invalid date type
    define("date/time values in array headings without fancy dates", function()
        local toml_str = [=[
[[logs]] # Log entry 1
  timestamp = 1979-05-27T07:32:00Z # Offset datetime
  message = "Log entry 1"

[[logs]] # Log entry 2
  timestamp = 1979-05-27 # Local date
  message = "Log entry 2"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.logs == 2, "Should have two logs")
        it(data.logs[1].timestamp == "1979-05-27T07:32:00Z", "First log timestamp should be correct")
        it(data.logs[2].timestamp == "1979-05-27", "Second log timestamp should be correct")
    end)

    define("decode table with array of tables and multiline strings and comments", function()
        local toml_str = [=[
[[docs]] # Document 1
  title = "Doc 1"
  content = """Line 1
Line 2""" # Multiline content

[[docs]] # Document 2
  title = "Doc 2"
  content = '''Another
Multiline
String''' # Another multiline content
]=]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.docs == 2, "Should have two docs")
        it(tostring(data.docs[1].content) == "Line 1\nLine 2", "First doc content should be correct")
        it(tostring(data.docs[2].content) == "Another\nMultiline\nString", "Second doc content should be correct")
    end)

    define("decode table with array of tables and inline tables/arrays and comments", function()
        local toml_str = [=[
[[items]] # Item 1
  id = 1
  details = { name = "Item A", price = 10 } # Inline table details
  tags = ["tag1", "tag2"] # Inline array tags

[[items]] # Item 2
  id = 2
  details = { name = "Item B", price = 20, available = true } # Another inline table
  tags = ["tag3"] # Another inline array
]=]
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(tomlua_default.type(data.items[1].details) == "TABLE_INLINE", "First item details should be inline table")
        it(tomlua_default.type(data.items[1].tags) == "ARRAY_INLINE", "First item tags should be inline array")
        it(data.items[2].details.available == true, "Second item details available should be correct")
    end)

    define("decode table with array of tables and mixed options and comments", function()
        local toml_str = [=[
[[data]] # Data entry 1
  key = 1 # Integer key
  123 = "value" # Quoted numeric key

[[data]] # Data entry 2
  key = 2 # Another integer key
  "key-with-dash" = true # Quoted dashed key
]=]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].key == 1, "First data key should be correct")
        it(data.data[1][123] == "value", "First data numeric key should be correct")
        it(data.data[2]["key-with-dash"] == true, "Second data dashed key should be correct")
    end)

    define("decode table with array of tables and overflow/underflow errors and comments", function()
        local toml_str = [=[
[[numbers]] # First number
  value = 1e400 # Overflow value

[[numbers]] # Second number
  value = 1.0e-320 # Underflow value
]=]
        local data, err = tomlua_overflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on overflow in array of tables")

        data, err = tomlua_underflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on underflow in array of tables")
    end)

    define("decode table with array of tables and errors and comments", function()
        local toml_str = [=[
[[items]] # Item 1
  key = 1
  key = 2 # Duplicate key
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "Should error on duplicate key in array of tables")

        local toml_str_2 = [=[
[[items]] # Item 1
  [items.sub]
    val = 1
  [items.sub]
    val = 2 # Duplicate table
]=]
        data, err = tomlua_default.decode(toml_str_2)
        it(err ~= nil, "Should error on duplicate table in array of tables")
    end)

    define("decode table with array of tables and mixed types and comments and empty lines", function()
        local toml_str = [=[
# Global comment

[[data]] # First entry
  id = 1
  name = "Entry A" # Name of entry
  values = [1, 2, 3] # Some values


[[data]] # Second entry
  id = 2
  name = "Entry B"
  config = { enabled = true, retries = 5 } # Configuration
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].id == 1, "First data ID should be correct")
        it(data.data[1].name == "Entry A", "First data name should be correct")
        it(eq(data.data[1].values, {1, 2, 3}), "First data values should be correct")
        it(data.data[2].config.enabled == true, "Second data config enabled should be correct")
    end)

    define("decode table with array of tables and complex nested structures and comments", function()
        local toml_str = [=[
[[servers]] # Web Server Configuration
  name = "Web Server"
  config = { ip = "192.168.1.1", ports = [80, 443], admin = { user = "admin", pass = "secret" } } # Server configuration
  status = "running" # Server status

[[servers]] # DB Server Configuration
  name = "DB Server"
  config = { ip = "192.168.1.2", port = 5432 } # Database configuration
  status = "stopped" # Database status
  logs = [ { timestamp = 1979-05-27T07:32:00Z, message = "Error" } ] # Log entries
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.servers == 2, "Should have two servers")
        it(data.servers[1].name == "Web Server", "First server name should be correct")
        it(data.servers[1].config.admin.user == "admin", "First server admin user should be correct")
        it(data.servers[2].config.ip == "192.168.1.2", "Second server IP should be correct")
        it(data.servers[2].logs[1].message == "Error", "Second server log message should be correct")
    end)

    define("decode table with array of tables and mixed bare and quoted keys and comments", function()
        local toml_str = [=[
[[users]] # User A details
  name = "Alice"
  "user-id" = 1 # User ID with dash

[[users]] # User B details
  name = "Bob"
  'user.id' = 2 # User ID with dot
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.users == 2, "Should have two users")
        it(data.users[1].name == "Alice", "First user name should be correct")
        it(data.users[1]["user-id"] == 1, "First user-id should be correct")
        it(data.users[2]["user.id"] == 2, "Second user.id should be correct")
    end)

    define("decode table with array of tables and unicode keys and values and comments", function()
        local toml_str = [=[
[[products]] # Product 1
  name = "Laptop"
  "型号" = "XPS 15" # Model number in Chinese

[[products]] # Product 2
  name = "Mouse"
  "颜色" = "黑色" # Color in Chinese
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].name == "Laptop", "First product name should be correct")
        it(data.products[1]["型号"] == "XPS 15", "First product unicode key should be correct")
        it(data.products[2]["颜色"] == "黑色", "Second product unicode key should be correct")
    end)

    define("decode table with array of tables and escaped strings in values and comments", function()
        local toml_str = [=[
[[messages]] # Message from Alice
  sender = "Alice"
  content = "Hello\nWorld" # Message with newline

[[messages]] # Message from Bob
  sender = "Bob"
  content = "He said \"Hi!\" again." # Message with escaped quotes
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.messages == 2, "Should have two messages")
        it(data.messages[1].content == "Hello\nWorld", "First message content with newline should be correct")
        it(data.messages[2].content == "He said \"Hi!\" again.", "Second message content with escaped quotes should be correct")
    end)

    define("decode table with array of tables and empty values and comments and empty lines", function()
        local toml_str = [=[
# Global config

[[config]] # Configuration A
  name = "Config A"
  value = "" # Empty value


[[config]] # Configuration B
  name = "Config B"
  value = "some value" # Non-empty value
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.config == 2, "Should have two configs")
        it(data.config[1].value == "", "First config value should be empty string")
        it(data.config[2].value == "some value", "Second config value should be correct")
    end)

    define("decode table with array of tables and boolean values and comments and empty lines", function()
        local toml_str = [=[
# Feature flags

[[features]] # Feature A
  name = "Feature A"
  enabled = true # Enabled


[[features]] # Feature B
  name = "Feature B"
  enabled = false # Disabled
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.features == 2, "Should have two features")
        it(data.features[1].enabled == true, "First feature enabled should be true")
        it(data.features[2].enabled == false, "Second feature enabled should be false")
    end)

    define("decode table with array of tables and integer/float values and comments and empty lines", function()
        local toml_str = [=[
# Metrics data

[[metrics]] # Metric A
  name = "Metric A"
  value = 100 # Integer value


[[metrics]] # Metric B
  name = "Metric B"
  value = 10.5 # Float value
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.metrics == 2, "Should have two metrics")
        it(data.metrics[1].value == 100, "First metric value should be integer")
        it(data.metrics[2].value == 10.5, "Second metric value should be float")
    end)

    define("decode table with array of tables and date/time values and comments and empty lines", function()
        local toml_str = [=[
# Log entries

[[logs]] # Log entry 1
  timestamp = 1979-05-27T07:32:00Z # Offset datetime
  message = "Log entry 1"


[[logs]] # Log entry 2
  timestamp = 1979-05-27 # Local date
  message = "Log entry 2"
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.logs == 2, "Should have two logs")
        it(tostring(data.logs[1].timestamp) == "1979-05-27T07:32:00Z", "First log timestamp should be correct")
        it(tostring(data.logs[2].timestamp) == "1979-05-27", "Second log timestamp should be correct")
    end)

    define("decode table with array of tables and multiline strings and comments and empty lines", function()
        local toml_str = [=[
# Documents

[[docs]] # Document 1
  title = "Doc 1"
  content = """Line 1
Line 2""" # Multiline content


[[docs]] # Document 2
  title = "Doc 2"
  content = '''Another
Multiline
String''' # Another multiline content
]=]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.docs == 2, "Should have two docs")
        it(tostring(data.docs[1].content) == "Line 1\nLine 2", "First doc content should be correct")
        it(tostring(data.docs[2].content) == "Another\nMultiline\nString", "Second doc content should be correct")
    end)

    define("decode table with array of tables and inline tables/arrays and comments and empty lines", function()
        local toml_str = [=[
# Item configurations

[[items]] # Item 1
  id = 1
  details = { name = "Item A", price = 10 } # Inline table details
  tags = ["tag1", "tag2"] # Inline array tags


[[items]] # Item 2
  id = 2
  details = { name = "Item B", price = 20, available = true } # Another inline table
  tags = ["tag3"] # Another inline array
]=]
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(tomlua_default.type(data.items[1].details) == "TABLE_INLINE", "First item details should be inline table")
        it(tomlua_default.type(data.items[1].tags) == "ARRAY_INLINE", "First item tags should be inline array")
        it(data.items[2].details.available == true, "Second item details available should be correct")
    end)

    define("decode table with array of tables and mixed options and comments and empty lines", function()
        local toml_str = [=[
# Data entries

[[data]] # Data entry 1
  key = 1 # Integer key
  123 = "value" # Quoted numeric key


[[data]] # Data entry 2
  key = 2 # Another integer key
  "key-with-dash" = true # Quoted dashed key
]=]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].key == 1, "First data key should be correct")
        it(data.data[1][123] == "value", "First data numeric key should be correct")
        it(data.data[2]["key-with-dash"] == true, "Second data dashed key should be correct")
    end)

    define("decode table with array of tables and overflow/underflow errors and comments and empty lines", function()
        local toml_str = [=[
# Number tests

[[numbers]] # First number
  value = 1e400 # Overflow value


[[numbers]] # Second number
  value = 1.0e-320 # Underflow value
]=]
        local data, err = tomlua_overflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on overflow in array of tables")

        data, err = tomlua_underflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on underflow in array of tables")
    end)

    define("decode table with array of tables and errors and comments and empty lines", function()
        local toml_str = [=[
# Item configurations

[[items]] # Item 1
  key = 1
  key = 2 # Duplicate key


[[items]] # Item 2
  [items.sub]
    val = 1
  [items.sub]
    val = 2 # Duplicate table
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "Should error on duplicate key in array of tables")

        local toml_str_2 = [=[
[[items]] # Item 1
  [items.sub]
    val = 1
  [items.sub]
    val = 2 # Duplicate table
]=]
        data, err = tomlua_default.decode(toml_str_2)
        it(err ~= nil, "Should error on duplicate table in array of tables")
    end)

    define("decode table with array of tables and mixed types and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Complex configuration

[[servers]] # Web Server Configuration
  name = "Web Server"
  config = { ip = "192.168.1.1", ports = [80, 443], admin = { user = "admin", pass = "secret" } } # Server configuration
  status = "running" # Server status


[[servers]] # DB Server Configuration
  name = "DB Server"
  config = { ip = "192.168.1.2", port = 5432 } # Database configuration
  status = "stopped" # Database status
  logs = [ { timestamp = 1979-05-27T07:32:00Z, message = "Error" } ] # Log entries
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.servers == 2, "Should have two servers")
        it(data.servers[1].name == "Web Server", "First server name should be correct")
        it(data.servers[1].config.admin.user == "admin", "First server admin user should be correct")
        it(data.servers[2].config.ip == "192.168.1.2", "Second server IP should be correct")
        it(data.servers[2].logs[1].message == "Error", "Second server log message should be correct")
    end)

    define("decode table with array of tables and mixed bare and quoted keys and comments and empty lines", function()
        local toml_str = [=[
# User configurations

[[users]] # User A details
  name = "Alice"
  "user-id" = 1 # User ID with dash


[[users]] # User B details
  name = "Bob"
  'user.id' = 2 # User ID with dot
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.users == 2, "Should have two users")
        it(data.users[1].name == "Alice", "First user name should be correct")
        it(data.users[1]["user-id"] == 1, "First user-id should be correct")
        it(data.users[2]["user.id"] == 2, "Second user.id should be correct")
    end)

    define("decode table with array of tables and unicode keys and values and comments and empty lines", function()
        local toml_str = [=[
# Product information

[[products]] # Product 1
  name = "Laptop"
  "型号" = "XPS 15" # Model number in Chinese


[[products]] # Product 2
  name = "Mouse"
  "颜色" = "黑色" # Color in Chinese
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.products == 2, "Should have two products")
        it(data.products[1].name == "Laptop", "First product name should be correct")
        it(data.products[1]["型号"] == "XPS 15", "First product unicode key should be correct")
        it(data.products[2]["颜色"] == "黑色", "Second product unicode key should be correct")
    end)

    define("decode table with array of tables and escaped strings in values and comments and empty lines", function()
        local toml_str = [=[
# Message logs

[[messages]] # Message from Alice
  sender = "Alice"
  content = "Hello\nWorld" # Message with newline


[[messages]] # Message from Bob
  sender = "Bob"
  content = "He said \"Hi!\" again." # Message with escaped quotes
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.messages == 2, "Should have two messages")
        it(data.messages[1].content == "Hello\nWorld", "First message content with newline should be correct")
        it(data.messages[2].content == "He said \"Hi!\" again.", "Second message content with escaped quotes should be correct")
    end)

    define("decode table with array of tables and empty values and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Complex configuration with empty values

[[config]] # Configuration A
  name = "Config A"
  value = "" # Empty value
  details = { enabled = true, tags = [] } # Details with empty array


[[config]] # Configuration B
  name = "Config B"
  value = "some value" # Non-empty value
  details = { enabled = false, options = { a = 1, b = 2 } } # Details with nested table
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.config == 2, "Should have two configs")
        it(data.config[1].value == "", "First config value should be empty string")
        it(eq(data.config[1].details.tags, {}), "First config details tags should be empty array")
        it(data.config[2].details.options.a == 1, "Second config details options a should be correct")
    end)

    define("decode table with array of tables and boolean values and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Feature flags with complex structure

[[features]] # Feature A
  name = "Feature A"
  enabled = true # Enabled
  settings = { level = "high", active = true }


[[features]] # Feature B
  name = "Feature B"
  enabled = false # Disabled
  settings = { level = "low", active = false, options = [1, 2] }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.features == 2, "Should have two features")
        it(data.features[1].enabled == true, "First feature enabled should be true")
        it(data.features[1].settings.level == "high", "First feature settings level should be correct")
        it(data.features[2].settings.options[1] == 1, "Second feature settings options first element should be correct")
    end)

    define("decode table with array of tables and integer/float values and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Metrics data with complex structure

[[metrics]] # Metric A
  name = "Metric A"
  value = 100 # Integer value
  thresholds = { warn = 80, error = 120 }


[[metrics]] # Metric B
  name = "Metric B"
  value = 10.5 # Float value
  thresholds = { warn = 5.0, error = 15.0, limits = [0.0, 20.0] }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.metrics == 2, "Should have two metrics")
        it(data.metrics[1].value == 100, "First metric value should be integer")
        it(data.metrics[1].thresholds.warn == 80, "First metric thresholds warn should be correct")
        it(data.metrics[2].thresholds.limits[2] == 20.0, "Second metric thresholds limits second element should be correct")
    end)

    define("decode table with array of tables and date/time values and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Log entries with complex structure

[[logs]] # Log entry 1
  timestamp = 1979-05-27T07:32:00Z # Offset datetime
  message = "Log entry 1"
  details = { user = "admin", event = "login" }


[[logs]] # Log entry 2
  timestamp = 1979-05-27 # Local date
  message = "Log entry 2"
  details = { user = "guest", event = "logout", duration = 10 }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.logs == 2, "Should have two logs")
        it(data.logs[1].timestamp == "1979-05-27T07:32:00Z", "First log timestamp should be correct")
        it(data.logs[1].details.user == "admin", "First log details user should be correct")
        it(data.logs[2].details.duration == 10, "Second log details duration should be correct")
    end)

    define("decode table with array of tables and multiline strings and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Documents with complex structure

[[docs]] # Document 1
  title = "Doc 1"
  content = """Line 1
Line 2""" # Multiline content
  metadata = { author = "Alice", version = 1 }


[[docs]] # Document 2
  title = "Doc 2"
  content = '''Another
Multiline
String''' # Another multiline content
  metadata = { author = "Bob", version = 2, tags = ["tech", "programming"] }
]=]
        local data, err = tomlua_multi_strings.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.docs == 2, "Should have two docs")
        it(tostring(data.docs[1].content) == "Line 1\nLine 2", "First doc content should be correct")
        it(data.docs[1].metadata.author == "Alice", "First doc metadata author should be correct")
        it(eq(data.docs[2].metadata.tags, {"tech", "programming"}), "Second doc metadata tags should be correct")
    end)

    define("decode table with array of tables and inline tables/arrays and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Item configurations with complex structure

[[items]] # Item 1
  id = 1
  details = { name = "Item A", price = 10 } # Inline table details
  tags = ["tag1", "tag2"] # Inline array tags
  options = { enabled = true, values = [100, 200] }


[[items]] # Item 2
  id = 2
  details = { name = "Item B", price = 20, available = true } # Another inline table
  tags = ["tag3"]
  options = { enabled = false, values = [300], metadata = { type = "test" } }
]=]
        local data, err = tomlua_mark_inline.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.items == 2, "Should have two items")
        it(tomlua_default.type(data.items[1].details) == "TABLE_INLINE", "First item details should be inline table")
        it(tomlua_default.type(data.items[1].tags) == "ARRAY_INLINE", "First item tags should be inline array")
        it(data.items[1].options.values[1] == 100, "First item options values first element should be correct")
        it(data.items[2].options.metadata.type == "test", "Second item options metadata type should be correct")
    end)

    define("decode table with array of tables and mixed options and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Data entries with complex structure

[[data]] # Data entry 1
  key = 1 # Integer key
  123 = "value" # Quoted numeric key
  metadata = { type = "numeric", unit = "count" }


[[data]] # Data entry 2
  key = 2 # Another integer key
  "key-with-dash" = true # Quoted dashed key
  metadata = { type = "boolean", flag = true, options = ["a", "b"] }
]=]
        local data, err = tomlua_int_keys.decode(toml_str)
        it(err == nil, "Should not error")
        it(#data.data == 2, "Should have two data entries")
        it(data.data[1].key == 1, "First data key should be correct")
        it(data.data[1][123] == "value", "First data numeric key should be correct")
        it(data.data[1].metadata.unit == "count", "First data metadata unit should be correct")
        it(data.data[2].metadata.options[1] == "a", "Second data metadata options first element should be correct")
    end)

    define("decode table with array of tables and overflow/underflow errors and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Number tests with complex structure

[[numbers]] # First number
  value = 1e400 # Overflow value
  info = { type = "float", range = "huge" }


[[numbers]] # Second number
  value = 1.0e-320 # Underflow value
  info = { type = "float", range = "tiny", details = { precision = "low" } }
]=]
        local data, err = tomlua_overflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on overflow in array of tables")

        data, err = tomlua_underflow_errors.decode(toml_str)
        it(err ~= nil, "Should error on underflow in array of tables")
    end)

    define("decode table with array of tables and errors and comments and empty lines and complex nested structures", function()
        local toml_str = [=[
# Item configurations with errors

[[items]] # Item 1
  key = 1
  key = 2 # Duplicate key
  details = { name = "Item A" }


[[items]] # Item 2
  [items.sub]
    val = 1
  [items.sub]
    val = 2 # Duplicate table
  details = { name = "Item B" }
]=]
        local data, err = tomlua_default.decode(toml_str)
        it(err ~= nil, "Should error on duplicate key in array of tables")

        local toml_str_2 = [=[
[[items]] # Item 1
  [items.sub]
    val = 1
  [items.sub]
    val = 2 # Duplicate table
  details = { name = "Item B" }
]=]
        data, err = tomlua_default.decode(toml_str_2)
        it(err ~= nil, "Should error on duplicate table in array of tables")
    end)

end
