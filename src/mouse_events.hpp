#ifndef MOUSE_EVENTS_H_INCLUDED
#define MOUSE_EVENTS_H_INCLUDED

#include "global.hpp"

#include "actions.hpp"
#include "display.hpp"
#include "gamestatus.hpp"
#include "pathfind.hpp"
#include "unit.hpp"

#include "SDL.h"

namespace events{

struct command_disabler
{
	command_disabler();
	~command_disabler();
};

class mouse_handler{
public:
	mouse_handler(display* gui, std::vector<team>& teams, unit_map& units, gamemap& map, 
		gamestatus& status, const game_data& gameinfo, undo_list& undo_stack, undo_list& redo_stack);
	void mouse_motion(const SDL_MouseMotionEvent& event, const int player_number, const bool browse);
	void mouse_press(const SDL_MouseButtonEvent& event, const int player_number, const bool browse);
	void cycle_units();
	void cycle_back_units();

	int get_path_turns() const { return path_turns_; }
	paths get_current_paths() { return current_paths_; }
	paths::route get_current_route() const { return current_route_; }
	const gamemap::location& get_last_hex() const { return last_hex_; }
	gamemap::location get_selected_hex() const { return selected_hex_; }
	const bool get_undo() const { return undo_; }
	const bool get_show_menu() const { return show_menu_; }
	void set_path_turns(const int path_turns) { path_turns_ = path_turns; }
	void set_current_paths(paths new_paths);
	void set_selected_hex(gamemap::location hex) { selected_hex_ = hex; }
	void set_gui(display* gui) { gui_ = gui; }
	void set_undo(const bool undo) { undo_ = undo; }

	unit_map::iterator selected_unit();
	const unit_map& visible_units();
private:
	team& viewing_team() { return teams_[(*gui_).viewing_team()]; }
	const team& viewing_team() const { return teams_[(*gui_).viewing_team()]; }
	team& current_team() { return teams_[team_num_-1]; }

	void mouse_motion(int x, int y, const bool browse);
	bool is_left_click(const SDL_MouseButtonEvent& event);
	bool is_middle_click(const SDL_MouseButtonEvent& event);
	bool is_right_click(const SDL_MouseButtonEvent& event);
	void left_click(const SDL_MouseButtonEvent& event, const bool browse);
	void clear_undo_stack();
	bool move_unit_along_current_route(bool check_shroud=true);
	bool attack_enemy(unit_map::iterator attacker, unit_map::iterator defender);
	void show_attack_options(unit_map::const_iterator u);
	gamemap::location current_unit_attacks_from(const gamemap::location& loc, const gamemap::location::DIRECTION preferred, const gamemap::location::DIRECTION second_preferred);
	unit_map::const_iterator find_unit(const gamemap::location& hex) const;
	unit_map::iterator find_unit(const gamemap::location& hex);
	bool unit_in_cycle(unit_map::const_iterator it);

	display* gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	gamemap& map_;
	gamestatus& status_;
	const game_data& gameinfo_;
	undo_list& undo_stack_;
	undo_list& redo_stack_;

	bool minimap_scrolling_;
	gamemap::location last_hex_;
	gamemap::location selected_hex_;
	gamemap::location::DIRECTION last_nearest_, last_second_nearest_;
	gamemap::location next_unit_;
	paths::route current_route_;
	paths current_paths_;
	bool enemy_paths_;
	mutable unit_map visible_units_;
	int path_turns_;
	unsigned int team_num_;

	//cached value indicating whether any enemy units are visible.
	//computed with enemies_visible()
	bool enemies_visible_;
	bool undo_;
	bool show_menu_;
};

extern int commands_disabled;
}

#endif
