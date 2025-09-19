---@meta
error("Cannot import a meta module")

_G.it = function(cond, msg, should_fail) end
_G.eq = function(a, b) end
_G.spy = function(f) end
