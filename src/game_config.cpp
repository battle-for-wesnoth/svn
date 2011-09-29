/* $Id$ */
/*
   Copyright (C) 2003 - 2011 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"
#include "game_config.hpp"

#include "color_range.hpp"
#include "config.hpp"
#include "foreach.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "util.hpp"
#include "version.hpp"
#include "wesconfig.h"
#include "serialization/string_utils.hpp"
#ifdef HAVE_REVISION
#include "revision.hpp"
#endif /* HAVE_REVISION */

static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

namespace game_config
{
	int base_income = 2;
	int village_income = 1;
	int poison_amount= 8;
	int rest_heal_amount= 2;
	int recall_cost = 20;
	int kill_experience = 8;
	unsigned lobby_network_timer = 100;
	unsigned lobby_refresh = 4000;
	const int gold_carryover_percentage = 80;
	const std::string version = VERSION;
#ifdef REVISION
	const std::string revision = VERSION " (" REVISION ")";
#else
	const std::string revision = VERSION;
#endif
	std::string wesnoth_program_dir;
	bool debug = false, editor = false, ignore_replay_errors = false, mp_debug = false, exit_at_end = false, new_syntax = false, no_delay = false, small_gui = false, disable_autosave = false;

	int cache_compression_level = 6;

	int title_logo_x = 0, title_logo_y = 0, title_buttons_x = 0, title_buttons_y = 0, title_buttons_padding = 0,
	    title_tip_x = 0, title_tip_width = 0, title_tip_padding = 0;

	std::string title_music,
			lobby_music,
			default_victory_music,
			default_defeat_music;

	namespace images {
	n_token::t_token game_title,
			// orbs and hp/xp bar
			moved_orb,
			unmoved_orb,
			partmoved_orb,
			enemy_orb,
			ally_orb,
			energy,
			// flags
			flag,
			flag_icon,
			// hex overlay
			terrain_mask,
			grid_top,
			grid_bottom,
			mouseover,
			selected,
			editor_brush,
			unreachable,
			linger,
			// GUI elements
			observer,
			tod_bright,
			tod_dark,
			///@todo de-hardcode this
			checked_menu = n_token::t_token( "buttons/checkbox-pressed.png"),
			unchecked_menu = n_token::t_token( "buttons/checkbox.png"),
			wml_menu = n_token::t_token( "buttons/WML-custom.png"),
			level,
			ellipsis,
			missing;
	} //images

	std::string shroud_prefix, fog_prefix;

	std::string flag_rgb;
	std::vector<Uint32> red_green_scale;
	std::vector<Uint32> red_green_scale_text;

	std::vector<Uint32> blue_white_scale;
	std::vector<Uint32> blue_white_scale_text;

	double hp_bar_scaling = 0.666;
	double xp_bar_scaling = 0.5;

	double hex_brightening = 1.5;
	double hex_semi_brightening = 1.25;

	std::vector<std::string> foot_speed_prefix;
	std::string foot_teleport_enter;
	std::string foot_teleport_exit;

	t_team_rgb_range team_rgb_range;
	std::map<std::string, t_string > team_rgb_name;

	std::map<std::string, std::vector<Uint32> > team_rgb_colors;

	const version_info wesnoth_version(VERSION);
	const version_info min_savegame_version(MIN_SAVEGAME_VERSION);
	const std::string  test_version("test");

	const std::string observer_team_name = "observer";

	const size_t max_loop = 65536;

	namespace sounds {
		const std::string turn_bell = "bell.wav",
		timer_bell = "timer.wav",
		receive_message = "chat-1.ogg,chat-2.ogg,chat-3.ogg",
		receive_message_highlight = "chat-highlight.ogg",
		receive_message_friend = "chat-friend.ogg",
		receive_message_server = "receive.wav",
		user_arrive = "arrive.wav",
		user_leave = "leave.wav",
		game_user_arrive = "join.wav",
		game_user_leave = "leave.wav";

		const std::string button_press = "button.wav",
		checkbox_release = "checkbox.wav",
		slider_adjust = "slider.wav",
		menu_expand = "expand.wav",
		menu_contract = "contract.wav",
		menu_select = "select.wav";
	}



#ifdef __AMIGAOS4__
	std::string path = "PROGDIR:";
#else
#ifdef WESNOTH_PATH
	std::string path = WESNOTH_PATH;
#else
	std::string path = "";
#endif
#endif

	std::string preferences_dir = "";

	std::vector<server_info> server_list;

	void load_config(const config &v)
	{
		base_income = v["base_income"].to_int(2);
		village_income = v["village_income"].to_int(1);
		poison_amount = v["poison_amount"].to_int(8);
		rest_heal_amount = v["rest_heal_amount"].to_int(2);
		recall_cost = v["recall_cost"].to_int(20);
		kill_experience = v["kill_experience"].to_int(8);
		lobby_refresh = v["lobby_refresh"].to_int(2000);

		title_music = v["title_music"].str();
		lobby_music = v["lobby_music"].str();
		default_victory_music = v["default_victory_music"].str();
		default_defeat_music = v["default_defeat_music"].str();

		if(const config &i = v.child("images")){
			using namespace game_config::images;
			game_title = i["game_title"].token();

			moved_orb = i["moved_orb"].token();
			unmoved_orb = i["unmoved_orb"].token();
			partmoved_orb = i["partmoved_orb"].token();
			enemy_orb = i["enemy_orb"].token();
			ally_orb = i["ally_orb"].token();
			energy = i["energy"].token();

			flag = i["flag"].token();
			flag_icon = i["flag_icon"].token();

			terrain_mask = i["terrain_mask"].token();
			grid_top = i["grid_top"].token();
			grid_bottom = i["grid_bottom"].token();
			mouseover = i["mouseover"].token();
			selected = i["selected"].token();
			editor_brush = i["editor_brush"].token();
			unreachable = i["unreachable"].token();
			linger = i["linger"].token();

			observer = i["observer"].token();
			tod_bright = i["tod_bright"].token();
			tod_dark = i["tod_dark"].token();
			level = i["level"].token();
			ellipsis = i["ellipsis"].token();
			missing = i["missing"].token();
		} // images

		hp_bar_scaling = v["hp_bar_scaling"].to_double(0.666);
		xp_bar_scaling = v["xp_bar_scaling"].to_double(0.5);
		hex_brightening = v["hex_brightening"].to_double(1.5);
		hex_semi_brightening = v["hex_semi_brightening"].to_double(1.25);

		foot_speed_prefix = utils::split(v["footprint_prefix"]);
		foot_teleport_enter = v["footprint_teleport_enter"].str();
		foot_teleport_exit = v["footprint_teleport_exit"].str();

		shroud_prefix = v["shroud_prefix"].str();
		fog_prefix  = v["fog_prefix"].str();

		add_color_info(v);

		if (const config::attribute_value *a = v.get("flag_rgb"))
			flag_rgb = a->str();

		red_green_scale = string2rgb(v["red_green_scale"]);
		if (red_green_scale.empty()) {
			red_green_scale.push_back(0x00FFFF00);
		}
		red_green_scale_text = string2rgb(v["red_green_scale_text"]);
		if (red_green_scale_text.empty()) {
			red_green_scale_text.push_back(0x00FFFF00);
		}

		blue_white_scale = string2rgb(v["blue_white_scale"]);
		if (blue_white_scale.empty()) {
			blue_white_scale.push_back(0x00FFFFFF);
		}
		blue_white_scale_text = string2rgb(v["blue_white_scale_text"]);
		if (blue_white_scale_text.empty()) {
			blue_white_scale_text.push_back(0x00FFFFFF);
		}

		server_list.clear();
		foreach (const config &server, v.child_range("server"))
		{
			server_info sinf;
			sinf.name = server["name"].str();
			sinf.address = server["address"].str();
			server_list.push_back(sinf);
		}
	}

	void add_color_info(const config &v)
	{
		foreach (const config &teamC, v.child_range("color_range")) {
			const config::attribute_value *a1 = teamC.get("id"),
				*a2 = teamC.get("rgb");
			if (!a1 || !a2) continue;
			std::string const & id = *a1;
			config::attribute_value const & idt = *a1;
			std::vector<Uint32> temp = string2rgb(*a2);
			team_rgb_range.insert(std::make_pair(n_token::t_token(id), color_range(temp)));
			team_rgb_name[id] = teamC["name"].token();
			//generate palette of same name;
			std::vector<Uint32> tp = palette(team_rgb_range[idt.token()]);
			if (tp.empty()) continue;
			team_rgb_colors.insert(std::make_pair(id,tp));
			//if this is being used, output log of palette for artists use.
			DBG_NG << "color palette creation:\n";
			std::ostringstream str;
			str << id << " = ";
			for (std::vector<Uint32>::const_iterator r = tp.begin(),
			     r_end = tp.end(), r_beg = r; r != r_end; ++r)
			{
				int red = ((*r) & 0x00FF0000) >> 16;
				int green = ((*r) & 0x0000FF00) >> 8;
				int blue = ((*r) & 0x000000FF);
				if (r != r_beg) str << ',';
				str << red << ',' << green << ',' << blue;
			}
			DBG_NG << str.str() << '\n';
		}

		foreach (const config &cp, v.child_range("color_palette"))
		{
			foreach (const config::attribute &rgb, cp.attribute_range())
			{
				try {
					team_rgb_colors.insert(std::make_pair(rgb.first, string2rgb(rgb.second)));
				} catch(bad_lexical_cast&) {
					//throw config::error(_("Invalid team color: ") + rgb_it->second);
					ERR_NG << "Invalid team color: " << rgb.second << "\n";
				}
			}
		}
	}

	const color_range& color_info(const n_token::t_token& name)
	{
		t_team_rgb_range::const_iterator i = team_rgb_range.find(name);
		if(i == team_rgb_range.end()) {
			try {
				team_rgb_range.insert(std::make_pair(name,color_range(string2rgb(name))));
				return color_info(name);
			} catch(bad_lexical_cast&) {
				//ERR_NG << "Invalid color range: " << name;
				//return color_info();
				throw config::error(_("Invalid color range: ") + name);
			}
		}
		return i->second;
	}

	const std::vector<Uint32>& tc_info(const std::string& name)
	{
		std::map<std::string, std::vector<Uint32> >::const_iterator i = team_rgb_colors.find(name);
		if(i == team_rgb_colors.end()) {
			try {
				team_rgb_colors.insert(std::make_pair(name,string2rgb(name)));
				return tc_info(name);
			} catch(bad_lexical_cast&) {
				static std::vector<Uint32> stv;
				//throw config::error(_("Invalid team color: ") + name);
				ERR_NG << "Invalid team color: " << name << "\n";
				return stv;
			}
		}
		return i->second;
	}

	Uint32 red_to_green(int val, bool for_text){
		const std::vector<Uint32>& color_scale =
				for_text ? red_green_scale_text : red_green_scale;
		val = std::max<int>(0, std::min<int>(val, 100));
		int lvl = (color_scale.size()-1) * val / 100;
		return color_scale[lvl];
	}

	Uint32 blue_to_white(int val, bool for_text){
		const std::vector<Uint32>& color_scale =
				for_text ? blue_white_scale_text : blue_white_scale;
		val = std::max<int>(0, std::min<int>(val, 100));
		int lvl = (color_scale.size()-1) * val / 100;
		return color_scale[lvl];
	}

} // game_config
