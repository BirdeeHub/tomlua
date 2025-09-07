local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]
local tomlib = upvals[3]

-- TODO: write this in C
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

-- TODO: write this in C
local function escape_toml_key(str)
    if str:find("^[A-Za-z0-9_-]*$") ~= nil then
        return str
    else
        return escape_toml_str(str)
    end
end

-- use this for working with the headings and key = value
-- expose a c push_heading, push_value, and push_key on your lua string buffer type
return function(input)
    local inspect = require("inspect")
    print(inspect(input))
    print(inspect(opts))
    print(inspect(tomlib))
    local res = {
        is_array({}),
        is_array({ 1, 2, 3, 4 }),
        is_array({ a = 1, b = 2, c = 3, d = 4 }),
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
    }
    for k, v in pairs(res) do
        print(k, ":", v)
    end
end
