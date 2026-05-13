local define, test_dir = ...

local tomlua = require("tomlua")()

-- A mixed table that requires int_keys to encode
local mixed = { "a", "b", c = "d" }

define("changing current opts.int_keys = true", function()
    tomlua.opts.int_keys = false
    local _, err = tomlua.encode(mixed)
    ok(err ~= nil, "encode without int_keys should error on mixed table")

    tomlua.opts.int_keys = true
    ok(tomlua.opts.int_keys == true, "opts.int_keys should read back as true")
    local result, err2 = tomlua.encode(mixed)
    ok(err2 == nil, "encode with int_keys=true should succeed")
    ok(result ~= nil, "should produce output")
end)

define("opts.int_keys survives new copy", function()
    tomlua.opts.fancy_dates = true
    tomlua.opts.int_keys = true
    local tomlua2 = tomlua{ fancy_dates = false, int_keys = false }
    ok(tomlua.opts.int_keys == true, "int_keys should still be true after clone")
    ok(tomlua.opts.fancy_dates == true, "fancy_dates should still be true after clone")
    local result, err = tomlua2.encode(mixed)
    ok(err ~= nil, "encode should fail with int_keys=false")
    local result, err = tomlua.encode(mixed)
    ok(err == nil, "encode should succeed with int_keys=true")
    local opts = tomlua.opts()
    ok(type(opts) == "table", "opts when called with no args should return a table")
end)

define("opts({ int_keys = true }) doesn't copy", function()
    tomlua.opts.fancy_dates = true
    tomlua.opts.int_keys = true
    tomlua.opts{ fancy_dates = false, int_keys = false }
    ok(tomlua.opts.int_keys == false, "int_keys should still be false after call")
    ok(tomlua.opts.fancy_dates == false, "fancy_dates should be false after call")
    local result, err = tomlua.encode(mixed)
    ok(err ~= nil, "encode should fail with int_keys=false")
    local opts = tomlua.opts()
    ok(type(opts) == "table", "opts when called with no args should return a table")
end)
