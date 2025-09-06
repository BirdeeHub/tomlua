local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]

local function to_escaped_toml_str(str)
    return ("\"%s\""):format(tostring(str):gsub("[\\\n\r\"\b\f\t]", function(c)
        if c == "\n" then
            c = "n"
        elseif c == "\b" then
            c = "b"
        elseif c == "\f" then
            c = "f"
        elseif c == "\r" then
            c = "r"
        elseif c == "\t" then
            c = "t"
        end
        return "\\"..c
    end))
end

return function(v)
    local inspect = require("inspect")
    print(inspect(v), inspect(opts), to_escaped_toml_str [[dsahdash"
    dsadsa
    dsadsa'
    """
    '''
    adsdasdas]])
end
