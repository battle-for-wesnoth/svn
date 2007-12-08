/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "../global.hpp"

#include "game.hpp"
#include "../map.hpp"
#include "../log.hpp"
#include "../wassert.hpp"

#include <iostream>
#include <sstream>

#define ERR_GAME LOG_STREAM(err, mp_server)
#define LOG_GAME LOG_STREAM(info, mp_server)
#define DBG_GAME LOG_STREAM(debug, mp_server)

int game::id_num = 1;
//! @todo remove after 1.3.12 is no longer allowed on the server.
bool game::send_gzipped = false;

game::game(player_map& players, const network::connection host, const std::string name)
	: player_info_(&players), id_(id_num++), name_(name), owner_(host),
	sides_(gamemap::MAX_PLAYERS), sides_taken_(gamemap::MAX_PLAYERS),
	side_controllers_(gamemap::MAX_PLAYERS), started_(false),
	description_(NULL),	end_turn_(0), allow_observers_(true),
	all_observers_muted_(false)
{
	// Hack to handle the pseudo games lobby_ and not_logged_in_.
	if (owner_ == 0) return;
	players_.push_back(owner_);
	const player_map::iterator pl = player_info_->find(owner_);
	if (pl == player_info_->end()) {
		ERR_GAME << "ERROR: Could not find host in player_info_. (socket: "
			<< owner_ << ")\n";
		return;
	}
	// Mark the host as unavailable in the lobby.
	pl->second.mark_available(id_, name_);

}

bool game::is_observer(const network::connection player) const {
	return std::find(observers_.begin(),observers_.end(),player) != observers_.end();
}

bool game::is_muted_observer(const network::connection player) const {
	return std::find(muted_observers_.begin(), muted_observers_.end(), player)
		!= muted_observers_.end();
}

bool game::is_player(const network::connection player) const {
	return std::find(players_.begin(),players_.end(),player) != players_.end();
}

bool game::filter_commands(const network::connection player, config& cfg) const {
	if(is_observer(player)) {
		std::vector<int> marked;
		int index = 0;

		const config::child_list& children = cfg.get_children("command");
		for(config::child_list::const_iterator i = children.begin();
			i != children.end(); ++i)
		{
			if ((observers_can_label() && (*i)->child("label") != NULL)
				|| (observers_can_chat() && (*i)->child("speak") != NULL)
				&& (*i)->all_children().size() == 1)
			{
			} else {
				DBG_GAME << "removing observer's illegal command\n";
				marked.push_back(index - marked.size());
			}

			++index;
		}

		for(std::vector<int>::const_iterator j = marked.begin(); j != marked.end(); ++j) {
			cfg.remove_child("command",*j);
		}

		return cfg.all_children().empty() == false;
	}

	return true;
}

namespace {
std::string describe_turns(int turn, const std::string& num_turns)
{
	char buf[50];
	snprintf(buf,sizeof(buf),"%d/",int(turn));

	if(num_turns == "-1") {
		return buf + std::string("-");
	} else {
		return buf + num_turns;
	}
}

}//anon namespace

void game::start_game()
{
	started_ = true;

	//set all side controllers to 'human' so that observers will understand that they can't
	//take control of any sides if they happen to have the same name as one of the descriptions
	
	for(config::child_itors sides = level_.child_range("side");
		sides.first != sides.second; ++sides.first)
	{
		if ((**sides.first)["controller"] != "null")
			(**sides.first)["controller"] = "human";
	}

	int turn =  lexical_cast_default<int>(level()["turn_at"], 1);
	end_turn_ = (turn - 1) * level_.get_children("side").size();
	if(description()) {
		description()->values["turn"] = describe_turns(turn, level()["turns"]);
	} else {
		ERR_GAME << "ERROR: Game without description_ started. (" << id_ << ")\n";
	}

	allow_observers_ = level_["observer"] != "no";

	// Send [observer] tags for all observers that are already in the game.
	for(user_vector::const_iterator ob = observers_.begin();
		ob != observers_.end(); ++ob)
	{
		const player_map::const_iterator obs = player_info_->find(*ob);
		if (obs == player_info_->end()) {
			ERR_GAME << "Game: " << id_
				<< " ERROR: Can not find observer in player_info_. (socket: "
				<< *ob << ")\n";
			continue;
		}
		config observer_join;
		observer_join.add_child("observer").values["name"] = obs->second.name();
		// Send observer join to everyone except the new observer.
		send_data(observer_join, *ob);
	}
}

bool game::take_side(network::connection player, const config& cfg)
{
	if (!is_member(player)) return false;

	//verify that side is a side id
	const std::string& side = cfg["side"];
	size_t side_num = lexical_cast_in_range<size_t, std::string>(side, 1, 1, gamemap::MAX_PLAYERS);

	//if the side is already taken, see if we can give the player
	//another side instead
	if(side_controllers_[side_num - 1] == "human"
	   || (sides_taken_[side_num - 1] && side_controllers_[side_num - 1] == "network")) {
		const config::child_list& sides = level_.get_children("side");
		for(config::child_list::const_iterator i = sides.begin(); i != sides.end(); ++i) {
			if((**i)["controller"] == "network") {
				//don't allow players to take sides in games with invalid side numbers
				try {
					side_num = lexical_cast<size_t, std::string>((**i)["side"]);
					if(side_num < 1 || side_num > gamemap::MAX_PLAYERS)
						return false;
				}
				catch(bad_lexical_cast&) {
					return false;
				}
				//check if the side is taken if not take it
				if(!sides_taken_[side_num - 1]) {
					side_controllers_[side_num - 1] = "network";
					sides_[side_num - 1] = player;
					sides_taken_[side_num - 1] = true;
					config new_cfg = cfg;
					new_cfg["side"] = (**i)["side"];
					network::send_data(new_cfg, owner_, send_gzipped);
					return true;
				}
			}
		}
		//if we get here we couldn't find a side to take
		return false;
	}
	//else take the current side
	if (player == owner_ && started_ && !cfg["controller"].empty()) {
		//if the owner transfered a side to an ai or a human in game
		//fake a "change_controller" command so other player update controller name
		config fake;
		config& change = fake.add_child("change_controller");
		change["side"] = side;
		change["player"] = cfg["name"];
		change["controller"] = "network";
		send_data(fake, owner_); //send change to all except owner
		//send a server message displaying new controller name
		send_data(construct_server_message(cfg["name"] + " takes control of side " + side));

		//update level_ so observers who join get the new player name
		config::child_itors it = level_.child_range("side");
		it.first += side_num - 1;
		wassert(it.first != it.second);
		(**it.first)["current_player"] = cfg["name"];

		side_controllers_[side_num - 1] = cfg["controller"];
	} else
		side_controllers_[side_num - 1] = "network";
	sides_[side_num - 1] = player;
	sides_taken_[side_num - 1] = true;
	// Send the taken side to the host.
	network::send_data(cfg, owner_, send_gzipped);
	return true;
}

//! Resets the side configuration according to the scenario data.
void game::update_side_data() {
	// Remember everyone that is in the game.
	const user_vector users = all_game_users();

	sides_taken_.clear();
	sides_taken_.resize(gamemap::MAX_PLAYERS);
	sides_.clear();
	sides_.resize(gamemap::MAX_PLAYERS);
	players_.clear();
	observers_.clear();

	const config::child_itors level_sides = level_.child_range("side");
	bool side_found;
	//for each player:
	// * Find the player name
	// * Find the side this player name corresponds to
	//! @todo: Should iterate over sides_ then over users.
	for(user_vector::const_iterator player = users.begin(); player != users.end(); ++player) {
		const player_map::const_iterator info = player_info_->find(*player);
		if (info == player_info_->end()) {
			ERR_GAME << "Game: " << id_ << " ERROR: unable to find player info for connection: "
				<< *player << "\n";
			continue;
		}

		side_found = false;
		size_t side_num;
		for(config::child_iterator sd = level_sides.first;
			sd != level_sides.second; ++sd)
		{
			try {
				side_num = lexical_cast<size_t, std::string>((**sd)["side"]);
				if(side_num < 1 || side_num > gamemap::MAX_PLAYERS)
					continue;
			}
			catch(bad_lexical_cast&) {
				continue;
			}
			if (!sides_taken_[side_num - 1]){
				if((**sd)["controller"] == "network") {
					side_controllers_[side_num - 1] = "network";
					/*TODO : change the next line (and all related code :o) to :
					  if((**sd)["curent_player"] == info->second.name()) {
					  I won't make it before 1.2 is out in case of regression */
					if((**sd)["user_description"] == info->second.name()) {
						sides_[side_num - 1] = *player;
						sides_taken_[side_num - 1] = true;
						side_found = true;
					}
					else {
						sides_taken_[side_num - 1] = false;
					}
				}
				else if((**sd)["controller"] == "ai") {
					sides_[side_num - 1] = owner_;
					side_controllers_[side_num - 1] = "ai";
					sides_taken_[side_num - 1] = true;
					side_found = true;
				}
				else if((**sd)["controller"] == "human") {
					sides_[side_num - 1] = owner_;
					sides_taken_[side_num - 1] = true;
					side_found = true;
					side_controllers_[side_num - 1] = "human";
				}
			}
		}

		if (side_found){
			players_.push_back(*player);
		} else {
			observers_.push_back(*player);
		}
	}
	DBG_GAME << debug_player_info();
}

void game::transfer_side_control(const network::connection sock, const config& cfg) {
	DBG_GAME << "transfer_side_control...\n";

	//check, if this socket belongs to a player
	const user_vector::iterator pl = std::find(players_.begin(), players_.end(), sock);
	if (pl == players_.end()) {
		ERR_GAME << "ERROR: Not a player of this game. (socket: " << sock << ")\n";
		return;
	}
	const std::string& newplayer_name = cfg["player"];
	//find the player that is passed control
	player_map::const_iterator newplayer;
	for (newplayer = player_info_->begin(); newplayer != player_info_->end(); newplayer++) {
		if (newplayer->second.name() == newplayer_name) {
			break;
		}
	}
	// Is he in this game?
	if (newplayer == player_info_->end() || !is_member(newplayer->first)) {
		network::send_data(construct_server_message("Player/Observer not in this game."), sock, send_gzipped);
		return;
	}
	// Check the side number.
	const std::string& side = cfg["side"];
	size_t side_num;
	try {
		side_num = lexical_cast<size_t, std::string>(side);
		if(side_num < 1 || side_num > gamemap::MAX_PLAYERS) {
			std::ostringstream msg;
			msg << "The side number has to be between 1 and " 
			    << gamemap::MAX_PLAYERS << ".";
			network::send_data(construct_server_message(msg.str()), sock, send_gzipped);
			return;
		}
	}
	catch(bad_lexical_cast&) {
		network::send_data(construct_server_message("Not a side number."), sock, send_gzipped);
		return;
	}
	if (side_num > level_.get_children("side").size()) {
		network::send_data(construct_server_message("Invalid side number."), sock, send_gzipped);
		return;
	}

	if(side_controllers_[side_num - 1] == "network" && sides_taken_[side_num - 1]
	   && cfg["own_side"] != "yes")
	{
		network::send_data(construct_server_message(
			"This side is already controlled by a player."), sock, send_gzipped);
		return;
	}
	// Check if the sender actually owns the side he gives away or is the host.
	if (!(sides_[side_num - 1] == sock || (sock == owner_))) {
		DBG_GAME << "Side belongs to: " << sides_[side_num - 1] << "\n";
		network::send_data(construct_server_message("Not your side."), sock, send_gzipped);
		return;
	}
	if (newplayer->first == sock) {
		network::send_data(construct_server_message(
			"That's already your side, silly."), sock, send_gzipped);
		return;
	}
	sides_[side_num - 1] = 0;
	bool host_leave = false;
	// If the player gave up their last side, make them an observer.
	if (std::find(sides_.begin(), sides_.end(), sock) == sides_.end()) {
		observers_.push_back(*pl);
		players_.erase(pl);
		const std::string& name = player_info_->find(sock)->second.name();
		// Tell others that the player becomes an observer.
		send_data(construct_server_message(name	+ " becomes an observer."));
		// Update the client side observer list for everyone except player.
		config observer_join;
		observer_join.add_child("observer").values["name"] = name;
		send_data(observer_join, sock);
		// If this player was the host of the game, choose another player.
		if (sock == owner_) {
			host_leave = true;
			if (!players_.empty()) {
				owner_ = players_.front();
				send_data(construct_server_message(name
					+ " has been chosen as the new host."));
			} else {
				owner_ = newplayer->first;
				send_data(construct_server_message(newplayer_name
					+ " has been chosen as the new host."));
			}
		}
	}
	side_controllers_[side_num - 1] = "network";
	sides_taken_[side_num - 1] = true;
	sides_[side_num - 1] = newplayer->first;

	//send "change_controller" msg that make all client update
	//the current player name
	config response;
	config& change = response.add_child("change_controller");

	change["side"] = side;
	change["player"] = newplayer_name;
	// Tell everyone but the new player that this side is network controlled now.
	change["controller"] = "network";
	send_data(response, newplayer->first);
	// Tell the new player that he controls this side now.
	change["controller"] = "human";
	network::send_data(response, newplayer->first, send_gzipped);

	// Update the level so observer who join get the new name.
	config::child_itors it = level_.child_range("side");
	it.first += side_num - 1;
	wassert(it.first != it.second);
	(**it.first)["current_player"] = newplayer_name;

	//if the host left and there are ai sides, transfer them to the new host
	//and notify the new host's client about its new status
	if (host_leave) {
		for (unsigned int i = 0; i < side_controllers_.size(); i++) {
			if (side_controllers_[i] == "ai") {
				change["side"] = lexical_cast<std::string, unsigned int>(i + 1);
				change["controller"] = "ai";
				network::send_data(response, owner_, send_gzipped);
				sides_[side_num - 1] = owner_;
			}
		}
		notify_new_host();
	}

	// If we gave the new side to an observer add him to players_.
	const user_vector::iterator itor = std::find(observers_.begin(),
		observers_.end(), newplayer->first);
	if (itor != observers_.end()) {
		players_.push_back(*itor);
		observers_.erase(itor);
		// Send everyone a message saying that the observer who is taking the
		// side has quit.
		config observer_quit;
		observer_quit.add_child("observer_quit").values["name"] = newplayer_name;
		send_data(observer_quit);
	}
	send_data(construct_server_message(newplayer_name
		+ " takes control of side " + side + "."));
}

void game::notify_new_host(){
	const player_map::const_iterator it_host = player_info_->find(owner_);
	if (it_host != player_info_->end()){
		config cfg;
		config& cfg_host_transfer = cfg.add_child("host_transfer");
		cfg_host_transfer["name"] = it_host->second.name();
		cfg_host_transfer["value"] = "1";
		network::send_data(cfg, owner_, send_gzipped);
	}
}

bool game::describe_slots() {
	if(started_ || description_ == NULL)
		return false;

	int available_slots = 0;
	int num_sides = level_.get_children("side").size();
	int i = 0;
	for(config::child_list::const_iterator it = level_.get_children("side").begin(); it != level_.get_children("side").end(); ++it, ++i) {
		if ((**it)["allow_player"] == "no" || (**it)["controller"] == "null") {
			num_sides--;
		} else {
			if (!sides_taken_[i])
				available_slots++;
		}
	}
	char buf[50];
	snprintf(buf,sizeof(buf), "%d/%d", available_slots, num_sides);

	if (buf != (*description_)["slots"]) {
		description_->values["slots"] = buf;
		return true;
	} else {
		return false;
	}
}

//! Checks whether the connection's ip address is banned.
bool game::player_is_banned(const network::connection sock) const {
	std::vector<std::string>::const_iterator ban =
		std::find(bans_.begin(), bans_.end(), network::ip_address(sock));
	return ban != bans_.end();
}

//! Mute an observer or give a message of all currently muted observers if no
//! name is given.
void game::mute_observer(const config& mute) {
	const std::string name = mute["username"];
	if (name.empty()) {
		if (all_observers_muted_) {
			send_data(construct_server_message("All observers are muted."));
			return;
		}
		std::string muted_nicks = "";
		for (user_vector::const_iterator muted_obs = muted_observers_.begin();
			 muted_obs != muted_observers_.end(); ++muted_obs)
		{
			if (muted_nicks != "") {
				muted_nicks += ", ";
			}
			muted_nicks += player_info_->find(*muted_obs)->second.name();
		}
		send_data(construct_server_message("Muted observers: " + muted_nicks));
		return;
	}
	const player_map::const_iterator pl = find_user(name);
		//! @todo FIXME: Maybe rather save muted nicks as a vector of strings
		//! and also allow muting of usernames not in the game.
	if (pl == player_info_->end() || !is_observer(pl->first)) {
		network::send_data(construct_server_message(
			"Observer not found."), owner_, send_gzipped);
		return;
	}
	if (is_muted_observer(pl->first)) {
		network::send_data(construct_server_message(pl->second.name()
			+ " is already muted."), owner_, send_gzipped);
		return;
	}
	//! Prevent muting ourselves.
	if (pl->first != owner_) {
		muted_observers_.push_back(pl->first);
		network::send_data(construct_server_message(
			"You have been muted."), pl->first, send_gzipped);
		send_data(construct_server_message(pl->second.name()
			+ " has been muted."), pl->first);
	}
}

//! Kick a member by name.
//! @return the network handle of the removed member if successful, '0' otherwise.
network::connection game::kick_member(const config& kick) {
	const std::string name = kick["username"];
	const player_map::const_iterator user = find_user(name);
	if (user == player_info_->end() || !is_member(user->first)) {
		network::send_data(construct_server_message(
			"Not a member of this game."), owner_, send_gzipped);
		return 0;
	}
	if (user->first == owner_) {
		network::send_data(construct_server_message(
			"Don't kick yourself, silly."), owner_, send_gzipped);
		return 0;
	}
	LOG_GAME << network::ip_address(owner_) << "\t"
		<< owner_ << "\tkicked: " << user->second.name() << "\tfrom game:\t"
		<< name_ << "\" (" << id_ << ")\n";
	network::send_data(construct_server_message("You have been kicked."),
		user->first, send_gzipped);
	const config msg = construct_server_message(name + " has been kicked.");
	send_data(msg, user->first);
	record_data(msg);
	// Tell the user to leave the game.
	network::send_data(config("leave_game"), user->first, send_gzipped);
	remove_player(user->first);
	return user->first;
}

//! Ban a user by name.
//! The user does not need to be in this game but logged in.
//! @return the network handle of the banned player if he was in this game, '0'
//! otherwise.
network::connection game::ban_user(const config& ban) {
	const std::string name = ban["username"];
	const player_map::const_iterator user = find_user(name);
	if (user == player_info_->end()) {
		network::send_data(construct_server_message(
			"User not found."), owner_, send_gzipped);
		return 0;
	}
	if (user->first == owner_) {
		network::send_data(construct_server_message(
			"Don't ban yourself, silly."), owner_, send_gzipped);
		return 0;
	}
	if (player_is_banned(user->first)) {
		network::send_data(construct_server_message(name
			+ " is already banned."), owner_, send_gzipped);
		return 0;
	}
	LOG_GAME << network::ip_address(owner_) << "\t"
		<< owner_ << "\tbanned: " << name << "\tfrom game:\t"
		<< name_ << "\" (" << id_ << ")\n";
	bans_.push_back(network::ip_address(user->first));
	const config& msg = construct_server_message(name + " has been banned.");
	send_data(msg, user->first);
	record_data(msg);
	if (is_member(user->first)) {
		network::send_data(construct_server_message("You have been banned."),
			user->first, send_gzipped);
		// Tell the user to leave the game.
		network::send_data(config("leave_game"), user->first, send_gzipped);
		remove_player(user->first);
		return user->first;
	}
	// Don't return the user if he wasn't in this game.
	return 0;
}

bool game::process_commands(const config& cfg) {
	//DBG_GAME << "processing commands: '" << cfg.debug() << "'\n";
	bool res = false;
	const config::child_list& cmd = cfg.get_children("command");
	for(config::child_list::const_iterator i = cmd.begin(); i != cmd.end(); ++i) {
		if((**i).child("end_turn") != NULL) {
			res = end_turn();
			//DBG_GAME << "res: " << (res ? "yes" : "no") << "\n";
		}
	}

	return res;
}

bool game::end_turn() {
	//it's a new turn every time each side in the game ends their turn.
	++end_turn_;

	const size_t nsides = level_.get_children("side").size();

	if(nsides == 0 || (end_turn_%nsides) != 0) {
		return false;
	}

	const size_t turn = end_turn_/nsides + 1;

	if (description_ == NULL) {
		return false;
	}
	description_->values["turn"] = describe_turns(int(turn), level()["turns"]);

	return true;
}

void game::add_player(const network::connection player, const bool observer) {
	if(is_member(player)) {
		ERR_GAME << "ERROR: Player is already in this game. (socket: "
			<< player << ")\n";
		return;
	}
	// Hack to handle the pseudo games lobby_ and not_logged_in_.
	if (owner_ == 0) {
		observers_.push_back(player);
		return;
	}
	const player_map::iterator user = player_info_->find(player);
	if (user == player_info_->end()) {
		ERR_GAME << "ERROR: Could not find user in player_info_. (socket: "
			<< owner_ << ")\n";
		return;
	}
	user->second.mark_available(id_, name_);

	//Check first if there are available sides
	//If not, add the player as observer
	//! @todo Instead take_side() should be used.
	unsigned int human_sides = 0;
	if (level_.get_children("side").size() > 0 && !observer){
		config::child_list sides = level_.get_children("side");
		for (config::child_list::const_iterator side = sides.begin(); side != sides.end(); side++){
			if (((**side)["controller"] == "human") || ((**side)["controller"] == "network")){
				human_sides++;
			}
		}
	}
	DBG_GAME << debug_player_info();
	if (human_sides > players_.size()){
		DBG_GAME << "adding player...\n";
		players_.push_back(player);
	} else if (!allow_observers_) {
		return; //false;
	} else {
		DBG_GAME << "adding observer...\n";
		observers_.push_back(player);
	}
	DBG_GAME << debug_player_info();
	send_user_list();
	// Send the user the game data.
	network::send_data(level_, player, send_gzipped);
	//if the game has already started, we add the player as an observer
	if(started_) {
		//tell this player that the game has started
		network::send_data(config("start_game"), player, send_gzipped);
		// Send the player the history of the game to-date.
		network::send_data(history_, player, send_gzipped);
		// Send observer join of all the observers in the game to the new player
		// only once the game started. The client forgets about it anyway
		// otherwise.
		for(user_vector::const_iterator ob = observers_.begin(); ob != observers_.end(); ++ob) {
			if(*ob != player) {
				const player_map::const_iterator obs = player_info_->find(*ob);
				if(obs != player_info_->end()) {
					config cfg;
					cfg.add_child("observer").values["name"] = obs->second.name();
					network::send_data(cfg, player, send_gzipped);
				}
			}
		}
	}
	config observer_join;
	observer_join.add_child("observer").values["name"] = user->second.name();
	// Send observer join to everyone except the new observer.
	send_data(observer_join, player);
}

void game::remove_player(const network::connection player, const bool notify_creator) {
	if (!is_member(player)) {
		ERR_GAME << "ERROR: User is not in this game. (socket: "
			<< player << ")\n";
		return;
	}
	// Hack to handle the pseudo games lobby_ and not_logged_in_.
	if (owner_ == 0) {
		const user_vector::iterator itor =
			std::find(observers_.begin(), observers_.end(), player);
		if (itor != observers_.end()) {
			observers_.erase(itor);
		} else {
			ERR_GAME << "ERROR: Observer is not in this game. (socket: "
			<< player << ")\n";
		}
		return;
	}
	DBG_GAME << debug_player_info();
	DBG_GAME << "removing player...\n";

	bool host = (player == owner_);
	bool observer = true;
	{
		const user_vector::iterator itor = std::find(players_.begin(), players_.end(), player);
		if (itor != players_.end()) {
			players_.erase(itor);
			observer = false;
		}
	}
	{
		const user_vector::iterator itor = std::find(observers_.begin(), observers_.end(), player);
		if (itor != observers_.end()) {
			observers_.erase(itor);
			if (!observer) 
				ERR_GAME << "ERROR: Player is also an observer. (socket: "
				<< player << ")\n";
			observer = true;
		}
	}
	const player_map::iterator user = player_info_->find(player);
	if (user == player_info_->end()) {
		ERR_GAME << "ERROR: Could not find user in player_info_. (socket: "
			<< player << ")\n";
		return;
	}
	user->second.mark_available();
	if (observer) {
		//they're just an observer, so send them having quit to clients
		config observer_quit;
		observer_quit.add_child("observer_quit").values["name"] = user->second.name();
		send_data(observer_quit);
		return;
	}

	// The game ends if there are no more players or the host left on a not
	// yet started game.
	if (players_.empty() || (host && !started_)) return;
	// If the player was host choose a new one.
	if (host) {
		owner_ = players_.front();
		std::string owner_name = "";
		const player_map::const_iterator owner = player_info_->find(owner_);
		if (owner == player_info_->end()) {
			ERR_GAME << "ERROR: Could not find new host in player_info_. (socket: "
				<< owner_ << ")\n";
		} else {
			owner_name = owner->second.name();
		}
		notify_new_host();
		send_data(construct_server_message(owner_name
			+ " has been chosen as new host."));
		//check for ai sides first and drop them, too, if the host left
		bool ai_transfer = false;
		//can't do this with an iterator, because it doesn't know the side_num - 1
		for (size_t side = 0; side < side_controllers_.size(); ++side){
			//send the host a notification of removal of this side
			if(notify_creator && players_.empty() == false && side_controllers_[side] == "ai") {
				ai_transfer = true;
				config drop;
				drop["side_drop"] = lexical_cast<std::string, size_t>(side + 1);
				drop["controller"] = "ai";
				network::send_data(drop, owner_, send_gzipped);
				sides_taken_[side] = false;
			}
		}
		if (ai_transfer) {
			std::string msg = "AI transferred to new host";
			send_data(construct_server_message(msg));
		}
	}

	//look for all sides the player controlled and drop them
	for (side_vector::iterator side = sides_.begin(); side != sides_.end(); ++side)	{
		if (*side != player) continue;
		//send the host a notification of removal of this side
		if (notify_creator && players_.empty() == false) {
			config drop;
			drop["side_drop"] = lexical_cast<std::string, size_t>(side - sides_.begin() + 1);
			drop["controller"] = side_controllers_[side - sides_.begin()];
			network::send_data(drop, owner_, send_gzipped);
		}
		side_controllers_[side - sides_.begin()] = "null";
		sides_taken_[side - sides_.begin()] = false;
		sides_[side - sides_.begin()] = 0;
	}
	DBG_GAME << debug_player_info();

	send_user_list(player);
}

void game::send_user_list(const network::connection exclude) const {
	//if the game hasn't started yet, then send all players a list
	//of the users in the game
	if (started_ == false && description_ != NULL) {
		//! @todo: Should be renamed to userlist.
		config cfg("gamelist");
		user_vector users = all_game_users();
		for(user_vector::const_iterator p = users.begin(); p != users.end(); ++p) {
			const player_map::const_iterator pl = player_info_->find(*p);
			if (pl != player_info_->end()) {
				config& user = cfg.add_child("user");
				user["name"] = pl->second.name();
			}
		}
		send_data(cfg, exclude);
	}
}


void game::send_data(const config& data, const network::connection exclude) const
{
	const user_vector users = all_game_users();
	for(user_vector::const_iterator i = users.begin(); i != users.end(); ++i) {
		if (*i != exclude) {
			network::send_data(data, *i, send_gzipped);
		}
	}
}

bool game::player_on_team(const std::string& team, const network::connection player) const {
	for (side_vector::const_iterator side = sides_.begin(); side != sides_.end();
		 side++)
	{
		const config* const side_cfg = level_.find_child("side", "side",
			lexical_cast<std::string, size_t>(side - sides_.begin() + 1));
		if (*side == player && side_cfg != NULL && (*side_cfg)["team_name"] == team) {
			return true;
		}
	}
	return false;
}

void game::send_data_team(const config& data, const std::string& team,
	const network::connection exclude) const
{
	for(user_vector::const_iterator i = players_.begin(); i != players_.end(); ++i) {
		if(*i != exclude && player_on_team(team,*i)) {
			network::send_data(data, *i, send_gzipped);
		}
	}
}

void game::send_data_observers(const config& data, const network::connection exclude) const {
	for(user_vector::const_iterator i = observers_.begin(); i != observers_.end(); ++i) {
		if (*i != exclude) {
			network::send_data(data, *i, send_gzipped);
		}
	}
}

void game::record_data(const config& data) {
	history_.append(data);
}

void game::reset_history() {
	history_.clear();
	end_turn_ = 0;
}

void game::end_game(const config& games_and_users_list) {
	const user_vector& users = all_game_users();
	// Set the availability status for all quitting players.
	for (user_vector::const_iterator user = users.begin();
		user != users.end(); user++)
	{
		const player_map::iterator pl = player_info_->find(*user);
		if (pl != player_info_->end()) {
			pl->second.mark_available();
		} else {
			ERR_GAME << "ERROR: Could not find player in player_info_. (socket: "
				<< *user << ")\n";
		}
	}
	send_data(config("leave_game"));
	send_data(games_and_users_list);
	players_.clear();
	observers_.clear();
}

void game::add_players(const game& other_game, const bool observer) {
	user_vector users = other_game.all_game_users();
	if (observer){
		observers_.insert(observers_.end(), users.begin(), users.end());
	}
	else{
		players_.insert(players_.end(), users.begin(), users.end());
	}
}

const user_vector game::all_game_users() const {
	user_vector res;

	res.insert(res.end(), players_.begin(), players_.end());
	res.insert(res.end(), observers_.begin(), observers_.end());

	return res;
}

std::string game::debug_player_info() const {
	std::stringstream result;
	result << "---------------------------------------\n";
	result << "game id: " << id_ << "\n";
	result << "players_.size: " << players_.size() << "\n";
	for (user_vector::const_iterator p = players_.begin(); p != players_.end(); p++){
		const player_map::const_iterator user = player_info_->find(*p);
		if (user != player_info_->end()){
			result << "player: " << user->second.name().c_str() << "\n";
		}
		else{
			result << "player not found\n";
		}
	}
	result << "observers_.size: " << observers_.size() << "\n";
	for (user_vector::const_iterator o = observers_.begin(); o != observers_.end(); o++){
		const player_map::const_iterator user = player_info_->find(*o);
		if (user != player_info_->end()){
			result << "observer: " << user->second.name().c_str() << "\n";
		}
		else{
			result << "observer not found\n";
		}
	}
	{
		result << "player_info_: begin\n";
		for (player_map::const_iterator info = player_info_->begin(); info != player_info_->end(); info++){
			result << info->second.name().c_str() << "\n";
		}
		result << "player_info_: end\n";
	}
	result << "---------------------------------------\n";
	return result.str();
}

//! Find a user by name.
player_map::const_iterator game::find_user(const std::string& name) const {
	player_map::const_iterator pl;
	for (pl = player_info_->begin(); pl != player_info_->end(); pl++) {
		if (pl->second.name() == name)
			return pl;
	}
	return player_info_->end();
}

config game::construct_server_message(const std::string& message) const
{
	config turn;
	if(started_) {
		config& cmd = turn.add_child("turn");
		config& cfg = cmd.add_child("command");
		config& msg = cfg.add_child("speak");
		msg["description"] = "server";
		msg["message"] = message;
	} else {
		config& msg = turn.add_child("message");
		msg["sender"] = "server";
		msg["message"] = message;
	}
	return turn;
}
