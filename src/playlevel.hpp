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
#ifndef PLAY_LEVEL_HPP_INCLUDED
#define PLAY_LEVEL_HPP_INCLUDED

#include "actions.hpp"
#include "ai.hpp"
#include "config.hpp"
#include "dialogs.hpp"
#include "display.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "key.hpp"
#include "pathfind.hpp"
#include "show_dialog.hpp"
#include "team.hpp"
#include "unit_types.hpp"
#include "unit.hpp"
#include "video.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

enum LEVEL_RESULT { VICTORY, DEFEAT, QUIT, LEVEL_CONTINUE, LEVEL_CONTINUE_NO_SAVE };

struct end_level_exception {
	end_level_exception(LEVEL_RESULT res, bool bonus=true)
	                     : result(res), gold_bonus(bonus)
	{}
	LEVEL_RESULT result;
	bool gold_bonus;
};

LEVEL_RESULT play_level(game_data& gameinfo, const config& terrain_config,
                        config* level, CVideo& video,
                        game_state& state_of_game,
						const std::vector<config*>& story);

#endif
