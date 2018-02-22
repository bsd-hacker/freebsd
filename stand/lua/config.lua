--
-- Copyright (c) 2015 Pedro Souza <pedrosouza@freebsd.org>
-- Copyright (C) 2018 Kyle Evans <kevans@FreeBSD.org>
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--
-- $FreeBSD$
--

local config = {}

local modules = {}

local pattern_table
local carousel_choices = {}

pattern_table = {
	[1] = {
		str = "^%s*(#.*)",
		process = function(_, _)  end
	},
	--  module_load="value"
	[2] = {
		str = "^%s*([%w_]+)_load%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			if modules[k] == nil then
				modules[k] = {}
			end
			modules[k].load = v:upper()
		end
	},
	--  module_name="value"
	[3] = {
		str = "^%s*([%w_]+)_name%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "name", v)
		end
	},
	--  module_type="value"
	[4] = {
		str = "^%s*([%w_]+)_type%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "type", v)
		end
	},
	--  module_flags="value"
	[5] = {
		str = "^%s*([%w_]+)_flags%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "flags", v)
		end
	},
	--  module_before="value"
	[6] = {
		str = "^%s*([%w_]+)_before%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "before", v)
		end
	},
	--  module_after="value"
	[7] = {
		str = "^%s*([%w_]+)_after%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "after", v)
		end
	},
	--  module_error="value"
	[8] = {
		str = "^%s*([%w_]+)_error%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			config.setKey(k, "error", v)
		end
	},
	--  exec="command"
	[9] = {
		str = "^%s*exec%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, _)
			if loader.perform(k) ~= 0 then
				print("Failed to exec '" .. k .. "'")
			end
		end
	},
	--  env_var="value"
	[10] = {
		str = "^%s*([%w%p]+)%s*=%s*\"([%w%s%p]-)\"%s*(.*)",
		process = function(k, v)
			if config.setenv(k, v) ~= 0 then
				print("Failed to set '" .. k ..
				    "' with value: " .. v .. "")
			end
		end
	},
	--  env_var=num
	[11] = {
		str = "^%s*([%w%p]+)%s*=%s*(%d+)%s*(.*)",
		process = function(k, v)
			if config.setenv(k, v) ~= 0 then
				print("Failed to set '" .. k ..
				    "' with value: " .. v .. "")
			end
		end
	}
}

-- Module exports
-- Which variables we changed
config.env_changed = {}
-- Values to restore env to (nil to unset)
config.env_restore = {}

-- The first item in every carousel is always the default item.
function config.getCarouselIndex(id)
	local val = carousel_choices[id]
	if val == nil then
		return 1
	end
	return val
end

function config.setCarouselIndex(id, idx)
	carousel_choices[id] = idx
end

function config.restoreEnv()
	-- Examine changed environment variables
	for k, v in pairs(config.env_changed) do
		local restore_value = config.env_restore[k]
		if restore_value == nil then
			-- This one doesn't need restored for some reason
			goto continue
		end
		local current_value = loader.getenv(k)
		if current_value ~= v then
			-- This was overwritten by some action taken on the menu
			-- most likely; we'll leave it be.
			goto continue
		end
		restore_value = restore_value.value
		if restore_value ~= nil then
			loader.setenv(k, restore_value)
		else
			loader.unsetenv(k)
		end
		::continue::
	end

	config.env_changed = {}
	config.env_restore = {}
end

function config.setenv(k, v)
	-- Track the original value for this if we haven't already
	if config.env_restore[k] == nil then
		config.env_restore[k] = {value = loader.getenv(k)}
	end

	config.env_changed[k] = v

	return loader.setenv(k, v)
end

function config.setKey(k, n, v)
	if modules[k] == nil then
		modules[k] = {}
	end
	modules[k][n] = v
end

function config.lsModules()
	print("== Listing modules")
	for k, v in pairs(modules) do
		print(k, v.load)
	end
	print("== List of modules ended")
end


function config.isValidComment(c)
	if c ~= nil then
		local s = c:match("^%s*#.*")
		if s == nil then
			s = c:match("^%s*$")
		end
		if s == nil then
			return false
		end
	end
	return true
end

function config.loadmod(mod, silent)
	local status = true
	for k, v in pairs(mod) do
		if v.load == "YES" then
			local str = "load "
			if v.flags ~= nil then
				str = str .. v.flags .. " "
			end
			if v.type ~= nil then
				str = str .. "-t " .. v.type .. " "
			end
			if v.name ~= nil then
				str = str .. v.name
			else
				str = str .. k
			end

			if v.before ~= nil then
				if loader.perform(v.before) ~= 0 then
					if not silent then
						print("Failed to execute '" ..
						    v.before ..
						    "' before loading '" .. k ..
						    "'")
					end
					status = false
				end
			end

			if loader.perform(str) ~= 0 then
				if not silent then
					print("Failed to execute '" .. str ..
					    "'")
				end
				if v.error ~= nil then
					loader.perform(v.error)
				end
				status = false
			end

			if v.after ~= nil then
				if loader.perform(v.after) ~= 0 then
					if not silent then
						print("Failed to execute '" ..
						    v.after ..
						    "' after loading '" .. k ..
						    "'")
					end
					status = false
				end
			end

--		else
--			if not silent then
--				print("Skipping module '". . k .. "'")
--			end
		end
	end

	return status
end

-- silent runs will not return false if we fail to open the file
function config.parse(name, silent)
	if silent == nil then
		silent = false
	end
	local f = io.open(name)
	if f == nil then
		if not silent then
			print("Failed to open config: '" .. name .. "'")
		end
		return silent
	end

	local text, _ = io.read(f)

	if text == nil then
		if not silent then
			print("Failed to read config: '" .. name .. "'")
		end
		return silent
	end

	local n = 1
	local status = true

	for line in text:gmatch("([^\n]+)") do
		if line:match("^%s*$") == nil then
			local found = false

			for _, val in ipairs(pattern_table) do
				local k, v, c = line:match(val.str)
				if k ~= nil then
					found = true

					if config.isValidComment(c) then
						val.process(k, v)
					else
						print("Malformed line (" .. n ..
						    "):\n\t'" .. line .. "'")
						status = false
					end

					break
				end
			end

			if not found then
				print("Malformed line (" .. n .. "):\n\t'" ..
				    line .. "'")
				status = false
			end
		end
		n = n + 1
	end

	return status
end

-- other_kernel is optionally the name of a kernel to load, if not the default
-- or autoloaded default from the module_path
function config.loadkernel(other_kernel)
	local flags = loader.getenv("kernel_options") or ""
	local kernel = other_kernel or loader.getenv("kernel")

	local try_load = function (names)
		for name in names:gmatch("([^;]+)%s*;?") do
			local r = loader.perform("load " .. flags ..
			    " " .. name)
			if r == 0 then
				return name
			end
		end
		return nil
	end

	local load_bootfile = function()
		local bootfile = loader.getenv("bootfile")

		-- append default kernel name
		if bootfile == nil then
			bootfile = "kernel"
		else
			bootfile = bootfile .. ";kernel"
		end

		return try_load(bootfile)
	end

	-- kernel not set, try load from default module_path
	if kernel == nil then
		local res = load_bootfile()

		if res ~= nil then
			-- Default kernel is loaded
			config.kernel_loaded = nil
			return true
		else
			print("No kernel set, failed to load from module_path")
			return false
		end
	else
		-- Use our cached module_path, so we don't end up with multiple
		-- automatically added kernel paths to our final module_path
		local module_path = config.module_path
		local res

		if other_kernel ~= nil then
			kernel = other_kernel
		end
		-- first try load kernel with module_path = /boot/${kernel}
		-- then try load with module_path=${kernel}
		local paths = {"/boot/" .. kernel, kernel}

		for _, v in pairs(paths) do
			loader.setenv("module_path", v)
			res = load_bootfile()

			-- succeeded, add path to module_path
			if res ~= nil then
				config.kernel_loaded = kernel
				if module_path ~= nil then
					loader.setenv("module_path", v .. ";" ..
					    module_path)
				end
				return true
			end
		end

		-- failed to load with ${kernel} as a directory
		-- try as a file
		res = try_load(kernel)
		if res ~= nil then
			config.kernel_loaded = kernel
			return true
		else
			print("Failed to load kernel '" .. kernel .. "'")
			return false
		end
	end
end

function config.selectkernel(kernel)
	config.kernel_selected = kernel
end

function config.load(file)
	if not file then
		file = "/boot/defaults/loader.conf"
	end

	if not config.parse(file) then
		print("Failed to parse configuration: '" .. file .. "'")
	end

	local f = loader.getenv("loader_conf_files")
	if f ~= nil then
		for name in f:gmatch("([%w%p]+)%s*") do
			-- These may or may not exist, and that's ok. Do a
			-- silent parse so that we complain on parse errors but
			-- not for them simply not existing.
			if not config.parse(name, true) then
				print("Failed to parse configuration: '" ..
				    name .. "'")
			end
		end
	end

	-- Cache the provided module_path at load time for later use
	config.module_path = loader.getenv("module_path")
end

-- Reload configuration
function config.reload(file)
	modules = {}
	config.restoreEnv()
	config.load(file)
end

function config.loadelf()
	local kernel = config.kernel_selected or config.kernel_loaded
	local loaded

	print("Loading kernel...")
	loaded = config.loadkernel(kernel)

	if not loaded then
		print("Failed to load any kernel")
		return
	end

	print("Loading configured modules...")
	if not config.loadmod(modules) then
		print("Could not load one or more modules!")
	end
end

return config
