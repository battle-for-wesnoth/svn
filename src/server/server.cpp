#include "../config.hpp"
#include "../network.hpp"

#include "SDL.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

class game
{
public:
	game() : id_(id_num++)
	{}

	bool is_member(network::connection player) const {
		return std::find(players_.begin(),players_.end(),player)
		                                         != players_.end();
	}

	void add_player(network::connection player) {
		players_.push_back(player);
	}

	void remove_player(network::connection player) {
		std::vector<network::connection>::iterator itor =
		             std::find(players_.begin(),players_.end(),player);
		if(itor != players_.end())
			players_.erase(itor);
	}

	int id() const { return id_; }

	void send_data(const config& data, network::connection exclude=0) {
		for(std::vector<network::connection>::const_iterator
		    i = players_.begin(); i != players_.end(); ++i) {
			if(*i != exclude) {
				network::send_data(data,*i);
			}
		}
	}

	bool level_init() const { return level_.child("side") != NULL; }

	config& level() { return level_; }

	bool empty() const { return players_.empty(); }

	void disconnect() {
		for(std::vector<network::connection>::iterator i = players_.begin();
		    i != players_.end(); ++i) {
			network::disconnect(*i);
		}

		players_.clear();
	}

private:
	static int id_num;
	int id_;
	std::vector<network::connection> players_;
	config level_;
};

int game::id_num = 1;

struct game_id_matches
{
	game_id_matches(int id) : id_(id) {}
	bool operator()(const game& g) const { return g.id() == id_; }

private:
	int id_;
};

int main()
{
	const network::manager net_manager;
	const network::server_manager server;

	config initial_response;
	initial_response.children["gamelist"].push_back(new config());
	config& gamelist = *initial_response.children["gamelist"].back();

	game lobby_players;
	std::vector<game> games;

	for(;;) {
		try {
			network::connection sock = network::accept_connection();
			if(sock) {
				network::send_data(initial_response,sock);
				lobby_players.add_player(sock);
			}

			config data;
			while((sock = network::receive_data(data)) != NULL) {
				if(lobby_players.is_member(sock)) {
					const config* const create_game = data.child("create_game");
					if(create_game != NULL) {

						std::cerr << "creating game...\n";

						//create the new game, remove the player from the
						//lobby and put him/her in the game they have created
						games.push_back(game());
						lobby_players.remove_player(sock);
						games.back().add_player(sock);

						//store the game data here at the moment
						games.back().level() = *create_game;
						std::stringstream converter;
						converter << games.back().id();
						games.back().level()["id"] = converter.str();

						continue;
					}

					//see if the player is joining a game
					const config* const join = data.child("join");
					if(join != NULL) {
						const std::string& id = (*join)["id"];
						const int nid = atoi(id.c_str());
						const std::vector<game>::iterator it =
						             std::find_if(games.begin(),games.end(),
						                          game_id_matches(nid));
						if(it == games.end()) {
							std::cerr << "attempt to join unknown game\n";
							continue;
						}

						lobby_players.remove_player(sock);
						it->add_player(sock);

						//send them the game data
						network::send_data(it->level(),sock);
					}

				} else {
					std::vector<game>::iterator g;
					for(g = games.begin(); g != games.end(); ++g) {
						if(g->is_member(sock))
							break;
					}

					if(g == games.end()) {
						std::cerr << "ERROR: unknown socket " << games.size() << "\n";
						continue;
					}

					//if this is data describing the level for a game
					if(data.child("side") != NULL) {

						//if this game is having its level-data initialized
						//for the first time, and is ready for players to join
						if(!g->level_init()) {
							
							//update our config object which describes the
							//open games
							gamelist.children["game"].push_back(
							                           new config(g->level()));

							//send all players in the lobby the list of games
							lobby_players.send_data(initial_response);
						}

						//record the new level data, and send to all players
						//who are in the game
						g->level() = data;

						std::cerr << "registered game: " << data.write() << "\n";
					}

					//forward data to all players who are in the game,
					//except for the original data sender
					g->send_data(data,sock);
				}
			}
			
		} catch(network::error& e) {
			if(!e.socket) {
				std::cerr << "fatal network error: " << e.message << "\n";
			} else {
				std::cerr << "error with socket: " << e.message << "\n";
				
				lobby_players.remove_player(e.socket);
				for(std::vector<game>::iterator i = games.begin();
				    i != games.end(); ++i) {
					if(i->is_member(e.socket)) {
						i->disconnect();
						games.erase(i);
					}
				}
			}
		}

		SDL_Delay(20);
	}
}
