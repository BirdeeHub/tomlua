package.cpath = "./lib/?.so;" .. package.cpath
local tomlua = require("tomlua")({ enhanced_tables = false, strict = true })

local inspect = require('inspect')
local cjson = require("cjson.safe")

local f = io.open("./tests/example.toml", "r")
local contents
if f then
    contents = f:read("*a")
    f:close()
end
-- print(contents)

-- Number of iterations for the benchmark
local run_toml_edit = tonumber(arg[2]) ~= 0
local iterations = tonumber(arg[3]) or tonumber(arg[2]) or 100

local function stats(target, elapsed, n)
    n = n or iterations
    return ("Parsed %s %d times in %.6f seconds, avg. %.6f iterations per second, avg. %.2f µ/iteration")
        :format(target, n, elapsed, n / elapsed, elapsed * 1e6 / n)
end
local function rate_compare(prefix, a, b, n)
    n = n or iterations
    return ("speed %s: %.2f%%, duration %s: %.2f%%"):format(prefix, ((n / a) / (n / b)) * 100, prefix, (a / b) * 100)
end

local last_result
local last_error

-- Benchmark
local start_time = os.clock()

for _ = 1, iterations do
    local data, err = tomlua.decode(contents)
    last_result = data
    if err then
        print("ERROR:", err)
        break
    end
end

local elapsed = os.clock() - start_time

print("Last result:", inspect(last_result))
print("Last error:", last_error)
print(stats("TOML", elapsed))

local jsonstr = cjson.encode(last_result)

-- Benchmark against cjson
start_time = os.clock()

for _ = 1, iterations do
    local data, err = cjson.decode(jsonstr)
    last_result = data
    if err then
        print("ERROR:", err)
        break
    end
end

local elapsed2 = os.clock() - start_time
print(stats("JSON", elapsed2))
print(rate_compare("tomlua/cjson", elapsed, elapsed2))

-- Benchmark existing toml lua implementation
if run_toml_edit then
    local toml_edit = require("toml_edit")
    start_time = os.clock()

    for _ = 1, iterations do
        local ok, data = pcall(toml_edit.parse_as_tbl, contents)
        last_result = data
        if not ok then
            print("ERROR:", data)
            break
        end
    end

    local elapsed3 = os.clock() - start_time
    print(stats("TOML", elapsed3))
    print(rate_compare("tomlua/toml_edit", elapsed, elapsed3))
end

print()
print("TODO remove junk test output")

print()
print("will this error (sorta)")
local data, err = tomlua.decode({ bleh = "haha", })
print(inspect(data), "  :  ", inspect(err))
print("will this error (no)")
data, err = tomlua.decode("hehe = 'haha'", "Im the wrong type and will be ignored!")
print(inspect(data), "  :  ", inspect(err))
print("will this error (sorta)")
data, err = tomlua.decode()
print(inspect(data), "  :  ", inspect(err))
print("will this error (no)")
data, err = tomlua.decode("hehe = 'haha'", { bleh = "haha", }, "does it ignore extra args?")
print(inspect(data), "  :  ", inspect(err))

print()
print("OVERLAY DEFAULTS TEST")

local testdata = {
    a = {
        b = { "1b", "2b" },
    },
    c = "hello",
    e = {
        f = { "1f", "2f" },
    },
}

local testtoml = [=[
a.b = [ "3b", "4b" ]
d = "hahaha"
[[e.f]]
testtable.value = true
[a]
newval = "nope"
]=]

data, err = tomlua.decode(testtoml, testdata)
print(inspect(data))
print(inspect(err))

print()
print("TODO: ENCODE")
print(tomlua.encode(testdata))
