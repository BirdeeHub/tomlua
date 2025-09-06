local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]

local function to_escaped_toml_str(str)
    return ("\"%s\""):format(tostring(str):gsub("[\\\n\r\"\b\f\t]", function(c)
        if c == "\\" then
            return "\\\\"
        elseif c == "\n" then
            return "\\n"
        elseif c == "\r" then
            return "\\r"
        elseif c == "\"" then
            return "\\\""
        elseif c == "\b" then
            return "\\b"
        elseif c == "\f" then
            return "\\f"
        elseif c == "\t" then
            return "\\t"
        end
    end))
end

return function(v)
    local inspect = require("inspect")
    print(inspect(v), inspect(opts), to_escaped_toml_str [[dsahdash"
    dsadsa\ \t \b
    dsadsa'
    """
    '''
    adsdasdas]])
end
