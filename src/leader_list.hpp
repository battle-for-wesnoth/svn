/* $Id$ */
/*
   Copyright (C) 
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LEADER_LIST_HPP_INCLUDED
#define LEADER_LIST_HPP_INCLUDED

#include "config.hpp"
#include "widgets/combo.hpp"
#include "gamestatus.hpp"
#include <string>

class leader_list_manager
{
public:
	leader_list_manager(const config::child_list& side_list, const game_data* data,
			gui::combo* combo = NULL);

	void set_combo(gui::combo* combo);
	void update_leader_list(int side);
	std::string get_leader() const;
	void set_leader(const std::string& leader);
	bool is_leader_ok(std::string leader);

private:
	std::vector<std::string> leaders_;
	config::child_list side_list_;
	const game_data* data_;
	gui::combo* combo_;

};

#endif

