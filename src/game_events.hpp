/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef GAME_EVENTS_H_INCLUDED
#define GAME_EVENTS_H_INCLUDED

#include "config.hpp"
#include "display.hpp"
#include "gamestatus.hpp"
#include "map.hpp"
#include "team.hpp"
#include "unit.hpp"
#include "unit_types.hpp"

#include <map>

//this file defines the game's events mechanism. Events might be units
//moving or fighting, or when victory or defeat occurs. A scenario's
//configuration file will define actions to take when certain events
//occur. This module is responsible for making sure that when the events
//occur, the actions take place.
//
//note that game events have nothing to do with SDL events, like mouse
//movement, keyboard events, etc. See events.hpp for how they are handled.
namespace game_events
{

const std::string& get_variable(const std::string& varname);

//the game event manager loads the scenario configuration object, and
//ensures that events are handled according to the scenario configuration
//for its lifetime.
//
//thus, a manager object should be created when a scenario is played, and
//destroyed at the end of the scenario.
struct manager {
	//note that references will be maintained, and must remain valid
	//for the life of the object.
	manager(config& scenario_cfg, display& disp, gamemap& map,
	        std::map<gamemap::location,unit>& units, std::vector<team>& teams,
		    game_state& state_of_game, game_data& data);
	~manager();
};

void write_events(config& cfg);

bool unit_matches_filter(unit_map::const_iterator itor, const config& filter);

//function to fire an event. Events may have up to two arguments, both of
//which must be locations.
bool fire(const std::string& event,
          const gamemap::location& loc1=gamemap::location::null_location,
          const gamemap::location& loc2=gamemap::location::null_location);

bool pump();

}

#endif
