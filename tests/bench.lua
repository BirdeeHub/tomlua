return function(test_dir, iterations, run_toml_edit)
    local tomlua = require("tomlua") { fancy_tables = false, fancy_dates = false }

    local inspect = require('inspect')
    local cjson = require("cjson.safe")

    local f = io.open(test_dir .. "/example.toml", "r")
    local contents
    if f then
        contents = f:read("*a")
        f:close()
    end
    -- print(contents)

    local function stats(target, elapsed, n, is_encode)
        n = n or iterations
        return ("%s %s %d times in %.6f seconds, avg. %.6f iterations per second, avg. %.2f µs/iteration")
            :format(is_encode and "Emitted" or "Parsed", target, n, elapsed, n / elapsed, elapsed * 1e6 / n)
    end
    local function rate_compare(prefix, a, b, n)
        n = n or iterations
        return ("speed %s: %.2f%%, duration %s: %.2f%%"):format(prefix, ((n / a) / (n / b)) * 100, prefix, (a / b) * 100)
    end

    local last_result
    local last_error

    -----------------------------------------
    ----------- Benchmarks ------------------
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

    start_time = os.clock()

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
    tomlua = require("tomlua") { fancy_tables = false, fancy_dates = false }
    local to_encode = tomlua.decode(contents)

    -- ENCODE

    -- Benchmark
    local start_time = os.clock()

    for _ = 1, iterations do
        local str, e = tomlua.encode(to_encode)
        last_result = str
        if e then
            print("ERROR:", e)
            break
        end
    end

    local elapsed = os.clock() - start_time

    print("Last result:", last_result)
    print("ENCODE BENCH")
    print(stats("TOML", elapsed, nil, true))


    -- Benchmark against cjson
    start_time = os.clock()

    for _ = 1, iterations do
        local str, e = cjson.encode(to_encode)
        last_result = str
        if e then
            print("ERROR:", e)
            break
        end
    end

    local elapsed2 = os.clock() - start_time
    print(stats("JSON", elapsed2, nil, true))
    print(rate_compare("tomlua/cjson", elapsed, elapsed2))
end
