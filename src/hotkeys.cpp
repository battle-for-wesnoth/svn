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

#include "config.hpp"
#include "events.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "playlevel.hpp"
#include "preferences.hpp"
#include "show_dialog.hpp"
#include "util.hpp"
#include "video.hpp"

#include "SDL.h"

#include <algorithm>
#include <cstdlib>
#include <map>

namespace hotkey {

static std::map<std::string,HOTKEY_COMMAND> m;
	
	
HOTKEY_COMMAND string_to_command(const std::string& str)
{
	if(m.empty()) {
		typedef std::pair<std::string,HOTKEY_COMMAND> val;
		m.insert(val("cycle",HOTKEY_CYCLE_UNITS));
		m.insert(val("endunitturn",HOTKEY_END_UNIT_TURN));
		m.insert(val("leader",HOTKEY_LEADER));
		m.insert(val("undo",HOTKEY_UNDO));
		m.insert(val("redo",HOTKEY_REDO));
		m.insert(val("zoomin",HOTKEY_ZOOM_IN));
		m.insert(val("zoomout",HOTKEY_ZOOM_OUT));
		m.insert(val("zoomdefault",HOTKEY_ZOOM_DEFAULT));
		m.insert(val("fullscreen",HOTKEY_FULLSCREEN));
		m.insert(val("accelerated",HOTKEY_ACCELERATED));
		m.insert(val("describeunit",HOTKEY_UNIT_DESCRIPTION));
		m.insert(val("renameunit",HOTKEY_RENAME_UNIT));
		m.insert(val("save",HOTKEY_SAVE_GAME));
		m.insert(val("load",HOTKEY_LOAD_GAME));
		m.insert(val("recruit",HOTKEY_RECRUIT));
		m.insert(val("repeatrecruit",HOTKEY_REPEAT_RECRUIT));
		m.insert(val("recall",HOTKEY_RECALL));
		m.insert(val("endturn",HOTKEY_ENDTURN));
		m.insert(val("togglegrid",HOTKEY_TOGGLE_GRID));
		m.insert(val("statustable",HOTKEY_STATUS_TABLE));
		m.insert(val("mute",HOTKEY_MUTE));
		m.insert(val("speak",HOTKEY_SPEAK));
		m.insert(val("createunit",HOTKEY_CREATE_UNIT));
		m.insert(val("changeside",HOTKEY_CHANGE_UNIT_SIDE));
		m.insert(val("preferences",HOTKEY_PREFERENCES));
		m.insert(val("objectives",HOTKEY_OBJECTIVES));
		m.insert(val("unitlist",HOTKEY_UNIT_LIST));
		m.insert(val("statistics",HOTKEY_STATISTICS));
		m.insert(val("quit",HOTKEY_QUIT_GAME));
		m.insert(val("labelterrain",HOTKEY_LABEL_TERRAIN));
		m.insert(val("showenemymoves",HOTKEY_SHOW_ENEMY_MOVES));
		m.insert(val("bestenemymoves",HOTKEY_BEST_ENEMY_MOVES));
		m.insert(val("editquit",HOTKEY_EDIT_QUIT));
		m.insert(val("editnewmap",HOTKEY_EDIT_NEW_MAP));
		m.insert(val("editloadmap",HOTKEY_EDIT_LOAD_MAP));
		m.insert(val("editsavemap",HOTKEY_EDIT_SAVE_MAP));
		m.insert(val("editsaveas",HOTKEY_EDIT_SAVE_AS));
		m.insert(val("editsetstartpos",HOTKEY_EDIT_SET_START_POS));
		m.insert(val("editfloodfill",HOTKEY_EDIT_FLOOD_FILL));
		m.insert(val("editfillselection",HOTKEY_EDIT_FILL_SELECTION));
		m.insert(val("editcut",HOTKEY_EDIT_CUT));
		m.insert(val("editcopy",HOTKEY_EDIT_COPY));
		m.insert(val("editpaste",HOTKEY_EDIT_PASTE));
		m.insert(val("editrevert",HOTKEY_EDIT_REVERT));
		m.insert(val("editresize",HOTKEY_EDIT_RESIZE));
		m.insert(val("editflip",HOTKEY_EDIT_FLIP));
		m.insert(val("editselectall",HOTKEY_EDIT_SELECT_ALL));
		m.insert(val("editdraw",HOTKEY_EDIT_DRAW));
		m.insert(val("delayshroud",HOTKEY_DELAY_SHROUD));
		m.insert(val("updateshroud",HOTKEY_UPDATE_SHROUD));
		m.insert(val("continue",HOTKEY_CONTINUE_MOVE));
		m.insert(val("search",HOTKEY_SEARCH));
		m.insert(val("speaktoally",HOTKEY_SPEAK_ALLY));
		m.insert(val("speaktoall",HOTKEY_SPEAK_ALL));
		m.insert(val("help",HOTKEY_HELP));
		m.insert(val("chatlog",HOTKEY_CHAT_LOG));
		m.insert(val("command",HOTKEY_USER_CMD));
	}
	
	const std::map<std::string,HOTKEY_COMMAND>::const_iterator i = m.find(str);
	if(i == m.end())
		return HOTKEY_NULL;
	else
		return i->second;
}
	
std::string command_to_string(const HOTKEY_COMMAND &command)
{
	for(std::map<std::string,HOTKEY_COMMAND>::iterator i = m.begin(); i != m.end(); ++i) {
		if(i->second == command) {
			return i->first;
		}
	}

	std::cerr << "\n command_to_string: No matching command found...";
	return "";
}

std::string command_to_description(const HOTKEY_COMMAND &command)
{
	switch (command) {
	case HOTKEY_CYCLE_UNITS: return _("Next unit");
	case HOTKEY_END_UNIT_TURN: return _("End Unit Turn");
	case HOTKEY_LEADER: return _("Leader");
	case HOTKEY_UNDO: return _("Undo");
	case HOTKEY_REDO: return _("Redo");
	case HOTKEY_ZOOM_IN: return _("Zoom In");
	case HOTKEY_ZOOM_OUT: return _("Zoom Out");
	case HOTKEY_ZOOM_DEFAULT: return _("Default Zoom");
	case HOTKEY_FULLSCREEN: return _("Fullscreen");
	case HOTKEY_ACCELERATED: return _("Accelerated");
	case HOTKEY_UNIT_DESCRIPTION: return _("Unit Description");
	case HOTKEY_RENAME_UNIT: return _("Rename Unit");
	case HOTKEY_SAVE_GAME: return _("Save Game");
	case HOTKEY_LOAD_GAME: return _("Load Game");
	case HOTKEY_RECRUIT: return _("Recruit");
	case HOTKEY_REPEAT_RECRUIT: return _("Repeat Recruit");
	case HOTKEY_RECALL: return _("Recall");
	case HOTKEY_ENDTURN: return _("End Turn");
	case HOTKEY_TOGGLE_GRID: return _("Toggle Grid");
	case HOTKEY_STATUS_TABLE: return _("Status Table");
	case HOTKEY_MUTE: return _("Mute");
	case HOTKEY_SPEAK: return _("Speak");
	case HOTKEY_CREATE_UNIT: return _("Create Unit (Debug!)");
	case HOTKEY_CHANGE_UNIT_SIDE: return _("Change Unit Side (Debug!)");
	case HOTKEY_PREFERENCES: return _("Preferences");
	case HOTKEY_OBJECTIVES: return _("Scenario Objectives");
	case HOTKEY_UNIT_LIST: return _("Unit List");
	case HOTKEY_STATISTICS: return _("Statistics");
	case HOTKEY_QUIT_GAME: return _("Quit Game");
	case HOTKEY_LABEL_TERRAIN: return _("Set Label");
	case HOTKEY_SHOW_ENEMY_MOVES: return _("Show Enemy Moves");
	case HOTKEY_BEST_ENEMY_MOVES: return _("Best Possible Enemy Moves");
	case HOTKEY_EDIT_QUIT: return _("Quit Editor");
	case HOTKEY_EDIT_NEW_MAP: return _("New Map");
	case HOTKEY_EDIT_LOAD_MAP: return _("Load Map");
	case HOTKEY_EDIT_SAVE_MAP: return _("Save Map");
	case HOTKEY_EDIT_SAVE_AS: return _("Save As");
	case HOTKEY_EDIT_SET_START_POS: return _("Set Player Start Position");
	case HOTKEY_EDIT_FLOOD_FILL: return _("Flood Fill");
	case HOTKEY_EDIT_FILL_SELECTION: return _("Fill Selection");
	case HOTKEY_EDIT_CUT: return _("Cut");
	case HOTKEY_EDIT_COPY: return _("Copy");
	case HOTKEY_EDIT_PASTE: return _("Paste");
	case HOTKEY_EDIT_REVERT: return _("Revert from Disk");
	case HOTKEY_EDIT_RESIZE: return _("Resize Map");
	case HOTKEY_EDIT_FLIP: return _("Flip Map");
	case HOTKEY_EDIT_SELECT_ALL: return _("Select All");
	case HOTKEY_EDIT_DRAW: return _("Draw Terrain");
	case HOTKEY_DELAY_SHROUD: return _("Delay Shroud Updates");
	case HOTKEY_UPDATE_SHROUD: return _("Update Shroud Now");
	case HOTKEY_CONTINUE_MOVE: return _("Continue Move");
	case HOTKEY_SEARCH: return _("Find Label or Unit");
	case HOTKEY_SPEAK_ALLY: return _("Speak to Ally");
	case HOTKEY_SPEAK_ALL: return _("Speak to All");
	case HOTKEY_HELP: return _("Help");
	case HOTKEY_CHAT_LOG: return _("View Chat Log");
	default:
	  std::cerr << "\n command_to_description: No matching command found...";
	  return "";
	}
}


hotkey_item::hotkey_item(const config& cfg) : lastres(false)
{
	action = string_to_command(cfg["command"]);

	const std::string& code = cfg["key"];
	if(code.empty()) {
		keycode = 0;
	} else if(code.size() >= 2 && tolower(code[0]) == 'f') {
		const int num = lexical_cast_default<int>(std::string(code.begin()+1,code.end()),1);
		keycode = num + SDLK_F1 - 1;
		std::cerr << "set key to F" << num << " = " << keycode << "\n";
	} else {
		keycode = code[0];
	}
	
	alt = (cfg["alt"] == "yes");
	ctrl = (cfg["ctrl"] == "yes");
	shift = (cfg["shift"] == "yes");
	command = (cfg["cmd"] == "yes");
}

bool operator==(const hotkey_item& a, const hotkey_item& b)
{
	return a.keycode == b.keycode && a.alt == b.alt &&
	       a.ctrl == b.ctrl && a.shift == b.shift && a.command == b.command;
}

bool operator!=(const hotkey_item& a, const hotkey_item& b)
{
	return !(a == b);
}

}

namespace {
std::vector<hotkey::hotkey_item> hotkeys;

}

struct hotkey_pressed {
	//this distinguishes between two modes of operation. If mods are disallowed,
	//then any match must be exact. I.e. "shift+a" does not match "a". If they are allowed,
	//then shift+a will match "a"
	enum ALLOW_MOD_KEYS { DISALLOW_MODS, ALLOW_MODS };
	hotkey_pressed(const SDL_KeyboardEvent& event, ALLOW_MOD_KEYS allow_mods=DISALLOW_MODS);

	bool operator()(const hotkey::hotkey_item& hk) const;

private:
	int keycode_;
	bool shift_, ctrl_, alt_, command_;
	bool mods_;
};

hotkey_pressed::hotkey_pressed(const SDL_KeyboardEvent& event, ALLOW_MOD_KEYS mods)
       : keycode_(event.keysym.sym), shift_(event.keysym.mod&KMOD_SHIFT),
         ctrl_(event.keysym.mod&KMOD_CTRL), alt_(event.keysym.mod&KMOD_ALT),
		 command_(event.keysym.mod&KMOD_LMETA), mods_(mods == ALLOW_MODS)
{}

bool hotkey_pressed::operator()(const hotkey::hotkey_item& hk) const
{
	if(mods_) {
		return hk.keycode == keycode_ && (shift_ == hk.shift || shift_ == true)
		                              && (ctrl_ == hk.ctrl || ctrl_ == true)
									  && (alt_ == hk.alt || alt_ == true);
	} else {
		return hk.keycode == keycode_ && shift_ == hk.shift &&
		       ctrl_ == hk.ctrl && alt_ == hk.alt && command_ == hk.command;
	}
}

namespace {

void add_hotkey(const config& cfg,bool overwrite)
{
	const hotkey::hotkey_item new_hotkey(cfg);
	for(std::vector<hotkey::hotkey_item>::iterator i = hotkeys.begin();
	    i != hotkeys.end(); ++i) {
		if(i->action == new_hotkey.action) {
		  if(overwrite)
			*i = new_hotkey;	
		  return;
		}
	}
	hotkeys.push_back(new_hotkey);
}

}

namespace hotkey {

void change_hotkey(hotkey_item& item)
{
	for(std::vector<hotkey::hotkey_item>::iterator i =hotkeys.begin();
		i!=hotkeys.end();i++)
	{
		if(item.action == i->action)
			*i = item;	
	}
}
	
basic_handler::basic_handler(display* disp, command_executor* exec) : disp_(disp), exec_(exec) {}

void basic_handler::handle_event(const SDL_Event& event)
{
	if(event.type == SDL_KEYDOWN && disp_ != NULL) {

		//if we're in a dialog we only want to handle things that are explicitly handled
		//by the executor. If we're not in a dialog we can call the regular key event handler
		if(!gui::in_dialog()) {
			key_event(*disp_,event.key,exec_);
		} else if(exec_ != NULL) {
			key_event_execute(*disp_,event.key,exec_);
		}
	}
}

void add_hotkeys(config& cfg,bool overwrite)
{
	const config::child_list& children = cfg.get_children("hotkey");
	for(config::child_list::const_iterator i = children.begin(); i != children.end(); ++i) {
	  add_hotkey(**i,overwrite);
	}
}

void save_hotkeys(config& cfg)
{
	const config::child_list children = cfg.get_children("hotkey");
	for(std::vector<hotkey_item>::iterator i = hotkeys.begin(); i != hotkeys.end(); ++i) {
		std::string action_name = command_to_string(i->action);

		config* item = cfg.find_child("hotkey","command",action_name);
		if(item == NULL)
			item = &cfg.add_child("hotkey");

		(*item)["command"] = action_name;

		if(i->keycode >= SDLK_F1 && i->keycode <= SDLK_F12) {
			std::string str = "FF";
			str[1] = '1' + i->keycode - SDLK_F1;
			(*item)["key"] = str;
		} else {
			(*item)["key"] = i->keycode;
		}

		(*item)["alt"] = (i->alt) ? "yes" : "no";
		(*item)["ctrl"] = (i->ctrl) ? "yes" : "no";
		(*item)["shift"] = (i->shift) ? "yes" : "no";
		(*item)["cmd"] = (i->command) ? "yes" : "no";
	}
}

std::vector<hotkey_item>& get_hotkeys()
{
	return hotkeys;
}

std::string get_hotkey_name(hotkey_item i)
{
 	std::stringstream str;			
	if (i.alt)
 	str << "alt+";
	if (i.ctrl)
	str << "ctrl+";
 	if (i.shift)
	str << "shift+";
	str << SDL_GetKeyName(SDLKey(i.keycode));
	return str.str();
}

void key_event(display& disp, const SDL_KeyboardEvent& event, command_executor* executor)
{
	if(event.keysym.sym == SDLK_ESCAPE && disp.in_game()) {
		std::cerr << "escape pressed..showing quit\n";
		const int res = gui::show_dialog(disp,NULL,_("Quit"),_("Do you really want to quit?"),gui::YES_NO);
		if(res == 0) {
			throw end_level_exception(QUIT);
		} else {
			return;
		}
	}

	key_event_execute(disp,event,executor);
}

void key_event_execute(display& disp, const SDL_KeyboardEvent& event, command_executor* executor)
{
	std::vector<hotkey_item>::iterator i = std::find_if(hotkeys.begin(),hotkeys.end(),hotkey_pressed(event));

	if(i == hotkeys.end()) {
		//no matching hotkey was found, but try an in-exact match.
		i = std::find_if(hotkeys.begin(),hotkeys.end(),hotkey_pressed(event,hotkey_pressed::ALLOW_MODS));
	}

	if(i == hotkeys.end()) {
		return;
	}

	execute_command(disp,i->action,executor);
}

void execute_command(display& disp, HOTKEY_COMMAND command, command_executor* executor)
{
	const int zoom_amount = 4;

	if(executor != NULL && executor->can_execute_command(command) == false)
		return;

	switch(command) {
		case HOTKEY_ZOOM_IN:
			disp.zoom(zoom_amount);
			break;
		case HOTKEY_ZOOM_OUT:
			disp.zoom(-zoom_amount);
			break;
		case HOTKEY_ZOOM_DEFAULT:
			disp.default_zoom();
			break;
		case HOTKEY_FULLSCREEN:
			preferences::set_fullscreen(!preferences::fullscreen());
			break;
		case HOTKEY_ACCELERATED:
			preferences::set_turbo(!preferences::turbo());
			break;
		case HOTKEY_MUTE:
			preferences::mute(!preferences::is_muted());
			break;
		case HOTKEY_CYCLE_UNITS:
			if(executor)
				executor->cycle_units();
			break;
		case HOTKEY_ENDTURN:
			if(executor)
				executor->end_turn();
			break;
		case HOTKEY_END_UNIT_TURN:
			if(executor)
				executor->end_unit_turn();
			break;
		case HOTKEY_LEADER:
			if(executor)
				executor->goto_leader();
			break;
		case HOTKEY_UNDO:
			if(executor)
				executor->undo();
			break;
		case HOTKEY_REDO:
			if(executor)
				executor->redo();
			break;
		case HOTKEY_UNIT_DESCRIPTION:
			if(executor)
				executor->unit_description();
			break;
		case HOTKEY_RENAME_UNIT:
			if(executor)
				executor->rename_unit();
			break;
		case HOTKEY_SAVE_GAME:
			if(executor)
				executor->save_game();
			break;
		case HOTKEY_LOAD_GAME:
			if(executor)
				executor->load_game();
			break;
		case HOTKEY_TOGGLE_GRID:
			if(executor)
				executor->toggle_grid();
			break;
		case HOTKEY_STATUS_TABLE:
			if(executor)
				executor->status_table();
			break;
		case HOTKEY_RECALL:
			if(executor)
				executor->recall();
			break;
		case HOTKEY_RECRUIT:
			if(executor)
				executor->recruit();
			break;
		case hotkey::HOTKEY_REPEAT_RECRUIT:
			if(executor)
				executor->repeat_recruit();
			break;
		case HOTKEY_SPEAK:
			if(executor)
				executor->speak();
			break;
		case HOTKEY_SPEAK_ALLY:
			if(executor) {
				preferences::set_message_private(true);
				executor->speak();
			}
			break;
		case HOTKEY_SPEAK_ALL:
			if(executor) {
				preferences::set_message_private(false);
				executor->speak();
			}
			break;
		case HOTKEY_CREATE_UNIT:
			if(executor)
				executor->create_unit();
			break;
		case HOTKEY_CHANGE_UNIT_SIDE:
			if(executor)
				executor->change_unit_side();
			break;
		case HOTKEY_PREFERENCES:
			if(executor)
				executor->preferences();
			break;
		case HOTKEY_OBJECTIVES:
			if(executor)
				executor->objectives();
			break;
		case HOTKEY_UNIT_LIST:
			if(executor)
				executor->unit_list();
			break;
		case HOTKEY_STATISTICS:
			if(executor)
				executor->show_statistics();
			break;
		case HOTKEY_LABEL_TERRAIN:
			if(executor)
				executor->label_terrain();
			break;
		case HOTKEY_SHOW_ENEMY_MOVES:
			if(executor)
				executor->show_enemy_moves(false);
			break;
		case HOTKEY_BEST_ENEMY_MOVES:
			if(executor)
				executor->show_enemy_moves(true);
			break;
		case HOTKEY_DELAY_SHROUD:
			if(executor)
				executor->toggle_shroud_updates();
			break;
		case HOTKEY_UPDATE_SHROUD:
			if(executor)
				executor->update_shroud_now();
			break;
		case HOTKEY_CONTINUE_MOVE:
			if(executor)
				executor->continue_move();
			break;
		case HOTKEY_SEARCH:
			if(executor)
				executor->search();
			break;
		case HOTKEY_QUIT_GAME: {
			if(disp.in_game()) {
				std::cerr << "is in game -- showing quit message\n";
				const int res = gui::show_dialog(disp,NULL,_("Quit"),_("Do you really want to quit?"),gui::YES_NO);
				if(res == 0) {
					throw end_level_exception(QUIT);
				}
			}

			break;
		}
		case HOTKEY_HELP:
			if(executor) {
				executor->show_help();
			}
			break;
		case HOTKEY_CHAT_LOG:
			if(executor) {
				executor->show_chat_log();
			}
			break;
		case HOTKEY_USER_CMD:
			if(executor) {
				executor->user_command();
			}
			break;
		case HOTKEY_EDIT_SET_TERRAIN:
			if(executor)
				executor->edit_set_terrain();
			break;
		case HOTKEY_EDIT_QUIT:
			if(executor)
				executor->edit_quit();
			break;
		 case HOTKEY_EDIT_NEW_MAP:
			if(executor)
				executor->edit_new_map();
			break;
		 case HOTKEY_EDIT_LOAD_MAP:
			if(executor)
				executor->edit_load_map();
			break;
		 case HOTKEY_EDIT_SAVE_MAP:
			if(executor)
				executor->edit_save_map();
			break;
		 case HOTKEY_EDIT_SAVE_AS:
			if(executor)
				executor->edit_save_as();
			break;
		 case HOTKEY_EDIT_SET_START_POS:
			if(executor)
				executor->edit_set_start_pos();
			break;
		 case HOTKEY_EDIT_FLOOD_FILL:
			if(executor)
				executor->edit_flood_fill();
			break;

		 case HOTKEY_EDIT_FILL_SELECTION:
			if(executor)
				executor->edit_fill_selection();
			break;
		 case HOTKEY_EDIT_CUT:
			if(executor)
				executor->edit_cut();
			break;
		 case HOTKEY_EDIT_PASTE:
			if(executor)
				executor->edit_paste();
			break;
		 case HOTKEY_EDIT_COPY:
			if(executor)
				executor->edit_copy();
			break;
		 case HOTKEY_EDIT_REVERT:
			if(executor)
				executor->edit_revert();
			break;
		 case HOTKEY_EDIT_RESIZE:
			if(executor)
				executor->edit_resize();
			break;
		 case HOTKEY_EDIT_FLIP:
			if(executor)
				executor->edit_flip();
			break;
		 case HOTKEY_EDIT_SELECT_ALL:
			if(executor)
				executor->edit_select_all();
			break;
		 case HOTKEY_EDIT_DRAW:
			if(executor)
				executor->edit_draw();
			break;
		default:
			std::cerr << "command_executor: unknown command number " << command << ", ignoring.\n";
			break;
	}
}

std::string command_executor::get_menu_image(hotkey::HOTKEY_COMMAND command) const {
	switch(get_action_state(command)) {
		case ACTION_ON: return game_config::checked_menu_image;
		case ACTION_OFF: return game_config::unchecked_menu_image;
		default: return get_action_image(command);
	}
}

}
