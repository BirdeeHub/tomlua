local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

---@class string_buffer
---@field push fun(self: string_buffer, str: string|string_buffer):string_buffer
---@field push_sep fun(self: string_buffer, sep: string, ...: string|string_buffer):string_buffer
---@field push_str fun(self: string_buffer, str: string):string_buffer
---@field push_multi_str fun(self: string_buffer, str: string):string_buffer
---@field push_num fun(self: string_buffer, n: number):string_buffer
---@field push_inline_value fun(self: string_buffer, value: table|string|number|boolean, array_level: number):string_buffer
---@field push_heading fun(self: string_buffer, is_array: boolean, ...: string):string_buffer
---@field push_keys fun(self: string_buffer, ...: string):string_buffer
---@field reset fun(self: string_buffer, ):string_buffer

-- TODO: remove inspect eventually, just for debugging
-- local inspect = require("inspect")

-- NOTE: if needed you can add stuff to the __index of the string buffer type
-- do
--     local buf_index = getmetatable(lib.new_buf()).__index
-- end

return function(input)
    ---@type string_buffer
    local dst = lib.new_buf()
    ---@type ({ keys: string[], value: string })[]
    local heading_q = {}
    for k, v in pairs(input) do
        -- TODO: deal with headings by putting them into the heading_q and then processing after the current level is processed
        dst:push_keys(k):push(" = "):push_inline_value(v, 0):push("\n")
    end
    print(dst)
    return "TODO"
end
