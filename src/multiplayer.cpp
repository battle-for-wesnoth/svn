/* $Id$ */
/*
   Copyright (C) 2007 - 2008
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "construct_dialog.hpp"
#include "dialogs.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "gui/dialogs/mp_connect.hpp"
#include "gui/dialogs/mp_create_game.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "md5.hpp"
#include "multiplayer.hpp"
#include "multiplayer_ui.hpp"
#include "multiplayer_connect.hpp"
#include "multiplayer_wait.hpp"
#include "multiplayer_lobby.hpp"
#include "multiplayer_create.hpp"
#include "playmp_controller.hpp"
#include "network.hpp"
#include "playcampaign.hpp"
#include "game_preferences.hpp"
#include "preferences_display.hpp"
#include "random.hpp"
#include "replay.hpp"
#include "video.hpp"
#include "statistics.hpp"
#include "serialization/string_utils.hpp"
#include "upload_log.hpp"
#include "wml_separators.hpp"

#define LOG_NW LOG_STREAM(info, network)

namespace {

class network_game_manager
{
public:
	// Add a constructor to avoid stupid warnings with some versions of GCC
	network_game_manager() {};

	~network_game_manager()
	{
		if(network::nconnections() > 0) {
			LOG_NW << "sending leave_game\n";
			config cfg;
			cfg.add_child("leave_game");
			network::send_data(cfg, 0, true);
			LOG_NW << "sent leave_game\n";
		}
	};
};

}

static void run_lobby_loop(display& disp, mp::ui& ui)
{
	disp.video().modeChanged();
	bool first = true;
	font::cache_mode(font::CACHE_LOBBY);
	while (ui.get_result() == mp::ui::CONTINUE) {
		if (disp.video().modeChanged() || first) {
			SDL_Rect lobby_pos = { 0, 0, disp.video().getx(), disp.video().gety() };
			ui.set_location(lobby_pos);
			first = false;
		}

		events::pump();
		events::raise_process_event();
		events::raise_draw_event();

		ui.process_network();

		disp.flip();
		disp.delay(20);
	}
	font::cache_mode(font::CACHE_GAME);
}

namespace {

enum server_type {
	ABORT_SERVER,
	WESNOTHD_SERVER,
	SIMPLE_SERVER
};

}

static server_type open_connection(game_display& disp, const std::string& original_host)
{
	std::string h = original_host;

	if(h.empty()) {
		gui2::tmp_connect dlg;

		dlg.show(disp.video());
		if(dlg.get_retval() == gui2::twindow::OK) {
			h = preferences::network_host();
		} else {
			return ABORT_SERVER;
		}
	}

	network::connection sock;

	const int pos = h.find_first_of(":");
	std::string host;
	unsigned int port;

	if(pos == -1) {
		host = h;
		port = 15000;
	} else {
		host = h.substr(0, pos);
		port = lexical_cast_default<unsigned int>(h.substr(pos + 1), 15000);
	}

	// shown_hosts is used to prevent the client being locked in a redirect
	// loop.
	typedef std::pair<std::string, int> hostpair;
	std::set<hostpair> shown_hosts;
	shown_hosts.insert(hostpair(host, port));

	config::child_list redirects;
	config data;
	sock = dialogs::network_connect_dialog(disp,_("Connecting to Server..."),host,port);

	do {

		if (!sock) {
			return ABORT_SERVER;
		}

		data.clear();
		network::connection data_res = dialogs::network_receive_dialog(
				disp,_("Reading from Server..."),data);
		if (!data_res) return ABORT_SERVER;
		mp::check_response(data_res, data);

		// Backwards-compatibility "version" attribute
		const std::string& version = data["version"];
		if(version.empty() == false && version != game_config::version) {
			utils::string_map i18n_symbols;
			i18n_symbols["version1"] = version;
			i18n_symbols["version2"] = game_config::version;
			const std::string errorstring = vgettext("The server requires version '$version1' while you are using version '$version2'", i18n_symbols);
			throw network::error(errorstring);
		}

		// Check for "redirect" messages
		if(data.child("redirect")) {
			config* redirect = data.child("redirect");

			host = (*redirect)["host"];
			port = lexical_cast_default<unsigned int>((*redirect)["port"], 15000);

			if(shown_hosts.find(hostpair(host,port)) != shown_hosts.end()) {
				throw network::error(_("Server-side redirect loop"));
			}
			shown_hosts.insert(hostpair(host, port));

			if(network::nconnections() > 0)
				network::disconnect();
			sock = dialogs::network_connect_dialog(disp,_("Connecting to Server..."),host,port);
			continue;
		}

		if(data.child("version")) {
			config cfg;
			config res;
			cfg["version"] = game_config::version;
			res.add_child("version", cfg);
			network::send_data(res, 0, true);
		}

		//if we got a direction to login
		if(data.child("mustlogin")) {
			bool first_time = true;
			config* error = NULL;

			std::vector<std::string> opts;
			opts.push_back(_("Log in with password"));
			opts.push_back(_("Request password reminder for this username"));
			opts.push_back(_("Choose a different username"));

			do {
				if(error != NULL) {
					gui::dialog(disp,"",(*error)["message"],gui::OK_ONLY).show();
				}

				std::string login = preferences::login();
				std::string password = "";
				std::string password_reminder = "";

				if(!first_time) {

					//Somewhat hacky implementation, including a goto of death

					/** @todo A fancy textbox that displays characters as dots or asterisks would nice. */
					if(!((*error)["password_request"].empty())) {
						const int res = gui::show_dialog(disp, NULL, _("Login"),
								(*error)["message"], gui::OK_CANCEL,
								&opts, NULL, _("Password: "), &password, mp::max_login_size);

						switch(res) {
							//Log in with password
							case 0:
								break;
							//Request a password reminder
							case 1:
								password_reminder = "yes";
								break;
							//Choose a different username
							case 2:
								password = "";
								goto new_username;
								break;
							default: return ABORT_SERVER;
						}

					} else {
						new_username:

						const int res = gui::show_dialog(disp, NULL, "",
								_("You must log in to this server"), gui::OK_CANCEL,
								NULL, NULL, _("Login: "), &login, mp::max_login_size);
						if(res != 0 || login.empty()) {
							return ABORT_SERVER;
						}
						preferences::set_login(login);
					}
				}

				first_time = false;

				config response ;
				config &sp = response.add_child("login") ;
				sp["username"] = login ;
				sp["password_reminder"] = password_reminder;

				// If password is not empty start hashing

				std::string result;
				if(!(password.empty())) {

					// Check if we have everything we need
					if((*error)["salt"].empty() || (*error)["hash_seed"].empty()) {
						return ABORT_SERVER;
					}

					std::string itoa64("./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

					std::string salt = (*error)["salt"];
					int hash_seed;
					try {
						hash_seed = lexical_cast_default<int>((*error)["hash_seed"]);
					} catch (bad_lexical_cast) {
						std::cerr << "Bad lexical cast reading hash_seed\n";
						return ABORT_SERVER;
					}

					// Start the MD5 hashing
					salt.append(password);
					MD5 md5_worker;
					md5_worker.update((unsigned char *)salt.c_str(),salt.length());
					md5_worker.finalize();
					unsigned char * output = (unsigned char *) malloc (sizeof(unsigned char) * 16);
					output = md5_worker.raw_digest();
					std::string temp_hash;
					do {
						temp_hash = std::string((char *) output, (char *) output + 16);
						temp_hash.append(password);	  								
						md5_worker.~MD5();
						MD5 md5_worker;
						md5_worker.update((unsigned char *)temp_hash.c_str(),temp_hash.length());
						md5_worker.finalize();
						output =  md5_worker.raw_digest();
					} while (--hash_seed);
					temp_hash = std::string((char *) output, (char *) output + 16);

					// Now encode the resulting mix
					std::string encoded_hash; 
					unsigned int i = 0, value;
					do {
						value = output[i++];
						encoded_hash.append(itoa64.substr(value & 0x3f,1));
						if(i < 16)
							value |= (int)output[i] << 8;
						encoded_hash.append(itoa64.substr((value >> 6) & 0x3f,1));
						if(i++ >= 16)
							break;
						if(i < 16)
							value |= (int)output[i] << 16;
						encoded_hash.append(itoa64.substr((value >> 12) & 0x3f,1));
						if(i++ >= 16)
							break;
						encoded_hash.append(itoa64.substr((value >> 18) & 0x3f,1));
					} while (i < 16);
					free (output);

					// Now mix the resulting hash with the random seed
					result = encoded_hash + (*error)["random_salt"];

					MD5 md5_worker2;	
					md5_worker2.update((unsigned char *)result.c_str(), result.size());
					md5_worker2.finalize();

					result = std::string(md5_worker2.hex_digest());
				}

				sp["password"] = result;

				// Login and enable selective pings -- saves server bandwidth
				// If ping_timeout has a non-zero value, do not enable
				// selective pings as this will cause clients to falsely
				// believe the server has died and disconnect.
				if( preferences::get_ping_timeout() ) {
				  // Pings required so disable selective pings
				  sp["selective_ping"] = "0" ;
				} else {
				  // Client is bandwidth friendly so allow
				  // server to optimize ping frequency as needed.
				  sp["selective_ping"] = "1" ;
				}
				network::send_data(response, 0, true);

				network::connection data_res = network::receive_data(data, 0, 3000);
				if(!data_res) {
					throw network::error(_("Connection timed out"));
				}

				error = data.child("error");
			} while(error != NULL);
		}
	} while(!(data.child("join_lobby") || data.child("join_game")));

	if (h != preferences::server_list().front().address)
		preferences::set_network_host(h);

	if (data.child("join_lobby")) {
		return WESNOTHD_SERVER;
	} else {
		return SIMPLE_SERVER;
	}

}


// The multiplayer logic consists in 4 screens:
//
// lobby <-> create <-> connect <-> (game)
//       <------------> wait    <-> (game)
//
// To each of this screen corresponds a dialog, each dialog being defined in
// the multiplayer_(screen) file.
//
// The functions enter_(screen)_mode are simple functions that take care of
// creating the dialogs, then, according to the dialog result, of calling other
// of those screen functions.

static void enter_wait_mode(game_display& disp, const config& game_config, mp::chat& chat, config& gamelist, bool observe)
{
	mp::ui::result res;
	game_state state;
	network_game_manager m;
	upload_log nolog(false);

	gamelist.clear();
	statistics::fresh_stats();

	{
		mp::wait ui(disp, game_config, chat, gamelist);

		ui.join_game(observe);

		run_lobby_loop(disp, ui);
		res = ui.get_result();

		if (res == mp::ui::PLAY) {
			ui.start_game();
			// FIXME commented a pointeless if since the else does exactly the same thing
			//if (preferences::skip_mp_replay()){
				//FIXME implement true skip replay
				//state = ui.request_snapshot();
				//state = ui.get_state();
			//}
			//else{
				state = ui.get_state();
			//}
		}
	}

	switch (res) {
	case mp::ui::PLAY:
		play_game(disp, state, game_config, nolog, IO_CLIENT,
			preferences::skip_mp_replay() && observe);
		recorder.clear();

		break;
	case mp::ui::QUIT:
	default:
		break;
	}
}

static void enter_create_mode(game_display& disp, const config& game_config, mp::chat& chat, config& gamelist, mp::controller default_controller, bool is_server);

static void enter_connect_mode(game_display& disp, const config& game_config,
		mp::chat& chat, config& gamelist, const mp::create::parameters& params,
		mp::controller default_controller, bool is_server)
{
	mp::ui::result res;
	game_state state;
	const network::manager net_manager(1,1);
	const network::server_manager serv_manager(15000, is_server ?
			network::server_manager::TRY_CREATE_SERVER :
			network::server_manager::NO_SERVER);
	network_game_manager m;
	upload_log nolog(false);

	gamelist.clear();
	statistics::fresh_stats();

	{
		mp::connect ui(disp, game_config, chat, gamelist, params, default_controller);
		run_lobby_loop(disp, ui);

		res = ui.get_result();

		// start_game() updates the parameters to reflect game start,
		// so it must be called before get_level()
		if (res == mp::ui::PLAY) {
			ui.start_game();
			state = ui.get_state();
		}
	}

	switch (res) {
	case mp::ui::PLAY:
		play_game(disp, state, game_config, nolog, IO_SERVER);
		recorder.clear();

		break;
	case mp::ui::CREATE:
		enter_create_mode(disp, game_config, chat, gamelist, default_controller, is_server);
		break;
	case mp::ui::QUIT:
	default:
		network::send_data(config("refresh_lobby"), 0, true);
		break;
	}
}

static void enter_create_mode(game_display& disp, const config& game_config, mp::chat& chat, config& gamelist, mp::controller default_controller, bool is_server)
{
	if(gui2::new_widgets) {

		gui2::tmp_create_game dlg(game_config);

		dlg.show(disp.video());

		network::send_data(config("refresh_lobby"), 0, true);
	} else {

		mp::ui::result res;
		mp::create::parameters params;

		{
			mp::create ui(disp, game_config, chat, gamelist);
			run_lobby_loop(disp, ui);
			res = ui.get_result();
			params = ui.get_parameters();
		}

		switch (res) {
		case mp::ui::CREATE:
			enter_connect_mode(disp, game_config, chat, gamelist, params, default_controller, is_server);
			break;
		case mp::ui::QUIT:
		default:
			//update lobby content
			network::send_data(config("refresh_lobby"), 0, true);
			break;
		}
	}
}

static void enter_lobby_mode(game_display& disp, const config& game_config, mp::chat& chat, config& gamelist)
{
	mp::ui::result res;

	while (true) {
		{
			mp::lobby ui(disp, game_config, chat, gamelist);
			run_lobby_loop(disp, ui);
			res = ui.get_result();
		}

		switch (res) {
		case mp::ui::JOIN:
			try {
				enter_wait_mode(disp, game_config, chat, gamelist, false);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui::show_error_message(disp, error.message);
				}
				//update lobby content
				network::send_data(config("refresh_lobby"), 0, true);
			}
			break;
		case mp::ui::OBSERVE:
			try {
				enter_wait_mode(disp, game_config, chat, gamelist, true);
			} catch(config::error& error) {
				if(!error.message.empty()) {
					gui::show_error_message(disp, error.message);
				}
			}
			//update lobby content
			network::send_data(config("refresh_lobby"), 0, true);
			break;
		case mp::ui::CREATE:
			try {
				enter_create_mode(disp, game_config, chat, gamelist, mp::CNTR_NETWORK, false);
			} catch(config::error& error) {
				if (!error.message.empty())
					gui::show_error_message(disp, error.message);
				//update lobby content
				network::send_data(config("refresh_lobby"), 0, true);
			}
			break;
		case mp::ui::QUIT:
			return;
		case mp::ui::PREFERENCES:
			{
				const preferences::display_manager disp_manager(&disp);
				preferences::show_preferences_dialog(disp,game_config);
				//update lobby content
				network::send_data(config("refresh_lobby"), 0, true);
			}
			break;
		default:
			return;
		}
	}
}

namespace mp {

void start_server(game_display& disp, const config& game_config,
		mp::controller default_controller, bool is_server)
{
        const rand_rng::set_random_generator generator_setter(&recorder);
	mp::chat chat;
	config gamelist;
	playmp_controller::set_replay_last_turn(0);
	preferences::set_message_private(false);
	enter_create_mode(disp, game_config, chat, gamelist, default_controller, is_server);
}

void start_client(game_display& disp, const config& game_config,
		const std::string host)
{
	const rand_rng::set_random_generator generator_setter(&recorder);
	const network::manager net_manager(1,1);

	mp::chat chat;
	config gamelist;
	server_type type = open_connection(disp, host);

	switch(type) {
	case WESNOTHD_SERVER:
		enter_lobby_mode(disp, game_config, chat, gamelist);
		break;
	case SIMPLE_SERVER:
		playmp_controller::set_replay_last_turn(0);
		preferences::set_message_private(false);
		enter_wait_mode(disp, game_config, chat, gamelist, false);
		break;
	case ABORT_SERVER:
		break;
	}
}

}

