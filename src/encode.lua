local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

---@class Tomlua.String_buffer
---@field push fun(self: Tomlua.String_buffer, str: string|Tomlua.String_buffer):Tomlua.String_buffer
---@field push_sep fun(self: Tomlua.String_buffer, sep: string, ...: string|Tomlua.String_buffer):Tomlua.String_buffer
---@field push_str fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_multi_str fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_num fun(self: Tomlua.String_buffer, n: number):Tomlua.String_buffer
---@field push_inline_value fun(self: Tomlua.String_buffer, value: any, array_level: number):Tomlua.String_buffer
---@field push_heading fun(self: Tomlua.String_buffer, is_array: boolean, ...: string):Tomlua.String_buffer
---@field push_keys fun(self: Tomlua.String_buffer, ...: string):Tomlua.String_buffer
---@field reset fun(self: Tomlua.String_buffer):Tomlua.String_buffer

--TODO: something like this which instead of pushing tables, returns the tables I need to split out
--It is to also return them if it is an array of ONLY TABLES, otherwise it prints the array inline
--@field push_heading_table fun(self: Tomlua.String_buffer, value: table):({ is_array: boolean, key: string, value: any })[]

--TODO: no need to return them for heading arrays tho, all the things inside these need to be inline
--@field push_heading_array fun(self: Tomlua.String_buffer, value: table):Tomlua.String_buffer

---@class Tomlua.Lib
---@field new_buf fun():Tomlua.String_buffer
---@field types table
---@field is_array fun(value: any?):boolean
---also checks if all items are tables
---@field is_heading_array fun(value: any?):boolean

-- TODO: remove inspect eventually, just for debugging
-- local inspect = require("inspect")

-- NOTE: if needed you can add stuff to the __index of the string buffer type in lua
-- do
--     local buf_index = getmetatable(lib.new_buf()).__index
-- end

return function(input)
    ---@type Tomlua.String_buffer
    local dst = lib.new_buf()
    ---@type ({ is_array: boolean, keys: string[], value: any })[]
    local heading_q = {}
    for k, v in pairs(input) do
        -- TODO: deal with headings by putting them into the heading_q and then processing after the current level is processed
        dst:push_keys(k):push(" = "):push_inline_value(v, 0):push("\n")
    end
    return dst
end
