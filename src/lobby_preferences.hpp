/* $Id$ */
/*
   Copyright (C) 2009 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LOBBY_PREFERENCES_HPP_INCLUDED
#define LOBBY_PREFERENCES_HPP_INCLUDED

#include <string>

namespace preferences {

	bool sort_list();
	void _set_sort_list(bool show);

	bool iconize_list();
	void _set_iconize_list(bool show);

	bool whisper_friends_only();
	void set_whisper_friends_only(bool v);

	bool auto_open_whisper_windows();
	void set_auto_open_whisper_windows(bool v);

	bool playerlist_sort_relation();
	void set_playerlist_sort_relation(bool v);

	bool playerlist_sort_name();
	void set_playerlist_sort_name(bool v);

	bool playerlist_single_group();
	void set_playerlist_single_group(bool v);

	bool filter_lobby();
	void set_filter_lobby(bool value);

	bool fi_invert();
	void set_fi_invert(bool value);

	bool fi_vacant_slots();
	void set_fi_vacant_slots(bool value);

	bool fi_friends_in_game();
	void set_fi_friends_in_game(bool value);

	std::string fi_text();
	void set_fi_text(const std::string& search_string);
} //end namespace preferences


#endif
