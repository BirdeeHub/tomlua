# TOMLUA

Parses .toml files into lua tables, and hopefully eventually back again.

Implemented in C

build with make and add the .so to your LUA_CPATH (or package.cpath at runtime) or install via luarocks

```lua
require("tomlua").decode(some_string)

-- or read into an existing table
require("tomlua").decode(some_string, { some = "defaults" })

-- TODO
require("tomlua").encode(some_string)
```

---

Just me practicing some C

Error reporting is bad currently.

It only has a decode function and that is it.

It will likely get an encode function at some point.

If it does it will also get a real README.

Basic benchmarking shows decode compares at about 1.5x cjson in runtime duration for 100000 iterations of processing the example.toml file.

However the cjson in the benchmark does not need to deal with comments or empty lines as it is parsing the result of cjson.encode in the benchmark, which is a single line.
So I feel this is pretty good for something that doesn't use simd or parallelism or other such fancy tricks.

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
