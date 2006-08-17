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

#include "global.hpp"

#include "ai_interface.hpp"
#include "config_adapter.hpp"
#include "cursor.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "game_events.hpp"
#include "halo.hpp"
#include "help.hpp"
#include "intro.hpp"
#include "log.hpp"
#include "map_create.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "replay_controller.hpp"
#include "sound.hpp"
#include "tooltips.hpp"

#include <iostream>
#include <iterator>

#define LOG_NG LOG_STREAM(info, engine)

LEVEL_RESULT play_replay_level(const game_data& gameinfo, const config& game_config,
		const config* level, CVideo& video, game_state& state_of_game)
{
	try{
		const int ticks = SDL_GetTicks();
		const int num_turns = atoi((*level)["turns"].c_str());
		LOG_NG << "creating objects... " << (SDL_GetTicks() - ticks) << "\n";
		replay_controller replaycontroller(*level, gameinfo, state_of_game, ticks, num_turns, game_config, video);
		LOG_NG << "created objects... " << (SDL_GetTicks() - replaycontroller.get_ticks()) << "\n";
		const events::command_disabler disable_commands;

		//replay event-loop
		for (;;){
			replaycontroller.play_slice();
		}
	}
	catch(end_level_exception&){
		LOG_NG << "play_replay_level: end_level_exception\n";
	}

	return LEVEL_CONTINUE;
}

replay_controller::replay_controller(const config& level, const game_data& gameinfo, game_state& state_of_game,
						   const int ticks, const int num_turns, const config& game_config,
						   CVideo& video)
	: play_controller(level, gameinfo, state_of_game, ticks, num_turns, game_config, video, false),
	gamestate_start_(state_of_game), status_start_(level, num_turns)
{
	current_turn_ = 1;
	delay_ = 0;
	is_playing_ = false;
	init();
	gamestate_start_ = gamestate_;
}

replay_controller::~replay_controller(){
}

void replay_controller::init(){
	LOG_NG << "in replay_controller::init()...\n";

	//guarantee the cursor goes back to 'normal' at the end of the level
	const cursor::setter cursor_setter(cursor::NORMAL);

	LOG_NG << "initializing replay-display... " << (SDL_GetTicks() - ticks_) << "\n";
	const config* theme_cfg = get_theme(game_config_, level_["theme"]);
	const config* replay_theme_cfg = theme_cfg->child("resolution")->child("replay");
	if (NULL != replay_theme_cfg)
	    gui_->get_theme().modify(replay_theme_cfg);
	LOG_NG << "done initializing replay-display... " << (SDL_GetTicks() - ticks_) << "\n";

	fire_prestart(true);
	init_gui();

	LOG_NG << "first_time..." << (recorder.is_skipping() ? "skipping" : "no skip") << "\n";

	fire_start(!loading_game_);
	update_gui();

	units_start_ = units_;
	teams_start_ = team_manager_.clone(teams_);
}

void replay_controller::init_gui(){
	LOG_NG << "Initializing GUI... " << (SDL_GetTicks() - ticks_) << "\n";
	play_controller::init_gui();

	gui_->scroll_to_leader(units_, player_number_);
	update_locker lock_display((*gui_).video(),false);
	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		t->set_fog(false);
		t->set_shroud(false);
	}
}

std::vector<team>& replay_controller::get_teams(){
	return teams_;
}

unit_map replay_controller::get_units(){
	return units_;
}

display& replay_controller::get_gui(){
	return *gui_;
}

gamemap& replay_controller::get_map(){
	return map_;
}

const gamestatus& replay_controller::get_status(){
	return status_;
}

const int replay_controller::get_player_number(){
	return player_number_;
}

const bool replay_controller::is_loading_game(){
	return loading_game_;
}

void replay_controller::reset_replay(){
	is_playing_ = false;
	player_number_ = 1;
	current_turn_ = 1;
	recorder.start_replay();
	units_ = units_start_;
	status_ = status_start_;
	gamestate_ = gamestate_start_;
	teams_ = team_manager_.clone(teams_start_);
	if (events_manager_ != NULL){
		delete events_manager_;
		events_manager_ = new game_events::manager(level_,*gui_,map_,units_,teams_,
											gamestate_,status_,gameinfo_);
	}
	fire_prestart(true);
	fire_start(!loading_game_);
	(*gui_).invalidate_all();
	(*gui_).draw();
}

void replay_controller::stop_replay(){
	is_playing_ = false;
}

void replay_controller::replay_next_turn(){
	is_playing_ = true;
	play_turn();
	is_playing_ = false;
}

void replay_controller::replay_next_side(){
	is_playing_ = true;
	play_side(player_number_ - 1, false);

	if ((size_t)player_number_ > teams_.size()) {
		player_number_ = 1;
		current_turn_++;
	}
	is_playing_ = false;
}

void replay_controller::replay_switch_fog(){
	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		t->set_fog(!t->uses_fog());
	}
	update_teams();
	update_gui();
}

void replay_controller::replay_switch_shroud(){
	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		t->set_shroud(!t->uses_shroud());
	}
	update_teams();
	update_gui();
}

void replay_controller::replay_skip_animation(){
	recorder.set_skip(!recorder.is_skipping());
	skip_replay_ = !skip_replay_;
}

void replay_controller::play_replay(){
	if (recorder.at_end()){
		return;
	}

	try{
		is_playing_ = true;

		LOG_NG << "starting main loop\n" << (SDL_GetTicks() - ticks_) << "\n";
		for(; !recorder.at_end() && is_playing_; first_player_ = 1
			) {
			play_turn();
			play_slice();
		} //end for loop
		is_playing_ = false;
	}
	catch(end_level_exception& e){
		if (e.result == QUIT) { throw e; }
	}
}

void replay_controller::play_turn(){
	if (recorder.at_end()){
		return;
	}

	LOG_NG << "turn: " << current_turn_ << "\n";

	while ( ((size_t)player_number_ <= teams_.size()) && (!recorder.at_end()) ){
		play_side(player_number_ - 1, false);
	}

	player_number_ = 1;
	current_turn_++;
}

void replay_controller::play_side(const unsigned int team_index, bool){
	if (recorder.at_end()){
		return;
	}

	play_controller::init_side(team_index, true);

	//if a side is dead, don't do their turn
	if(!current_team().is_empty() && team_units(units_,player_number_) > 0) {

		do_replay(true);

		finish_side_turn();

		for(unit_map::iterator uit = units_.begin(); uit != units_.end(); ++uit) {
			if(uit->second.side() != (size_t)player_number_){
				//this is necessary for replays in order to show possible movements
				uit->second.new_turn();
			}
		}
	}

	player_number_++;

	if ((size_t)player_number_ > teams_.size()) {
		status_.next_turn();
		finish_turn();
	}
	update_teams();
	update_gui();
}

void replay_controller::update_teams(){
	int next_team = player_number_;
	if ((size_t)next_team > teams_.size()) { next_team = 1; }
	if (teams_[next_team - 1].uses_fog()){
		gui_->set_team(next_team - 1);
		::clear_shroud(*gui_, status_, map_, gameinfo_, units_, teams_, next_team - 1);
	}
	if (teams_[next_team - 1].uses_shroud()){
		gui_->set_team(next_team - 1);
		recalculate_fog(map_, status_, gameinfo_, units_, teams_, next_team - 1);
	}
	gui_->set_playing_team(next_team - 1);
	(*gui_).scroll_to_leader(units_, next_team);
}

void replay_controller::update_gui(){
	(*gui_).recalculate_minimap();
	(*gui_).redraw_minimap();
	(*gui_).invalidate_all();
	events::raise_draw_event();
	(*gui_).draw();
}

bool replay_controller::can_execute_command(hotkey::HOTKEY_COMMAND command) const
{
	bool result = play_controller::can_execute_command(command);

	switch(command) {

	//commands we can always do
	case hotkey::HOTKEY_PLAY_REPLAY:
	case hotkey::HOTKEY_RESET_REPLAY:
	case hotkey::HOTKEY_STOP_REPLAY:
	case hotkey::HOTKEY_REPLAY_NEXT_TURN:
	case hotkey::HOTKEY_REPLAY_NEXT_SIDE:
	case hotkey::HOTKEY_REPLAY_FOG:
	case hotkey::HOTKEY_REPLAY_SHROUD:
	case hotkey::HOTKEY_REPLAY_SKIP_ANIMATION:
		return true;

	default:
		return result;
	}
}
