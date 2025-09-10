local upvals = {...}
local opts = upvals[1]
local lib = upvals[2]

-- TODO: remove inspect eventually, just for debugging
-- local inspect = require("inspect")

-- NOTE: if needed you can add stuff to the __index of the string buffer type
-- do
--     local buf_index = getmetatable(lib.new_buf()).__index
-- end

return function(input)
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
