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
#ifndef REPLAY_H_INCLUDED
#define REPLAY_H_INCLUDED

#include "config.hpp"
#include "display.hpp"
#include "map.hpp"
#include "unit.hpp"

struct verification_manager
{
	verification_manager(const unit_map& units);
	~verification_manager();
};

int get_random();

const config* get_random_results();
void set_random_results(const config& cfg);

class replay
{
public:
	replay();
	explicit replay(const config& cfg);

	config& get_config();

	void set_save_info(const game_state& save);
	const game_state& get_save_info() const;

	void set_skip(int turns_to_skip);
	void next_skip();
	bool skipping() const;

	void save_game(const std::string& label, const config& snapshot,
	               const config& starting_pos, bool include_replay = true);

	void add_recruit(int unit_index, const gamemap::location& loc);
	void add_recall(int unit_index, const gamemap::location& loc);
	void add_disband(int unit_index);
	void add_movement(const gamemap::location& a, const gamemap::location& b);
	void add_attack(const gamemap::location& a, const gamemap::location& b,
	                int weapon);
	void choose_option(int index);
	void add_label(const std::string& text, const gamemap::location& loc);
	void end_turn();

	void speak(const config& cfg);
	std::string build_chat_log(const std::string& team) const;

	//get data range will get a range of moves from the replay system.
	//if data_type is 'ALL_DATA' then it will return all data in this range
	//except for undoable data that has already been sent. If data_type is
	//NON_UNDO_DATA, then it will only retrieve undoable data, and will mark
	//it as already sent.
	//undoable data includes moves such as placing a label or speaking, which is
	//ignored by the undo system.
	enum DATA_TYPE { ALL_DATA, NON_UNDO_DATA };
	config get_data_range(int cmd_start, int cmd_end, DATA_TYPE data_type=ALL_DATA);
	config get_last_turn(int num_turns=1);

	void undo();

	int get_random();
	const config* get_random_results() const;
	void set_random_results(const config& cfg);

	void start_replay();
	config* get_next_action();

	bool at_end() const;
	void set_to_end();

	void clear();
	bool empty();

	enum MARK_SENT { MARK_AS_UNSENT, MARK_AS_SENT };
	void add_config(const config& cfg, MARK_SENT mark=MARK_AS_UNSENT);

	int ncommands();

	void mark_current();

	struct error {};

private:
	//generic for add_movement and add_attack
	void add_pos(const std::string& type,
	             const gamemap::location& a, const gamemap::location& b);

	void add_value(const std::string& type, int value);

	const config::child_list& commands() const;
	config* add_command();
	config cfg_;
	unsigned int pos_;

	config* current_;
	config* random_;

	game_state saveInfo_;

	int skip_;
};

replay& get_replay_source();

extern replay recorder;

//replays up to one turn from the recorder object
//returns true if it got to the end of the turn without data running out
bool do_replay(display& disp, const gamemap& map, const game_data& gameinfo,
               std::map<gamemap::location,unit>& units,
			   std::vector<team>& teams, int team_num, const gamestatus& state,
			   game_state& state_of_game, replay* obj=NULL);

//an object which can be made to undo a recorded move
//unless the transaction is confirmed
struct replay_undo
{
	replay_undo(replay& obj) : obj_(&obj) {}
	~replay_undo() { if(obj_) obj_->undo(); }
	void confirm_transaction() { obj_ = NULL; }
	
private:
	replay* obj_;
};

class replay_network_sender
{
public:
	replay_network_sender(replay& obj);
	~replay_network_sender();

	void sync_non_undoable();
	void commit_and_sync();
private:
	replay& obj_;
	int upto_;
};

#endif
