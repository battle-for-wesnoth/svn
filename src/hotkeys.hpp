/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef HOTKEYS_HPP_INCLUDED
#define HOTKEYS_HPP_INCLUDED

#include "config.hpp"
#include "display.hpp"
#include "events.hpp"
#include "key.hpp"

//the hotkey system allows hotkey definitions to be loaded from
//configuration objects, and then detect if a keyboard event
//refers to a hotkey command being executed.
namespace hotkey {

enum HOTKEY_COMMAND { HOTKEY_CYCLE_UNITS, HOTKEY_END_UNIT_TURN, HOTKEY_LEADER,
                      HOTKEY_UNDO, HOTKEY_REDO,
                      HOTKEY_ZOOM_IN, HOTKEY_ZOOM_OUT, HOTKEY_ZOOM_DEFAULT,
                      HOTKEY_FULLSCREEN, HOTKEY_ACCELERATED,
                      HOTKEY_TERRAIN_TABLE, HOTKEY_ATTACK_RESISTANCE,
                      HOTKEY_UNIT_DESCRIPTION, HOTKEY_RENAME_UNIT, HOTKEY_SAVE_GAME,
                      HOTKEY_RECRUIT, HOTKEY_REPEAT_RECRUIT, HOTKEY_RECALL, HOTKEY_ENDTURN,
                      HOTKEY_TOGGLE_GRID, HOTKEY_STATUS_TABLE, HOTKEY_MUTE,
					  HOTKEY_SPEAK, HOTKEY_CREATE_UNIT, HOTKEY_PREFERENCES,
					  HOTKEY_OBJECTIVES, HOTKEY_UNIT_LIST, HOTKEY_STATISTICS, HOTKEY_QUIT_GAME,
                      HOTKEY_LABEL_TERRAIN, HOTKEY_SHOW_ENEMY_MOVES, HOTKEY_BEST_ENEMY_MOVES,
					  HOTKEY_TOGGLE_SHROUD, HOTKEY_UPDATE_SHROUD,

					  //editing specific commands
		      HOTKEY_EDIT_SET_TERRAIN,
		      HOTKEY_EDIT_QUIT, HOTKEY_EDIT_SAVE_MAP, 
		      HOTKEY_EDIT_SAVE_AS, HOTKEY_EDIT_SET_START_POS,
		      HOTKEY_EDIT_NEW_MAP, HOTKEY_EDIT_LOAD_MAP,
					  HOTKEY_NULL };

struct hotkey_item {
	explicit hotkey_item(const config& cfg);

	HOTKEY_COMMAND action;
	int keycode;
	bool alt, ctrl, shift;
	mutable bool lastres;
};
	
//function to load a hotkey configuration object. hotkey configuration objects
//are a list of nodes that look like:
//[hotkey]
//command="cmd"
//key="k"
//ctrl=(yes|no)
//alt=(yes|no)
//shift=(yes|no)
//[/hotkey]
//where "cmd" is a command name, and "k" is a key. see hotkeys.cpp for the
//valid command names.
void add_hotkeys(config& cfg, bool overwrite);

void change_hotkey(hotkey_item& item);

void save_hotkeys(config& cfg);

// return list of current hotkeys
std::vector<hotkey_item>& get_hotkeys();

// Return "name" of hotkey for example :"ctrl+alt+g"
std::string get_hotkey_name(hotkey_item item);

std::string command_to_string(const HOTKEY_COMMAND &command);
HOTKEY_COMMAND string_to_command(const std::string& str);

//abstract base class for objects that implement the ability
//to execute hotkey commands.
class command_executor
{
public:
	virtual void cycle_units() = 0;
	virtual void end_turn() = 0;
	virtual void goto_leader() = 0;
	virtual void end_unit_turn() = 0;
	virtual void undo() = 0;
	virtual void redo() = 0;
	virtual void terrain_table() = 0;
	virtual void attack_resistance() = 0;
	virtual void unit_description() = 0;
	virtual void rename_unit() = 0;
	virtual void save_game() = 0;
	virtual void toggle_grid() = 0;
	virtual void status_table() = 0;
	virtual void recall() = 0;
	virtual void recruit() = 0;
	virtual void repeat_recruit() = 0;
	virtual void speak() = 0;
	virtual void create_unit() = 0;
	virtual void preferences() = 0;
	virtual void objectives() = 0;
	virtual void unit_list() = 0;
	virtual void show_statistics() = 0;
	virtual void label_terrain() = 0;
	virtual void show_enemy_moves(bool ignore_units) = 0;
	virtual void toggle_shroud_updates() = 0;
	virtual void update_shroud_now() = 0;

	// Map editor stuff.
	virtual void edit_set_terrain() {}
	virtual void edit_quit() {}
	virtual void edit_new_map() {}
	virtual void edit_load_map() {}
	virtual void edit_save_map() {}
	virtual void edit_save_as() {}
	virtual void edit_set_start_pos() {}

	virtual bool can_execute_command(HOTKEY_COMMAND command) const = 0;
};

//function to be called every time a key event is intercepted. Will
//call the relevant function in executor if the keyboard event is
//not NULL. Also handles some events in the function itself, and so
//is still meaningful to call with executor=NULL
void key_event(display& disp, const SDL_KeyboardEvent& event, command_executor* executor);

void execute_command(display& disp, HOTKEY_COMMAND command, command_executor* executor);

//object which will ensure that basic keyboard events like escape
//are handled properly for the duration of its lifetime
struct basic_handler : public events::handler {
	basic_handler(display* disp);

	void handle_event(const SDL_Event& event);

private:
	display* disp_;
};

}

#endif
