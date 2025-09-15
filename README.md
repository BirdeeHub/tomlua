# TOMLUA

> [!WARNING]
> This is not finished software yet! But the pieces are all there! Needs polish.
> Tests/CI, better docs, error descriptions which are less lazy, push to luarocks and nixpkgs, etc.

Parses toml files into and out of lua tables ASAP.

Implemented in C

This is not intended to replace packages like [toml_edit](https://github.com/nvim-neorocks/toml-edit.lua), which are slower but better for editing existing files.

It will not be re-emitting comments, and it will not re-output in the same format as the ingested file.

If you wish to edit existing toml, you should do that using a package more suited for that.

This is instead intended for hot-path parsing of toml files.

Basic benchmarking shows promising results.

Speed is slightly slower than cjson but comparable despite parsing toml rather than json (cjson is 1.2x - 2.0x faster, depending on the file and settings)

But it is 10x faster than toml_edit

---

build with make and add the tomlua.so to your LUA_CPATH (or package.cpath at runtime) or install via luarocks

make requires that LUA_INCDIR be set to the path of your lua headers,
and (without further configuration) you should also have lua in your path and be in the root of the repository.

If you do not have gcc, install it, or set CC to clang or some other c compiler.

Yes there will eventually be better build instructions.

It is also packaged via nix (flake or flakeless)!
It offers an overlay to add it to any of the package sets,
packages for all versions of lua 5.1+,
and has a dev shell you can use to build it via make as well.

```lua
package.cpath = package.cpath .. ";/path/to/tomlua/lib/?.so"

local tomlua = require("tomlua")

-- you can call it like a function and get a copy with different options
local tomlua2 = tomlua {
    -- fancy_tables allows multiline inline tables with a trailing comma
    -- key = value still must be on the same line
    -- the tables, much like arrays, must still start on the same line as their key as well
    fancy_tables = false,
    -- adds further uniqueness checking to be fully compliant with the toml spec
    strict = false,
    -- causes keys that parse as lua integers to be interpreted as integer keys
    -- unless they were enclosed in "" or ''
    int_keys = false,
    -- causes dates to be parsed into a userdata type
    -- encode will write them correctly as well
    fancy_dates = false,
    -- causes multiline strings to be parsed into a userdata type
    -- which can be converted to a lua string with tostring
    multi_strings = false,
}

-- or you can set them directly on the current object
tomlua.opts.fancy_dates = true

local data, err = tomlua.decode(some_string)

-- or read into an existing table
data, err = tomlua.decode(some_string, { some = "defaults" })

-- encode always accepts fancy dates, never outputs fancy tables, and is unaffected by all opts
-- instead you may customize dates by replacing them with tomlua date types
-- and you may make arrays appear as tables, or empty tables appear as arrays,
-- local empty_toml_array = setmetatable({}, {
--   toml_type = tomlua.types.ARRAY
-- })
-- local a_toml_table = setmetatable({ "a", "table", "with", "integer", "keys" }, {
--   toml_type = tomlua.types.TABLE
-- })
local str, err = tomlua.encode(some_table)

tomlua.types = {
    UNTYPED, -- 0  -- Untyped TOML Item
    STRING, -- 1  -- lua string
    STRING_MULTI, -- 2  -- lua string
    INTEGER, -- 3  -- lua number
    FLOAT, -- 4  -- lua number
    BOOL, -- 5  -- lua bool
    ARRAY, -- 6  -- lua table
    TABLE, -- 7  -- lua table
    LOCAL_DATE, -- 8  -- string, or userdata with fancy_dates
    LOCAL_TIME, -- 9  -- string, or userdata with fancy_dates
    LOCAL_DATETIME, -- 10  -- string, or userdata with fancy_dates
    OFFSET_DATETIME, -- 11  -- string, or userdata with fancy_dates
}
tomlua.typename(typenumber) --> tomlua.types[result] = typenumber

-- get the type tomlua thinks a lua value is
tomlua.type(value) --> number from tomlua.types

local toml_multiline_str = tomlua.str_2_mul("hello\nworld")
local regular_str = tostring(toml_multiline_str)

-- accepts utc_timestamp,
-- toml date string,
-- a table or array with the same fields as date,
-- or another date
---@type fun(string|number|table|userdata?): userdata
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
date.year = 2222 -- set values
for k, v in getmetatable(date).__pairs(date) do
    print(k, v)
end
local timestamp = date() -- call with no args to get timestamp
local date2 = tomlua.new_date(timestamp)
date(timestamp + 12345) -- set value from timestamp (__call takes same arg as new_date)
print(date > date2) -- true
print(date) -- print as toml date string
```

```c
enum TomlType {
    TOML_UNTYPED,  // Untyped TOML Item
    TOML_STRING,  // lua string
    TOML_STRING_MULTI,  // lua string
    TOML_INTEGER,  // lua number
    TOML_FLOAT,  // lua number
    TOML_BOOL,  // lua bool
    TOML_ARRAY,  // lua table
    TOML_TABLE,  // lua table
    TOML_LOCAL_DATE,  // string, or userdata with fancy_dates
    TOML_LOCAL_TIME,  // string, or userdata with fancy_dates
    TOML_LOCAL_DATETIME,  // string, or userdata with fancy_dates
    TOML_OFFSET_DATETIME,  // string, or userdata with fancy_dates
};
```

---

On startup you may have many toml files to parse in some situations, if you used it in a package spec format of some kind for example.

This is a tiny c library designed to chew through those as if you were using cjson for parsing json.

It is able to emit toml as well, but this is mostly for completeness.

Usually, toml is used as a config format, and not a serialization format.

This is because there is a lot of ambiguity in how you can define values when emitting it,
which makes it hard to emit sensibly and quickly.

As a result of that, editing existing toml files, or emitting them at all, only really happens outside of the hot path.

This means that it makes a lot of sense to use a simple but fast parser to parse config files on startup.

And then later you can use one which is better for editing but is slower when making things like a settings page which may edit the file.
