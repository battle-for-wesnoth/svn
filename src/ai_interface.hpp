/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai_interface.hpp
 * Interface to the AI.
 */

#ifndef AI_INTERFACE_HPP_INCLUDED
#define AI_INTERFACE_HPP_INCLUDED

class game_display;
class gamemap;

#include "formula_callable.hpp"
#include "generic_event.hpp"
#include "pathfind.hpp"
#include "gamestatus.hpp"
#include "playturn.hpp"

class ai_interface : public game_logic::formula_callable {
public:

	/** A convenient typedef for the often used 'location' object. */
	typedef map_location location;

	/** The standard way in which a map of possible moves is recorded. */
	typedef std::multimap<location,location> move_map;

	/**
	 * info is structure which holds references to all the important objects
	 * that an AI might need access to, in order to make and implement its
	 * decisions.
	 */
	struct info {
		info(game_display& disp, gamemap& map, unit_map& units,
			std::vector<team>& teams, unsigned int team_num, gamestatus& state, class turn_info& turn_data, class game_state& game_state)
			: disp(disp), map(map), units(units), teams(teams),
			  team_num(team_num), state(state), turn_data_(turn_data), game_state_(game_state), master(true)
		{}

		/** The display object, used to draw the moves the AI makes. */
		game_display& disp;

		/** The map of the game -- use this object to find the terrain at any location. */
		gamemap& map;

		/** The map of units. It maps locations -> units. */
		unit_map& units;

		/** A list of the teams in the game. */
		std::vector<team>& teams;

		/**
		 * The number of the team the AI is.
		 *
		 * Note: this number is 1-based, so 1 must be subtracted for using it
		 * as index of 'teams'.
		 */
		unsigned int team_num;

		/** Information about what turn it is, and what time of day. */
		gamestatus& state;

		/**
		 * The object that allows the player to interact with the game.  Should
		 * not be used outside of ai_interface.
		 */
		class turn_info& turn_data_;

		/** The global game state, because we may set the completion field. */
		class game_state& game_state_;
		bool master;
	};

	/**
	 * The constructor.
	 *
	 * All derived classes should take an argument of type info& which they
	 * should pass to this constructor.
	 */
	ai_interface(info& arg) : info_(arg), last_interact_(0), user_interact_("ai_user_interact"),
		unit_recruited_("ai_unit_recruited"), unit_moved_("ai_unit_moved"),
		enemy_attacked_("ai_enemy_attacked") {
		add_ref(); //this class shouldn't be reference counted.
	}
	virtual ~ai_interface() {}

	/**
	 * Function that is called when the AI must play its turn.
	 * Derived classes should implement their AI algorithm in this function.
	 */
	virtual void play_turn() = 0;

	/**
	 * Function called when a a new turn is played
	 * Derived persistant AIs should call this function each turn (expect first)
	 */
	virtual void new_turn() {
		last_interact_ = 0;
	}

	/** Return a reference to the 'team' object for the AI. */
	team& current_team() { return info_.teams[info_.team_num-1]; }
	const team& current_team() const { return info_.teams[info_.team_num-1]; }

	/** Show a diagnostic message on the screen. */
	void diagnostic(const std::string& msg);

	/** Display a debug message as a chat message. */
	void log_message(const std::string& msg);

	// Exposes events to allow for attaching/detaching handlers.
	events::generic_event& user_interact()  { return user_interact_; }
	events::generic_event& unit_recruited() { return unit_recruited_; }
	events::generic_event& unit_moved()     { return unit_moved_; }
	events::generic_event& enemy_attacked() { return enemy_attacked_; }

        // If the AI manager should manage the AI once constructed.
        virtual bool manager_manage_ai() const { return false ; } ;

        // If an AI manager should reap the AI at end of game
        // Older AIs should use the default - false.
        virtual bool manager_reap_ai() { return false ; } ;

        /** Set the team */
        virtual void set_team(int team) { info_.team_num = team; }

        /** Evaluate */
        virtual std::string evaluate(const std::string& /*str*/)
			{ return "evaluate command not implemented by this AI"; }

        /** Return a message with information about the ai. Useful for making debugging ai-independent. */
        virtual std::string describe_self() { return "? ai"; }
protected:
	/**
	 * This function should be called to attack an enemy.
	 *
	 * @param u            The location of the attacking unit. (Note this shouldn't
	 *                     be a reference since attack::attack() can invalidate the
	 *                     unit_map and references to the map are also invalid then.)
	 * @param target       The location of the target unit. This unit must be in
	 *                     range of the attacking unit's weapon. (See note at param u.)
	 * @param weapon       The number of the weapon (0-based) which should be used
	 *                     by the attacker. (It must be a valid weapon of the attacker.)
	 * @param def_weapon   The number of the weapon (0-based) which should be used
	 *                     by the defender. (It must be a valid weapon of the defender.)
	 */
	void attack_enemy(const location u, const location target, int att_weapon, int def_weapon);

	/**
	 * This function should be called to move a unit.
	 *
	 * Once the unit has been moved, its movement allowance is set to 0.
	 * @param from                The location of the unit being moved.
	 * @param to                  The location to be moved to. This must be a
	 *                            valid move for the unit.
	 * @param possible_moves      The map of possible moves, as obtained from
	 *                            'calculate_possible_moves'.
	 */
	location move_unit(location from, location to, std::map<location,paths>& possible_moves);

	/**
	 * Identical to 'move_unit', except that the unit's movement
	 * isn't set to 0 after the move is complete.
	 */
	location move_unit_partial(location from, location t, std::map<location,paths>& possible_moves);

	/**
	 * Calculate the moves units may possibly make.
	 *
	 * @param possible_moves      A map which will be filled with the paths
	 *                            each unit can take to get to every possible
	 *                            destination. You probably don't want to use
	 *                            this object at all, except to pass to
	 *                            'move_unit'.
	 * @param srcdst              A map of units to all their possible
	 *                            destinations.
	 * @param dstsrc              A map of destinations to all the units that
	 *                            can move to that destination.
	 * @param enemy               if true, a map of possible moves for enemies
	 *                            will be calculated. If false, a map of
	 *                            possible moves for units on the AI's side
	 *                            will be calculated.  The AI's own leader will
	 *                            not be included in this map.
	 * @param assume_full_movement
	 *                            If true, the function will operate on the
	 *                            assumption that all units can move their full
	 *                            movement allotment.
	 * @param remove_destinations a pointer to a set of possible destinations
	 *                            to omit.
	 */
	void calculate_possible_moves(std::map<location,paths>& possible_moves,
		move_map& srcdst, move_map& dstsrc, bool enemy,
		bool assume_full_movement=false,
		const std::set<location>* remove_destinations=NULL) const;

 	/**
	 * A more fundamental version of calculate_possible_moves which allows the
	 * use of a speculative unit map.
	 */
	void calculate_moves(const unit_map& units,
		std::map<location,paths>& possible_moves, move_map& srcdst,
		move_map& dstsrc, bool enemy, bool assume_full_movement=false,
		const std::set<location>* remove_destinations=NULL,
		bool see_all=false) const;

	/**
	 * Recruit a unit. It will recruit the unit with the given name,
	 * at the given location, or at an available location to recruit units
	 * if 'loc' is not a valid recruiting location.
	 *
	 * @retval false              If recruitment cannot be performed, because
	 *                            there are no available tiles, or not enough
	 *                            money.
	 */
	bool recruit(const std::string& unit_name, location loc=location());

	/**
	 * Functions to retrieve the 'info' object.
	 * Used by derived classes to discover all necessary game information.
	 */
	info& get_info() { return info_; }
	const info& get_info() const { return info_; }

	/**
	 * Function which should be called frequently to allow the user to interact
	 * with the interface. This function will make sure that interaction
	 * doesn't occur too often, so there is no problem with calling it very
	 * regularly.
	 */
	void raise_user_interact();

	/** Notifies all interested observers of the event respectively. */
	void raise_unit_recruited() { unit_recruited_.notify_observers(); }
	void raise_unit_moved() {  unit_moved_.notify_observers(); }
	void raise_enemy_attacked() { enemy_attacked_.notify_observers(); }
protected:
	virtual void get_inputs(std::vector<game_logic::formula_input>* inputs) const;
	virtual variant get_value(const std::string& key) const;
private:
	info info_;
	int last_interact_;
	events::generic_event user_interact_;
	events::generic_event unit_recruited_;
	events::generic_event unit_moved_;
	events::generic_event enemy_attacked_;
};

#endif
