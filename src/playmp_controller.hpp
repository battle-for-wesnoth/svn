/*
   Copyright (C) 2006 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef PLAYMP_CONTROLLER_H_INCLUDED
#define PLAYMP_CONTROLLER_H_INCLUDED

#include "global.hpp"

#include "hotkeys.hpp"
#include "playsingle_controller.hpp"

#include <vector>

class playmp_controller : public playsingle_controller
{
public:
	playmp_controller(const config& level, const game_data& gameinfo, game_state& state_of_game,
		const int ticks, const int num_turns, const config& game_config, CVideo& video, bool skip_replay);

	static unsigned int replay_last_turn() { return replay_last_turn_; }
	static void set_replay_last_turn(unsigned int turn);

protected:
	virtual void handle_generic_event(const std::string& name);

	virtual void speak();
	virtual void clear_labels();
	virtual bool can_execute_command(hotkey::HOTKEY_COMMAND command) const;

	virtual void play_side(const unsigned int team_index);
	virtual void before_human_turn();
	virtual void play_human_turn();
	virtual void after_human_turn();
	virtual void finish_side_turn();
	bool play_network_turn();

	turn_info* turn_data_;

	int beep_warning_time_;
private:
	void process_oos(const std::string& err_msg);
	static unsigned int replay_last_turn_;
};


LEVEL_RESULT playmp_scenario(const game_data& gameinfo, const config& terrain_config,
		config const* level, CVideo& video,	game_state& state_of_game,
		const config::child_list& story, upload_log& log, bool skip_replay);

#endif
