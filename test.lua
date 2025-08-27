-- local f = io.open("./example.toml", "r")
-- local contents
-- if f then
--     contents = f:read("*a")
--     f:close()
-- end
local inspect = require('inspect')
local tomlua = require("tomlua")
local toml_data = [=[
    testkey = "value"
    testkey2 = true
    testkey3 = false
    testkey4 = 1
    testkey5 = 1.5
    testkey6 = [
        1, 2, 3, 4, 5,
    ]
    testkey7 = { a.d = 1, b = [ 2, 3 ], c = 3 }

    [[test]]
    names.hello = "h\u1234i"
    key = "value"
    [[test]]
    key = "value"
    "names"."boo" = "hi"
    names."boo2" = "hi2"
    "tk2-dsadas.com" = "value"
    [test2]
    key = "value"
    [test2]
    "tk1-assass.com" = "value"
    [[test2.key2]]
    das1 = "dasda"
    das2 = 'dasda'
    'das3' = '''da
    sda'''
    das4 = """da
    sda"""

    # todo: ??
    # release_date = 2025-08-24T12:00:00Z
    # last_backup = 2025-08-23T23:45:12-07:00
    # trailing comma in table and multiline not allowed
    # testkey10 = { a.d = 1, b = [ 2, 3 ], c = 3, }
    # testkey8 = {
    #    a.d = 1,
    #    b = [ 2, 3 ],
    #    c = 3
    # }
    # more invalid syntax
    # dasdsadsa = aaa2121dasssss22
    # dasdsadsa = 22aaa2121dasssss22
]=]
-- print(contents)
-- print(inspect(tomlua))
-- Number of iterations for the benchmark
local iterations = 100000

-- Benchmark
local start_time = os.clock()

local last_result
local last_error

for i = 1, iterations do
    local data, err = tomlua.decode(toml_data)
    last_result = data
    last_error = err
end

local elapsed = os.clock() - start_time

print(string.format("Parsed TOML %d times in %.6f seconds, avg. %.6f iterations per second", iterations, elapsed, iterations / elapsed))
print("Last result:", inspect(last_result))
print("Last error:", last_error)
