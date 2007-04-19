/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

class config;

#include "color_range.hpp"

#include <string>
#include <vector>
#include <map>

//basic game configuration information is here.
namespace game_config
{
	extern int base_income;
	extern int village_income;
	extern int poison_amount;
	extern int rest_heal_amount;
	extern int recall_cost;
	extern int kill_experience;
	extern int lobby_refresh;
	extern const std::string version;
	extern const std::string svnrev;

	extern bool debug, editor, ignore_replay_errors, mp_debug, exit_at_end, no_delay, disable_autosave;

	extern std::string path;

	extern std::string game_icon, game_title, game_logo, title_music,
	  moved_ball_image, unmoved_ball_image, partmoved_ball_image,
	  enemy_ball_image, ally_ball_image, energy_image,
	  flag_image, cross_image,
	  terrain_mask_image, observer_image,
	  checked_menu_image, unchecked_menu_image, wml_menu_image, level_image, ellipsis_image;

  	extern std::string flag_rgb;

	extern std::vector<std::string> foot_left_nw,foot_left_n,foot_right_nw,foot_right_n;

	extern int title_logo_x, title_logo_y, title_buttons_x, title_buttons_y, title_buttons_padding, title_tip_x, title_tip_width, title_tip_padding;

     extern std::map<std::string, color_range> team_rgb_range;
     extern std::map<std::string, std::string> team_rgb_name;
	
	extern std::map<std::string, std::vector<Uint32> > team_rgb_colors;
	namespace sounds {
		extern const std::string turn_bell, receive_message, user_arrive, user_leave;
		extern const std::string button_press, checkbox_release, slider_adjust,
			menu_expand, menu_contract, menu_select;
	}
	
        void load_config(const config* cfg);
        
     const void add_color_info(const config& v);
	const std::vector<Uint32>& tc_info(const std::string& name);
}

#endif
