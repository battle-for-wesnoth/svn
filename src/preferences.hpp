/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file preferences.hpp */

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

// Only there temporary
#ifdef USE_TINY_GUI
const int min_allowed_width = 320;
const int min_allowed_height = 240;
#elif defined USE_SMALL_GUI
const int min_allowed_width = 800;
const int min_allowed_height = 480;
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

	void write_preferences();

	// Low-level, should be seen only by preferences_display ?
	void set(const std::string key, std::string value);
	void set_child(const std::string& key, const config& val);
	config* get_child(const std::string& key);
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

	bool idle_anim();
	void _set_idle_anim(const bool ison);

	int idle_anim_rate();
	void _set_idle_anim_rate(const int rate);

	const std::string& language();
	void set_language(const std::string& s);

	bool adjust_gamma();
	void _set_adjust_gamma(bool val);

	int gamma();
	void _set_gamma(int gamma);

	// Don't rename it to sound() because of a gcc-3.3 branch bug,
	// which will cause it to conflict with the sound namespace.
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

	// Proxies for preferences_dialog
	void load_hotkeys();
	void save_hotkeys();

	void add_alias(const std::string& alias, const std::string& command);
	config* get_alias();

	bool use_colour_cursors();
	void _set_colour_cursors(bool value);

	int scroll_speed();
	void set_scroll_speed(const int scroll);
	
	bool mouse_scroll_enabled();
	void enable_mouse_scroll(bool value);

	/** Set editor red tint level. */
	void set_editor_r(int value);
	/** Set editor green tint level. */
	void set_editor_g(int value);
	/** Set editor blue tint level. */
	void set_editor_b(int value);
	
	/** Get editor red tint level. */
	int editor_r(void);
	/** Get editor green tint level. */
	int editor_g(void);
	/** Get editor blue tint level. */
	int editor_b(void);

	int draw_delay();
	void set_draw_delay(int value);

	bool animate_map();
	bool show_standing_animations();

	bool show_fps();
	void set_show_fps(bool value);

	bool grid();
	void _set_grid(bool ison);
} // end namespace preferences

#endif
