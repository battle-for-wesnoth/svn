/*
  Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
  Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/


#include "editor.hpp"
#include "../config.hpp"
#include "../game_config.hpp"
#include "../filesystem.hpp"
#include "../font.hpp"
#include "../image.hpp"
#include "../map.hpp"
#include "../team.hpp"
#include "../util.hpp"
#include "../preferences.hpp"
#include "../language.hpp"
#include "../cursor.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv)
{
	game_config::editor = true;

	int arg;
	for(arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val.empty()) {
			continue;
		}

 		if(val == "--help" || val == "-h") {
 			std::cout << "usage: " << argv[0]
 		    << " [options] [map]\n"
 			<< "  -f, --fullscreen  Runs the game in full-screen\n"
 			<< "  -h, --help        Prints this message and exits\n"
 			<< "  --path            Prints the name of the game data directory and exits\n"
 			<< "  -w, --windowed    Runs the game in windowed mode\n"
 			<< "  -v, --version     Prints the game's version number and exits\n"
 			<< "  --resolution      Set the resolution of the window\n"
   		    << "  --datadir         Select the data directory to use\n";
 			return 0;
 		} else if(val == "--version" || val == "-v") {
 			std::cout << "Battle for Wesnoth " << game_config::version
 			          << "\n";
 			return 0;
 		} else if(val == "--path") {
 			std::cout <<  game_config::path
 			          << "\n";
 			return 0;
		}
	}

	CVideo video;

	const font::manager font_manager;
	const preferences::manager prefs_manager;
	const image::manager image_manager;
	std::string filename = "";
	std::string mapdata;

	for(arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val.empty()) {
			continue;
		}
		if(val == "--resolution" || val == "-r") {
			if(arg+1 != argc) {
				++arg;
				const std::string val(argv[arg]);
				const std::vector<std::string> res = config::split(val,'x');
				if(res.size() == 2) {
					const int xres = lexical_cast_default<int>(res.front());
					const int yres = lexical_cast_default<int>(res.back());
					if(xres > 0 && yres > 0) {
						const std::pair<int,int> resolution(xres,yres);
						preferences::set_resolution(resolution);
					}
				}
			}
		} else if(val == "--windowed" || val == "-w") {
			preferences::set_fullscreen(false);
		} else if(val == "--fullscreen" || val == "-f") {
			preferences::set_fullscreen(true);
		} else if(val == "--datadir") {
			if (arg+1 != argc) {
				arg++;
				const std::string val(argv[arg]);
				if (!is_directory(val)) {
					std::cerr << "Could not find directory '" << val << "'\n";
					return 1;
				}
				game_config::path = val;
			}
		} else if(val[0] == '-') {
			std::cerr << "unknown option: " << val << "\n";
			return 0;
		} else {
			filename = val;
			try {
				mapdata = read_file(filename);
			}
			catch (io_exception) {
				std::cerr << "Could not read the map file, sorry." << std::endl;
				return 1;
			}
		}
	}
	// Blatant cut and paste from game.cpp
	image::set_wm_icon();
	int video_flags = preferences::fullscreen() ? FULL_SCREEN : 0;
	std::pair<int,int> resolution = preferences::resolution();
	
	std::cerr << "checking mode possible...\n";
	const int bpp = video.modePossible(resolution.first,resolution.second,16,video_flags);
	
	std::cerr << bpp << "\n";
	
	if(bpp == 0) {
		//Video mode not supported, maybe from bad prefs.
		std::cerr << "The video mode, " << resolution.first
				  << "x" << resolution.second << "x16 "
				  << "is not supported\nAttempting 1024x768x16...\n";
		
		//Attempt 1024x768.
		resolution.first = 1024;
		resolution.second = 768;
		
		int bpp = video.modePossible(resolution.first,resolution.second,16,video_flags);
	
		if(bpp == 0) {
				 //Attempt 1024x768.
			resolution.first = 1024;
			resolution.second = 768;
			std::cerr << "1024x768x16 is not possible.\nAttempting 800x600x16...\n";
			
			resolution.first = 800;
			resolution.second = 600;
			
			bpp = video.modePossible(resolution.first,resolution.second,16,video_flags);
		}
		
		if(bpp == 0) {
			//couldn't do 1024x768 or 800x600
			
			std::cerr << "The required video mode, " << resolution.first
					  << "x" << resolution.second << "x16 "
					  << "is not supported\n";
			
			return 0;
		}
	}
	
	std::cerr << "setting mode to " << resolution.first << "x" << resolution.second << "\n";
	const int res = video.setMode(resolution.first,resolution.second,bpp,video_flags);
	video.setBpp(bpp);
	if(res == 0) {
		std::cerr << "required video mode, " << resolution.first << "x"
				  << resolution.second << "x" << bpp << " is not supported\n";
		return 0;
	}
	preproc_map defines_map;
	defines_map["MEDIUM"] = preproc_define();
	defines_map["NORMAL"] = preproc_define();
	// Multiplayer define is needed to access the random map generator
	// settings. Maybe they should be moved? Or set an EDITOR define and
	// make it load that way maybe.
	defines_map["MULTIPLAYER"] = preproc_define();
	config cfg;
	try {
		cfg.read(preprocess_file("data/game.cfg", &defines_map));
	}
	catch (config::error e) {
		std::cerr << "Error when reading game config: '" << e.message << "'" << std::endl;
	}

	set_language("English");

	if(mapdata.empty()) {
		for(int i = 0; i != 20; ++i) {
			mapdata = mapdata + "gggggggggggggggggggg\n";
		}
	}
	
	srand(time(NULL));
	bool done = false;
	gamestatus status(cfg, 0);
	std::vector<team> teams;
	// Add a dummy team so the reports will be handled properly.
	teams.push_back(team(cfg));
	config* theme_cfg = cfg.find_child("theme", "name", "editor");
	config dummy_theme("");
	if (!theme_cfg) {
		std::cerr << "Editor theme could not be loaded." << std::endl;
		theme_cfg = &dummy_theme;
	}

	std::cerr << "entering while...\n";
	std::map<gamemap::location,unit> units;
	events::event_context ec;
	while (! done) {
		try {
			std::cerr << "creating map...\n";
			gamemap map(cfg, mapdata);

			std::cerr << "Using theme cfg: " << std::endl << theme_cfg->write() << std::endl;
			display gui(units, video, map, status, teams,
				    *theme_cfg, cfg);
	
			map_editor::map_editor editor(gui, map, *theme_cfg, cfg);
			editor.set_file_to_save_as(filename);
			editor.main_loop();
			done = true;
		}
		catch (map_editor::map_editor::new_map_exception &e) {
			mapdata = e.new_map;
			filename = e.new_filename;
		}
		catch (gamemap::incorrect_format_exception) {
			std::cerr << "The map is not in a correct format, sorry." << std::endl;
			return 1;
		}
	}
	return 0;
}
