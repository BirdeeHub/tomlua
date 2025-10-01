# TOMLUA

> ⚠️ **Warning:** This is a work-in-progress! It still needs the final polish!
> When this package is no longer in its early development cycle, its version will be set to v1.0.0 for the initial release.

**TOMLUA** is a fast TOML parser and emitter for Lua, implemented in C. It is designed for **hot-path parsing** of TOML files into Lua tables, not for editing existing files.

It is not intended to replace packages like [toml\_edit](https://github.com/nvim-neorocks/toml-edit.lua), which are slower but more suitable for editing TOML while preserving comments and formatting.

---

## Features

* Fast parsing of TOML files into Lua tables.
* Emits TOML from Lua tables (also quickly).
* Compatible with Lua 5.1+.
* Some advanced TOML compliance features are optional (`fancy_dates`, etc.).
* Allows you to read directly into an existing lua table of defaults. This cuts out the step of merging them yourself, making it even more performant and easier to use!

## Limitations

* Does **not preserve comments**.
* Output may differ from the original TOML formatting.
* Intended primarily for **reading TOML on startup**, or writing whole files, not for editing.

## Performance

Basic benchmarking shows promising results:

* Slightly slower than `cjson` (2x longer), despite parsing TOML instead of JSON.
* Around 7x faster than `toml_edit` for parsing (not accounting for the added ability to read directly into an existing lua table).

## Installation

### Using Make

```bash
# Ensure LUA_INCDIR points to Lua headers directory
make LUA_INCDIR=/path/to/lua/includes

# Add the resulting module to Lua's package.cpath
export LUA_CPATH="$LUA_CPATH;/path/to/tomlua/lib/?.so"
```

* Requires a C compiler (GCC, Clang, etc.). If not gcc, set CC variable as well
* You should be in the root of the repository.

### Using LuaRocks

```bash
luarocks install tomlua
```

### Using Nix

* Flake or flakeless support, both methods return according to the [flake outputs schema](https://wiki.nixos.org/wiki/Flakes).
* Overlay and packages available for Lua versions 5.1+, as well as a neovim plugin.
* Dev shell included for building via `make`.
* Not yet on nixpkgs

### Useage

```lua
package.cpath = package.cpath .. ";/path/to/tomlua/lib/?.so"

-- mark it like this for lsp type info! (sorry, C compiled module)
---@module 'tomlua.meta'
local tomlua = require("tomlua")

-- you can call it like a function and get a copy of the library with a new options table
local tomlua2 = tomlua {
    -- fancy_tables allows multiline inline tables with a trailing comma
    -- key = value still must be on the same line
    -- the tables, much like arrays, must still start on the same line as their key as well
    fancy_tables = false,
    -- causes keys that parse as lua integers to be interpreted as integer keys
    -- unless they were enclosed in "" or ''
    int_keys = false,
    -- causes dates to be parsed into a userdata type
    -- encode will write them correctly as well
    fancy_dates = false,
    -- causes multiline strings to be parsed into a userdata type
    -- which can be converted to a lua string with tostring
    multi_strings = false,
    -- adds metafield toml_type to inline table and array decode results
    -- such that it keeps track of what was inline or a heading in the file for encode
    mark_inline = false,
    -- if tomlua.decode reads a number bigger than can fit in a lua number or integer
    -- with false it will set + or - math.huge for the value and return normally
    -- with true it will return nil + an error
    overflow_errors = false,
    -- like overflow_errors setting, but for float precision underflow
    underflow_errors = false,
}

-- or you can set them directly on the current object
tomlua.opts.fancy_dates = true

local data, err = tomlua.decode(some_string)

local data, err = tomlua.decode(some_string, {
    some = "defaults",
    can_go = "here",
    tables_and_arrays = "defined in headings will extend the associated value",
    and_inline = "values from the toml will override it instead",
})

-- encode always accepts fancy dates, never outputs fancy tables, and is unaffected by all opts
-- instead you may customize dates by replacing them with tomlua date types
-- and you may decide to make some strings multiline with tomlua.str_2_mul
-- and you may make arrays appear as tables, or empty tables appear as arrays,
-- local empty_toml_array = setmetatable({}, {
--   toml_type = "ARRAY"
-- })
-- local a_toml_table = setmetatable({ "a", "table", "with", "integer", "keys" }, {
--   toml_type = tomlua.types.TABLE -- it also accepts the numbers
-- })
-- you may do the same with the ARRAY_INLINE and TABLE_INLINE types to force them to display inline
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
    ARRAY_INLINE, -- 8  -- same as ARRAY, but forces encode to print it as a inline
    TABLE_INLINE, -- 9  -- same as TABLE, but forces encode to print it as a inline
    LOCAL_DATE, -- 10  -- string, or userdata with fancy_dates
    LOCAL_TIME, -- 11  -- string, or userdata with fancy_dates
    LOCAL_DATETIME, -- 12  -- string, or userdata with fancy_dates
    OFFSET_DATETIME, -- 13  -- string, or userdata with fancy_dates
}
-- get the type tomlua thinks a lua value is
tomlua.type(value) --> returns one of the names from tomlua.types
-- get the type tomlua thinks a lua value is
tomlua.type_of(value) --> returns corresponding number from tomlua.types instead
tomlua.typename(typenumber) --> tomlua.types[result] = typenumber

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
-- for k, v in pairs(date) do -- in lua5.2+ this also works
    print(k, v)
end
local timestamp = date() -- call with no args to get timestamp
local date2 = tomlua.new_date(timestamp)
date(timestamp + 12345) -- set value from timestamp (__call takes same arg as new_date)
print(date > date2) -- true
print(date) -- print as toml date string
```

## Philosophy

* TOML is usually a **config format**, not a serialization format.
* Parsing speed is important for **startup-heavy workflows**.
* Editing or re-emitting TOML is **secondary**; using dedicated editor libraries for that may make sense still.

On startup you may have many toml files to parse in some situations, if you used it in a package spec format of some kind for example.

This is a tiny c library designed to chew through those as if you were using cjson for parsing json.

It is able to emit toml as well, and still does so quickly, but this was mostly for completeness.

Usually, toml is used as a config format, and not a serialization format.

This is because there is a lot of ambiguity in how you can define values when emitting it,
which makes it hard to emit sensibly and quickly.

As a result of that, editing existing toml files, or emitting them at all, only really happens outside of the hot path.

This means that it makes a lot of sense to use a simple but fast parser to parse config files on startup.

And then later you can use one which is better for editing but is slower when making things like a settings page which may edit the file.
