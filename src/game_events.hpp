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

#ifndef GAME_EVENTS_H_INCLUDED
#define GAME_EVENTS_H_INCLUDED

#include "map.hpp"
#include "unit_map.hpp"
#include "variable.hpp"

/**
 * @file
 * Define the game's events mechanism.
 *
 * Events might be units moving or fighting, or when victory or defeat occurs.
 * A scenario's configuration file will define actions to take when certain events occur.
 * This module is responsible for making sure that when the events occur, the actions take place.
 *
 * Note that game events have nothing to do with SDL events,
 * like mouse movement, keyboard events, etc.
 * See events.hpp for how they are handled.
 */

class unit;

/**
 * Changes a terrain location.
 * Ensures that villages are properly lost and that new terrains are discovered.
 */
void change_terrain(const map_location &loc, const t_translation::t_terrain &t,
	gamemap::tmerge_mode mode, bool replace_if_failed);

namespace game_events
{

	// The game event manager loads the scenario configuration object,
	// and ensures that events are handled according to the
	// scenario configuration for its lifetime.
	//
	// Thus, a manager object should be created when a scenario is played,
	// and destroyed at the end of the scenario.
	struct manager {
		// Note that references will be maintained,
		// and must remain valid for the life of the object.
		manager(const config& scenario_cfg);
		~manager();

	private:
		variable::manager variable_manager;
	};

	struct entity_location : public map_location {
		entity_location(const map_location &loc, size_t id = 0);
		entity_location(const map_location &loc, size_t id,
		                const map_location &filter_loc);
		explicit entity_location(const unit &);
		entity_location(const unit &u, const map_location &filter_loc);

		int filter_x() const { return filter_loc_.x; }
		int filter_y() const { return filter_loc_.y; }
		bool matches_unit(const unit_map::const_iterator & un_it) const;
		bool matches_unit_filter(const unit_map::const_iterator & un_it,
		                         const vconfig & filter) const;

		static const entity_location null_entity;

	private:
		/// The underlying ID of the unit associated with this.
		/// Set to 0 if there is no associated unit.
		size_t id_;

		/// This map_location allows a unit to be filtered as if it were
		/// somewhere other than where it is. (Use the parent struct if
		/// you want to locate the unit.)
		map_location filter_loc_;
	};


	struct queued_event {
		queued_event(const std::string& name, const game_events::entity_location& loc1,
				const game_events::entity_location& loc2,
				const config& data)
			: name(name), loc1(loc1), loc2(loc2),data(data) {}

		std::string name;
		game_events::entity_location loc1;
		game_events::entity_location loc2;
		config data;
	};

	class event_handler
	{
		public:
			event_handler(const config &cfg, bool is_menu_item = false);

			bool matches_name(const std::string& name) const;

			bool disabled() const { return disabled_; }
			bool is_menu_item() const { return is_menu_item_; }

			void handle_event(const queued_event& event_info);

			const config &get_config() const { return cfg_; }
		private:
			bool first_time_only_;
			bool disabled_;
			bool is_menu_item_;
			config cfg_;
	};

	/**
	 * Runs the action handler associated to the command sequence @a cfg.
	 */
	void handle_event_commands(const queued_event &event_info, const vconfig &cfg);

	/**
	 * Runs the action handler associated to @a cmd with parameters @a cfg.
	 */
	void handle_event_command(const std::string &cmd,
		const queued_event &event_info, const vconfig &cfg);

	void write_events(config& cfg);
	void add_events(
		const config::const_child_itors &cfgs,
		const std::string& type = std::string());

	bool unit_matches_filter(const unit &, const vconfig& filter);

	/** Used for [wml_message]. */
	void handle_wml_log_message(const config& cfg);
	/** Used for [deprecated_message]. */
	void handle_deprecated_message(const config& cfg);

	/** Gets a vector of sides from side= attribute in a given config node.
     * Promotes consistent behavior.
	*/
	std::vector<int> get_sides_vector(const vconfig& cfg);

	/**
	 * Function to fire an event.
	 *
	 * Events may have up to two arguments, both of which must be locations.
	 */
	bool fire(const std::string& event,
			const entity_location& loc1=entity_location::null_entity,
			const entity_location& loc2=entity_location::null_entity,
			const config& data=config());

	void raise(const std::string& event,
			const entity_location& loc1=entity_location::null_entity,
			const entity_location& loc2=entity_location::null_entity,
			const config& data=config());

	bool conditional_passed(const vconfig& cond, bool backwards_compat=true);

	/**
	 * Handles newly-created handlers. Flushes WML messages and errors.
	 */
	void commit();

	bool pump();

	typedef void (*action_handler)(const game_events::queued_event &, const vconfig &);

	/**
	 * This function can be used to detect when no WML/Lua has been executed.
	 */
	size_t wml_tracking();

} // end namespace game_events

#endif
