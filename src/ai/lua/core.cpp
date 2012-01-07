/* $Id$ */
/*
   Copyright (C) 2010 - 2012 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Provides core classes for the Lua AI.
 *
 */

#include "lua/lualib.h"
#include "lua/lauxlib.h"

#include <cassert>
#include <cstring>

#include "core.hpp"
#include "../../scripting/lua.hpp"
#include "../../scripting/lua_api.hpp"
#include "lua_object.hpp" // (Nephro)


#include "../../actions.hpp"
#include "../../attack_prediction.hpp"
#include "../../filesystem.hpp"
#include "../../foreach.hpp"
#include "../../game_display.hpp"
#include "../../gamestatus.hpp"
#include "../../log.hpp"
#include "../../map.hpp"
#include "../../pathfind/pathfind.hpp"
#include "../../play_controller.hpp"
#include "../../resources.hpp"
#include "../../terrain_translation.hpp"
#include "../../terrain_filter.hpp"
#include "../../unit.hpp"
#include "../actions.hpp"
#include "../composite/engine_lua.hpp"
#include "../composite/contexts.hpp"
#include <lua/llimits.h>

static lg::log_domain log_ai_engine_lua("ai/engine/lua");
#define LOG_LUA LOG_STREAM(info, log_ai_engine_lua)
#define ERR_LUA LOG_STREAM(err, log_ai_engine_lua)

static char const aisKey     = 0;

namespace ai {

static void push_map_location(lua_State *L, const map_location& ml);

void lua_ai_context::init(lua_State *L)
{
	// Create the ai elements table.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_newtable(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

void lua_ai_context::get_persistent_data(config &cfg) const
{
	int top = lua_gettop(L);

	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_rawgeti(L, -1, num_);

	lua_getfield(L, -1, "data");
	luaW_toconfig(L, -1, cfg);

	lua_settop(L, top);
}

void lua_ai_context::set_persistent_data(const config &cfg)
{
	int top = lua_gettop(L);

	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_rawgeti(L, -1, num_);

	luaW_pushconfig(L, cfg);
	lua_setfield(L, -2, "data");

	lua_settop(L, top);
}
static ai::engine_lua &get_engine(lua_State *L)
{
	return *(static_cast<ai::engine_lua*>(
			lua_touserdata(L, lua_upvalueindex(1))));
}

static ai::readonly_context &get_readonly_context(lua_State *L)
{
	return get_engine(L).get_readonly_context();
}

static int transform_ai_action(lua_State *L, ai::action_result_ptr action_result)
{
	lua_newtable(L);
	lua_pushboolean(L,action_result->is_ok());
	lua_setfield(L, -2, "ok");
	lua_pushboolean(L,action_result->is_gamestate_changed());
	lua_setfield(L, -2, "gamestate_changed");
	lua_pushinteger(L,action_result->get_status());
	lua_setfield(L, -2, "status");
	return 1;
}

static bool to_map_location(lua_State *L, int &index, map_location &res)
{
	if (lua_isuserdata(L, index))
	{
		unit const *u = luaW_tounit(L, index);
		if (!u) return false;
		res = u->get_location();
		++index;
	}
	else
	{
		if (!lua_isnumber(L, index)) return false;
		res.x = lua_tointeger(L, index) - 1;
		++index;
		if (!lua_isnumber(L, index)) return false;
		res.y = lua_tointeger(L, index) - 1;
		++index;
	}

	return true;
}

static int cfun_ai_get_suitable_keep(lua_State *L)
{
	int index = 1;

	ai::readonly_context &context = get_readonly_context(L);
	unit const *leader;
	if (lua_isuserdata(L, index))
	{
		leader = luaW_tounit(L, index);
		if (!leader) return luaL_argerror(L, 1, "unknown unit");
	}
	else return luaL_typerror(L, 1, "unit");
	const map_location loc = leader->get_location();
	const pathfind::paths leader_paths(*resources::game_map, *resources::units, loc,
		*resources::teams, false, true, context.current_team());
	const map_location &res = context.suitable_keep(loc,leader_paths);
	if (!res.valid()) {
		return 0;
	}
	else {
		lua_pushnumber(L, res.x+1);
		lua_pushnumber(L, res.y+1);
		return 2;
	}
}

static int ai_execute_move(lua_State *L, bool remove_movement)
{
	int index = 1;
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, index, "location (unit/integers)");
	}

	int side = get_readonly_context(L).get_side();
	map_location from, to;
	if (!to_map_location(L, index, from)) goto error_call_destructors;
	if (!to_map_location(L, index, to)) goto error_call_destructors;
	bool unreach_is_ok = false;
	if (lua_isboolean(L, index)) {
		unreach_is_ok = lua_toboolean(L, index);
	}
ai::move_result_ptr move_result = ai::actions::execute_move_action(side,true,from,to,remove_movement, unreach_is_ok);
	return transform_ai_action(L,move_result);
}

static int cfun_ai_execute_move_full(lua_State *L)
{
	return ai_execute_move(L, true);
}

static int cfun_ai_execute_move_partial(lua_State *L)
{
	return ai_execute_move(L, false);
}

static int cfun_ai_execute_attack(lua_State *L)
{
	int index = 1;
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, index, "location (unit/integers)");
	}

	ai::readonly_context &context = get_readonly_context(L);

	int side = context.get_side();
	map_location attacker, defender;
	if (!to_map_location(L, index, attacker)) goto error_call_destructors;
	if (!to_map_location(L, index, defender)) goto error_call_destructors;

	int attacker_weapon = -1;//-1 means 'select what is best'
	double aggression = context.get_aggression();//use the aggression from the context

	if (!lua_isnoneornil(L, index+1) && lua_isnumber(L,index+1)) {
		aggression = lua_tonumber(L, index+1);
	}

	if (!lua_isnoneornil(L, index)) {
		attacker_weapon = lua_tointeger(L, index);
	}

	ai::attack_result_ptr attack_result = ai::actions::execute_attack_action(side,true,attacker,defender,attacker_weapon,aggression);
	return transform_ai_action(L,attack_result);
}

static int ai_execute_stopunit_select(lua_State *L, bool remove_movement, bool remove_attacks)
{
	int index = 1;
	if (false) {
		error_call_destructors:
		return luaL_typerror(L, index, "location (unit/integers)");
	}

	int side = get_readonly_context(L).get_side();
	map_location loc;
	if (!to_map_location(L, index, loc)) goto error_call_destructors;

	ai::stopunit_result_ptr stopunit_result = ai::actions::execute_stopunit_action(side,true,loc,remove_movement,remove_attacks);
	return transform_ai_action(L,stopunit_result);
}

static int cfun_ai_execute_stopunit_moves(lua_State *L)
{
	return ai_execute_stopunit_select(L, true, false);
}

static int cfun_ai_execute_stopunit_attacks(lua_State *L)
{
	return ai_execute_stopunit_select(L, false, true);
}

static int cfun_ai_execute_stopunit_all(lua_State *L)
{
	return ai_execute_stopunit_select(L, true, true);
}

static int cfun_ai_execute_recruit(lua_State *L)
{
	const char *unit_name = luaL_checkstring(L, 1);
	int side = get_readonly_context(L).get_side();
	map_location where;
	if (!lua_isnoneornil(L, 2)) {
		where.x = lua_tonumber(L, 2) - 1;
		where.y = lua_tonumber(L, 3) - 1;
	}
	//TODO fendrin: talk to Crab about the from argument.
	map_location from = map_location::null_location;
	ai::recruit_result_ptr recruit_result = ai::actions::execute_recruit_action(side,true,std::string(unit_name),where,from);
	return transform_ai_action(L,recruit_result);
}

static int cfun_ai_execute_recall(lua_State *L)
{
	const char *unit_id = luaL_checkstring(L, 1);
	int side = get_readonly_context(L).get_side();
	map_location where;
	if (!lua_isnoneornil(L, 2)) {
		where.x = lua_tonumber(L, 2) - 1;
		where.y = lua_tonumber(L, 3) - 1;
	}
	//TODO fendrin: talk to Crab about the from argument.
	map_location from = map_location::null_location;
	ai::recall_result_ptr recall_result = ai::actions::execute_recall_action(side,true,std::string(unit_id),where,from);
	return transform_ai_action(L,recall_result);
}

// Goals and targets


static int cfun_ai_get_targets(lua_State *L)
{
	move_map enemy_dst_src = get_readonly_context(L).get_enemy_dstsrc();
	std::vector<target> targets = get_engine(L).get_ai_context()->find_targets(enemy_dst_src);
	int i = 1;

	lua_createtable(L, 0, 0);
	for (std::vector<target>::iterator it = targets.begin(); it != targets.end(); it++)
	{
		lua_pushinteger(L, i);

		//to factor out
		lua_createtable(L, 3, 0);


		lua_pushstring(L, "type");
		lua_pushnumber(L, it->type);
		lua_rawset(L, -3);

		lua_pushstring(L, "loc");
		push_map_location(L, it->loc);
		lua_rawset(L, -3);

		lua_pushstring(L, "value");
		lua_pushnumber(L, it->value);
		lua_rawset(L, -3);

		lua_rawset(L, -3);
		++i;
	}
	return 1;
}

// Aspect section
static int cfun_ai_get_aggression(lua_State *L)
{
	double aggression = get_readonly_context(L).get_aggression();
	lua_pushnumber(L, aggression);
	return 1;
}

static int cfun_ai_get_attack_depth(lua_State *L)
{
	int attack_depth = get_readonly_context(L).get_attack_depth();
	lua_pushnumber(L, attack_depth);
	return 1;
}

static int cfun_ai_get_avoid(lua_State *L)
{
	std::set<map_location> locs;
	terrain_filter avoid = get_readonly_context(L).get_avoid();
	avoid.get_locations(locs, true); // is it true here?

	int sz = locs.size();
	lua_createtable(L, sz, 0); // create a table that we'll use as an array

	std::set<map_location>::iterator it = locs.begin();
	for (int i = 0; it != locs.end(); ++it, ++i)
	{
		lua_pushinteger(L, i + 1); // Index for the map location
		lua_createtable(L, 2, 0); // Table for a single map location

		lua_pushstring(L, "x");
		lua_pushinteger(L, it->x);
		lua_settable(L, -3);

		lua_pushstring(L, "y");
		lua_pushinteger(L, it->y);
		lua_settable(L, -3);

		lua_settable(L, -3);
	}

	return 1;
}

static int cfun_ai_get_caution(lua_State *L)
{
	double caution = get_readonly_context(L).get_caution();
	lua_pushnumber(L, caution);
	return 1;
}

static int cfun_ai_get_grouping(lua_State *L)
{
	std::string grouping = get_readonly_context(L).get_grouping();
	lua_pushstring(L, grouping.c_str());
	return 1;
}

static int cfun_ai_get_leader_aggression(lua_State *L)
{
	double leader_aggression = get_readonly_context(L).get_leader_aggression();
	lua_pushnumber(L, leader_aggression);
	return 1;
}

static int cfun_ai_get_leader_goal(lua_State *L)
{
	config goal = get_readonly_context(L).get_leader_goal();
	luaW_pushconfig(L, goal);
	return 1;
}

static int cfun_ai_get_leader_value(lua_State *L)
{
	double leader_value = get_readonly_context(L).get_leader_value();
	lua_pushnumber(L, leader_value);
	return 1;
}

static int cfun_ai_get_passive_leader(lua_State *L)
{
	bool passive_leader = get_readonly_context(L).get_passive_leader();
	lua_pushboolean(L, passive_leader);
	return 1;
}

static int cfun_ai_get_passive_leader_shares_keep(lua_State *L)
{
	bool passive_leader_shares_keep = get_readonly_context(L).get_passive_leader_shares_keep();
	lua_pushboolean(L, passive_leader_shares_keep);
	return 1;
}

static int cfun_ai_get_number_of_possible_recruits_to_force_recruit(lua_State *L)
{
	double noprtfr = get_readonly_context(L).get_number_of_possible_recruits_to_force_recruit(); // @note: abbreviation
	lua_pushnumber(L, noprtfr);
	return 1;
}

static int cfun_ai_get_recruitment_ignore_bad_combat(lua_State *L)
{
	bool recruitment_ignore_bad_combat = get_readonly_context(L).get_recruitment_ignore_bad_combat();
	lua_pushboolean(L, recruitment_ignore_bad_combat);
	return 1;
}

static int cfun_ai_get_recruitment_ignore_bad_movement(lua_State *L)
{
	bool recruitment_ignore_bad_movement = get_readonly_context(L).get_recruitment_ignore_bad_movement();
	lua_pushboolean(L, recruitment_ignore_bad_movement);
	return 1;
}

static int cfun_ai_get_recruitment_pattern(lua_State *L)
{
	std::vector<std::string> recruiting = get_readonly_context(L).get_recruitment_pattern();
	int size = recruiting.size();
	lua_createtable(L, size, 0); // create an exmpty table with predefined size
	for (int i = 0; i < size; ++i)
	{
		lua_pushinteger(L, i + 1); // Indexing in Lua starts from 1
		lua_pushstring(L, recruiting[i].c_str());
		lua_settable(L, -3);
	}
	return 1;
}

static int cfun_ai_get_scout_village_targeting(lua_State *L)
{
	double scout_village_targeting = get_readonly_context(L).get_scout_village_targeting();
	lua_pushnumber(L, scout_village_targeting);
	return 1;
}

static int cfun_ai_get_simple_targeting(lua_State *L)
{
	bool simple_targeting = get_readonly_context(L).get_simple_targeting();
	lua_pushboolean(L, simple_targeting);
	return 1;
}

static int cfun_ai_get_support_villages(lua_State *L)
{
	bool support_villages = get_readonly_context(L).get_support_villages();
	lua_pushboolean(L, support_villages);
	return 1;
}

static int cfun_ai_get_village_value(lua_State *L)
{
	double village_value = get_readonly_context(L).get_village_value();
	lua_pushnumber(L, village_value);
	return 1;
}

static int cfun_ai_get_villages_per_scout(lua_State *L)
{
	int villages_per_scout = get_readonly_context(L).get_villages_per_scout();
	lua_pushnumber(L, villages_per_scout);
	return 1;
}
// End of aspect section

static void push_map_location(lua_State *L, const map_location& ml)
{
	lua_createtable(L, 2, 0);

	lua_pushstring(L, "x");
	lua_pushinteger(L, ml.x + 1);
	lua_rawset(L, -3);

	lua_pushstring(L, "y");
	lua_pushinteger(L, ml.y + 1);
	lua_rawset(L, -3);
}

static void push_move_map(lua_State *L, const move_map& m)
{
	move_map::const_iterator it = m.begin();

	int index = 1;

	lua_createtable(L, 0, 0); // the main table

	do {
		map_location key = it->first;

		push_map_location(L, key);
		lua_createtable(L, 0, 0);

		while (key == it->first) {

			push_map_location(L, it->second);
			lua_rawseti(L, -2, index);

			++index;
			++it;

		}

		lua_settable(L, -3);

		index = 1;

	} while (it != m.end());
}

static int cfun_ai_get_dstsrc(lua_State *L)
{
	move_map dst_src = get_readonly_context(L).get_dstsrc();
	push_move_map(L, dst_src);
	return 1;
}

static int cfun_ai_get_srcdst(lua_State *L)
{
	move_map src_dst = get_readonly_context(L).get_dstsrc();
	push_move_map(L, src_dst);
	return 1;
}

static int cfun_ai_get_enemy_dstsrc(lua_State *L)
{
	move_map enemy_dst_src = get_readonly_context(L).get_enemy_dstsrc();
	push_move_map(L, enemy_dst_src);
	return 1;
}

static int cfun_ai_get_enemy_srcdst(lua_State *L)
{
	move_map enemy_src_dst = get_readonly_context(L).get_enemy_dstsrc();
	push_move_map(L, enemy_src_dst);
	return 1;
}

lua_ai_context* lua_ai_context::create(lua_State *L, char const *code, ai::engine_lua *engine)
{
	int res_ai = luaL_loadstring(L, code);//stack size is now 1 [ -1: ai_context]
	if (res_ai)
	{

		char const *m = lua_tostring(L, -1);
		ERR_LUA << "error while initializing ai:  " <<m << '\n';
		lua_pop(L, 2);//return with stack size 0 []
		return NULL;
	}
	//push data table here
	lua_newtable(L);// stack size is 2 [ -1: new table, -2: ai as string ]
	lua_pushinteger(L, engine->get_readonly_context().get_side());

	lua_setfield(L, -2, "side");//stack size is 2 [- 1: new table; -2 ai as string]

	static luaL_reg const callbacks[] = {
		{ "attack", 			&cfun_ai_execute_attack			},
		// Move maps
		{ "get_dstsrc", 		&cfun_ai_get_dstsrc			},
		{ "get_srcdst", 		&cfun_ai_get_srcdst			},
		{ "get_enemy_dstsrc", 		&cfun_ai_get_enemy_dstsrc		},
		{ "get_enemy_srcdst", 		&cfun_ai_get_enemy_srcdst		},
		// End of move maps
		// Goals and targets
		{ "get_targets",		&cfun_ai_get_targets			},
		// End of G & T
		// Aspects
		{ "get_aggression", 		&cfun_ai_get_aggression           	},
		{ "get_avoid", 			&cfun_ai_get_avoid			},
		{ "get_attack_depth",		&cfun_ai_get_attack_depth		},
		{ "get_caution", 		&cfun_ai_get_caution			},
		{ "get_grouping",		&cfun_ai_get_grouping			},
		{ "get_leader_aggression", 	&cfun_ai_get_leader_aggression		},
		{ "get_leader_goal", 		&cfun_ai_get_leader_goal		},
		{ "get_leader_value", 		&cfun_ai_get_leader_value		},
		{ "get_number_of_possible_recruits_to_force_recruit", &cfun_ai_get_number_of_possible_recruits_to_force_recruit},
		{ "get_passive_leader", 	&cfun_ai_get_passive_leader		},
		{ "get_passive_leader_shares_keep", &cfun_ai_get_passive_leader_shares_keep},
		{ "get_recruitment_ignore_bad_combat", &cfun_ai_get_recruitment_ignore_bad_combat},
		{ "get_recruitment_ignore_bad_movement", &cfun_ai_get_recruitment_ignore_bad_movement},
		{ "get_recruitment_pattern", 	&cfun_ai_get_recruitment_pattern 	},
		{ "get_scout_village_targeting", &cfun_ai_get_scout_village_targeting	},
		{ "get_simple_targeting", 	&cfun_ai_get_simple_targeting		},
		{ "get_support_villages",	&cfun_ai_get_support_villages		},
		{ "get_village_value",		&cfun_ai_get_village_value		},
		{ "get_villages_per_scout",	&cfun_ai_get_villages_per_scout		},
		// End of aspects
		{ "move",             		&cfun_ai_execute_move_partial		},
		{ "move_full",        		&cfun_ai_execute_move_full        	},
		{ "recall",          		&cfun_ai_execute_recall           	},
		{ "recruit",          		&cfun_ai_execute_recruit         	},
		{ "stopunit_all",     		&cfun_ai_execute_stopunit_all     	},
		{ "stopunit_attacks",		&cfun_ai_execute_stopunit_attacks 	},
		{ "stopunit_moves",   		&cfun_ai_execute_stopunit_moves 	},
		{ "suitable_keep",   		&cfun_ai_get_suitable_keep		},
		{ NULL, NULL }
	};

	for (const luaL_reg *p = callbacks; p->name; ++p) {
		lua_pushlightuserdata(L, engine);
		lua_pushcclosure(L, p->func, 1);
		lua_setfield(L, -2, p->name);
	}

	//compile the ai as a closure
	if (!luaW_pcall(L, 1, 1, true)) {
		return NULL;//return with stack size 0 []
	}

	// Retrieve the ai elements table from the registry.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);   //stack size is now 2  [-1: ais_table -2: f]
	// Push the function in the table so that it is not collected.
	size_t length_ai = lua_objlen(L, -1);//length of ais_table
	lua_pushvalue(L, -2); //stack size is now 3: [-1: ai_context  -2: ais_table  -3: ai_context]
	lua_rawseti(L, -2, length_ai + 1);// ais_table[length+1]=ai_context.  stack size is now 2 [-1: ais_table  -2: ai_context]
	lua_pop(L, 2);
	return new lua_ai_context(L, length_ai + 1, engine->get_readonly_context().get_side());
}

lua_ai_action_handler* lua_ai_action_handler::create(lua_State *L, char const *code, lua_ai_context &context)
{
	int res = luaL_loadstring(L, code);//stack size is now 1 [ -1: f]
	if (res)
	{
		char const *m = lua_tostring(L, -1);
		ERR_LUA << "error while creating ai function:  " <<m << '\n';
		lua_pop(L, 2);//return with stack size 0 []
		return NULL;
	}


	// Retrieve the ai elements table from the registry.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);   //stack size is now 2  [-1: ais_table -2: f]
	// Push the function in the table so that it is not collected.
	size_t length = lua_objlen(L, -1);//length of ais_table
	lua_pushvalue(L, -2); //stack size is now 3: [-1: f  -2: ais_table  -3: f]
	lua_rawseti(L, -2, length + 1);// ais_table[length+1]=f.  stack size is now 2 [-1: ais_table  -2: f]
	lua_remove(L, -1);//stack size is now 1 [-1: f]
	lua_remove(L, -1);//stack size is now 0 []
	// Create the proxy C++ action handler.
	return new lua_ai_action_handler(L, context, length + 1);
}


void lua_ai_context::load()
{
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));//stack size is now 1 [-1: ais_table key]
	lua_rawget(L, LUA_REGISTRYINDEX);//stack size is still 1 [-1: ais_table]
	lua_rawgeti(L, -1, num_);//stack size is 2 [-1: ai_context -2: ais_table]
	lua_remove(L,-2);
}

lua_ai_context::~lua_ai_context()
{
	// Remove the ai context from the registry, so that it can be collected.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);
	lua_rawseti(L, -2, num_);
	lua_pop(L, 1);
}

void lua_ai_action_handler::handle(config &cfg, bool configOut, lua_object_ptr l_obj)
{
	int initial_top = lua_gettop(L);//get the old stack size

	// Load the user function from the registry.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));//stack size is now 1 [-1: ais_table key]
	lua_rawget(L, LUA_REGISTRYINDEX);//stack size is still 1 [-1: ais_table]
	lua_rawgeti(L, -1, num_);//stack size is 2 [-1: ai_action  -2: ais_table]
	lua_remove(L, -2);//stack size is 1 [-1: ai_action]
	//load the lua ai context as a parameter
	context_.load();//stack size is 2 [-1: ai_context -2: ai_action]

	if (!configOut)
	{
		luaW_pushconfig(L, cfg);
		luaW_pcall(L, 2, 0, true);
	}
	else if (luaW_pcall(L, 1, 5, true)) // @note for Crab: how much nrets should we actually have here
	{				    // there were 2 initially, but aspects like recruitment pattern
		l_obj->store(L, initial_top + 1); // return a lot of results
	}

	lua_settop(L, initial_top);//empty stack
}

lua_ai_action_handler::~lua_ai_action_handler()
{
	// Remove the function from the registry, so that it can be collected.
	lua_pushlightuserdata(L, static_cast<void *>(const_cast<char *>(&aisKey)));
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);
	lua_rawseti(L, -2, num_);
	lua_pop(L, 1);
}

} // of namespace ai
