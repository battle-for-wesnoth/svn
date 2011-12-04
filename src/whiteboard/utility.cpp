/* $Id$ */
/*
 Copyright (C) 2010 - 2011 by Gabriel Morin <gabrielmorin (at) gmail (dot) com>
 Part of the Battle for Wesnoth Project http://www.wesnoth.org

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
 */

#include "utility.hpp"

#include "manager.hpp"

#include "actions.hpp"
#include "foreach.hpp"
#include "game_display.hpp"
#include "map.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "unit.hpp"

namespace wb {

size_t viewer_team()
{
	return resources::screen->viewing_team();
}

int viewer_side()
{
	return resources::screen->viewing_side();
}

side_actions_ptr viewer_actions()
{
	side_actions_ptr side_actions =
			(*resources::teams)[resources::screen->viewing_team()].get_side_actions();
	return side_actions;
}

side_actions_ptr current_side_actions()
{
	side_actions_ptr side_actions =
			(*resources::teams)[resources::controller->current_side() - 1].get_side_actions();
	return side_actions;
}

unit const* find_backup_leader(unit const& leader)
{
	assert(leader.can_recruit());
	assert(resources::game_map->is_keep(leader.get_location()));
	foreach(unit const& unit, *resources::units)
	{
		if (unit.can_recruit() &&
				resources::game_map->is_keep(unit.get_location()) &&
				unit.id() != leader.id())
		{
			if (can_recruit_on(*resources::game_map, unit.get_location(), leader.get_location()))
				return &unit;
		}
	}
	return NULL;
}

unit* find_recruiter(size_t team_index, map_location const& hex)
{
	gamemap& map = *resources::game_map;

	if(!map.on_board(hex))
		return NULL;

	if(!map.is_castle(hex))
		return NULL;

	foreach(unit& u, *resources::units)
		if(u.can_recruit()
				&& u.side() == static_cast<int>(team_index+1)
				&& can_recruit_on(map,u.get_location(),hex))
			return &u;
	return NULL;
}

unit* future_visible_unit(map_location hex, int viewer_side)
{
	future_map planned_unit_map;
	if(!resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "future_visible_unit cannot find unit, future unit map failed to build.\n";
		return NULL;
	}
	//use global method get_visible_unit
	return get_visible_unit(hex, resources::teams->at(viewer_side - 1), false);
}

unit* future_visible_unit(int on_side, map_location hex, int viewer_side)
{
	unit* unit = future_visible_unit(hex, viewer_side);
	if (unit && unit->side() == on_side)
		return unit;
	else
		return NULL;
}

int path_cost(std::vector<map_location> const& path, unit const& u)
{
	if(path.size() < 2)
		return 0;

	map_location const& dest = path.back();
	if((resources::game_map->is_village(dest) && !resources::teams->at(u.side()-1).owns_village(dest))
			|| pathfind::enemy_zoc(*resources::teams,dest,resources::teams->at(u.side()-1),u.side()))
		return u.total_movement();

	int result = 0;
	gamemap const& map = *resources::game_map;
	foreach(map_location const& loc, std::make_pair(path.begin()+1,path.end()))
		result += u.movement_cost(map[loc]);
	return result;
}

temporary_unit_hider::temporary_unit_hider(unit& u)
		: unit_(&u)
	{unit_->set_hidden(true);}
temporary_unit_hider::~temporary_unit_hider()
	{unit_->set_hidden(false);}

void ghost_owner_unit(unit* unit)
{
	unit->set_disabled_ghosted(false);
	resources::screen->invalidate(unit->get_location());
}

void unghost_owner_unit(unit* unit)
{
	unit->set_standing(true);
	resources::screen->invalidate(unit->get_location());
}

bool has_actions()
{
	foreach(team& t, *resources::teams)
		if (!t.get_side_actions()->empty())
			return true;

	return false;
}

} //end namespace wb

