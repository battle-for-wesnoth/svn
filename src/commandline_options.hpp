/* $Id$ */
/*
   Copyright (C) 2011 by Lukasz Dobrogowski <lukasz.dobrogowski@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef COMMANDLINE_OPTIONS_HPP_INCLUDED
#define COMMANDLINE_OPTIONS_HPP_INCLUDED

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <string>
#include <utility>
#include <vector>

class commandline_options
{
public:
	commandline_options(int argc, char **argv);

	/// BitsPerPixel specified by --bpp option.
	boost::optional<int> bpp;
	/// Non-empty if --campaign was given on the command line. ID of the campaign we want to start.
	boost::optional<std::string> campaign;
	/// Non-empty if --campaign-difficulty was given on the command line. Numerical difficulty of the campaign to be played. Dependant on --campaign.
	boost::optional<int> campaign_difficulty;
	/// Non-empty if --campaign-scenario was given on the command line. Chooses starting scenario in the campaign to be played. Dependant on --campaign.
	boost::optional<std::string> campaign_scenario;
	/// True if --clock was given on the command line. Enables
	bool clock;
	/// True if --config-path was given on the command line. Prints path to user config directory and exits.
	bool config_path;
	/// Non-empty if --config-dir was given on the command line. Sets the config dir to the specified one.
	boost::optional<std::string> config_dir;
	/// Non-empty if --config-dir was given on the command line. Sets the config dir to the specified one.
	boost::optional<std::string> data_dir;
	/// True if --debug was given on the command line. Enables debug mode.
	bool debug;
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
	/// Non-empty if --debug-dot-level was given on the command line.
	boost::optional<std::string> debug_dot_level;
	/// Non-empty if --debug-dot-domain was given on the command line.
	boost::optional<std::string> debug_dot_domain;
#endif
	/// Non-empty if --editor was given on the command line. Goes directly into editor. If string is longer than 0, it contains path to the file to edit.
	boost::optional<std::string> editor;
	/// True if --fps was given on the command line. Shows number of fps.
	bool fps;
	/// True if --fullscreen was given on the command line. Starts Wesnoth in fullscreen mode.
	bool fullscreen;
	/// Non-empty if --gunzip was given on the command line. Uncompresses a .gz file and exits.
	boost::optional<std::string> gunzip;
	/// Non-empty if --gzip was given on the command line. Compresses a file to .gz and exits.
	boost::optional<std::string> gzip;
	/// Contains parsed arguments of --log-* (e.g. --log-debug).
	/// Vector of pairs (severity, log domain).
	boost::optional<std::vector<std::pair<int, std::string> > > log;
	/// Non-empty if --load was given on the command line. Savegame specified to load after start.
	boost::optional<std::string> load;
	/// Non-empty if --logdomains was given on the command line. Prints possible logdomains filtered by given string and exits.
	boost::optional<std::string> logdomains;
	/// True if --multiplayer was given on the command line. Goes directly into multiplayer mode.
	bool multiplayer;
	/// Max FPS specified by --max-fps option.
	boost::optional<int> max_fps;
	/// True if --nocache was given on the command line. Disables cache usage.
	bool nocache;
	/// True if --nodelay was given on the command line.
	bool nodelay;
	/// True if --nogui was given on the command line. Disables GUI.
	bool nogui;
	/// True if --nomusic was given on the command line. Disables music.
	bool nomusic;
	/// True if --nosound was given on the command line. Disables sound.
	bool nosound;
	/// True if --new-storyscreens was given on the command line. Hidden option to help testing the work-in-progress new storyscreen code.
	bool new_storyscreens;
	/// True if --new-syntax was given on the command line. Does magic.
	bool new_syntax;
	/// True if --new-widgets was given on the command line. Hidden option to enable the new widget toolkit.
	bool new_widgets;
	/// True if --path was given on the command line. Prints the path to data directory and exits.
	bool path;
	/// True if --preprocess was given on the command line. Starts Wesnoth in preprocessor-only mode.
	bool preprocess;
	/// Defines that were given to the --preprocess option.
	boost::optional<std::vector<std::string> > preprocess_defines;
	/// Non-empty if --preprocess-input-macros was given on the command line. Specifies a file that contains [preproc_define]s to be included before preprocessing. Dependant on --preprocess.
	boost::optional<std::string> preprocess_input_macros;
	/// Non-empty if --preprocess-output-macros was given on the command line. Outputs all preprocessed macros to the specified file. Dependant on --preprocess.
	boost::optional<std::string> preprocess_output_macros;
	/// Path to parse that was given to the --preprocess option.
	boost::optional<std::string> preprocess_path;
	/// Target (output) path tha was given to the --preprocess option.
	boost::optional<std::string> preprocess_target;
	/// True if --proxy was given on the command line. Enables proxy mode.
	bool proxy;
	/// Non-empty if --proxy-address was given on the command line.
	boost::optional<std::string> proxy_address;
	/// Non-empty if --proxy-password was given on the command line.
	boost::optional<std::string> proxy_password;
	/// Non-empty if --proxy-port was given on the command line.
	boost::optional<std::string> proxy_port;
	/// Non-empty if --proxy-user was given on the command line.
	boost::optional<std::string> proxy_user;
	/// Pair of AxB values specified after --resolution. Changes Wesnoth resolution.
	boost::optional<std::pair<int,int> > resolution;
	/// RNG seed specified by --rng-seed option. Initializes RNG with given seed.
	boost::optional<int> rng_seed;
	/// Non-empty if --server was given on the command line.  Connects Wesnoth to specified server. If no server was specified afterwards, contains an empty string.
	boost::optional<std::string> server;
	/// True if --screenshot was given on the command line. Starts Wesnoth in screenshot mode.
	bool screenshot;
	/// Map file to make a screenshot of. First parameter given after --screenshot.
	boost::optional<std::string> screenshot_map_file;
	/// Output file to put screenshot in. Second parameter given after --screenshot.
	boost::optional<std::string> screenshot_output_file;
	/// True if --smallgui was given on the command line. Makes Wesnoth use small gui layout.
	bool smallgui;
	/// True if --test was given on the command line. Goes directly into test mode.
	bool test;
	/// True if --validcache was given on the command line. Makes Wesnoth assume the cache is valid.
	bool validcache;
	/// True if --version was given on the command line. Prints version and exits.
	bool version;
	/// True if --windowed was given on the command line. Starts Wesnoth in windowed mode.
	bool windowed;
	/// True if --with-replay was given on the command line. Shows replay of the loaded file.
	bool with_replay;
private:
};

#endif
