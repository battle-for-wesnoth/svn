/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "SDL.h"

#include "actions.hpp"
#include "ai.hpp"
#include "config.hpp"
#include "dialogs.hpp"
#include "display.hpp"
#include "font.hpp"
#include "game_config.hpp"
#include "game_events.hpp"
#include "gamestatus.hpp"
#include "key.hpp"
#include "language.hpp"
#include "menu.hpp"
#include "multiplayer.hpp"
#include "pathfind.hpp"
#include "playlevel.hpp"
#include "preferences.hpp"
#include "replay.hpp"
#include "sound.hpp"
#include "team.hpp"
#include "unit_types.hpp"
#include "unit.hpp"
#include "video.hpp"
#include "widgets/button.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

LEVEL_RESULT play_game(display& disp, game_state& state, config& game_config,
				       game_data& units_data, CVideo& video)
{
	std::string type = state.campaign_type;
	if(type.empty())
		type = "scenario";
	const std::vector<config*>& scenarios = game_config.children[type];

	for(int i = state.scenario; i < scenarios.size(); ++i) {
		std::vector<config*>& story = scenarios[i]->children["story"];

		try {
			LEVEL_RESULT res = REPLAY;

			state.label = scenarios[i]->values["name"];

			recorder.set_save_info(state);

			while(res == REPLAY) {
				state = recorder.get_save_info();
				if(!recorder.empty()) {
					recorder.start_replay();
				}

				res = play_level(units_data,game_config,scenarios[i],
				                 video,state,story);
			}

			recorder.clear();
			state.replay_data.clear();

			if(res != VICTORY) {
				return res;
			}
		} catch(gamestatus::load_game_failed& e) {
			std::cerr << "The game could not be loaded: " << e.message << "\n";
			return QUIT;
		} catch(gamestatus::game_error& e) {
			std::cerr << "An error occurred while playing the game: "
			          << e.message << "\n";	
			return QUIT;
		} catch(gamemap::incorrect_format_exception& e) {
			std::cerr << "The game map could not be loaded: " << e.msg_ << "\n";
			return QUIT;
		}

		//skip over any scenarios which should be skipped, because their
		//conditions aren't met
		while(i+1 < scenarios.size()) {
			bool skip = false;

			std::vector<config*>& conditions =
			                    scenarios[i+1]->children["condition"];
			for(std::vector<config*>::iterator cond = conditions.begin();
			    cond != conditions.end(); ++cond) {
				if(game_events::conditional_passed(state,NULL,**cond) == false)
					skip = true;
			}

			if(!skip)
				break;

			++i;
		}

		//if this isn't the last scenario, then save the game
		if(i+1 < scenarios.size()) {
			state.label = scenarios[i+1]->values["name"];
			state.scenario = i+1;
			
			const int should_save = gui::show_dialog(disp,NULL,"",
			                    string_table["save_game_message"],
			                    gui::YES_NO,NULL,NULL,
								string_table["save_game_label"],&state.label);

			if(should_save == 0) {
				save_game(state);
			}
		}
	}

	return VICTORY;
}

int play_game(int argc, char** argv)
{
	std::string text_chr = read_file("data/text.chr");
	text_chr.resize(256*8);

	CVideo video(text_chr.c_str());
	const font::manager font_manager;
	const sound::manager sound_manager;
	const preferences::manager prefs_manager;

	std::map<std::string,std::string> defines_map;
	defines_map["NORMAL"] = "";
	config game_config(preprocess_file("data/game.cfg", &defines_map));
	
	const std::vector<config*>& units = game_config.children["units"];
	if(units.empty()) {
		std::cerr << "Could not find units configuration\n";
		std::cerr << game_config.write();
		return 0;
	}

	game_data units_data(*units[0]);

	const bool lang_res = set_language(get_locale(), game_config);
	if(!lang_res) {
		std::cerr << "No translation for locale '" << get_locale()
		          << "', default to locale 'en'\n";

		const bool lang_res = set_language("en", game_config);
		if(!lang_res) {
			std::cerr << "Language data not found\n";
		}
	}
	
	bool test_mode = false;
	int video_flags = preferences::fullscreen() ? FULL_SCREEN : 0;
	
	for(int arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val == "-windowed") {
			video_flags = 0;
		} else if(val == "-test") {
			test_mode = true;
		} else if(val == "-debug") {
			game_config::debug = true;
		}
	}
	
	const int bpp = video.modePossible(1152,864,16,video_flags);

	if(bpp == 0) {
		std::cerr << "The required video mode, 1024x768x16 "
		          << "is not supported\n";

		if(video_flags == FULL_SCREEN && argc == 0)
			std::cerr << "Try running the program with the -windowed option "
			          << "using a 16bpp X windows setting\n";

		if(video_flags == 0 && argc == 0)
			std::cerr << "Try running with the -fullscreen option\n";

		return 0;
	}

	if(bpp != 16) {
		std::cerr << "Video mode must be emulated; the game may run slowly. "
		          << "For best results, run the program on a 16 bpp display\n";
	}

	const int res = video.setMode(1152,864,16,video_flags);
	if(res != 16) {
		std::cerr << "required video mode, 1024x768x16 is not supported\n";
		return 0;
	}

	SDL_WM_SetCaption(string_table["game_title"].c_str(), NULL);

	for(;;) {
		sound::play_music("wesnoth-1.ogg");

		game_state state;

		display::unit_map u_map;
		config dummy_cfg("");
		display disp(u_map,video,gamemap(dummy_cfg,"1"),gamestatus(0),
		             std::vector<team>());

		if(test_mode) {
			state.campaign_type = "test";
			state.scenario = 0;

			play_game(disp,state,game_config,units_data,video);
			return 0;
		}
		
		gui::TITLE_RESULT res = gui::show_title(disp);
		if(res == gui::QUIT_GAME) {
			return 0;
		} else if(res == gui::LOAD_GAME) {
			srand(SDL_GetTicks());
		   
			const std::vector<std::string>& games = get_saves_list();

			if(games.empty()) {
				gui::show_dialog(disp,NULL,
				                 string_table["no_saves_heading"],
								 string_table["no_saves_message"],
				                 gui::OK_ONLY);
				continue;
			}

			const int res = gui::show_dialog(disp,NULL,
							 string_table["load_game_heading"],
							 string_table["load_game_message"],
					         gui::OK_CANCEL, &games);
			if(res == -1)
				continue;

			try {
				load_game(units_data,games[res],state);
				defines_map.clear();
				defines_map[state.difficulty] = "";
			} catch(gamestatus::load_game_failed& e) {
				gui::show_dialog(disp,NULL,string_table["bad_save_heading"],
				           string_table["bad_save_message"],gui::OK_ONLY);
				continue;
			}

			recorder = replay(state.replay_data);
			if(!recorder.empty()) {
				const int res = gui::show_dialog(disp,NULL,
				               "", string_table["replay_game_message"],
							   gui::YES_NO);
				//if yes, then show the replay, otherwise
				//skip showing the replay
				if(res == 0) {
					recorder.set_skip(0);
				} else {
					std::cerr << "skipping...\n";
					recorder.set_skip(-1);
				}
			}

			if(state.campaign_type == "multiplayer") {
				recorder.set_save_info(state);
				std::vector<config*> story;

				try {
					play_level(units_data,game_config,&state.starting_pos,
					           video,state,story);
					recorder.clear();
				} catch(gamestatus::load_game_failed& e) {
					std::cerr << "error loading the game: " << e.message
					          << "\n";
					return 0;
				} catch(gamestatus::game_error& e) {
					std::cerr << "error while playing the game: "
					          << e.message << "\n";
					return 0;
				}

				continue;
			}
		} else if(res == gui::TUTORIAL) {
			state.campaign_type = "tutorial";
			state.scenario = 0;
		} else if(res == gui::NEW_CAMPAIGN) {
			state.campaign_type = "scenario";
			state.scenario = 0;

			static const std::string difficulties[] = {"EASY","NORMAL","HARD"};
			const int ndiff = sizeof(difficulties)/sizeof(*difficulties);
			std::vector<std::string> options;

			for(int i = 0; i != ndiff; ++i) {
				options.push_back(string_table[difficulties[i]]);
				if(options.back().empty())
					options.back() = difficulties[i];
			}

			const int res = gui::show_dialog(disp,NULL,"",
			                            string_table["difficulty_level"],
			                            gui::OK_CANCEL,&options);
			if(res == -1)
				continue;

			assert(res >= 0 && res < options.size());

			state.difficulty = difficulties[res];
			defines_map.clear();
			defines_map[difficulties[res]] = "";
		} else if(res == gui::MULTIPLAYER) {
			state.campaign_type = "multiplayer";
			state.scenario = 0;

			try {
				play_multiplayer(disp,units_data,game_config,state);
			} catch(gamestatus::load_game_failed& e) {
				std::cerr << "error loading the game: " << e.message
				          << "\n";
				return 0;
			} catch(gamestatus::game_error& e) {
				std::cerr << "error while playing the game: "
				          << e.message << "\n";
				return 0;
			}

			continue;
		} else if(res == gui::CHANGE_LANGUAGE) {

			const std::vector<std::string>& langs = get_languages(game_config);
			const int res = gui::show_dialog(disp,NULL,"",
			                         string_table["language_button"] + ":",
			                         gui::OK_CANCEL,&langs);
			if(res >= 0 && res < langs.size()) {
				set_language(langs[res], game_config);
				preferences::set_locale(langs[res]);
			}
			continue;
		}
		
		//make a new game config item based on the difficulty level
		config game_config(preprocess_file("data/game.cfg", &defines_map));

		const std::vector<config*>& units = game_config.children["units"];
		if(units.empty()) {
			std::cerr << "Could not find units configuration\n";
			std::cerr << game_config.write();
			return 0;
		}

		game_data units_data(*units[0]);

		const LEVEL_RESULT result = play_game(disp,state,game_config,
		                                      units_data,video);
		if(result == VICTORY) {
			gui::show_dialog(disp,NULL,
			  string_table["end_game_heading"],
			  string_table["end_game_message"],
			  gui::OK_ONLY);
		}
	}

	return 0;
}

int main(int argc, char** argv)
{
	try {
		return play_game(argc,argv);
	} catch(CVideo::error&) {
		std::cerr << "Could not initialize video\n";
	} catch(config::error& e) {
		std::cerr << e.message << "\n";
	} catch(gui::button::error&) {
		std::cerr << "Could not create button: Image could not be found\n";
	}

	return 0;
}
