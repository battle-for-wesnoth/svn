#include "language.hpp"
#include "log.hpp"
#include "multiplayer.hpp"
#include "multiplayer_client.hpp"
#include "multiplayer_lobby.hpp"
#include "playlevel.hpp"
#include "preferences.hpp"
#include "replay.hpp"
#include "show_dialog.hpp"

#include <sstream>

namespace {

class gamelist_manager : public gui::dialog_action
{
public:
	int do_action() {
		const network::connection res = network::receive_data(cfg);
		if(res != NULL && get_gamelist() != NULL) {
			return UPDATED_GAMELIST;
		} else {
			return CONTINUE_DIALOG;
		}
	}

	const config* get_gamelist() const {
		return cfg.child("gamelist");
	}

	enum { UPDATED_GAMELIST = -3 };

private:
	config cfg;
};

enum GAME_LIST_RESULT { QUIT_GAME, CREATE_GAME, JOIN_GAME };

GAME_LIST_RESULT manage_game_list(display& disp, const config* gamelist)
{
	gamelist_manager manager;
	for(;;) {
		std::vector<std::string> options;
		options.push_back(string_table["create_new_game"]);
		for(config::const_child_itors i = gamelist->child_range("game");
		    i.first != i.second; ++i.first) {
			options.push_back((**i.first)["name"]);
		}
		
		options.push_back(string_table["quit_button"]);

		const int res = gui::show_dialog(disp,NULL,"","Choose game to join",
		                       gui::MESSAGE,&options,NULL,"",NULL,&manager);
		if(res == gamelist_manager::UPDATED_GAMELIST) {
			gamelist = manager.get_gamelist();
		} else if(res == 0) {
			std::string name;
			const int res = gui::show_dialog(disp,NULL,"","Name your game:",
			                   gui::OK_CANCEL,NULL,NULL,"Name:",&name);
			if(res == 0) {
				config response;
				config create_game;
				create_game["name"] = name;
				response.children["create_game"].push_back(
				                                    new config(create_game));
				network::send_data(response);

				return CREATE_GAME;
			}
		} else if(size_t(res) == options.size()-1) {
			return QUIT_GAME;
		} else if(res > 0 && size_t(res) < options.size()) {
			const config::const_child_itors i = gamelist->child_range("game");
			const size_t index = size_t(res)-1;
			assert(i.second - i.first > int(index));
			const std::string& id = (**(i.first+index))["id"];
			
			config response, join;
			join["id"] = id;
			response.children["join"].push_back(new config(join));
			network::send_data(response);

			return JOIN_GAME;
		}
	}

	return QUIT_GAME;
}

void check_response(network::connection res, const config& data)
{
	if(!res) {
		throw network::error(string_table["connection_timeout"]);
	}

	const config* err = data.child("error");
	if(err != NULL) {
		throw network::error((*err)["message"]);
	}
}

void receive_gamelist(config& data)
{
	for(;;) {
		const network::connection res = network::receive_data(data,0,3000);
		check_response(res,data);

		if(data.child("gamelist")) {
			break;
		}
	}
}

class wait_for_start : public gui::dialog_action
{
public:
	wait_for_start(config& cfg) : got_side(false), sides_(cfg) {}

	int do_action() {
		config reply;
		const network::connection res = network::receive_data(reply);
		if(res) {
			std::cerr << "received data while waiting: " << reply.write() << "\n";
			if(reply.values["failed"] == "yes") {
				got_side = false;
				throw network::error("Side chosen is unavailable");
			} else if(reply["side_secured"].empty() == false) {
				got_side = true;
			} else if(reply.child("start_game")) {
				std::cerr << "received start_game message\n";
				//break out of dialog
				return START_GAME;
			} else if(reply.child("leave_game")) {
				return GAME_CANCELLED;
			} else {
				sides_ = reply;
			}
		}

		return CONTINUE_DIALOG;
	}

	bool got_side;

	enum { START_GAME = 1, GAME_CANCELLED = 2 };

private:
	config& sides_;
};
}

void play_multiplayer_client(display& disp, game_data& units_data, config& cfg,
                             game_state& state)
{
	log_scope("playing multiplayer client");

	const network::manager net_manager;

	std::string host = preferences::network_host();
	const int res = gui::show_dialog(disp,NULL,"","",
	                                 gui::OK_CANCEL,NULL,NULL,
	                                 string_table["remote_host"] + ": ",&host);
	if(res != 0 || host.empty()) {
		return;
	}

	network::connection sock;

	sock = network::connect(host);
	config sides, data;

	network::connection data_res = network::receive_data(data,0,10000);
	check_response(data_res,data);

	preferences::set_network_host(host);

	//if response contained a version number
	const std::string& version = data["version"];
	if(version.empty() == false && version != game_config::version) {
		throw network::error("The server requires version '" + version
		      + "' while you are using version'" + game_config::version + "'");
	}

	bool logged_in = false;

	//if we got a direction to login
	if(data.child("mustlogin")) {

		config* error = NULL;

		do {
			if(error != NULL) {
				gui::show_dialog(disp,NULL,"",(*error)["message"],gui::OK_ONLY);
			}

			std::string login = preferences::login();
			const int res = gui::show_dialog(disp,NULL,"",
			                    "You must log in to this server",gui::OK_CANCEL,
			                    NULL,NULL,"Login: ",&login);
			if(res != 0 || login.empty()) {
				return;
			}

			preferences::set_login(login);

			config response;
			response.add_child("login")["username"] = login;
			network::send_data(response);
	
			data_res = network::receive_data(data,0,5000);
			if(!data_res) {
				throw network::error(string_table["connection_timeout"]);
			}

			error = data.child("error");
		} while(error != NULL);

		logged_in = true;
	}

	for(bool first_time = true;
	    (first_time || logged_in) && network::nconnections() > 0;
	    first_time = false) {

		if(!first_time) {
			receive_gamelist(data);
		}

		//if we got a gamelist back - otherwise we have
		//got a description of the game back
		const config* const gamelist = data.child("gamelist");
		if(gamelist != NULL) {
			config game_data = data;
			const lobby::RESULT res = lobby::enter(disp,game_data);
			switch(res) {
				case lobby::QUIT: {
					return;
				}
				case lobby::CREATE: {
					play_multiplayer(disp,units_data,cfg,state,false);
					continue;
				}
				case lobby::JOIN: {
					break;
				}
			}
    
			for(;;) {
				data_res = network::receive_data(sides,0,5000);
				check_response(data_res,data);
    
				//if we have got valid side data
				if(sides.child("gamelist") == NULL) {
					break;
				}
			}
		} else {
			sides = data;
		}

		//ensure we send a close game message to the server when we are done
		network_game_manager game_manager;
    
		std::map<int,int> choice_map;
		std::vector<std::string> choices;
		choices.push_back(string_table["observer"]);
    
		std::vector<config*>& sides_list = sides.children["side"];
		for(std::vector<config*>::iterator s = sides_list.begin();
		    s != sides_list.end(); ++s) {
			if((*s)->values["controller"] == "network" &&
			   (*s)->values["taken"] != "yes") {
				choice_map[choices.size()] = 1 + s - sides_list.begin();
				choices.push_back((*s)->values["name"] + " - " +
				                  (*s)->values["type"]);
			}
		}
    
		const int choice = gui::show_dialog(disp,NULL,"","Choose side:",
		                                    gui::OK_CANCEL,&choices);
		if(choice < 0) {
			continue;
		}
    
		const int team_num = choice_map[choice];
		const bool observer = choice == 0;
    
		//send our choice of team to the server
		if(!observer) {

			config response;
			std::stringstream stream;
			stream << team_num;
			response["side"] = stream.str();
			response["description"] = preferences::login();

			const std::vector<config*>& possible_sides =
			                            cfg.children["multiplayer_side"];
			if(possible_sides.empty()) {
				std::cerr << "no multiplayer sides found\n";
				return;
			}

			std::vector<std::string> choices;
			for(std::vector<config*>::const_iterator side =
			    possible_sides.begin(); side != possible_sides.end(); ++side) {
				choices.push_back((**side)["name"]);
			}

			const size_t choice = size_t(gui::show_dialog(disp,NULL,"",
			     string_table["client_choose_side"],gui::OK_ONLY,&choices));
			assert(choice < possible_sides.size());

			const config& chosen_side = *possible_sides[choice];
			response["name"] = chosen_side["name"];
			response["type"] = chosen_side["type"];
			response["recruit"] = chosen_side["recruit"];
			response["music"] = chosen_side["music"];

			network::send_data(response);
		}
    
		wait_for_start waiter(sides);
		const int dialog_res = gui::show_dialog(disp,NULL,"",
		                        string_table["waiting_start"],
		                        gui::CANCEL_ONLY,NULL,NULL,"",NULL,&waiter);
		if(dialog_res == wait_for_start::GAME_CANCELLED) {
			gui::show_dialog(disp,NULL,"",string_table["game_cancelled"],
			                 gui::OK_ONLY);
			continue;
		} else if(dialog_res != wait_for_start::START_GAME) {
			continue;
		}
    
		if(!observer && !waiter.got_side) {
			throw network::error("Choice of team unavailable.");
		}
    
		//we want to make the network/human players look right from our
		//perspective
		{
			std::vector<config*>& sides_list = sides.children["side"];
			for(std::vector<config*>::iterator side = sides_list.begin();
			    side != sides_list.end(); ++side) {
				string_map& values = (*side)->values;
				if(team_num-1 == side - sides_list.begin())
					values["controller"] = "human";
				else
					values["controller"] = "network";
			}
		}
    
		//any replay data is only temporary and should be removed from
		//the level data in case we want to save the game later
		config* const replay_data = sides.child("replay");
		config replay_data_store;
		if(replay_data != NULL) {
			replay_data_store = *replay_data;
			std::cerr << "setting replay\n";
			recorder = replay(replay_data_store);
			if(!recorder.empty()) {
				const int res = gui::show_dialog(disp,NULL,
		               "", string_table["replay_game_message"],
					   gui::YES_NO);
				//if yes, then show the replay, otherwise
				//skip showing the replay
				if(res == 0) {
					recorder.set_skip(0);
				} else {
					std::cerr << "skipping...\n";
					recorder.set_skip(-1);
				}
			}
    
			sides.children["replay"].clear();
		}
    
		std::cerr << "starting game\n";
    
		state.starting_pos = sides;
    
		recorder.set_save_info(state);
    
		std::vector<config*> story;
		play_level(units_data,cfg,&sides,disp.video(),state,story);
		recorder.clear();
	}
}
