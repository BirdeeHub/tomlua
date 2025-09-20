local function deepeq(a, b)
	-- Different types: false
	if type(a) ~= type(b) then return false end
	-- Functions
	if type(a) == 'function' then
		return string.dump(a) == string.dump(b)
	end
	-- Primitives and equal pointers
	if a == b then return true end
	-- Only equal tables could have passed previous tests
	if type(a) ~= 'table' then return false end
	-- Compare tables field by field
	for k, v in pairs(a) do
		if b[k] == nil or not deepeq(v, b[k]) then return false end
	end
	for k, v in pairs(b) do
		if a[k] == nil or not deepeq(v, a[k]) then return false end
	end
	return true
end

-- Compatibility for Lua 5.1 and Lua 5.2
local function args(...)
	return { n = select('#', ...), ... }
end

local function spy(f)
	local s = {}
	setmetatable(s, {
		__call = function(s, ...)
			s.called = s.called or {}
			local a = args(...)
			table.insert(s.called, { ... })
			if f then
				local r
				r = args(pcall(f, (unpack or table.unpack)(a, 1, a.n)))
				if not r[1] then
					s.errors = s.errors or {}
					s.errors[#s.called] = r[2]
				else
					return (unpack or table.unpack)(r, 2, r.n)
				end
			end
		end
	})
	return s
end

local pendingtests = {}
local env = _G

local function runpending()
	if pendingtests[1] ~= nil then pendingtests[1](runpending) end
end

return setmetatable({
	tests_passed = 0,
	tests_failed = 0,
	gambiarrahandler = function(self, e, desc, msg, err)
		local suffix = tostring(msg) .. (err and "\n(with error: " .. err .. ")" or "")
		if e == 'pass' then
			print("   [32m✔[0m " .. suffix)
			self.tests_passed = self.tests_passed + 1
		elseif e == 'fail' then
			print("   [31m✘[0m " .. suffix)
			self.tests_failed = self.tests_failed + 1
		elseif e == 'except' then
			print("   [35m‼[0m " .. suffix)
			self.tests_failed = self.tests_failed + 1
		elseif e == 'begin' then
			print(" \x1b[36m▶\x1b[0m " .. desc)
		elseif e == 'end' then
		end
	end,
}, {
	__index = function(self, key)
		if key == 'end_tests' then
			return function()
				if (self.tests_failed or 0) > 0 then os.exit(1) end
			end
		end
	end,
	__call = function(self, name, f, async)
		if type(name) == 'function' then
			self.gambiarrahandler = name
			env = f or _G
			return
		end

		local testfn = function(next)
			local prev = {
				it = env.it,
				spy = env.spy,
				eq = env.eq
			}

			local restore = function()
				env.it = prev.it
				env.spy = prev.spy
				env.eq = prev.eq
				self.gambiarrahandler(self, 'end', name)
				table.remove(pendingtests, 1)
				if next then next() end
			end

			local handler = self.gambiarrahandler

			env.eq = deepeq
			env.spy = spy
			env.it = function(cond, msg, should_fail)
				if not msg then
					msg = debug.getinfo(2, 'S').short_src .. ":" .. debug.getinfo(2, 'l')
					.currentline
				end
				if type(cond) == 'function' then
					local control, value = pcall(cond)
					if should_fail and not control or not should_fail and control then
						handler(self, 'pass', name, msg)
					else
						handler(self, 'fail', name, msg, not should_fail and tostring(value) or "Task failed successfully. No error, that is the problem.")
					end
				elseif should_fail and not cond or not should_fail and cond then
					handler(self, 'pass', name, msg)
				else
					handler(self, 'fail', name, msg)
				end
			end

			handler(self, 'begin', name);
			local ok, err = pcall(f, restore)
			if not ok then
				handler(self, 'except', name, err)
			end

			if not async then
				handler(self, 'end', name);
				env.it = prev.it;
				env.spy = prev.spy;
				env.eq = prev.eq;
			end
		end

		if not async then
			testfn()
		else
			table.insert(pendingtests, testfn)
			if #pendingtests == 1 then
				runpending()
			end
		end
	end
})
