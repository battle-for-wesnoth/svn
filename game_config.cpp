/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "game_config.hpp"

namespace game_config
{
	const int unit_cost = 1;
	const int base_income = 2;
	const int tower_income = 2;
	const int heal_amount = 8;
	const int healer_heals_per_turn = 12;
	const int recall_cost = 20;
	const std::string version = "0.4.4";
	bool debug = false;
}
