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

#include "global.hpp"

#include "SDL.h"
#include "SDL_mixer.h"

#include "about.hpp"
#include "config.hpp"
#include "construct_dialog.hpp"
#include "cursor.hpp"
#include "dialogs.hpp"
#include "game_display.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "game_config.hpp"
#include "game_errors.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "gui/dialogs/addon_connect.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/widgets/button.hpp"
#include "help.hpp"
#include "hotkeys.hpp"
#include "intro.hpp"
#include "language.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "widgets/menu.hpp"
#include "marked-up_text.hpp"
#include "multiplayer.hpp"
#include "network.hpp"
#include "playcampaign.hpp"
#include "preferences_display.hpp"
#include "publish_campaign.hpp"
#include "replay.hpp"
#include "sound.hpp"
#include "statistics.hpp"
#include "thread.hpp"
#include "titlescreen.hpp"
#include "util.hpp"
#include "upload_log.hpp"
#include "wml_exception.hpp"
#include "wml_separators.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/binary_wml.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "sha1.hpp"

#ifdef HAVE_PYTHON
#include "ai_python.hpp"
#endif

#include "wesconfig.h"

#include <clocale>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#define ERR_CONFIG LOG_STREAM(err, config)
#define WRN_CONFIG LOG_STREAM(warn, config)
#define DBG_CONFIG LOG_STREAM(debug, config)
#define LOG_CONFIG LOG_STREAM(info, config)
#define LOG_GENERAL LOG_STREAM(info, general)
#define DBG_GENERAL LOG_STREAM(debug, general)
#define ERR_NET LOG_STREAM(err, network)
#define LOG_NET LOG_STREAM(info, network)
#define ERR_FS LOG_STREAM(err, filesystem)

static bool less_campaigns_rank(const config* a, const config* b) {
	return lexical_cast_default<int>((*a)["rank"],1000) <
	       lexical_cast_default<int>((*b)["rank"],1000);
}

namespace {

static bool new_widgets = false;

class game_controller
{
public:
	game_controller(int argc, char** argv);
	~game_controller();

	game_display& disp();

	bool init_video();
	bool init_config();
	bool init_language();
	bool play_test();
	bool play_multiplayer_mode();

	void reset_game_cfg();
	void reset_defines_map();
	void reload_changed_game_config();
	void read_configs(std::string&);

	bool is_loading() const;
	bool load_game();
	void set_tutorial();

	bool new_campaign();
	bool goto_campaign();
	bool goto_multiplayer();
	bool play_multiplayer();
	void manage_addons();
	void download_campaigns(std::string host);
	bool change_language();

	void show_help();
	void show_preferences();
	void show_upload_begging();

	enum RELOAD_GAME_DATA { RELOAD_DATA, NO_RELOAD_DATA };
	void play_game(RELOAD_GAME_DATA reload=RELOAD_DATA);
	void play_replay();
	const config& game_config(){return game_config_;};

private:
	game_controller(const game_controller&);
	void operator=(const game_controller&);

	void read_game_cfg(bool use_cache);
	void refresh_game_cfg(bool reset_translations=false);
    void set_unit_data();

	void upload_campaign(const std::string& campaign, network::connection sock);
	void delete_campaign(const std::string& campaign, network::connection sock);
	void remove_campaign(const std::string& campaign);

	const int argc_;
	int arg_;
	const char* const * const argv_;

	//this should get destroyed *after* the video, since we want
	//to clean up threads after the display disappears.
	const threading::manager thread_manager;

	CVideo video_;

	const font::manager font_manager_;
	const preferences::manager prefs_manager_;
	const image::manager image_manager_;
	const events::event_context main_event_context_;
	const hotkey::manager hotkey_manager_;
	const upload_log::manager upload_log_manager_;
	sound::music_thinker music_thinker_;
	resize_monitor resize_monitor_;
	binary_paths_manager paths_manager_;

	std::string test_scenario_;

	bool test_mode_, multiplayer_mode_, no_gui_;
	bool use_caching_;
	bool force_valid_cache_;
	int force_bpp_;

	config game_config_;

	util::scoped_ptr<game_display> disp_;

	game_state state_;

	std::string loaded_game_;
	bool loaded_game_show_replay_;
	bool loaded_game_cancel_orders_;

	preproc_map defines_map_, old_defines_map_;

	std::string multiplayer_server_;
	bool jump_to_campaign_, jump_to_multiplayer_;
};

game_controller::game_controller(int argc, char** argv)
   : argc_(argc), arg_(1), argv_(argv), thread_manager(),
     test_scenario_("test"), test_mode_(false), multiplayer_mode_(false),
     no_gui_(false), use_caching_(true), force_valid_cache_(false),
     force_bpp_(-1), disp_(NULL), loaded_game_show_replay_(false),
     loaded_game_cancel_orders_(false),
     jump_to_campaign_(false), jump_to_multiplayer_(false)
{
	bool no_sound = false;
	for(arg_ = 1; arg_ != argc_; ++arg_) {
		const std::string val(argv_[arg_]);
		if(val.empty()) {
			continue;
		}

		if(val == "--fps") {
			preferences::set_show_fps(true);
		} else if(val == "--nocache") {
			use_caching_ = false;
		} else if(val == "--max-fps") {
			if(arg_+1 != argc_) {
				++arg_;
				int fps = lexical_cast_default<int>(argv_[arg_], 50);
				fps = minimum<int>(fps, 1000);
				fps = maximum<int>(fps, 1);
				fps = 1000 / fps;
				// increase the delay to avoid going above the maximum
				if(1000 % fps != 0) {
					++fps;
				}
				preferences::set_draw_delay(fps);
			}
		} else if(val == "--validcache") {
			force_valid_cache_ = true;
		} else if(val == "--resolution" || val == "-r") {
			if(arg_+1 != argc_) {
				++arg_;
				const std::string val(argv_[arg_]);
				const std::vector<std::string> res = utils::split(val, 'x');
				if(res.size() == 2) {
					const int xres = lexical_cast_default<int>(res.front());
					const int yres = lexical_cast_default<int>(res.back());
					if(xres > 0 && yres > 0) {
						const std::pair<int,int> resolution(xres,yres);
						preferences::set_resolution(resolution);
					}
				}
			}
		} else if(val == "--bpp") {
			if(arg_+1 != argc_) {
				++arg_;
				force_bpp_ = lexical_cast_default<int>(argv_[arg_],-1);
			}
		} else if(val == "--load" || val == "-l") {
			if(arg_+1 != argc_) {
				++arg_;
				loaded_game_ = argv_[arg_];
			}
		} else if(val == "--with-replay") {
			loaded_game_show_replay_ = true;

		} else if(val == "--nogui") {
			no_gui_ = true;
			no_sound = true;
			preferences::disable_preferences_save();
		} else if(val == "--windowed" || val == "-w") {
			preferences::set_fullscreen(false);
		} else if(val == "--fullscreen" || val == "-f") {
			preferences::set_fullscreen(true);

		} else if(val == "--campaign" || val == "-c") {
			jump_to_campaign_ = true;

		} else if(val == "--server" || val == "-s"){
			jump_to_multiplayer_ = true;
			//Do we have any server specified ?
			if(argc_ > arg_+1){
				multiplayer_server_ = argv_[arg_+1];
				++arg_;
			//Pick the first server in config
			}else{
				if(game_config::server_list.size() > 0)
					multiplayer_server_ = preferences::network_host();
				else
					multiplayer_server_ = "";
			}

		} else if(val == "--multiplayer" || val == "-m") {
			multiplayer_mode_ = true;
			break; //parse the rest of the arguments when we set up the game
		} else if(val == "--test" || val == "-t") {
			test_mode_ = true;
			// If we have -t foo it's ambiguous whether it foo is the parameter
			// for Wesnoth or the start directory so we assume it's the starting
			// directory.
			if(arg_ + 2 < argc_ && argv_[arg_ + 1][0] != '-') {
				++arg_;
				test_scenario_ = argv_[arg_];
			}
		} else if(val == "--debug" || val == "-d") {
			game_config::debug = true;
			game_config::mp_debug = true;
		} else if(val == "--no-delay") {
			game_config::no_delay = true;
		} else if (val.substr(0, 6) == "--log-") {
		} else if(val == "--nosound") {
			no_sound = true;
		} else if(val == "--new-widgets") {
			// This is a hidden option to enable the new widget toolkit.
			new_widgets = true;
		} else if(val[0] == '-') {
			std::cerr << "unknown option: " << val << std::endl;
			throw config::error("unknown option");
		} else {

		  std::cerr << "Setting path using " << val << std::endl;
			if(val[0] == '/') {
				game_config::path = val;
			} else {
				game_config::path = get_cwd() + '/' + val;
			}

			if(!is_directory(game_config::path)) {
				std::cerr << "Could not find directory '" << game_config::path << "'\n";
				throw config::error("directory not found");
			}

		}
	}
	std::cerr << "Data at '" << game_config::path << "'\n";

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {
		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	}
}

game_display& game_controller::disp()
{
	if(disp_.get() == NULL) {

		if(get_video_surface() == NULL) {
			throw CVideo::error();
		}

		static unit_map dummy_umap;
		static config dummy_cfg;
		static gamemap dummy_map(dummy_cfg, "");
		static gamestatus dummy_status(dummy_cfg, 0);
		static std::vector<team> dummy_teams;
		disp_.assign(new game_display(dummy_umap, video_, dummy_map, dummy_status,
			dummy_teams, dummy_cfg, dummy_cfg, dummy_cfg));
	}

	return *disp_.get();
}

bool game_controller::init_video()
{
	if(no_gui_) {
		if(!multiplayer_mode_) {
			std::cerr << "--nogui flag is only valid with --multiplayer flag\n";
			return false;
		}
		video_.make_fake();
		game_config::no_delay = true;
		return true;
	}

	image::set_wm_icon();

	int video_flags = preferences::fullscreen() ? FULL_SCREEN : 0;

	std::pair<int,int> resolution = preferences::resolution();

	int DefaultBPP = 24;
	const SDL_VideoInfo* const video_info = SDL_GetVideoInfo();
	if(video_info != NULL && video_info->vfmt != NULL) {
		DefaultBPP = video_info->vfmt->BitsPerPixel;
	}

	std::cerr << "Checking video mode: " << resolution.first
		  << "x" << resolution.second << "x" << DefaultBPP << "...\n";
	int bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);

	if(bpp == 0) {
		//Video mode not supported, maybe from bad prefs.
		std::cerr << "Video mode " << resolution.first
		          << "x" << resolution.second << "x" << DefaultBPP << " "
		          << "is not supported - attempting 1024x768x" << DefaultBPP << "...\n";

		//Attempt 1024x768.
		resolution.first = 1024;
		resolution.second = 768;

		bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);

		if(bpp == 0) {
			std::cerr << "1024x768x" << DefaultBPP << " not available - attempting 800x600x" << DefaultBPP << "...\n";

			resolution.first = 800;
			resolution.second = 600;

			bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);
		}

#ifdef USE_TINY_GUI
		if(bpp == 0) {
			std::cerr << "800x600x" << DefaultBPP << " not available - attempting 640x480x" << DefaultBPP << "...\n";

			resolution.first = 640;
			resolution.second = 480;

			bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);
		}

		if(bpp == 0) {
			std::cerr << "640x480x" << DefaultBPP << " not available - attempting 320x240x" << DefaultBPP << "...\n";

			resolution.first = 320;
			resolution.second = 240;

			bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);
		}
#endif

#ifdef USE_SMALL_GUI
        if(bpp == 0) {
            std::cerr << "800x600x" << DefaultBPP << " not available - attempting 800x480x" << DefaultBPP << "...\n";

            resolution.first = 800;
            resolution.second = 480;

            bpp = video_.modePossible(resolution.first,resolution.second,DefaultBPP,video_flags);
        }
#endif

		if(bpp == 0) {
			//couldn't do 1024x768 or 800x600

			std::cerr << "The required video mode, " << resolution.first
			          << "x" << resolution.second << "x" << DefaultBPP << " "
			          << "is not supported\n";

			if((video_flags&FULL_SCREEN) != 0) {
				std::cerr << "Try running the program with the --windowed option "
				          << "using a " << DefaultBPP << "bpp X windows setting\n";
			}

			if((video_flags&FULL_SCREEN) == 0) {
				std::cerr << "Try running with the --fullscreen option\n";
			}

			return false;
		}
	}

	if(force_bpp_ > 0) {
		bpp = force_bpp_;
	}

	std::cerr << "setting mode to " << resolution.first << "x" << resolution.second << "x" << bpp << "\n";
	const int res = video_.setMode(resolution.first,resolution.second,bpp,video_flags);
	video_.setBpp(bpp);
	if(res == 0) {
		std::cerr << "required video mode, " << resolution.first << "x"
		          << resolution.second << "x" << bpp << " is not supported\n";
		return false;
	}

	return true;
}

bool game_controller::init_config()
{
	//Resets old_defines_map_, to force refresh_game_cfg to reload
	//everything.
	old_defines_map_.clear();
	reset_game_cfg();
	refresh_game_cfg();

	game_config::load_config(game_config_.child("game_config"));

	hotkey::load_hotkeys(game_config_);
	paths_manager_.set_paths(game_config_);
	::init_textdomains(game_config_);
	about::set_about(game_config_);

	return true;
}

bool game_controller::init_language()
{
	if(!::load_language_list())
		return false;

	if (!::set_language(get_locale()))
		return false;

	if(!no_gui_) {
		std::string wm_title_string = _("The Battle for Wesnoth");
		wm_title_string += " - " + game_config::revision;
		SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
	}

	hotkey::load_descriptions();

	return true;
}

bool game_controller::play_test()
{
	static bool first_time = true;

	if(test_mode_ == false) {
		return true;
	}
	if(!first_time)
		return false;

	first_time = false;

	state_.campaign_type = "test";
	state_.scenario = test_scenario_;

	try {
		refresh_game_cfg();
	} catch(config::error&) {
		reset_game_cfg();
		refresh_game_cfg();
		return false;
	}

	try {
		upload_log nolog(false);
		::play_game(disp(),state_,game_config_,nolog);
	} catch(game::load_game_exception& e) {
		loaded_game_ = e.game;
		loaded_game_show_replay_ = e.show_replay;
		loaded_game_cancel_orders_ = e.cancel_orders;
		test_mode_ = false;
		return true;
	}

	return false;
}

bool game_controller::play_multiplayer_mode()
{
	state_ = game_state();

	if(!multiplayer_mode_) {
		return true;
	}

	std::string era = "era_default";
	std::string scenario = "multiplayer_The_Freelands";
	std::map<int,std::string> side_types, side_controllers, side_algorithms;
	std::map<int,string_map> side_parameters;
	std::string turns = "50";

	size_t sides_counted = 0;

	for(++arg_; arg_ < argc_; ++arg_) {
		const std::string val(argv_[arg_]);
		if(val.empty()) {
			continue;
		}

		std::vector<std::string> name_value = utils::split(val, '=');
		if(name_value.size() > 2) {
			std::cerr << "invalid argument '" << val << "'\n";
			return false;
		} else if(name_value.size() == 2) {
			const std::string name = name_value.front();
			const std::string value = name_value.back();

			const std::string name_head = name.substr(0,name.size()-1);
			const char name_tail = name[name.size()-1];
			const bool last_digit = isdigit(name_tail) ? true:false;
			const size_t side = name_tail - '0';

			if(last_digit && side > sides_counted) {
				std::cerr << "counted sides: " << side << "\n";
				sides_counted = side;
			}

			if(name == "--scenario") {
				scenario = value;
			} else if(name == "--turns") {
				turns = value;
			} else if(name == "--era") {
				era = value;
			} else if(last_digit && name_head == "--controller") {
				side_controllers[side] = value;
			} else if(last_digit && name_head == "--algorithm") {
				side_algorithms[side] = value;
			} else if(last_digit && name_head == "--side") {
				side_types[side] = value;
			} else if(last_digit && name_head == "--parm") {
				const std::vector<std::string> name_value = utils::split(value, ':');
				if(name_value.size() != 2) {
					std::cerr << "argument to '" << name << "' must be in the format name:value\n";
					return false;
				}

				side_parameters[side][name_value.front()] = name_value.back();
			} else {
				std::cerr << "unrecognized option: '" << name << "'\n";
				return false;
			}
		} else {
			if (val == "--exit-at-end") {
				game_config::exit_at_end = true;
			}
		}
	}

	const config* const lvl = game_config_.find_child("multiplayer","id",scenario);
	if(lvl == NULL) {
		std::cerr << "Could not find scenario '" << scenario << "'\n";
		return false;
	}

	state_.campaign_type = "multiplayer";
	state_.scenario = "";
	state_.snapshot = config();

	config level = *lvl;
	std::vector<config*> story;

	const config* const era_cfg = game_config_.find_child("era","id",era);
	if(era_cfg == NULL) {
		std::cerr << "Could not find era '" << era << "'\n";
		return false;
	}

	level["turns"] = turns;

	const config* const side = era_cfg->child("multiplayer_side");
	if(side == NULL) {
		std::cerr << "Could not find multiplayer side\n";
		return false;
	}

	while(level.get_children("side").size() < sides_counted) {
		std::cerr << "now adding side...\n";
		level.add_child("side");
	}

	int side_num = 1;
	for(config::child_itors itors = level.child_range("side"); itors.first != itors.second; ++itors.first, ++side_num) {
		std::map<int,std::string>::const_iterator type = side_types.find(side_num),
		                                          controller = side_controllers.find(side_num),
		                                          algorithm = side_algorithms.find(side_num);

		const config* side = type == side_types.end() ?
			era_cfg->find_child("multiplayer_side", "random_faction", "yes") :
			era_cfg->find_child("multiplayer_side", "id", type->second);

		if (side == NULL) {
			std::string side_name = (type == side_types.end() ? "default" : type->second);
			std::cerr << "Could not find side '" << side_name << "' for side " << side_num << "\n";
			return false;
		}

		if (utils::string_bool((*side)["random_faction"])) {
			const config::child_list& factions = era_cfg->get_children("multiplayer_side");
			std::vector<std::string> faction_choices, faction_excepts;
			faction_choices = utils::split((*side)["choices"]);
			if(faction_choices.size() == 1 && faction_choices.front() == "") {
				faction_choices.clear();
			}
			faction_excepts = utils::split((*side)["except"]);;
			if(faction_excepts.size() == 1 && faction_excepts.front() == "") {
				faction_excepts.clear();
			}
			for(unsigned int i = 0, j = 0; i < factions.size(); ++i) {
				if (utils::string_bool((*factions[i])["random_faction"]) != true) {
					const std::string& faction_id = (*factions[i])["id"];
					if (
						!faction_choices.empty() &&
						std::find(faction_choices.begin(),faction_choices.end(),faction_id) == faction_choices.end()
					)
						continue;
					if (
						!faction_excepts.empty() &&
						std::find(faction_excepts.begin(),faction_excepts.end(),faction_id) != faction_excepts.end()
					)
						continue;
					j++;
					if (rand()%j == 0) {
						side = factions[i];
					}
				}
			}
			if (utils::string_bool((*side)["random_faction"], false) == true) {
				std::string side_name = (type == side_types.end() ? "default" : type->second);
				std::cerr << "Could not find any non-random faction for side " << side_num << "\n";
				return false;
			}
		}

		char buf[20];
		snprintf(buf,sizeof(buf),"%d",side_num);
		(*itors.first)->values["side"] = buf;

		(*itors.first)->values["canrecruit"] = "yes";

		(*itors.first)->append(*side);

		if(controller != side_controllers.end()) {
			(*itors.first)->values["controller"] = controller->second;
		}

		if(algorithm != side_algorithms.end()) {
			(*itors.first)->values["ai_algorithm"] = algorithm->second;
		}

		config& ai_params = (*itors.first)->add_child("ai");

		//now add in any arbitrary parameters given to the side
		for(string_map::const_iterator j = side_parameters[side_num].begin(); j != side_parameters[side_num].end(); ++j) {
			(*itors.first)->values[j->first] = j->second;
			ai_params[j->first] = j->second;
		}
	}

	try {
		upload_log nolog(false);
		state_.snapshot = level;
		::play_game(disp(),state_,game_config_,nolog);
	} catch(game::error& e) {
		std::cerr << "caught error: '" << e.message << "'\n";
	} catch(game::load_game_exception& e) {
		//the user's trying to load a game, so go into the normal title screen loop and load one
		loaded_game_ = e.game;
		loaded_game_show_replay_ = e.show_replay;
		loaded_game_cancel_orders_ = e.cancel_orders;
		return true;
	} catch(twml_exception& e) {
		e.show(disp());
		return false;
	} catch(...) {
		std::cerr << "caught unknown error playing level...\n";
	}

	return false;
}

bool game_controller::is_loading() const
{
	return loaded_game_.empty() == false;
}

bool game_controller::load_game()
{
	state_ = game_state();

	bool show_replay = loaded_game_show_replay_;
	bool cancel_orders = loaded_game_cancel_orders_;

	const std::string game = loaded_game_.empty() ? dialogs::load_game_dialog(disp(),game_config_,&show_replay,&cancel_orders) : loaded_game_;

	loaded_game_ = "";

	if(game == "") {
		return false;
	}

	try {
		//to load a save file, we first load the file in, then we re-parse game
		//data with the save's #defines, and then we finally parse the save file,
		//with the game data ready to go.

		config cfg;
		std::string error_log;
		read_save_file(game,cfg,&error_log);
		if(!error_log.empty()) {
			gui::show_error_message(disp(),
					_("Warning: The file you have tried to load is corrupt. Loading anyway.\n") +
					error_log);
		}

		reset_defines_map();
		defines_map_[cfg["difficulty"]] = preproc_define();

		if(defines_map_.count("NORMAL")) {
			defines_map_["MEDIUM"] = preproc_define();
		}

		const std::string& campaign_define = cfg["campaign_define"];
		if(campaign_define.empty() == false) {
			defines_map_[campaign_define] = preproc_define();
		}
		if (campaign_define.empty() && (cfg["campaign_type"] == "multiplayer")){
			defines_map_["MULTIPLAYER"] = preproc_define();
		}

		const std::vector<std::string> campaign_xtra_defines = utils::split(cfg["campaign_extra_defines"]);

		for(std::vector<std::string>::const_iterator i = campaign_xtra_defines.begin(); i != campaign_xtra_defines.end(); ++i) {
			defines_map_[*i] = preproc_define();
		}

		try {
			refresh_game_cfg();
		} catch(config::error&) {
			reset_game_cfg();
			refresh_game_cfg();
			return false;
		}

		const std::string version = cfg["version"];
		if(version != game_config::version) {
			// do not load if too old, if either the savegame or the current game
			// has the version 'test' allow loading
			if(version < game_config::min_savegame_version &&
					game_config::test_version.full != version &&
					game_config::test_version.full != game_config::version) {

				/* GCC-3.3 needs a temp var otherwise compilation fails */
				gui::message_dialog dlg(disp(), "", _("This save is from a version too old to be loaded."));
				dlg.show();
				return false;
			}

			const int res = gui::dialog(disp(),"",
			                      _("This save is from a different version of the game. Do you want to try to load it?"),
			                      gui::YES_NO).show();
			if(res == 1) {
				return false;
			}
		}

		state_ = game_state(cfg, show_replay);

		// Get the status of the random in the snapshot.
		// For a replay we need to restore the start only, the replaying gets at
		// proper location.
		// For normal loading also restore the call count.
		const int seed = lexical_cast_default<int>
			(cfg["random_seed"], 42);
		const unsigned calls = show_replay ? 0 :
			lexical_cast_default<unsigned> (state_.snapshot["random_calls"]);
		state_.rng().seed_random(seed, calls);

	} catch(game::error& e) {
		gui::show_error_message(disp(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		return false;
	} catch(config::error& e) {
		gui::show_error_message(disp(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		return false;
	} catch(io_exception&) {
		gui::show_error_message(disp(), _("File I/O Error while reading the game"));
		return false;
	} catch(twml_exception& e) {
		e.show(disp());
		return false;
	}
	recorder = replay(state_.replay_data);
	recorder.start_replay();
	recorder.set_skip(false);

	LOG_CONFIG << "has snapshot: " << (state_.snapshot.child("side") ? "yes" : "no") << "\n";

	if(state_.snapshot.child("side") == NULL) {
		// No snapshot; this is a start-of-scenario
		if (show_replay) {
			// There won't be any turns to replay, but the
			// user gets to watch the intro sequence again ...
			LOG_CONFIG << "replaying (start of scenario)\n";
		} else {
			LOG_CONFIG << "skipping...\n";
			recorder.set_skip(false);
		}
	} else {
		// We have a snapshot. But does the user want to see a replay?
		if(show_replay) {
			statistics::clear_current_scenario();
			LOG_CONFIG << "replaying (snapshot)\n";
		} else {
			LOG_CONFIG << "setting replay to end...\n";
			recorder.set_to_end();
			if(!recorder.at_end()) {
				WRN_CONFIG << "recorder is not at the end!!!\n";
			}
		}
	}

	if(state_.campaign_type == "tutorial") {
		defines_map_["TUTORIAL"] = preproc_define();
	} else if(state_.campaign_type == "multiplayer") {
		for(config::child_itors sides = state_.snapshot.child_range("side");
		    sides.first != sides.second; ++sides.first) {
			if((**sides.first)["controller"] == "network")
				(**sides.first)["controller"] = "human";
		}
	}

	if (cancel_orders) {
		for(config::child_itors sides = state_.snapshot.child_range("side");
			    sides.first != sides.second; ++sides.first) {
				if((**sides.first)["controller"] == "human") {
					for (config::child_itors units = (**sides.first).child_range("unit");
							units.first != units.second; ++units.first) {
						(**units.first)["goto_x"] = "-999";
						(**units.first)["goto_y"] = "-999";
					}
				}
		}
	}

	return true;
}

void game_controller::set_tutorial()
{
	state_ = game_state();
	state_.campaign_type = "tutorial";
	state_.scenario = "tutorial";
	state_.campaign_define = "TUTORIAL";
	reset_defines_map();
	defines_map_["TUTORIAL"] = preproc_define();

}

bool game_controller::new_campaign()
{
	state_ = game_state();
	state_.campaign_type = "scenario";

	config::child_list campaigns = game_config_.get_children("campaign");
	std::sort(campaigns.begin(),campaigns.end(),less_campaigns_rank);

	std::vector<std::string> campaign_names;
	std::vector<std::pair<std::string,std::string> > campaign_desc;

	for(config::child_list::const_iterator i = campaigns.begin(); i != campaigns.end(); ++i) {
		std::stringstream str;
		const std::string& icon = (**i)["icon"];
		const std::string desc = (**i)["description"];
		const std::string image = (**i)["image"];
		if(icon.empty()) {
			str << COLUMN_SEPARATOR;
		} else {
			str << IMAGE_PREFIX << icon << COLUMN_SEPARATOR;
		}

		str << (**i)["name"];

		campaign_names.push_back(str.str());
		campaign_desc.push_back(std::pair<std::string,std::string>(desc,image));
	}

	if(campaign_names.size() <= 0) {
	  gui::show_error_message(disp(),
				  _("No campaigns are available.\n"));
		return false;
	}
	dialogs::campaign_preview_pane campaign_preview(disp().video(),&campaign_desc);
	gui::dialog cmenu(disp(), _("Play a campaign"), " ", gui::OK_CANCEL);
	cmenu.set_menu(campaign_names);
	cmenu.add_pane(&campaign_preview);
	gui::dialog::dimension_measurements dim = cmenu.layout();
	Uint16 screen_width = screen_area().w;
	Uint16 dialog_width = cmenu.get_frame().get_layout().exterior.w;
	if(screen_width < 850 && screen_width - dialog_width > 20) {
		// On small resolutions, reduce the amount of unused horizontal space
		campaign_preview.set_width(campaign_preview.width() + screen_width - dialog_width - 20);
		dim = cmenu.layout();
	}
	SDL_Rect& preview_loc = dim.panes[&campaign_preview];
	preview_loc.y = dim.menu_y;
	if(dim.menu_height > 0) {
		preview_loc.h = dim.menu_height;
	} else {
		preview_loc.h = cmenu.get_menu().height();
	}
	cmenu.set_layout(dim);

	if(cmenu.show() == -1) {
		return false;
	}

	const config& campaign = *campaigns[cmenu.result()];

	state_.campaign = campaign["id"];
	state_.abbrev = campaign["abbrev"];
	state_.scenario = campaign["first_scenario"];

	const std::string difficulty_descriptions = campaign["difficulty_descriptions"];
	std::vector<std::string> difficulty_options = utils::split(difficulty_descriptions, ';');

	const std::vector<std::string> difficulties = utils::split(campaign["difficulties"]);

	if(difficulties.empty() == false) {
		if(difficulty_options.size() != difficulties.size()) {
			difficulty_options.resize(difficulties.size());
			std::copy(difficulties.begin(),difficulties.end(),difficulty_options.begin());
		}

		gui::dialog dlg(disp(), _("Difficulty"),
			_("Select difficulty level:"), gui::OK_CANCEL);
		dlg.set_menu(difficulty_options);
		if(dlg.show() == -1) {
			return false;
		}

		state_.difficulty = difficulties[dlg.result()];
		reset_defines_map();
		defines_map_[difficulties[dlg.result()]] = preproc_define();
	}

	state_.campaign_define = campaign["define"];
	state_.campaign_xtra_defines = utils::split(campaign["extra_defines"]);

	return true;
}

}

bool game_controller::goto_campaign()
{
	if(jump_to_campaign_){
		jump_to_campaign_ = false;
		if(new_campaign()) {
			play_game(game_controller::RELOAD_DATA);
		}else{
			return false;
		}
	}
	return true;
}

bool game_controller::goto_multiplayer()
{
	if(jump_to_multiplayer_){
		jump_to_multiplayer_ = false;
		if(play_multiplayer()){
			;
		}else{
			return false;
		}
	}
	return true;
}

static std::string format_file_size(const std::string& size_str)
{
	double size = lexical_cast_default<double>(size_str,0.0);

	const double k = 1024;
	if(size > 0.0) {
		std::string size_postfix = _("B");
		if(size > k) {
			size /= k;
			size_postfix = _("KB");
			if(size > k) {
				size /= k;
				size_postfix = _("MB");
			}
		}

		std::ostringstream stream;
#ifdef _MSC_VER
		// Visual C++ makes 'precision' set the number of decimal places.
		// Other platforms make it set the number of significant figures
		stream.precision(1);
		stream << std::fixed << size << size_postfix;
#else
		if (size < 100) stream.precision(3);
		else size = static_cast<int>(size);
		stream << size << size_postfix;
#endif
		return stream.str();
	} else {
		return "";
	}
}

namespace
{
	void game_controller::reload_changed_game_config()
	{
		//force a reload of configuration information
		old_defines_map_.clear();
		reset_game_cfg();
		data_tree_checksum(true); // Reload checksums
		refresh_game_cfg();
		::init_textdomains(game_config_);
		paths_manager_.set_paths(game_config_);
		clear_binary_paths_cache();
	}

	// Manage add-ons
	void game_controller::manage_addons()
	{
		int res;
		std::string host;
		if(new_widgets) {
			gui2::taddon_connect addon_dlg;
			
			addon_dlg.set_host_name(preferences::campaign_server());
			addon_dlg.show(disp().video());
			
			res = addon_dlg.get_retval();
			if(res == gui2::tbutton::OK) {
				res = 0;
				host = addon_dlg.host_name();
			} 
		} else {

			gui::dialog d(disp(),
						_("Connect to Server"),
						_("You will now connect to a server to download add-ons."),
						gui::OK_CANCEL);
			d.set_textbox(_("Server: "), preferences::campaign_server());
			d.add_button( new gui::dialog_button(disp().video(), _("Remove Add-ons"),
				gui::button::TYPE_PRESS, 2), gui::dialog::BUTTON_EXTRA);
			res = d.show();
			host = d.textbox_text();
		}

		if (res == 0 )	// Get Add-Ons
		{
			download_campaigns(host);
		}
		else if (res == 2) // Manage Add-Ons
		{

			std::vector<std::string> addons;
			std::vector<std::string> addon_dirs;

			const std::string campaign_dir = get_user_data_dir() + "/data/campaigns/";

			get_files_in_dir(campaign_dir, &addons, &addon_dirs, FILE_NAME_ONLY);

			// Strip the ".cfg" extension and replace "_" with " " for display.
			std::vector<std::string>::iterator i = addons.begin();
			while(i != addons.end())
			{
				std::string::size_type pos = i->rfind(".cfg", i->size());
				if(pos == std::string::npos) {
					i = addons.erase(i);
				} else {
					i->erase(pos);
					// remove it from the directory list too
					for(std::vector<std::string>::iterator j = addon_dirs.begin(); j != addon_dirs.end() ; ++j) {
						if (*i == *j) {
							addon_dirs.erase(j);
							break;
						}
					};
					std::replace(i->begin(), i->end(), '_', ' ');
					i++;
				}
			}
			// process the addons of type Addon/_main.cfg
			i = addon_dirs.begin();
			while(i != addon_dirs.end())
			{
				if (file_exists(campaign_dir + *i + "/_main.cfg")) {
					std::replace(i->begin(), i->end(), '_', ' ');
					addons.push_back(*i);
					i++;
				} else {
					i = addon_dirs.erase(i);
				}
			}

			if (addons.empty())
			{
				gui::show_error_message(disp(), _("You have no Add-ons installed."));
				return;
			}

			gui::menu::basic_sorter sorter;
			sorter.set_alpha_sort(1);


			int index = 0;

			do
			{
				gui::dialog addon_dialog(disp(),
							 _("Remove Add-ons"), _("Choose the add-on to remove."),
							 gui::OK_CANCEL);
				gui::menu::imgsel_style &addon_style = gui::menu::bluebg_style;

				gui::menu *addon_menu = new gui::menu(disp().video(), addons, false, -1,
						gui::dialog::max_menu_width, &sorter, &addon_style, false);
				addon_dialog.set_menu(addon_menu);
				index = addon_dialog.show();

				if(index < 0) return;

				std::string confirm_message = _("Are you sure you want to remove the add-on '$addon|'?");
				utils::string_map symbols;
				symbols["addon"] = addons.at(index);
				confirm_message = utils::interpolate_variables_into_string(confirm_message, &symbols);
				res = gui::dialog(disp(), _("Confirm"),	confirm_message, gui::YES_NO).show();
			} while (res != 0);

			bool delete_success = true;

			//Put underscores back in the name and remove the addon
			std::string filename = addons.at(index);
			std::replace(filename.begin(), filename.end(), ' ', '_');
			delete_success &= delete_directory(campaign_dir + filename);
			//Report results
			if (delete_success)
			{
				delete_success &= delete_directory(campaign_dir + filename + ".cfg");
				reload_changed_game_config();

				std::string message = _("Add-on '$addon|' deleted.");
				utils::string_map symbols;
				symbols["addon"] = addons.at(index);
				message = utils::interpolate_variables_into_string(message, &symbols);
				/* GCC-3.3 needs a temp var otherwise compilation fails */
				gui::dialog dlg(disp(), _("Add-on deleted"), message, gui::OK_ONLY);
				dlg.show();
			}
			else
			{
				/* GCC-3.3 needs a temp var otherwise compilation fails */
				gui::dialog dlg2(disp(), _("Error"), _("Add-on could not be deleted -- a file was not found."),
						gui::OK_ONLY);
				dlg2.show();
			}
	}
		else // Cancel or unexpected result
			return;
	}

void game_controller::download_campaigns(std::string host)
{
	const std::vector<std::string> items = utils::split(host, ':');
	if(items.empty()) {
		return;
	}

	host = items.front();
	preferences::set_campaign_server(host);

	try {
		const network::manager net_manager;
		const network::connection sock = dialogs::network_connect_dialog(disp(), _("Connecting to Server..."),
										items.front(), lexical_cast_default<int>(items.back(),15003) );
		if(!sock) {
			gui::show_error_message(disp(), _("Could not connect to host."));
			preferences::set_campaign_server("");
			return;
		}

		config cfg;
		cfg.add_child("request_campaign_list");
		// @todo Should be enabled once the campaign server can be recompiled.
		network::send_data(cfg, sock, false);

		network::connection res = dialogs::network_receive_dialog(disp(),_("Asking for list of add-ons"),cfg,sock);
		if(!res) {
			return;
		}

		const config* const error = cfg.child("error");
		if(error != NULL) {
			gui::show_error_message(disp(), (*error)["message"]);
			return;
		}

		const config* const campaigns_cfg = cfg.child("campaigns");
		if(campaigns_cfg == NULL) {
			gui::show_error_message(disp(), _("Error communicating with the server."));
			return;
		}

		std::vector<std::string> campaigns, versions, uploads, options;

		std::string sep(1, COLUMN_SEPARATOR);

		std::stringstream heading;
		heading << HEADING_PREFIX << sep << _("Name") << sep << _("Version") << sep
				<< _("Author") << sep << _("Downloads") << sep << _("Size");

		const config::child_list& cmps = campaigns_cfg->get_children("campaign");
		const std::vector<std::string>& publish_options = available_campaigns();

		std::vector<std::string> delete_options;

		std::vector<int> sizes;

		for(config::child_list::const_iterator i = cmps.begin(); i != cmps.end(); ++i) {
			const std::string& name = (**i)["name"];
			campaigns.push_back(name);
			versions.push_back((**i)["version"]);
			uploads.push_back((**i)["uploads"]);

			if(std::count(publish_options.begin(),publish_options.end(),name) != 0) {
				delete_options.push_back(name);
			}

			std::string title = (**i)["title"];
			if(title == "") {
				title = name;
				std::replace(title.begin(),title.end(),'_',' ');
			}

			std::string version   = (**i)["version"],
			            author    = (**i)["author"];

			utils::truncate_as_wstring(title, 20);
			utils::truncate_as_wstring(version, 12);
			utils::truncate_as_wstring(author, 16);

			//add negative sizes to reverse the sort order
			sizes.push_back(-atoi((**i)["size"].c_str()));

			std::string icon = (**i)["icon"];
			if(icon.find("units/") != std::string::npos
			&& icon.find_first_of('~') == std::string::npos) {
				//a hack to prevent magenta icons, because they look awful
				icon.append("~RC(magenta>red)");
			}
			options.push_back(IMAGE_PREFIX + icon + COLUMN_SEPARATOR +
			                  title + COLUMN_SEPARATOR +
			                  version + COLUMN_SEPARATOR +
			                  author + COLUMN_SEPARATOR +
			                  (**i)["downloads"].str() + COLUMN_SEPARATOR +
			                  format_file_size((**i)["size"]));
		}

		options.push_back(heading.str());

		for(std::vector<std::string>::const_iterator j = publish_options.begin(); j != publish_options.end(); ++j) {
			options.push_back(sep + _("Publish add-on: ") + *j);
		}

		for(std::vector<std::string>::const_iterator d = delete_options.begin(); d != delete_options.end(); ++d) {
			options.push_back(sep + _("Delete add-on: ") + *d);
		}

		if(campaigns.empty() && publish_options.empty()) {
			gui::show_error_message(disp(), _("There are no add-ons available for download from this server."));
			return;
		}

		gui::menu::basic_sorter sorter;
		sorter.set_alpha_sort(1).set_alpha_sort(2).set_alpha_sort(3).set_numeric_sort(4).set_position_sort(5,sizes);

		gui::dialog addon_dialog(disp(), _("Get Add-ons"),
					       _("Choose the add-on to download."),
					       gui::OK_CANCEL);
		gui::menu::imgsel_style addon_style(gui::menu::bluebg_style);

		//make sure the icon isn't too big
		addon_style.scale_images(font::relative_size(72), font::relative_size(72));
		gui::menu *addon_menu = new gui::menu(disp().video(), options, false, -1,
			gui::dialog::max_menu_width, &sorter, &addon_style, false);
		addon_dialog.set_menu(addon_menu);
		const int index = addon_dialog.show();
		if(index < 0) {
			return;
		}

		if(index >= int(campaigns.size() + publish_options.size())) {
			delete_campaign(delete_options[index - int(campaigns.size() + publish_options.size())],sock);
			return;
		}

		if(index >= int(campaigns.size())) {
			upload_campaign(publish_options[index - int(campaigns.size())],sock);
			return;
		}

		// Get all dependencies of the campaign selected for download.
		const config *selected_campaign = campaigns_cfg->find_child("campaign", "name", campaigns[index]);
		std::vector<std::string> dependencies = utils::split((*selected_campaign)["dependencies"]);
		if (!dependencies.empty()) {
			// Get all dependencies which are not already installed.
			// TODO: Somehow determine if the version is outdated.
			const std::vector<std::string>& installed = installed_campaigns();
			std::vector<std::string>::iterator i;
			std::string missing = "";
			for (i = dependencies.begin(); i != dependencies.end(); i++) {
				if (std::find(installed.begin(), installed.end(), *i) == installed.end()) {
					missing += "\n" + *i;
				}
			}
			// If there are any, display a message.
			// TODO: Somehow offer to automatically download
			// the missing dependencies.
			if (!missing.empty()) {
				if (gui::dialog(disp(),
						      _("Dependencies"),
						      std::string(_("This add-on requires the following additional dependencies:")) +
							"\n" + missing +
							"\n" + _("Do you still want to download it?"), gui::OK_CANCEL).show())
					return;
			}
		}

		config request;
		request.add_child("request_campaign")["name"] = campaigns[index];
		// @todo Should be enabled once the campaign server can be recompiled.
		network::send_data(request, sock, false);

		res = dialogs::network_receive_dialog(disp(),_("Downloading add-on..."),cfg,sock);
		if(!res) {
			return;
		}

		if(cfg.child("error") != NULL) {
			gui::show_error_message(disp(), (*cfg.child("error"))["message"]);
			return;
		}

		if(!check_names_legal(cfg)) {
			gui::show_error_message(disp(), "The add-on has an invalid file or directory name and can not be installed.");
			return;
		}

		//remove any existing versions of the just downloaded campaign
		//assuming it consists of a dir and a cfg file
		remove_campaign(campaigns[index]);

		//add revision info to the addon
                config *maindir = cfg.find_child("dir", "name", campaigns[index]);
                if (maindir) {
                    config f;
                    f["name"] = "info.cfg";
                    std::string s;
                    s += "[info]\n";
                    s += "version=\"" + versions[index] + "\"\n";
                    s += "uploads=\"" + uploads[index] + "\"\n";
                    s += "[/info]\n";
                    f["contents"] = s;
                    maindir->add_child("file", f);
                }

		//put a break at line below to see that it really works.
		unarchive_campaign(cfg);

		reload_changed_game_config();

		std::string warning = "";
		std::vector<config *> scripts = find_scripts(cfg, ".unchecked");
		if (!scripts.empty()) {
			warning += "\nUnchecked script files found:";
			std::vector<config *>::iterator i;
			for (i = scripts.begin(); i != scripts.end(); ++i) {
				warning += "\n" + (**i)["name"];
			}
		}

		/* GCC-3.3 needs a temp var otherwise compilation fails */
		gui::message_dialog dlg(disp(),_("Add-on Installed"),_("The add-on has been installed."));
		dlg.show();
	} catch(config::error&) {
		gui::show_error_message(disp(), _("Network communication error."));
	} catch(network::error&) {
		gui::show_error_message(disp(), _("Remote host disconnected."));
	} catch(io_exception&) {
		gui::show_error_message(disp(), _("There was a problem creating the files necessary to install this add-on."));
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

void game_controller::upload_campaign(const std::string& campaign, network::connection sock)
{
	config request_terms;
	request_terms.add_child("request_terms");
	// @todo Should be enabled once the campaign server can be recompiled.
	network::send_data(request_terms, sock, false);
	config data;
	sock = network::receive_data(data,sock,5000);
	if(!sock) {
		gui::show_error_message(disp(), _("Connection timed out"));
		return;
	} else if(data.child("error")) {
		std::string error_message = _("The server responded with an error: \"$error|\"");
		utils::string_map symbols;
		symbols["error"] = (*data.child("error"))["message"].str();
		error_message = utils::interpolate_variables_into_string(error_message, &symbols);
		gui::show_error_message(disp(), error_message);
		return;
	} else if(data.child("message")) {
		const int res = gui::dialog(disp(),_("Terms"),(*data.child("message"))["message"],gui::OK_CANCEL).show();
		if(res != 0) {
			return;
		}
	}

	config cfg;
	get_campaign_info(campaign,cfg);

	std::string passphrase = cfg["passphrase"];
	if(passphrase.empty()) {
		passphrase.resize(8);
		for(size_t n = 0; n != 8; ++n) {
			passphrase[n] = 'a' + (rand()%26);
		}
		cfg["passphrase"] = passphrase;
		set_campaign_info(campaign,cfg);
	}

	cfg["name"] = campaign;

	config campaign_data;
	archive_campaign(campaign,campaign_data);

	data.clear();
	data.add_child("upload",cfg).add_child("data",campaign_data);

	LOG_NET << "uploading campaign...\n";
	// @todo Should be enabled once the campaign server can be recompiled.
	network::send_data(data, sock, false);

	sock = dialogs::network_send_dialog(disp(),_("Sending add-on"),data,sock);
	if(!sock) {
		return;
	} else if(data.child("error")) {
		gui::show_error_message(disp(), _("The server responded with an error: \"") +
		                        (*data.child("error"))["message"].str() + '"');
	} else if(data.child("message")) {
		/* GCC-3.3 needs a temp var otherwise compilation fails */
		gui::message_dialog dlg(disp(),_("Response"),(*data.child("message"))["message"]);
		dlg.show();
	}
}

void game_controller::delete_campaign(const std::string& campaign, network::connection sock)
{
	config cfg;
	get_campaign_info(campaign,cfg);

	config msg;
	msg["name"] = campaign;
	msg["passphrase"] = cfg["passphrase"];

	config data;
	data.add_child("delete",msg);

	// @todo Should be enabled once the campaign server can be recompiled.
	network::send_data(data, sock, false);

	sock = network::receive_data(data,sock,5000);
	if(!sock) {
		gui::show_error_message(disp(), _("Connection timed out"));
	} else if(data.child("error")) {
		gui::show_error_message(disp(), _("The server responded with an error: \"") +
		                        (*data.child("error"))["message"].str() + '"');
	} else if(data.child("message")) {
		/* GCC-3.3 needs a temp var otherwise compilation fails */
		gui::message_dialog dlg(disp(),_("Response"),(*data.child("message"))["message"]);
		dlg.show();
	}
}

void game_controller::remove_campaign(const std::string& campaign)
{
	const std::string campaign_dir = get_user_data_dir() + "/data/campaigns/" + campaign;
	delete_directory(campaign_dir);
	if (file_exists(campaign_dir + ".cfg"))
		delete_directory(campaign_dir + ".cfg");
}


bool game_controller::play_multiplayer()
{
	int res;

	state_ = game_state();
	state_.campaign_type = "multiplayer";
	state_.campaign_define = "MULTIPLAYER";

	//Print Gui only if the user hasn't specified any server
	if( multiplayer_server_.empty() ){
		std::vector<std::string> host_or_join;
		std::string const pre = IMAGE_PREFIX + std::string("icons/icon-");
		char const sep1 = COLUMN_SEPARATOR, sep2 = HELP_STRING_SEPARATOR;

		host_or_join.push_back(pre + "server.png"
			+ sep1 + _("Join Official Server")
			+ sep2 + _("Log on to the official Wesnoth multiplayer server"));
		host_or_join.push_back(pre + "serverother.png"
			+ sep1 + _("Connect to Server")
			+ sep2 + _("Join a different server"));
		host_or_join.push_back(pre + "hotseat.png"
			+ sep1 + _("Local Game")
			+ sep2 + _("Play a multiplayer game with the AI or humans sharing the same machine"));

		std::string login = preferences::login();

		{
			gui::dialog d(disp(), _("Multiplayer"), "", gui::OK_CANCEL);
			d.set_menu(host_or_join);
			d.set_textbox(_("Login: "), login, mp::max_login_size, font::relative_size(250));
			res = d.show();
			login = d.textbox_text();
		}
		if (res < 0)
			return false;


		preferences::set_login(login);

	}else{
		res = 3;
	}

	try {

		/* do */ {
			reset_defines_map();
			defines_map_[state_.campaign_define] = preproc_define();
			refresh_game_cfg();
			events::discard(INPUT_MASK); // prevent the "keylogger" effect
			cursor::set(cursor::NORMAL);
		}

		if(res == 2) {
			std::vector<std::string> chat;
			config game_data;

			const mp::controller cntr = mp::CNTR_LOCAL;
			const bool is_server = false;

			mp::start_server(disp(), game_config_, cntr, is_server);

		} else if(res == 0 || res == 1 || res == 3 ) {
			std::string host;
			if(res == 0) {
				host = preferences::server_list().front().address;
			}else if(res == 3){
				host = multiplayer_server_;
				multiplayer_server_ = "";
			}
			mp::start_client(disp(), game_config_, host);
		}
	} catch(game::load_game_failed& e) {
		gui::show_error_message(disp(), _("The game could not be loaded: ") + e.message);
	} catch(game::game_error& e) {
		gui::show_error_message(disp(), _("Error while playing the game: ") + e.message);
	} catch(network::error& e) {
		if(e.message != "") {
			ERR_NET << "caught network::error: " << e.message << "\n";
			/* GCC-3.3 needs a temp var otherwise compilation fails */
			gui::dialog dlg(disp(),"",e.message,gui::OK_ONLY);
			dlg.show();
		} else {
			ERR_NET << "caught network::error\n";
		}
	} catch(config::error& e) {
		if(e.message != "") {
			ERR_CONFIG << "caught config::error: " << e.message << "\n";
			/* GCC-3.3 needs a temp var otherwise compilation fails */
			gui::dialog dlg2(disp(),"",e.message,gui::OK_ONLY);
			dlg2.show();
		} else {
			ERR_CONFIG << "caught config::error\n";
		}
	} catch(gamemap::incorrect_format_exception& e) {
		gui::show_error_message(disp(), std::string(_("The game map could not be loaded: ")) + e.msg_);
	} catch(game::load_game_exception& e) {
		//this will make it so next time through the title screen loop, this game is loaded
		loaded_game_ = e.game;
		loaded_game_show_replay_ = e.show_replay;
		loaded_game_cancel_orders_ = e.cancel_orders;
	} catch(twml_exception& e) {
		e.show(disp());
	}

	return false;
}

bool game_controller::change_language()
{
	if(new_widgets) {
			gui2::tlanguage_selection dlg;

			dlg.show(disp().video());
			
			if(dlg.get_retval() == gui2::tbutton::OK) {
				std::cerr << "OK\n";
				if(!no_gui_) {
					std::string wm_title_string = _("The Battle for Wesnoth");
					wm_title_string += " - " + game_config::revision;
					SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
				}

				refresh_game_cfg(true);
			} else {
				std::cerr << "Cancel\n";
			}

	} else {
		const std::vector<language_def>& languages = get_languages();
		std::vector<std::string> langs;

		for (std::vector<language_def>::const_iterator itor = languages.begin();
				itor != languages.end(); ++itor) {
			if (*itor == get_language()) {
				langs.push_back("*" + itor->language);
			} else {
				langs.push_back(itor->language);
			}
		}

		gui::dialog lmenu(disp(),_("Language"),
								 _("Choose your preferred language:"),
								 gui::OK_CANCEL);
		lmenu.set_menu(langs);
		const int res = lmenu.show();
		if(size_t(res) < langs.size()) {
			::set_language(languages[res]);
			preferences::set_language(languages[res].localename);

			if(!no_gui_) {
				std::string wm_title_string = _("The Battle for Wesnoth");
				wm_title_string += " - " + game_config::revision;
				SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
			}

			refresh_game_cfg(true);
		} else {
			return false;
		}

		font::load_font_config();
		hotkey::load_descriptions();
	}
	return true;
}

void game_controller::show_preferences()
{
	const preferences::display_manager disp_manager(&disp());
	preferences::show_preferences_dialog(disp(),game_config_);

	disp().redraw_everything();
}

void game_controller::show_upload_begging()
{
	upload_log_dialog::show_beg_dialog(disp());

	disp().redraw_everything();
}


void game_controller::read_configs(std::string& error_log)
{
	preproc_map defines_map(defines_map_);

	std::string user_error_log;
	//read the file and then write to the cache
	scoped_istream stream = preprocess_file("data/", &defines_map, &error_log);

	//reset the parse counter before reading the game files
	if (loadscreen::global_loadscreen) {
		loadscreen::global_loadscreen->parser_counter = 0;
	}

	read(game_config_, *stream, &error_log);

	//load usermade add-ons
	const std::string user_campaign_dir = get_user_data_dir() + "/data/campaigns/";
	std::vector<std::string> user_campaigns, error_campaigns;
	get_files_in_dir(user_campaign_dir,NULL,&user_campaigns,ENTIRE_FILE_PATH);
	for(std::vector<std::string>::const_iterator uc = user_campaigns.begin(); uc != user_campaigns.end(); ++uc) {
		std::string oldstyle_cfg = *uc + ".cfg";
		std::string main_cfg = *uc + "/_main.cfg";
		std::string toplevel;
		if (file_exists(oldstyle_cfg))
			toplevel = oldstyle_cfg;
		else if (file_exists(main_cfg))
			toplevel = main_cfg;
		else
			continue;

		try {
			preproc_map user_defines_map(defines_map);
			scoped_istream stream = preprocess_file(toplevel, &user_defines_map);

			std::string campaign_error_log;

			config user_campaign_cfg;
			read(user_campaign_cfg,*stream,&campaign_error_log);

			if(campaign_error_log.empty()) {
				game_config_.append(user_campaign_cfg);
			} else {
				user_error_log += campaign_error_log;
				error_campaigns.push_back(*uc);
			}
		} catch(config::error& err) {
			ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
			error_campaigns.push_back(*uc);

			user_error_log += err.message + "\n";
		} catch(preproc_config::error&) {
			ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
			error_campaigns.push_back(*uc);
			//no need to modify the error log here, already done by the preprocessor

		} catch(io_exception&) {
			ERR_CONFIG << "error reading usermade add-on '" << *uc << "'\n";
			error_campaigns.push_back(*uc);
		}
	}

	if(error_campaigns.empty() == false) {
		std::stringstream msg;
		msg << _n("The following add-on had errors and could not be loaded:",
				"The following add-ons had errors and could not be loaded:",
				error_campaigns.size());
		for(std::vector<std::string>::const_iterator i = error_campaigns.begin(); i != error_campaigns.end(); ++i) {
			msg << "\n" << *i;
		}

		msg << "\n" << _("ERROR DETAILS:") << "\n" << font::nullify_markup(user_error_log);

		gui::show_error_message(disp(),msg.str());
	}

	game_config_.merge_children("units");

	config& hashes = game_config_.add_child("multiplayer_hashes");
	for(config::child_list::const_iterator ch = game_config_.get_children("multiplayer").begin(); ch != game_config_.get_children("multiplayer").end(); ++ch) {
		hashes[(**ch)["id"]] = (*ch)->hash();
	}

}

//this function reads the game configuration, searching for valid cached copies first
void game_controller::read_game_cfg(bool use_cache)
{
	log_scope("read_game_cfg");

	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());

	bool is_valid = true;
	std::stringstream str;
	for(preproc_map::const_iterator i = defines_map_.begin(); i != defines_map_.end(); ++i) {
		if(i->second.value != "" || i->second.arguments.empty() == false) {
			is_valid = false;
			break;
		}

		str << " " << i->first;
	}
	//std::string localename = get_locale().localename;
	//str << "-lang_" << (localename.empty() ? "default" : localename);

	if(is_valid) {
		const std::string& cache = get_cache_dir();
		if(cache != "") {
			sha1_hash sha(str.str()); // use a hash for a shorter display of the defines
			const std::string fname = cache + "/cache-v" + game_config::version + "-" + sha.display();
			const std::string fname_checksum = fname + ".checksum";

			file_tree_checksum dir_checksum;

			if(use_cache && !force_valid_cache_) {
				try {
					if(file_exists(fname_checksum)) {
						config checksum_cfg;
						scoped_istream stream = istream_file(fname_checksum);
						read(checksum_cfg, *stream);
						dir_checksum = file_tree_checksum(checksum_cfg);
					}
				} catch(config::error&) {
					ERR_CONFIG << "cache checksum is corrupt\n";
				} catch(io_exception&) {
					ERR_CONFIG << "error reading cache checksum\n";
				}
			}

			if(force_valid_cache_) {
				LOG_CONFIG << "skipping cache validation (forced)\n";
			}

			if(use_cache && file_exists(fname) && (force_valid_cache_ || (dir_checksum == data_tree_checksum()))) {
				LOG_CONFIG << "found valid cache at '" << fname << "' using it\n";
				log_scope("read cache");
				try {
					scoped_istream stream = istream_file(fname);
					read_compressed(game_config_, *stream);
					set_unit_data();
					return;
				} catch(config::error&) {
					ERR_CONFIG << "cache is corrupt. Loading from files\n";
				} catch(io_exception&) {
					ERR_CONFIG << "error reading cache. Loading from files\n";
				}
			}

			LOG_CONFIG << "no valid cache found. Writing cache to '" << fname << " with defines_map "<< str.str() << "'\n";
			DBG_CONFIG << ((use_cache && file_exists(fname)) ? "yes":"no ") << " " << dir_checksum.modified << "==" << data_tree_checksum().modified << "  " << dir_checksum.nfiles << "==" << data_tree_checksum().nfiles << "  " << dir_checksum.sum_size << "==" << data_tree_checksum().sum_size << "\n";

			std::string error_log;

			read_configs(error_log);

			if(!error_log.empty()) {
				gui::show_error_message(disp(),
						_("Warning: Errors occurred while loading game configuration files: '") +
						font::nullify_markup(error_log));

			} else {
				try {
					scoped_ostream cache = ostream_file(fname);
					write_compressed(*cache, game_config_);
					config checksum_cfg;
					data_tree_checksum().write(checksum_cfg);
					scoped_ostream checksum = ostream_file(fname_checksum);
					write(*checksum, checksum_cfg);
				} catch(io_exception&) {
					ERR_FS << "could not write to cache '" << fname << "'\n";
				}
			}

			set_unit_data();
			return;
		}
	}

	ERR_CONFIG << "caching cannot be done. Reading file\n";

	std::string error_log;

	read_configs(error_log);
	if(!error_log.empty()) {
		gui::show_error_message(disp(),
				_("Warning: Errors occurred while loading game configuration files: '") +
				font::nullify_markup(error_log));

	}
	set_unit_data();
}

void game_controller::set_unit_data(){
    const config* const units = game_config_.child("units");
    if(units != NULL) {
        unit_type_data::types().set_config(*units);
    }
}

void game_controller::refresh_game_cfg(bool reset_translations)
{
	try {
		if(old_defines_map_.empty() || defines_map_ != old_defines_map_ || reset_translations) {
			cursor::setter cur(cursor::WAIT);

			if(!reset_translations) {
				game_config_.clear();
				read_game_cfg(use_caching_);
			} else {
				game_config_.reset_translation();
				// we may have translatable strings in [game_config]
				// e.g. team color names are defined there
				game_config::load_config(game_config_.child("game_config"));
			}

			old_defines_map_ = defines_map_;
		}
	} catch(config::error& e) {
		ERR_CONFIG << "Error loading game configuration files\n";
		gui::show_error_message(disp(), _("Error loading game configuration files: '") +
			font::nullify_markup(e.message) + _("' (The game will now exit)"));
		throw e;
	} catch(preproc_config::error& e) {
		ERR_CONFIG << "Error loading game configuration files\n";
		gui::show_error_message(disp(), _("Error loading game configuration files: '") +
			font::nullify_markup(e.message) + _("' (The game will now exit)"));
		throw e;
	}
}

void game_controller::reset_game_cfg()
{
	reset_defines_map();

	//load in the game's configuration files
#if defined(__APPLE__)
	defines_map_["APPLE"] = preproc_define();
#endif

	if(multiplayer_mode_) {
		defines_map_["MULTIPLAYER"] = preproc_define();
	} else {
		defines_map_["NORMAL"] = preproc_define();
		defines_map_["MEDIUM"] = preproc_define();
	}

	//refresh_game_cfg();
}

void game_controller::reset_defines_map()
{
	defines_map_.clear();

/* APPLE is only meant for configuration so it's not there */

#ifdef USE_TINY_GUI
	defines_map_["TINY"] = preproc_define();
#endif

#ifdef USE_SMALL_GUI
    defines_map_["SMALL_GUI"] = preproc_define();
#endif

#ifdef HAVE_PYTHON
	defines_map_["PYTHON"] = preproc_define();
#endif
}

void game_controller::play_game(RELOAD_GAME_DATA reload)
{
	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());
	loadscreen::global_loadscreen->set_progress(0, _("Loading data files"));
	if(reload == RELOAD_DATA) {
		if(state_.campaign_define.empty() == false) {
			defines_map_[state_.campaign_define] = preproc_define();
		}

		for( std::vector<std::string>::const_iterator i = state_.campaign_xtra_defines.begin();
		     i != state_.campaign_xtra_defines.end(); ++i) {
			defines_map_[*i] = preproc_define();
		}

		if(defines_map_.count("NORMAL")) {
			defines_map_["MEDIUM"] = preproc_define();
		}

		try {
			refresh_game_cfg();
		} catch(config::error&) {
			reset_game_cfg();
			refresh_game_cfg();
			return;
		}
	}

	loadscreen::global_loadscreen->set_progress(60);

	const binary_paths_manager bin_paths_manager(game_config_);

	try {
		// Only record log for single-player games & tutorial.
		upload_log log(state_.campaign_type.empty()
					   || state_.campaign_type == "scenario"
					   || state_.campaign_type == "tutorial");

		const LEVEL_RESULT result = ::play_game(disp(),state_,game_config_, log);
		// don't show The End for multiplayer scenario
		// change this if MP campaigns are implemented
		if((result == VICTORY || result == LEVEL_CONTINUE_NO_SAVE) && (state_.campaign_type.empty() || state_.campaign_type != "multiplayer")) {
			the_end(disp());
			about::show_about(disp(),state_.campaign);
		}
	} catch(game::load_game_exception& e) {

		//this will make it so next time through the title screen loop, this game is loaded
		loaded_game_ = e.game;
		loaded_game_show_replay_ = e.show_replay;
		loaded_game_cancel_orders_ = e.cancel_orders;

	} catch(twml_exception& e) {
		e.show(disp());
	}
}

} //end anon namespace

void game_controller::play_replay()
{
	const binary_paths_manager bin_paths_manager(game_config_);

	try {
		::play_replay(disp(),state_,game_config_,video_);

	} catch(game::load_game_exception& e) {

		//this will make it so next time through the title screen loop, this game is loaded
		loaded_game_ = e.game;
		loaded_game_show_replay_ = e.show_replay;
		loaded_game_cancel_orders_ = e.cancel_orders;

	} catch(twml_exception& e) {
		e.show(disp());
	}
}

game_controller::~game_controller()
{
	delete gui::empty_menu;
	sound::close_sound();
}

// this is needed to allow identical functionality with clean refactoring
// play_game only returns on an error, all returns within play_game can
// be replaced with this
static void safe_exit(int res) {

	LOG_GENERAL << "exiting with code " << res << "\n";
#ifdef OS2 /* required to correctly shutdown SDL on OS/2 */
        SDL_Quit();
#endif
	exit(res);
}

// maybe this should go in a util file somewhere?
static void gzip_codec(const std::string & input_file, const std::string & output_file, bool encode)
{
	try {
	std::ofstream ofile(output_file.c_str(), std::ios_base::out
			| std::ios_base::binary | std::ios_base::binary);
			std::ifstream ifile(input_file.c_str(),
			std::ios_base::in | std::ios_base::binary);
		boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
		if(encode)
			in.push(boost::iostreams::gzip_compressor());
		else
			in.push(boost::iostreams::gzip_decompressor());
		in.push(ifile);
		boost::iostreams::copy(in, ofile);
			ifile.close();
		safe_exit(remove(input_file.c_str()));
		}  catch(io_exception& e) {
		std::cerr << "IO error: " << e.what() << "\n";
	}
}

static void gzip_encode(const std::string & input_file, const std::string & output_file)
{
	gzip_codec(input_file, output_file, true);
}

static void gzip_decode(const std::string & input_file, const std::string & output_file)
{
	gzip_codec(input_file, output_file, false);
}


//! Process commandline-arguments
static int play_game(int argc, char** argv)
{
	//parse arguments that shouldn't require a display device
	int arg;
	for(arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val.empty()) {
			continue;
		}

		if(val == "--help" || val == "-h") {
			// When adding items don't forget to update doc/man/wesnoth.6
			std::cout << "usage: " << argv[0]
			<< " [OPTIONS] [DATA-DIRECTORY]\n"
			<< "  --bpp number                 sets BitsPerPixel value. Example: --bpp 32\n"
			<< "  --compress INFILE OUTFILE    compresses a savefile (INFILE) that is in text WML\n"
			<< "                               format into binary WML format (OUTFILE).\n"
			<< "  -d, --debug                  shows extra debugging information and enables\n"
			<< "                               additional command mode options in-game.\n"
			<< "  --decompress INFILE OUTFILE  decompresses a savefile (INFILE) that is in binary\n"
			<< "                               WML format into text WML format (OUTFILE).\n"
			<< "  -f, --fullscreen             runs the game in full screen mode.\n"
			<< "  --fps                        displays the number of frames per second the game\n"
			<< "                               is currently running at, in a corner of the screen.\n"
			<< "  --gunzip INFILE.gz           decompresses a file (INFILE.gz) in gzip format\n"
			<< "                               and stores it without the .gz suffix.\n"
			<< "                               INFILE.gz will be removed.\n"
			<< "  --gzip INFILE                compresses a file (INFILE) in gzip format,\n"
			<< "                               stores it as INFILE.gz and removes INFILE.\n"
			<< "  -h, --help                   prints this message and exits.\n"
			<< "  --load SAVEGAME              loads the file SAVEGAME from the standard save\n"
			<< "                               game directory.\n"
			<< "  --with-replay                replays the file SAVEGAME loaded with --load option.\n"
			<< "  --log-<level>=<domain1>,<domain2>,...\n"
			<< "                               sets the severity level of the log domains.\n"
			<< "                               'all' can be used to match any log domain.\n"
			<< "                               Available levels: error, warning, info, debug.\n"
			<< "                               By default the 'error' level is used.\n"
			<< "  --logdomains                 List defined log domains and exit.\n"
			<< "  --nocache                    disables caching of game data.\n"
			<< "  --validcache                 assume that cache is valid (dangerous)\n"
			<< "  --nosound                    runs the game without sounds and music.\n"
			<< "  --max-fps                    the maximum fps the game tries to run at the value\n"
			<< "                               should be between the 1 and 1000, the default is 50.\n"
			<< "  --path                       prints the name of the game data directory and exits.\n"
#ifdef HAVE_PYTHON
			<< "  --python-api                 prints the runtime documentation for the python API.\n"
#endif
			<< "  -r, --resolution XxY         sets the screen resolution. Example: -r 800x600\n"
			<< "  -t, --test                   runs the game in a small test scenario.\n"
			<< "  -v, --version                prints the game's version number and exits.\n"
			<< "  -w, --windowed               runs the game in windowed mode.\n"
			<< "  --no-delay                   run the game without any delays.\n"
			<< "  -c, --campaign               skip menu, and go directly to campaign selection menu.\n"
			<< "  -s, --server [host]          skip menu, and connect to the host if specified or to the first host in your preferences.\n"
			<< "  -m, --multiplayer            runs a multiplayer game. There are additional\n"
			<< "                               options that can be used as explained below:\n"
			<< "  --algorithm<number>=value    selects a non-standard algorithm to be used by the\n"
			<< "                               AI controller for this side.\n"
			<< "  --controller<number>=value   selects the controller for this side.\n"
			<< "  --era=value                  selects the era to be played in by its id.\n"
			<< "  --nogui                      runs the game without the GUI. Must appear before\n"
			<< "                               --multiplayer to have the desired effect.\n"
			<< "  --parm<number>=name:value    sets additional parameters for this side.\n"
			<< "  --scenario=value             selects a multiplayer scenario. The default\n"
			<< "                               scenario is \"multiplayer_The_Freelands\".\n"
			<< "  --side<number>=value         selects a faction of the current era for this side\n"
			<< "                               by id.\n"
			<< "  --turns=value                sets the number of turns. The default is \"50\".\n"
			<< "  --exit-at-end                exit Wesnoth at end of scenario.\n"
			<< "  --new-widgets                there is a new WIP widget toolkit this switch enables the new toolkit\n"
			<< "                               (VERY EXPERIMENTAL don't file bug reports since most are known).\n"
			;
			return 0;
		} else if(val == "--version" || val == "-v") {
			std::cout << _("Battle for Wesnoth") << " " << game_config::version
			          << "\n";
			return 0;
		} else if(val == "--path") {
			std::cout <<  game_config::path
			          << "\n";
			return 0;
#ifdef HAVE_PYTHON
		} else if(val == "--python-api") {
			python_ai::invoke("documentation.py");
			return 0;
#endif
		} else if (val.substr(0, 6) == "--log-") {
			size_t p = val.find('=');
			if (p == std::string::npos) {
				std::cerr << "unknown option: " << val << '\n';
				return 0;
			}
			std::string s = val.substr(6, p - 6);
			int severity;
			if (s == "error") severity = 0;
			else if (s == "warning") severity = 1;
			else if (s == "info") severity = 2;
			else if (s == "debug") severity = 3;
			else {
				std::cerr << "unknown debug level: " << s << '\n';
				return 0;
			}
			while (p != std::string::npos) {
				size_t q = val.find(',', p + 1);
				s = val.substr(p + 1, q == std::string::npos ? q : q - (p + 1));
				if (!lg::set_log_domain_severity(s, severity)) {
					std::cerr << "unknown debug domain: " << s << '\n';
					return 0;
				}
				p = q;
			}
		//! @todo the (de)compress will be removed, the feature is broken for quite
		//! a while and is replaced with --g(un)zip. 1.5.1 would be a nice point for
		//! removal.
		} else if(val == "--compress" || val == "--decompress") {
			if(argc != arg+3) {
				std::cerr << "format of " << val << " command: " << val << " <input file> <output file>\n";
				return 0;
			}

			const std::string input(argv[arg+1]);
			const std::string output(argv[arg+2]);

			scoped_istream stream = istream_file(input);
			if (stream->fail()) {
				std::cerr << "could not read file '" << input << "'\n";
				return 0;
			}

			config cfg;

			const bool compress = val == "--compress";
			try {
				const bool is_compressed = detect_format_and_read(cfg, *stream);
				if(is_compressed && compress) {
					std::cerr << input << " is already compressed\n";
					return 0;
				} else if(!is_compressed && !compress) {
					std::cerr << input << " is already decompressed\n";
					return 0;
				}

				scoped_ostream output_stream = ostream_file(output);
				write_possibly_compressed(*output_stream, cfg, compress);
			} catch(config::error& e) {
				std::cerr << input << " is not a valid Wesnoth file: " << e.message << "\n";
			} catch(io_exception& e) {
				std::cerr << "IO error: " << e.what() << "\n";
			}

			return 0;

		} else if(val == "--gzip") {
			if(argc != arg + 2) {
				std::cerr << "format of " << val << " command: " << val << " <input file>\n";
				return 2;
			}

			const std::string input_file(argv[arg + 1]);
			const std::string output_file(input_file + ".gz");
			gzip_encode(input_file, output_file); 

		} else if(val == "--gunzip") {
			if(argc != arg + 2) {
				std::cerr << "format of " << val << " command: " << val << " <input file>\n";
				return 2;
			}

			const std::string input_file(argv[arg + 1]);
			if(! is_gzip_file(input_file)) {

				std::cerr << "file '" << input_file << "'isn't a .gz file\n";
				return 2;
			}
			const std::string output_file(
				input_file, 0, input_file.length() - 3);

			gzip_decode(input_file, output_file);

		} else if(val == "--logdomains") {
			std::cout << lg::list_logdomains() << "\n";
			return 0;
		}

	}

	srand(time(NULL));

	game_controller game(argc,argv);
	const int start_ticks = SDL_GetTicks();

	// I would prefer to setup locale first so that early error
	// messages can get localized, but we need the game_controller
	// initialized to have get_intl_dir() to work.  Note: this
	// setlocale() but this does not take GUI language setting
	// into account.
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain (PACKAGE, intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	bindtextdomain (PACKAGE "-lib", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-lib", "UTF-8");
	textdomain (PACKAGE);

	bool res;

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if(res == false) {
		std::cerr << "could not initialize fonts\n";
		return 0;
	}

	res = game.init_video();
	if(res == false) {
		std::cerr << "could not initialize display\n";
		return 0;
	}

	const cursor::manager cursor_manager;
	cursor::set(cursor::WAIT);

	loadscreen::global_loadscreen = new loadscreen(game.disp().video());
	loadscreen::global_loadscreen->clear_screen();

	res = game.init_language();
	if(res == false) {
		std::cerr << "could not initialize the language\n";
		return 0;
	}

	loadscreen::global_loadscreen->increment_progress(5, _("Loading game configuration."));
	res = game.init_config();
	if(res == false) {
		std::cerr << "could not initialize game config\n";
		return 0;
	}
	loadscreen::global_loadscreen->increment_progress(10, _("Re-initialize fonts for the current language."));

	res = font::load_font_config();
	if(res == false) {
		std::cerr << "could not re-initialize fonts for the current language\n";
		return 0;
	}

#if defined(_X11) && !defined(__APPLE__)
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

	config tips_of_day;

	loadscreen::global_loadscreen->set_progress(100, _("Loading title screen."));
	delete loadscreen::global_loadscreen;
	loadscreen::global_loadscreen = NULL;

	LOG_CONFIG << "time elapsed: "<<  (SDL_GetTicks() - start_ticks) << " ms\n";

	for(int first_time = true;;first_time = false){
        //init_config already processed the configs, so we don't need to do it for the
        //first loop pass.
		if (!first_time){
			//make sure the game config is always set to how it should be at the title screen
			//game.reset_game_cfg();
		}

		// reset the TC, since a game can modify it, and it may be used
		// by images in add-ons or campaigns dialogs
		image::set_team_colors();

		statistics::fresh_stats();

		sound::play_music_repeatedly(game_config::title_music);

		if(game.play_test() == false) {
			return 0;
		}

		if(game.play_multiplayer_mode() == false) {
			return 0;
		}

		recorder.clear();

		//Start directly a campaign
		if(game.goto_campaign() == false){
			continue; //Go to main menu
		}

		//Start directly a multiplayer
		//Eventually with a specified server
		if(game.goto_multiplayer() == false){
			continue; //Go to main menu
		}


		gui::TITLE_RESULT res = game.is_loading() ? gui::LOAD_GAME : gui::TIP_NEXT;

		while(res == gui::TIP_NEXT) {
			res = gui::show_title(game.disp(),tips_of_day);
		}

		game_controller::RELOAD_GAME_DATA should_reload = game_controller::RELOAD_DATA;
		if(res == gui::QUIT_GAME) {
			LOG_GENERAL << "quitting game...\n";
			return 0;
		} else if(res == gui::LOAD_GAME) {
			if(game.load_game() == false) {
				continue;
			}

			should_reload = game_controller::NO_RELOAD_DATA;
		} else if(res == gui::TUTORIAL) {
			game.set_tutorial();
		} else if(res == gui::NEW_CAMPAIGN) {
			if(game.new_campaign() == false) {
				continue;
			}
		} else if(res == gui::MULTIPLAYER) {
			if(game.play_multiplayer() == false) {
				continue;
			}
		} else if(res == gui::CHANGE_LANGUAGE) {
			if(game.change_language() == true) {
				tips_of_day.clear();
			}
			continue;
		} else if(res == gui::EDIT_PREFERENCES) {
			game.show_preferences();
			continue;
		} else if(res == gui::SHOW_ABOUT) {
			about::show_about(game.disp());
			continue;
		} else if(res == gui::SHOW_HELP) {
			help::help_manager help_manager(&game.game_config(), NULL);
			help::show_help(game.disp());
			continue;
		} else if(res == gui::GET_ADDONS) {
			game.manage_addons();
			continue;
		} else if(res == gui::BEG_FOR_UPLOAD) {
			game.show_upload_begging();
			continue;
#ifdef MAP_EDITOR
		} else if(res == gui::START_MAP_EDITOR) {
			gui::show_error_message(game.disp(), "The map editor is not available. Yet.");
			continue;
#endif
		}

		if (recorder.at_end()){
			game.play_game(should_reload);
		}
		else{
			game.play_replay();
		}
	}

	return 0;
}

int main(int argc, char** argv)
{
#ifdef OS2 /* required for SDL_GetTicks to work on OS/2 */
        if(SDL_Init(SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}
#endif

	try {
		std::cerr << "Battle for Wesnoth v" << game_config::revision << '\n';
		time_t t = time(NULL);
		std::cerr << "Started on " << ctime(&t) << "\n";

		const int res = play_game(argc,argv);
		safe_exit(res);
	} catch(CVideo::error&) {
		std::cerr << "Could not initialize video. Exiting.\n";
	} catch(font::manager::error&) {
		std::cerr << "Could not initialize fonts. Exiting.\n";
	} catch(config::error& e) {
		std::cerr << e.message << "\n";
	} catch(gui::button::error&) {
		std::cerr << "Could not create button: Image could not be found\n";
	} catch(CVideo::quit&) {
		//just means the game should quit
	} catch(end_level_exception&) {
		std::cerr << "caught end_level_exception (quitting)\n";
	} catch(std::bad_alloc&) {
		std::cerr << "Ran out of memory. Aborted.\n";
	} catch(twml_exception& e) {
		std::cerr << "WML exception:\nUser message: "
			<< e.user_message << "\nDev message: " << e.dev_message << '\n';
	}

	return 0;
} // end main

