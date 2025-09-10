local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

-- TODO: remove inspect eventually, just for debugging
local inspect = require("inspect")
do
    local buf_index = getmetatable(lib.new_buf()).__index
    -- -- assumes inline, and non-nil, deal with headings before this
    buf_index.push_inline_value = function(buf, value, array_level, vtype)
        vtype = vtype or type(value)
        if vtype == "number" then
            buf:push_num(value)
        elseif vtype == "string" then
            buf:push_str(value)
        elseif vtype == "boolean" then
            buf:push(value and "true" or "false")
        elseif vtype == "table" then
            if lib.is_array(value) then
                if not value[1] then buf:push("[]") return end
                local indent = array_level and (" "):rep((array_level + 1) * 2) or nil
                if indent then
                    buf:push("[\n"):push(indent)
                else
                    buf:push("[ ")
                end
                local i = 1
                while value[i] ~= nil do
                    buf:push_inline_value(value[i], array_level and array_level + 1 or nil)
                    i = i + 1
                    if value[i] == nil then
                        break
                    elseif indent then
                        buf:push(",\n"):push(indent)
                    else
                        buf:push(", ")
                    end
                end
                if array_level then
                    buf:push("\n"):push((" "):rep(array_level * 2)):push("]")
                else
                    buf:push(" ]")
                end
            else
                buf:push("{ ")
                for k, v in pairs(value) do
                    buf:push_keys(k):push(" = "):push_inline_value(v)
                    if next(value, k) ~= nil then
                        buf:push(", ")
                    end
                end
                buf:push(" }")
            end
        else
            error(vtype .. " is not a valid type for push_value")
        end
    end
end
return function(input)
    local dst = lib.new_buf()
    local heading_q = {}
    -- TODO: start outputting some toml, dealing with headings by putting them into the heading_q and then processing after the current level is processed
    dst:push_heading(true, "atableheading")
        :push_keys("hello", "test")
        :push(" = ")
        :push_inline_value({ "hello", "hi", { "hiagain", "boo" }, { haha = { "lol", "roflmao" } } }, 0)
    print(dst)
    return "TODO"
end
