# TOMLUA

> [!WARNING]
> This is not finished software yet!

Parses .toml files into lua tables quickly, and hopefully eventually back again.

Implemented in C

This is not intended to replace packages like [toml_edit](https://github.com/nvim-neorocks/toml-edit.lua)

It will not be handling comments, and it will not re-output in the same format as the ingested file.

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

local data, err = require("tomlua").decode(some_string)

-- or read into an existing table
data, err = require("tomlua").decode(some_string, { some = "defaults" })-- , true) for strict mode eventually, or maybe a separate function

-- TODO
local str, err = require("tomlua").encode(some_table) -- with options to control some emit options via metatables on values
```

```c
enum ValueType {
    VALUE_STRING, // lua string
    VALUE_INTEGER, // lua number
    VALUE_FLOAT, // lua number
    VALUE_BOOL, // lua bool
    VALUE_ARRAY, // lua table
    VALUE_TABLE, // lua table
    LOCAL_DATE, // string for now
    LOCAL_TIME, // string for now
    LOCAL_DATETIME, // string for now
    OFFSET_DATETIME, // string for now
};
```

---

I might have written it in rust or zig but I wanted to practice my C

Error reporting is bad currently, but the infrastructure for handling them well is in place already for when I want to make them dynamic

It only has a decode function and that is it so far.

It will likely get an encode function at some point.

If it does I will finish the README.

Basic benchmarking shows decode compares at about 1.5x cjson in runtime duration for 100000+ iterations of processing the example.toml file.

However the cjson in the benchmark does not need to deal with comments or empty lines as it is parsing the result of cjson.encode in the benchmark, which is a single line.
So I feel this is pretty good for something that doesn't use SIMD(yet?) or parallelism or other such fancy tricks.

Unfortunately, I am also slightly cheating, I have yet to add strict mode for decode which checks for table uniqueness following the toml spec precisely. This will be something you can toggle on or off.
