/* $Id$ */
/*
   Copyright (C) 2009 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file scripting/lua.cpp
 * Provides a Lua interpreter.
 *
 * @warning Lua's error handling is done by setjmp/longjmp, so be careful
 *   never to call a Lua error function that will jump while in the scope
 *   of a C++ object with a destructor. This is why this file uses goto-s
 *   to force object unscoping before erroring out. This is also why
 *   lua_getglobal, lua_setglobal, lua_gettable, lua_settable, lua_getfield,
 *   and lua_setfield, should not be called in tainted context.
 *
 * @note Naming conventions:
 *   - intf_ functions are exported in the wesnoth domain,
 *   - impl_ functions are hidden inside metatables,
 *   - cfun_ functions are closures,
 *   - luaW_ functions are helpers in Lua style.
 */

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

#include <cassert>
#include <cstring>

#include "actions.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "game_display.hpp"
#include "gamestatus.hpp"
#include "log.hpp"
#include "map.hpp"
#include "pathfind/pathfind.hpp"
#ifdef EXPERIMENTAL
#include "pathfind/teleport.hpp"
#endif
#include "play_controller.hpp"
#include "resources.hpp"
#include "scripting/lua.hpp"
#include "terrain_translation.hpp"
#include "unit.hpp"

static lg::log_domain log_scripting_lua("scripting/lua");
#define LOG_LUA LOG_STREAM(info, log_scripting_lua)
#define ERR_LUA LOG_STREAM(err, log_scripting_lua)

/**
 * Stack storing the queued_event objects needed for calling WML actions.
 */
struct queued_event_context
{
	typedef game_events::queued_event qe;
	static qe default_qe;
	static qe const *current_qe;
	static qe const &get()
	{ return *(current_qe ? current_qe : &default_qe); }
	qe const *previous_qe;

	queued_event_context(qe const *new_qe)
		: previous_qe(current_qe)
	{
		current_qe = new_qe;
	}

	~queued_event_context()
	{ current_qe = previous_qe; }
};

game_events::queued_event const *queued_event_context::current_qe = 0;
game_events::queued_event queued_event_context::default_qe
	("_from_lua", map_location(), map_location(), config());

/* Dummy pointer for getting unique keys for Lua's registry. */
static char const executeKey = 0;
static char const getsideKey = 0;
static char const gettextKey = 0;
static char const gettypeKey = 0;
static char const getunitKey = 0;
static char const tstringKey = 0;
static char const uactionKey = 0;
static char const vconfigKey = 0;
static char const wactionKey = 0;

/* Global definition so that it does not leak on longjmp. */
static std::string error_buffer;

/**
 * Displays a message in the chat window.
 */
static void chat_message(std::string const &caption, std::string const &msg)
{
	resources::screen->add_chat_message(time(NULL), caption, 0, msg,
		events::chat_handler::MESSAGE_PUBLIC, false);
}

/**
 * Pushes a config as a volatile vconfig on the top of the stack.
 */
static void luaW_pushvconfig(lua_State *L, config const &cfg)
{
	new(lua_newuserdata(L, sizeof(vconfig))) vconfig(cfg, true);
	lua_pushlightuserdata(L, (void *)&vconfigKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);
}

/**
 * Pushes a t_string on the top of the stack.
 */
static void luaW_pushtstring(lua_State *L, t_string const &v)
{
	new(lua_newuserdata(L, sizeof(t_string))) t_string(v);
	lua_pushlightuserdata(L, (void *)&tstringKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);
}

/**
 * Converts a string into a Lua object pushed at the top of the stack.
 * Boolean ("yes"/"no") and numbers are detected and typed accordingly.
 */
static void luaW_pushscalar(lua_State *L, t_string const &v)
{
	if (!v.translatable())
	{
		char *pe;
		char const *pb = v.c_str();
		double d = strtod(v.c_str(), &pe);
		if (pe != pb && *pe == '\0')
			lua_pushnumber(L, d);
		else if (v == "yes")
			lua_pushboolean(L, true);
		else if (v == "no")
			lua_pushboolean(L, false);
		else
			lua_pushstring(L, pb);
	}
	else
	{
		luaW_pushtstring(L, v);
	}
}

/**
 * Returns true if the metatable of the object is the one found in the registry.
 */
static bool luaW_hasmetatable(lua_State *L, int index, char const &key)
{
	if (!lua_getmetatable(L, index))
		return false;
	lua_pushlightuserdata(L, (void *)&key);
	lua_rawget(L, LUA_REGISTRYINDEX);
	bool ok = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);
	return ok;
}

/**
 * Converts a scalar to a translatable string.
 */
static bool luaW_totstring(lua_State *L, int index, t_string &str)
{
	switch (lua_type(L, index)) {
		case LUA_TBOOLEAN:
			str = lua_toboolean(L, index) ? "yes" : "no";
			break;
		case LUA_TNUMBER:
		case LUA_TSTRING:
			str = lua_tostring(L, index);
			break;
		case LUA_TUSERDATA:
		{
			if (!luaW_hasmetatable(L, index, tstringKey)) return false;
			str = *static_cast<t_string *>(lua_touserdata(L, index));
			break;
		}
		default:
			return false;
	}
	return true;
}

/**
 * Converts a config object to a Lua table.
 * The destination table should be at the top of the stack on entry. It is
 * still at the top on exit.
 */
static void table_of_wml_config(lua_State *L, config const &cfg)
{
	if (!lua_checkstack(L, LUA_MINSTACK))
		return;

	int k = 1;
	foreach (const config::any_child &ch, cfg.all_children_range())
	{
		lua_createtable(L, 2, 0);
		lua_pushstring(L, ch.key.c_str());
		lua_rawseti(L, -2, 1);
		lua_newtable(L);
		table_of_wml_config(L, ch.cfg);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, k++);
	}
	foreach (const config::attribute &attr, cfg.attribute_range())
	{
		luaW_pushscalar(L, attr.second);
		lua_setfield(L, -2, attr.first.c_str());
	}
}

#define return_misformed() \
  do { lua_settop(L, initial_top); return false; } while (0)

/**
 * Converts an optional table or vconfig to a config object.
 * @param tstring_meta absolute stack position of t_string's metatable, or 0 if none.
 * @return false if some attributes had not the proper type.
 * @note If the table has holes in the integer keys or floating-point keys,
 *       some keys will be ignored and the error will go undetected.
 */
static bool luaW_toconfig(lua_State *L, int index, config &cfg, int tstring_meta = 0)
{
	if (!lua_checkstack(L, LUA_MINSTACK))
		return false;

	// Get the absolute index of the table.
	int initial_top = lua_gettop(L);
	if (-initial_top <= index && index <= -1)
		index = initial_top + index + 1;

	switch (lua_type(L, index))
	{
		case LUA_TTABLE:
			break;
		case LUA_TUSERDATA:
		{
			if (!luaW_hasmetatable(L, index, vconfigKey))
				return false;
			cfg = static_cast<vconfig *>(lua_touserdata(L, index))->get_parsed_config();
			return true;
		}
		case LUA_TNONE:
		case LUA_TNIL:
			return true;
		default:
			return false;
	}

	// Get t_string's metatable, so that it can be used later to detect t_string object.
	if (!tstring_meta) {
		lua_pushlightuserdata(L, (void *)&tstringKey);
		lua_rawget(L, LUA_REGISTRYINDEX);
		tstring_meta = initial_top + 1;
	}

	// First convert the children (integer indices).
	for (int i = 1, i_end = lua_objlen(L, index); i <= i_end; ++i)
	{
		lua_rawgeti(L, index, i);
		if (!lua_istable(L, -1)) return_misformed();
		lua_rawgeti(L, -1, 1);
		char const *m = lua_tostring(L, -1);
		if (!m) return_misformed();
		lua_rawgeti(L, -2, 2);
		if (!luaW_toconfig(L, -1, cfg.add_child(m), tstring_meta))
			return_misformed();
		lua_pop(L, 3);
	}

	// Then convert the attributes (string indices).
	for (lua_pushnil(L); lua_next(L, index); lua_pop(L, 1))
	{
		if (lua_isnumber(L, -2)) continue;
		if (!lua_isstring(L, -2)) return_misformed();
		t_string v;
		switch (lua_type(L, -1)) {
			case LUA_TBOOLEAN:
				v = lua_toboolean(L, -1) ? "yes" : "no";
				break;
			case LUA_TNUMBER:
			case LUA_TSTRING:
				v = lua_tostring(L, -1);
				break;
			case LUA_TUSERDATA:
			{
				if (!lua_getmetatable(L, -1)) return_misformed();
				bool tstr = lua_rawequal(L, -1, tstring_meta) != 0;
				lua_pop(L, 1);
				if (!tstr) return_misformed();
				v = *static_cast<t_string *>(lua_touserdata(L, -1));
				break;
			}
			default:
				return_misformed();
		}
		cfg[lua_tostring(L, -2)] = v;
	}

	lua_settop(L, initial_top);
	return true;
}

#undef return_misformed

/**
 * Gets an optional vconfig from either a table or a userdata.
 * @param def true if an empty config should be created for a missing value.
 * @return false in case of failure.
 */
static bool luaW_tovconfig(lua_State *L, int index, vconfig &vcfg, bool def = true)
{
	switch (lua_type(L, index))
	{
		case LUA_TTABLE:
		{
			config cfg;
			bool ok = luaW_toconfig(L, index, cfg);
			if (!ok) return false;
			vcfg = vconfig(cfg, true);
			break;
		}
		case LUA_TUSERDATA:
			if (!luaW_hasmetatable(L, index, vconfigKey))
				return false;
			vcfg = *static_cast<vconfig *>(lua_touserdata(L, index));
			break;
		case LUA_TNONE:
		case LUA_TNIL:
			if (def)
				vcfg = vconfig(config(), true);
			break;
		default:
			return false;
	}
	return true;
}

/**
 * Calls a Lua function stored below its @a nArgs arguments at the top of the stack.
 * @return true if the call was successful and @a nRets return values are available.
 */
static bool luaW_pcall(lua_State *L
		, int nArgs, int nRets, bool allow_wml_error = false)
{
	// Load the error handler before the function and its arguments.
	lua_pushlightuserdata(L, (void *)&executeKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_insert(L, -2 - nArgs);

	// Call the function.
	int res = lua_pcall(L, nArgs, nRets, -2 - nArgs);
	if (res)
	{
		char const *m = lua_tostring(L, -1);
		if (allow_wml_error && strncmp(m, "~wml:", 5) == 0) {
			m += 5;
			char const *e = strstr(m, "stack traceback");
			lg::wml_error << std::string(m, e ? e - m : strlen(m));
		} else {
			chat_message("Lua error", m);
			ERR_LUA << m << '\n';
		}
		lua_pop(L, 2);
		return false;
	}

	// Remove the error handler.
	lua_remove(L, -1 - nRets);
	return true;
}

/**
 * Creates a t_string object (__call metamethod).
 * - Arg 1: userdata containing the domain.
 * - Arg 2: string to translate.
 * - Ret 1: string containing the translatable string.
 */
static int impl_gettext(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);
	char const *d = static_cast<char *>(lua_touserdata(L, 1));
	// Hidden metamethod, so d has to be a string. Use it to create a t_string.
	luaW_pushtstring(L, t_string(m, d));
	return 1;
}

/**
 * Creates an interface for gettext
 * - Arg 1: string containing the domain.
 * - Ret 1: a full userdata with __call pointing to lua_gettext.
 */
static int intf_textdomain(lua_State *L)
{
	size_t l;
	char const *m = luaL_checklstring(L, 1, &l);
	void *p = lua_newuserdata(L, l + 1);
	memcpy(p, m, l + 1);
	lua_pushlightuserdata(L, (void *)&gettextKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);
	return 1;
}

/**
 * Converts a Lua value at position @a src and appends it to @a dst.
 * @note This function is private to lua_tstring_concat. It expects two things.
 *       First, the t_string metatable is at the top of the stack on entry. (It
 *       is still there on exit.) Second, the caller hasn't any valuable object
 *       with dynamic lifetime, since they would be leaked on error.
 */
static void tstring_concat_aux(lua_State *L, t_string &dst, int src)
{
	switch (lua_type(L, src)) {
		case LUA_TNUMBER:
		case LUA_TSTRING:
			dst += lua_tostring(L, src);
			break;
		case LUA_TUSERDATA:
			// Compare its metatable with t_string's metatable.
			if (!lua_getmetatable(L, src) || !lua_rawequal(L, -1, -2))
				luaL_typerror(L, src, "string");
			dst += *static_cast<t_string *>(lua_touserdata(L, src));
			lua_pop(L, 1);
			break;
		default:
			luaL_typerror(L, src, "string");
	}
}

/**
 * Appends a scalar to a t_string object (__concat metamethod).
 */
static int impl_tstring_concat(lua_State *L)
{
	// Create a new t_string.
	t_string *t = new(lua_newuserdata(L, sizeof(t_string))) t_string;

	lua_pushlightuserdata(L, (void *)&tstringKey);
	lua_rawget(L, LUA_REGISTRYINDEX);

	// Append both arguments to t.
	tstring_concat_aux(L, *t, 1);
	tstring_concat_aux(L, *t, 2);

	lua_setmetatable(L, -2);
	return 1;
}

/**
 * Destroys a t_string object before it is collected (__gc metamethod).
 */
static int impl_tstring_collect(lua_State *L)
{
	t_string *t = static_cast<t_string *>(lua_touserdata(L, 1));
	t->t_string::~t_string();
	return 0;
}

/**
 * Converts a t_string object to a string (__tostring metamethod);
 * that is, performs a translation.
 */
static int impl_tstring_tostring(lua_State *L)
{
	t_string *t = static_cast<t_string *>(lua_touserdata(L, 1));
	lua_pushstring(L, t->c_str());
	return 1;
}

/**
 * Gets the parsed field of a vconfig object (_index metamethod).
 * Special fields __literal and __parsed return Lua tables.
 */
static int impl_vconfig_get(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));

	if (lua_isnumber(L, 2))
	{
		vconfig::all_children_iterator i = v->ordered_begin();
		unsigned len = std::distance(i, v->ordered_end());
		unsigned pos = lua_tointeger(L, 2) - 1;
		if (pos >= len) return 0;
		std::advance(i, pos);
		lua_createtable(L, 2, 0);
		lua_pushstring(L, i.get_key().c_str());
		lua_rawseti(L, -2, 1);
		new(lua_newuserdata(L, sizeof(vconfig))) vconfig(i.get_child());
		lua_pushlightuserdata(L, (void *)&vconfigKey);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_setmetatable(L, -2);
		lua_rawseti(L, -2, 2);
		return 1;
	}

	char const *m = luaL_checkstring(L, 2);
	if (strcmp(m, "__literal") == 0) {
		lua_newtable(L);
		table_of_wml_config(L, v->get_config());
		return 1;
	}
	if (strcmp(m, "__parsed") == 0) {
		lua_newtable(L);
		table_of_wml_config(L, v->get_parsed_config());
		return 1;
	}

	if (v->null() || !v->has_attribute(m)) return 0;
	luaW_pushscalar(L, (*v)[m]);
	return 1;
}

/**
 * Returns the number of a child of a vconfig object.
 */
static int impl_vconfig_size(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));
	lua_pushinteger(L, v->null() ? 0 :
		std::distance(v->ordered_begin(), v->ordered_end()));
	return 1;
}

/**
 * Destroys a vconfig object before it is collected (__gc metamethod).
 */
static int impl_vconfig_collect(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));
	v->vconfig::~vconfig();
	return 0;
}

#define return_tstring_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		luaW_pushtstring(L, accessor); \
		return 1; \
	}

#define return_string_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		lua_pushstring(L, accessor.c_str()); \
		return 1; \
	}

#define return_int_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		lua_pushinteger(L, accessor); \
		return 1; \
	}

#define return_bool_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		lua_pushboolean(L, accessor); \
		return 1; \
	}

#define return_cfg_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		config cfg; \
		accessor; \
		lua_newtable(L); \
		table_of_wml_config(L, cfg); \
		return 1; \
	}

#define return_cfgref_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		lua_newtable(L); \
		table_of_wml_config(L, accessor); \
		return 1; \
	}

#define modify_tstring_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		if (lua_type(L, -1) == LUA_TUSERDATA) { \
			lua_pushlightuserdata(L, (void *)&tstringKey); \
			lua_rawget(L, LUA_REGISTRYINDEX); \
			if (!lua_getmetatable(L, -2) || !lua_rawequal(L, -1, -2)) \
				return luaL_typerror(L, -3, "(translatable) string"); \
			const t_string &value = *static_cast<t_string *>(lua_touserdata(L, -3)); \
			accessor; \
		} else { \
			const char *value = luaL_checkstring(L, -1); \
			accessor; \
		} \
		return 0; \
	}

#define modify_string_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		const char *value = luaL_checkstring(L, -1); \
		accessor; \
		return 0; \
	}

#define modify_int_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		int value = luaL_checkint(L, -1); \
		accessor; \
		return 0; \
	}

#define modify_bool_attrib(name, accessor) \
	if (strcmp(m, name) == 0) { \
		int value = lua_toboolean(L, -1); \
		accessor; \
		return 0; \
	}

/**
 * Gets some data on a unit type (__index metamethod).
 * - Arg 1: table containing an "id" field.
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_unit_type_get(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);
	lua_pushstring(L, "id");
	lua_rawget(L, 1);
	const unit_type *utp = unit_types.find(lua_tostring(L, -1));
	if (!utp) return luaL_argerror(L, 1, "unknown unit type");
	unit_type const &ut = *utp;

	// Find the corresponding attribute.
	return_tstring_attrib("name", ut.type_name());
	return_int_attrib("max_hitpoints", ut.hitpoints());
	return_int_attrib("max_moves", ut.movement());
	return_int_attrib("max_experience", ut.experience_needed());
	return_int_attrib("cost", ut.cost());
	return_int_attrib("level", ut.level());
	return_cfgref_attrib("__cfg", ut.get_cfg());
	return 0;
}

/**
 * Gets the unit type corresponding to an id.
 * - Arg 1: string containing the unit type id.
 * - Ret 1: table with an "id" field and with __index pointing to lua_unit_type_get.
 */
static int intf_get_unit_type(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	if (!unit_types.unit_type_exists(m)) return 0;

	lua_createtable(L, 0, 1);
	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "id");
	lua_pushlightuserdata(L, (void *)&gettypeKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);
	return 1;
}

/**
 * Gets the ids of all the unit types.
 * - Ret 1: table containing the ids.
 */
static int intf_get_unit_type_ids(lua_State *L)
{
	lua_newtable(L);
	int i = 1;
	foreach (const unit_type_data::unit_type_map::value_type &ut, unit_types.types())
	{
		lua_pushstring(L, ut.first.c_str());
		lua_rawseti(L, -2, i);
		++i;
	}
	return 1;
}

/**
 * Gets some data on a unit (__index metamethod).
 * - Arg 1: full userdata containing the unit id.
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_unit_get(lua_State *L)
{
	if (false) {
		error_call_destructors:
		return luaL_argerror(L, 1, "unknown unit");
	}

	size_t id = *static_cast<size_t *>(lua_touserdata(L, 1));
	char const *m = luaL_checkstring(L, 2);

	unit_map::const_unit_iterator ui = resources::units->find(id);
	if (!ui.valid()) goto error_call_destructors;
	unit const &u = ui->second;

	// Find the corresponding attribute.
	return_int_attrib("x", ui->first.x + 1);
	return_int_attrib("y", ui->first.y + 1);
	return_int_attrib("side", u.side());
	return_string_attrib("id", u.id());
	return_int_attrib("hitpoints", u.hitpoints());
	return_int_attrib("max_hitpoints", u.max_hitpoints());
	return_int_attrib("experience", u.experience());
	return_int_attrib("max_experience", u.max_experience());
	return_int_attrib("moves", u.movement_left());
	return_int_attrib("max_moves", u.total_movement());
	return_tstring_attrib("name", u.name());
	return_string_attrib("side_id", u.side_id());
	return_bool_attrib("canrecruit", u.can_recruit());
	return_bool_attrib("petrified", u.incapacitated());
	return_bool_attrib("resting", u.resting());
	return_string_attrib("role", u.get_role());
	return_string_attrib("facing", map_location::write_direction(u.facing()));
	return_cfg_attrib("__cfg", u.write(cfg); ui->first.write(cfg));
	return 0;
}

/**
 * Sets some data on a unit (__newindex metamethod).
 * - Arg 1: full userdata containing the unit id.
 * - Arg 2: string containing the name of the property.
 * - Arg 3: something containing the attribute.
 */
static int impl_unit_set(lua_State *L)
{
	if (false) {
		error_call_destructors_1:
		return luaL_argerror(L, 1, "unknown unit");
		error_call_destructors_2:
		return luaL_argerror(L, 2, "unknown modifiable property");
	}

	size_t id = *static_cast<size_t *>(lua_touserdata(L, 1));
	char const *m = luaL_checkstring(L, 2);
	lua_settop(L, 3);

	unit_map::unit_iterator ui = resources::units->find(id);
	if (!ui.valid()) goto error_call_destructors_1;
	unit &u = ui->second;

	// Find the corresponding attribute.
	modify_int_attrib("side", u.set_side(value));
	modify_int_attrib("moves", u.set_movement(value));
	modify_bool_attrib("resting", u.set_resting(value != 0));
	modify_tstring_attrib("name", u.set_name(value));
	modify_string_attrib("role", u.set_role(value));
	modify_string_attrib("facing", u.set_facing(map_location::parse_direction(value)));
	goto error_call_destructors_2;
}

/**
 * Gets the numeric ids of all the units.
 * - Arg 1: optional table containing a filter
 * - Ret 1: table containing full userdata with __index pointing to
 *          lua_unit_get and __newindex pointing to lua_unit_set.
 */
static int intf_get_units(lua_State *L)
{
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, 1, "WML table");
	}

	vconfig filter;
	if (!luaW_tovconfig(L, 1, filter, false))
		goto error_call_destructors;

	// Go through all the units while keeping the following stack:
	// 1: metatable, 2: return table, 3: userdata, 4: metatable copy
	lua_settop(L, 0);
	lua_pushlightuserdata(L, (void *)&getunitKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_newtable(L);
	int i = 1;
	unit_map &units = *resources::units;
	for (unit_map::const_unit_iterator ui = units.begin(), ui_end = units.end();
	     ui != ui_end; ++ui)
	{
		if (!filter.null() && !ui->second.matches_filter(filter, ui->first))
			continue;
		size_t *p = static_cast<size_t *>(lua_newuserdata(L, sizeof(size_t)));
		*p = ui->second.underlying_id();
		lua_pushvalue(L, 1);
		lua_setmetatable(L, 3);
		lua_rawseti(L, 2, i);
		++i;
	}
	return 1;
}

/**
 * Fires a WML event handler.
 * - Arg 1: string containing the handler name.
 * - Arg 2: optional WML config.
 */
static int intf_fire(lua_State *L)
{
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, 2, "WML table");
	}

	char const *m = luaL_checkstring(L, 1);

	vconfig vcfg;
	if (!luaW_tovconfig(L, 2, vcfg))
		goto error_call_destructors;

	game_events::handle_event_command(m, queued_event_context::get(), vcfg);
	return 0;
}

/**
 * Gets a WML variable.
 * - Arg 1: string containing the variable name.
 * - Arg 2: optional bool indicating if tables for containers should be left empty.
 * - Ret 1: value of the variable, if any.
 */
static int intf_get_variable(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	variable_info v(m, false, variable_info::TYPE_SCALAR);
	if (v.is_valid) {
		luaW_pushscalar(L, v.as_scalar());
		return 1;
	} else {
		variable_info w(m, false, variable_info::TYPE_CONTAINER);
		if (w.is_valid) {
			lua_newtable(L);
			if (lua_toboolean(L, 2))
				table_of_wml_config(L, w.as_container());
			return 1;
		}
	}
	return 0;
}

/**
 * Sets a WML variable.
 * - Arg 1: string containing the variable name.
 * - Arg 2: bool/int/string/table containing the value.
 */
static int intf_set_variable(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, 2, "WML table or scalar");
	}

	if (lua_isnoneornil(L, 2)) {
		resources::state_of_game->clear_variable(m);
		return 0;
	}

	variable_info v(m);
	switch (lua_type(L, 2)) {
		case LUA_TBOOLEAN:
			v.as_scalar() = lua_toboolean(L, 2) ? "yes" : "no";
			break;
		case LUA_TNUMBER:
		case LUA_TSTRING:
			v.as_scalar() = lua_tostring(L, 2);
			break;
		case LUA_TUSERDATA:
			if (luaW_hasmetatable(L, 2, tstringKey)) {
				v.as_scalar() = *static_cast<t_string *>(lua_touserdata(L, 2));
				break;
			}
			// no break
		case LUA_TTABLE:
		{
			config &cfg = v.as_container();
			cfg.clear();
			if (!luaW_toconfig(L, 2, cfg))
				goto error_call_destructors;
			break;
		}
		default:
			goto error_call_destructors;
	}
	return 0;
}

/**
 * Loads and executes a Lua file.
 * - Arg 1: string containing the file name.
 * - Ret *: values returned by executing the file body.
 */
static int intf_dofile(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	if (false) {
		error_call_destructors_1:
		return luaL_argerror(L, 1, "file not found");
		error_call_destructors_2:
		return lua_error(L);
		continue_call_destructor:
		lua_call(L, 0, LUA_MULTRET);
		return lua_gettop(L);
	}
	std::string p = get_wml_location(m);
	if (p.empty())
		goto error_call_destructors_1;

	lua_settop(L, 0);
	if (luaL_loadfile(L, p.c_str()))
		goto error_call_destructors_2;

	goto continue_call_destructor;
}

/**
 * Loads and executes a Lua file, if there is no corresponding entry in wesnoth.package.
 * Stores the result of the script in wesnoth.package and returns it.
 * - Arg 1: string containing the file name.
 * - Ret 1: value returned by the script.
 */
static int intf_require(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	if (false) {
		error_call_destructors_1:
		return luaL_argerror(L, 1, "file not found");
	}

	// Check if there is already an entry.
	lua_pushstring(L, "wesnoth");
	lua_rawget(L, LUA_GLOBALSINDEX);
	lua_pushstring(L, "package");
	lua_rawget(L, -2);
	lua_pushvalue(L, 1);
	lua_rawget(L, -2);
	if (!lua_isnil(L, -1)) return 1;
	lua_pop(L, 1);

	std::string p = get_wml_location(m);
	if (p.empty())
		goto error_call_destructors_1;

	// Compile the file.
	int res = luaL_loadfile(L, p.c_str());
	if (res)
	{
		char const *m = lua_tostring(L, -1);
		chat_message("Lua error", m);
		ERR_LUA << m << '\n';
		return 0;
	}

	// Execute it.
	if (!luaW_pcall(L, 0, 1)) return 0;

	// Add the return value to the table.
	lua_pushvalue(L, 1);
	lua_pushvalue(L, -2);
	lua_settable(L, -4);
	return 1;
}

/**
 * Calls a WML action handler (__call metamethod).
 * - Arg 1: userdata containing the handler.
 * - Arg 2: optional WML config.
 */
static int impl_wml_action_call(lua_State *L)
{
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, 2, "WML table");
	}

	vconfig vcfg;
	if (!luaW_tovconfig(L, 2, vcfg))
		goto error_call_destructors;

	game_events::action_handler **h =
		static_cast<game_events::action_handler **>(lua_touserdata(L, 1));
	// Hidden metamethod, so h has to be an action handler.
	(*h)->handle(queued_event_context::get(), vcfg);
	return 0;
}

/**
 * Destroys a WML action handler before its pointer is collected (__gc metamethod).
 * - Arg 1: userdata containing the handler.
 */
static int impl_wml_action_collect(lua_State *L)
{
	game_events::action_handler **h =
		static_cast<game_events::action_handler **>(lua_touserdata(L, 1));
	// Hidden metamethod, so h has to be an action handler.
	delete *h;
	return 0;
}

/**
 * Calls the first upvalue and passes the first argument.
 * - Arg 1: optional WML config.
 */
static int cfun_wml_action_proxy(lua_State *L)
{
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, 1, "WML table");
	}

	lua_pushvalue(L, lua_upvalueindex(1));

	switch (lua_type(L, 1))
	{
		case LUA_TTABLE:
		{
			config cfg;
			if (!luaW_toconfig(L, 1, cfg))
				goto error_call_destructors;
			luaW_pushvconfig(L, cfg);
			break;
		}
		case LUA_TUSERDATA:
		{
			if (!luaW_hasmetatable(L, 1, vconfigKey))
				goto error_call_destructors;
			lua_pushvalue(L, 1);
			break;
		}
		case LUA_TNONE:
		case LUA_TNIL:
			lua_createtable(L, 0, 0);
			break;
		default:
			goto error_call_destructors;
	}

	lua_call(L, 1, 0);
	return 0;
}

/**
 * Proxy class for calling WML action handlers defined in Lua.
 */
struct lua_action_handler : game_events::action_handler
{
	lua_State *L;
	int num;

	lua_action_handler(lua_State *l, int n) : L(l), num(n) {}
	void handle(const game_events::queued_event &, const vconfig &);
	~lua_action_handler();
};

void lua_action_handler::handle(const game_events::queued_event &ev, const vconfig &cfg)
{
	// Load the user function from the registry.
	lua_pushlightuserdata(L, (void *)&uactionKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_rawgeti(L, -1, num);
	lua_remove(L, -2);

	// Push the WML table argument.
	new(lua_newuserdata(L, sizeof(vconfig))) vconfig(cfg);
	lua_pushlightuserdata(L, (void *)&vconfigKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);

	queued_event_context dummy(&ev);
	luaW_pcall(L, 1, 0, true);
}

lua_action_handler::~lua_action_handler()
{
	// Remove the function from the registry, so that it can be collected.
	lua_pushlightuserdata(L, (void *)&uactionKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);
	lua_rawseti(L, -2, num);
	lua_pop(L, 1);
}

/**
 * Registers a function as WML action handler.
 * - Arg 1: string containing the WML tag.
 * - Arg 2: optional function taking a WML table as argument.
 * - Ret 1: previous action handler, if any.
 */
static int intf_register_wml_action(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	bool enable = !lua_isnoneornil(L, 2);

	// Retrieve the user action table from the registry.
	lua_pushlightuserdata(L, (void *)&uactionKey);
	lua_rawget(L, LUA_REGISTRYINDEX);

	lua_action_handler *h = NULL;
	if (enable)
	{
		// Push the function in the table so that it is not collected.
		size_t length = lua_objlen(L, -1);
		lua_pushvalue(L, 2);
		lua_rawseti(L, -2, length + 1);

		// Create the proxy C++ action handler.
		h = new lua_action_handler(L, length + 1);
	}

	// Register the new handler and retrieve the previous one.
	game_events::action_handler *previous;
	game_events::register_action_handler(m, h, &previous);
	if (!previous) return 0;

	// Detect if the previous handler was already from Lua.
	lua_action_handler *lua_prev = dynamic_cast<lua_action_handler *>(previous);
	if (lua_prev)
	{
		// Extract the function from the table,
		// and put a lightweight wrapper around it.
		lua_rawgeti(L, -1, lua_prev->num);
		lua_pushcclosure(L, cfun_wml_action_proxy, 1);

		// Delete the old heavyweight wraper.
		delete lua_prev;
		return 1;
	}

	// Wrap and return the previous handler.
	void *p = lua_newuserdata(L, sizeof(game_events::action_handler *));
	*static_cast<game_events::action_handler **>(p) = previous;
	lua_pushlightuserdata(L, (void *)&wactionKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);
	return 1;
}

/**
 * Gets some data on a side (__index metamethod).
 * - Arg 1: full userdata containing the team.
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_side_get(lua_State *L)
{
	// Hidden metamethod, so arg1 has to be a pointer to a team.
	team &t = **static_cast<team **>(lua_touserdata(L, 1));
	char const *m = luaL_checkstring(L, 2);

	// Find the corresponding attribute.
	return_int_attrib("gold", t.gold());
	return_tstring_attrib("objectives", t.objectives());
	return_int_attrib("village_gold", t.village_gold());
	return_int_attrib("base_income", t.base_income());
	return_int_attrib("total_income", t.total_income());
	return_bool_attrib("objectives_changed", t.objectives_changed());
	return_tstring_attrib("user_team_name", t.user_team_name());
	return_string_attrib("team_name", t.team_name());

	if (strcmp(m, "recruit") == 0) {
		std::set<std::string> const &recruits = t.recruits();
		lua_createtable(L, recruits.size(), 0);
		int i = 1;
		foreach (std::string const &r, t.recruits()) {
			lua_pushstring(L, r.c_str());
			lua_rawseti(L, -2, i++);
		}
		return 1;
	}

	return_cfg_attrib("__cfg", t.write(cfg));
	return 0;
}

/**
 * Sets some data on a side (__newindex metamethod).
 * - Arg 1: full userdata containing the team.
 * - Arg 2: string containing the name of the property.
 * - Arg 3: something containing the attribute.
 */
static int impl_side_set(lua_State *L)
{
	// Hidden metamethod, so arg1 has to be a pointer to a team.
	team &t = **static_cast<team **>(lua_touserdata(L, 1));
	char const *m = luaL_checkstring(L, 2);
	lua_settop(L, 3);

	// Find the corresponding attribute.
	modify_int_attrib("gold", t.set_gold(value));
	modify_tstring_attrib("objectives", t.set_objectives(value, true));
	modify_int_attrib("village_gold", t.set_village_gold(value));
	modify_int_attrib("base_income", t.set_base_income(value));
	modify_bool_attrib("objectives_changed", t.set_objectives_changed(value != 0));
	modify_tstring_attrib("user_team_name", t.change_team(t.team_name(), value));
	modify_string_attrib("team_name", t.change_team(value, t.user_team_name()));

	if (strcmp(m, "recruit") == 0) {
		t.set_recruits(std::set<std::string>());
		if (!lua_istable(L, 3)) return 0;
		for (int i = 1;; ++i) {
			lua_rawgeti(L, 3, i);
			if (lua_isnil(L, -1)) break;
			t.add_recruit(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
		return 0;
	}

	return luaL_argerror(L, 2, "unknown modifiable property");
}

/**
 * Gets a proxy userdata for a side.
 * - Arg 1: integer for the side.
 * - Ret 1: full userdata with __index pointing to lua_side_get
 *          and __newindex pointing to lua_side_set.
 */
static int intf_get_side(lua_State *L)
{
	int s = luaL_checkint(L, 1);

	size_t t = s - 1;
	std::vector<team> &teams = *resources::teams;
	if (t >= teams.size()) return 0;

	// Create a full userdata containing a pointer to the team.
	team **p = static_cast<team **>(lua_newuserdata(L, sizeof(team *)));
	*p = &teams[t];

	// Get the metatable from the registry and set it.
	lua_pushlightuserdata(L, (void *)&getsideKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, 2);

	return 1;
}

/**
 * Gets a terrain code.
 * - Args 1,2: map location.
 * - Ret 1: string.
 */
static int intf_get_terrain(lua_State *L)
{
	int x = luaL_checkint(L, 1);
	int y = luaL_checkint(L, 2);

	t_translation::t_terrain const &t = resources::game_map->
		get_terrain(map_location(x - 1, y - 1));
	lua_pushstring(L, t_translation::write_terrain_code(t).c_str());
	return 1;
}

/**
 * Sets a terrain code.
 * - Args 1,2: map location.
 * - Arg 3: terrain code string.
 */
static int intf_set_terrain(lua_State *L)
{
	int x = luaL_checkint(L, 1);
	int y = luaL_checkint(L, 2);
	char const *m = luaL_checkstring(L, 3);

	t_translation::t_terrain t = t_translation::read_terrain_code(m);
	if (t != t_translation::NONE_TERRAIN)
		change_terrain(map_location(x - 1, y - 1), t, gamemap::BOTH, false);
	return 0;
}

/**
 * Gets details about a terrain.
 * - Arg 1: terrain code string.
 * - Ret 1: table.
 */
static int intf_get_terrain_info(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	t_translation::t_terrain t = t_translation::read_terrain_code(m);
	if (t == t_translation::NONE_TERRAIN) return 0;
	terrain_type const &info = resources::game_map->get_terrain_info(t);

	lua_newtable(L);
	lua_pushstring(L, info.id().c_str());
	lua_setfield(L, -2, "id");
	luaW_pushtstring(L, info.name());
	lua_setfield(L, -2, "name");
	luaW_pushtstring(L, info.description());
	lua_setfield(L, -2, "description");
	lua_pushboolean(L, info.is_village());
	lua_setfield(L, -2, "village");
	lua_pushboolean(L, info.is_castle());
	lua_setfield(L, -2, "castle");
	lua_pushboolean(L, info.is_keep());
	lua_setfield(L, -2, "keep");
	lua_pushinteger(L, info.gives_healing());
	lua_setfield(L, -2, "healing");

	return 1;
}

/**
 * Gets the side of a village owner.
 * - Args 1,2: map location.
 * - Ret 1: integer.
 */
static int intf_get_village_owner(lua_State *L)
{
	int x = luaL_checkint(L, 1);
	int y = luaL_checkint(L, 2);

	map_location loc(x - 1, y - 1);
	if (!resources::game_map->is_village(loc))
		return 0;

	int side = village_owner(loc, *resources::teams) + 1;
	if (!side) return 0;
	lua_pushinteger(L, side);
	return 1;
}

/**
 * Sets the owner of a village.
 * - Args 1,2: map location.
 * - Arg 3: integer for the side or empty to remove ownership.
 */
static int intf_set_village_owner(lua_State *L)
{
	int x = luaL_checkint(L, 1);
	int y = luaL_checkint(L, 2);
	int new_side = lua_isnoneornil(L, 3) ? 0 : luaL_checkint(L, 3);

	std::vector<team> &teams = *resources::teams;
	map_location loc(x - 1, y - 1);
	if (!resources::game_map->is_village(loc))
		return 0;

	int old_side = village_owner(loc, teams) + 1;
	if (new_side == old_side || new_side < 0 || new_side > (int)teams.size())
		return 0;

	if (old_side) teams[old_side - 1].lose_village(loc);
	if (new_side) teams[new_side - 1].get_village(loc);
	return 0;
}

/**
 * Returns the map size.
 * - Ret 1: width.
 * - Ret 2: height.
 * - Ret 3: border size.
 */
static int intf_get_map_size(lua_State *L)
{
	const gamemap &map = *resources::game_map;
	lua_pushinteger(L, map.w());
	lua_pushinteger(L, map.h());
	lua_pushinteger(L, map.border_size());
	return 3;
}

/**
 * Returns the currently selected tile.
 * - Ret 1: x.
 * - Ret 2: y.
 */
static int intf_get_selected_tile(lua_State *L)
{
	const map_location &loc = resources::screen->selected_hex();
	if (!resources::game_map->on_board(loc)) return 0;
	lua_pushinteger(L, loc.x + 1);
	lua_pushinteger(L, loc.y + 1);
	return 2;
}

/**
 * Gets some game_config data (__index metamethod).
 * - Arg 1: userdata (ignored).
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_game_config_get(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);

	// Find the corresponding attribute.
	return_int_attrib("base_income", game_config::base_income);
	return_int_attrib("village_income", game_config::village_income);
	return_int_attrib("poison_amount", game_config::poison_amount);
	return_int_attrib("rest_heal_amount", game_config::rest_heal_amount);
	return_int_attrib("recall_cost", game_config::recall_cost);
	return_int_attrib("kill_experience", game_config::kill_experience);
	return_string_attrib("version", game_config::version);
	return 0;
}

/**
 * Sets some game_config data (__newindex metamethod).
 * - Arg 1: userdata (ignored).
 * - Arg 2: string containing the name of the property.
 * - Arg 3: something containing the attribute.
 */
static int impl_game_config_set(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);
	lua_settop(L, 3);

	// Find the corresponding attribute.
	modify_int_attrib("base_income", game_config::base_income = value);
	modify_int_attrib("village_income", game_config::village_income = value);
	modify_int_attrib("poison_amount", game_config::poison_amount = value);
	modify_int_attrib("rest_heal_amount", game_config::rest_heal_amount = value);
	modify_int_attrib("recall_cost", game_config::recall_cost = value);
	modify_int_attrib("kill_experience", game_config::kill_experience = value);
	return luaL_argerror(L, 2, "unknown modifiable property");
}

/**
 * Gets some data about current point of game (__index metamethod).
 * - Arg 1: userdata (ignored).
 * - Arg 2: string containing the name of the property.
 * - Ret 1: something containing the attribute.
 */
static int impl_current_get(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);

	// Find the corresponding attribute.
	return_int_attrib("side", resources::controller->current_side());
	return_int_attrib("turn", resources::controller->turn());
	return 0;
}

/**
 * Displays a message in the chat window and in the logs.
 * - Arg 1: optional message header.
 * - Arg 2 (or 1): message.
 */
static int intf_message(lua_State *L)
{
	char const *m = luaL_checkstring(L, 1);
	char const *h = m;
	if (lua_isnone(L, 2)) {
		h = "Lua";
	} else {
		m = luaL_checkstring(L, 2);
	}
	chat_message(h, m);
	LOG_LUA << "Script says: \"" << m << "\"\n";
	return 0;
}

/**
 * Evaluates a boolean WML conditional.
 * - Arg 1: WML table.
 * - Ret 1: boolean.
 */
static int intf_eval_conditional(lua_State *L)
{
	if (lua_isnoneornil(L, 1)) {
		error_call_destructors:
		return luaL_typerror(L, 1, "WML table");
	}

	config cond;
	if (!luaW_toconfig(L, 1, cond))
		goto error_call_destructors;

	bool b = game_events::conditional_passed(resources::units, vconfig(cond));
	lua_pushboolean(L, b);
	return 1;
}

/**
 * Cost function object relying on a Lua function.
 * @note The stack index of the Lua function must be valid each time the cost is computed.
 */
struct lua_calculator : pathfind::cost_calculator
{
	lua_State *L;
	int index;

	lua_calculator(lua_State *L_, int i): L(L_), index(i) {}
	double cost(const map_location &loc, double so_far) const;
};

double lua_calculator::cost(const map_location &loc, double so_far) const
{
	// Copy the user function and push the location and current cost.
	lua_pushvalue(L, index);
	lua_pushinteger(L, loc.x + 1);
	lua_pushinteger(L, loc.y + 1);
	lua_pushnumber(L, so_far);

	// Execute the user function.
	if (!luaW_pcall(L, 3, 1)) return 1.;

	// Return a cost of at least 1 mp to avoid issues in pathfinder.
	// (Condition is inverted to detect NaNs.)
	double cost = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return !(cost >= 1.) ? 1. : cost;
}

/**
 * Finds a path between two locations.
 * - Args 1,2: source location. (Or Arg 1: unit.)
 * - Args 3,4: destination.
 * - Arg 5: optional cost function or
 *          table (optional fields: ignore_units, ignore_teleport, max_cost, viewing_side).
 * - Ret 1: array of pairs containing path steps.
 * - Ret 2: path cost.
 */
static int intf_find_path(lua_State *L)
{
	int arg = 1;
	if (false) {
		error_call_destructors:
		return luaL_argerror(L, arg, "???");
	}

	map_location src, dst;
	unit_map &units = *resources::units;
	const unit *u = NULL;

	if (lua_isuserdata(L, arg))
	{
		if (!luaW_hasmetatable(L, 1, getunitKey))
			goto error_call_destructors;
		size_t id = *static_cast<size_t *>(lua_touserdata(L, 1));
		unit_map::const_unit_iterator ui = units.find(id);
		if (!ui.valid())
			goto error_call_destructors;
		u = &ui->second;
		src = u->get_location();
		++arg;
	}
	else
	{
		if (!lua_isnumber(L, arg))
			goto error_call_destructors;
		src.x = lua_tointeger(L, arg) - 1;
		++arg;
		if (!lua_isnumber(L, arg))
			goto error_call_destructors;
		src.y = lua_tointeger(L, arg) - 1;
		unit_map::const_unit_iterator ui = units.find(src);
		if (!ui.valid())
			goto error_call_destructors;
		u = &ui->second;
		++arg;
	}

	if (!lua_isnumber(L, arg))
		goto error_call_destructors;
	dst.x = lua_tointeger(L, arg) - 1;
	++arg;
	if (!lua_isnumber(L, arg))
		goto error_call_destructors;
	dst.y = lua_tointeger(L, arg) - 1;
	++arg;

	std::vector<team> &teams = *resources::teams;
	gamemap &map = *resources::game_map;
	int viewing_side = u->side();
	bool ignore_units = false, see_all = false, ignore_teleport = false;
	double stop_at = 10000;
	pathfind::cost_calculator *calc = NULL;

	if (lua_istable(L, arg))
	{
		lua_pushstring(L, "ignore_units");
		lua_rawget(L, arg);
		ignore_units = lua_toboolean(L, -1);
		lua_pop(L, 1);

		lua_pushstring(L, "ignore_teleport");
		lua_rawget(L, arg);
		ignore_teleport = lua_toboolean(L, -1);
		lua_pop(L, 1);

		lua_pushstring(L, "max_cost");
		lua_rawget(L, arg);
		if (!lua_isnil(L, -1))
			stop_at = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushstring(L, "viewing_side");
		lua_rawget(L, arg);
		if (!lua_isnil(L, -1)) {
			int i = lua_tointeger(L, -1);
			if (i >= 1 && i <= int(teams.size())) viewing_side = i;
			else see_all = true;
		}
		lua_pop(L, 1);
	}
	else if (lua_isfunction(L, arg))
	{
		calc = new lua_calculator(L, arg);
	}

	team &viewing_team = teams[viewing_side - 1];
#ifndef EXPERIMENTAL
	std::set<map_location> teleport_locations;

	if (!ignore_teleport) {
	  teleport_locations = pathfind::get_teleport_locations(
			*u, units, viewing_team, see_all, ignore_units);
	}
#else
	const pathfind::teleport_map teleport_locations = !ignore_teleport ? pathfind::get_teleport_locations(
			*u, units, viewing_team, see_all, ignore_units) : pathfind::teleport_map();
#endif

	if (!calc) {
		calc = new pathfind::shortest_path_calculator(*u, viewing_team,
			units, teams, map, ignore_units, false, see_all);
	}

	pathfind::plain_route res = pathfind::a_star_search(src, dst, stop_at, calc, map.w(), map.h(),
		&teleport_locations);
	delete calc;

	int nb = res.steps.size();
	lua_createtable(L, nb, 0);
	for (int i = 0; i < nb; ++i)
	{
		lua_createtable(L, 2, 0);
		lua_pushinteger(L, res.steps[i].x + 1);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, res.steps[i].y + 1);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, i + 1);
	}
	lua_pushinteger(L, res.move_cost);

	return 2;
}

/**
 * Places a unit on the map.
 * - Args 1,2: (optional) location.
 * - Arg 3: WML table describing a unit, or nothing/nil to delete.
 */
static int intf_put_unit(lua_State *L)
{
	int unit_arg = 1;
	if (false) {
		error_call_destructors_1:
		return luaL_typerror(L, unit_arg, "WML table");
		error_call_destructors_2:
		return luaL_argerror(L, unit_arg, error_buffer.c_str());
		error_call_destructors_3:
		return luaL_argerror(L, 1, "invalid location");
	}

	unit *u = NULL;
	int x = 0, y = 0;
	if (lua_isnumber(L, 1)) {
		unit_arg = 3;
		x = lua_tointeger(L, 1);
		y = lua_tointeger(L, 2);
	}

	if (!lua_isnoneornil(L, unit_arg)) {
		config cfg;
		if (!luaW_toconfig(L, unit_arg, cfg))
			goto error_call_destructors_1;
		if (unit_arg == 1) {
			x = lexical_cast_default(cfg["x"], 0);
			y = lexical_cast_default(cfg["y"], 0);
			cfg.remove_attribute("x");
			cfg.remove_attribute("y");
		}
		try {
			u = new unit(resources::units, cfg, true, resources::state_of_game);
		} catch (const game::error &e) {
			error_buffer = "broken unit WML [" + e.message + "]";
			goto error_call_destructors_2;
		}
	}

	map_location loc(x - 1, y - 1);
	if (!resources::game_map->on_board(loc)) {
		delete u;
		goto error_call_destructors_3;
	}

	resources::units->erase(loc);
	if (u) resources::units->add(loc, *u);
	delete u;

	return 0;
}

/**
 * Finds a vacant tile.
 * - Args 1,2: location.
 * - Arg 3: optional unit for checking movement type.
 */
static int intf_find_vacant_tile(lua_State *L)
{
	int x = lua_tointeger(L, 1), y = lua_tointeger(L, 2);

	const unit *u = NULL;
	if (luaW_hasmetatable(L, 3, getunitKey)) {
		size_t id = *static_cast<size_t *>(lua_touserdata(L, 3));
		unit_map::const_unit_iterator ui = resources::units->find(id);
		if (ui.valid()) u = &ui->second;
	}

	map_location res = find_vacant_tile(*resources::game_map,
					    *resources::units, map_location(x -1, y - 1), pathfind::VACANT_ANY, u);
	if (!res.valid()) return 0;

	lua_pushinteger(L, res.x + 1);
	lua_pushinteger(L, res.y + 1);
	return 2;
}

/**
 * Floats some text on the map.
 * - Args 1,2: location.
 * - Arg 3: string.
 */
static int intf_float_label(lua_State *L)
{
	if (false) {
		error_call_destructors_1:
		return luaL_argerror(L, 3, "invalid string");
	}

	map_location loc;
	loc.x = lua_tointeger(L, 1) - 1;
	loc.y = lua_tointeger(L, 2) - 1;

	t_string text;
	if (!luaW_totstring(L, 3, text)) goto error_call_destructors_1;
	resources::screen->float_label(loc, text, font::LABEL_COLOUR.r,
		font::LABEL_COLOUR.g, font::LABEL_COLOUR.b);
	return 0;
}

LuaKernel::LuaKernel()
	: mState(luaL_newstate())
{
	lua_State *L = mState;

	// Open safe libraries. (Debug is not, but it will be closed below.)
	static const luaL_Reg safe_libs[] = {
		{ "",       luaopen_base   },
		{ "table",  luaopen_table  },
		{ "string", luaopen_string },
		{ "math",   luaopen_math   },
		{ "debug",  luaopen_debug  },
		{ NULL, NULL }
	};
	for (luaL_Reg const *lib = safe_libs; lib->func; ++lib)
	{
		lua_pushcfunction(L, lib->func);
		lua_pushstring(L, lib->name);
		lua_call(L, 1, 0);
	}

	// Put some callback functions in the scripting environment.
	static luaL_reg const callbacks[] = {
		{ "dofile",                   &intf_dofile                   },
		{ "fire",                     &intf_fire                     },
		{ "eval_conditional",         &intf_eval_conditional         },
		{ "find_path",                &intf_find_path                },
		{ "find_vacant_tile",         &intf_find_vacant_tile         },
		{ "float_label",              &intf_float_label              },
		{ "get_map_size",             &intf_get_map_size             },
		{ "get_selected_tile",        &intf_get_selected_tile        },
		{ "get_side",                 &intf_get_side                 },
		{ "get_terrain",              &intf_get_terrain              },
		{ "get_terrain_info",         &intf_get_terrain_info         },
		{ "get_unit_type",            &intf_get_unit_type            },
		{ "get_unit_type_ids",        &intf_get_unit_type_ids        },
		{ "get_units",                &intf_get_units                },
		{ "get_variable",             &intf_get_variable             },
		{ "get_village_owner",        &intf_get_village_owner        },
		{ "message",                  &intf_message                  },
		{ "put_unit",                 &intf_put_unit                 },
		{ "register_wml_action",      &intf_register_wml_action      },
		{ "require",                  &intf_require                  },
		{ "set_terrain",              &intf_set_terrain              },
		{ "set_variable",             &intf_set_variable             },
		{ "set_village_owner",        &intf_set_village_owner        },
		{ "textdomain",               &intf_textdomain               },
		{ NULL, NULL }
	};
	luaL_register(L, "wesnoth", callbacks);

	// Create the getside metatable.
	lua_pushlightuserdata(L, (void *)&getsideKey);
	lua_createtable(L, 0, 3);
	lua_pushcfunction(L, impl_side_get);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, impl_side_set);
	lua_setfield(L, -2, "__newindex");
	lua_pushstring(L, "side");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the gettext metatable.
	lua_pushlightuserdata(L, (void *)&gettextKey);
	lua_createtable(L, 0, 2);
	lua_pushcfunction(L, impl_gettext);
	lua_setfield(L, -2, "__call");
	lua_pushstring(L, "message domain");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the gettype metatable.
	lua_pushlightuserdata(L, (void *)&gettypeKey);
	lua_createtable(L, 0, 2);
	lua_pushcfunction(L, impl_unit_type_get);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, "unit type");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the getunit metatable.
	lua_pushlightuserdata(L, (void *)&getunitKey);
	lua_createtable(L, 0, 3);
	lua_pushcfunction(L, impl_unit_get);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, impl_unit_set);
	lua_setfield(L, -2, "__newindex");
	lua_pushstring(L, "unit");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the tstring metatable.
	lua_pushlightuserdata(L, (void *)&tstringKey);
	lua_createtable(L, 0, 4);
	lua_pushcfunction(L, impl_tstring_concat);
	lua_setfield(L, -2, "__concat");
	lua_pushcfunction(L, impl_tstring_collect);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, impl_tstring_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushstring(L, "translatable string");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the vconfig metatable.
	lua_pushlightuserdata(L, (void *)&vconfigKey);
	lua_createtable(L, 0, 4);
	lua_pushcfunction(L, impl_vconfig_collect);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, impl_vconfig_get);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, impl_vconfig_size);
	lua_setfield(L, -2, "__len");
	lua_pushstring(L, "wml object");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the wml action metatable.
	lua_pushlightuserdata(L, (void *)&wactionKey);
	lua_createtable(L, 0, 3);
	lua_pushcfunction(L, impl_wml_action_call);
	lua_setfield(L, -2, "__call");
	lua_pushcfunction(L, impl_wml_action_collect);
	lua_setfield(L, -2, "__gc");
	lua_pushstring(L, "wml action handler");
	lua_setfield(L, -2, "__metatable");
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Delete dofile and loadfile.
	lua_pushnil(L);
	lua_setglobal(L, "dofile");
	lua_pushnil(L);
	lua_setglobal(L, "loadfile");

	// Create the user action table.
	lua_pushlightuserdata(L, (void *)&uactionKey);
	lua_newtable(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	// Create the game_config variable with its metatable.
	lua_getglobal(L, "wesnoth");
	lua_newuserdata(L, 0);
	lua_createtable(L, 0, 3);
	lua_pushcfunction(L, impl_game_config_get);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, impl_game_config_set);
	lua_setfield(L, -2, "__newindex");
	lua_pushstring(L, "game config");
	lua_setfield(L, -2, "__metatable");
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "game_config");
	lua_pop(L, 1);

	// Create the current variable with its metatable.
	lua_getglobal(L, "wesnoth");
	lua_newuserdata(L, 0);
	lua_createtable(L, 0, 2);
	lua_pushcfunction(L, impl_current_get);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, "current config");
	lua_setfield(L, -2, "__metatable");
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "current");
	lua_pop(L, 1);

	// Create the package table.
	lua_getglobal(L, "wesnoth");
	lua_newtable(L);
	lua_setfield(L, -2, "package");
	lua_pop(L, 1);

	// Store the error handler, then close debug.
	lua_pushlightuserdata(L, (void *)&executeKey);
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_remove(L, -2);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);
	lua_setglobal(L, "debug");

	lua_settop(L, 0);
}

LuaKernel::~LuaKernel()
{
	lua_close(mState);
}

/**
 * Runs a script from an event handler.
 */
void LuaKernel::run_event(vconfig const &cfg, game_events::queued_event const &ev)
{
	lua_State *L = mState;

	// Get user-defined arguments; append locations and weapons to it.
	config args;
	vconfig vargs = cfg.child("args");
	if (!vargs.null()) {
		args = vargs.get_config();
	}
	if (const config &weapon = ev.data.child("first")) {
		args.add_child("weapon", weapon);
	}
	if (const config &weapon = ev.data.child("second")) {
		args.add_child("second_weapon", weapon);
	}
	if (ev.loc1.valid()) {
		args["x1"] = str_cast(ev.loc1.x + 1);
		args["y1"] = str_cast(ev.loc1.y + 1);
	}
	if (ev.loc2.valid()) {
		args["x2"] = str_cast(ev.loc2.x + 1);
		args["y2"] = str_cast(ev.loc2.y + 1);
	}

	// Get the code from the uninterpolated config object, so that $ symbols
	// are not messed with.
	const std::string &prog = cfg.get_config()["code"];

	queued_event_context dummy(&ev);
	luaW_pushvconfig(L, args);
	execute(prog.c_str(), 1, 0);
}

/**
 * Runs a script from a unit filter.
 * The script is an already compiled function given by its name.
 */
bool LuaKernel::run_filter(char const *name, unit const &u)
{
	lua_State *L = mState;

	unit_map::const_unit_iterator ui = resources::units->find(u.get_location());
	if (!ui.valid()) return false;

	// Get the user filter by name.
	lua_pushstring(L, name);
	lua_rawget(L, LUA_GLOBALSINDEX);

	// Pass the unit as argument.
	size_t *p = static_cast<size_t *>(lua_newuserdata(L, sizeof(size_t)));
	*p = ui->second.underlying_id();
	lua_pushlightuserdata(L, (void *)&getunitKey);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_setmetatable(L, -2);

	if (!luaW_pcall(L, 1, 1)) return false;

	bool b = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return b;
}

/**
 * Runs a script on a stack containing @a nArgs arguments.
 * @return true if the script was successful and @a nRets return values are available.
 */
bool LuaKernel::execute(char const *prog, int nArgs, int nRets)
{
	lua_State *L = mState;

	// Compile script into a variadic function.
	int res = luaL_loadstring(L, prog);
	if (res)
	{
		char const *m = lua_tostring(L, -1);
		chat_message("Lua error", m);
		ERR_LUA << m << '\n';
		lua_pop(L, 2);
		return false;
	}

	// Place the function before its arguments.
	if (nArgs)
		lua_insert(L, -1 - nArgs);

	return luaW_pcall(L, nArgs, nRets);
}
