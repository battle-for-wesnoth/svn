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

//! @file server/server.cpp
//! Wesnoth-Server, for multiplayer-games.

#include "../global.hpp"

#include "../config.hpp"
#include "../game_config.hpp"
#include "../log.hpp"
#include "../map.hpp" // gamemap::MAX_PLAYERS
#include "../network.hpp"
#include "../filesystem.hpp"
#include "../time.hpp"
#include "../serialization/parser.hpp"
#include "../serialization/preprocessor.hpp"
#include "../serialization/string_utils.hpp"

#include "game.hpp"
#include "input_stream.hpp"
#include "metrics.hpp"
#include "player.hpp"
#include "proxy.hpp"
#include "simple_wml.hpp"
#include "ban.hpp"

#include "user_handler.hpp"
// For now sample_user_handler is broken due
// to the hardcoding of the phpbb hashing algorithms
// #include "sample_user_handler.hpp"

#ifdef HAVE_MYSQLPP
#include "forum_user_handler.hpp"
#endif

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <queue>

#include <csignal>

#ifndef _WIN32
#include <sys/times.h>


namespace {

clock_t get_cpu_time(bool active) {
	if(!active) {
		return 0;
	}
	struct tms buf;
	times(&buf);
	return buf.tms_utime + buf.tms_stime;
}

}

#else

// on Windows we don't calculate CPU time
clock_t get_cpu_time(bool active) {
	return 0;
}

#endif

//! fatal and directly server related errors/warnings,
//! ie not caused by erroneous client data
#define ERR_SERVER LOG_STREAM(err, mp_server)
//! clients send wrong/unexpected data
#define WRN_SERVER LOG_STREAM(warn, mp_server)
//! normal events
#define LOG_SERVER LOG_STREAM(info, mp_server)
#define DBG_SERVER LOG_STREAM(debug, mp_server)
#define ERR_CONFIG LOG_STREAM(err, config)
#define WRN_CONFIG LOG_STREAM(warn, config)

//compatibility code for MS compilers
#ifndef SIGHUP
#define SIGHUP 20
#endif
// FIXME: should define SIGINT here too, but to what?

sig_atomic_t config_reload = 0;

static void reload_config(int signal) {
	assert(signal == SIGHUP);
	config_reload = 1;
}

static void exit_sigint(int signal) {
	assert(signal == SIGINT);
	LOG_SERVER << "SIGINT caught, exiting without cleanup immediately.\n";
	exit(1);
}

static void exit_sigterm(int signal) {
	assert(signal == SIGTERM);
	LOG_SERVER << "SIGTERM caught, exiting without cleanup immediately.\n";
	exit(1);
}

namespace {

// we take profiling info on every n requests
int request_sample_frequency = 1;

void send_doc(simple_wml::document& doc, network::connection connection, std::string type = "")
{
	if (type.empty())
		type = doc.root().first_child().to_string();
	simple_wml::string_span s = doc.output_compressed();
	network::send_raw_data(s.begin(), s.size(), connection, type);
}

void make_add_diff(const simple_wml::node& src, const char* gamelist,
                   const char* type,
                   simple_wml::document& out, int index=-1)
{
	if(out.root().child("gamelist_diff") == NULL) {
		out.root().add_child("gamelist_diff");
	}

	simple_wml::node* top = out.root().child("gamelist_diff");
	if(gamelist) {
		top = &top->add_child("change_child");
		top->set_attr_int("index", 0);
		top = &top->add_child("gamelist");
	}

	simple_wml::node& insert = top->add_child("insert_child");
	const simple_wml::node::child_list& children = src.children(type);
	assert(!children.empty());
	if(index < 0) {
		index = children.size() - 1;
	}

	assert(index < static_cast<int>(children.size()));
	insert.set_attr_int("index", index);
	children[index]->copy_into(insert.add_child(type));
}

bool make_delete_diff(const simple_wml::node& src,
                      const char* gamelist,
                      const char* type,
                      const simple_wml::node* remove,
					  simple_wml::document& out)
{
	if(out.root().child("gamelist_diff") == NULL) {
		out.root().add_child("gamelist_diff");
	}

	simple_wml::node* top = out.root().child("gamelist_diff");
	if(gamelist) {
		top = &top->add_child("change_child");
		top->set_attr_int("index", 0);
		top = &top->add_child("gamelist");
	}

	const simple_wml::node::child_list& children = src.children(type);
	const simple_wml::node::child_list::const_iterator itor =
	    std::find(children.begin(), children.end(), remove);
	if(itor == children.end()) {
		return false;
	}
	const int index = itor - children.begin();
	simple_wml::node& del = top->add_child("delete_child");
	del.set_attr_int("index", index);
	del.add_child(type);
	return true;
}

bool make_change_diff(const simple_wml::node& src,
                      const char* gamelist,
                      const char* type,
					  const simple_wml::node* item,
					  simple_wml::document& out)
{
	if(out.root().child("gamelist_diff") == NULL) {
		out.root().add_child("gamelist_diff");
	}

	simple_wml::node* top = out.root().child("gamelist_diff");
	if(gamelist) {
		top = &top->add_child("change_child");
		top->set_attr_int("index", 0);
		top = &top->add_child("gamelist");
	}
	const simple_wml::node::child_list& children = src.children(type);
	const simple_wml::node::child_list::const_iterator itor =
	    std::find(children.begin(), children.end(), item);
	if(itor == children.end()) {
		return false;
	}

	simple_wml::node& diff = *top;
	simple_wml::node& del = diff.add_child("delete_child");
	const int index = itor - children.begin();
	del.set_attr_int("index", index);
	del.add_child(type);

	//inserts will be processed first by the client, so insert at index+1,
	//and then when the delete is processed we'll slide into the right position
	simple_wml::node& insert = diff.add_child("insert_child");
	insert.set_attr_int("index", index+1);
	children[index]->copy_into(insert.add_child(type));
	return true;
}

}
class fps_limiter {
	size_t start_ticks_;
	size_t ms_per_frame_;
public:
	fps_limiter(size_t ms_per_frame = 20) : start_ticks_(0),ms_per_frame_(ms_per_frame)
	{}

	void limit() {
		size_t current_ticks = SDL_GetTicks();
		if (current_ticks - start_ticks_ < ms_per_frame_) {
			SDL_Delay(ms_per_frame_ - (current_ticks - start_ticks_));
			start_ticks_ += ms_per_frame_;
		} else {
			start_ticks_ = current_ticks;
		}
	}

	void set_ms_per_frame(size_t ms_per_frame)
	{
		ms_per_frame_ = ms_per_frame;
	}

	void set_fps(size_t fps)
	{
		ms_per_frame_ = 1000 / fps;
	}
};


class server
{
public:
	server(int port, input_stream& input, const std::string& config_file, size_t min_threads,size_t max_threads);
	void run();
private:
	void send_error(network::connection sock, const char* msg) const;
	void send_error_dup(network::connection sock, const std::string& msg) const;

	// The same as send_error(), we just add an extra child to the response
	// telling the client the chosen username requires a password.
	void send_password_request(network::connection sock, const char* msg, const std::string& user);

	const network::manager net_manager_;
	network::server_manager server_;
	wesnothd::ban_manager ban_manager_;

	user_handler* user_handler_;
	std::map<network::connection,std::string> seeds_;

	//! std::map<network::connection,player>
	wesnothd::player_map players_;
	wesnothd::player_map ghost_players_ ;

	std::vector<wesnothd::game*> games_;
	wesnothd::game not_logged_in_;
	//! The lobby is implemented as a game.
	wesnothd::game lobby_;

	//! server socket/fifo
	input_stream& input_;

	const std::string config_file_;
	config cfg_;
	//! Read the server config from file 'config_file_'.
	config read_config() const;
	
	// settings from the server config
	std::set<std::string> accepted_versions_;
	std::map<std::string,config> redirected_versions_;
	std::map<std::string,config> proxy_versions_;
	std::vector<std::string> disallowed_names_;
	std::string admin_passwd_;
	std::set<network::connection> admins_;
	std::string motd_;
	size_t default_max_messages_;
	size_t default_time_period_;
	size_t concurrent_connections_;
	bool graceful_restart;
	time_t lan_server_;
	time_t last_user_seen_time_;
	std::string restart_command;
	//! Parse the server config into local variables.
	void load_config();
	
	bool ip_exceeds_connection_limit(const std::string& ip) const;
	bool is_ip_banned(const std::string& ip) const;

	simple_wml::document version_query_response_;
	simple_wml::document login_response_;
	simple_wml::document join_lobby_response_;
	simple_wml::document games_and_users_list_;

	metrics metrics_;

	time_t last_ping_;
	time_t last_stats_;
	void dump_stats(const time_t& now);

	time_t last_uh_clean_;
	void clean_user_handler(const time_t& now);

	void process_data(const network::connection sock,
	                  simple_wml::document& data);
	void process_login(const network::connection sock,
	                   simple_wml::document& data);
	//! Handle queries from clients.
	void process_query(const network::connection sock,
	                   simple_wml::node& query);
	//! Process commands from admins and users.
	std::string process_command(const std::string& cmd, const std::string& issuer_name);
	//! Handle private messages between players.
	void process_whisper(const network::connection sock,
	                     simple_wml::node& whisper) const;
    //! Handle nickname registreation related requests from clients
	void process_nickserv(const network::connection sock,
	                   simple_wml::node& data);
	void process_data_lobby(const network::connection sock,
	                        simple_wml::document& data);
	void process_data_game(const network::connection sock,
	                       simple_wml::document& data);
	void delete_game(std::vector<wesnothd::game*>::iterator game_it);

	void update_game_in_lobby(const wesnothd::game* g, network::connection exclude=0);

	void start_new_server();
};

server::server(int port, input_stream& input, const std::string& config_file, 
		size_t min_threads,size_t max_threads) :
	net_manager_(min_threads,max_threads), 
	server_(port),
	ban_manager_(),
	user_handler_(NULL),
	seeds_(),
	players_(),
	ghost_players_(),
	games_(),
	not_logged_in_(players_),
	lobby_(players_), 
	input_(input), 
	config_file_(config_file),
	cfg_(read_config()), 
	accepted_versions_(),
	redirected_versions_(),
	proxy_versions_(),
	disallowed_names_(),
	admin_passwd_(),
	admins_(),
	motd_(),
	default_max_messages_(0),
	default_time_period_(0),
	concurrent_connections_(0),
	graceful_restart(false),
	lan_server_(time(NULL)),
	last_user_seen_time_(time(NULL)),
	restart_command(),
	version_query_response_("[version]\n[/version]\n", simple_wml::INIT_COMPRESSED),
	login_response_("[mustlogin]\n[/mustlogin]\n", simple_wml::INIT_COMPRESSED), 
	join_lobby_response_("[join_lobby]\n[/join_lobby]\n", simple_wml::INIT_COMPRESSED),
	games_and_users_list_("[gamelist]\n[/gamelist]\n", simple_wml::INIT_STATIC),
	metrics_(),
	last_ping_(time(NULL)), 
	last_stats_(last_ping_),
	last_uh_clean_(last_ping_)
{
	load_config();
	ban_manager_.read();
#ifndef _MSC_VER
	signal(SIGHUP, reload_config);
#endif
	signal(SIGINT, exit_sigint);
	signal(SIGTERM, exit_sigterm);

	// If there is a [user_handler] tag in the config file
	// allow nick registration, otherwise we set user_handler_
	// to NULL. Thus we must check user_handler_ for not being
	// NULL everytime we want to use it.

	if(cfg_.child("user_handler")) {
		// samples_user_handler is broken, see above
		/*if(cfg_["user_handler"] == "sample") {
			user_handler_ = new suh(*(cfg_.child("user_handler")));
		} else*/ if (cfg_["user_handler"] == "forum") {
			#ifdef HAVE_MYSQLPP
			user_handler_ = new fuh(*(cfg_.child("user_handler")));
			#endif
			#ifndef HAVE_MYSQLPP
			user_handler_ = NULL;
			#endif
		}

		// Initiate the mailer class with the [mail] tag
		// from the config file
		if(user_handler_) user_handler_->init_mailer(cfg_.child("mail"));

	} else {
		user_handler_ = NULL;
	}
}

void server::send_error(network::connection sock, const char* msg) const
{
	simple_wml::document doc;
	doc.root().add_child("error").set_attr("message", msg);
	simple_wml::string_span output = doc.output_compressed();
	network::send_raw_data(output.begin(), output.size(), sock, "error");
}

void server::send_error_dup(network::connection sock, const std::string& msg) const
{
	simple_wml::document doc;
	doc.root().add_child("error").set_attr_dup("message", msg.c_str());
	simple_wml::string_span output = doc.output_compressed();
	network::send_raw_data(output.begin(), output.size(), sock, "error");
}

void server::send_password_request(network::connection sock, const char* msg, const std::string& user)
{

	std::string salt1 = user_handler_->create_salt();
	std::string salt2 = user_handler_->create_pepper(user, 0);
	std::string salt3 = user_handler_->create_pepper(user, 1);

	seeds_.insert(std::pair<network::connection,std::string>(sock, salt1));

	simple_wml::document doc;
	doc.root().add_child("error").set_attr("message", msg);
	(*(doc.root().child("error"))).set_attr("password_request", "yes");
	(*(doc.root().child("error"))).set_attr("random_salt", salt1.c_str());
	(*(doc.root().child("error"))).set_attr("hash_seed", salt2.c_str());
	(*(doc.root().child("error"))).set_attr("salt", salt3.c_str());

	simple_wml::string_span output = doc.output_compressed();
	network::send_raw_data(output.begin(), output.size(), sock);
}

config server::read_config() const {
	config configuration;
	if (config_file_ == "") return configuration;
	scoped_istream stream = preprocess_file(config_file_);
	std::string errors;
	try {
		read(configuration, *stream, &errors);
		if (errors.empty()) {
			LOG_SERVER << "Server configuration from file: '" << config_file_
				<< "' read.\n";
		} else {
			ERR_CONFIG << "ERROR: Errors reading configuration file: '"
				<< errors << "'.\n";
		}
	} catch(config::error& e) {
		ERR_CONFIG << "ERROR: Could not read configuration file: '"
			<< config_file_ << "': '" << e.message << "'.\n";
	}
	return configuration;
}

void server::load_config() {
	admin_passwd_ = cfg_["passwd"];
	motd_ = cfg_["motd"];
	lan_server_ = lexical_cast_default<time_t>(cfg_["lan_server"], 0);

	disallowed_names_.clear();
	if (cfg_["disallow_names"] == "") {
		disallowed_names_.push_back("*admin*");
		disallowed_names_.push_back("*admln*");
		disallowed_names_.push_back("*server*");
		disallowed_names_.push_back("player");
		disallowed_names_.push_back("network");
		disallowed_names_.push_back("human");
		disallowed_names_.push_back("computer");
		disallowed_names_.push_back("ai");
		disallowed_names_.push_back("ai?");
	} else {
		disallowed_names_ = utils::split(cfg_["disallow_names"]);
	}
	default_max_messages_ =
			lexical_cast_default<size_t>(cfg_["max_messages"],4);
	default_time_period_ =
			lexical_cast_default<size_t>(cfg_["messages_time_period"],10);
	concurrent_connections_ =
			lexical_cast_default<size_t>(cfg_["connections_allowed"],5);
	// Example config line: 
	// restart_command="./wesnothd-debug -d -c ~/.wesnoth1.5/server.cfg"
	// remember to make new one as a daemon or it will block old one
	restart_command = cfg_["restart_command"];

	ntime::source::get_source().set_frame_time(lexical_cast_default<size_t>(cfg_["ms_per_frame"],20));

	accepted_versions_.clear();
	const std::string& versions = cfg_["versions_accepted"];
	if (versions.empty() == false) {
		const std::vector<std::string> accepted(utils::split(versions));
		for (std::vector<std::string>::const_iterator i = accepted.begin(); i != accepted.end(); ++i) {
			accepted_versions_.insert(*i);
		}
	} else {
		accepted_versions_.insert(game_config::version);
		accepted_versions_.insert("test");
	}

	redirected_versions_.clear();
	const config::child_list& redirects = cfg_.get_children("redirect");
	for (config::child_list::const_iterator i = redirects.begin(); i != redirects.end(); ++i) {
		const std::vector<std::string> versions(utils::split((**i)["version"]));
		for (std::vector<std::string>::const_iterator j = versions.begin(); j != versions.end(); ++j) {
			redirected_versions_[*j] = **i;
		}
	}

	proxy_versions_.clear();
	const config::child_list& proxies = cfg_.get_children("proxy");
	for (config::child_list::const_iterator p = proxies.begin(); p != proxies.end(); ++p) {
		const std::vector<std::string> versions(utils::split((**p)["version"]));
		for (std::vector<std::string>::const_iterator j = versions.begin(); j != versions.end(); ++j) {
			proxy_versions_[*j] = **p;
		}
	}
	ban_manager_.load_config(cfg_);
}

bool server::ip_exceeds_connection_limit(const std::string& ip) const {
	if (concurrent_connections_ == 0) return false;
	size_t connections = 0;
	for (wesnothd::player_map::const_iterator i = players_.begin(); i != players_.end(); ++i) {
		if (network::ip_address(i->first) == ip) {
			++connections;
		}
	}

	return connections >= concurrent_connections_;
}

bool server::is_ip_banned(const std::string& ip) const {
	return ban_manager_.is_ip_banned(ip);
}

void server::dump_stats(const time_t& now) {
	last_stats_ = now;
	LOG_SERVER << "Statistics:"
		<< "\tnumber_of_games = " << games_.size()
		<< "\tnumber_of_users = " << players_.size()
		 << "\tnumber_of_ghost_users = " << ghost_players_.size()
		<< "\tlobby_users = " << lobby_.nobservers() << "\n";
}

void server::clean_user_handler(const time_t& now) {
	if(!user_handler_) {
		return;
	}
	last_uh_clean_ = now;
	user_handler_->clean_up();
}

void server::run() {
	int graceful_counter = 0;

	for (int loop = 0;; ++loop) {
		// Try to run with 50 FPS all the time
		// Server will respond a bit faster under heavy load
		ntime::source::get_source().start_frame();
		try {
			// We are going to waith 10 seconds before shutting down so users can get out of game.
			if (graceful_restart && games_.size() == 0 && ++graceful_counter > 500 )
			{
				// TODO: We should implement client side autoreconnect.
				// Idea:
				// server should send [reconnect]host=host,port=number[/reconnect]
				// Then client would reconnect to new server automaticaly.
				// This would also allow server to move to new port or address if there is need

				process_command("msg All games ended. Shutting down now. Reconnect to the new server.", "system");
				throw network::error("shut down");
			}

			if (config_reload == 1) {
				cfg_ = read_config();
				load_config();
				config_reload = 0;
			}

			// Process commands from the server socket/fifo
			std::string admin_cmd;
			if (input_.read_line(admin_cmd)) {
				process_command(admin_cmd, "*socket*");
			}

			time_t now = time(NULL); 
			if (last_ping_ + 30 <= now) {
				if (lan_server_ && players_.empty() && last_user_seen_time_ + lan_server_ < now)
				{
					LOG_SERVER << "Lan server has been empty for  " << (now - last_user_seen_time_) << " seconds. Shutting down!\n";
					// We have to shutdown
					graceful_restart = true;
				}
				// and check if bans have expired
				ban_manager_.check_ban_times(now);
				// Make sure we log stats every 5 minutes
				if (last_stats_ + 5*60 <= now) {
					dump_stats(now);
				}

				// Cleaning the user_handler once a day should be more than enough
				if (last_uh_clean_ + 60 * 60 * 24 <= now) {
					clean_user_handler(now);
				}

				// Send network stats every hour
				static size_t prev_hour = localtime(&now)->tm_hour;
				if (prev_hour != localtime(&now)->tm_hour)
				{
					prev_hour = localtime(&now)->tm_hour;
					LOG_SERVER << network::get_bandwidth_stats();

				}

				// send a 'ping' to all players to detect ghosts
				DBG_SERVER << "Pinging inactive players.\n" ;
 				std::ostringstream strstr ;
 				strstr << "ping=\"" << lexical_cast<std::string>(now) << "\"" ;
 				simple_wml::document ping( strstr.str().c_str(),
 							   simple_wml::INIT_COMPRESSED ) ;
 				simple_wml::string_span s = ping.output_compressed() ;
 				for (wesnothd::player_map::const_iterator i = ghost_players_.begin();
 				     i != ghost_players_.end(); ++i)
 				  {
 				    DBG_SERVER << "Pinging " << i->second.name() << "(" << i->first << ").\n" ;
 				    network::send_raw_data( s.begin(), 
 							    s.size(),
 							    i->first, "ping" ) ;
 				  }

 				// Copy new player list on top of ghost_players_ list.
 				// Only a single thread should be accessing this
				{
 				  // Erase before we copy - speeds inserts
 				  ghost_players_.clear() ;
 				  std::copy( players_.begin(), players_.end(),
 					     std::inserter( ghost_players_, ghost_players_.begin() ) ) ;
				}
				last_ping_ = now;
			}

			network::process_send_queue();

			network::connection sock = network::accept_connection();
			if (sock) {
				const std::string& ip = network::ip_address(sock);
				if (is_ip_banned(ip)) {
					LOG_SERVER << ip << "\trejected banned user.\n";
					send_error(sock, "You are banned.");
					network::disconnect(sock);
				} else if (ip_exceeds_connection_limit(ip)) {
					LOG_SERVER << ip << "\trejected ip due to excessive connections\n";
					send_error(sock, "Too many connections from your IP.");
					network::disconnect(sock);
				} else {
					DBG_SERVER << ip << "\tnew connection accepted. (socket: "
						<< sock << ")\n";
					send_doc(version_query_response_, sock);
					not_logged_in_.add_player(sock, true);
				}
			}

			static int sample_counter = 0;

			std::vector<char> buf;
			network::bandwidth_in_ptr bandwidth_type;
			while ((sock = network::receive_data(buf, &bandwidth_type)) != network::null_connection) {
				metrics_.service_request();

				if(buf.empty()) {
					std::cerr << "received empty packet\n";
					continue;
				}

				//TODO: this is a HUGE HACK. There was a bug in Wesnoth 1.4
				//that caused it to still use binary WML for the leave game
				//message. (ugh). We will see if this looks like binary WML
				//and if it does, assume it's a leave_game message
				if(buf.front() < 5) {
				  std::cerr << "hit wesnoth bug...forcing disconnection incorrectly." << buf.front() << std::endl ;
					static simple_wml::document leave_game_doc(
					    "[leave_game]\n[/leave_game]\n",
						simple_wml::INIT_COMPRESSED);
					process_data(sock, leave_game_doc);
					continue;
				}

				const bool sample = request_sample_frequency >= 1 && (sample_counter++ % request_sample_frequency) == 0;

				const clock_t before_parsing = get_cpu_time(sample);

				char* buf_ptr = new char [buf.size()];
				memcpy(buf_ptr, &buf[0], buf.size());
				simple_wml::string_span compressed_buf(buf_ptr, buf.size());
				simple_wml::document data(compressed_buf);
				data.take_ownership_of_buffer(buf_ptr);
				std::vector<char>().swap(buf);

				const clock_t after_parsing = get_cpu_time(sample);

				process_data(sock, data);

				bandwidth_type->set_type(data.root().first_child().to_string());
				if(sample) {
					const clock_t after_processing = get_cpu_time(sample);
					metrics_.record_sample(data.root().first_child(),
					          after_parsing - before_parsing,
							  after_processing - after_parsing);
				}
			}

			metrics_.no_requests();

		} catch(config::error& e) {
			WRN_CONFIG << "Warning: error in received data: " << e.message << "\n";
		} catch(simple_wml::error& e) {
			WRN_CONFIG << "Warning: error in received data\n";
		} catch(network::error& e) {
			if (e.message == "shut down") {
				LOG_SERVER << "Try to disconnect all users...\n";
				for (wesnothd::player_map::const_iterator pl = players_.begin();
					pl != players_.end(); ++pl)
				{
					network::disconnect(pl->first);
				}
				LOG_SERVER << "Shutting server down.\n";
				break;
			}
			if (!e.socket) {
				ERR_SERVER << "network error: " << e.message << "\n";
				e.disconnect();
				continue;
			}
			DBG_SERVER << "socket closed: " << e.message << "\n";
			const std::string ip = network::ip_address(e.socket);
			if (proxy::is_proxy(e.socket)) {
				LOG_SERVER << ip << "\tProxy user disconnected.\n";
				proxy::disconnect(e.socket);
				e.disconnect();
				DBG_SERVER << "done closing socket...\n";
				continue;
			}
			// Was the user already logged in?
			const wesnothd::player_map::iterator pl_it = players_.find(e.socket);
			if (pl_it == players_.end()) {
				if (not_logged_in_.is_observer(e.socket)) {
					DBG_SERVER << ip << "\tNot logged in user disconnected.\n";
					not_logged_in_.remove_player(e.socket);
				} else {
					WRN_SERVER << ip << "\tWarning: User disconnected right after the connection was accepted.\n";
				}
				e.disconnect();
				DBG_SERVER << "done closing socket...\n";
				continue;
			}
			const simple_wml::node::child_list& users = games_and_users_list_.root().children("user");
			const size_t index = std::find(users.begin(), users.end(), pl_it->second.config_address()) - users.begin();
			if (index < users.size()) {
				simple_wml::document diff;
				if(make_delete_diff(games_and_users_list_.root(), NULL, "user",
				                    pl_it->second.config_address(), diff)) {
					lobby_.send_data(diff, e.socket);
				}

				games_and_users_list_.root().remove_child("user",index);
			} else {
				ERR_SERVER << ip << "ERROR: Could not find user to remove: "
					<< pl_it->second.name() << " in games_and_users_list_.\n";
			}
			// Was the player in the lobby or a game?
			if (lobby_.is_member(e.socket)) {
				lobby_.remove_player(e.socket);
				LOG_SERVER << ip << "\t" << pl_it->second.name()
					<< "\thas logged off. (socket: " << e.socket << ")\n";
				                 
			} else {
				for (std::vector<wesnothd::game*>::iterator g = games_.begin();
					g != games_.end(); ++g)
				{
					if (!(*g)->is_member(e.socket)) {
						continue;
					}
					// Did the last player leave?
					if ((*g)->remove_player(e.socket, true)) {
						delete_game(g);
						break;
					} else {
						(*g)->describe_slots();

						update_game_in_lobby(*g, e.socket);
					}
					break;
				}
			}
			players_.erase(pl_it);
			if (lan_server_)
			{
				last_user_seen_time_ = time(0);
			}
			e.disconnect();
			DBG_SERVER << "done closing socket...\n";
		}
	}
}

void server::process_data(const network::connection sock,
                          simple_wml::document& data) {
	if (proxy::is_proxy(sock)) {
		proxy::received_data(sock, data);
	}

	// We know the client is alive for this interval
	// Remove player from ghost_players map if selective_ping
	// is enabled for the player.
	const wesnothd::player_map::const_iterator pl = ghost_players_.find( sock ) ;
	if( pl != ghost_players_.end() && pl->second.selective_ping() ) {
	  ghost_players_.erase( sock ) ;
	}

	// Process the message
	simple_wml::node& root = data.root();
	if(root.has_attr("ping")) {
		// Ignore client side pings for now.
		return;
	} else if(not_logged_in_.is_observer(sock)) {
		// Someone who is not yet logged in is sending login details.
		process_login(sock, data);
	} else if (simple_wml::node* query = root.child("query")) {
		process_query(sock, *query);
	} else if (simple_wml::node* nickserv = root.child("nickserv")) {
		process_nickserv(sock, *nickserv);
    } else if (simple_wml::node* whisper = root.child("whisper")) {
		process_whisper(sock, *whisper);
	} else if (lobby_.is_observer(sock)) {
		process_data_lobby(sock, data);
	} else {
		process_data_game(sock, data);
	}
}



void server::process_login(const network::connection sock,
                           simple_wml::document& data) {

	// See if the client is sending their version number.
	if (const simple_wml::node* const version = data.child("version")) {
		const simple_wml::string_span& version_str_span = (*version)["version"];
		const std::string version_str(version_str_span.begin(),
		                              version_str_span.end());
		std::set<std::string>::const_iterator accepted_it;
		// Check if it is an accepted version.
		for (accepted_it = accepted_versions_.begin();
			accepted_it != accepted_versions_.end(); ++accepted_it) {
			if (utils::wildcard_string_match(version_str,*accepted_it)) break;
		}
		if (accepted_it != accepted_versions_.end()) {
			LOG_SERVER << network::ip_address(sock)
				<< "\tplayer joined using accepted version " << version_str
				<< ":\ttelling them to log in.\n";
			send_doc(login_response_, sock);
			return;
		}
		std::map<std::string,config>::const_iterator config_it;
		// Check if it is a redirected version
		for (config_it = redirected_versions_.begin();
			config_it != redirected_versions_.end(); ++config_it)
		{
			if (utils::wildcard_string_match(version_str,config_it->first))
				break;
		}
		if (config_it != redirected_versions_.end()) {
			LOG_SERVER << network::ip_address(sock)
				<< "\tplayer joined using version " << version_str
				<< ":\tredirecting them to " << config_it->second["host"]
				<< ":" << config_it->second["port"] << "\n";
			config response;
			response.add_child("redirect",config_it->second);
			network::send_data(response, sock, true, "redirect");
			return;
		}
		// Check if it's a version we should start a proxy for.
		for (config_it = proxy_versions_.begin();
			config_it != proxy_versions_.end(); ++config_it)
		{
			if (utils::wildcard_string_match(version_str,config_it->first))
				break;
		}
		if (config_it != proxy_versions_.end()) {
			LOG_SERVER << network::ip_address(sock)
				<< "\tplayer joined using version " << version_str
				<< ":\tconnecting them by proxy to " << config_it->second["host"]
				<< ":" << config_it->second["port"] << "\n";
			proxy::create_proxy(sock,config_it->second["host"],
				lexical_cast_default<int>(config_it->second["port"],15000));
			return;
		}
		// No match, send a response and reject them.
		LOG_SERVER << network::ip_address(sock)
			<< "\tplayer joined using unknown version " << version_str
			<< ":\trejecting them\n";
		config response;
		if (!accepted_versions_.empty()) {
			response["version"] = *accepted_versions_.begin();
		} else if (redirected_versions_.empty() == false) {
			response["version"] = redirected_versions_.begin()->first;
		} else {
			ERR_SERVER << "ERROR: This server doesn't accept any versions at all.\n";
			response["version"] = "null";
		}
		network::send_data(response, sock, true, "error");
		return;
	}

	const simple_wml::node* const login = data.child("login");
	// Client must send a login first.
	if (login == NULL) {
		send_error(sock, "You must login first.");
		return;
	}

	// Check if the username is valid (all alpha-numeric plus underscore and hyphen)
	std::string username = (*login)["username"].to_string();
	if (!utils::isvalid_username(username)) {
		send_error(sock, "This username contains invalid "
			"characters. Only alpha-numeric characters, underscores and hyphens"
			"are allowed.");
		return;
	}
	if (username.size() > 18) {
		send_error(sock, "This username is too long. Usernames must be 18 characers or less.");
		return;
	}
	// Check if the username is allowed.
	for (std::vector<std::string>::const_iterator d_it = disallowed_names_.begin();
		d_it != disallowed_names_.end(); ++d_it)
	{
		if (utils::wildcard_string_match(utils::lowercase(username),
			utils::lowercase(*d_it)))
		{
			send_error(sock, "The nick you chose is reserved and cannot be used by players");
			return;
		}
	}

	// If this is a request for password reminder
	if(user_handler_) {
		std::string password_reminder = (*login)["password_reminder"].to_string();
		if(password_reminder == "yes") {
			try {
				user_handler_->password_reminder(username);
				send_error(sock, "Your password reminder email has been sent.");
			} catch (user_handler::error e) {
				send_error(sock, ("There was an error sending your password reminder email. The error message was: " +
				e.message).c_str());
			}
			return;
		}
	}

	// Check the username isn't already taken
	wesnothd::player_map::const_iterator p;
	for (p = players_.begin(); p != players_.end(); ++p) {
		if (p->second.name() == username) {
			send_error(sock, "The username you chose is already taken.");
			return;
		}
	}

	// Check for password

	// Current login procedure  for registered nicks is:
	// - Client asks to log in with a particular nick
	// - Server sends client random salt plus some info
	// 	generated from the original hash that is required to
	// 	regenerate the hash
	// - Client generates hash for the user provided password
	// 	and mixes it with the received random salt
	// - Server received salted hash, salts the valid hash with
	// 	the same salt it sent to the client and compares the results

	bool registered = false;
	if(user_handler_) {
		std::string password = (*login)["password"].to_string();
		if(user_handler_->user_exists(username)) {
			// This name is registered and no password provided
			if(password.empty()) {
				send_password_request(sock, ("The username '" + username + "' is registered on this server.").c_str(), username);
				return;
			}
			// A password (or hashed password) was provided, however
			// there is no seed
			if(seeds_[sock].empty()) {
				send_password_request(sock, "Please try again.", username);
			}
			// This name is registered and an incorrect password provided
			else if(!(user_handler_->login(username, password, seeds_[sock]))) {
				// Reset the random seed
				seeds_.erase(sock);
				send_password_request(sock, ("The password you provided for the username '" + username +
						"' was incorrect.").c_str(), username);

				LOG_SERVER << network::ip_address(sock) << "\t"
						<< "Login attempt with incorrect password for username '" << username << "'.\n";
				return;
			}
		// This name exists and the password was neither empty nor incorrect
		registered = true;
		// Reset the random seed
		seeds_.erase(sock);
		user_handler_->user_logged_in(username);
		}
	}

	// Check if the version is now avaliable. If it is not, this player must
	// always be pinged.
	bool selective_ping = false ;
	if( (*login)["selective_ping"].to_bool() ) {
	  selective_ping = true ;
	  DBG_SERVER << "selective ping is ENABLED for " << sock << "\n" ;
	} else {
	  DBG_SERVER << "selective ping is DISABLED for  " << sock << "\n" ;
	}

	send_doc(join_lobby_response_, sock);

	simple_wml::node& player_cfg = games_and_users_list_.root().add_child("user");
	const player new_player(username, player_cfg, registered, default_max_messages_,
				default_time_period_, selective_ping );

	// If the new player does not have selective ping enabled, immediately
	// add the player to the ghost player's list. This ensures a client won't
	// have to wait as long as x2 the current ping delay; which could cause
	// a client-side disconnection.
	players_.insert(std::pair<network::connection,player>(sock, new_player));
	if( !selective_ping )
	  ghost_players_.insert( std::pair<network::connection,player>(sock, new_player) ) ;

	not_logged_in_.remove_player(sock);
	lobby_.add_player(sock, true);

	// Send the new player the entire list of games and players
	send_doc(games_and_users_list_, sock);

	if (motd_ != "") {
		lobby_.send_server_message(motd_.c_str(), sock);
	}

	// Send other players in the lobby the update that the player has joined
	simple_wml::document diff;
	make_add_diff(games_and_users_list_.root(), NULL, "user", diff);
	lobby_.send_data(diff, sock);

	LOG_SERVER << network::ip_address(sock) << "\t" << username
		<< "\thas logged on. (socket: " << sock << ")\n";

	for (std::vector<wesnothd::game*>::const_iterator g = games_.begin(); g != games_.end(); ++g) {
		// Note: This string is parsed by the client to identify lobby join messages!
		(*g)->send_server_message_to_all((username + " has logged into the lobby").c_str());
	}
}

void server::process_query(const network::connection sock,
                           simple_wml::node& query) {
	const wesnothd::player_map::const_iterator pl = players_.find(sock);
	if (pl == players_.end()) {
		DBG_SERVER << "ERROR: process_query(): Could not find player with socket: " << sock << "\n";
		return;
	}
	const simple_wml::string_span& command(query["type"]);
	std::ostringstream response;
	const std::string& help_msg = "Available commands are: help, metrics,"
			" motd, netstats [all], status, wml.";
	if (admins_.count(sock) != 0) {
		LOG_SERVER << "Admin Command:" << "\ttype: " << command
			<< "\tIP: "<< network::ip_address(sock) 
			<< "\tnick: "<< pl->second.name() << std::endl;
		response << process_command(command.to_string(), pl->second.name());
	// Commands a player may issue.
	} else if (command == "help") {
		response << help_msg;
	} else if (command == "status") {
		response << process_command(command.to_string() + " " + pl->second.name(), pl->second.name());
	} else if (command == "status " + pl->second.name() || command == "metrics"
	|| command == "motd" || command == "wml" || command == "netstats" || command == "netstats all") {
		response << process_command(command.to_string(), pl->second.name());
	} else if (command == admin_passwd_) {
		LOG_SERVER << "New Admin recognized:" << "\tIP: "
			<< network::ip_address(sock) << "\tnick: "
			<< pl->second.name() << std::endl;
		admins_.insert(sock);
		response << "You are now recognized as an administrator.";
	} else if (admin_passwd_.empty() == false) {
		WRN_SERVER << "FAILED Admin attempt: '" << command << "'\tIP: "
			<< network::ip_address(sock) << "\tnick: "
			<< pl->second.name() << std::endl;
		response << "Error: unrecognized query: '" << command << "'\n" << help_msg;
	} else {
		response << "Error: unrecognized query: '" << command << "'\n" << help_msg;
	}
	lobby_.send_server_message(response.str().c_str(), sock);
}

void server::start_new_server() {
	if (restart_command.empty())
		return;

	// Example config line: 
	// restart_command="./wesnothd-debug -d -c ~/.wesnoth1.5/server.cfg"
	// remember to make new one as a daemon or it will block old one
	std::system(restart_command.c_str());

	LOG_SERVER << "New server started with command: " << restart_command << "\n";
}

std::string server::process_command(const std::string& query, const std::string& issuer_name) {
	std::ostringstream out;
	const std::string::const_iterator i = std::find(query.begin(),query.end(),' ');
	const std::string command(query.begin(),i);
	std::string parameters = (i == query.end() ? "" : std::string(i+1,query.end()));
	utils::strip(parameters);
	const std::string& help_msg = "Available commands are: ban(s) [<mask>] [<time>] <reason>,"
			"kick <mask>, k(ick)ban [<mask>] [<time>] <reason>, help, metrics, netstats,"
			" (lobby)msg <message>, motd [<message>], status [<mask>],"
			" unban <ipmask>";
	// Shutdown and restart commands can only be issued via the socket.
	if (command == "shut_down" && issuer_name == "*socket*") {
		if (parameters == "now") {
			throw network::error("shut down");
		} else {
			// Graceful shut down.
			server_.stop();
			input_.stop();
			graceful_restart = true;
			process_command("msg The server is shutting down. You may finish your games but can't start new ones. Once all games have ended the server will exit.", issuer_name);
			out << "Server is doing graceful shut down.";
		}

#ifndef _WIN32  // Not sure if this works on windows
		// TODO: check if this works in windows.
	} else if (command == "restart" && issuer_name == "*socket*") {
		if (restart_command.empty()) {
			out << "No restart_command configured! Not restarting.";
		} else {
			LOG_SERVER << "Graceful restart requested.";
			graceful_restart = true;
			// stop listening socket
			server_.stop();
			input_.stop();
			// start new server
			start_new_server();
			process_command("msg The server has been restarted. You may finish your games but can't start new ones and new players can't join this server.", issuer_name);
			out << "New server started.";
		}
#endif
	} else if (command == "help") {
		out << help_msg;
	} else if (command == "metrics") {
		out << metrics_ << "Current number of games = " << games_.size() << "\n"
		  "Total number of users = " << players_.size() << "\n"
		  "Total number of ghost users = " << ghost_players_.size() << "\n"
		  "Number of users in the lobby = " << lobby_.nobservers() << "\n";
	} else if (command == "wml") {
		out << simple_wml::document::stats();
	} else if (command == "netstats") {
		network::pending_statistics stats = network::get_pending_stats();
		out << "Network stats:\nPending send buffers: "
		    << stats.npending_sends << "\nBytes in buffers: "
			<< stats.nbytes_pending_sends << "\n";
		if (parameters == "all")
			out << network::get_bandwidth_stats_all();
		else
			out << network::get_bandwidth_stats(); // stats from previuos hour
	} else if (command == "msg" || command == "lobbymsg") {
		if (parameters == "") {
			return "You must type a message.";
		}
		lobby_.send_server_message_to_all(parameters.c_str());
		if (command == "msg") {
			for (std::vector<wesnothd::game*>::const_iterator g = games_.begin(); g != games_.end(); ++g) {
				(*g)->send_server_message_to_all(parameters.c_str());
			}
		}
		LOG_SERVER << "<server> " + parameters + "\n";
		out << "message '" << parameters << "' relayed to players\n";
	} else if (command == "status") {
		out << "STATUS REPORT\n";
		for (wesnothd::player_map::const_iterator pl = players_.begin(); pl != players_.end(); ++pl) {
			if (parameters == ""
				|| utils::wildcard_string_match(pl->second.name(), parameters)
				|| utils::wildcard_string_match(network::ip_address(pl->first), parameters)) {
				const network::connection_stats& stats = network::get_connection_stats(pl->first);
				const int time_connected = stats.time_connected/1000;
				const int seconds = time_connected%60;
				const int minutes = (time_connected/60)%60;
				const int hours = time_connected/(60*60);
				out << "'" << pl->second.name() << "' @ " << network::ip_address(pl->first)
					<< " connected for " << hours << ":" << minutes << ":" << seconds
					<< " sent " << stats.bytes_sent << " bytes, received "
					<< stats.bytes_received << " bytes\n";
			}
		}
	} else if (command == "ban" || command == "bans" || command == "kban" || command == "kickban" || command == "gban") {
		if (parameters == "") {
			ban_manager_.list_bans(out);
		} else if (parameters == "deleted") {
			ban_manager_.list_deleted_bans(out);
		}else {
			bool banned_ = false;
			const bool kick = (command == "kban" || command == "kickban");
			const bool group_ban = command == "gban";
			std::string::iterator first_space = std::find(parameters.begin(), parameters.end(), ' ');
			if (first_space == parameters.end())
			{
				return ban_manager_.get_ban_help();
			}
			std::string::iterator second_space = std::find(first_space+1, parameters.end(), ' ');
			const std::string target(parameters.begin(), first_space);
			std::string group;
			if (group_ban)
			{
				group = std::string(first_space+1, second_space);
				first_space = second_space;
				second_space = std::find(first_space+1, parameters.end(), ' ');
			}
			const std::string time(first_space+1,second_space);
			time_t parsed_time = ban_manager_.parse_time(time);
			if (parsed_time == 0)
			{
				second_space = first_space;
			}

			if (second_space == parameters.end())
			{
				--second_space;
			}
			std::string reason(second_space + 1, parameters.end());
			utils::strip(reason);
			if (reason.empty())
				return ban_manager_.get_ban_help();

			// if we find a '.' consider it an ip mask 
			//! @todo  FIXME: should also check for only numbers
			if (std::count(target.begin(), target.end(), '.') >= 1) {
				banned_ = true;

				std::string err = ban_manager_.ban(target, parsed_time, reason, issuer_name, group);
				out << err;
	
				if (kick) {
					for (wesnothd::player_map::const_iterator pl = players_.begin();
						pl != players_.end(); ++pl)
					{
						if (utils::wildcard_string_match(network::ip_address(pl->first), target)) {
							out << "\nKicked " << pl->second.name() << ".";
							network::queue_disconnect(pl->first);
						}
					}
				}
			} else {
				for (wesnothd::player_map::const_iterator pl = players_.begin();
					pl != players_.end(); ++pl)
				{
					if (utils::wildcard_string_match(pl->second.name(), target)) {
						banned_ = true;
						const std::string& ip = network::ip_address(pl->first);
						if (!is_ip_banned(ip)) {
							std::string err = ban_manager_.ban(ip,parsed_time, reason, issuer_name, group);
							out << err;
						}
						if (kick) {
							out << "\nKicked " << pl->second.name() << ".";
							network::queue_disconnect(pl->first);
						}
					}
				}
				if (!banned_) {
					out << "Nickmask '" << target << "' did not match, no bans set.";
				}
			}
		}
	} else if (command == "unban") {
		if (parameters == "") {
			return "You must enter an ipmask to unban.";
		}
		ban_manager_.unban(out, parameters);
	} else if (command == "ungban") {
		if (parameters == "") {
			return "You must enter an ipmask to unban.";
		}
		ban_manager_.unban_group(out, parameters);
	} else if (command == "kick") {
		if (parameters == "") {
			return "You must enter a mask to kick.";
		}
		bool kicked = false;
		// if we find a '.' consider it an ip mask
		if (std::count(parameters.begin(), parameters.end(), '.') >= 1) {
			for (wesnothd::player_map::const_iterator pl = players_.begin();
				pl != players_.end(); ++pl)
			{
				if (utils::wildcard_string_match(network::ip_address(pl->first), parameters)) {
					kicked = true;
					out << "Kicked " << pl->second.name() << ".\n";
					network::queue_disconnect(pl->first);
				}
			}
		} else {
			for (wesnothd::player_map::const_iterator pl = players_.begin();
				pl != players_.end(); ++pl)
			{
				if (utils::wildcard_string_match(pl->second.name(), parameters)) {
					kicked = true;
					out << "Kicked " << pl->second.name() << " ("
						<< network::ip_address(pl->first) << ").\n";
					network::queue_disconnect(pl->first);
				}
			}
		}
		if (!kicked) out << "No user matched '" << parameters << "'.\n";
	} else if(command == "sample") {
		request_sample_frequency = atoi(parameters.c_str());
		if(request_sample_frequency <= 0) {
			out << "Sampling turned off";
		} else {
			out << "Sampling every " << request_sample_frequency << " requests\n";
		}
	} else if (command == "motd") {
		if (parameters == "") {
			if (motd_ != "") {
				out << "Message of the day: " << motd_;
				return out.str();
			} else {
				return "No message of the day set.";
			}
		}
		motd_ = parameters;
		out << "Message of the day set to: " << motd_;
	} else {
		out << "Command '" << command << "' is not recognized.\n" << help_msg;
	}

	LOG_SERVER << out.str() << "\n";
	return out.str();
}

void server::process_nickserv(const network::connection sock, simple_wml::node& data) {
	const wesnothd::player_map::iterator pl = players_.find(sock);
	if (pl == players_.end()) {
		DBG_SERVER << "ERROR: Could not find player with socket: " << sock << "\n";
		return;
	}

	// Check if this server allows nick registration at all
	if(!user_handler_) {
		lobby_.send_server_message("This server does not allow to register on it.", sock);
		return;
	}

	if(data.child("register")) {
		try {
			(user_handler_->add_user(pl->second.name(), (*data.child("register"))["mail"].to_string(),
				(*data.child("register"))["password"].to_string()));

			std::stringstream msg;
			msg << "Your username has been registered." <<
					// Warn that providing an email address might be a good idea
					((*data.child("register"))["mail"].empty() ?
					" It is recommended that you provide an email address for password recovery." : "");
			lobby_.send_server_message(msg.str().c_str(), sock);

			// Mark the player as registered and send the other clients
			// an update to dislpay this change
			pl->second.mark_registered();

			simple_wml::document diff;
			make_change_diff(games_and_users_list_.root(), NULL,
						 "user", pl->second.config_address(), diff);
			lobby_.send_data(diff);

		} catch (user_handler::error e) {
			lobby_.send_server_message(("There was and error registering your username. The error message was: "
			+ e.message).c_str(), sock);
		}
		return;
	}

	// A user requested to update his password or mail
	if(data.child("set")) {
		if(!(user_handler_->user_exists(pl->second.name()))) {
			lobby_.send_server_message("You are not registered. Please register first.",
					sock);
			return;
		}

		const simple_wml::node& set = *(data.child("set"));

		try {
			user_handler_->set_user_detail(pl->second.name(), set["detail"].to_string(), set["value"].to_string());

			lobby_.send_server_message("Your details have been updated.", sock);

		} catch (user_handler::error e) {
			lobby_.send_server_message(("There was and error updating your details. The error message was: "
			+ e.message).c_str(), sock);
		}

		return;
	}

	// A user requested information about another user
	if(data.child("details")) {
		lobby_.send_server_message(("Valid details for this server are: " +
				user_handler_->get_valid_details()).c_str(), sock);
		return;
	}

	// A user requested a list of which details can be set
	if(data.child("info")) {
		try {
			std::string res = user_handler_->user_info((*data.child("info"))["name"].to_string());
			lobby_.send_server_message(res.c_str(), sock);
		} catch (user_handler::error e) {
			lobby_.send_server_message(("There was and error looking up the details of the user '" +
			(*data.child("info"))["name"].to_string() + "'. " +" The error message was: "
			+ e.message).c_str(), sock);
		}
		return;
	}

	// A user requested to delete his nick
	if(data.child("drop")) {
		if(!(user_handler_->user_exists(pl->second.name()))) {
			lobby_.send_server_message("You are not registered.",
					sock);
			return;
		}

		// With the current policy of dissallowing to log in with a
		// registerd username without the password we should never get
		// to calling this
		if(!(pl->second.registered())) {
			lobby_.send_server_message("You are not logged in.",
					sock);
			return;
		}

		try {
			user_handler_->remove_user(pl->second.name());
			lobby_.send_server_message("Your username has been dropped.", sock);

			// Mark the player as not registered and send the other clients
			// an update to dislpay this change
			pl->second.mark_registered(false);

			simple_wml::document diff;
			make_change_diff(games_and_users_list_.root(), NULL,
						 "user", pl->second.config_address(), diff);
			lobby_.send_data(diff);
		} catch (user_handler::error e) {
			lobby_.send_server_message(("There was and error dropping your username. The error message was: "
			+ e.message).c_str(), sock);
		}
		return;
	}
}

void server::process_whisper(const network::connection sock,
                             simple_wml::node& whisper) const {
	if ((whisper["receiver"] == "") || (whisper["message"] == "")) {
		static simple_wml::document data(
		  "[message]\n"
		  "message=\"Invalid number of arguments\"\n"
		  "sender=\"server\"\n"
		  "[/message]\n", simple_wml::INIT_COMPRESSED);
		send_doc(data, sock);
		return;
	}
	const wesnothd::player_map::const_iterator pl = players_.find(sock);
	if (pl == players_.end()) {
		ERR_SERVER << "ERROR: Could not find whispering player. (socket: "
			<< sock << ")\n";
		return;
	}
	whisper.set_attr_dup("sender", pl->second.name().c_str());
	bool dont_send = false;
	const simple_wml::string_span& whisper_receiver = whisper["receiver"];
	for (wesnothd::player_map::const_iterator i = players_.begin(); i != players_.end(); ++i) {
		if (whisper_receiver != i->second.name().c_str()) {
			continue;
		}

		std::vector<wesnothd::game*>::const_iterator g;
		for (g = games_.begin(); g != games_.end(); ++g) {
			if (!(*g)->is_member(i->first)) continue;
			// Don't send to players in a running game the sender is part of.
			dont_send = ((*g)->started() && (*g)->is_player(i->first) && (*g)->is_member(sock));
			break;
		}
		if (dont_send) {
			break;
		}

		simple_wml::document cwhisper;
		whisper.copy_into(cwhisper.root().add_child("whisper"));
		send_doc(cwhisper, i->first);
		return;
	}

	simple_wml::document data;
	simple_wml::node& msg = data.root().add_child("message");

	if (dont_send) {
		msg.set_attr("message", "You cannot send private messages to players in a running game you observe.");
	} else {
		msg.set_attr_dup("message", ("Can't find '" + whisper["receiver"].to_string() + "'.").c_str());
	}

	msg.set_attr("sender", "server");
	send_doc(data, sock);
}

void server::process_data_lobby(const network::connection sock,
                                simple_wml::document& data) {
	DBG_SERVER << "in process_data_lobby...\n";

	const wesnothd::player_map::iterator pl = players_.find(sock);
	if (pl == players_.end()) {
		ERR_SERVER << "ERROR: Could not find player in players_. (socket: "
			<< sock << ")\n";
		return;
	}

	if (data.root().child("create_game")) {
		if (graceful_restart) {
			static simple_wml::document leave_game_doc("[leave_game]\n[/leave_game]\n", simple_wml::INIT_COMPRESSED);
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("This server is shutting down. You aren't allowed to make new games. Please reconnect to the new server.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		}
		const std::string game_name = (*data.root().child("create_game"))["name"].to_string();
		const std::string game_password = (*data.root().child("create_game"))["password"].to_string();
		DBG_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
			<< "\tcreates a new game: \"" << game_name << "\".\n";
		// Create the new game, remove the player from the lobby
		// and set the player as the host/owner.
		games_.push_back(new wesnothd::game(players_, sock, game_name));
		wesnothd::game& g = *games_.back();
		if(game_password.empty() == false) {
			g.set_password(game_password);
		}

		data.root().child("create_game")->copy_into(g.level().root());
		lobby_.remove_player(sock);
		simple_wml::document diff;
		if(make_change_diff(games_and_users_list_.root(), NULL,
		                    "user", pl->second.config_address(), diff)) {
			lobby_.send_data(diff);
		}
		return;
	}

	// See if the player is joining a game
	if (data.root().child("join")) {
		const bool observer = data.root().child("join")->attr("observe").to_bool();
		const std::string& password = (*data.root().child("join"))["password"].to_string();
		int game_id = (*data.root().child("join"))["id"].to_int();

		const std::vector<wesnothd::game*>::iterator g =
			std::find_if(games_.begin(),games_.end(), wesnothd::game_id_matches(game_id));

		static simple_wml::document leave_game_doc("[leave_game]\n[/leave_game]\n", simple_wml::INIT_COMPRESSED);
		if (g == games_.end()) {
			WRN_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tattempted to join unknown game:\t" << game_id << ".\n";
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("Attempt to join unknown game.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		} else if ((*g)->player_is_banned(sock)) {
			DBG_SERVER << network::ip_address(sock) << "\tReject banned player: "
				<< pl->second.name() << "\tfrom game:\t\"" << (*g)->name()
				<< "\" (" << game_id << ").\n";
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("You are banned from this game.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		} else if(!observer && !(*g)->password_matches(password)) {
			WRN_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tattempted to join game:\t\"" << (*g)->name() << "\" ("
				<< game_id << ") with bad password\n";
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("Incorrect password.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		} else if (observer && !(*g)->allow_observers()) {
			WRN_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tattempted to observe game:\t\"" << (*g)->name() << "\" ("
				<< game_id << ") which doesn't allow observers.\n";
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("Attempt to observe a game that doesn't allow observers.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		} else if (!(*g)->level_init()) {
			WRN_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tattempted to join uninitialized game:\t\"" << (*g)->name()
				<< "\" (" << game_id << ").\n";
			send_doc(leave_game_doc, sock);
			lobby_.send_server_message("Attempt to observe a game that doesn't allow observers.", sock);
			send_doc(games_and_users_list_, sock);
			return;
		}
		LOG_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
			<< "\tjoined game:\t\"" << (*g)->name()
			<< "\" (" << game_id << (observer ? ") as an observer.\n" : ").\n");
		lobby_.remove_player(sock);
		(*g)->add_player(sock, observer);
		(*g)->describe_slots();

		//send notification of changes to the game and user
		simple_wml::document diff;
		bool diff1 = make_change_diff(*games_and_users_list_.root().child("gamelist"),
					      "gamelist", "game", (*g)->description(), diff);
		bool diff2 = make_change_diff(games_and_users_list_.root(), NULL,
					      "user", pl->second.config_address(), diff);
		if (diff1 || diff2) {
			lobby_.send_data(diff);
		}
	}

	// See if it's a message, in which case we add the name of the sender,
	// and forward it to all players in the lobby.
	if (data.child("message")) {
		lobby_.process_message(data, pl);
	}

	// Player requests update of lobby content,
	// for example when cancelling the create game dialog
	if (data.child("refresh_lobby")) {
		send_doc(games_and_users_list_, sock);
	}
}

//! Process data sent by a player in a game. Note that 'data' by default gets
//! broadcasted and saved in the replay.
void server::process_data_game(const network::connection sock,
                               simple_wml::document& data) {
	DBG_SERVER << "in process_data_game...\n";
	
	const wesnothd::player_map::iterator pl = players_.find(sock);
	if (pl == players_.end()) {
		ERR_SERVER << "ERROR: Could not find player in players_. (socket: "
			<< sock << ")\n";
		return;
	}

	std::vector<wesnothd::game*>::iterator itor;
	for (itor = games_.begin(); itor != games_.end(); ++itor) {
		if ((*itor)->is_owner(sock) || (*itor)->is_member(sock))
			break;
	}
	if (itor == games_.end()) {
		ERR_SERVER << "ERROR: Could not find game for player: "
			<< pl->second.name() << ". (socket: " << sock << ")\n";
		return;
	}

	wesnothd::game* g = *itor;

	// If this is data describing the level for a game.
	if (data.root().child("side")) {
		if (!g->is_owner(sock)) {
			return;
		}
		size_t nsides = 0;
		const simple_wml::node::child_list& sides = data.root().children("side");
		for (simple_wml::node::child_list::const_iterator s = sides.begin(); s != sides.end(); ++s) {
	        	++nsides;
		}
		if (nsides > gamemap::MAX_PLAYERS) {
			delete_game(itor);
			std::stringstream msg;
			msg << "This server does not support games with more than "
				<< gamemap::MAX_PLAYERS << " sides.";
			lobby_.send_server_message(msg.str().c_str(), sock);
			return;
		}
		
		// If this game is having its level data initialized
		// for the first time, and is ready for players to join.
		// We should currently have a summary of the game in g->level().
		// We want to move this summary to the games_and_users_list_, and
		// place a pointer to that summary in the game's description.
		// g->level() should then receive the full data for the game.
		if (!g->level_init()) {
			LOG_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tcreated game:\t\"" << g->name() << "\" ("
				<< g->id() << ").\n";
			// Update our config object which describes the open games,
			// and save a pointer to the description in the new game.
			simple_wml::node* const gamelist = games_and_users_list_.child("gamelist");
			assert(gamelist != NULL);
			simple_wml::node& desc = gamelist->add_child("game");
			g->level().root().copy_into(desc);
			g->set_description(&desc);
			desc.set_attr_dup("id", lexical_cast<std::string>(g->id()).c_str());
		} else {
			WRN_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< "\tsent scenario data in game:\t\"" << g->name() << "\" ("
				<< g->id() << ") although it's already initialized.\n";
			return;
		}

		assert(games_and_users_list_.child("gamelist")->children("game").empty() == false);

		simple_wml::node& desc = *g->description();
		// Update the game's description.
		// If there is no shroud, then tell players in the lobby
		// what the map looks like
		if (!data["mp_shroud"].to_bool()) {
			desc.set_attr_dup("map_data", data["map_data"]);
		}
		if(data.child("era")) {
			desc.set_attr_dup("mp_era", data.child("era")->attr("id"));
			if(!(utils::string_bool(data.child("era")->attr("require_era").to_string(),true))) {
				desc.set_attr("require_era", "no");
			}
		} else {
			desc.set_attr("mp_era", "");
		}

		// map id
		desc.set_attr_dup("mp_scenario", data["id"]);
		desc.set_attr_dup("observer", data["observer"]);
		desc.set_attr_dup("mp_village_gold", data["mp_village_gold"]);
		desc.set_attr_dup("experience_modifier", data["experience_modifier"]);
		desc.set_attr_dup("mp_fog", data["mp_fog"]);
		desc.set_attr_dup("mp_shroud", data["mp_shroud"]);
		desc.set_attr_dup("mp_use_map_settings", data["mp_use_map_settings"]);
		desc.set_attr_dup("mp_countdown", data["mp_countdown"]);
		desc.set_attr_dup("mp_countdown_init_time", data["mp_countdown_init_time"]);
		desc.set_attr_dup("mp_countdown_turn_bonus", data["mp_countdown_turn_bonus"]);
		desc.set_attr_dup("mp_countdown_reservoir_time", data["mp_countdown_reservoir_time"]);
		desc.set_attr_dup("mp_countdown_action_bonus", data["mp_countdown_action_bonus"]);
		desc.set_attr_dup("savegame", data["savegame"]);
		desc.set_attr_dup("hash", data["hash"]);
		//desc["map_name"] = data["name"];
		//desc["map_description"] = data["description"];
		//desc[""] = data["objectives"];
		//desc[""] = data["random_start_time"];
		//desc[""] = data["turns"];
		//desc["client_version"] = data["version"];

		// Record the full scenario in g->level()

		g->level().swap(data);
		// The host already put himself in the scenario so we just need
		// to update_side_data().
		//g->take_side(sock);
		g->update_side_data();
		g->describe_slots();

		assert(games_and_users_list_.child("gamelist")->children("game").empty() == false);

		// Send the update of the game description to the lobby.
		simple_wml::document diff;
		make_add_diff(*games_and_users_list_.child("gamelist"), "gamelist", "game", diff);
		lobby_.send_data(diff);

		//! @todo FIXME: Why not save the level data in the history_?
		return;
// Everything below should only be processed if the game is already intialized.
	} else if (!g->level_init()) {
		WRN_SERVER << "Received unknown data from: " << pl->second.name() 
			<< " (socket:" << sock
			<< ") while the scenario wasn't yet initialized.\n" << data.output();
		return;
	// If the host is sending the next scenario data.
	} else if (data.child("store_next_scenario")) {
		if (!g->is_owner(sock)) return;
		if (!g->level_init()) {
			WRN_SERVER << network::ip_address(sock) << "\tWarning: "
				<< pl->second.name() << "\tsent [store_next_scenario] in game:\t\""
				<< g->name() << "\" (" << g->id()
				<< ") while the scenario is not yet initialized.";
			return;
		}
		size_t nsides = 0;
		const simple_wml::node::child_list& sides = data.root().children("side");
		for (simple_wml::node::child_list::const_iterator s = sides.begin(); s != sides.end(); ++s) {
	        	++nsides;
		}
		if (nsides > gamemap::MAX_PLAYERS) {
			delete_game(itor);
			std::stringstream msg;
			msg << "This server does not support games with more than "
				<< gamemap::MAX_PLAYERS << " sides.";
			lobby_.send_server_message(msg.str().c_str(), sock);
			return;
		}
		const simple_wml::node& s = *data.child("store_next_scenario");
		// Record the full scenario in g->level()
		g->level().clear();
		s.copy_into(g->level().root());
		g->start_game(pl);
		if (g->description() == NULL) {
			ERR_SERVER << network::ip_address(sock) << "\tERROR: \""
				<< g->name() << "\" (" << g->id()
				<< ") is initialized but has no description_.\n";
			return;
		}
		simple_wml::node& desc = *g->description();
		// Update the game's description.
		// If there is no shroud, then tell players in the lobby
		// what the map looks like.
		if (s["mp_shroud"].to_bool()) {
			desc.set_attr_dup("map_data", s["map_data"]);
		}

		if(s.child("era")) {
			desc.set_attr_dup("mp_era", s.child("era")->attr("id"));
		} else {
			desc.set_attr("mp_era", "");
		}

		// map id
		desc.set_attr_dup("mp_scenario", s["id"]);
		desc.set_attr_dup("observer", s["observer"]);
		desc.set_attr_dup("mp_village_gold", s["mp_village_gold"]);
		desc.set_attr_dup("experience_modifier", s["experience_modifier"]);
		desc.set_attr_dup("mp_fog", s["mp_fog"]);
		desc.set_attr_dup("mp_shroud", s["mp_shroud"]);
		desc.set_attr_dup("mp_use_map_settings", s["mp_use_map_settings"]);
		desc.set_attr_dup("mp_countdown", s["mp_countdown"]);
		desc.set_attr_dup("mp_countdown_init_time", s["mp_countdown_init_time"]);
		desc.set_attr_dup("mp_countdown_turn_bonus", s["mp_countdown_turn_bonus"]);
		desc.set_attr_dup("mp_countdown_reservoir_time", s["mp_countdown_reservoir_time"]);
		desc.set_attr_dup("mp_countdown_action_bonus", s["mp_countdown_action_bonus"]);
		desc.set_attr_dup("hash", s["hash"]);
		//desc["map_name"] = s["name"];
		//desc["map_description"] = s["description"];
		//desc[""] = s["objectives"];
		//desc[""] = s["random_start_time"];
		//desc[""] = s["turns"];
		//desc["client_version"] = s["version"];
		// Send the update of the game description to the lobby.

		//update the game having changed in the lobby
		update_game_in_lobby(g);
		return;
	// If a player advances to the next scenario of a mp campaign. (deprecated)
	} else if(data.child("notify_next_scenario")) {
		//g->send_data(g->construct_server_message(pl->second.name()
		//		+ " advanced to the next scenario."), sock);
		return;
	// A mp client sends a request for the next scenario of a mp campaign.
	} else if (data.child("load_next_scenario")) {
		g->load_next_scenario(pl);
		return;
	} else if (data.child("start_game")) {
		if (!g->is_owner(sock)) return;
		// Send notification of the game starting immediately.
		// g->start_game() will send data that assumes
		// the [start_game] message has been sent
		g->send_data(data, sock);
		g->start_game(pl);

		//update the game having changed in the lobby
		update_game_in_lobby(g);
		return;
	} else if (data.child("leave_game")) {
		if ((g->is_player(sock) && g->nplayers() == 1)
			|| (g->is_owner(sock) && !g->started())) {
			LOG_SERVER << network::ip_address(sock) << "\t" << pl->second.name()
				<< (g->started() ? "\tended game:\t\"" : "\taborted game:\t\"")
				<< g->name() << "\" (" << g->id() << ")"
				<< (g->started() ? " at turn: "
					+ lexical_cast_default<std::string,size_t>(g->current_turn())
					+ " with reason: '" + g->termination_reason() + "'" : "")
				<< ".\n";
			g->send_server_message_to_all((pl->second.name() + " ended the game.").c_str(), pl->first);
			// Remove the player in delete_game() with all other remaining
			// ones so he gets the updated gamelist.
			delete_game(itor);
		} else {
			g->remove_player(sock);
			lobby_.add_player(sock, true);
			g->describe_slots();

			// Send all other players in the lobby the update to the gamelist.
			simple_wml::document diff;
			bool diff1 = make_change_diff(*games_and_users_list_.root().child("gamelist"),
						      "gamelist", "game", g->description(), diff);
			bool diff2 = make_change_diff(games_and_users_list_.root(), NULL,
						      "user", pl->second.config_address(), diff);
			if (diff1 || diff2) {
				lobby_.send_data(diff, sock);
			}

			// Send the player who has quit the gamelist.
			send_doc(games_and_users_list_, sock);
		}
		return;
	// If this is data describing side changes by the host.
	} else if (data.child("scenario_diff")) {
		if (!g->is_owner(sock)) return;
		g->level().root().apply_diff(*data.child("scenario_diff"));
		const simple_wml::node* cfg_change = data.child("scenario_diff")->child("change_child");
		if ((cfg_change != NULL) && (cfg_change->child("side") != NULL)) {
			g->update_side_data();
		}
		if (g->describe_slots()) {
			update_game_in_lobby(g);
		}
		g->send_data(data, sock);
		return;
	// If a player changes his faction.
	} else if (data.child("change_faction")) {
		g->send_data(data, sock);
		return;
	// If the owner of a side is changing the controller.
	} else if (data.child("change_controller")) {
		const simple_wml::node& change = *data.child("change_controller");
		g->transfer_side_control(sock, change);
		if (g->describe_slots()) {
			update_game_in_lobby(g);
		}
		// FIXME: Why not save it in the history_? (if successful)
		return;
	// If all observers should be muted. (toggles)
	} else if (data.child("muteall")) {
		if (!g->is_owner(sock)) {
			g->send_server_message("You cannot mute: not the game host.", sock);
			return;
		}
		g->mute_all_observers();
		return;
	// If an observer should be muted.
	} else if (data.child("mute")) {
		g->mute_observer(*data.child("mute"), pl);
		return;
	// The owner is kicking/banning someone from the game.
	} else if (data.child("kick") || data.child("ban")) {
		bool ban = (data.child("ban") != NULL);
		const network::connection user = 
				(ban ? g->ban_user(*data.child("ban"), pl)
				: g->kick_member(*data.child("kick"), pl));
		if (user) {
			lobby_.add_player(user, true);
			if (g->describe_slots()) {
				update_game_in_lobby(g, user);
			}
			// Send the removed user the lobby game list.
			send_doc(games_and_users_list_, user);
			// FIXME: should also send a user diff to the lobby
			//        to mark this player as available for others
		}
		return;
	// If info is being provided about the game state.
	} else if (data.child("info")) {
		if (!g->is_player(sock)) return;
		const simple_wml::node& info = *data.child("info");
		if (info["type"] == "termination") {
			g->set_termination_reason(info["condition"].to_string());
		}
		return;
	} else if (data.child("turn")) {
		// Notify the game of the commands, and if it changes
		// the description, then sync the new description
		// to players in the lobby.
		if (g->process_turn(data, pl)) {
			update_game_in_lobby(g);
		}
		return;
	} else if (data.child("message")) {
		g->process_message(data, pl);
		return;
	// Data to store and broadcast.
	} else if (data.child("stop_updates")) {
//		if (g->started()) g->record_data(data);
		g->send_data(data, sock);
		return;
	// Data to ignore.
	} else if (data.child("error")
	|| data.child("side_secured")
	|| data.root().has_attr("failed")
	|| data.root().has_attr("side_drop")
	|| data.root().has_attr("side")) {
		return;
	}

	WRN_SERVER << "Received unknown data from: " << pl->second.name() 
		<< ". (socket:" << sock << ")\n" << data.output();
}

void server::delete_game(std::vector<wesnothd::game*>::iterator game_it) {
	metrics_.game_terminated((*game_it)->termination_reason());

	simple_wml::node* const gamelist = games_and_users_list_.child("gamelist");
	assert(gamelist != NULL);

	// Send a diff of the gamelist with the game deleted to players in the lobby
	simple_wml::document diff;
	bool send_diff = false;
	if(make_delete_diff(*gamelist, "gamelist", "game",
	                    (*game_it)->description(), diff)) {
		send_diff = true;
	}

	// Delete the game from the games_and_users_list_.
	const simple_wml::node::child_list& games = gamelist->children("game");
	const simple_wml::node::child_list::const_iterator g =
		std::find(games.begin(), games.end(), (*game_it)->description());
	if (g != games.end()) {
		const size_t index = g - games.begin();
		gamelist->remove_child("game", index);
	} else {
		// Can happen when the game ends before the scenario was transfered.
		LOG_SERVER << "Could not find game (" << (*game_it)->id()
			<< ") to delete in games_and_users_list_.\n";
	}
	const wesnothd::user_vector& users = (*game_it)->all_game_users();
	// Set the availability status for all quitting users.
	for (wesnothd::user_vector::const_iterator user = users.begin();
		user != users.end(); user++)
	{
		const wesnothd::player_map::iterator pl = players_.find(*user);
		if (pl != players_.end()) {
			pl->second.mark_available();
			if (make_change_diff(games_and_users_list_.root(), NULL,
					     "user", pl->second.config_address(), diff)) {
				send_diff = true;
			}
		} else {
			ERR_SERVER << "ERROR: delete_game(): Could not find user in players_. (socket: "
				<< *user << ")\n";
		}
	}
	if (send_diff) {
		lobby_.send_data(diff);
	}

	//send users in the game a notification to leave the game since it has ended
	static simple_wml::document leave_game_doc("[leave_game]\n[/leave_game]\n", simple_wml::INIT_COMPRESSED);
	(*game_it)->send_data(leave_game_doc);
	// Put the remaining users back in the lobby.
	lobby_.add_players(**game_it, true);

	(*game_it)->send_data(games_and_users_list_);

	delete *game_it;
	games_.erase(game_it);
}

void server::update_game_in_lobby(const wesnothd::game* g, network::connection exclude)
{
	simple_wml::document diff;
	if(make_change_diff(*games_and_users_list_.root().child("gamelist"), "gamelist", "game", g->description(), diff)) {
		lobby_.send_data(diff, exclude);
	}
}

       #include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>
#ifndef _MSC_VER
       #include <unistd.h>
#endif

int main(int argc, char** argv) {
	int port = 15000;
	size_t min_threads = 5;
	size_t max_threads = 0;

	std::string config_file;

#ifndef FIFODIR
# warning "No FIFODIR set"
# define FIFODIR "/var/run/wesnothd"
#endif
	std::string fifo_path = std::string(FIFODIR) + "/socket";

	// setting path to currentworking directory
	game_config::path = get_cwd();

	// show 'info' by default
	lg::set_log_domain_severity("server", 2);
	lg::timestamps(true);

	for (int arg = 1; arg != argc; ++arg) {
		const std::string val(argv[arg]);
		if (val.empty()) {
			continue;
		}

		if ((val == "--config" || val == "-c") && arg+1 != argc) {
			config_file = argv[++arg];
		} else if (val == "--verbose" || val == "-v") {
			lg::set_log_domain_severity("all",3);
		} else if (val.substr(0, 6) == "--log-") {
			size_t p = val.find('=');
			if (p == std::string::npos) {
				std::cerr << "unknown option: " << val << '\n';
				return 0;
			}
			std::string s = val.substr(6, p - 6);
			int severity;
			if (s == "error") severity = 0;
			else if (s == "warning") severity = 1;
			else if (s == "info") severity = 2;
			else if (s == "debug") severity = 3;
			else {
				std::cerr << "unknown debug level: " << s << '\n';
				return 0;
			}
			while (p != std::string::npos) {
				size_t q = val.find(',', p + 1);
				s = val.substr(p + 1, q == std::string::npos ? q : q - (p + 1));
				if (!lg::set_log_domain_severity(s, severity)) {
					std::cerr << "unknown debug domain: " << s << '\n';
					return 0;
				}
				p = q;
			}
		} else if ((val == "--port" || val == "-p") && arg+1 != argc) {
			port = atoi(argv[++arg]);
		} else if (val == "--help" || val == "-h") {
			std::cout << "usage: " << argv[0]
				<< " [-dvV] [-c path] [-m n] [-p port] [-t n]\n"
				<< "  -c, --config <path>        Tells wesnothd where to find the config file to use.\n"
				<< "  -d, --daemon               Runs wesnothd as a daemon.\n"
				<< "  -h, --help                 Shows this usage message.\n"
				<< "  --log-<level>=<domain1>,<domain2>,...\n"
				<< "                             sets the severity level of the debug domains.\n"
				<< "                             'all' can be used to match any debug domain.\n"
				<< "                             Available levels: error, warning, info, debug.\n"
				<< "  -p, --port <port>          Binds the server to the specified port.\n"
				<< "  -t, --threads <n>          Uses n worker threads for network I/O (default: 5).\n"
				<< "  -v  --verbose              Turns on more verbose logging.\n"
				<< "  -V, --version              Returns the server version.\n";
			return 0;
		} else if (val == "--version" || val == "-V") {
			std::cout << "Battle for Wesnoth server " << game_config::version
				<< "\n";
			return 0;
		} else if (val == "--daemon" || val == "-d") {
#ifdef _WIN32
			ERR_SERVER << "Running as a daemon is not supported on this platform\n";
			return -1;
#else
			const pid_t pid = fork();
			if (pid < 0) {
				ERR_SERVER << "Could not fork and run as a daemon\n";
				return -1;
			} else if (pid > 0) {
				std::cout << "Started wesnothd as a daemon with process id "
					<< pid << "\n";
				return 0;
			}

			setsid();
#endif
		} else if ((val == "--threads" || val == "-t") && arg+1 != argc) {
			min_threads = atoi(argv[++arg]);
			if (min_threads > 30) {
				min_threads = 30;
			}
		} else if ((val == "--max-threads" || val == "-T") && arg+1 != argc) {
			max_threads = atoi(argv[++arg]);
		} else if(val == "--request_sample_frequency" && arg+1 != argc) {
			request_sample_frequency = atoi(argv[++arg]);
		} else if (val[0] == '-') {
			ERR_SERVER << "unknown option: " << val << "\n";
			return 0;
		} else {
			port = atoi(argv[arg]);
		}
	}
	input_stream input(fifo_path);

	network::set_raw_data_only();

	try {
		server(port, input, config_file, min_threads, max_threads).run();
	} catch(network::error& e) {
		ERR_SERVER << "Caught network error while server was running. Aborting.: "
			<< e.message << "\n";
		//! @todo errno should be passed here with the error or it might not be
		//! the true errno anymore. Seems to work good enough for now though.
		return errno;
	} catch(std::bad_alloc&) {
                ERR_SERVER << "Ran out of memory. Aborting.\n";
		return ENOMEM;
	} catch(...) {
		ERR_SERVER << "Caught unknown error while server was running. Aborting.\n";
		return -1;
	}

	return 0;
}
