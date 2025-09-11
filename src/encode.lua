---@class Tomlua.Deferred_Heading
---@field is_array boolean
---@field keys string[]
---@field value any

---@class Tomlua.String_buffer
---@field push fun(self: Tomlua.String_buffer, str: string|Tomlua.String_buffer):Tomlua.String_buffer
---@field push_inline_value fun(self: Tomlua.String_buffer, visited: table<table, any>, value: any, array_level: number?):Tomlua.String_buffer
---@field push_heading fun(self: Tomlua.String_buffer, is_array: boolean, ...: string):Tomlua.String_buffer
---@field push_keys fun(self: Tomlua.String_buffer, ...: string):Tomlua.String_buffer

---@class Tomlua.Lib
---@field new_buf fun():Tomlua.String_buffer
---@field types table
---also checks if all items are tables, returns is_heading_array, is_array
---@field is_heading_array fun(value: any?):boolean, boolean

local upvals = {...}
local opts = upvals[1]
---@type Tomlua.Lib
local lib = upvals[2]

--instead of pushing tables, returns the tables I need to split out
--It is to also return them if it is an array of ONLY TABLES, otherwise it prints the array inline
---@type fun(self: Tomlua.String_buffer, visited: table<table, any>, value: table, ...: string):Tomlua.Deferred_Heading[]
local function push_heading_table(self, visited, value, ...)
    ---@type Tomlua.Deferred_Heading[]
    local result = {}
    self:push_heading(false, ...)
    for k, v in pairs(value) do
        local vtype = type(v)
        if vtype == "table" then
            local is_heading_array, is_array = lib.is_heading_array(v)
            if is_heading_array then
                local keys = {...}
                table.insert(keys, k)
                table.insert(result, { is_array = true, keys = keys, value = v })
            elseif is_array then
                self:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
            else
                local keys = {...}
                table.insert(keys, k)
                table.insert(result, { is_array = false, keys = keys, value = v })
            end
        else
            self:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
        end
    end
    return result
end

--no need to return them for heading arrays tho, all the things inside these need to be inline
--only call it on things checked with is_heading_array
---@type fun(self: Tomlua.String_buffer, visited: table<table, any>, value: table, ...: string)
local function push_heading_array(self, visited, value, ...)
    for _, val in ipairs(value) do
        self:push_heading(true, ...)
        for k, v in pairs(val) do
            self:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
        end
    end
end

return function(input)
    ---@type Tomlua.String_buffer
    local dst = lib.new_buf()
    ---@type Tomlua.Deferred_Heading[]
    local heading_q = {}
    local function flush_q(q, visited)
        local i = 1
        while i <= #q do
            local h = q[i]
            if h.is_array then
                push_heading_array(dst, visited, h.value, unpack(h.keys))
            else
                flush_q(push_heading_table(dst, visited, h.value, unpack(h.keys)))
            end
            i = i + 1
        end
    end
    local ok, val = pcall(function()
        ---@type table<table, boolean?>
        local visited = { [input] = true, }
        for k, v in pairs(input) do
            local vtype = type(v)
            if vtype == "table" then
                local is_heading_array, is_array = lib.is_heading_array(v)
                if is_heading_array then
                    table.insert(heading_q, { is_array = true, keys = { k }, value = v })
                elseif is_array then
                    dst:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
                else
                    table.insert(heading_q, { is_array = false, keys = { k }, value = v })
                end
            else
                dst:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
            end
        end
        flush_q(heading_q, visited)
        return tostring(dst)
    end, input)
    if ok then
        return val
    else
        return nil, val
    end
end
