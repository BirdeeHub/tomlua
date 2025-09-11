# TOMLUA

> [!WARNING]
> This is not finished software yet!

Parses toml files into lua tables quickly, and hopefully eventually back again.

Implemented in C

This is not intended to replace packages like [toml_edit](https://github.com/nvim-neorocks/toml-edit.lua)

It will not be handling and re-emitting comments, and it will not re-output in the same format as the ingested file.

If you wish to edit existing toml, you should do that using a package more suited for that.

This is instead intended for hot-path parsing of toml files.

On startup you may have many toml files to parse in some situations, if you used it in a package spec format of some kind for example.

This is a tiny c library designed to chew through those as if you were using cjson for parsing json.

It will be able to emit toml eventually as well, but this is mostly for completeness. It will not be taking into account the format or comments of the original ingested file.

Usually, toml is used as a config format, and not a serialization format.

This is because there is a lot of ambiguity in how you can define values when emitting it,
which makes it hard to emit sensibly and quickly.

As a result of that, editing existing toml files, or emitting them at all, only really happens outside of the hot path.

This means that it makes a lot of sense to use a simple but fast parser to parse config files on startup.

And then later you can use one which is better for editing but is slower when making things like a settings page which may edit the file.

---

build with make and add the tomlua.so to your LUA_CPATH (or package.cpath at runtime) or install via luarocks

Yes there will eventually be better build instructions.

```lua
package.cpath = package.cpath .. ";/path/to/tomlua/lib/?.so"

local tomlua = require("tomlua")({
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
    -- you may index, iterate with pairs or ipairs, or use tostring on them
    -- encode will write them correctly as well
    fancy_dates = false,
})

local data, err = tomlua.decode(some_string)

-- or read into an existing table (always accepts fancy dates, never outputs fancy tables, currently unaffected by all other opts)
data, err = tomlua.decode(some_string, { some = "defaults" })

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

-- TODO: get the type for tomlua of a lua value
tomlua.type(value) --> value from tomlua.types
-- TODO:
tomlua.typename(typenumber) --> string
-- TODO:
tomlua.new_date(--[[???]])
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

I might have written it in rust or zig but I wanted to practice my C

Error reporting is bad currently, but the infrastructure for handling them well is in place already for when I want to make them dynamic

It only has a decode function and that is it so far.

It will likely get an encode function at some point.

If it does I will finish the README.

Basic benchmarking shows decode compares at about 1.5x cjson in runtime duration for 100000+ iterations of processing the example.toml file.

1_000_000 iterations

```
Parsed TOML 1000000 times in 16.736250 seconds, avg. 59750.541489 iterations per second, avg. 16.74 µs/iteration
Parsed TOML 1000000 times in 28.898695 seconds, avg. 34603.638676 iterations per second, avg. 28.90 µs/iteration
speed tomlua/tomlua_strict: 172.67%, duration tomlua/tomlua_strict: 57.91%
Parsed JSON 1000000 times in 13.078626 seconds, avg. 76460.631262 iterations per second, avg. 13.08 µs/iteration
speed tomlua/cjson: 78.15%, duration tomlua/cjson: 127.97%
```

100_000 iterations with toml_edit as well

```
Parsed TOML 100000 times in 1.442044 seconds, avg. 69346.011633 iterations per second, avg. 14.42 µs/iteration
Parsed TOML 100000 times in 2.487302 seconds, avg. 40204.205199 iterations per second, avg. 24.87 µs/iteration
speed tomlua/tomlua_strict: 172.48%, duration tomlua/tomlua_strict: 57.98%
Parsed JSON 100000 times in 1.454340 seconds, avg. 68759.712309 iterations per second, avg. 14.54 µs/iteration
speed tomlua/cjson: 100.85%, duration tomlua/cjson: 99.15%
Parsed TOML 100000 times in 14.866264 seconds, avg. 6726.639591 iterations per second, avg. 148.66 µs/iteration
speed tomlua/toml_edit: 1030.92%, duration tomlua/toml_edit: 9.70%
```

However the cjson in the benchmark does not need to deal with comments or empty lines, or the headings of toml.
cjson is parsing the result of cjson.encode on the output of tomlua.decode.

So I feel this is pretty good for something that doesn't use SIMD(yet?) or parallelism or other such fancy tricks.
