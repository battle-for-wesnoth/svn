/* $Id$ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
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
	bool debug = false, editor = false, ignore_replay_errors = false, mp_debug = false, exit_at_end = false, no_delay = false, small_gui = false, disable_autosave = false;

	int cache_compression_level = 6;

	std::string game_icon = "wesnoth-icon-small.png", game_title, game_logo, title_music, lobby_music;
	int title_logo_x = 0, title_logo_y = 0, title_buttons_x = 0, title_buttons_y = 0, title_buttons_padding = 0,
	    title_tip_x = 0, title_tip_width = 0, title_tip_padding = 0;

	std::string terrain_mask_image = "terrain/alphamask.png";
	std::string grid_image_top = "terrain/grid-top.png";
	std::string grid_image_bottom = "terrain/grid-bottom.png";
	std::string unreachable_image = "terrain/darken.png";    /**< overlay image for unreachable tiles. */
	std::string linger_image = "terrain/darken-linger.png";  /**< overlay image for tiles in linger mode. */

	std::string shroud_prefix;
	std::string fog_prefix;
	std::vector<std::string> fog_variants;
	std::vector<std::string> shroud_variants;

	std::string energy_image = "misc/bar-energy.png";
	std::string moved_ball_image = "misc/ball-moved.png";
	std::string unmoved_ball_image = "misc/ball-unmoved.png";
	std::string partmoved_ball_image = "misc/ball-partmoved.png";
	std::string enemy_ball_image = "misc/ball-enemy.png";
	std::string ally_ball_image = "misc/ball-ally.png";
	std::string flag_image = "flags/flag-1.png:150,flags/flag-2.png:150,flags/flag-3.png:150,flags/flag-4.png:150";
	std::string flag_icon_image = "flags/flag_icon.png";
	std::string flag_rgb = "flag_green";
	std::vector<Uint32> red_green_scale;
	std::vector<Uint32> red_green_scale_text;

	double hp_bar_scaling = 0.666;
	double xp_bar_scaling = 0.5;

	std::vector<std::string> foot_speed_prefix;
	std::string foot_teleport_enter = "footsteps/teleport-in.png";
	std::string foot_teleport_exit = "footsteps/teleport-out.png";

	std::string observer_image = "misc/eye.png";
	std::string tod_bright_image = "misc/tod-bright.png";
	std::string tod_dark_image = "misc/tod-dark.png";
	std::string unchecked_menu_image = "buttons/checkbox.png";
	std::string checked_menu_image = "buttons/checkbox-pressed.png";
	std::string wml_menu_image = "buttons/WML-custom.png";

	std::string level_image;
	std::string ellipsis_image;

	std::string default_victory_music;
	std::string default_defeat_music;

	std::map<std::string, color_range > team_rgb_range;
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

	void load_config(const config* cfg)
	{
		if(cfg == NULL)
			return;

		const config& v = *cfg;

		base_income = v["base_income"].to_int(2);
		village_income = v["village_income"].to_int(1);
		poison_amount = v["poison_amount"].to_int(8);
		rest_heal_amount = v["rest_heal_amount"].to_int(2);
		recall_cost = v["recall_cost"].to_int(20);
		kill_experience = v["kill_experience"].to_int(8);
		lobby_refresh = v["lobby_refresh"].to_int(2000);

		game_icon = v["icon"].str();
		game_title = v["title"].str();
		game_logo = v["logo"].str();
		title_music = v["title_music"].str();
		lobby_music = v["lobby_music"].str();

		title_logo_x = v["logo_x"];
		title_logo_y = v["logo_y"];
		title_buttons_x = v["buttons_x"];
		title_buttons_y = v["buttons_y"];
		title_buttons_padding = v["buttons_padding"];

		title_tip_x = v["tip_x"].to_int();
		title_tip_width = v["tip_width"].to_int();
		title_tip_padding = v["tip_padding"].to_int();


		energy_image = v["energy_image"].str();
		moved_ball_image = v["moved_ball_image"].str();
		unmoved_ball_image = v["unmoved_ball_image"].str();
		partmoved_ball_image = v["partmoved_ball_image"].str();
		enemy_ball_image = v["enemy_ball_image"].str();
		ally_ball_image = v["ally_ball_image"].str();
		flag_image = v["flag_image"].str();
		flag_icon_image = v["flag_icon_image"].str();

		hp_bar_scaling = v["hp_bar_scaling"].to_double();
		xp_bar_scaling = v["xp_bar_scaling"].to_double();

		foot_speed_prefix = utils::split(v["footprint_prefix"]);
		foot_teleport_enter = v["footprint_teleport_enter"].str();
		foot_teleport_exit = v["footprint_teleport_exit"].str();

		terrain_mask_image = v["terrain_mask_image"].str();
		grid_image_top = v["grid_image_top"].str();
		grid_image_bottom = v["grid_image_bottom"].str();
		unreachable_image = v["unreachable_image"].str();

		shroud_prefix = v["shroud_prefix"].str();
		fog_prefix  = v["fog_prefix"].str();
		fog_variants = utils::split(v["fog_variants"].str());
		shroud_variants = utils::split(v["shroud_variants"].str());

		observer_image = v["observer_image"].str();
		tod_bright_image = v["tod_bright_image"].str();
		tod_dark_image = v["tod_dark_image"].str();

		level_image = v["level_image"].str();
		ellipsis_image = v["ellipsis_image"].str();
		default_victory_music = v["default_victory_music"].str();
		default_defeat_music = v["default_defeat_music"].str();

		add_color_info(v);

		if (const config::attribute_value *a = v.get("flag_rgb")) flag_rgb = a->str();

		red_green_scale = string2rgb(v["red_green_scale"]);
		if (red_green_scale.empty()) {
			red_green_scale.push_back(0x00FFFF00);
		}
		red_green_scale_text = string2rgb(v["red_green_scale_text"]);
		if (red_green_scale_text.empty()) {
			red_green_scale_text.push_back(0x00FFFF00);
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
		foreach (const config &teamC, v.child_range("color_range"))
		{
			const config::attribute_value *a1 = teamC.get("id"),
				*a2 = teamC.get("rgb");
			if (!a1 || !a2) continue;
			std::string id = *a1;
			std::vector<Uint32> temp = string2rgb(*a2);
			team_rgb_range.insert(std::make_pair(id,color_range(temp)));
			team_rgb_name[id] = teamC["name"];
			//generate palette of same name;
			std::vector<Uint32> tp = palette(team_rgb_range[id]);
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

	const color_range& color_info(const std::string& name)
	{
		std::map<std::string, color_range>::const_iterator i = team_rgb_range.find(name);
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

} // game_config
