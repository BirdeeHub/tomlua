local inspect = require('inspect')
local tomlua = require("tomlua")
local cjson = require('cjson.safe')

local f = io.open("./example.toml", "r")
local contents
if f then
    contents = f:read("*a")
    f:close()
end
-- print(contents)

-- Number of iterations for the benchmark
local iterations = 100000

local last_result
local last_error

-- Benchmark
local start_time = os.clock()

for _ = 1, iterations do
    local data, err = tomlua.decode(contents)
    last_result = data
    last_error = err
end

local elapsed = os.clock() - start_time

print("Last result:", inspect(last_result))
print("Last error:", last_error)
print(string.format("Parsed TOML %d times in %.6f seconds, avg. %.6f iterations per second", iterations, elapsed, iterations / elapsed))

local jsonstr = cjson.encode(last_result)

-- Benchmark
start_time = os.clock()

for _ = 1, iterations do
    local data, err = cjson.decode(jsonstr)
    last_result = data
    last_error = err
end

local elapsed2 = os.clock() - start_time
print(string.format("Parsed JSON %d times in %.6f seconds, avg. %.6f iterations per second", iterations, elapsed2, iterations / elapsed2))
print(string.format("tomlua/cjson: %.2f%%", (elapsed / elapsed2) * 100))

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

local data, err = tomlua.decode(testtoml, testdata)
print(inspect(data))
print(inspect(err))
