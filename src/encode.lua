---@class Tomlua.Deferred_Heading
---@field is_array boolean
---@field keys string[]
---@field value any

---@class Tomlua.String_buffer
---@field push fun(self: Tomlua.String_buffer, str: string):Tomlua.String_buffer
---@field push_inline_value fun(self: Tomlua.String_buffer, value: any, array_level: number?):Tomlua.String_buffer
---@field push_heading fun(self: Tomlua.String_buffer, is_array: boolean, keys...: string):Tomlua.String_buffer
---@field push_keys fun(self: Tomlua.String_buffer, keys...: string):Tomlua.String_buffer

local check_heading_array, new_buf = ...
local unpack = unpack or table.unpack
return function(input)
    ---@type Tomlua.String_buffer
    local dst = new_buf()

    --instead of pushing tables, returns the tables I need to split out
    --It is to also return them if it is an array of ONLY TABLES, otherwise it prints the array inline
    ---@type fun(value: table, keys...: string):Tomlua.Deferred_Heading[]
    local function push_heading_table(value, ...)
        ---@type Tomlua.Deferred_Heading[]
        local result = {}
        dst:push_heading(false, ...)
        for k, v in pairs(value) do
            if type(v) == "table" then
                local table_type = check_heading_array(v)
                if table_type == 2 then
                    local keys = {...}
                    table.insert(keys, k)
                    table.insert(result, { is_array = true, keys = keys, value = v })
                elseif table_type == 1 then
                    dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
                else
                    local keys = {...}
                    table.insert(keys, k)
                    table.insert(result, { is_array = false, keys = keys, value = v })
                end
            else
                dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
            end
        end
        dst:push("\n")
        return result
    end
    ---@type fun(q: Tomlua.Deferred_Heading[])
    local function flush_q(q)
        for _, h in ipairs(q) do
            if h.is_array then
                for _, val in ipairs(h.value) do
                    dst:push_heading(true, unpack(h.keys))
                    for k, v in pairs(val) do
                        dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
                    end
                end
                dst:push("\n")
            else
                flush_q(push_heading_table(h.value, unpack(h.keys)))
            end
        end
    end

    ---@type Tomlua.Deferred_Heading[]
    local heading_q = {}
    local ok, val = pcall(function()
        for k, v in pairs(input) do
            local vtype = type(v)
            if vtype == "table" then
                local table_type = check_heading_array(v)
                if table_type == 2 then
                    table.insert(heading_q, { is_array = true, keys = { k }, value = v })
                elseif table_type == 1 then
                    dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
                else
                    table.insert(heading_q, { is_array = false, keys = { k }, value = v })
                end
            else
                dst:push_keys(k):push(" = "):push_inline_value(v):push("\n")
            end
        end
        flush_q(heading_q)
        return tostring(dst)
    end, input)
    if ok then
        return val
    else
        return nil, val
    end
end
