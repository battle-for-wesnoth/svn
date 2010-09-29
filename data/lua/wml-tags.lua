--! #textdomain wesnoth

-- Backward-compatibility hack to avoid executing the file twice due to
-- old preload events. (To be removed in 1.11.) The hack assumes this
-- file is the first one to be executed.
if wesnoth.package["lua/helper.lua"] then return end

function wesnoth.game_events.on_load(cfg)
	if #cfg == 0 then return end
	local t = {}
	for i = 1,#cfg do t[i] = string.format("[%s]", cfg[i][1]) end
	error(string.format("~wml:%s not supported at scenario toplevel", table.concat(t, ', ')), 0)
end

function wesnoth.game_events.on_save()
	return {}
end

wesnoth.require "lua/wml/objectives.lua"
wesnoth.require "lua/wml/items.lua"

local helper = wesnoth.require "lua/helper.lua"
local wml_actions = wesnoth.wml_actions

local function trim(s)
	local r = string.gsub(s, "^%s*(.-)%s*$", "%1")
	return r
end

local engine_message = wml_actions.message

function wml_actions.message(cfg)
	local show_if = helper.get_child(cfg, "show_if")
	if not show_if or wesnoth.eval_conditional(show_if) then
		engine_message(cfg)
	end
end

local function get_team(cfg, tag)
	local side = tonumber(cfg.side or 1) or
		helper.wml_error(tag .. " given a noninteger side= attribute.")
	local team = wesnoth.sides[side] or
		helper.wml_error(tag .. " given an invalid side= attribute.")
	return team
end

function wml_actions.chat(cfg)
	local side_list = cfg.side
	local message = tostring(cfg.message) or
		helper.wml_error "[chat] missing required message= attribute."

	local speaker = cfg.speaker
	if speaker then
		speaker = tostring(speaker)
		local speaking_unit = wesnoth.get_variable(speaker)
		if speaking_unit then speaker = speaking_unit.name end
	else
		speaker = "WML"
	end

	if not side_list then
		wesnoth.message(speaker, message)
	else
		for v in string.gmatch(side_list, "[^%s,][^,]*") do
			local side = wesnoth.get_side(tonumber(v))
			if side.controller == "human" then
				wesnoth.message(speaker, message)
				break
			end
		end
	end
end

function wml_actions.gold(cfg)
	local team = get_team(cfg, "[gold]")
	local amount = tonumber(cfg.amount) or
		helper.wml_error "[gold] missing required amount= attribute."
	team.gold = team.gold + amount
end

function wml_actions.store_gold(cfg)
	local team = get_team(cfg, "[store_gold]")
	wesnoth.set_variable(cfg.variable or "gold", team.gold)
end

function wml_actions.clear_variable(cfg)
	local names = cfg.name or
		helper.wml_error "[clear_variable] missing required name= attribute."
	for w in string.gmatch(names, "[^%s,][^,]*") do
		wesnoth.set_variable(trim(w))
	end
end

function wml_actions.store_unit_type_ids(cfg)
	local types = {}
	for k,v in pairs(wesnoth.unit_types) do
		table.insert(types, k)
	end
	table.sort(types)
	types = table.concat(types, ',')
	wesnoth.set_variable(cfg.variable or "unit_type_ids", types)
end

function wml_actions.store_unit_type(cfg)
	local var = cfg.variable or "unit_type"
	local types = cfg.type or
		helper.wml_error "[store_unit_type] missing required type= attribute."
	wesnoth.set_variable(var)
	local i = 0
	for w in string.gmatch(types, "[^%s,][^,]*") do
		local unit_type = wesnoth.unit_types[w] or
			helper.wml_error(string.format("Attempt to store nonexistent unit type '%s'.", w))
		wesnoth.set_variable(string.format("%s[%d]", var, i), unit_type.__cfg)
		i = i + 1
	end
end

function wml_actions.fire_event(cfg)
	local u1 = helper.get_child(cfg, "primary_unit")
	u1 = u1 and wesnoth.get_units(u1)[1]
	local x1, y1 = 0, 0
	if u1 then x1, y1 = u1.x, u1.y end

	local u2 = helper.get_child(cfg, "secondary_unit")
	u2 = u2 and wesnoth.get_units(u2)[1]
	local x2, y2 = 0, 0
	if u2 then x2, y2 = u2.x, u2.y end

	local w1 = helper.get_child(cfg, "primary_attack")
	local w2 = helper.get_child(cfg, "secondary_attack")
	if w2 then w1 = w1 or {} end

	wesnoth.fire_event(cfg.name, x1, y1, x2, y2, w1, w2)
end

function wml_actions.disallow_recruit(cfg)
	local team = get_team(cfg, "[disallow_recruit]")
	local v = team.recruit
	for w in string.gmatch(cfg.type, "[^%s,][^,]*") do
		for i, r in ipairs(v) do
			if r == w then
				table.remove(v, i)
				break
			end
		end
	end
	team.recruit = v
end

function wml_actions.set_recruit(cfg)
	local team = get_team(cfg, "[set_recruit]")
	local v = {}
	for w in string.gmatch(cfg.recruit, "[^%s,][^,]*") do
		table.insert(v, w)
	end
	team.recruit = v
end

function wml_actions.store_map_dimensions(cfg)
	local var = cfg.variable or "map_size"
	local w, h, b = wesnoth.get_map_size()
	wesnoth.set_variable(var .. ".width", w)
	wesnoth.set_variable(var .. ".height", h)
	wesnoth.set_variable(var .. ".border_size", b)
end

function wml_actions.unit_worth(cfg)
	local u = wesnoth.get_units(cfg)[1] or
		helper.wml_error "[unit_worth]'s filter didn't match any unit"
	local ut = wesnoth.unit_types[u.type]
	local hp = u.hitpoints / u.max_hitpoints
	local xp = u.experience / u.max_experience
	local best_adv = ut.cost
	for w in string.gmatch(ut.__cfg.advances_to, "[^%s,][^,]*") do
		local uta = wesnoth.unit_types[w]
		if uta and uta.cost > best_adv then best_adv = uta.cost end
	end
	wesnoth.set_variable("cost", ut.cost)
	wesnoth.set_variable("next_cost", best_adv)
	wesnoth.set_variable("health", math.floor(hp * 100))
	wesnoth.set_variable("experience", math.floor(xp * 100))
	wesnoth.set_variable("unit_worth", math.floor(math.max(ut.cost * hp, best_adv * xp)))
end

function wml_actions.wml_action(cfg)
	-- The new tag's name
	local name = cfg.name or
		helper.wml_error "[wml_action] missing required name= attribute."
	local code = cfg.lua_function or
		helper.wml_error "[wml_action] missing required lua_function= attribute."
	local bytecode, message = loadstring(code)
	if not bytecode then
		helper.wml_error("[wml_action] failed to compile Lua code: " .. message)
	end
	-- The lua function that is executed when the tag is called
	local lua_function = bytecode() or
		helper.wml_error "[wml_action] expects a Lua code returning a function."
	wml_actions[name] = lua_function
end

function wml_actions.lua(cfg)
	local cfg = helper.shallow_literal(cfg)
	local bytecode, message = loadstring(cfg.code or "")
	if not bytecode then error("~lua:" .. message, 0) end
	bytecode(helper.get_child(cfg, "args"))
end

function wml_actions.music(cfg)
	wesnoth.set_music(cfg)
end

local function handle_event_commands(cfg)
	for i = 1, #cfg do
		local v = cfg[i]
		local cmd = v[1]
		if not string.find(cmd, "^filter") and cmd ~= "condition" then
			cmd = wml_actions[cmd] or
				helper.wml_error(string.format("[%s] not supported", cmd))
			cmd(v[2])
		end
	end
	-- Apply music alterations once all the commands have been processed.
	wesnoth.set_music()
end

wml_actions.command = handle_event_commands

local function if_while_handler(max_iter, pass, fail, cfg)
	for i = 1, max_iter do
		local t = wesnoth.eval_conditional(cfg) and pass or fail
		if not t then return end
		for v in helper.child_range(cfg, t) do
			handle_event_commands(v)
		end
	end
end

local function if_handler(cfg)
	if_while_handler(1, "then", "else", cfg)
end

local function while_handler(cfg)
	if_while_handler(65536, "do", nil, cfg)
end

wml_actions["if"] = if_handler
wml_actions["while"] = while_handler

function wml_actions.switch(cfg)
	local value = wesnoth.get_variable(cfg.variable)
	local found = false
	-- Execute all the [case]s where the value matches.
	for v in helper.child_range(cfg, "case") do
		local match = false
		for w in string.gmatch(v.value, "[^%s,][^,]*") do
			if w == tostring(value) then match = true ; break end
		end
		if match then
			handle_event_commands(v)
			found = true
		end
	end
	-- Otherwise execute [else] statements.
	if not found then
		for v in helper.child_range(cfg, "else") do
			handle_event_commands(v)
		end
	end
end

function wml_actions.scroll_to(cfg)
	wesnoth.scroll_to_tile(cfg.x, cfg.y, cfg.check_fogged)
end

function wml_actions.scroll_to_unit(cfg)
	local u = wesnoth.get_units(cfg)[1]
	if not u then return end
	wesnoth.scroll_to_tile(u.x, u.y, cfg.check_fogged)
end

function wml_actions.unit_overlay(cfg)
	local img = cfg.image
	for i,u in ipairs(wesnoth.get_units(cfg)) do
		local ucfg = u.__cfg
		for w in string.gmatch(ucfg.overlays, "[^%s,][^,]*") do
			if w == img then ucfg = nil end
		end
		if ucfg then
			ucfg.overlays = ucfg.overlays .. ',' .. img
			wesnoth.put_unit(ucfg)
		end
	end
end

function wml_actions.remove_unit_overlay(cfg)
	local img = cfg.image
	for i,u in ipairs(wesnoth.get_units(cfg)) do
		local ucfg = u.__cfg
		local t = {}
		for w in string.gmatch(ucfg.overlays, "[^%s,][^,]*") do
			if w ~= img then table.insert(t, w) end
		end
		ucfg.overlays = table.concat(t, ',')
		wesnoth.put_unit(ucfg)
	end
end

function wml_actions.store_turns(cfg)
	local var = cfg.variable or "turns"
	wesnoth.set_variable(var, wesnoth.game_config.last_turn)
end

function wml_actions.store_unit(cfg)
	local filter = helper.get_child(cfg, "filter") or
		helper.wml_error "[store_unit] missing required [filter] tag"
	local kill_units = cfg.kill

	local var = cfg.variable or "unit"
	local idx = 0
	if cfg.mode == "append" then
		idx = wesnoth.get_variable(var .. ".length")
	else
		wesnoth.set_variable(var)
	end

	for i,u in ipairs(wesnoth.get_units(filter)) do
		wesnoth.set_variable(string.format("%s[%d]", var, idx), u.__cfg)
		idx = idx + 1
		if kill_units then wesnoth.put_unit(u.x, u.y) end
	end

	if (not cfg.x or cfg.x == "recall") and (not cfg.y or cfg.y == "recall") then
		for i,u in ipairs(wesnoth.get_recall_units(filter)) do
			local ucfg = u.__cfg
			ucfg.x = "recall"
			ucfg.y = "recall"
			wesnoth.set_variable(string.format("%s[%d]", var, idx), ucfg)
			idx = idx + 1
			if kill_units then wesnoth.extract_unit(u) end
		end
	end
end

function wml_actions.sound(cfg)
	wesnoth.play_sound(cfg.name, cfg["repeat"])
end

function wml_actions.store_locations(cfg)
	local var = cfg.variable or "location"
	-- the variable can be mentioned in a [find_in] subtag, so it
	-- cannot be cleared before the locations are recovered
	local locs = wesnoth.get_locations(cfg)
	wesnoth.set_variable(var)
	for i, loc in ipairs(locs) do
		local x, y = loc[1], loc[2]
		local t = wesnoth.get_terrain(x, y)
		local res = { x = x, y = y, terrain = t }
		if wesnoth.get_terrain_info(t).village then
			res.owner_side = wesnoth.get_village_owner(x, y) or 0
		end
		wesnoth.set_variable(string.format("%s[%d]", var, i - 1), res)
	end
end

function wml_actions.store_reachable_locations(cfg)
	local unit_filter = helper.get_child(cfg, "filter") or
		helper.wml_error "[store_reachable_locations] missing required [filter] tag"
	local location_filter = helper.get_child(cfg, "filter_location")
	local range = cfg.range or "movement"
	local variable = cfg.variable or helper.wml_error "[store_reachable_locations] missing required variable= key"


	local locs = {}
	for i,unit in ipairs(wesnoth.get_units(unit_filter)) do
		local reach = wesnoth.find_reach(unit)

		for j,loc in ipairs(reach) do
			if wesnoth.match_location(loc[1], loc[2], location_filter) then

				table.insert(locs, {loc[1], loc[2]})
			end
		end
	end

	if range == "attack" then
		-- doesn't work yet
	end

	helper.set_variable_array(variable, locs)
end

function wml_actions.hide_unit(cfg)
	for i,u in ipairs(wesnoth.get_units(cfg)) do
		u.hidden = true
	end
	wml_actions.redraw {}
end

function wml_actions.unhide_unit(cfg)
	for i,u in ipairs(wesnoth.get_units(cfg)) do
		u.hidden = false
	end
	wml_actions.redraw {}
end

function wml_actions.modify_unit(cfg)
	local unit_variable = "LUA_modify_unit"

	local function handle_attributes(cfg, unit_path)
		for current_key, current_value in pairs(cfg) do
			if type(current_value) ~= "table" then
				wesnoth.set_variable(string.format("%s.%s", unit_path, current_key), current_value)
			end
		end
	end

	local function handle_child(cfg, unit_path)
		local children_handled = {}
		handle_attributes(cfg, unit_path)

		for current_index, current_table in ipairs(cfg) do
			local current_tag = current_table[1]
			local tag_index = children_handled[current_tag] or 0
			handle_child(current_table[2], string.format("%s.%s[%u]",
				unit_path, current_tag, tag_index))
			children_handled[current_tag] = tag_index + 1
		end
	end

	local function handle_unit(cfg, unit_num)
		local children_handled = {}
		local unit_path = string.format("%s[%u]", unit_variable, unit_num)
		handle_attributes(cfg, unit_path)

		for current_index, current_table in ipairs(cfg) do
			local current_tag = current_table[1]
			if current_tag == "filter" then
				-- nothing
			elseif current_tag == "object" or current_tag == "trait" then
				local unit = wesnoth.get_variable(unit_path)
				unit = wesnoth.create_unit(unit)
				wesnoth.add_modification(unit, current_tag, current_table[2])
				unit = unit.__cfg;
				wesnoth.set_variable(unit_path, unit)
			else
				local tag_index = children_handled[current_tag] or 0
				handle_child(current_table[2], string.format("%s.%s[%u]",
					unit_path, current_tag, tag_index))
				children_handled[current_tag] = tag_index + 1
			end
		end

		local type = cfg.type
		if type then
			wesnoth.set_variable(unit_path .. ".advances_to", type)
			wesnoth.set_variable(unit_path .. ".experience", wesnoth.get_variable(unit_path .. ".max_experience"))
		end
		wml_actions.unstore_unit { variable = unit_path }
	end

	local filter = helper.get_child(cfg, "filter") or helper.wml_error "[modify_unit] missing required [filter] tag"
	local cfg = helper.parsed(cfg)

	wml_actions.store_unit { {"filter", filter}, variable = unit_variable, kill = true }
	local max_index = wesnoth.get_variable(unit_variable .. ".length") - 1

	for current_unit = 0, max_index do
		handle_unit(cfg, current_unit)
	end

	wesnoth.set_variable(unit_variable)
end

function wml_actions.move_unit(cfg)
	local coordinate_error = "invalid coordinate in [move_unit]"
	local to_x = tostring(cfg.to_x) or helper.wml_error(coordinate_error)
	local to_y = tostring(cfg.to_y) or helper.wml_error(coordinate_error)
	local fire_event = cfg.fire_event
	cfg = helper.literal(cfg)
	cfg.to_x, cfg.to_y, cfg.fire_event = nil, nil, nil
	local units = wesnoth.get_units(cfg)

	local pattern = "[^%s,]+"
	for current_unit_index, current_unit in ipairs(units) do
		if not fire_event or current_unit.valid then
			local xs, ys = string.gmatch(to_x, pattern), string.gmatch(to_y, pattern)
			local move_string_x = current_unit.x
			local move_string_y = current_unit.y

			local x, y = xs(), ys()
			while true do
				x = tonumber(x) or helper.wml_error(coordinate_error)
				y = tonumber(y) or helper.wml_error(coordinate_error)
				x, y = wesnoth.find_vacant_tile(x, y, current_unit)
				move_string_x = string.format("%s,%u", move_string_x, x)
				move_string_y = string.format("%s,%u", move_string_y, y)
				local next_x, next_y = xs(), ys()
				if not next_x and not next_y then break end
				x, y = next_x, next_y
			end

			if current_unit.x < x then current_unit.facing = "se"
			elseif current_unit.x > x then current_unit.facing = "sw"
			end

			wesnoth.extract_unit(current_unit)
			local current_unit_cfg = current_unit.__cfg
			wml_actions.move_unit_fake {
				type = current_unit_cfg.type,
				gender = current_unit_cfg.gender,
				variation = current_unit_cfg.variation,
				side = current_unit_cfg.side,
				x = move_string_x,
				y = move_string_y
			}
			local x2, y2 = current_unit.x, current_unit.y
			current_unit.x, current_unit.y = x, y
			wesnoth.put_unit(current_unit)

			if fire_event then
				wesnoth.fire_event("moveto", x, y, x2, y2)
			end
		end
	end
end

function wml_actions.capture_village(cfg)
	local side = cfg.side
	if side then side = tonumber(side) or helper.wml_error("invalid side in [capture_village]") end
	local locs = wesnoth.get_locations(cfg)

	for i, loc in ipairs(locs) do
		wesnoth.set_village_owner(loc[1], loc[2], side)
	end
end

function wml_actions.terrain(cfg)
	local terrain = cfg.terrain or helper.wml_error("[terrain] missing required terrain= attribute")
	cfg = helper.shallow_literal(cfg)
	cfg.terrain = nil
	for i, loc in ipairs(wesnoth.get_locations(cfg)) do
		wesnoth.set_terrain(loc[1], loc[2], terrain, cfg.layer, cfg.replace_if_failed)
	end
end
