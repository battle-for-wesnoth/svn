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
#include "video.hpp"

#include "SDL.h"

#include <algorithm>
#include <cstdlib>
#include <map>

namespace {

enum HOTKEY_COMMAND { HOTKEY_CYCLE_UNITS, HOTKEY_END_UNIT_TURN, HOTKEY_LEADER,
                      HOTKEY_UNDO, HOTKEY_REDO,
                      HOTKEY_ZOOM_IN, HOTKEY_ZOOM_OUT, HOTKEY_ZOOM_DEFAULT,
                      HOTKEY_FULLSCREEN, HOTKEY_ACCELERATED,
                      HOTKEY_TERRAIN_TABLE, HOTKEY_ATTACK_RESISTANCE,
                      HOTKEY_UNIT_DESCRIPTION, HOTKEY_SAVE_GAME,
                      HOTKEY_RECRUIT, HOTKEY_REPEAT_RECRUIT, HOTKEY_RECALL, HOTKEY_ENDTURN,
                      HOTKEY_TOGGLE_GRID, HOTKEY_STATUS_TABLE,
                      HOTKEY_NULL };

HOTKEY_COMMAND string_to_command(const std::string& str)
{
	static std::map<std::string,HOTKEY_COMMAND> m;
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
		m.insert(val("resistance",HOTKEY_ATTACK_RESISTANCE));
		m.insert(val("terraintable",HOTKEY_TERRAIN_TABLE));
		m.insert(val("describeunit",HOTKEY_UNIT_DESCRIPTION));
		m.insert(val("save",HOTKEY_SAVE_GAME));
		m.insert(val("recruit",HOTKEY_RECRUIT));
		m.insert(val("repeatrecruit",HOTKEY_REPEAT_RECRUIT));
		m.insert(val("recall",HOTKEY_RECALL));
		m.insert(val("endturn",HOTKEY_ENDTURN));
		m.insert(val("togglegrid",HOTKEY_TOGGLE_GRID));
		m.insert(val("statustable",HOTKEY_STATUS_TABLE));
	}

	const std::map<std::string,HOTKEY_COMMAND>::const_iterator i = m.find(str);
	if(i == m.end())
		return HOTKEY_NULL;
	else
		return i->second;
}

struct hotkey_item {
	explicit hotkey_item(config& cfg);

	HOTKEY_COMMAND action;
	int keycode;
	bool alt, ctrl, shift;
	mutable bool lastres;
};

hotkey_item::hotkey_item(config& cfg) : lastres(false)
{
	std::map<std::string,std::string>& m = cfg.values;
	action = string_to_command(m["command"]);

	keycode = m["key"].empty() ? 0 : m["key"][0];
	alt = (m["alt"] == "yes");
	ctrl = (m["ctrl"] == "yes");
	shift = (m["shift"] == "yes");
}

bool operator==(const hotkey_item& a, const hotkey_item& b)
{
	return a.keycode == b.keycode && a.alt == b.alt &&
	       a.ctrl == b.ctrl && a.shift == b.shift;
}

bool operator!=(const hotkey_item& a, const hotkey_item& b)
{
	return !(a == b);
}

std::vector<hotkey_item> hotkeys;

}

struct hotkey_pressed {
	hotkey_pressed(const SDL_KeyboardEvent& event);

	bool operator()(const hotkey_item& hk) const;

private:
	int keycode_;
	bool shift_, ctrl_, alt_;
};

hotkey_pressed::hotkey_pressed(const SDL_KeyboardEvent& event)
       : keycode_(event.keysym.sym), shift_(event.keysym.mod&KMOD_SHIFT),
         ctrl_(event.keysym.mod&KMOD_CTRL), alt_(event.keysym.mod&KMOD_ALT)
{}

bool hotkey_pressed::operator()(const hotkey_item& hk) const
{
	return hk.keycode == keycode_ && shift_ == hk.shift &&
	       ctrl_ == hk.ctrl && alt_ == hk.alt;
}

namespace {

void add_hotkey(config& cfg)
{
	const hotkey_item new_hotkey(cfg);
	const std::vector<hotkey_item>::iterator i =
	               std::find(hotkeys.begin(),hotkeys.end(),new_hotkey);
	if(i != hotkeys.end()) {
		*i = new_hotkey;
	} else {
		hotkeys.push_back(new_hotkey);
	}
}

}

namespace hotkey {

basic_handler::basic_handler(display& disp) : disp_(disp) {}

void basic_handler::handle_event(const SDL_Event& event)
{
	if(event.type == SDL_KEYDOWN && !gui::in_dialog()) {
		key_event(disp_,event.key,NULL);
	}
}

void add_hotkeys(config& cfg)
{
	std::vector<config*>& children = cfg.children["hotkey"];
	for(std::vector<config*>::iterator i = children.begin();
	    i != children.end(); ++i) {
		add_hotkey(**i);
	}
}

void key_event(display& disp, const SDL_KeyboardEvent& event,
               command_executor* executor)
{
	const double zoom_amount = 5.0;

	if(event.keysym.sym == SDLK_ESCAPE) {
		const int res = gui::show_dialog(disp,NULL,"",
		                   string_table["quit_message"],gui::YES_NO);
		if(res == 0) {
			throw end_level_exception(QUIT);
		} else {
			return;
		}
	}

	const std::vector<hotkey_item>::iterator i =
	        std::find_if(hotkeys.begin(),hotkeys.end(),hotkey_pressed(event));

	if(i == hotkeys.end())
		return;

	switch(i->action) {
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
		case HOTKEY_TERRAIN_TABLE:
			if(executor)
				executor->terrain_table();
			break;
		case HOTKEY_ATTACK_RESISTANCE:
			if(executor)
				executor->attack_resistance();
			break;
		case HOTKEY_UNIT_DESCRIPTION:
			if(executor)
				executor->unit_description();
			break;
		case HOTKEY_SAVE_GAME:
			if(executor)
				executor->save_game();
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
		case HOTKEY_REPEAT_RECRUIT:
			if(executor)
				executor->repeat_recruit();
			break;
		default:
			break;
	}
}

}
