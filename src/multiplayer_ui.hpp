/* $Id$ */
/*
   Copyright (C)
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef MULTIPLAYER_UI_HPP_INCLUDED
#define MULTIPLAYER_UI_HPP_INCLUDED

#include "config.hpp"
#include "hotkeys.hpp"
#include "network.hpp"
#include "preferences_display.hpp"
#include "widgets/label.hpp"
#include "widgets/button.hpp"
#include "widgets/menu.hpp"
#include "widgets/textbox.hpp"
#include "menu_events.hpp"

#include <deque>
#include <string>

class display;
class game_state;

namespace mp {

enum controller { CNTR_NETWORK = 0, CNTR_LOCAL, CNTR_COMPUTER, CNTR_EMPTY, CNTR_LAST };

void check_response(network::connection res, const config& data);

void level_to_gamestate(config& level, game_state& state, bool saved_game=false);

std::string get_colour_string(int id);

//this class memorizes a chat session.
class chat
{
public:
	chat();

	void add_message(const std::string& user, const std::string& message);

	void init_textbox(gui::textbox& textbox);
	void update_textbox(gui::textbox& textbox);

private:
	struct msg {
		msg(const std::string& user, const std::string& message) :
			user(user), message(message) {};
		std::string user;
		std::string message;
	};
	typedef std::deque<msg> msg_hist;

	std::string format_message(const msg& message);

	msg_hist message_history_;
	msg_hist::size_type last_update_;
};

//a base class for the different multiplayer base dialogs: game list, create
//game, wait game, game setup
class ui : public gui::widget, private events::chat_handler, private font::floating_label_context
{
public:
	enum result { CONTINUE, JOIN, OBSERVE, CREATE, PREFERENCES, PLAY, QUIT };

	ui(game_display& d, const std::string& title,
			const config& cfg, chat& c, config& gamelist);

	// Asks the multiplayer_ui to pump some data from the network, and then
	// to process it. The actual processing will be left to the child
	// classes, through process_network_data and process_network_error
	void process_network();

	// Returns the result of the current widget. While the result is equal
	// to continue, the widget should not be destroyed.
	result get_result();

	// Hides children, moves them (using layout_children), then shows them.
	// The methodes hide_children and layout_children are supposed to be
	// overridden by subclasses of this class which add new sub-widgets.
	void set_location(const SDL_Rect& rect);
	using widget::set_location;

protected:
	int xscale(int x) const;
	int yscale(int y) const;

	SDL_Rect client_area() const;

	game_display& disp_;
	game_display& disp() { return disp_; };

	// Returns the main game config, as defined by loading the preprocessed
	// WML files. Children of this class may need this to obtain, for
	// example, the list of available eras.
	const config& game_config() const;

	virtual void draw_contents();

	virtual void process_event();

	virtual void handle_event(const SDL_Event& event);
	virtual void handle_key_event(const SDL_KeyboardEvent& event);

	// Override chat_handler
	void send_chat_query(const std::string& args);
	void add_chat_message(const std::string& speaker, int side, const std::string& message, game_display::MESSAGE_TYPE type=game_display::MESSAGE_PRIVATE);
	void send_chat_message(const std::string& message, bool allies_only=false);


	// Processes any pending network data. Called by the public
	// process_network() method. Overridden by subclasses who add more
	// behaviour for network.
	virtual void process_network_data(const config& data, const network::connection sock);

	// Processes any pending network error. Called by the public
	// process_network() method. Overridden by subclasses
	virtual void process_network_error(network::error& error);

	// Return true if we must accept incoming connections, false if not.
	// Defaults to not.
	virtual bool accept_connections() { return false; };

	// Processes a pending network connection.
	virtual void process_network_connection(const network::connection sock);

	// Hides or shows all gui::widget children of this widget. Should be
	// overridden by subclasses which add their own children.
	virtual void hide_children(bool hide=true);

	// Lays the children out. This method is to be overridden by the
	// subclasses of the mp_ui class; it will be called
	virtual void layout_children(const SDL_Rect& rect);

	// Sets the result of this dialog, to be checked by get_result()
	result set_result(result res);

	// Sets the name of the selected game which is used to highlight
	// the names of the players which have joined this game
	void set_selected_game(const std::string game_name);

	// Called each time the gamelist_ variable is updated. May be
	// overridden by child classes to add custom gamelist behaviour.
	virtual void gamelist_updated(bool silent=true);

	// Sets the user list
	void set_user_list(const std::vector<std::string>&, bool silent);
	void set_user_menu_items(const std::vector<std::string>& list);

	// Returns the current gamelist
	config& gamelist() { return gamelist_; };

	const gui::widget& title() const;

private:
	/** Set to true when the widgets are intialized. Allows delayed
	 * initialization on first positioning. */
	bool initialized_;
	bool gamelist_initialized_;

	// Ensures standard hotkeys are coorectly handled
	const hotkey::basic_handler hotkey_handler_;

	const preferences::display_manager disp_manager_;

	// The main game configuration, as defined by loading the preprocessed
	// WML files. Access using the game_config() method if necessary.
	const config& game_config_;

	chat& chat_;

	config& gamelist_;

	gui::label title_;
#ifndef USE_TINY_GUI
	gui::textbox entry_textbox_;
#endif
	gui::textbox chat_textbox_;

	gui::menu users_menu_;

	std::vector<std::string> user_list_;

	std::string selected_game_;

	result result_;

	bool gamelist_refresh_;

	Uint32 lobby_clock_;

public:
	enum user_relation { ME, FRIEND, NEUTRAL, IGNORED };
	enum user_state    { LOBBY, GAME, SEL_GAME };

private:
	struct user_info
	{
		std::string    name;
		std::string    location;
		user_relation  relation;
		user_state     state;
		bool operator> (const user_info& b) const;
	};
};

}

#endif
