/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef PREFERENCES_HPP_INCLUDED
#define PREFERENCES_HPP_INCLUDED

class config;
class display;

#include "game_config.hpp"
#include "terrain_translation.hpp"
#include "config.hpp"

#include <string>
#include <utility>
#include <set>

// only there temporary
#ifdef USE_TINY_GUI
const int min_allowed_width = 320;
const int min_allowed_height = 240;
#else
const int min_allowed_width = 800;
const int min_allowed_height = 600;
#endif

namespace preferences {

	struct base_manager
	{
		base_manager();
		~base_manager();
	};

	// low-level, should be seen only by preferences_display ?
	void set(std::string key, std::string value);
	const std::string get(const std::string key);
	void erase(const std::string key);

	void disable_preferences_save();

	config* get_prefs();

	bool fullscreen();
	void _set_fullscreen(bool ison);

	std::pair<int,int> resolution();
	void _set_resolution(const std::pair<int,int>& res);

	bool turbo();
	void _set_turbo(bool ison);

	double turbo_speed();
	void save_turbo_speed(const double speed);

	const std::string& language();
	void set_language(const std::string& s);

	// don't rename it to sound() because of a gcc-3.3 branch bug
	// which will cause it to conflict with the sound namespace
	bool sound_on();
	bool set_sound(bool ison);

	unsigned int sample_rate();
	void save_sample_rate(const unsigned int rate);

	size_t sound_buffer_size();
	void save_sound_buffer_size(const size_t size);

	int sound_volume();
	void set_sound_volume(int vol);

	int bell_volume();
	void set_bell_volume(int vol);

	int UI_volume();
	void set_UI_volume(int vol);

	bool music_on();
	bool set_music(bool ison);

	int music_volume();
	void set_music_volume(int vol);

	bool turn_bell();
	bool set_turn_bell(bool ison);

	bool UI_sound_on();
	bool set_UI_sound(bool ison);

	bool message_bell();

	// proxies for preferences_dialog
	void load_hotkeys();
	void save_hotkeys();

	bool use_colour_cursors();
	void _set_colour_cursors(bool value);

	int scroll_speed();
	void set_scroll_speed(int scroll);

	int draw_delay();
	void set_draw_delay(int value);

	bool show_fps();
	void set_show_fps(bool value);

	bool grid();
	void _set_grid(bool ison);
}

#endif
