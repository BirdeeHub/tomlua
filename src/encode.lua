---@class Tomlua.String_buffer
---@field push fun(self: Tomlua.String_buffer, str: string|Tomlua.String_buffer):Tomlua.String_buffer
---@field push_sep fun(self: Tomlua.String_buffer, sep: string, ...: string|Tomlua.String_buffer):Tomlua.String_buffer
---@field push_str fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_multi_str fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_num fun(self: Tomlua.String_buffer, n: number):Tomlua.String_buffer
---@field push_inline_value fun(self: Tomlua.String_buffer, value: any, array_level: number?):Tomlua.String_buffer
---@field push_heading fun(self: Tomlua.String_buffer, is_array: boolean, ...: string):Tomlua.String_buffer
---@field push_keys fun(self: Tomlua.String_buffer, ...: string):Tomlua.String_buffer
---@field reset fun(self: Tomlua.String_buffer):Tomlua.String_buffer
---@field push_heading_array fun(self: Tomlua.String_buffer, value: table, ...: string):Tomlua.String_buffer

--TODO: something like these, outlined further below
--@field push_heading_table fun(self: Tomlua.String_buffer, value: table, ...: string):{ is_array: boolean, keys: string[], value: any }[]

---@class Tomlua.Lib
---@field new_buf fun():Tomlua.String_buffer
---@field types table
---@field is_array fun(value: any?):boolean
---also checks if all items are tables
---@field is_heading_array fun(value: any?):boolean

local upvals = {...}
local opts = upvals[1]
---@type Tomlua.Lib
local lib = upvals[2]

-- TODO: remove inspect eventually, just for debugging
-- local inspect = require("inspect")

---@class Tomlua.Deferred_Heading
---@field is_array boolean
---@field keys string[]
---@field value any

do
    local buf_index = getmetatable(lib.new_buf()).__index
--TODO: something like this which instead of pushing tables, returns the tables I need to split out
--It is to also return them if it is an array of ONLY TABLES, otherwise it prints the array inline
    ---@type fun(self: Tomlua.String_buffer, queue: Tomlua.Deferred_Heading[], value: table, ...: string):Tomlua.Deferred_Heading[]
    buf_index.push_heading_table = function(self, queue, value, ...)
    end

    --no need to return them for heading arrays tho, all the things inside these need to be inline
    --only call it on things checked with is_heading_array
    ---@type fun(self: Tomlua.String_buffer, value: table, ...: string):Tomlua.String_buffer
    buf_index.push_heading_array = function(self, value, ...)
        for _, val in ipairs(value) do
            self:push_heading(true, ...)
            for k, v in pairs(val) do
                self:push_keys(k):push(" = "):push_inline_value(v):push("\n")
            end
        end
        return self
    end
end

return function(input)
    ---@type Tomlua.String_buffer
    local dst = lib.new_buf()
    ---@type Tomlua.Deferred_Heading[]
    local heading_q = {}
    for k, v in pairs(input) do
        -- TODO: deal with headings by putting them into the heading_q and then processing after the current level is processed
        dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
    end
    return dst
end
