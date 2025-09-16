---@class Tomlua.Deferred_Heading
---@field is_array boolean
---@field key string
---@field value any

---@class Tomlua.String_buffer
---@field push fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_inline_value fun(self: Tomlua.String_buffer, visited: table<table, boolean?>, value: any, array_level: number?):Tomlua.String_buffer
---@field push_heading fun(self: Tomlua.String_buffer, is_array: boolean, keys...: string):Tomlua.String_buffer
---@field push_heading_table fun(self: Tomlua.String_buffer, value: any, keys...: string?): Tomlua.Deferred_Heading[]
---@field push_keys fun(self: Tomlua.String_buffer, keys...: string):Tomlua.String_buffer

local new_buf = ...
local unpack = unpack or table.unpack
return function(input)
    ---@type Tomlua.String_buffer
    local dst = new_buf()

    ---@type fun(visited: table<table, boolean?>, q: Tomlua.Deferred_Heading[], keys: string[])
    local function flush_q(visited, q, keys)
        local klen = #keys
        for i = 1, #q do
            local h = rawget(q, i)
            rawset(keys, klen + 1, rawget(h, "key"))
            if h.is_array then
                dst:push("\n")
                for _, val in ipairs(rawget(h, "value")) do
                    dst:push_heading(true, unpack(keys))
                    for k, v in pairs(val) do
                        dst:push_keys(k):push(" = "):push_inline_value(visited, v):push("\n")
                    end
                end
            else
                local v = rawget(h, "value")
                if rawget(visited, v) then
                    error("Circular reference in table")
                end
                rawset(visited, v, true)
                flush_q(
                    visited,
                    dst:push("\n"):push_heading_table(v, unpack(keys)),
                    keys
                )
                rawset(visited, v, nil)
            end
            rawset(keys, klen + 1, nil)
        end
    end

    local ok, val = pcall(function()
        local visited = {}
        flush_q(visited, dst:push_heading_table(input), {})
        return tostring(dst)
    end)
    if ok then return val
    else return nil, val
    end
end
