/*
   Copyright (C) 2003 - 2012 by David White <dave@whitevine.net>
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
 * Various functions related to the creation of units (recruits, recalls,
 * and placed units).
 */

#ifndef ACTIONS_CREATE_H_INCLUDED
#define ACTIONS_CREATE_H_INCLUDED

class  config;
class  team;
class  vconfig;

#include "../map_location.hpp"
#include "../unit.hpp"


class unit_creator {
public:
	unit_creator(team &tm, const map_location &start_pos);
	unit_creator& allow_show(bool b);
	unit_creator& allow_get_village(bool b);
	unit_creator& allow_rename_side(bool b);
	unit_creator& allow_invalidate(bool b);
	unit_creator& allow_discover(bool b);
	unit_creator& allow_add_to_recall(bool b);

	/**
	 * finds a suitable location for unit
	 * @retval map_location::null_location if unit is to be put into recall list
	 * @retval valid on-board map location otherwise
	 */
	map_location find_location(const config &cfg, const unit* pass_check=NULL);


	/**
	 * adds a unit on map without firing any events (so, usable during team construction in gamestatus)
	 */
	void add_unit(const config &cfg, const vconfig* vcfg = NULL);

private:
	void post_create(const map_location &loc, const unit &new_unit, bool anim);

	bool add_to_recall_;
	bool discover_;
	bool get_village_;
	bool invalidate_;
	bool rename_side_;
	bool show_;
	const map_location start_pos_;
	team &team_;

};


/// Checks to see if a leader at @a leader_loc could recruit on @a recruit_loc.
bool can_recruit_on(const map_location& leader_loc, const map_location& recruit_loc, int side);
/// Checks to see if @a leader (assumed a leader) can recruit on @a recruit_loc.
/// This takes into account terrain, shroud, and whether or not there is already
/// a visible unit at recruit_loc.
inline bool can_recruit_on(const unit& leader, const map_location& recruit_loc)
{ return can_recruit_on(leader.get_location(), recruit_loc, leader.side()); }

/**
 * Finds a location to place a unit.
 * A leader of the @a side must be on a keep
 * connected by castle to a legal recruiting location. Otherwise, an error
 * message explaining this is returned.
 *
 * If no errors are encountered, the location where a unit can be recruited
 * is stored in @a recruit_location. Its value is considered first, if it is a
 * legal option.
 *
 * @return an empty string on success. Otherwise a human-readable message
 *         describing the failure is returned.
 */
std::string find_recruit_location(const int side, map_location &recruit_location, map_location& recruited_from, const std::string& unit_type);

/**
 * Finds a location to recall @a unit_recall.
 * A leader of the @a side must be on a keep
 * connected by castle to a legal recalling location. Otherwise, an error
 * message explaining this is returned.
 *
 * If no errors are encountered, the location where a unit can be recalled
 * is stored in @a recall_location. Its value is considered first, if it is a
 * legal option.
 *
 * @return an empty string on success. Otherwise a human-readable message
 *         describing the failure is returned.
 */
std::string find_recall_location(const int side, map_location& recall_location, map_location& recall_from, const unit &unit_recall);

/**
 * Get's the recruitable units from a side's leaders' personal recruit lists who can recruit on a specific hex field.
 * @param side of the leaders to search for their personal recruit lists.
 * @param recruit_location the hex field being part of the castle the player wants to recruit on.
 * @return a set of units that can be recruited by leaders on a keep connected by castle tiles with @a recruit_location.
 */
const std::set<std::string> get_recruits_for_location(int side, const map_location &recruit_location);

/**
 * Get's the recruitable units from a side's leaders' personal recruit lists who can recruit on a specific hex field.
 * If no leader is able to recruit on the given location the full recall list of the side is returned.
 * @param side of the leaders to search for their personal recruit lists.
 * @param recruit_location the hex field being part of the castle the player wants to recruit on.
 * @return a set of units that can be recruited by @a side on @a recall_loc or the full recall list of @a side.
 */
const std::vector<const unit*> get_recalls_for_location(int side, const map_location &recruit_location);

/**
 * Place a unit into the game.
 * The unit will be placed on @a recruit_location, which should be retrieved
 * through a call to recruit_location().
 */
void place_recruit(const unit &u, const map_location &recruit_location, const map_location& recruited_from,
	bool is_recall, bool show = false, bool fire_event = true, bool full_movement = false,
	bool wml_triggered = false);

#endif
