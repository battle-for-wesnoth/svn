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
 * Sighting.
 */

#include "vision.hpp"

#include "move.hpp"

#include "../game_display.hpp"
#include "../game_events.hpp"
#include "../log.hpp"
#include "../map.hpp"
#include "../map_label.hpp"
#include "../map_location.hpp"
#include "../pathfind/pathfind.hpp"
#include "../resources.hpp"
#include "../team.hpp"
#include "../unit.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)


namespace actions {


/**
 * A record of a sighting event.
 * Records the unit doing a sighting, the location of that unit at the
 * time of the sighting, and the location of the sighted unit.
 */
struct shroud_clearer::sight_data {
	sight_data(size_t viewed_id, const map_location & viewed_loc,
	           size_t viewer_id, const map_location & viewer_loc) :
		seen_id(viewed_id),    seen_loc(viewed_loc),
		sighter_id(viewer_id), sighter_loc(viewer_loc)
	{}

	size_t       seen_id;
	map_location seen_loc;
	size_t       sighter_id;
	map_location sighter_loc;
};


/**
 * Convenience wrapper for adding sighting data to the sightings_ vector.
 */
inline void shroud_clearer::record_sighting(
	const unit & seen,    const map_location & seen_loc,
	const unit & sighter, const map_location & sighter_loc)
{
	sightings_.push_back(sight_data(seen.underlying_id(),    seen_loc,
	                                sighter.underlying_id(), sighter_loc));
}


/**
 * Default constructor.
 */
shroud_clearer::shroud_clearer() : jamming_(), sightings_(), view_team_(NULL)
{}


/**
 * Destructor.
 * The purpose of explictly defining this is so we can log an error if the
 * sighted events were neither fired nor explicitly ignored.
 */
shroud_clearer::~shroud_clearer()
{
	if ( !sightings_.empty() ) {
		ERR_NG << sightings_.size() << " sighted events were ignored.\n";
	}
}

/**
 * Causes this object's "jamming" map to be recalculated.
 * This gets called as needed, and can also be manually invoked
 * via cache_units().
 * @param new_team[in]  The team whose vision will be used. If NULL, the
 *                      jamming map will be cleared.
 */
void shroud_clearer::calculate_jamming(const team * new_team)
{
	// Reset data.
	jamming_.clear();
	view_team_ = new_team;

	if ( view_team_ == NULL )
		return;

	// Build the map.
	BOOST_FOREACH (const unit &u, *resources::units)
	{
		if ( u.jamming() < 1  ||  !view_team_->is_enemy(u.side()) )
			continue;

		pathfind::jamming_path jam_path(*resources::game_map, u, u.get_location());
		BOOST_FOREACH(const pathfind::paths::step& st, jam_path.destinations) {
			if ( jamming_[st.curr] < st.move_left )
				jamming_[st.curr] = st.move_left;
		}
	}
}


/**
 * Clears shroud from a single location.
 * This also records sighted events for later firing.
 *
 * In a few cases, this will also clear corner hexes that otherwise would
 * not normally get cleared.
 * @param tm               The team whose fog/shroud is affected.
 * @param loc              The location to clear.
 * @param viewer           The unit doing the viewing (for sighted events).
 * @param view_loc         The location viewer is assumed at (for sighted events).
 * @param check_units      If false, there is no checking for an uncovered unit.
 * @param enemy_count      Incremented if an enemy is uncovered.
 * @param friend_count     Incremented if a friend is uncovered.
 * @param spectator        Will be told if a unit is uncovered.
 *
 * @return whether or not information was uncovered (i.e. returns true if
 *         the specified location was fogged/ shrouded under shared vision/maps).
 */
bool shroud_clearer::clear_loc(team &tm, const map_location &loc,
                               const unit & viewer, const map_location &view_loc,
                               bool check_units, size_t &enemy_count,
                               size_t &friend_count, move_unit_spectator * spectator)
{
	gamemap &map = *resources::game_map;
	// This counts as clearing a tile for the return value if it is on the
	// board and currently fogged under shared vision. (No need to explicitly
	// check for shrouded since shrouded implies fogged.)
	bool was_fogged = tm.fogged(loc);
	bool result = was_fogged && map.on_board(loc);

	// Clear the border as well as the board, so that the half-hexes
	// at the edge can also be cleared of fog/shroud.
	if ( map.on_board_with_border(loc) ) {
		// Both functions should be executed so don't use || which
		// uses short-cut evaluation.
		// (This is different than the return value because shared vision does
		// not apply here.)
		if ( tm.clear_shroud(loc) | tm.clear_fog(loc) ) {
			// If we are near a corner, the corner might also need to be cleared.
			// This happens at the lower-left corner and at either the upper- or
			// lower- right corner (depending on the width).

			// Lower-left corner:
			if ( loc.x == 0  &&  loc.y == map.h()-1 ) {
				const map_location corner(-1, map.h());
				tm.clear_shroud(corner);
				tm.clear_fog(corner);
			}
			// Lower-right corner, odd width:
			else if ( is_odd(map.w())  &&  loc.x == map.w()-1  &&  loc.y == map.h()-1 ) {
				const map_location corner(map.w(), map.h());
				tm.clear_shroud(corner);
				tm.clear_fog(corner);
			}
			// Upper-right corner, even width:
			else if ( is_even(map.w())  &&  loc.x == map.w()-1  &&  loc.y == 0) {
				const map_location corner(map.w(), -1);
				tm.clear_shroud(corner);
				tm.clear_fog(corner);
			}
		}
	}

	// Possible screen invalidation.
	if ( was_fogged ) {
		resources::screen->invalidate(loc);
		// Need to also invalidate adjacent hexes to get rid of the
		// "fog edge" graphics.
		map_location adjacent[6];
		get_adjacent_tiles(loc, adjacent);
		for ( int i = 0; i != 6; ++i )
			resources::screen->invalidate(adjacent[i]);
	}

	// Check for units?
	if ( result  &&  check_units  &&  loc != viewer.get_location() ) {
		// Uncovered a unit?
		unit_map::const_iterator sight_it = find_visible_unit(loc, tm);
		if ( sight_it.valid() ) {
			record_sighting(*sight_it, loc, viewer, view_loc);

			// Track this?
			if ( !sight_it->get_state(unit::STATE_PETRIFIED) ) {
				if ( tm.is_enemy(sight_it->side()) ) {
					++enemy_count;
					if ( spectator )
						spectator->add_seen_enemy(sight_it);
				} else {
					++friend_count;
					if ( spectator )
						spectator->add_seen_friend(sight_it);
				}
			}
		}
	}

	return result;
}


/**
 * Clears shroud (and fog) around the provided location for @a view_team
 * as if @a viewer was standing there.
 * This will also record sighted events, which should be either fired or
 * explicitly cleared.
 *
 * @param known_units      These locations are not checked for uncovered units.
 * @param enemy_count      Incremented for each enemy uncovered (excluding known_units).
 * @param friend_count     Incremented for each friend uncovered (excluding known_units).
 * @param spectator        Will be told of uncovered units (excluding known_units).
 *
 * @return whether or not information was uncovered (i.e. returns true if any
 *         locations in visual range were fogged/shrouded under shared vision/maps).
 */
bool shroud_clearer::clear_unit(const map_location &view_loc,
                                const unit &viewer, team &view_team,
                                const std::set<map_location>* known_units,
                                size_t * enemy_count, size_t * friend_count,
                                move_unit_spectator * spectator)
{
	bool cleared_something = false;
	// Dummy variables to make some logic simpler.
	size_t enemies=0, friends=0;
	if ( enemy_count == NULL )
		enemy_count = &enemies;
	if ( friend_count == NULL )
		friend_count = &friends;

	// Make sure the jamming map is up-to-date.
	if ( view_team_ != &view_team )
		calculate_jamming(&view_team);

	// Clear the fog.
	pathfind::vision_path sight(*resources::game_map, viewer, view_loc, jamming_);
	BOOST_FOREACH (const pathfind::paths::step &dest, sight.destinations) {
		bool known = known_units  &&  known_units->count(dest.curr) != 0;
		if ( clear_loc(view_team, dest.curr, viewer, view_loc, !known,
		               *enemy_count, *friend_count, spectator) )
			cleared_something = true;
	}
	//TODO guard with game_config option
	BOOST_FOREACH (const map_location &dest, sight.edges) {
		bool known = known_units  &&  known_units->count(dest) != 0;
		if ( clear_loc(view_team, dest, viewer, view_loc, !known,
		               *enemy_count, *friend_count, spectator) )
			cleared_something = true;
	}

	return cleared_something;
}


/**
 * Clears the record of sighted events from earlier fog/shroud clearing.
 * This should be called if the events are to be ignored and not fired.
 * (Non-cleared, non-fired events will be logged as an error.)
 */
void shroud_clearer::drop_events()
{
	if ( !sightings_.empty() ) {
		DBG_NG << sightings_.size() << " sighted events were dropped.\n";
	}
	sightings_.clear();
}


/**
 * Fires the sighted events that were recorded by earlier fog/shroud clearing.
 * @return true if the events have mutated the game state.
 */
bool shroud_clearer::fire_events()
{
	static const std::string sighted_str("sighted");
	const unit_map & units = *resources::units;

	// Possible/probable quick abort.
	if ( sightings_.empty() )
		return false;

	// In case of exceptions, clear sightings_ before processing events.
	std::vector<sight_data> sight_list;
	sight_list.swap(sightings_);

	BOOST_FOREACH (const sight_data & event, sight_list) {
		// Try to locate the sighting unit.
		unit_map::const_iterator find_it = units.find(event.sighter_id);
		const map_location & sight_loc =
			find_it == units.end() ? map_location::null_location :
			                         find_it->get_location();

		{	// Raise the event based on the latest data.
			using namespace game_events;
			raise(sighted_str,
			      entity_location(event.seen_loc, event.seen_id),
			      entity_location(sight_loc, event.sighter_id, event.sighter_loc));
		}
	}

	return game_events::pump();
}


/**
 * The invalidations that should occur after invoking clear_shroud_unit().
 * This is separate since clear_shroud_unit() might be invoked several
 * times in a row, and the invalidations might only need to be done once.
 */
void shroud_clearer::invalidate_after_clear()
{
	resources::screen->invalidate_game_status();
	resources::screen->recalculate_minimap();
	resources::screen->labels().recalculate_shroud();
	// The tiles are invalidated as they are cleared, so no need
	// to invalidate them here.
}


}//namespace actions


/**
 * Function that recalculates the fog of war.
 *
 * This is used at the end of a turn and for the defender at the end of
 * combat. As a back-up, it is also called when clearing shroud at the
 * beginning of a turn.
 * This function does nothing if the indicated side does not use fog.
 * The display is invalidated as needed.
 *
 * @param[in] side The side whose fog will be recalculated.
 */
void recalculate_fog(int side)
{
	team &tm = (*resources::teams)[side - 1];

	if (!tm.uses_fog())
		return;

	// Exclude currently seen units from sighting events.
	std::set<map_location> visible_locs;
	BOOST_FOREACH (const unit &u, *resources::units) {
		const map_location & u_location = u.get_location();

		if ( !tm.fogged(u_location) )
			visible_locs.insert(u_location);
	}

	tm.refog();
	// Invalidate the screen before clearing the shroud.
	// This speeds up the invalidations within clear_shroud_unit().
	resources::screen->invalidate_all();

	actions::shroud_clearer clearer;
	BOOST_FOREACH(const unit &u, *resources::units)
	{
		if ( u.side() == side )
			clearer.clear_unit(u.get_location(), u, tm, &visible_locs);
	}
	// Update the screen.
	clearer.invalidate_after_clear();

	//FIXME: We should possibly stop dropping these events.
	clearer.drop_events();
}

/**
 * Function that will clear shroud (and fog) based on current unit positions.
 *
 * This will not re-fog hexes unless reset_fog is set to true.
 * This function will do nothing if the side uses neither shroud nor fog.
 * The display is invalidated as needed.
 *
 * @param[in] side      The side whose shroud (and fog) will be cleared.
 * @param[in] reset_fog If set to true, the fog will also be recalculated
 *                      (refogging hexes that can no longer be seen).
 * @returns true if some shroud/fog is actually cleared away.
 */
bool clear_shroud(int side, bool reset_fog)
{
	team &tm = (*resources::teams)[side - 1];
	if (!tm.uses_shroud() && !tm.uses_fog())
		return false;

	bool result = false;

	actions::shroud_clearer clearer;
	BOOST_FOREACH(const unit &u, *resources::units)
	{
		if ( u.side() == side )
			result |= clearer.clear_unit(u.get_location(), u, tm);
	}
	// Update the screen.
	if ( result )
		clearer.invalidate_after_clear();

	//FIXME: We should probably stop dropping these events.
	clearer.drop_events();

	if ( reset_fog ) {
		// Note: This will not reveal any new tiles, so result is not affected.
		recalculate_fog(side);
	}

	return result;
}

