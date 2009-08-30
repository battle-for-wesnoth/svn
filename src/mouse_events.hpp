/* $Id$ */
/*
   Copyright (C) 2006 - 2009 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MOUSE_EVENTS_H_INCLUDED
#define MOUSE_EVENTS_H_INCLUDED

#include "global.hpp"

#include "actions.hpp"
#include "game_display.hpp"
#include "pathfind.hpp"
#include "random.hpp"
#include "unit_map.hpp"
#include "mouse_handler_base.hpp"

#include "SDL.h"

class tod_manager;

namespace events{

class mouse_handler : public mouse_handler_base {
public:
	mouse_handler(game_display* gui, std::vector<team>& teams, unit_map& units, gamemap& map,
		tod_manager& tod_mng, undo_list& undo_stack, undo_list& redo_stack);
	~mouse_handler();
	static mouse_handler* get_singleton() { return singleton_ ;}
	void set_side(int side_number);
	void mouse_press(const SDL_MouseButtonEvent& event, const bool browse);
	void cycle_units(const bool browse, const bool reverse = false);
	void cycle_back_units(const bool browse) { cycle_units(browse, true); }

	int get_path_turns() const { return path_turns_; }
	paths get_current_paths() { return current_paths_; }
	const map_location& get_last_hex() const { return last_hex_; }
	map_location get_selected_hex() const { return selected_hex_; }
	bool get_undo() const { return undo_; }
	void set_path_turns(const int path_turns) { path_turns_ = path_turns; }
	void set_current_paths(paths new_paths);
	void set_selected_hex(map_location hex) { selected_hex_ = hex; }
	void deselect_hex();
	void invalidate_reachmap() { reachmap_invalid_ = true; }

	void set_gui(game_display* gui) { gui_ = gui; }
	void set_undo(const bool undo) { undo_ = undo; }

	unit_map::iterator selected_unit();

	void add_waypoint(const map_location& loc) {waypoints_.push_back(loc);}
	void clear_waypoints() {waypoints_.clear();}
	marked_route get_route(unit_map::const_iterator un, map_location go_to, team &team);
protected:
	/**
	 * Due to the way this class is constructed we can assume that the
	 * display* gui_ member actually points to a game_display (derived class)
	 */
	game_display& gui() { return *gui_; }
	/** Const version */
	const game_display& gui() const { return *gui_; }

	team& viewing_team() { return teams_[gui().viewing_team()]; }
	const team& viewing_team() const { return teams_[gui().viewing_team()]; }
	team &current_team() { return teams_[side_num_ - 1]; }

	int drag_threshold() const;
	/**
	 * Use update to force an update of the mouse state.
	 */
	void mouse_motion(int x, int y, const bool browse, bool update=false);
	bool right_click_show_menu(int x, int y, const bool browse);
	bool left_click(int x, int y, const bool browse);
	void select_hex(const map_location& hex, const bool browse);
	void clear_undo_stack();
	bool move_unit_along_current_route(bool check_shroud, bool attackmove=false);
	// wrapper to catch bad_alloc so this should be called
	bool attack_enemy(unit_map::iterator attacker, unit_map::iterator defender);
	// the real function but can throw bad_alloc
	bool attack_enemy_(unit_map::iterator attacker, unit_map::iterator defender);

	// the perform attack function called after a random seed is obtained
	void perform_attack(map_location attacker_loc, map_location defender_loc,
		int attacker_weapon, int defender_weapon, rand_rng::seed_t seed);

	void show_attack_options(const unit_map::const_iterator &u);
	map_location current_unit_attacks_from(const map_location& loc);
	unit_map::const_iterator find_unit(const map_location& hex) const;
	unit_map::iterator find_unit(const map_location& hex);
	bool unit_in_cycle(unit_map::const_iterator it);
private:
	gamemap& map_;
	game_display* gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	tod_manager& tod_manager_;
	undo_list& undo_stack_;
	undo_list& redo_stack_;

	// previous highlighted hexes
	// the hex of the selected unit and empty hex are "free"
	map_location previous_hex_;
	map_location previous_free_hex_;
	map_location selected_hex_;
	map_location next_unit_;
	marked_route current_route_;
	std::vector<map_location> waypoints_;
	paths current_paths_;
	bool enemy_paths_;
	int path_turns_;
	int side_num_;

	bool undo_;
	bool over_route_;
	bool attackmove_;
	bool reachmap_invalid_;
	bool show_partial_move_;

	static mouse_handler * singleton_;
};

}

#endif
