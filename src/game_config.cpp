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
#include "game_config.hpp"

#include <cstdlib>

namespace game_config
{
	int base_income = 2;
	int tower_income = 1;
	int heal_amount = 4;
	int healer_heals_per_turn = 8;
	int cure_amount = 8;
	int curer_heals_per_turn = 18;
	int rest_heal_amount= 2;
	int recall_cost = 20;
	int kill_experience = 8;
	const std::string version = "0.7";
	bool debug = false;

	std::string game_icon, game_title, game_logo, title_music;
	int title_logo_x = 0, title_logo_y = 0, title_buttons_x = 0, title_buttons_y = 0, title_buttons_padding = 0;

	std::string missile_n_image, missile_ne_image;

	std::string map_image = "misc/map.png";
	std::string rightside_image = "misc/rightside.png";
	std::string rightside_image_bot = "misc/rightside-bottom.png";

	std::string moved_energy_image = "moved-energy.png";
	std::string unmoved_energy_image = "unmoved-energy.png";
	std::string partmoved_energy_image = "partmoved-energy.png";
	std::string enemy_energy_image = "enemy-energy.png";
	std::string ally_energy_image = "ally-energy.png";

	std::string dot_image = "misc/dot.png";
	std::string cross_image = "misc/cross.png";

	std::string foot_left_nw, foot_left_n, foot_right_nw, foot_right_n;

#ifdef WESNOTH_PATH
	std::string path = WESNOTH_PATH;
#else
	std::string path = "";
#endif

	void load_config(const config* cfg)
	{
		if(cfg == NULL)
			return;

		const config& v = *cfg;

		base_income = atoi(v["base_income"].c_str());
		tower_income = atoi(v["village_income"].c_str());
		heal_amount = atoi(v["heal_amount"].c_str());
		healer_heals_per_turn = atoi(v["healer_heals_per_turn"].c_str());
		cure_amount = atoi(v["cure_amount"].c_str());
		curer_heals_per_turn = atoi(v["curer_heals_per_turn"].c_str());
		rest_heal_amount = atoi(v["rest_heal_amount"].c_str());
		recall_cost = atoi(v["recall_cost"].c_str());
		kill_experience = atoi(v["kill_experience"].c_str());

		game_icon = v["icon"];
		game_title = v["title"];
		game_logo = v["logo"];
		title_music = v["title_music"];

		title_logo_x = atoi(v["logo_x"].c_str());
		title_logo_y = atoi(v["logo_y"].c_str());
		title_buttons_x = atoi(v["buttons_x"].c_str());
		title_buttons_y = atoi(v["buttons_y"].c_str());
		title_buttons_padding = atoi(v["buttons_padding"].c_str());

		map_image = v["map_image"];
		rightside_image = v["sidebar_image"];
		rightside_image_bot = v["sidebar_image_bottom"];

		moved_energy_image = v["moved_energy_image"];
		unmoved_energy_image = v["unmoved_energy_image"];
		partmoved_energy_image = v["partmoved_energy_image"];
		enemy_energy_image = v["enemy_energy_image"];
		ally_energy_image = v["ally_energy_image"];

		cross_image = v["cross_image"];
		dot_image = v["dot_image"];

		foot_left_nw = v["footprint_left_nw"];
		foot_left_n = v["footprint_left_n"];
		foot_right_nw = v["footprint_right_nw"];
		foot_right_n = v["footprint_right_n"];

		missile_n_image = v["missile_n_image"];
		missile_ne_image = v["missile_ne_image"];
	}
}
