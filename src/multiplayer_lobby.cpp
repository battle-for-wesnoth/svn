#include "events.hpp"
#include "font.hpp"
#include "image.hpp"
#include "key.hpp"
#include "language.hpp"
#include "multiplayer_lobby.hpp"
#include "network.hpp"
#include "show_dialog.hpp"
#include "widgets/textbox.hpp"
#include "widgets/button.hpp"
#include "widgets/menu.hpp"

#include "SDL.h"

#include <sstream>

namespace {
	int xscale(display& disp, int x)
	{
		return (x*disp.x())/1024;
	}

	int yscale(display& disp, int y)
	{
		return (y*disp.y())/768;
	}
}

namespace lobby {

RESULT enter(display& disp, config& game_data)
{
	const events::resize_lock prevent_resizing;

	CKey key;

	std::vector<std::string> messages;

	gui::textbox message_entry(disp,500);

	scoped_sdl_surface background(image::get_image("misc/lobby.png",image::UNSCALED));
	background.assign(scale_surface(background,disp.x(),disp.y()));

	update_whole_screen();

	for(;;) {
		if(background != NULL)
			SDL_BlitSurface(background, NULL, disp.video().getSurface(), NULL);

		// Display Chats
		std::stringstream text;
		for(size_t n = messages.size() > 8 ? messages.size()-8 : 0;
		    n != messages.size(); ++n) {
			text << messages[n] << "\n";
		}

		const SDL_Rect chat_area = { xscale(disp,19), yscale(disp,574), xscale(disp,832), yscale(disp,130) };

		font::draw_text(&disp,chat_area,12,font::NORMAL_COLOUR,
		                text.str(),xscale(disp,19),yscale(disp,574));
		update_rect(chat_area);

		// Game List GUI
		const config* const gamelist = game_data.child("gamelist");
		assert(gamelist != NULL);

		std::vector<std::string> options;
		config::const_child_itors i;
		for(i = gamelist->child_range("game"); i.first != i.second; ++i.first) {
			std::string name = (**i.first)["name"];
			if(name.size() > 30)
				name.resize(30);
			options.push_back(name);
		}

		const bool games_available = options.empty() == false;
		if(!games_available) {
			options.push_back("<no games open>");
		}

		gui::menu games_menu(disp,options);
		gui::button join_game(disp,string_table["join_game"]);
		gui::button new_game(disp,string_table["create_new_game"]);
		gui::button quit_game(disp,string_table["quit_button"]);
		
		std::vector<std::string> users;
		for(i = game_data.child_range("user"); i.first != i.second; ++i.first) {
			std::string name = (**i.first)["name"];
			if(name.size() > 30)
				name.resize(30);

			users.push_back(name);
		}

		std::cerr << "have " << users.size() << " users\n";

		if(users.empty()) {
			std::cerr << "ERROR: empty user list received\n";
			users.push_back("error");
		}

		gui::menu users_menu(disp,users);

		// Set GUI locations
		users_menu.set_loc(xscale(disp,869),yscale(disp,23));
		users_menu.set_width(xscale(disp,129));
		update_rect(xscale(disp,869),yscale(disp,23),xscale(disp,129),yscale(disp,725));
		games_menu.set_loc(xscale(disp,19),yscale(disp,23));
		games_menu.set_width(xscale(disp,832));
		update_rect(xscale(disp,19),yscale(disp,23),xscale(disp,832),yscale(disp,520));
		join_game.set_xy(xscale(disp,19),yscale(disp,545));
		new_game.set_xy(xscale(disp,19)+join_game.width()+5,yscale(disp,545));
		quit_game.set_xy(xscale(disp,19)+join_game.width()+5+new_game.width()+5,yscale(disp,545));
		message_entry.set_position(xscale(disp,19),yscale(disp,725));
		message_entry.set_width(xscale(disp,832));

		update_whole_screen();

		bool old_enter = true;

		for(;;) {
			int mousex, mousey;
			const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

			const bool left_button = mouse_flags&SDL_BUTTON_LMASK;

			games_menu.process(mousex,mousey,left_button,
			                   key[SDLK_UP],key[SDLK_DOWN],
			                   key[SDLK_PAGEUP],key[SDLK_PAGEDOWN]);

			users_menu.process(mousex,mousey,left_button,
			                   key[SDLK_UP],key[SDLK_DOWN],
			                   key[SDLK_PAGEUP],key[SDLK_PAGEDOWN]);
			 
			if(games_available &&
			   join_game.process(mousex,mousey,left_button)) {
				const size_t index = size_t(games_menu.selection());
				const config::const_child_itors i
				                   = gamelist->child_range("game");
				assert(index < size_t(i.second - i.first));
				const std::string& id = (**(i.first+index))["id"];

				config response;
				config& join = response.add_child("join");
				join["id"] = id;
				network::send_data(response);
				return JOIN;
			}
			
			if(new_game.process(mousex,mousey,left_button)) {
				return CREATE;
				break;
			}

			const bool enter = key[SDLK_RETURN] && !old_enter;
			old_enter = key[SDLK_RETURN];
			if((enter) &&
			   message_entry.text().empty() == false) {
				config msg;
				config& child = msg.add_child("message");
				child["message"] = message_entry.text();
				network::send_data(msg);
				message_entry.clear();
			}

			if(key[SDLK_ESCAPE] || quit_game.process(mousex,mousey,left_button)){
				return QUIT;
			}

			events::raise_process_event();
			events::raise_draw_event();

			config data;

			//if the list is refreshed, we want to redraw the entire screen
			const network::connection res = network::receive_data(data);
			if(res) {
				if(data.child("gamelist")) {
					game_data = data;
					break;
				} else if(data.child("error")) {
					throw network::error((*data.child("error"))["message"]);
				} else if(data.child("message")) {
					const config& msg = *data.child("message");
					std::stringstream message;
					message << "<" << msg["sender"] << ">  " << msg["message"];
					messages.push_back(message.str());
					break;
				}

				update_whole_screen();
			}

			events::pump();
			disp.video().flip();
			SDL_Delay(20);
		}
	}
}

}
