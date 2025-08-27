Just me practicing some C

Error reporting is bad.

It only has a decode function and that is it.

Maybe it will get an encode function at some point.

Maybe it will also get a real README, and some proper build instructions (although, it is in a nix flake so you can flake.nix for now idk)

Basic benchmarking shows decode compares at between 1.25-2.00 cjson in runtime duration for 100000 iterations of processing the example.toml file.

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
