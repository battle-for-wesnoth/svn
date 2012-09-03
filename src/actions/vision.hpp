/* $Id$ */
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
 * Various functions implementing vision (through fog of war and shroud).
 */

#ifndef ACTIONS_VISION_H_INCLUDED
#define ACTIONS_VISION_H_INCLUDED

struct map_location;
class  move_unit_spectator;
class  team;
class  unit;

#include <boost/noncopyable.hpp>
#include <cstring>
#include <map>
#include <set>
#include <vector>


namespace actions {

/// Class to encapsulate fog/shroud clearing and the resultant sighted events.
/// Note: This class uses teams as parameters (instead of sides) since a
/// function using this should first check to see if fog/shroud is in use (to
/// save processing when it is not), which implies the team is readily available.
class shroud_clearer : public boost::noncopyable {
public:
	shroud_clearer();
	~shroud_clearer();

	/// Function to be called if units have moved or otherwise changed.
	/// It can also be called if it is desirable to calculate the cache
	/// in advance of fog clearing.
	/// @param[in] new_team  The team whose vision will be used. If left as
	///                      NULL, the cache will be just be cleared (to be
	///                      recalculated later as needed).
	void cache_units(const team * new_team=NULL) { calculate_jamming(new_team); }
	// cache_units() is currently a near-synonym for calculate_jamming(). The
	// reason for the two names is so the private function says what it does,
	// while the public one says why it might be invoked.

	/// Clears shroud (and fog) around the provided location for @a view_team
	/// as if @a viewer was standing there.
	bool clear_unit(const map_location &view_loc,
	                const unit &viewer, team &view_team,
    	            const std::set<map_location>* known_units = NULL,
	                size_t * enemy_count = NULL, size_t * friend_count = NULL,
	                move_unit_spectator * spectator = NULL);
	/// Clears shroud (and fog) around the provided location as if @a viewer
	/// was standing there.
	bool clear_unit(const map_location &view_loc, const unit &viewer,
	                bool can_delay = false, bool invalidate = true);

	/// Erases the record of sighted events from earlier fog/shroud clearing.
	void drop_events();

	/// Fires the sighted events that were earlier recorded by fog/shroud clearing.
	bool fire_events();

	/// The invalidations that should occur after invoking clear_shroud_unit().
	/// This is separate since clear_shroud_unit() might be invoked several
	/// times in a row, and the invalidations might only need to be done once.
	void invalidate_after_clear();

private:
	/// A record of a sighting event.
	struct sight_data;

	/// Causes this object's "jamming" map to be recalculated.
	void calculate_jamming(const team * new_team);

	/// Clears shroud from a single location.
	bool clear_loc(team &tm, const map_location &loc,
	               const unit & viewer, const map_location &view_loc,
	               bool check_units, size_t &enemy_count, size_t &friend_count,
	               move_unit_spectator * spectator = NULL);

	/// Convenience wrapper for adding sighting data to the sightings_ vector.
	inline void record_sighting(const unit & seen,    const map_location & seen_loc,
	                            const unit & sighter, const map_location & sighter_loc);

private: // data
	std::map<map_location, int> jamming_;
	std::vector<sight_data> sightings_;
	/// Keeps track of the team associated with jamming_.
	const team * view_team_;
};


/// Returns the sides that cannot currently see @a target.
std::vector<int> get_sides_not_seeing(const unit & target);
/// Fires sighted events for the sides that can see @a target.
bool actor_sighted(const unit & target, const std::vector<int> * cache =  NULL);


}//namespace actions

/// Function that recalculates the fog of war.
void recalculate_fog(int side);

/// Function that will clear shroud (and fog) based on current unit positions.
bool clear_shroud(int side, bool reset_fog=false);

#endif
