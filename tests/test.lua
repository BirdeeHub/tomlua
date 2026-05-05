#!/usr/bin/env lua
local cfg = string.gmatch(package.config, "(%S+)")
local dirsep, pathsep, pathmark = cfg() or "/", cfg() or ";", cfg() or "?"
local function cwd()
    local sep = dirsep
    local info = debug.getinfo(1, "S")
    local source = info.source
    if source:sub(1, 1) == "@" then
        local realpath = ((vim or {}).uv or (vim or {}).loop or {}).fs_realpath
        local path = source:sub(2)
        path = realpath and realpath(path) or path
        local dir, file = path:match("^(.*[" .. sep .. "])([^" .. sep .. "]+)$")
        return dir or ("." .. sep), file
    end
    return "." .. sep, nil
end
local here = cwd()
local dircat = function(...) return table.concat({...}, dirsep) end
local pathcat = function(...) return table.concat({...}, pathsep) end
package.path = pathcat(package.path, dircat(here .. "gambiarra", pathmark .. ".lua"))

if arg[1] then
    package.cpath = pathcat(dircat(arg[1], pathmark .. ".so"), package.cpath)
end

local run_toml_edit = not arg[4] -- if arg[4] then skip toml edit

if tonumber(arg[2]) == 1 then
	local test = require('gambiarra')
	dofile(here .. 'scratch.lua')(test)
	test.await(function(self)
		self.report()
		if (self.tests_failed or 0) > 0 then
			os.exit(1)
		else
			os.exit(0)
		end
	end)
elseif tonumber(arg[2]) == 2 then
	dofile(here .. 'bench.lua')(here, arg[3] and tonumber(arg[3]) or 100, run_toml_edit)
else
	local test = require("gambiarra")

	local files = test.read_dir(here, function(filename)
		return filename:match("_test%.lua$")
	end)
	for _, file in ipairs(files) do
		local success, msg = pcall(loadfile or load, here .. file)
		if success then
			---@cast msg function
			success, msg = pcall(msg, test, here)
		end
		io.write(
			"\n "
				.. test.icons._end
				.. " "
				.. file
				.. " "
				.. (success and test.icons.pass or test.icons.fail)
				.. (msg and " : " .. tostring(msg) or "")
				.. "\n"
		)
	end

	test.await(function(self)
		self.report()
		if (self.tests_failed or 0) > 0 then
			os.exit(1)
		else
			os.exit(0)
		end
	end)
end
