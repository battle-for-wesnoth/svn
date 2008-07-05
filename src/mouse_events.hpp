/* $Id$ */
/*
   Copyright (C) 2006 - 2008 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
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
#include "unit_map.hpp"
#include "mouse_handler_base.hpp"
class gamestatus;

#include "SDL.h"

namespace events{

class mouse_handler : public mouse_handler_base {
public:
	mouse_handler(game_display* gui, std::vector<team>& teams, unit_map& units, gamemap& map,
		gamestatus& status, undo_list& undo_stack, undo_list& redo_stack);
	~mouse_handler();
	static mouse_handler* get_singleton() { return singleton_ ;}
	void set_team(const int team_number);
	void mouse_press(const SDL_MouseButtonEvent& event, const bool browse);
	void cycle_units(const bool browse, const bool reverse = false);
	void cycle_back_units(const bool browse) { cycle_units(browse, true); }

	int get_path_turns() const { return path_turns_; }
	paths get_current_paths() { return current_paths_; }
	const gamemap::location& get_last_hex() const { return last_hex_; }
	gamemap::location get_selected_hex() const { return selected_hex_; }
	bool get_undo() const { return undo_; }
	bool get_show_menu() const { return show_menu_; }
	void set_path_turns(const int path_turns) { path_turns_ = path_turns; }
	void set_current_paths(paths new_paths);
	void set_selected_hex(gamemap::location hex) { selected_hex_ = hex; }
	void deselect_hex();
	void invalidate_reachmap() { reachmap_invalid_ = true; }

	void set_gui(game_display* gui) { gui_ = gui; }
	void set_undo(const bool undo) { undo_ = undo; }

	unit_map::iterator selected_unit();
	paths::route get_route(unit_map::const_iterator un, gamemap::location go_to, team &team);
protected:
	/** 
	 * Due to the way this class is constructed we can assume that the
	 * display* gui_ member actually points to a game_display (derived class)
	 */
	game_display& gui() { return static_cast<game_display&>(*gui_); }
	/** Const version */
	const game_display& gui() const { return static_cast<game_display&>(*gui_); }
	
	team& viewing_team() { return teams_[gui().viewing_team()]; }
	const team& viewing_team() const { return teams_[gui().viewing_team()]; }
	team& current_team() { return teams_[team_num_-1]; }

	/** 
	 * Use update to force an update of the mouse state.
	 */
	void mouse_motion(int x, int y, const bool browse, bool update=false);
	bool right_click_before_menu(const SDL_MouseButtonEvent& event, const bool browse);	
	bool left_click(const SDL_MouseButtonEvent& event, const bool browse);
	void select_hex(const gamemap::location& hex, const bool browse);
	void clear_undo_stack();
	bool move_unit_along_current_route(bool check_shroud, bool attackmove=false);
	// wrapper to catch bad_alloc so this should be called
	bool attack_enemy(unit_map::iterator attacker, unit_map::iterator defender);
	// the real function but can throw bad_alloc
	bool attack_enemy_(unit_map::iterator attacker, unit_map::iterator defender);
	void show_attack_options(unit_map::const_iterator u);
	gamemap::location current_unit_attacks_from(const gamemap::location& loc);
	unit_map::const_iterator find_unit(const gamemap::location& hex) const;
	unit_map::iterator find_unit(const gamemap::location& hex);
	bool unit_in_cycle(unit_map::const_iterator it);
private:
	std::vector<team>& teams_;
	unit_map& units_;
	gamestatus& status_;
	undo_list& undo_stack_;
	undo_list& redo_stack_;

	// previous highlighted hexes
	// the hex of the selected unit and empty hex are "free"
	gamemap::location previous_hex_;
	gamemap::location previous_free_hex_;
	gamemap::location selected_hex_;
	gamemap::location next_unit_;
	paths::route current_route_;
	paths current_paths_;
	bool enemy_paths_;
	int path_turns_;
	unsigned int team_num_;

	//cached value indicating whether any enemy units are visible.
	//computed with enemies_visible()
	bool enemies_visible_;
	bool undo_;
	bool over_route_;
	bool attackmove_;
	bool reachmap_invalid_;
	bool show_partial_move_;

	static mouse_handler * singleton_;
};

}

#endif
