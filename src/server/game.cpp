/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "../global.hpp"

#include "game.hpp"
#include "log.hpp"
#include "../util.hpp"
#include "../wassert.hpp"

#include <iostream>
#include <sstream>

#define LOG_SERVER LOG_STREAM(info, general)

int game::id_num = 1;

game::game(const player_map& info) : player_info_(&info), id_(id_num++), sides_taken_(9), side_controllers_(9), started_(false), description_(NULL), end_turn_(0), allow_observers_(true), all_observers_muted_(false)
{}

bool game::is_owner(network::connection player) const
{
	return (!players_.empty() && player == players_.front());
}

bool game::is_member(network::connection player) const
{
	return is_player(player) || is_observer(player);
}

bool game::is_needed(network::connection player) const
{
	return (!players_.empty() && player == players_.front());
}

bool game::is_observer(network::connection player) const
{
	return std::find(observers_.begin(),observers_.end(),player) != observers_.end();
}

bool game::is_muted_observer(network::connection player) const
{
	return std::find(muted_observers_.begin(), muted_observers_.end(), player) != muted_observers_.end();
}

bool game::is_player(network::connection player) const
{
	return std::find(players_.begin(),players_.end(),player) != players_.end();
}

bool game::all_observers_muted() const{
	return all_observers_muted_;
}

bool game::observers_can_label() const
{
	return false;
}

bool game::observers_can_chat() const
{
	return true;
}

bool game::filter_commands(network::connection player, config& cfg)
{
	if(is_observer(player)) {
		std::vector<int> marked;
		int index = 0;

		const config::child_list& children = cfg.get_children("command");
		for(config::child_list::const_iterator i = children.begin(); i != children.end(); ++i) {

			if(observers_can_label() && (*i)->child("label") != NULL && (*i)->all_children().size() == 1) {
				;
			} else if(observers_can_chat() && (*i)->child("speak") != NULL && (*i)->all_children().size() == 1) {
				;
			} else {
				std::cerr << "removing observer's illegal command\n";
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

}

void game::start_game()
{
	started_ = true;

	//set all side controllers to 'human' so that observers will understand that they can't
	//take control of any sides if they happen to have the same name as one of the descriptions
	config::child_itors sides = level_.child_range("side");
	for(; sides.first != sides.second; ++sides.first) {
		(**sides.first)["controller"] = "human";
	}

	describe_slots();
	if(description()) {
		description()->values["turn"] = describe_turns(1,level()["turns"]);
	}

	allow_observers_ = level_["observer"] != "no";

	//send [observer] tags for all observers that have already joined.
	//we can do this by re-joining all players that don't have sides, and aren't
	//the first player (game creator)
	for(std::vector<network::connection>::const_iterator pl = observers_.begin(); pl != observers_.end(); ++pl) {
		if(sides_.count(*pl) == 0) {
			add_player(*pl);
		}
	}
}

bool game::take_side(network::connection player, const config& cfg)
{
	wassert(is_member(player));

	// verify that side is a side id
	const std::string& side = cfg["side"];
	size_t side_num;
	try {
		side_num = lexical_cast<size_t, std::string>(side);
		if(side_num < 1 || side_num > 9)
			return false;
	}
	catch(bad_lexical_cast&) {
		return false;
	}

	size_t side_index = static_cast<size_t>(side_num - 1);

	//if the side is already taken, see if we can give the player
	//another side instead
	if(side_controllers_[side_index] == "human" || (sides_taken_[side_index] && side_controllers_[side_index] == "network")) {
		const config::child_list& sides = level_.get_children("side");
		for(config::child_list::const_iterator i = sides.begin(); i != sides.end(); ++i) {
			if((**i)["controller"] == "network") {
				// don't allow players to take sides in games with invalid side numbers
				try {
					side_num = lexical_cast<size_t, std::string>((**i)["side"]);
					if(side_num < 1 || side_num > 9)
						return false;
				}
				catch(bad_lexical_cast&) {
					return false;
				}
				// check if the side is taken if not take it
				side_index = static_cast<size_t>(side_num - 1);
				if(!sides_taken_[side_index]) {
					side_controllers_[side_index] = "network";
					sides_.insert(std::pair<network::connection, size_t>(player, side_index));
					sides_taken_[side_index] = true;
					config new_cfg = cfg;
					new_cfg["side"] = (**i)["side"];
					network::queue_data(new_cfg, players_.front());
					return true;
				}
			}
		}
		// if we get here we couldn't find a side to take
		return false;
	}
	// else take the current side
	side_controllers_[side_index] = "network";
	sides_.insert(std::pair<network::connection, size_t>(player, side_index));
	sides_taken_[side_index] = true;
	network::queue_data(cfg, players_.front());
	return true;
}

void game::update_side_data()
{
	sides_taken_.clear();
	sides_taken_.resize(9);
	sides_.clear();

	const config::child_itors level_sides = level_.child_range("side");

	//for each player:
	// * Find the player name
	// * Find the side this player name corresponds to
	for(std::vector<network::connection>::const_iterator player = players_.begin();
			player != players_.end(); ++player) {

		player_map::const_iterator info = player_info_->find(*player);
		if (info == player_info_->end()) {
			std::cerr << "Error: unable to find player info for connection " << *player << "\n";
			continue;
		}

		size_t side_num;
		size_t side_index;
		config::child_iterator sd;
		for(sd = level_sides.first; sd != level_sides.second; ++sd) {
			try {
				side_num = lexical_cast<size_t, std::string>((**sd)["side"]);
				if(side_num < 1 || side_num > 9)
					continue;
			}
			catch(bad_lexical_cast&) {
				continue;
			}
			side_index = static_cast<size_t>(side_num - 1);

			if (! sides_taken_[side_index]){
				if((**sd)["controller"] == "network") {
					side_controllers_[side_index] = "network";
					if((**sd)["user_description"] == info->second.name()) {
						sides_.insert(std::pair<network::connection, size_t>(*player, side_index));
						sides_taken_[side_index] = true;
					}
					else {
						sides_taken_[side_index] = false;
					}
				}
				else if((**sd)["controller"] == "ai") {
					side_controllers_[side_index] = "ai";
					sides_taken_[side_index] = true;
				}
				else if((**sd)["controller"] == "human") {
					sides_.insert(std::pair<network::connection,size_t>(players_.front(),side_index));
					sides_taken_[side_index] = true;
					side_controllers_[side_index] = "human";
				}
			}
		}
	}
}

const std::string& game::transfer_side_control(const config& cfg)
{
	const std::string& player = cfg["player"];

	//find the socket for the player that is passed control
	network::connection sock_entering = NULL;
	for (player_map::const_iterator pl = player_info_->begin(); pl != player_info_->end(); pl++){
		if (pl->second.name() == player){
			sock_entering = pl->first;
			break;
		}
	}

	if (sock_entering == NULL){
		static const std::string notfound = "Player/Observer not found";
		return notfound;
	}

	user_vector::iterator i = find_in_observers(sock_entering);
	user_vector::iterator j = find_in_players(sock_entering);

	if (i == observers_.end() && j == players_.end()){
		static const std::string notfound = "Player/Observer not found";
		return notfound;
	}

	const std::string& side = cfg["side"];
	static const std::string invalid = "Invalid side number";
	size_t side_num;
	try {
		side_num = lexical_cast<size_t, std::string>(side);
		if(side_num < 1 || side_num > 9)
			return invalid;
	}
	catch(bad_lexical_cast&) {
		return invalid;
	}
	const size_t nsides = level_.get_children("side").size();
	if(side_num > nsides)
		return invalid;

	const size_t side_index = static_cast<size_t>(side_num - 1);

	if(side_controllers_[side_index] == "network" && sides_taken_[side_index] && cfg["own_side"] != "yes") {
		static const std::string already = "This side is already controlled by a player";
		return already;
	}

	//The player owns this side
	if(cfg["own_side"] == "yes") {
		//get the socket of the player that issued the command
		network::connection sock = NULL;
		for (std::multimap<network::connection, size_t>::iterator s = sides_.begin(); s != sides_.end(); s++){
			if (s->second == side_index){
				sock = s->first;
				break;
			}
		}

		if (sock == NULL){
			static const std::string player_not_found = "This side is not listed for the game";
			return player_not_found;
		}

		//check, if this socket belongs to a player
		user_vector::iterator p = find_in_players(sock);
		if (p == players_.end()){
			static const std::string no_player = "The player for this side could not be found";
			return no_player;
		}

		//if this player controls only one side, make him an observer
		if (sides_.count(sock) == 1){
			//we need to save this information before the player is erased
			bool host = is_needed(sock);

			observers_.push_back(*p);
			players_.erase(p);

			//tell others that the player becomes an observer
			config cfg_observer = construct_server_message(find_player(sock)->name() + " becomes observer");
			send_data(cfg_observer);

			//update the client observer list for everyone except player
			config observer_join;
			observer_join.add_child("observer").values["name"] = find_player(sock)->name();
			send_data(observer_join, sock);

			//if this player was the host of the game, transfer game control to another player
			if (host && transfer_game_control() != NULL){
				const config& msg = construct_server_message(transfer_game_control()->name() + " has been chosen as new host");
				send_data(msg);
			}

			//reiterate because iterators became invalid
			i = find_in_observers(sock_entering);
			j = find_in_players(sock_entering);
		}

		//clear the sides_ entry
		sides_.erase(s);
	}

	side_controllers_[side_index] = "network";
	sides_taken_[side_index] = true;
	sides_.insert(std::pair<const network::connection,size_t>(sock_entering, side_index));

	// send a response to the host and to the new controller
	config response;
	config& change = response.add_child("change_controller");

	change["side"] = side;

	change["controller"] = "network";
	network::queue_data(response,players_.front());

	change["controller"] = "human";
	network::queue_data(response, sock_entering);

	if(sides_.count(sock_entering) < 2) {
		//send everyone a message saying that the observer who is taking the side has quit
		config observer_quit;
		observer_quit.add_child("observer_quit").values["name"] = player;
		send_data(observer_quit);
	}
	if (i != observers_.end()){
		players_.push_back(*i);
		observers_.erase(i);
	}
	static const std::string success = "";
	return success;
}

size_t game::available_slots() const
{
	size_t available_slots = 0;
	const config::child_list& sides = level_.get_children("side");
	for(config::child_list::const_iterator i = sides.begin(); i != sides.end(); ++i) {
		//std::cerr << "side controller: '" << (**i)["controller"] << "'\n";
		//std::cerr << "description: '" << (**i)["description"] << "'\n";
		if((**i)["controller"] == "network" && (**i)["description"].empty()) {
			++available_slots;
		}
	}

	return available_slots;
}

bool game::describe_slots()
{
	if(description() == NULL)
		return false;

	const int val = int(available_slots());
	char buf[50];
	int num_sides = level_.get_children("side").size();
	for(config::child_list::const_iterator it = level_.get_children("side").begin(); it != level_.get_children("side").end(); ++it) {
		if((**it)["allow_player"] == "no") {
			num_sides--;
		}
	}
	snprintf(buf,sizeof(buf),"%d/%d",val,num_sides);

	if(buf != (*description())["slots"]) {
		description()->values["slots"] = buf;
		description()->values["observer"] = level_["observer"];
		return true;
	} else {
		return false;
	}
}

bool game::player_is_banned(network::connection sock) const
{
	if(bans_.empty()) {
		return false;
	}

	const player_map::const_iterator itor = player_info_->find(sock);
	if(itor == player_info_->end()) {
		return false;
	}

	const player& info = itor->second;
	const std::string& ipaddress = network::ip_address(sock);
	for(std::vector<ban>::const_iterator i = bans_.begin(); i != bans_.end(); ++i) {
		if(info.name() == i->username || ipaddress == i->ipaddress) {
			return true;
		}
	}

	return false;
}

const player* game::mute_observer(network::connection sock){
	const player* muted_observer = NULL;
	player_map::const_iterator it = player_info_->find(sock);
	if (it != player_info_->end() && ! is_muted_observer(sock)){
		muted_observers_.push_back(sock);
		muted_observer = &it->second;
	}

	return muted_observer;
}

void game::mute_all_observers(bool mute){
	all_observers_muted_ = mute;
}

void game::ban_player(network::connection sock)
{
	const player_map::const_iterator itor = player_info_->find(sock);
	if(itor != player_info_->end()) {
		bans_.push_back(ban(itor->second.name(),network::ip_address(sock)));
	}

	remove_player(sock);
}

bool game::process_commands(const config& cfg)
{
	//std::cerr << "processing commands: '" << cfg.write() << "'\n";
	bool res = false;
	const config::child_list& cmd = cfg.get_children("command");
	for(config::child_list::const_iterator i = cmd.begin(); i != cmd.end(); ++i) {
		if((**i).child("end_turn") != NULL) {
			res = res || end_turn();
			//std::cerr << "res: " << (res ? "yes" : "no") << "\n";
		}
	}

	return res;
}

bool game::end_turn()
{
	//it's a new turn every time each side in the game ends their turn.
	++end_turn_;

	const size_t nsides = level_.get_children("side").size();

	if((end_turn_%nsides) != 0) {
		return false;
	}

	const size_t turn = end_turn_/nsides + 1;

	config* const desc = description();
	if(desc == NULL) {
		return false;
	}

	desc->values["turn"] = describe_turns(int(turn),level()["turns"]);

	return true;
}

void game::add_player(network::connection player, bool observer)
{
	//if the game has already started, we add the player as an observer
	if(started_) {
		if(!allow_observers_) {
			return;
		}

		player_map::const_iterator info = player_info_->find(player);
		if(info != player_info_->end()) {
			config observer_join;
			observer_join.add_child("observer").values["name"] = info->second.name();
			//send observer join to everyone except player
			send_data(observer_join, player);
		}

		//tell this player that the game has started
		config cfg;
		cfg.add_child("start_game");
		network::queue_data(cfg, player);

		//send observer join of all the observers in the game to player
		for(std::vector<network::connection>::const_iterator ob = observers_.begin(); ob != observers_.end(); ++ob) {
			if(*ob != player) {
				info = player_info_->find(*ob);
				if(info != player_info_->end()) {
					cfg.clear();
					cfg.add_child("observer").values["name"] = info->second.name();
					network::queue_data(cfg, player);
				}
			}
		}
	}

	//if the player is already in the game, don't add them.
	user_vector users = all_game_users();
	if(std::find(users.begin(),users.end(),player) != users.end()) {
		return;
	}

	//Check first if there are available sides
	//If not, add the player as observer
	unsigned int human_sides = 0;
	if (level_.get_children("side").size() > 0 && !observer){
		config::child_list sides = level_.get_children("side");
		for (config::child_list::const_iterator side = sides.begin(); side != sides.end(); side++){
			if (((**side)["controller"] == "human") || ((**side)["controller"] == "network")){
				human_sides++;
			}
		}
	}
	else{
		if (level_.get_attribute("human_sides") != ""){
			human_sides = lexical_cast<unsigned int>(level_["human_sides"]);
		}
	}
	LOG_SERVER << debug_player_info();
	if (human_sides > players_.size()){
		LOG_SERVER << "adding player...\n";
		players_.push_back(player);
	} else{
		LOG_SERVER << "adding observer...\n";
		observers_.push_back(player);
	}
	LOG_SERVER << debug_player_info();
	send_user_list();
	player_map::const_iterator info = player_info_->find(player);
	if(info != player_info_->end()) {
		config observer_join;
		observer_join.add_child("observer").values["name"] = info->second.name();
		//send observer join to everyone except player
		send_data(observer_join, player);
	}

	//send the player the history of the game to-date
	network::queue_data(history_,player);
}

void game::remove_player(network::connection player, bool notify_creator)
{
	LOG_SERVER << "removing player...\n";
	{
		const user_vector::iterator itor = std::find(players_.begin(),players_.end(),player);

		if(itor != players_.end())
			players_.erase(itor);
	}

	{
		const user_vector::iterator itor = std::find(observers_.begin(),observers_.end(),player);

		if(itor != observers_.end())
			observers_.erase(itor);
	}

	LOG_SERVER << debug_player_info();
	bool observer = true;
	std::pair<std::multimap<network::connection,size_t>::const_iterator,
	          std::multimap<network::connection,size_t>::const_iterator> sides = sides_.equal_range(player);
	while(sides.first != sides.second) {
		//send the host a notification of removal of this side
		if(notify_creator && players_.empty() == false) {
			config drop;
			drop["side_drop"] = lexical_cast<std::string, size_t>(sides.first->second + 1);
			network::queue_data(drop, players_.front());
		}
		sides_taken_[sides.first->second] = false;
		observer = false;
		++sides.first;
	}
	if(!observer)
		sides_.erase(player);

	send_user_list();

	const player_map::const_iterator pl = player_info_->find(player);
	if(!observer || pl == player_info_->end()) {
		return;
	}

	//they're just an observer, so send them having quit to clients
	config observer_quit;
	observer_quit.add_child("observer_quit").values["name"] = pl->second.name();
	send_data(observer_quit);
}

void game::send_user_list()
{
	//if the game hasn't started yet, then send all players a list
	//of the users in the game
	if(started_ == false && description() != NULL) {
		config cfg;
		cfg.add_child("gamelist");
		user_vector users = all_game_users();
		for(user_vector::const_iterator p = users.begin(); p != users.end(); ++p) {
			const player_map::const_iterator info = player_info_->find(*p);
			if(info != player_info_->end()) {
				config& user = cfg.add_child("user");
				user["name"] = info->second.name();
			}
		}

		send_data(cfg);
	}
}


int game::id() const
{
	return id_;
}

void game::send_data(const config& data, network::connection exclude)
{
	const user_vector users = all_game_users();
	for(user_vector::const_iterator i = users.begin(); i != users.end(); ++i) {
		if(*i != exclude && (allow_observers_ || is_needed(*i) || sides_.count(*i) == 1)) {
			network::queue_data(data,*i);
		}
	}
}

bool game::player_on_team(const std::string& team, network::connection player) const
{
	std::pair<std::multimap<network::connection,size_t>::const_iterator,
	          std::multimap<network::connection,size_t>::const_iterator> sides = sides_.equal_range(player);
	while(sides.first != sides.second) {
		const config* const side_cfg = level_.find_child("side","side", lexical_cast<std::string, size_t>(sides.first->second + 1));
		if(side_cfg != NULL && (*side_cfg)["team_name"] == team) {
			return true;
		}
		++sides.first;
	}

	return false;
}

void game::send_data_team(const config& data, const std::string& team, network::connection exclude)
{
	for(std::vector<network::connection>::const_iterator i = players_.begin(); i != players_.end(); ++i) {
		if(*i != exclude && player_on_team(team,*i)) {
			network::queue_data(data,*i);
		}
	}
}

void game::send_data_observers(const config& data)
{
	for(std::vector<network::connection>::const_iterator i = observers_.begin(); i != observers_.end(); ++i) {
		network::queue_data(data,*i);
	}
}

void game::record_data(const config& data)
{
	history_.append(data);
}

void game::reset_history()
{
	history_.clear();
}

bool game::level_init() const
{
	return level_.child("side") != NULL;
}

config& game::level()
{
	return level_;
}

bool game::empty() const
{
	return players_.empty() && observers_.empty();
}

void game::disconnect()
{
	const user_vector users = all_game_users();
	for(user_vector::const_iterator i = users.begin();
	    i != users.end(); ++i) {
		network::queue_disconnect(*i);
	}

	players_.clear();
	observers_.clear();
}

void game::set_description(config* desc)
{
	description_ = desc;
}

config* game::description()
{
	return description_;
}

void game::add_players(const game& other_game, bool observer)
{
	user_vector users = other_game.all_game_users();
	if (observer){
		observers_.insert(observers_.end(), users.begin(), users.end());
	}
	else{
		players_.insert(players_.end(), users.begin(), users.end());
	}
}

bool game::started() const
{
	return started_;
}

const user_vector game::all_game_users() const{
	user_vector res;

	res.insert(res.end(), players_.begin(), players_.end());
	res.insert(res.end(), observers_.begin(), observers_.end());

	return res;
}

std::string game::debug_player_info() const{
	std::stringstream result;
	result << "---------------------------------------\n";
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

const player* game::find_player(network::connection sock) const{
	const player* result = NULL;
	for (player_map::const_iterator info = player_info_->begin(); info != player_info_->end(); info++){
		if (info->first == sock){
			result = &info->second;
		}
	}
	return result;
}

const player* game::transfer_game_control(){
	const player* result = NULL;

	//search for the next available player in the players list
	if (players_.size() > 0){
		result = find_player(players_[0]);
	}
	return result;
}

user_vector::iterator game::find_in_players(network::connection sock){
	user_vector::iterator p;
	for (p = players_.begin(); p != players_.end(); p++){
		if ((*p) == sock){
			return p;
		}
	}
	return players_.end();
}

user_vector::iterator game::find_in_observers(network::connection sock){
	user_vector::iterator o;
	for (o = observers_.begin(); o != observers_.end(); o++){
		if ((*o) == sock){
			return o;
		}
	}
	return observers_.end();
}

config game::construct_server_message(const std::string& message)
{
	config turn;
	if(started()) {
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
