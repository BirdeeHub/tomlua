local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

-- use this for working with the headings and key = value
-- expose a c push_heading, push_value, and push_key on your lua string buffer type
-- tostring and return the buffer 1 time at the end
return function(input)
    local inspect = require("inspect")
    local res = {
        input = inspect(input),
        opts = inspect(opts),
        lib = inspect(lib),
        isarr1 = lib.is_array({}),
        isarr2 = lib.is_array({ 1, 2, 3, 4 }),
        isarr3 = lib.is_array({ a = 1, b = 2, c = 3, d = 4 }),
    }
    for k, v in pairs(res) do
        print(k, ":", v)
    end
end
