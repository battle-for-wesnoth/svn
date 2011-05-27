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

#include "SDL.h"
#include "SDL_mixer.h"

#include "about.hpp"
#include "addon/manager.hpp"
#include "ai/configuration.hpp"
#include "config.hpp"
#include "config_cache.hpp"
#include "construct_dialog.hpp"
#include "cursor.hpp"
#include "dialogs.hpp"
#include "editor/editor_main.hpp"
#include "foreach.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "formula.hpp"
#include "game_config.hpp"
#include "game_controller.hpp"
#include "game_errors.hpp"
#include "game_preferences.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "gui/dialogs/addon_connect.hpp"
#include "gui/dialogs/campaign_selection.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_method_selection.hpp"
#include "gui/dialogs/title_screen.hpp"
#include "gui/dialogs/transient_message.hpp"
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
#include "gui/widgets/debug.hpp"
#endif
#include "gui/auxiliary/event/handler.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "hotkeys.hpp"
#include "intro.hpp"
#include "language.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "widgets/menu.hpp"
#include "marked-up_text.hpp"
#include "network.hpp"
#include "playcampaign.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "statistics.hpp"
#include "wml_exception.hpp"
#include "wml_separators.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "sha1.hpp"
#include "version.hpp"

#include "wesconfig.h"

#include <cerrno>
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

// Minimum stack cookie to prevent stack overflow on AmigaOS4
#ifdef __amigaos4__
const char __attribute__((used)) stackcookie[] = "\0$STACK: 16000000";
#endif

static lg::log_domain log_config("config");
#define LOG_CONFIG LOG_STREAM(info, log_config)

#define LOG_GENERAL LOG_STREAM(info, lg::general)

static lg::log_domain log_preprocessor("preprocessor");
#define LOG_PREPROC LOG_STREAM(info,log_preprocessor)

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

struct preprocess_options
{
public:
	preprocess_options(): output_macros_path_("false"), input_macros_()
	{
	}
	std::string output_macros_path_;
	preproc_map input_macros_;
};

/** Process commandline-arguments */
static int process_command_args(int argc, char** argv) {
	const std::string program = argv[0];
	game_config::wesnoth_program_dir = directory_name(program);
	preprocess_options preproc;

	//parse arguments that shouldn't require a display device
	int arg;
	for(arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if(val.empty()) {
			continue;
		}

		if(val == "--help" || val == "-h") {
			// When adding items don't forget to update doc/man/wesnoth.6
			// Options are sorted alphabetically by --long-option.
			// Please keep the output to 80 chars per line.
			std::cout << "usage: " << argv[0]
			<< " [<options>] [<data-directory>]\n"
			<< "Available options:\n"
			<< "  --bpp <number>               sets BitsPerPixel value. Example: --bpp 32\n"
			<< "  -c, --campaign[[<difficulty>] <id_c> [<id_s>]]\n"
			<< "                               goes directly to the campaign.\n"
			<< "                               - difficulty : the difficulty of the specified\n"
			<< "                                          campaign (1 to max - Default is 1)\n"
			<< "                               - id_c: the id of the campaign. A selection \n"
			<< "                                       menu will appear if none specified\n"
			<< "                               - id_s: the id of the scenario from the\n"
			<< "                                       specified campaign\n"
			<< "                               Note: When using this switch please ensure that\n"
			<< "                               you specify the data directory path as the\n"
			<< "                               final argument aswell, otherwise the game\n"
			<< "                               will take the campaign/scenario id as the data dir.\n"
			<< "  --config-dir <name>          sets the path of the user config directory to\n"
			<< "                               $HOME/<name> or My Documents\\My Games\\<name> for windows.\n"
			<< "                               You can specify also an absolute path outside the\n"
			<< "                               $HOME or My Documents\\My Games directory.\n"
			<< "  --config-path                prints the path of the user config directory and\n"
			<< "                               exits.\n"
			<< "  --data-dir <directory>       overrides the data directory with the one specified.\n"
			<< "  -d, --debug                  enables additional command mode options in-game.\n"
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
			<< "  --debug-dot-level=<level1>,<level2>,...\n"
			<< "                               sets the level of the debug dot files.\n"
			<< "                               These files are used for debugging the widgets\n"
			<< "                               especially the for the layout engine. When enabled\n"
			<< "                               the engine will produce dot files which can be\n"
			<< "                               converted to images with the dot tool.\n"
			<< "                               Available levels:\n"
			<< "                               - size  : generate the size info of the widget.\n"
			<< "                               - state : generate the state info of the widget.\n"
			<< "  --debug-dot-domain=<domain1>,<domain2>,...\n"
			<< "                               sets the domain of the debug dot files.\n"
			<< "                               see --debug-dot-level for more info.\n"
			<< "                               Available domains:\n"
			<< "                               show   : generate the data when the dialog is\n"
			<< "                                        about to be shown.\n"
			<< "                               layout : generate the data during the layout\n"
			<< "                                        phase (might result in multiple files. \n"
			<< "                               The data can also be generated when the F12 is\n"
			<< "                               pressed in a dialog.\n"
#endif
			<< "  -e, --editor [<file>]        starts the in-game map editor directly. If <file>\n"
			<< "                               is specified, equivalent to -e --load <file>.\n"
			<< "  --fps                        displays the number of frames per second the\n"
			<< "                               game is currently running at, in a corner of\n"
			<< "                               the screen.\n"
			<< "  -f, --fullscreen             runs the game in full screen mode.\n"
			<< "  --gunzip <infile>.gz         decompresses a file (<infile>.gz) in gzip format\n"
			<< "                               and stores it without the .gz suffix.\n"
			<< "                               <infile>.gz will be removed.\n"
			<< "  --gzip <infile>              compresses a file (<infile>) in gzip format,\n"
			<< "                               stores it as <infile>.gz and removes <infile>.\n"
			<< "  -h, --help                   prints this message and exits.\n"
			<< "  -l, --load <file>            loads the save <file> from the standard save\n"
			<< "                               game directory.\n"
			<< "                               When launching the map editor via -e, the map\n"
			<< "                               <file> is loaded, relative to the current\n"
			<< "                               directory. If it is a directory, the editor\n"
			<< "                               will start with a load map dialog opened there.\n"
			<< "  --log-<level>=<domain1>,<domain2>,...\n"
			<< "                               sets the severity level of the log domains.\n"
			<< "                               'all' can be used to match any log domain.\n"
			<< "                               Available levels: error, warning, info, debug.\n"
			<< "                               By default the 'error' level is used.\n"
			<< "  --logdomains [filter]        lists defined log domains (only the ones containing\n"
			<< "                               [filter] if used) and exits.\n"
			<< "  --max-fps                    the maximum fps the game tries to run at. Values\n"
			<< "                               should be between 1 and 1000, the default is 50.\n"
			<< "  -m, --multiplayer            starts a multiplayer game. There are additional\n"
			<< "                               options that can be used as explained below:\n"
			<< "    --ai_config<number>=value  selects a configuration file to load for this side.\n"
			<< "    --algorithm<number>=value  selects a non-standard algorithm to be used by\n"
			<< "                               the AI controller for this side.\n"
			<< "    --controller<number>=value selects the controller for this side.\n"
			<< "    --era=value                selects the era to be played in by its id.\n"
			<< "    --exit-at-end              exit Wesnoth at the end of the scenario.\n"
			<< "    --nogui                    runs the game without the GUI. Must appear before\n"
			<< "                               --multiplayer to have the desired effect.\n"
			<< "    --parm<number>=name:value  sets additional parameters for this side.\n"
			<< "    --scenario=value           selects a multiplayer scenario. The default\n"
			<< "                               scenario is \"multiplayer_The_Freelands\".\n"
			<< "    --side<number>=value       selects a faction of the current era for this\n"
			<< "                               side by id.\n"
			<< "    --turns=value              sets the number of turns. The default is \"50\".\n"
			<< "  --new-syntax                 enables the new campaign syntax parsing.\n"
			<< "  --no-delay                   runs the game without any delays.\n"
			<< "  --nocache                    disables caching of game data.\n"
			<< "  --nomusic                    runs the game without music.\n"
			<< "  --nosound                    runs the game without sounds and music.\n"
			<< "  --path                       prints the path to the data directory and exits.\n"
			<< "  --preprocess, -p[=<define1>,<define2>,...] <file/folder> <target directory>\n"
			<< "                               preprocesses a specified file/folder. The preprocessed\n"
			<< "                               file(s) will be written in the specified target\n"
			<< "                               directory: a plain cfg file and a processed cfg file.\n"
			<< "                               define1,define2,...  - the extra defines will\n"
			<< "                               be added before processing the files. If you add\n"
			<< "                               them you must add the '=' character before.\n"
			<< "                               If 'SKIP_CORE' is in the define list the\n"
			<< "                               data/core won't be preprocessed.\n"
			<< " --preprocess-input-macros <source file>\n"
			<< "                               used only by the '--preprocess' command.\n"
			<< "                               Specifies a file that contains [preproc_define]s\n"
			<< "                               to be included before preprocessing.\n"
			<< " --preprocess-output-macros [<target file>]\n"
			<< "                               used only by the '--preprocess' command.\n"
			<< "                               Will output all preprocessed macros in the target file.\n"
			<< "                               If the file is not specified the output will be\n"
			<< "                               file '_MACROS_.cfg' in the target directory of\n"
			<< "                               preprocess's command. This switch should be typed\n"
			<< "                               before the --preprocess command.\n"
			<< "  -r, --resolution XxY         sets the screen resolution. Example: -r 800x600\n"
			<< "  --rng-seed <number>          seeds the random number generator with number\n"
			<< "                               Example: --rng-seed 0\n"
			<< "  --screenshot <map> <output>  Saves a screenshot of <map> to <output> without\n"
			<< "                               initializing a screen. Editor must be compiled\n"
			<< "                               in for this to work.\n"
			<< "  -s, --server [<host>]        connects to the host if specified\n"
			<< "                               or to the first host in your preferences.\n"
			<< "  -t, --test                   runs the game in a small test scenario.\n"
			<< "  --validcache                 assumes that the cache is valid. (dangerous)\n"
			<< "  -v, --version                prints the game's version number and exits.\n"
			<< "  -w, --windowed               runs the game in windowed mode.\n"
			<< "  --with-replay                replays the file loaded with the --load option.\n"
			<< "  --new-widgets                there is a new WIP widget toolkit this switch\n"
			<< "                               enables the new toolkit (VERY EXPERIMENTAL don't\n"
			<< "                               file bug reports since most are known).\n"
			<< "                               Parts of the library are deemed stable and will\n"
			<< "                               work without this switch.\n"
			<< "  --clock                      Adds the option to show a clock for testing the\n"
			<< "                               drawing timer.\n"
			;
			return 0;
		} else if(val == "--version" || val == "-v") {
			std::cout << "Battle for Wesnoth" << " " << game_config::version
			          << "\n";
			return 0;
		} else if (val == "--config-path") {
			std::cout << get_user_data_dir() << '\n';
			return 0;
		} else if(val == "--path") {
			std::cout <<  game_config::path
			          << "\n";
			return 0;
		}
		else if (val == "--screenshot" ) {
			if(!(argc > arg + 2)) {
				std::cerr << "format of " << val << " command: " << val << " <map file> <output file>\n";
				return 2;
			}
			static char opt[] = "SDL_VIDEODRIVER=dummy";
			SDL_putenv(opt);
		}
		else if(val == "--config-dir") {
			if (argc <= ++arg)
				break;
			set_preferences_dir(argv[arg]);
		} else if(val == "--data-dir") {
			if(arg +1 != argc) {
				++arg;
				const std::string datadir(argv[arg]);
				std::cerr << "Overriding data directory with " << datadir << std::endl;
#ifdef _WIN32
				// use c_str to ensure that index 1 points to valid element since c_str() returns null-terminated string
				if(datadir.c_str()[1] == ':') {
#else
				if(datadir[0] == '/') {
#endif
					game_config::path = datadir;
				} else {
					game_config::path = get_cwd() + '/' + datadir;
				}

				if(!is_directory(game_config::path)) {
					std::cerr << "Could not find directory '" << game_config::path << "'\n";
					throw config::error("directory not found");
				}

				// don't update font as we already updating it in game ctor
			}
			else
				std::cerr << "please specify a data directory\n";
		} else if (val.substr(0, 6) == "--log-") {
			size_t p = val.find('=');
			if (p == std::string::npos) {
				std::cerr << "unknown option: " << val << '\n';
				return 2;
			}
			std::string s = val.substr(6, p - 6);
			int severity;
			if (s == "error") severity = 0;
			else if (s == "warning") severity = 1;
			else if (s == "info") severity = 2;
			else if (s == "debug") severity = 3;
			else {
				std::cerr << "unknown debug level: " << s << '\n';
				return 2;
			}
			while (p != std::string::npos) {
				size_t q = val.find(',', p + 1);
				s = val.substr(p + 1, q == std::string::npos ? q : q - (p + 1));
				if (!lg::set_log_domain_severity(s, severity)) {
					std::cerr << "unknown debug domain: " << s << '\n';
					return 2;
				}
				p = q;
			}
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
			std::string filter;
			if(arg + 1 != argc) {
				++arg;
				filter = argv[arg];
			}
			std::cout << lg::list_logdomains(filter);
			return 0;
		} else if(val == "--rng-seed") {
			if (argc <= ++arg) {
				std::cerr << "format of \" " << val << " " << argv[arg] << " \" is bad\n";
				return 2;
			}
			srand(lexical_cast_default<unsigned int>(argv[arg]));
		} else if (val == "--preprocess-input-macros") {
			if (arg + 1 < argc)
			{
				++arg;
				std::string file = argv[arg];
				if (file_exists(file) == false)
				{
					std::cerr << "please specify an existing file. File "<< file <<" doesn't exist.\n";
					return 1;
				}

				std::cerr << SDL_GetTicks() << " Reading cached defines from: " << file << "\n";

				config cfg;
				std::string error_log;
				scoped_istream stream = istream_file(file);
				read(cfg, *stream);

				int read = 0;
				// use static preproc_define::read_pair(config) to make a object
				foreach (const config::any_child &value, cfg.all_children_range()) {
					const preproc_map::value_type def = preproc_define::read_pair(value.cfg);
					preproc.input_macros_[def.first] = def.second;
					++read;
				}
				std::cerr << SDL_GetTicks() << " Read " << read << " defines.\n";
			}
			else {
				std::cerr << "please specify input macros file.\n";
				return 2;
			}
		} else if (val == "--preprocess-output-macros") {
			preproc.output_macros_path_ = "true";
			if (arg + 1 < argc && argv[arg+1][0] != '-')
			{
				++arg;
				preproc.output_macros_path_ = argv[arg];
			}
		} else if (val.find("--preprocess") == 0 || val.find("-p") == 0){
			if (arg + 2 < argc){
				++arg;
				const std::string resourceToProcess(argv[arg]);
				++arg;
				const std::string targetDir(argv[arg]);

				Uint32 startTime = SDL_GetTicks();
				// if the users add the SKIP_CORE define we won't preprocess data/core
				bool skipCore = false;
				bool skipTerrainGFX = false;
				// the 'core_defines_map' is the one got from data/core macros
				preproc_map defines_map(preproc.input_macros_);
				std::string error_log;

				// add the specified defines
				size_t pos=std::string::npos;
				if (val.find("--preprocess=") == 0)
					pos = val.find("=");
				else if (val.find("-p=") == 0)
					pos = val.find("=");

				// we have some defines specified
				if (pos != std::string::npos)
				{
					std::string tmp_val = val.substr(pos+1);
					while (pos != std::string::npos)
					{
						size_t tmpPos = val.find(',', pos+1);
						tmp_val = val.substr(pos + 1,
							tmpPos == std::string::npos ? tmpPos : tmpPos - (pos+1));
						pos = tmpPos;

						if (tmp_val.empty()){
							std::cerr << "empty define supplied\n";
							continue;
						}

						LOG_PREPROC<<"adding define: "<< tmp_val<<'\n';
						defines_map.insert(std::make_pair(tmp_val,
							preproc_define(tmp_val)));
						if (tmp_val == "SKIP_CORE")
						{
							std::cerr << "'SKIP_CORE' defined.\n";
							skipCore = true;
						}
						else if (tmp_val == "NO_TERRAIN_GFX")
						{
							std::cerr << "'NO_TERRAIN_GFX' defined.\n";
							skipTerrainGFX = true;
						}
					}
					std::cerr << "added " << defines_map.size() << " defines.\n";
				}

				// preprocess core macros first if we don't skip the core
				if (skipCore == false)
				{
					std::cerr << "preprocessing common macros from 'data/core' ...\n";

					// process each folder explicitly to gain speed
					preprocess_resource(game_config::path + "/data/core/macros",&defines_map);
					if (skipTerrainGFX == false)
						preprocess_resource(game_config::path + "/data/core/terrain-graphics",&defines_map);

					std::cerr << "acquired " << (defines_map.size() - preproc.input_macros_.size())
						<< " 'data/core' defines.\n";
				}
				else
					std::cerr << "skipped 'data/core'\n";

				// preprocess resource
				std::cerr << "preprocessing specified resource: "
						  << resourceToProcess << " ...\n";
				preprocess_resource(resourceToProcess, &defines_map, true,true, targetDir);
				std::cerr << "acquired " << (defines_map.size() - preproc.input_macros_.size())
					      << " total defines.\n";

				if (preproc.output_macros_path_ != "false")
				{
					std::string outputPath = targetDir + "/_MACROS_.cfg";
					if (preproc.output_macros_path_ != "true")
						outputPath = preproc.output_macros_path_;

					std::cerr << "writing '" << outputPath << "' with "
							  << defines_map.size() << " defines.\n";

					scoped_ostream out = ostream_file(outputPath);
					if (!out->fail())
					{
						config_writer writer(*out,false);

						for(preproc_map::iterator itor = defines_map.begin();
							itor != defines_map.end(); ++itor)
						{
							(*itor).second.write(writer, (*itor).first);
						}
					}
					else
						std::cerr << "couldn't open the file.\n";
				}

				std::cerr << "preprocessing finished. Took "<< SDL_GetTicks() - startTime << " ticks.\n";
				return 0;
			}
			else{
				std::cerr << "Please specify a source file/folder and a target folder\n";
				return 2;
			}
		}
	}

	// Not the most intuitive solution, but I wanted to leave current semantics for now
	return -1;
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
static void init_locale() {
	#ifdef _WIN32
	    std::setlocale(LC_ALL, "English");
	#else
		std::setlocale(LC_ALL, "C");
		std::setlocale(LC_MESSAGES, "");
	#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain (PACKAGE, intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	bindtextdomain (PACKAGE "-lib", intl_dir.c_str());
	bind_textdomain_codeset (PACKAGE "-lib", "UTF-8");
	textdomain (PACKAGE);
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	srand(time(NULL));

	int finished = process_command_args(argc, argv);
	if(finished != -1) {
		return finished;
	}

	//ensure recorder has an actually random seed instead of what it got during
	//static initialization (before any srand() call)
	recorder.set_seed(rand());

	game_controller game(argc,argv);
	const int start_ticks = SDL_GetTicks();

	init_locale();

	bool res;

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if(res == false) {
		std::cerr << "could not initialize fonts\n";
		return 1;
	}

	res = game.init_language();
	if(res == false) {
		std::cerr << "could not initialize the language\n";
		return 1;
	}

	res = game.init_video();
	if(res == false) {
		std::cerr << "could not initialize display\n";
		return 1;
	}

	const cursor::manager cursor_manager;
	cursor::set(cursor::WAIT);

	loadscreen::global_loadscreen_manager loadscreen_manager(game.disp().video());

	loadscreen::start_stage("init gui");
	gui2::init();
	const gui2::event::tmanager gui_event_manager;

	loadscreen::start_stage("load config");
	res = game.init_config();
	if(res == false) {
		std::cerr << "could not initialize game config\n";
		return 1;
	}
	loadscreen::start_stage("init fonts");

	res = font::load_font_config();
	if(res == false) {
		std::cerr << "could not re-initialize fonts for the current language\n";
		return 1;
	}

	loadscreen::start_stage("refresh addons");
	refresh_addon_version_info_cache();

#if defined(_X11) && !defined(__APPLE__)
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

	config tips_of_day;

	loadscreen::start_stage("titlescreen");

	LOG_CONFIG << "time elapsed: "<<  (SDL_GetTicks() - start_ticks) << " ms\n";

	for (;;)
	{
		// reset the TC, since a game can modify it, and it may be used
		// by images in add-ons or campaigns dialogs
		image::set_team_colors();

		statistics::fresh_stats();

        if (!game.is_loading()) {
			const config &cfg = game.game_config().child("titlescreen_music");
			if (cfg) {
	            sound::play_music_repeatedly(game_config::title_music);
				foreach (const config &i, cfg.child_range("music")) {
					sound::play_music_config(i);
				}
				sound::commit_music_changes();
			} else {
				sound::empty_playlist();
				sound::stop_music();
			}
        }

		loadscreen_manager.reset();

		if(game.play_test() == false) {
			return 0;
		}

		if(game.play_multiplayer_mode() == false) {
			return 0;
		}

		if(game.play_screenshot_mode() == false) {
			return 0;
		}

		recorder.clear();

		//Start directly a campaign
		if(game.goto_campaign() == false){
			if (game.jump_to_campaign_id().empty())
				continue; //Go to main menu
			else
				return 1; //we got an error starting the campaign from command line
		}

		//Start directly a multiplayer
		//Eventually with a specified server
		if(game.goto_multiplayer() == false){
			continue; //Go to main menu
		}

		if (game.goto_editor() == false) {
			return 0;
		}

		gui2::ttitle_screen::tresult res = game.is_loading()
				? gui2::ttitle_screen::LOAD_GAME
				: gui2::ttitle_screen::NOTHING;

		const preferences::display_manager disp_manager(&game.disp());

		const font::floating_label_context label_manager;

		cursor::set(cursor::NORMAL);
		if(res == gui2::ttitle_screen::NOTHING) {
			const hotkey::basic_handler key_handler(&game.disp());
			gui2::ttitle_screen dlg;
			dlg.show(game.disp().video());

			res = static_cast<gui2::ttitle_screen::tresult>(dlg.get_retval());
		}

		game_controller::RELOAD_GAME_DATA should_reload = game_controller::RELOAD_DATA;

		if(res == gui2::ttitle_screen::QUIT_GAME) {
			LOG_GENERAL << "quitting game...\n";
			return 0;
		} else if(res == gui2::ttitle_screen::LOAD_GAME) {
			if(game.load_game() == false) {
				game.clear_loaded_game();
				res = gui2::ttitle_screen::NOTHING;
				continue;
			}
			should_reload = game_controller::NO_RELOAD_DATA;
		} else if(res == gui2::ttitle_screen::TUTORIAL) {
			game.set_tutorial();
		} else if(res == gui2::ttitle_screen::NEW_CAMPAIGN) {
			if(game.new_campaign() == false) {
				continue;
			}
		} else if(res == gui2::ttitle_screen::MULTIPLAYER) {
			game_config::debug = game_config::mp_debug;
			if(game.play_multiplayer() == false) {
				continue;
			}
		} else if(res == gui2::ttitle_screen::CHANGE_LANGUAGE) {
			if (game.change_language()) {
				tips_of_day.clear();
				t_string::reset_translations();
				image::flush_cache();
			}
			continue;
		} else if(res == gui2::ttitle_screen::EDIT_PREFERENCES) {
			game.show_preferences();
			continue;
		} else if(res == gui2::ttitle_screen::SHOW_ABOUT) {
			about::show_about(game.disp());
			continue;
		} else if(res == gui2::ttitle_screen::SHOW_HELP) {
			help::help_manager help_manager(&game.game_config(), NULL);
			help::show_help(game.disp());
			continue;
		} else if(res == gui2::ttitle_screen::GET_ADDONS) {
			try {
				manage_addons(game.disp());
			} catch(config_changed_exception const&) {
				game.reload_changed_game_config();
			}
			continue;
		} else if(res == gui2::ttitle_screen::RELOAD_GAME_DATA) {
			loadscreen::global_loadscreen_manager loadscreen(game.disp().video());
			game.reload_changed_game_config();
			image::flush_cache();
			continue;
		} else if(res == gui2::ttitle_screen::START_MAP_EDITOR) {
			///@todo editor can ask the game to quit completely
			if (game.start_editor() == editor::EXIT_QUIT_TO_DESKTOP) {
				return 0;
			}
			continue;
		}

		if (recorder.at_end()){
			game.launch_game(should_reload);
		}
		else{
			game.play_replay();
		}
	}

	return 0;
}

#ifndef DISABLE_POOL_ALLOC
extern "C" {
void init_custom_malloc();
}
#endif

int main(int argc, char** argv)
{
#if defined(_OPENMP) && !defined(_WIN32)
	// Wesnoth is a special case for OMP
	// OMP wait strategy is to have threads busy-loop for 100ms
	// if there is nothing to do, they then go to sleep.
	// this avoids the scheduler putting the thread to sleep when work
	// is about to be available
	//
	// However Wesnoth has a lot of very small jobs that need to be done
	// at each redraw => 50fps every 2ms.
	// All the threads are thus busy-waiting all the time, hogging the CPU
	// To avoid that problem, we need to set the OMP_WAIT_POLICY env var
	// but that var is read by OMP at library loading time (before main)
	// thus the relaunching of ourselves after setting the variable.
	if (!getenv("OMP_WAIT_POLICY")) {
		setenv("OMP_WAIT_POLICY", "PASSIVE", 1);
		execv(argv[0], argv);
	}
#endif
#ifndef DISABLE_POOL_ALLOC
	init_custom_malloc();
#endif
	if(SDL_Init(SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}

	try {
		std::cerr << "Battle for Wesnoth v" << game_config::revision << '\n';
		const time_t t = time(NULL);
		std::cerr << "Started on " << ctime(&t) << "\n";

		const std::string exe_dir = get_exe_dir();
		if(!exe_dir.empty() && file_exists(exe_dir + "/data/_main.cfg")) {
			std::cerr << "Automatically found a possible data directory at "
			          << exe_dir << '\n';
			game_config::path = exe_dir;
		}

		const int res = do_gameloop(argc,argv);
		safe_exit(res);
	} catch(CVideo::error&) {
		std::cerr << "Could not initialize video. Exiting.\n";
		return 1;
	} catch(font::manager::error&) {
		std::cerr << "Could not initialize fonts. Exiting.\n";
		return 1;
	} catch(config::error& e) {
		std::cerr << e.message << "\n";
		return 1;
	} catch(gui::button::error&) {
		std::cerr << "Could not create button: Image could not be found\n";
		return 1;
	} catch(CVideo::quit&) {
		//just means the game should quit
	} catch(end_level_exception&) {
		std::cerr << "caught end_level_exception (quitting)\n";
	} catch(twml_exception& e) {
		std::cerr << "WML exception:\nUser message: "
			<< e.user_message << "\nDev message: " << e.dev_message << '\n';
		return 1;
	} catch(game_logic::formula_error& e) {
		std::cerr << e.what()
			<< "\n\nGame will be aborted.\n";
		return 1;
	} catch(game::error &) {
		// A message has already been displayed.
		return 1;
	} catch(std::bad_alloc&) {
		std::cerr << "Ran out of memory. Aborted.\n";
		return ENOMEM;
	}

	return 0;
} // end main

