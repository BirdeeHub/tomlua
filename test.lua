local f = io.open("./example.toml", "r")
local contents
if f then
    contents = f:read("*a")
    f:close()
end
local inspect = require('inspect')
local tomlua = require("tomlua")
print(contents)
-- Number of iterations for the benchmark
local iterations = 1

-- Benchmark
local start_time = os.clock()

local last_result
local last_error

for _ = 1, iterations do
    local data, err = tomlua.decode(contents)
    last_result = data
    last_error = err
end

local elapsed = os.clock() - start_time

print(string.format("Parsed TOML %d times in %.6f seconds, avg. %.6f iterations per second", iterations, elapsed, iterations / elapsed))
print("Last result:", inspect(last_result))
print("Last error:", last_error)
