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


local core = require("core");
local color = require("color");
local config = require("config");
local screen = require("screen");
local drawer = require("drawer");

local menu = {};

local OnOff;
local skip;
local run;
local autoboot;
local carousel_choices = {};

menu.handlers = {
	-- Menu handlers take the current menu and selected entry as parameters,
	-- and should return a boolean indicating whether execution should
	-- continue or not. The return value may be omitted if this entry should
	-- have no bearing on whether we continue or not, indicating that we
	-- should just continue after execution.
	[core.MENU_ENTRY] = function(current_menu, entry)
		-- run function
		entry.func();
	end,
	[core.MENU_CAROUSEL_ENTRY] = function(current_menu, entry)
		-- carousel (rotating) functionality
		local carid = entry.carousel_id;
		local caridx = menu.getCarouselIndex(carid);
		local choices = entry.items();

		if (#choices > 0) then
			caridx = (caridx % #choices) + 1;
			menu.setCarouselIndex(carid, caridx);
			entry.func(caridx, choices[caridx], choices);
		end
	end,
	[core.MENU_SUBMENU] = function(current_menu, entry)
		-- recurse
		return menu.run(entry.submenu());
	end,
	[core.MENU_RETURN] = function(current_menu, entry)
		-- allow entry to have a function/side effect
		if (entry.func ~= nil) then
			entry.func();
		end
		return false;
	end,
};
-- loader menu tree is rooted at menu.welcome

menu.boot_options = {
	entries = {
		-- return to welcome menu
		{
			entry_type = core.MENU_RETURN,
			name = function()
				return "Back to main menu" ..
				    color.highlight(" [Backspace]");
			end
		},

		-- load defaults
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return "Load System " .. color.highlight("D") ..
				    "efaults";
			end,
			func = function()
				core.setDefaults();
			end,
			alias = {"d", "D"}
		},

		{
			entry_type = core.MENU_SEPARATOR,
			name = function()
				return "";
			end
		},

		{
			entry_type = core.MENU_SEPARATOR,
			name = function()
				return "Boot Options:";
			end
		},

		-- acpi
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff(color.highlight("A") ..
				    "CPI       :", core.acpi);
			end,
			func = function()
				core.setACPI();
			end,
			alias = {"a", "A"}
		},
		-- safe mode
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff("Safe " .. color.highlight("M") ..
				    "ode  :", core.sm);
			end,
			func = function()
				core.setSafeMode();
			end,
			alias = {"m", "M"}
		},
		-- single user
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff(color.highlight("S") ..
				    "ingle user:", core.su);
			end,
			func = function()
				core.setSingleUser();
			end,
			alias = {"s", "S"}
		},
		-- verbose boot
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff(color.highlight("V") ..
				    "erbose    :", core.verbose);
			end,
			func = function()
				core.setVerbose();
			end,
			alias = {"v", "V"}
		},
	},
};

menu.welcome = {
	entries = function()
		local menu_entries = menu.welcome.all_entries;
		-- Swap the first two menu items on single user boot
		if (core.isSingleUserBoot()) then
			-- We'll cache the swapped menu, for performance
			if (menu.welcome.swapped_menu ~= nil) then
				return menu.welcome.swapped_menu;
			end
			-- Shallow copy the table
			menu_entries = core.shallowCopyTable(menu_entries);

			-- Swap the first two menu entries
			menu_entries[1], menu_entries[2] =
			    menu_entries[2], menu_entries[1];

			-- Then set their names to their alternate names
			menu_entries[1].name, menu_entries[2].name =
			    menu_entries[1].alternate_name,
			    menu_entries[2].alternate_name;
			menu.welcome.swapped_menu = menu_entries;
		end
		return menu_entries;
	end,
	all_entries = {
		-- boot multi user
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return color.highlight("B") ..
				    "oot Multi user " ..
				    color.highlight("[Enter]");
			end,
			-- Not a standard menu entry function!
			alternate_name = function()
				return color.highlight("B") ..
				    "oot Multi user";
			end,
			func = function()
				core.setSingleUser(false);
				core.boot();
			end,
			alias = {"b", "B"}
		},

		-- boot single user
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return "Boot " .. color.highlight("S") ..
				    "ingle user";
			end,
			-- Not a standard menu entry function!
			alternate_name = function()
				return "Boot " .. color.highlight("S") ..
				    "ingle user " .. color.highlight("[Enter]");
			end,
			func = function()
				core.setSingleUser(true);
				core.boot();
			end,
			alias = {"s", "S"}
		},

		-- escape to interpreter
		{
			entry_type = core.MENU_RETURN,
			name = function()
				return color.highlight("Esc") ..
				    "ape to loader prompt";
			end,
			func = function()
				loader.setenv("autoboot_delay", "NO");
			end,
			alias = {core.KEYSTR_ESCAPE}
		},

		-- reboot
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return color.highlight("R") .. "eboot";
			end,
			func = function()
				loader.perform("reboot");
			end,
			alias = {"r", "R"}
		},


		{
			entry_type = core.MENU_SEPARATOR,
			name = function()
				return "";
			end
		},

		{
			entry_type = core.MENU_SEPARATOR,
			name = function()
				return "Options:";
			end
		},

		-- kernel options
		{
			entry_type = core.MENU_CAROUSEL_ENTRY,
			carousel_id = "kernel",
			items = core.kernelList,
			name = function(idx, choice, all_choices)
				if (#all_choices == 0) then
					return "Kernel: ";
				end

				local is_default = (idx == 1);
				local kernel_name = "";
				local name_color;
				if (is_default) then
					name_color = color.escapef(color.GREEN);
					kernel_name = "default/";
				else
					name_color = color.escapef(color.BLUE);
				end
				kernel_name = kernel_name .. name_color ..
				    choice .. color.default();
				return color.highlight("K") .. "ernel: " ..
				    kernel_name .. " (" .. idx .. " of " ..
				    #all_choices .. ")";
			end,
			func = function(idx, choice, all_choices)
				config.selectkernel(choice);
			end,
			alias = {"k", "K"}
		},

		-- boot options
		{
			entry_type = core.MENU_SUBMENU,
			name = function()
				return "Boot " .. color.highlight("O") ..
				    "ptions";
			end,
			submenu = function()
				return menu.boot_options;
			end,
			alias = {"o", "O"}
		},
	},
};

-- The first item in every carousel is always the default item.
function menu.getCarouselIndex(id)
	local val = carousel_choices[id];
	if (val == nil) then
		return 1;
	end
	return val;
end

function menu.setCarouselIndex(id, idx)
	carousel_choices[id] = idx;
end

function menu.run(m)

	if (menu.skip()) then
		core.autoboot();
		return false;
	end

	if (m == nil) then
		m = menu.welcome;
	end

	-- redraw screen
	screen.clear();
	screen.defcursor();
	local alias_table = drawer.drawscreen(m);

	menu.autoboot();

	cont = true;
	while (cont) do
		local key = io.getchar();

		-- Special key behaviors
		if ((key == core.KEY_BACKSPACE) or (key == core.KEY_DELETE)) and
		    (m ~= menu.welcome) then
			break;
		elseif (key == core.KEY_ENTER) then
			core.boot();
			-- Should not return
		end

		key = string.char(key)
		-- check to see if key is an alias
		local sel_entry = nil;
		for k, v in pairs(alias_table) do
			if (key == k) then
				sel_entry = v;
			end
		end

		-- if we have an alias do the assigned action:
		if (sel_entry ~= nil) then
			-- Get menu handler
			local handler = menu.handlers[sel_entry.entry_type];
			if (handler ~= nil) then
				-- The handler's return value indicates whether
				-- we need to exit this menu. An omitted return
				-- value means "continue" by default.
				cont = handler(m, sel_entry);
				if (cont == nil) then
					cont = true;
				end
			end
			-- if we got an alias key the screen is out of date:
			screen.clear();
			screen.defcursor();
			alias_table = drawer.drawscreen(m);
		end
	end

	if (m == menu.welcome) then
		screen.defcursor();
		print("Exiting menu!");
		config.loadelf();
		return false;
	end

	return true;
end

function menu.skip()
	if (core.isSerialBoot()) then
		return true;
	end
	local c = string.lower(loader.getenv("console") or "");
	if ((c:match("^efi[ ;]") or c:match("[ ;]efi[ ;]")) ~= nil) then
		return true;
	end

	c = string.lower(loader.getenv("beastie_disable") or "");
	print("beastie_disable", c);
	return c == "yes";
end

function menu.autoboot()
	if (menu.already_autoboot == true) then
		return;
	end
	menu.already_autoboot = true;

	local ab = loader.getenv("autoboot_delay");
	if (ab ~= nil) and (ab:lower() == "no") then
		return;
	elseif (tonumber(ab) == -1) then
		core.boot();
	end
	ab = tonumber(ab) or 10;

	local x = loader.getenv("loader_menu_timeout_x") or 5;
	local y = loader.getenv("loader_menu_timeout_y") or 22;

	local endtime = loader.time() + ab;
	local time;

	repeat
		time = endtime - loader.time();
		screen.setcursor(x, y);
		print("Autoboot in " .. time ..
		    " seconds, hit [Enter] to boot" ..
		    " or any other key to stop     ");
		screen.defcursor();
		if (io.ischar()) then
			local ch = io.getchar();
			if (ch == core.KEY_ENTER) then
				break;
			else
				-- erase autoboot msg
				screen.setcursor(0, y);
				print("                                        "
				    .. "                                        ");
				screen.defcursor();
				return;
			end
		end

		loader.delay(50000);
	until time <= 0;
	core.boot();

end

function OnOff(str, b)
	if (b) then
		return str .. color.escapef(color.GREEN) .. "On" ..
		    color.escapef(color.WHITE);
	else
		return str .. color.escapef(color.RED) .. "off" ..
		    color.escapef(color.WHITE);
	end
end

return menu;
