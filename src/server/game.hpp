/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GAME_HPP_INCLUDED
#define GAME_HPP_INCLUDED

#include "../network.hpp"
#include "player.hpp"

#include "simple_wml.hpp"

#include <map>
#include <vector>

//class player;

namespace wesnothd {

typedef std::map<network::connection,player> player_map;
typedef std::vector<network::connection> user_vector;
typedef std::vector<network::connection> side_vector;

class game
{
public:
	game(player_map& players, const network::connection host=0, const std::string name="");
	~game();

	int id() const { return id_; }
	const std::string& name() const { return name_; }

	bool is_owner(const network::connection player) const { return (player == owner_); }
	bool is_member(const network::connection player) const
	{ return is_player(player) || is_observer(player); }
	bool allow_observers() const;
	bool is_observer(const network::connection player) const;
	bool is_player(const network::connection player) const;
	bool player_is_banned(const network::connection player) const;
	bool level_init() const { return level_.child("side") != NULL; }
	bool started() const { return started_; }

	size_t nplayers() const { return players_.size(); }
	size_t nobservers() const { return observers_.size(); }
	size_t current_turn() const { return (nsides_ ? end_turn_ / nsides_ + 1 : 0); }

	void mute_all_observers();
	//! Mute an observer by name.
	void mute_observer(const simple_wml::node& mute, const player_map::const_iterator muter);
	//! Kick a member by name.
	network::connection kick_member(const simple_wml::node& kick, const player_map::const_iterator kicker);
	//! Ban and kick a user by name. He doesn't need to be in this game.
	network::connection ban_user(const simple_wml::node& ban, const player_map::const_iterator banner);

	void add_player(const network::connection player, bool observer = false);
	bool remove_player(const network::connection player, const bool disconnect=false);

	//! Adds players and observers into one vector and returns that.
	const user_vector all_game_users() const;
	//! Adds players from one game to another. This is used to add players and
	//! observers from a game to the lobby (which is also implemented as a game),
	//! if that game ends. The second parameter controls, wether the players are
	//! added to the players_ or observers_ vector (default observers_).
	void add_players(const game& other_game, const bool observer = true);

	void start_game(const player_map::const_iterator starter);
	//! A user (player only?) asks for the next scenario to advance to.
	void load_next_scenario(const player_map::const_iterator user) const;

	//! Resets the side configuration according to the scenario data.
	void update_side_data();
	//! Let's a player owning a side give it to another player or observer.
	void transfer_side_control(const network::connection sock, const simple_wml::node& cfg);

	void process_message(simple_wml::document& data, const player_map::iterator user);
	//! Filters and processes (some) commands.
	//! Returns true iff the turn ended.
	bool process_turn(simple_wml::document& data, const player_map::const_iterator user);
	//! Set the description to the number of available slots.
	//! Returns true iff the number of slots has changed.
	bool describe_slots();

	void send_server_message_to_all(const char* message, network::connection exclude=0) const;
	void send_server_message(const char* message, network::connection sock=0, simple_wml::document* doc=NULL) const;
	//! Send data to all players in this game except 'exclude'.
	void send_and_record_server_message(const char* message,
			const network::connection exclude=0);
	void send_data(simple_wml::document& data, const network::connection exclude=0, const std::string packet_type = "unknown") const;
	void send_to_one(simple_wml::document& data, const network::connection sock, const std::string packet_type = "unknown") const;

	void record_data(simple_wml::document* data);

	//! The full scenario data.
	simple_wml::document& level() { return level_; }

	//! Functions to set/get the address of the game's summary description as
	//! sent to players in the lobby.
	void set_description(simple_wml::node* desc);
	simple_wml::node* description() const { return description_; }

	void set_password(const std::string& passwd) { password_ = passwd; }
	bool password_matches(const std::string& passwd) const {
		return password_.empty() || passwd == password_;
	}

	const std::string& termination_reason() const {
		static const std::string aborted = "aborted";
		static const std::string not_started = "not started";
		return started_ ? (termination_.empty() ? aborted : termination_) : not_started;
	}

	void set_termination_reason(const std::string& reason);

private:
	//forbidden operations
	game(const game&);
	void operator=(const game&);

	size_t current_side() const { return (nsides_ ? end_turn_ % nsides_ : 0); }
	network::connection current_player() const
	{ return (nsides_ ? sides_[current_side()] : 0); }
	bool is_current_player(const network::connection player) const
	{ return (current_player() == player); }
	bool is_muted_observer(const network::connection player) const;
	bool all_observers_muted() const { return all_observers_muted_; }

	//! Figures out which side to take and tells that side to the game owner.
	bool take_side(const player_map::const_iterator user);
	//! Send [change_controller] message to tell all clients the new controller's name.
	void send_change_controller(const size_t side_num,
			const player_map::const_iterator newplayer,
			const bool player_left=true);
	void transfer_ai_sides();
	void send_data_team(simple_wml::document& data, const simple_wml::string_span& team,
			const network::connection exclude=0) const;
	void send_data_observers(simple_wml::document& data, const network::connection exclude=0) const;
	//! Send [observer] tags of all the observers in the game to the user or
	//! everyone if none given.
	void send_observerjoins(const network::connection sock=0) const;
	void send_observerquit(const player_map::const_iterator observer) const;
	void send_history(const network::connection sock) const;
	//! In case of a host transfer, notify the new host about its status.
	void notify_new_host();
	//! Convenience function for finding a user by name.
	player_map::const_iterator find_user(const simple_wml::string_span& name) const;

	bool observers_can_label() const { return false; }
	bool observers_can_chat() const { return true; }
	bool is_legal_command(const simple_wml::node& command, bool is_player);
	//! Function which returns true iff 'player' is on 'team'.
	bool is_on_team(const simple_wml::string_span& team, const network::connection player) const;

	//! Function which should be called every time a player ends their turn
	//! (i.e. [end_turn] received). This will update the 'turn' attribute for
	//! the game's description when appropriate. Will return true iff there has
	//! been a change.
	bool end_turn();

	//! Function to send a list of users to all clients.
	//! Only sends data if the game is initialized but not yet started.
	void send_user_list(const network::connection exclude=0) const;

	//! Helps debugging player and observer lists.
	std::string debug_player_info() const;

	player_map* player_info_;

	static int id_num;
	int id_;
	//! The name of the game.
	std::string name_;
	std::string password_;
	//! The game host or later owner (if the host left).
	network::connection owner_;
	//! A vector of players (members owning a side).
	user_vector players_;
	//! A vector of observers (members not owning a side).
	user_vector observers_;
	user_vector muted_observers_;
	//! A vector of side owners.
	side_vector sides_;
	//! A vector indicating what sides are actually taken. (Really needed?)
	std::vector<bool> sides_taken_;
	//! A vector of controller strings indicating the type.
	//! "network" - a side controlled by some member of the game (not the owner)
	//! "human"   - a side controlled by the owner
	//! "ai"      - a side of the owner controlled by an AI
	//! "null"    - an empty side
	std::vector<std::string> side_controllers_;
	
	//! Number of sides in the current scenario.
	int nsides_;
	bool started_;

	//! The current scenario data.
	simple_wml::document level_;
	//! Replay data.
	mutable std::vector<simple_wml::document*> history_;
	//! Pointer to the game's description in the games_and_users_list_.
	simple_wml::node* description_;

	int end_turn_;

	bool all_observers_muted_;

	std::vector<std::string> bans_;

	std::string termination_;
};

struct game_id_matches {
	game_id_matches(int id) : id_(id) {};
	bool operator()(const game* g) const { return g->id() == id_; }

private:
	int id_;
};
}
#endif

