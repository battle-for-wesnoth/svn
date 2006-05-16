/*
   Copyright (C) 2006 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MENU_EVENTS_H_INCLUDED
#define MENU_EVENTS_H_INCLUDED

#include "global.hpp"

#include "dialogs.hpp"
#include "display.hpp"
#include "floating_textbox.hpp"
#include "gamestatus.hpp"
#include "mouse_events.hpp"
#include "statistics.hpp"
#include "widgets/textbox.hpp"

enum LEVEL_RESULT { VICTORY, DEFEAT, QUIT, LEVEL_CONTINUE, LEVEL_CONTINUE_NO_SAVE };

struct end_level_exception {
	end_level_exception(LEVEL_RESULT res, bool bonus=true)
	                     : result(res), gold_bonus(bonus)
	{}
	LEVEL_RESULT result;
	bool gold_bonus;
};

struct end_turn_exception {
	end_turn_exception(unsigned int r = 0): redo(r) {}
	unsigned int redo;
};

namespace events{

class chat_handler {
public:
	chat_handler() {}
	virtual ~chat_handler();

protected:
	void do_speak(const std::string& message, bool allies_only=false);

	//called from do_speak
	virtual void add_chat_message(const std::string& speaker, int side, const std::string& message, display::MESSAGE_TYPE type=display::MESSAGE_PRIVATE)=0;
	virtual void send_chat_message(const std::string& message, bool allies_only=false)=0;
	virtual void send_chat_query(const std::string& /*args*/) {}
};

class menu_handler : private chat_handler {
public:
	menu_handler(display* gui, unit_map& units, std::vector<team>& teams,
		const config& level, const game_data& gameinfo, const gamemap& map,
		const config& game_config, const gamestatus& status, game_state& gamestate,
		undo_list& undo_stack, undo_list& redo_stack);
	virtual ~menu_handler();

	const undo_list& get_undo_list() const;
	gui::floating_textbox& get_textbox();
	void set_gui(display* gui) { gui_ = gui; }

	void objectives(const unsigned int team_num);
	void show_statistics();
	void unit_list();
	void status_table();
	void save_game(const std::string& message, gui::DIALOG_TYPE dialog_type);
	void load_game();
	void preferences();
	void show_chat_log();
	void show_help();
	void speak();
	void recruit(const bool browse, const unsigned int team_num, const gamemap::location& last_hex);
	void repeat_recruit(const unsigned int team_num, const gamemap::location& last_hex);
	void recall(const unsigned int team_num, const gamemap::location& last_hex);
	void undo(const unsigned int team_num, mouse_handler& mousehandler);
	void redo(const unsigned int team_num, mouse_handler& mousehandler);
	void show_enemy_moves(bool ignore_units, const unsigned int team_num);
	void toggle_shroud_updates(const unsigned int team_num);
	void update_shroud_now(const unsigned int team_num);
	bool end_turn(const unsigned int team_num);
	void goto_leader(const unsigned int team_num);
	void unit_description(mouse_handler& mousehandler);
	void rename_unit(mouse_handler& mousehandler);
	void create_unit(mouse_handler& mousehandler);
	void change_unit_side(mouse_handler& mousehandler);
	void label_terrain(mouse_handler& mousehandler);
	void clear_labels();
	void continue_move(mouse_handler& mousehandler, const unsigned int team_num);
	void toggle_grid();
	void unit_hold_position(mouse_handler& mousehandler, const unsigned int team_num);
	void end_unit_turn(mouse_handler& mousehandler, const unsigned int team_num);
	void search();
	void user_command();

	unit_map::iterator current_unit(mouse_handler& mousehandler);
	unit_map::const_iterator current_unit(const mouse_handler& mousehandler) const;
	void move_unit_to_loc(const unit_map::const_iterator& ui, const gamemap::location& target, 
		bool continue_move, const unsigned int team_num, mouse_handler& mousehandler);
	void do_speak();
	void do_search(const std::string& new_search);
	void do_command(const std::string& str, const unsigned int team_num, mouse_handler& mousehandler);
	void clear_undo_stack(const unsigned int team_num);
protected:
	void add_chat_message(const std::string& speaker, int side, const std::string& message, display::MESSAGE_TYPE type=display::MESSAGE_PRIVATE);
	void send_chat_message(const std::string& message, bool allies_only=false);
private:
	//void do_speak(const std::string& message, bool allies_only);
	void do_recruit(const std::string& name, const unsigned int team_num, const gamemap::location& last_hex);
	std::vector<std::string> create_unit_table(const statistics::stats::str_int_map& m);
	void write_game_snapshot(config& start) const;
	bool has_friends() const;
	bool clear_shroud(const unsigned int team_num);
	void change_side_controller(const std::string& side, const std::string& player, bool orphan_side=false);

	display* gui_;
	unit_map& units_;
	std::vector<team>& teams_;
	const config& level_;
	const game_data& gameinfo_;
	const gamemap& map_;
	const config& game_config_;
	const gamestatus& status_;
	game_state& gamestate_;

	undo_list& undo_stack_;
	undo_list& redo_stack_;
	gui::floating_textbox textbox_info_;
	std::string last_search_;
	gamemap::location last_search_hit_;

	std::string last_recruit_;
};

}
#endif
