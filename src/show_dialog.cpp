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

#include "config.hpp"
#include "events.hpp"
#include "font.hpp"
#include "image.hpp"
#include "language.hpp"
#include "playlevel.hpp"
#include "show_dialog.hpp"
#include "language.hpp"
#include "sdl_utils.hpp"
#include "tooltips.hpp"
#include "util.hpp"
#include "widgets/button.hpp"
#include "widgets/menu.hpp"
#include "widgets/textbox.hpp"

#include <iostream>
#include <numeric>

namespace {
bool is_in_dialog = false;
}

namespace gui {

bool in_dialog() { return is_in_dialog; }

dialog_manager::dialog_manager() : reset_to(is_in_dialog) {is_in_dialog = true;}
dialog_manager::~dialog_manager() { is_in_dialog = reset_to; }

void draw_dialog_frame(int x, int y, int w, int h, display& disp)
{
	draw_dialog_background(x,y,w,h,disp);

	const scoped_sdl_surface top(image::get_image("misc/menu-border-top.png",image::UNSCALED));
	const scoped_sdl_surface bot(image::get_image("misc/menu-border-bottom.png",image::UNSCALED));
	const scoped_sdl_surface left(image::get_image("misc/menu-border-left.png",image::UNSCALED));
	const scoped_sdl_surface right(image::get_image("misc/menu-border-right.png",image::UNSCALED));

	if(top == NULL || bot == NULL || left == NULL || right == NULL)
		return;

	scoped_sdl_surface top_image(scale_surface(top,w,top->h));

	if(top_image.get() != NULL) {
		disp.blit_surface(x,y-top->h,top_image.get());
	}

	scoped_sdl_surface bot_image(scale_surface(bot,w,bot->h));

	if(bot_image.get() != NULL) {
		disp.blit_surface(x,y+h,bot_image.get());
	}

	scoped_sdl_surface left_image(scale_surface(left,left->w,h));

	if(left_image.get() != NULL) {
		disp.blit_surface(x-left->w,y,left_image.get());
	}

	scoped_sdl_surface right_image(scale_surface(right,right->w,h));

	if(right_image.get() != NULL) {
		disp.blit_surface(x+w,y,right_image.get());
	}

	update_rect(x-left->w,y-top->h,w+left->w+right->w,h+top->h+bot->h);

	const scoped_sdl_surface top_left(image::get_image("misc/menu-border-topleft.png",image::UNSCALED));
	const scoped_sdl_surface bot_left(image::get_image("misc/menu-border-botleft.png",image::UNSCALED));
	const scoped_sdl_surface top_right(image::get_image("misc/menu-border-topright.png",image::UNSCALED));
	const scoped_sdl_surface bot_right(image::get_image("misc/menu-border-botright.png",image::UNSCALED));
	if(top_left == NULL || bot_left == NULL || top_right == NULL ||
	   bot_right == NULL)
		return;

	disp.blit_surface(x-top_left->w,y-top_left->h,top_left);
	disp.blit_surface(x-bot_left->w,y+h,bot_left);
	disp.blit_surface(x+w,y-top_right->h,top_right);
	disp.blit_surface(x+w,y+h,bot_right);
}

void draw_dialog_background(int x, int y, int w, int h, display& disp)
{
	static const std::string menu_background = "misc/menu-background.png";

	const scoped_sdl_surface bg(image::get_image(menu_background,image::UNSCALED));

	const SDL_Rect& screen_bounds = disp.screen_area();
	if(x < 0)
		x = 0;

	if(y < 0)
		y = 0;

	if(x + w > screen_bounds.w)
		w = screen_bounds.w - x;

	if(y + h > screen_bounds.h)
		h = screen_bounds.h - y;

	for(int i = 0; i < w; i += bg->w) {
		for(int j = 0; j < h; j += bg->h) {
			SDL_Rect src = {0,0,0,0};
			src.w = minimum(w - i,bg->w);
			src.h = minimum(h - j,bg->h);
			SDL_Rect dst = src;
			dst.x = x + i;
			dst.y = y + j;
			SDL_BlitSurface(bg,&src,disp.video().getSurface(),&dst);
		}
	}
}

void draw_rectangle(int x, int y, int w, int h, short colour,
                    SDL_Surface* target)
{
	if(x < 0 || y < 0 || x+w >= target->w || y+h >= target->h) {
		std::cerr << "Rectangle has illegal co-ordinates: " << x << "," << y
		          << "," << w << "," << h << "\n";
		return;
	}

	surface_lock dstlock(target);

	short* top_left = dstlock.pixels() + target->w*y+x;
	short* top_right = top_left + w;
	short* bot_left = top_left + target->w*h;
	short* bot_right = bot_left + w;

	std::fill(top_left,top_right+1,colour);
	std::fill(bot_left,bot_right+1,colour);
	while(top_left != bot_left) {
		*top_left = colour;
		*top_right = colour;
		top_left += target->w;
		top_right += target->w;
	}
}

void draw_solid_tinted_rectangle(int x, int y, int w, int h,
                                 int r, int g, int b,
                                 double alpha, SDL_Surface* target)
{
	if(x < 0 || y < 0 || x+w >= target->w || y+h >= target->h) {
		std::cerr << "Rectangle has illegal co-ordinates: " << x << "," << y
		          << "," << w << "," << h << "\n";
		return;
	}

	surface_lock dstlock(target);

	const SDL_PixelFormat* const fmt = target->format;
	short* p = dstlock.pixels() + target->w*y + x;
	while(h > 0) {
		short* beg = p;
		short* const end = p + w;
		while(beg != end) {
			const int cur_r = ((*beg&fmt->Rmask) >> fmt->Rshift) << fmt->Rloss;
			const int cur_g = ((*beg&fmt->Gmask) >> fmt->Gshift) << fmt->Gloss;
			const int cur_b = ((*beg&fmt->Bmask) >> fmt->Bshift) << fmt->Bloss;

			const int new_r = int(double(cur_r)*(1.0-alpha) + double(r)*alpha);
			const int new_g = int(double(cur_g)*(1.0-alpha) + double(g)*alpha);
			const int new_b = int(double(cur_b)*(1.0-alpha) + double(b)*alpha);

			*beg = ((new_r >> fmt->Rloss) << fmt->Rshift) |
			       ((new_g >> fmt->Gloss) << fmt->Gshift) |
			       ((new_b >> fmt->Bloss) << fmt->Bshift);

			++beg;
		}

		p += target->w;
		--h;
	}
}

} //end namespace gui

namespace gui
{

size_t text_to_lines(std::string& message, size_t max_length)
{
	size_t cur_line = 0, longest_line = 0;
	for(std::string::iterator i = message.begin(); i != message.end(); ++i) {
		if(*i == ' ' && cur_line > max_length)
			*i = '\n';

		if(*i == '\n' || i+1 == message.end()) {
			if(cur_line > longest_line)
				longest_line = cur_line;

			cur_line = 0;

		} else {
			++cur_line;
		}
	}

	return longest_line;
}

int show_dialog(display& disp, SDL_Surface* image,
                const std::string& caption, const std::string& msg,
                DIALOG_TYPE type,
				const std::vector<std::string>* menu_items_ptr,
				const std::vector<unit>* units_ptr,
				const std::string& text_widget_label,
				std::string* text_widget_text,
                dialog_action* action, std::vector<check_item>* options)
{
	if(disp.update_locked())
		return -1;

	const events::event_context dialog_events_context;
	const dialog_manager manager;

	const events::resize_lock prevent_resizing;

	const std::vector<std::string>& menu_items =
	   (menu_items_ptr == NULL) ? std::vector<std::string>() : *menu_items_ptr;
	const std::vector<unit>& units =
	   (units_ptr == NULL) ? std::vector<unit>() : *units_ptr;

	static const int message_font_size = 16;
	static const int caption_font_size = 18;

	CVideo& screen = disp.video();
	SDL_Surface* const scr = screen.getSurface();

	SDL_Rect clipRect = { 0, 0, disp.x(), disp.y() };

	const bool use_textbox = text_widget_text != NULL;
	static const std::string default_text_string = "";
	const unsigned int text_box_width = 350;
	textbox text_widget(disp,text_box_width,
	                    use_textbox ? *text_widget_text : default_text_string);

	int text_widget_width = 0;
	int text_widget_height = 0;
	if(use_textbox) {
		text_widget_width =
		 font::draw_text(NULL, clipRect, message_font_size,
		                 font::NORMAL_COLOUR, text_widget_label, 0, 0, NULL).w +
		                            text_widget.location().w;
		text_widget_height = text_widget.location().h + 6;
	}

	menu menu_(disp,menu_items,type == MESSAGE);

	const int border_size = 6;

	const int max_line_length = 58;

	std::string message = msg;
	const size_t longest_line = text_to_lines(message,max_line_length);

	SDL_Rect text_size = { 0, 0, 0, 0 };
	if(!message.empty()) {
		text_size = font::draw_text(NULL, clipRect, message_font_size,
						            font::NORMAL_COLOUR, message, 0, 0, NULL);
	}

	SDL_Rect caption_size = { 0, 0, 0, 0 };
	if(!caption.empty()) {
		caption_size = font::draw_text(NULL, clipRect, caption_font_size,
		                               font::NORMAL_COLOUR,caption,0,0,NULL);
	}

	const std::string* button_list = NULL;
	std::vector<button> buttons;
	switch(type) {
		case MESSAGE:
			break;
		case OK_ONLY: {
			static const std::string thebuttons[] = { "ok_button", "" };
			button_list = thebuttons;
			break;
		}

		case YES_NO: {
			static const std::string thebuttons[] = { "yes_button",
			                                          "no_button", ""};
			button_list = thebuttons;
			break;
		}

		case OK_CANCEL: {
			static const std::string thebuttons[] = { "ok_button",
			                                          "cancel_button",""};
			button_list = thebuttons;
			break;
		}

		case CANCEL_ONLY: {
			static const std::string thebuttons[] = { "cancel_button", "" };
			button_list = thebuttons;
			break;
		}
	}

	const int button_height_padding = 10;
	int button_width_padding = 0;
	int button_heights = 0;
	int button_widths = 0;
	if(button_list != NULL) {
		try {
			while(button_list->empty() == false) {
				buttons.push_back(button(disp,string_table[*button_list]));

				if(buttons.back().height() > button_heights)
					button_heights = buttons.back().height();

				button_widths += buttons.back().width();
				button_width_padding += 5;

				++button_list;
			}

		} catch(button::error&) {
			std::cerr << "error initializing button!\n";
		}
	}

	if(button_heights > 0) {
		button_heights += button_height_padding;
	}

	int check_button_height = 0;
	int check_button_width = 0;

	std::vector<button> check_buttons;
	if(options != NULL) {
		for(std::vector<check_item>::const_iterator i = options->begin(); i != options->end(); ++i) {
			button check_button(disp,i->label,button::TYPE_CHECK);
			check_button_height += check_button.height() + button_height_padding;
			check_button_width = maximum(check_button.width(),check_button_width);

			check_buttons.push_back(check_button);
		}
	}

	const int left_padding = 10;
	const int right_padding = 10;
	const int image_h_padding = image != NULL ? 10 : 0;
	const int top_padding = 10;
	const int bottom_padding = 10;
	const int menu_hpadding = text_size.h > 0 && menu_.height() > 0 ? 10 : 0;
	const int padding_width = left_padding + right_padding + image_h_padding;
	const int padding_height = top_padding + bottom_padding + menu_hpadding;
	const int caption_width = caption_size.w;
	const int image_width = image != NULL ? image->w : 0;
	const int total_image_width = caption_width > image_width ?
	                              caption_width : image_width;
	const int image_height = image != NULL ? image->h : 0;

	int text_width = text_size.w;
	if(menu_.width() > text_width)
		text_width = menu_.width();

	int total_width = total_image_width + text_width +
	                  padding_width;
	if(button_widths + button_width_padding > total_width)
		total_width = button_widths + button_width_padding;

	if(text_widget_width+left_padding+right_padding > total_width)
		total_width = text_widget_width+left_padding+right_padding;

	const int total_height = (image_height+8 > text_size.h ?
	                          image_height+8 : text_size.h) +
	                         padding_height + button_heights + menu_.height() +
							 text_widget_height + check_button_height;

	int xloc = scr->w/2 - total_width/2;
	int yloc = scr->h/2 - total_height/2;

	int unitx = 0;
	int unity = 0;
	//if we are showing a dialog with unit details, then we have
	//to make more room for it
	if(!units.empty()) {
		xloc += scr->w/10;
		unitx = xloc - 300;
		if(unitx < 10)
			unitx = 10;

		unity = yloc;
	}

	const int button_wpadding = total_width - button_widths;
	int button_offset = 0;
	for(size_t button_num = 0; button_num != buttons.size(); ++button_num) {
		const int padding_amount = button_wpadding/(buttons.size()+1);
		buttons[button_num].set_x(xloc + padding_amount*(button_num+1) +
		                          button_offset);
		buttons[button_num].set_y(yloc + total_height - button_heights);
		button_offset += buttons[button_num].width();
	}

	SDL_Rect dlgr = {xloc-10,yloc-10,total_width+20,total_height+20};

	const surface_restorer restorer(&disp.video(),dlgr);

	draw_dialog_frame(xloc,yloc,total_width,total_height,disp);

	if(menu_.height() > 0)
		menu_.set_loc(xloc+total_image_width+left_padding+image_h_padding,
		              yloc+top_padding+text_size.h+menu_hpadding);

	if(image != NULL) {
		const int x = xloc + left_padding;
		const int y = yloc + top_padding;

		disp.blit_surface(x,y,image);

		int center_font = 0;
		if(caption_size.w < image->w) {
			center_font = image->w/2 - caption_size.w/2;
		}

		font::draw_text(&disp, clipRect, caption_font_size,
		                font::NORMAL_COLOUR, caption,
						xloc+left_padding+center_font,
		                yloc+top_padding+image->h-6, NULL);
	}

	const int unitw = 200;
	const int unith = disp.y()/2;

	font::draw_text(&disp, clipRect, message_font_size,
	                font::NORMAL_COLOUR, message,
					xloc+total_image_width+left_padding+image_h_padding,
					yloc+top_padding);

	const int image_h = image != NULL ? image->h : 0;
	const int text_widget_y = yloc+top_padding+image_h-6+text_size.h+menu_hpadding;

	if(use_textbox) {
		text_widget.set_position(xloc + left_padding +
		                         text_widget_width - text_widget.location().w,
		                         text_widget_y);
		events::raise_draw_event();
		font::draw_text(&disp, clipRect, message_font_size,
		                font::NORMAL_COLOUR, text_widget_label,
						xloc + left_padding,text_widget_y);
	}

	//set the position of any tick boxes. they go right below the menu, slammed against
	//the right side of the dialog
	if(options != NULL) {
		int options_y = text_widget_y + (use_textbox ? text_widget.location().h : 0) + menu_.height() + button_height_padding + menu_hpadding;
		for(size_t i = 0; i != check_buttons.size(); ++i) {
			check_buttons[i].set_x(xloc + total_width - padding_width - check_buttons[i].width());
			check_buttons[i].set_y(options_y);

			options_y += check_buttons[i].height() + button_height_padding;
			check_buttons[i].set_check((*options)[i].checked);
		}
	}

	screen.flip();

	CKey key;

	bool left_button = true, right_button = true, key_down = true,
	     up_arrow = false, down_arrow = false,
	     page_up = false, page_down = false;

	disp.invalidate_all();

	int cur_selection = -1;

	SDL_Rect unit_details_rect;
	unit_details_rect.w = 0;

	bool first_time = true;

	for(;;) {
		events::pump();

		int mousex, mousey;
		const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

		const bool new_right_button = mouse_flags&SDL_BUTTON_RMASK;
		const bool new_left_button = mouse_flags&SDL_BUTTON_LMASK;
		const bool new_key_down = key[SDLK_SPACE] || key[SDLK_RETURN] ||
		                          key[SDLK_ESCAPE];

		const bool new_up_arrow = key[SDLK_UP];
		const bool new_down_arrow = key[SDLK_DOWN];

		const bool new_page_up = key[SDLK_PAGEUP];
		const bool new_page_down = key[SDLK_PAGEDOWN];

		int select_item = -1;
		for(int item = 0; item != 10; ++item) {
			if(key['1' + item])
				select_item = item;
		}

		if((!key_down && key[SDLK_RETURN] || menu_.double_clicked()) &&
		   (type == YES_NO || type == OK_CANCEL || type == OK_ONLY)) {

			if(text_widget_text != NULL && use_textbox)
				*text_widget_text = text_widget.text();

			if(menu_.height() == 0) {
				return 0;
			} else {
				return menu_.selection();
			}
		}

		if(!key_down && key[SDLK_ESCAPE] && type == MESSAGE) {
			return -1;
		}

		if(!key_down && key[SDLK_ESCAPE] &&
		   (type == YES_NO || type == OK_CANCEL)) {

			if(menu_.height() == 0) {
				return 1;
			} else {
				return -1;
			}
		}

		if(menu_.selection() != cur_selection || first_time) {
			cur_selection = menu_.selection();

			int selection = cur_selection;
			if(first_time)
				selection = 0;

			if(size_t(selection) < units.size()) {
				draw_dialog_frame(unitx,unity,unitw,unith,disp);

				SDL_Rect clip_rect = { unitx, unity, unitw, unith };

				disp.draw_unit_details(unitx+left_padding,
				   unity+top_padding+60, gamemap::location(), units[selection],
				   unit_details_rect, unitx+left_padding, unity+top_padding,
				   &clip_rect);
				disp.update_display();
			}
		}

		first_time = false;

		if(menu_.height() > 0) {
			const int res = menu_.process(mousex,mousey,new_left_button,
			                              !up_arrow && new_up_arrow,
						      !down_arrow && new_down_arrow,
			                              !page_up && new_page_up,
			                              !page_down && new_page_down,
			                              select_item);
			if(res != -1)
			{
				return res;	
			}
		}

		up_arrow = new_up_arrow;
		down_arrow = new_down_arrow;
		page_up = new_page_up;
		page_down = new_page_down;

		events::raise_process_event();
		events::raise_draw_event();

		if(buttons.empty() && (new_left_button && !left_button ||
		                       new_right_button && !right_button) ||
		   buttons.size() < 2 && new_key_down && !key_down &&
		   menu_.height() == 0)
			break;

		left_button = new_left_button;
		right_button = new_right_button;
		key_down = new_key_down;	

		for(std::vector<button>::iterator button_it = buttons.begin();
		    button_it != buttons.end(); ++button_it) {
			if(button_it->process(mousex,mousey,left_button)) {
				if(text_widget_text != NULL && use_textbox)
					*text_widget_text = text_widget.text();

				//if the menu is not used, then return the index of the
				//button pressed, otherwise return the index of the menu
				//item selected if the last button is not pressed, and
				//cancel (-1) otherwise
				if(menu_.height() == 0) {
					return button_it - buttons.begin();
				} else if(buttons.size() <= 1 ||
				       size_t(button_it-buttons.begin()) != buttons.size()-1) {
					return menu_.selection();
				} else {
					return -1;
				}
			}
		}

		for(unsigned int n = 0; n != check_buttons.size(); ++n) {
			check_buttons[n].process(mousex,mousey,left_button);
			check_buttons[n].draw();
			(*options)[n].checked = check_buttons[n].checked();
		}

		if(action != NULL) {
			const int act = action->do_action();
			if(act != dialog_action::CONTINUE_DIALOG) {
				return act;
			}
		}

		disp.video().flip();
		SDL_Delay(20);
	}

	return -1;
}

}

namespace {
class dialog_action_receive_network : public gui::dialog_action
{
public:
	dialog_action_receive_network(network::connection connection, config& cfg, const std::pair<int,int>& connection_stats);
	int do_action();
	network::connection result() const;

	enum { CONNECTION_COMPLETE = 1, CONNECTION_CONTINUING = 2 };
private:
	config& cfg_;
	network::connection connection_, res_;
	std::pair<int,int> stats_;
};

dialog_action_receive_network::dialog_action_receive_network(network::connection connection, config& cfg,
															 const std::pair<int,int>& stats)
: cfg_(cfg), connection_(connection), res_(0), stats_(stats)
{
}

int dialog_action_receive_network::do_action()
{
	res_ = network::receive_data(cfg_,connection_,100);
	if(res_ != 0)
		return CONNECTION_COMPLETE;
	else if(network::current_transfer_stats().first != stats_.first) {
		std::cerr << "continuing connection...\n";
		return CONNECTION_CONTINUING;
	} else
		return CONTINUE_DIALOG;
};

network::connection dialog_action_receive_network::result() const
{
	return res_;
}

}

namespace gui {

network::connection network_data_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num)
{
	for(;;) {
		const std::pair<int,int> stats = network::current_transfer_stats();
		std::stringstream str;
		str << msg;
		if(stats.first != -1) {
			str << ": " << (stats.first/1024) << "/" << (stats.second/1024) << string_table["kilobytes"];
		}

		dialog_action_receive_network receiver(connection_num,cfg,stats);
		const int res = gui::show_dialog(disp,NULL,"",str.str(),CANCEL_ONLY,NULL,NULL,"",NULL,&receiver);
		if(res != int(dialog_action_receive_network::CONNECTION_CONTINUING)) {
			return receiver.result();
		}
	}
}

void fade_logo(display& screen, int xpos, int ypos)
{
	const scoped_sdl_surface logo_unscaled(image::get_image(game_config::game_logo,image::UNSCALED));
	const scoped_sdl_surface logo(scale_surface(logo_unscaled,(logo_unscaled->w*screen.x())/1024,(logo_unscaled->h*screen.y())/768));
	if(logo == NULL) {
		std::cerr << "Could not find game logo\n";
		return;
	}

	//only once, when the game is first started, the logo fades in
	static bool faded_in = false;

	if(!faded_in) {
		faded_in = true;
		CKey key;

		bool last_button = key[SDLK_ESCAPE] || key[SDLK_SPACE];

		for(int i = 0; i < 170; i += 5) {
			const bool new_button = key[SDLK_ESCAPE] || key[SDLK_SPACE];
			if(new_button && !last_button)
				break;

			last_button = new_button;

			SDL_SetAlpha(logo,SDL_SRCALPHA,i);
			screen.blit_surface(xpos,ypos,logo);
			update_rect(xpos,ypos,logo->w,logo->h);
			screen.video().flip();
			SDL_Delay(20);
			events::pump();
		}
	}

	SDL_SetAlpha(logo,SDL_SRCALPHA,255);
	screen.blit_surface(xpos,ypos,logo);
	update_rect(xpos,ypos,logo->w,logo->h);
}

TITLE_RESULT show_title(display& screen)
{
	const events::resize_lock prevent_resizing;
	
	const scoped_sdl_surface title_surface_unscaled(image::get_image(game_config::game_title,image::UNSCALED));
	const scoped_sdl_surface title_surface(scale_surface(title_surface_unscaled,screen.x(),screen.y()));

	if(title_surface == NULL) {
		std::cerr << "Could not find title image\n";
	} else {
		screen.blit_surface(0,0,title_surface);
		update_rect(screen.screen_area());
	}

	fade_logo(screen,(game_config::title_logo_x*screen.x())/1024,(game_config::title_logo_y*screen.y())/768);

	const std::string& version_str = string_table["version"] + " " +
	                                 game_config::version;

	const SDL_Rect version_area = font::draw_text(NULL,screen.screen_area(),10,
	                                    font::NORMAL_COLOUR,version_str,0,0);
	const size_t versiony = screen.y() - version_area.h;

	if(versiony < size_t(screen.y())) {
		font::draw_text(&screen,screen.screen_area(),
		                  10,font::NORMAL_COLOUR,version_str,0,versiony);
	}

	const int menu_xbase = (game_config::title_buttons_x*screen.x())/1024;
	const int menu_xincr = 0;
	const int menu_ybase = (game_config::title_buttons_y*screen.y())/768;
	const int menu_yincr = 40;

	//members of this array must correspond to the enumeration TITLE_RESULT
	static const std::string button_labels[] = { "tutorial_button", "campaign_button", "multiplayer_button",
		"load_button", "language_button", "preferences", "about_button", "quit_button" };

	static const size_t nbuttons = sizeof(button_labels)/sizeof(*button_labels);

	std::vector<button> buttons;
	size_t b, max_width = 0;
	for(b = 0; b != nbuttons; ++b) {
		buttons.push_back(button(screen,string_table[button_labels[b]]));
		buttons.back().set_xy(menu_xbase + b*menu_xincr, menu_ybase + b*menu_yincr);
		max_width = maximum<size_t>(max_width,buttons.back().width());
	}

	const size_t padding = 10;
	draw_dialog_frame(menu_xbase-padding,menu_ybase-padding,max_width+padding*2,menu_yincr*(nbuttons-1)+buttons.back().height()+padding*2,screen);

	for(b = 0; b != nbuttons; ++b) {
		buttons.back().draw();
	}

	screen.video().flip();

	CKey key;

	bool last_escape = key[SDLK_ESCAPE];

	update_whole_screen();

	for(;;) {
		int mousex, mousey;
		const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);
		const bool left_button = mouse_flags&SDL_BUTTON_LMASK;

		for(size_t b = 0; b != buttons.size(); ++b) {
			if(buttons[b].process(mousex,mousey,left_button)) {
				return TITLE_RESULT(b);
			}
		}

		if(!last_escape && key[SDLK_ESCAPE])
			return QUIT_GAME;

		last_escape = key[SDLK_ESCAPE];

		screen.video().flip();

		events::pump();

		SDL_Delay(50);
	}

	return QUIT_GAME;
}

} //end namespace gui
