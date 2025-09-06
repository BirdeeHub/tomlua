local opts = ({...})[1]
return function(t)
    local inspect = require("inspect")
    print(inspect(t), inspect(opts))
end
