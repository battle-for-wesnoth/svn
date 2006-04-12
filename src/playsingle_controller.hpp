/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef PLAYSINGLE_CONTROLLER_H_INCLUDED
#define PLAYSINGLE_CONTROLLER_H_INCLUDED

#include "global.hpp"

#include "cursor.hpp"
#include "hotkeys.hpp"
#include "menu_events.hpp"
#include "play_controller.hpp"
#include "playturn.hpp"
#include "random.hpp"
#include "upload_log.hpp"

#include <vector>

class playsingle_controller : public play_controller
{
public:
	playsingle_controller(const config& level, const game_data& gameinfo, game_state& state_of_game, 
		const int ticks, const int num_turns, const config& game_config, CVideo& video, bool skip_replay);

	LEVEL_RESULT play_scenario(const std::vector<config*>& story, upload_log& log, bool skip_replay);

	virtual void recruit();
	virtual void repeat_recruit();
	virtual void recall();
	virtual bool can_execute_command(hotkey::HOTKEY_COMMAND command) const;
	virtual void toggle_shroud_updates();
	virtual void update_shroud_now();
	virtual void end_turn();
	virtual void rename_unit();
	virtual void create_unit();
	virtual void change_unit_side();
	virtual void label_terrain();
	virtual void continue_move();
	virtual void unit_hold_position();
	virtual void end_unit_turn();
	virtual void user_command();

	void playsingle_slice();

protected:
	virtual void play_turn();
	virtual void play_side(const int team_index);
	virtual void before_human_turn();
	virtual void play_human_turn();
	virtual void after_human_turn();
	void play_ai_turn();
	virtual void init_gui();
	void check_time_over();

	const set_random_generator generator_setter;
	const cursor::setter cursor_setter;
	std::deque<config> data_backlog_;
	gui::floating_textbox textbox_info_;
	replay_network_sender replay_sender_;

	bool end_turn_;
	bool player_type_changed_;
	bool replaying_;
private:
};


LEVEL_RESULT playsingle_scenario(const game_data& gameinfo, const config& terrain_config,
		const config* level, CVideo& video,	game_state& state_of_game,
		const std::vector<config*>& story, upload_log& loo, bool skip_replay);

#endif
