local upvals = {...}
local opts = upvals[1]
local is_array = upvals[2]
return function(t)
    local inspect = require("inspect")
    print(inspect(t), inspect(opts), inspect(is_array))
end
