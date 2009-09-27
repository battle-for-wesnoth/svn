local helper = {}

--! Interrupts the current execution and displays a chat message that looks like a WML error.
function helper.wml_error(m)
	error("~wml:" .. m, 0)
end

--! Returns an iterator over teams that can be used in a for-in loop.
function helper.all_teams()
	local function f(s)
		local i = s.i
		local team = wesnoth.get_side(i)
		s.i = i + 1
		return team
	end
	return f, { i = 1 }
end

--! Returns the first subtag of @a cfg with the given @a name.
function helper.get_child(cfg, name)
	-- ipairs cannot be used on a vconfig object
	for i = 1, #cfg do
		local v = cfg[i]
		if v[1] == name then return v[2] end
	end
end

--! Returns an iterator over all the subtags of @a cfg with the given @a name.
function helper.child_range(cfg, tag)
	local function f(s)
		local c
		repeat
			local i = s.i
			c = cfg[i]
			if not c then return end
			s.i = i + 1
		until c[1] == tag
		return c[2]
	end
	return f, { i = 1 }
end

--! Modifies all the units satisfying the given @a filter.
--! @param vars key/value pairs that need changing.
--! @note Usable only during WML actions.
function helper.modify_unit(filter, vars)
	wesnoth.fire("store_unit", {
		[1] = { "filter", filter },
		variable = "LUA_modify_unit",
		kill = true
	})
	for i = 0, wesnoth.get_variable("LUA_modify_unit.length") - 1 do
		local u = "LUA_modify_unit[" .. i .. "]"
		for k, v in pairs(vars) do
			wesnoth.set_variable(u .. '.' .. k, v)
		end
		wesnoth.fire("unstore_unit", {
			variable = u,
			find_vacant = false
		})
	end
	wesnoth.set_variable("LUA_modify_unit")
end

--! Fakes the move of a unit satisfying the given @a filter to position @a x, @a y.
--! @note Usable only during WML actions.
function helper.move_unit_fake(filter, to_x, to_y)
	wesnoth.fire("store_unit", {
		[1] = { "filter", filter },
		variable = "LUA_move_unit",
		kill = false
	})
	local from_x = wesnoth.get_variable("LUA_move_unit.x")
	local from_y = wesnoth.get_variable("LUA_move_unit.y")

	wesnoth.fire("scroll_to", { x=from_x, y=from_y })

	if to_x < from_x then
		wesnoth.set_variable("LUA_move_unit.facing", "sw")
	elseif to_x > from_x then
		wesnoth.set_variable("LUA_move_unit.facing", "se")
	end
	wesnoth.set_variable("LUA_move_unit.x", to_x)
	wesnoth.set_variable("LUA_move_unit.y", to_y)

	wesnoth.fire("kill", {
		x = from_x,
		y = from_y,
		animate = false,
		fire_event = false
	})

	wesnoth.fire("move_unit_fake", {
		type      = "$LUA_move_unit.type",
		gender    = "$LUA_move_unit.gender",
		variation = "$LUA_move_unit.variation",
		side      = "$LUA_move_unit.side",
		x         = from_x .. ',' .. to_x,
		y         = from_y .. ',' .. to_y
	})

	wesnoth.fire("unstore_unit", { variable="LUA_move_unit", find_vacant=true })
	wesnoth.fire("redraw")
	wesnoth.set_variable("LUA_move_unit")
end

local variable_mt = {}

local function get_variable_proxy(k)
	local v = wesnoth.get_variable(k, true)
	if type(v) == "table" then
		v = setmetatable({ __varname = k }, variable_mt)
	end
	return v
end

local function set_variable_proxy(k, v)
	if getmetatable(v) == variable_mt then
		v = wesnoth.get_variable(v.__varname)
	end
	wesnoth.set_variable(k, v)
end

function variable_mt.__index(t, k)
	local i = tonumber(k)
	if i then
		k = t.__varname .. '[' .. i .. ']'
	else
		k = t.__varname .. '.' .. k
	end
	return get_variable_proxy(k)
end

function variable_mt.__newindex(t, k, v)
	local i = tonumber(k)
	if i then
		k = t.__varname .. '[' .. i .. ']'
	else
		k = t.__varname .. '.' .. k
	end
	set_variable_proxy(k, v)
end

local root_variable_mt = {
	__index    = function(t, k)    return get_variable_proxy(k)    end,
	__newindex = function(t, k, v)
		if type(v) == "function" then
			-- User-friendliness when _G is overloaded early.
			-- FIXME: It should be disabled outside the "preload" event.
			rawset(t, k, v)
		else
			set_variable_proxy(k, v)
		end
	end
}

--! Sets the metable of @a t so that it can be used to access WML variables.
--! @return @a t.
--! @code
--! helper.set_wml_var_metatable(_G)
--! my_persistent_variable = 42
--! @endcode
function helper.set_wml_var_metatable(t)
	return setmetatable(t, root_variable_mt)
end

local fire_action_mt = {
	__index = function(t, n)
		return function(cfg) wesnoth.fire(n, cfg) end
	end
}

--! Sets the metable of @a t so that it can be used to fire WML actions.
--! @return @a t.
--! @code
--! W = helper.set_wml_action_metatable {}
--! W.message { speaker = "narrator", message = "?" }
--! @endcode
function helper.set_wml_action_metatable(t)
	return setmetatable(t, fire_action_mt)
end

local create_tag_mt = {
	__index = function(t, n)
		return function(cfg) return { n, cfg } end
	end
}

--! Sets the metable of @a t so that it can be used to create subtags with less brackets.
--! @return @a t.
--! @code
--! T = helper.set_wml_tag_metatable {}
--! W.event { name = "new turn", T.message { speaker = "narrator", message = "?" } }
--! @endcode
function helper.set_wml_tag_metatable(t)
	return setmetatable(t, create_tag_mt)
end

return helper
