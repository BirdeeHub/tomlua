local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]
return function(v)
    local inspect = require("inspect")
    print(inspect(v), inspect(opts))
end
