local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

-- use this for working with the headings and key = value
-- expose a c push_heading, push_value, and push_key on your lua string buffer type
-- tostring and return the buffer 1 time at the end
return function(input)
    local inspect = require("inspect")
    local buf = lib.new_buf()
        :push_keys("t1", "t2", "t3")
        :push(" = ")
        :push_str("test")
        :push("\n")
        :push_keys("t4-_", [['\"
tg\U000111115]], "t6")
        :push(" = ")
        :push_multi_str([[
            test "
            bleh'"""sss
        ]])
        :push("\n# testing push_num:\n")
        :push_num(123.456)
        :push("\n")
        :push_num(123)
        :push("\n")
        :push_num(1222222222222244444444444444444444444443)
        :push("\n")
        :push_num(math.huge)
        :push("\n")
        :push_num(-math.huge)
        :push("\n")
        :push_num(0/0)
        :push("\n# testing push_sep:\n")
        :push_sep(" ", "testa","testb","testc")
        :push("\n")
    local res = {
        input = inspect(input),
        opts = inspect(opts),
        lib = inspect(lib),
        buf = inspect(buf),
        strbuf = buf,
        isarr1 = lib.is_array({}),
        isarr2 = lib.is_array({ 1, 2, 3, 4 }),
        isarr3 = lib.is_array({ a = 1, b = 2, c = 3, d = 4 }),
        isarr4 = lib.is_array(setmetatable({ a = 1, b = 2, c = 3, d = 4 }, { toml_type = lib.types.ARRAY })),
    }
    for k, v in pairs(res) do
        print(k, ":", v)
    end
end
