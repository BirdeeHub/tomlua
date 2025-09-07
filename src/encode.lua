local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]

local function escape_toml_str(str)
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

local function escape_toml_key(str)
    if str:find("^[A-Za-z0-9_-]*$") ~= nil then
        return str
    else
        return escape_toml_str(str)
    end
end

return function(v)
    local inspect = require("inspect")
    print(
        inspect(v),
        inspect(opts),
        escape_toml_str [[dsahdash"
        dsadsa\ \t \b
        dsadsa'
        """
        '''
        adsdasdas]],
        escape_toml_key [[1ab12-23_1sAAGG]],
        escape_toml_key [[dsahdash"
        dsadsa\ \t \b
        dsadsa'
        """
        '''
        adsdasdas]]
    )
end
