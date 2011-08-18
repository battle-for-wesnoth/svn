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

#ifndef WB_MANAGER_HPP_
#define WB_MANAGER_HPP_

#include "side_actions.hpp"
#include "typedefs.hpp"

#include "config.hpp"
#include "map_location.hpp"
#include "network.hpp"

#include <boost/noncopyable.hpp>

class CKey;
class team;

namespace pathfind {
	struct marked_route;
}

namespace wb {

class mapbuilder_visitor;
class highlight_visitor;

/**
 * This class is the frontend of the whiteboard framework for the rest of the Wesnoth code.
 */
class manager : private boost::noncopyable
{
public:

	manager();
	~manager();

	void print_help_once();

	///Determine whether the whiteboard is activated.
	bool is_active() const { return active_; }
	///Activates/Deactivates the whiteboard
	void set_active(bool active);
	///Called by the key that temporarily toggles the activated state when held
	void set_invert_behavior(bool invert);

	///Used to ask the whiteboard if its general purpose hotkeys can be called now
	bool can_execute_hotkey() const;
	///Used to ask the whiteboard if its action reordering hotkeys can be called now
	bool can_reorder_action() const;
	///Used to ask permission to the wb to move a leader, to avoid invalidating planned recruits
	bool allow_leader_to_move(unit const& leader) const;

	/**
	 * The on_* methods below inform the whiteboard of specific events
	 */
	void on_init_side();
	void on_finish_side_turn(int side);
	void on_mouseover_change(const map_location& hex);
	void on_deselect_hex(){ erase_temp_move();}
	void on_gamestate_change();
	void on_viewer_change(size_t team_index);
	void on_change_controller(int side, team& t);

	/// Called by replay_network_sender to add whiteboard data to the outgoing network packets
	void send_network_data();
	/// Called by turn_info::process_network_data() when network data needs to be processed
	void process_network_data(config const&);
	/// Adds a side_actions::net_cmd to net_buffer_[team_index], whereupon it will (later) be sent to all allies
	void queue_net_cmd(size_t team_index, side_actions::net_cmd const&);

	/// Whether the current side has actions in its planned actions queue
	static bool current_side_has_actions();

	/// Validates all actions of the current viewing side
	void validate_viewer_actions();

	/// Transforms the unit map so that it now reflects the future state of things,
	/// i.e. when all planned actions will have been executed
	void set_planned_unit_map();
	/// Restore the regular unit map
	void set_real_unit_map();
	/// Whether the planned unit map is currently applied
	bool has_planned_unit_map() const { return planned_unit_map_active_; }

	/**
	 * Callback from the display when drawing hexes, to allow the whiteboard to
	 * add visual elements. Some visual elements such as arrows and fake units
	 * are not handled through this function, but separately registered with the display.
	 */
	void draw_hex(const map_location& hex);

	/// Creates a temporary visual arrow, that follows the cursor, for move creation purposes
	void create_temp_move();
	/// Informs whether an arrow is being displayed for move creation purposes
	bool has_temp_move() const { return route_ && !fake_units_.empty() && !move_arrows_.empty(); }
	/// Erase the temporary arrow
	void erase_temp_move();
	/// Creates a move action for the current side, and erases the temp move.
	/// The move action is inserted at the end of the queue, to be executed last.
	void save_temp_move();

	/// Creates an attack or attack-move action for the current side
	void save_temp_attack(const map_location& attack_from, const map_location& target_hex);

	/// Creates a recruit action for the current side
	/// @return true if manager has saved a planned recruit
	bool save_recruit(const std::string& name, int side_num, const map_location& recruit_hex);

	/// Creates a recall action for the current side
	/// @return true if manager has saved a planned recall
	bool save_recall(const unit& unit, int side_num, const map_location& recall_hex);

	/// Creates a suppose-dead action for the current side
	void save_suppose_dead(unit& curr_unit, map_location const& loc);

	/** Executes first action in the queue for current side */
	void contextual_execute();
	/** Executes all actions in the queue in sequence */
	void execute_all_actions();
	/** Deletes last action in the queue for current side */
	void contextual_delete();
	/** Moves the action determined by the UI toward the beginning of the queue  */
	void contextual_bump_up_action();
	/** Moves the action determined by the UI toward the beginning of the queue  */
	void contextual_bump_down_action();
	/** Deletes all planned actions for all teams */
	void erase_all_actions();

	/// Get the highlight visitor instance in use by the manager
	boost::weak_ptr<highlight_visitor> get_highlighter() { return highlighter_; }

	/** Checks whether the specified unit has at least one planned action */
	bool unit_has_actions(unit const* unit) const;

	/// Used to track gold spending per-side when building the planned unit map
	/// Is referenced by the top bar gold display
	int get_spent_gold_for(int side);

	/// Determines whether or not the undo_stack should be cleared.
	///@todo Only when there are networked allies and we have set a preferences option
	bool should_clear_undo() const {return true;}
	/// Updates shroud and clears the undo_stack and redo_stack.
	void clear_undo();

	/// Displays the whiteboard options dialog.
	void options_dlg();

private:
	void validate_actions_if_needed();
	/// Called by all of the save_***() methods after they have added their action to the queue
	void on_save_action() const;
	void update_plan_hiding(size_t viewing_team) const;
	void update_plan_hiding() const; //same as above, but uses wb::viewer_team() as default argument

	///Tracks whether the whiteboard is active.
	bool active_;
	bool inverted_behavior_;
	bool self_activate_once_;
	bool print_help_once_;
	bool wait_for_side_init_;
	bool planned_unit_map_active_;
	/** Track whenever we're modifying actions, to avoid dual execution etc. */
	bool executing_actions_;
	/** Track whether the gamestate changed and we need to validate actions. */
	bool gamestate_mutated_;

	boost::scoped_ptr<mapbuilder_visitor> mapbuilder_;
	boost::shared_ptr<highlight_visitor> highlighter_;

	boost::scoped_ptr<pathfind::marked_route> route_;

	std::vector<arrow_ptr> move_arrows_;
	std::vector<fake_unit_ptr> fake_units_;

	boost::scoped_ptr<CKey> key_poller_;

	std::vector<map_location> hidden_unit_hexes_;

	///net_buffer_[i] = whiteboard network data to be sent "from" teams[i].
	std::vector<config> net_buffer_;

	///team_plans_hidden_[i] = whether or not to hide actions from teams[i].
	std::vector<bool> team_plans_hidden_;
};

/** Applies the planned unit map for the duration of the struct's life.
 *  Reverts to real unit map on exit, no matter what was the status when the struct was created. */
struct scoped_planned_unit_map
{
	scoped_planned_unit_map();
	~scoped_planned_unit_map();
	bool has_planned_unit_map_;
};

/** Ensures that the real unit map is active for the duration of the struct's life,
 * and reverts to planned unit map if it was active when the struct was created. */
struct scoped_real_unit_map
{
	scoped_real_unit_map();
	~scoped_real_unit_map();
	bool has_planned_unit_map_;
};

/// Predicate that compares the id() of two units. Useful for searches in unit vectors with std::find_if()
struct unit_comparator_predicate {
	unit_comparator_predicate(unit const& unit) : unit_(unit) {}
	bool operator()(unit const& unit);
private:
	unit const& unit_;
};

} // end namespace wb

#endif /* WB_MANAGER_HPP_ */
