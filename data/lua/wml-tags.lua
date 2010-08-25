--! #textdomain wesnoth

-- Backward-compatibility hack to avoid executing the file twice due to
-- old preload events. (To be removed in 1.11.) The hack assumes this
-- file is the first one to be executed.
if wesnoth.package["lua/helper.lua"] then return end

local helper = wesnoth.require "lua/helper.lua"
local wml_actions = wesnoth.wml_actions

local function trim(s)
	local r = string.gsub(s, "^%s*(.-)%s*$", "%1")
	return r
end

local function color_prefix(r, g, b)
	return string.format('<span foreground="#%02x%02x%02x">', r, g, b)
end

local function insert_before_nl(s, t)
	return string.gsub(tostring(s), "[^\n]*", "%0" .. t, 1)
end

local scenario_objectives = {}

local objectives_old_save = wesnoth.game_events.on_save
local objectives_old_load = wesnoth.game_events.on_load

function wesnoth.game_events.on_save()
	local custom_cfg = objectives_old_save and objectives_old_saves() or {}
	for i,v in pairs(scenario_objectives) do
		v.side = i
		table.insert(custom_cfg, { "objectives", v })
	end
	return custom_cfg
end

function wesnoth.game_events.on_load(cfg)
	for i = #cfg,1,-1 do
		local v = cfg[i]
		if v[1] == "objectives" then
			local v2 = v[2]
			scenario_objectives[v2.side or 0] = v2
			table.remove(cfg, i)
		end
	end
	if objectives_old_load then objectives_old_load(cfg) end
end

local function generate_objectives(cfg)
	-- Note: when changing the text formatting, remember to check if you also
	-- need to change the hardcoded default multiplayer objective text in
	-- multiplayer_connect.cpp.

	local _ = wesnoth.textdomain("wesnoth")
	local objectives = ""
	local win_objectives = ""
	local lose_objectives = ""
	local gold_carryover = ""
	local notes = ""

	local win_string = cfg.victory_string or _ "Victory:"
	local lose_string = cfg.defeat_string or _ "Defeat:"
	local gold_carryover_string = cfg.gold_carryover_string or _ "Gold carryover:"
	local notes_string = cfg.notes_string or _ "Notes:"

	local bullet = "&#8226; "

	for obj in helper.child_range(cfg, "objective") do
		local show_if = helper.get_child(obj, "show_if")
		if not show_if or wesnoth.eval_conditional(show_if) then
			local condition = obj.condition
			if condition == "win" then
				local caption = obj.caption

				if caption ~= nil then
					win_objectives = win_objectives .. caption .. "\n"
				end

				win_objectives = win_objectives .. color_prefix(0, 255, 0) .. bullet .. obj.description .. "</span>" .. "\n"
			elseif condition == "lose" then
				local caption = obj.caption
				local turn_counter = ""

				if obj.show_turn_counter then
					local current_turn = wesnoth.current.turn
					local turn_limit = wesnoth.game_config.last_turn

					if turn_limit >= current_turn then
						turn_counter = "<small> " .. string.format(tostring(_"(current_turn/turn_limit)^(%d/%d)"), current_turn, turn_limit) .. "</small>"
					end
				end

				if caption ~= nil then
					lose_objectives = lose_objectives .. caption .. "\n"
				end

				lose_objectives = lose_objectives .. color_prefix(255, 0, 0) .. bullet .. obj.description .. turn_counter .. "</span>" .. "\n"
			else
				wesnoth.message "Unknown condition, ignoring."
			end
		end
	end

	for obj in helper.child_range(cfg, "gold_carryover") do
		if obj.bonus ~= nil then
			if obj.bonus then
				gold_carryover = color_prefix(255, 255, 192) .. bullet .. "<small>" .. _"Early finish bonus." .. "</small></span>\n"
			else
				gold_carryover = color_prefix(255, 255, 192) .. bullet .. "<small>" .. _"No early finish bonus." .. "</small></span>\n"
			end
		end

		if obj.carryover_percentage then
			local carryover_amount_string = ""

			if obj.carryover_percentage == 0 then
				carryover_amount_string = _"No gold carried over to the next scenario."
			else
				carryover_amount_string = string.format(tostring(_ "%d%% of gold carried over to the next scenario."), obj.carryover_percentage)
			end

			gold_carryover = gold_carryover .. color_prefix(255, 255, 192) .. bullet .. "<small>" .. carryover_amount_string .. "</small></span>\n"
		end
	end

	for note in helper.child_range(cfg, "note") do
		if note.description ~= nil then
			notes = notes .. color_prefix(255, 255, 255) .. bullet .. "<small>" .. note.description .. "</small></span>\n"
		end
	end

	local summary = cfg.summary
	if summary then
		objectives = "<big>" .. insert_before_nl(summary, "</big>") .. "\n"
	end
	if win_objectives ~= "" then
		objectives = objectives .. "<big>" .. win_string .. "</big>\n" .. win_objectives
	end
	if lose_objectives ~= "" then
		objectives = objectives .. "<big>" .. lose_string .. "</big>\n" .. lose_objectives
	end
	if gold_carryover ~= "" then
		objectives = objectives .. gold_carryover_string .. "\n" .. gold_carryover
	end
	if notes ~= "" then
		objectives = objectives .. notes_string .. "\n" .. notes
	end
	local note = cfg.note
	if note then
		objectives = objectives .. note .. "\n"
	end

	return string.sub(tostring(objectives), 1, -2)
end

function wml_actions.objectives(cfg)
	cfg = helper.parsed(cfg)
	local side = cfg.side or 0
	local silent = cfg.silent

	-- Save the objectives in a WML variable in case they have to be regenerated later.
	cfg.side = nil
	cfg.silent = nil
	scenario_objectives[side] = cfg

	-- Generate objectives for the given sides
	local objectives = generate_objectives(cfg)
	if side == 0 then
		for side, team in ipairs(wesnoth.sides) do
			team.objectives = objectives
			if not silent then team.objectives_changed = true end
		end
	else
		local team = wesnoth.sides[side]
		team.objectives = objectives
		if not silent then team.objectives_changed = true end
	end
end

function wml_actions.show_objectives(cfg)
	local side = cfg.side or 0
	local cfg0 = scenario_objectives[0]
	if side == 0 then
		local objectives0 = cfg0 and generate_objectives(cfg0)
		for side, team in ipairs(wesnoth.sides) do
			cfg = scenario_objectives[side]
			local objectives = (cfg and generate_objectives(cfg)) or objectives0
			if objectives then team.objectives = objectives end
			team.objectives_changed = true
		end
	else
		local team = wesnoth.sides[side]
		cfg = scenario_objectives[side] or cfg0
		local objectives = cfg and generate_objectives(cfg)
		if objectives then team.objectives = objectives end
		team.objectives_changed = true
	end
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
	local cfg = helper.literal(cfg)
	local args = helper.get_child(cfg, "args") or {}
	local ev = wesnoth.current.event_context
	args.x1 = ev.x1
	args.y1 = ev.y1
	args.x2 = ev.x2
	args.y2 = ev.y2
	table.insert(args, ev[1])
	table.insert(args, ev[2])
	local bytecode, message = loadstring(cfg.code or "")
	if not bytecode then error("~lua:" .. message, 0) end
	bytecode(args)
end

function wml_actions.music(cfg)
	wesnoth.set_music(cfg)
end

local function handle_event_commands(cfg)
	for i = 1, #cfg do
		local v = cfg[i]
		local cmd = v[1]
		if not string.find(cmd, "^filter") then
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

	local function proxy_unit_is_valid(unit)
		local function dummy() return unit.x end
		return pcall(dummy)
	end

	local pattern = "[^%s,]+"
	for current_unit_index, current_unit in ipairs(units) do
		if fire_event and not proxy_unit_is_valid(current_unit) then
		else
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
