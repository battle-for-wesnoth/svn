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

#include "playmp_controller.hpp"

#include "log.hpp"
#include "sound.hpp"

#define LOG_NG LOG_STREAM(info, engine)

LEVEL_RESULT playmp_scenario(const game_data& gameinfo, const config& game_config,
		config const* level, CVideo& video, game_state& state_of_game,
		const config::child_list& story, upload_log& log, bool skip_replay)
{
	const int ticks = SDL_GetTicks();
	const int num_turns = atoi((*level)["turns"].c_str());
	playmp_controller playcontroller(*level, gameinfo, state_of_game, ticks, num_turns, game_config, video, skip_replay);
	return playcontroller.play_scenario(story, log, skip_replay);
}

playmp_controller::playmp_controller(const config& level, const game_data& gameinfo, game_state& state_of_game, 
									 const int ticks, const int num_turns, const config& game_config, CVideo& video,
									 bool skip_replay)
	: playsingle_controller(level, gameinfo, state_of_game, ticks, num_turns, game_config, video, skip_replay)
{
	beep_warning_time_ = 10000; //Starts beeping each second when time is less than this (millisec)
	turn_data_ = NULL;
}

void playmp_controller::clear_labels(){
	menu_handler_.clear_labels();
}

void playmp_controller::speak(){
	menu_handler_.speak();
}

void playmp_controller::play_side(const int team_index){
//goto this label if the type of a team (human/ai/networked) has changed mid-turn
redo_turn:
	bool player_type_changed_ = false;
	end_turn_ = false;

	if(current_team().is_human() || current_team().is_ai()) {
		playsingle_controller::play_side(team_index);
	} else if(current_team().is_network()) {
		player_type_changed_ = play_network_turn();
	}

	if (player_type_changed_) { goto redo_turn; }
}

void playmp_controller::before_human_turn(){
	playsingle_controller::before_human_turn();

	turn_data_ = new turn_info(gameinfo_,gamestate_,status_,
		*gui_,map_,teams_,player_number_,units_,replay_sender_);
}

void playmp_controller::play_human_turn(){
	int cur_ticks = SDL_GetTicks();

	gui_->enable_menu("endturn", true);
	while(!end_turn_) {

		try {
			config cfg;
			const network::connection res = network::receive_data(cfg);
			std::deque<config> backlog;

			if(res != network::null_connection) {
				turn_data_->process_network_data(cfg,res,backlog,skip_replay_);
			}

			play_slice();
		} catch(end_level_exception& e) {
			turn_data_->send_data();
			throw e;
		}


		if (current_team().countdown_time() > 0 &&  ( level_["mp_countdown"] == "yes" ) ){
			SDL_Delay(1);
			const int ticks = SDL_GetTicks();
			int new_time = current_team().countdown_time()-maximum<int>(1,(ticks - cur_ticks));
			if (new_time > 0 ){
				current_team().set_countdown_time(current_team().countdown_time()-maximum<int>(1,(ticks - cur_ticks)));
				cur_ticks = ticks;
				if ( current_team().countdown_time() <= beep_warning_time_){
					beep_warning_time_ = beep_warning_time_ - 1000;
					sound::play_sound("bell.wav");
				}
			} else {
				// Clock time ended
				// If no turn bonus or action bonus -> defeat
				const int action_increment = lexical_cast_default<int>(level_["mp_countdown_action_bonus"],0);
				if ( lexical_cast_default<int>(level_["mp_countdown_turn_bonus"],0) == 0 
					&& (action_increment == 0 || current_team().action_bonus_count() == 0)) {
					// Not possible to end level in MP with throw end_level_exception(DEFEAT);
					// because remote players only notice network disconnection
					// Current solution end remaining turns automatically
					current_team().set_countdown_time(10);
					recorder.add_countdown_update(current_team().countdown_time(),player_number_);
					recorder.end_turn();
					turn_data_->send_data();
					throw end_turn_exception();
				} else {
					const int maxtime = lexical_cast_default<int>(level_["mp_countdown_reservoir_time"],0);
					int secs = lexical_cast_default<int>(level_["mp_countdown_turn_bonus"],0);
					secs += action_increment  * current_team().action_bonus_count();
					current_team().set_action_bonus_count(0);
					secs = (secs > maxtime) ? maxtime : secs;
					current_team().set_countdown_time(1000 * secs);
					recorder.add_countdown_update(current_team().countdown_time(),player_number_);
					recorder.end_turn();
					turn_data_->send_data();
					throw end_turn_exception();
				}
			}

		}

		gui_->invalidate_animations();
		gui_->draw();

		turn_data_->send_data();
	}
	menu_handler_.clear_undo_stack(player_number_);
}

void playmp_controller::after_human_turn(){
	if ( level_["mp_countdown"] == "yes" ){
		current_team().set_countdown_time(current_team().countdown_time() + 1000 * lexical_cast_default<int>(level_["mp_countdown_turn_bonus"],0));
		recorder.add_countdown_update(current_team().countdown_time(),player_number_);
	}

	//send one more time to make sure network is up-to-date.
	turn_data_->send_data();
	if (turn_data_ != NULL){
		delete turn_data_;
		turn_data_ = NULL;
	}

	playsingle_controller::after_human_turn();
}

void playmp_controller::finish_side_turn(){
	play_controller::finish_side_turn();
	//just in case due to an exception turn_data_ has not been deleted in after_human_turn
	if (turn_data_ != NULL){
		delete turn_data_;
		turn_data_ = NULL;
	}
}

bool playmp_controller::play_network_turn(){
	LOG_NG << "is networked...\n";

	browse_ = true;
	gui_->enable_menu("endturn", false);
	turn_info turn_data(gameinfo_,gamestate_,status_,*gui_,
				map_,teams_,player_number_,units_, replay_sender_);

	for(;;) {

		bool have_data = false;
		config cfg;

		network::connection from = network::null_connection;

		if(data_backlog_.empty() == false) {
			have_data = true;
			cfg = data_backlog_.front();
			data_backlog_.pop_front();
		} else {
			from = network::receive_data(cfg);
			have_data = from != network::null_connection;
		}

		if(have_data) {
			const turn_info::PROCESS_DATA_RESULT result = turn_data.process_network_data(cfg,from,data_backlog_,skip_replay_);
			if(result == turn_info::PROCESS_RESTART_TURN) {
				return true;
			} else if(result == turn_info::PROCESS_END_TURN) {
				break;
			}
		}

		play_slice();
		turn_data.send_data();
		gui_->draw();
	}

	LOG_NG << "finished networked...\n";
	return false;
}

void playmp_controller::handle_generic_event(const std::string& name){
	turn_info turn_data(gameinfo_,gamestate_,status_,*gui_,
						map_,teams_,player_number_,units_, replay_sender_);

	if (name == "ai_user_interact"){
		playsingle_controller::handle_generic_event(name);
		turn_data.send_data();
	}
	else if ((name == "ai_unit_recruited") || (name == "ai_unit_moved") 
		|| (name == "ai_enemy_attacked")){
		turn_data.sync_network();
	}
}

bool playmp_controller::can_execute_command(hotkey::HOTKEY_COMMAND command) const
{
	switch (command){
		case hotkey::HOTKEY_SPEAK:
		case hotkey::HOTKEY_SPEAK_ALLY:
		case hotkey::HOTKEY_SPEAK_ALL:
		case hotkey::HOTKEY_CLEAR_LABELS:
			return network::nconnections() > 0;
	    default:
			return playsingle_controller::can_execute_command(command);
	}
}
