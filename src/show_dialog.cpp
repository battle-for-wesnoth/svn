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
#include "cursor.hpp"
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

#include "SDL_ttf.h"

#include <iostream>
#include <numeric>

namespace {
bool is_in_dialog = false;
}

namespace gui {

const int ButtonHPadding = 10;
const int ButtonVPadding = 10;

bool in_dialog() { return is_in_dialog; }

dialog_manager::dialog_manager() : cursor::setter(cursor::NORMAL), reset_to(is_in_dialog)
{
	is_in_dialog = true;
}

dialog_manager::~dialog_manager()
{
	is_in_dialog = reset_to;
}

void draw_dialog_frame(int x, int y, int w, int h, display& disp, const std::string* dialog_style, surface_restorer* restorer)
{
	if(dialog_style == NULL) {
		static const std::string default_style("menu");
		dialog_style = &default_style;
	}

	const scoped_sdl_surface top(image::get_image("misc/" + *dialog_style + "-border-top.png",image::UNSCALED));
	const scoped_sdl_surface bot(image::get_image("misc/" + *dialog_style + "-border-bottom.png",image::UNSCALED));
	const scoped_sdl_surface left(image::get_image("misc/" + *dialog_style + "-border-left.png",image::UNSCALED));
	const scoped_sdl_surface right(image::get_image("misc/" + *dialog_style + "-border-right.png",image::UNSCALED));

	const bool have_border = top != NULL && bot != NULL && left != NULL && right != NULL;

	if(have_border && restorer != NULL) {
		const SDL_Rect rect = {x-top->h,y-left->w,w+left->w+right->w,h+top->h+bot->h};
		*restorer = surface_restorer(&disp.video(),rect);
	} else if(restorer != NULL) {
		const SDL_Rect rect = {x,y,w,h};
		*restorer = surface_restorer(&disp.video(),rect);
	}

	draw_dialog_background(x,y,w,h,disp,*dialog_style);

	if(have_border == false) {
		return;
	}

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

	const scoped_sdl_surface top_left(image::get_image("misc/" + *dialog_style + "-border-topleft.png",image::UNSCALED));
	const scoped_sdl_surface bot_left(image::get_image("misc/" + *dialog_style + "-border-botleft.png",image::UNSCALED));
	const scoped_sdl_surface top_right(image::get_image("misc/" + *dialog_style + "-border-topright.png",image::UNSCALED));
	const scoped_sdl_surface bot_right(image::get_image("misc/" + *dialog_style + "-border-botright.png",image::UNSCALED));
	if(top_left == NULL || bot_left == NULL || top_right == NULL || bot_right == NULL) {
		return;
	}

	disp.blit_surface(x-top_left->w,y-top_left->h,top_left);
	disp.blit_surface(x-bot_left->w,y+h,bot_left);
	disp.blit_surface(x+w,y-top_right->h,top_right);
	disp.blit_surface(x+w,y+h,bot_right);
}

void draw_dialog_background(int x, int y, int w, int h, display& disp, const std::string& style)
{
	const std::string menu_background = "misc/" + style + "-background.png";

	const scoped_sdl_surface bg(image::get_image(menu_background,image::UNSCALED));
	if(bg == NULL) {
		std::cerr << "could not find dialog background '" << style << "'\n";
		return;
	}

	const SDL_Rect& screen_bounds = disp.screen_area();
	if(x < 0) {
		w += x;
		x = 0;
	}

	if(y < 0) {
		h += y;
		y = 0;
	}

	if(x > screen_bounds.w) {
		return;
	}

	if(y > screen_bounds.h) {
		return;
	}

	if(x + w > screen_bounds.w) {
		w = screen_bounds.w - x;
	}

	if(y + h > screen_bounds.h) {
		h = screen_bounds.h - y;
	}

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

SDL_Rect draw_dialog_title(int x, int y, display* disp, const std::string& text)
{
	SDL_Rect rect = {0,0,10000,10000};
	if(disp != NULL) {
		rect = disp->screen_area();
	}

	return font::draw_text(disp,rect,24,font::NORMAL_COLOUR,text,x+5,y+5,NULL,false,font::USE_MARKUP,TTF_STYLE_BOLD);
}

void draw_dialog(int x, int y, int w, int h, display& disp, const std::string& title,
				 const std::string* style, std::vector<button*>* buttons,
				 surface_restorer* restorer)
{
	int border_size = 10;
	SDL_Rect title_area = {0,0,0,0};
	if(title != "") {
		title_area = draw_dialog_title(0,0,NULL,title);
		title_area.w += border_size;
		title_area.h += border_size;
	}

	SDL_Rect buttons_area = {0,0,0,0};

	if(buttons != NULL) {
		for(std::vector<button*>::const_iterator b = buttons->begin(); b != buttons->end(); ++b) {
			buttons_area.w += (**b).width() + ButtonHPadding;
			buttons_area.h = maximum<int>((**b).height() + ButtonVPadding,buttons_area.h);
		}

		buttons_area.x = -buttons_area.w;
		buttons_area.y = y + h;

		buttons_area.w += ButtonHPadding;
	}

	const int xpos = x;
	const int ypos = y - int(title_area.h);
	const int width = maximum<int>(w,maximum<int>(int(title_area.w),int(buttons_area.w)));
	const int height = title_area.h + buttons_area.h + h;

	buttons_area.x += xpos + width;

	draw_dialog_frame(xpos,ypos,width,height,disp,style,restorer);

	if(title != "") {
		draw_dialog_title(x + border_size, y - title_area.h, &disp, title);
	}

	if(buttons != NULL) {
		for(std::vector<button*>::const_iterator b = buttons->begin(); b != buttons->end(); ++b) {
			(**b).set_location(buttons_area.x,buttons_area.y);
			buttons_area.x += (**b).width() + ButtonHPadding;
		}
	}
}

void draw_rectangle(int x, int y, int w, int h, Uint16 colour,SDL_Surface* target)
{
	if(x < 0 || y < 0 || x+w >= target->w || y+h >= target->h) {
		std::cerr << "Rectangle has illegal co-ordinates: " << x << "," << y
		          << "," << w << "," << h << "\n";
		return;
	}

	SDL_Rect top = {x,y,w,1};
	SDL_Rect bot = {x,y+h-1,w,1};
	SDL_Rect left = {x,y,1,h};
	SDL_Rect right = {x+w-1,y,1,h};

	SDL_FillRect(target,&top,colour);
	SDL_FillRect(target,&bot,colour);
	SDL_FillRect(target,&left,colour);
	SDL_FillRect(target,&right,colour);
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

	SDL_Rect rect = {x,y,w,h};
	fill_rect_alpha(rect,SDL_MapRGB(target->format,r,g,b),Uint8(alpha*255),target);
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
                dialog_action* action, std::vector<check_item>* options, int xloc, int yloc,
				const std::string* dialog_style, std::vector<dialog_button>* action_buttons)
{
	if(disp.update_locked())
		return -1;

	std::cerr << "showing dialog '" << caption << "' '" << msg << "'\n";

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

	SDL_Rect clipRect = disp.screen_area();

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
		text_widget_height = text_widget.location().h + 16;
	}

	menu menu_(disp,menu_items,type == MESSAGE);

	const int border_size = 6;

	const int max_line_length = 54;

	std::string message = msg;
	const size_t longest_line = text_to_lines(message,max_line_length);

	SDL_Rect text_size = { 0, 0, 0, 0 };
	if(!message.empty()) {
		text_size = font::draw_text(NULL, clipRect, message_font_size,
						            font::NORMAL_COLOUR, message, 0, 0, NULL);
	}

	SDL_Rect caption_size = { 0, 0, 0, 0 };
	if(caption != "" && image != NULL) {
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

		case CLOSE_ONLY: {
			static const std::string thebuttons[] = { "close_button", "" };
			button_list = thebuttons;
			break;
		}
	}

	if(button_list != NULL) {
		try {
			while(button_list->empty() == false) {
				buttons.push_back(button(disp,string_table[*button_list]));

				++button_list;
			}

		} catch(button::error&) {
			std::cerr << "error initializing button!\n";
		}
	}

	int check_button_height = 0;
	int check_button_width = 0;
	const int button_height_padding = 5;

	std::vector<button> check_buttons;
	if(options != NULL) {
		for(std::vector<check_item>::const_iterator i = options->begin(); i != options->end(); ++i) {
			button check_button(disp,i->label,button::TYPE_CHECK);
			check_button_height += check_button.height() + button_height_padding;
			check_button_width = maximum<int>(check_button.width(),check_button_width);

			check_buttons.push_back(check_button);
		}
	}

	if(action_buttons != NULL) {
		for(std::vector<dialog_button>::const_iterator i = action_buttons->begin(); i != action_buttons->end(); ++i) {
			button new_button(disp,i->label);
			check_button_height += new_button.height() + button_height_padding;
			check_button_width = maximum<int>(new_button.width(),check_button_width);

			check_buttons.push_back(new_button);
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
	const int total_image_height = image_height + caption_size.h;

	int text_width = text_size.w;
	if(menu_.width() > text_width)
		text_width = menu_.width();

	int total_width = total_image_width + text_width + padding_width;

	if(text_widget_width+left_padding+right_padding > total_width)
		total_width = text_widget_width+left_padding+right_padding;

	const int total_height = (total_image_height > text_size.h ?
	                          total_image_height : text_size.h) +
	                         padding_height + menu_.height() +
							 text_widget_height + check_button_height;

	if(xloc <= -1 || yloc <= -1) {
		xloc = scr->w/2 - total_width/2;
		yloc = scr->h/2 - total_height/2;
	}

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

	const int border_padding = 10;
	const int frame_width = total_width + border_padding*2;
	const int frame_height = total_height + border_padding*2;

	if(xloc + frame_width > scr->w) {
		xloc = scr->w - frame_width;
	}

	if(yloc + frame_height > scr->h) {
		yloc = scr->h - frame_height;
	}

	std::vector<button*> buttons_ptr;
	for(std::vector<button>::iterator bt = buttons.begin(); bt != buttons.end(); ++bt) {
		buttons_ptr.push_back(&*bt);
	}

	surface_restorer restorer;

	const std::string& title = image == NULL ? caption : "";
	draw_dialog(xloc,yloc,total_width,total_height,disp,title,dialog_style,&buttons_ptr,&restorer);

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
		                yloc+top_padding+image->h, NULL);
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
		const int text_widget_y_unpadded = text_widget_y + (text_widget_height - text_widget.location().h)/2;
		text_widget.set_location(xloc + left_padding +
		                         text_widget_width - text_widget.location().w,
		                         text_widget_y_unpadded);
		events::raise_draw_event();
		font::draw_text(&disp, clipRect, message_font_size,
		                font::NORMAL_COLOUR, text_widget_label,
						xloc + left_padding,text_widget_y_unpadded);
	}

	//set the position of any tick boxes. they go right below the menu, slammed against
	//the right side of the dialog
	if(check_buttons.empty() == false) {
		int options_y = text_widget_y + text_widget_height + menu_.height() + button_height_padding + menu_hpadding;
		for(size_t i = 0; i != check_buttons.size(); ++i) {
			check_buttons[i].set_location(xloc + total_width - padding_width - check_buttons[i].width(),options_y);

			options_y += check_buttons[i].height() + button_height_padding;

			if(options != NULL && i < options->size()) {
				check_buttons[i].set_check((*options)[i].checked);
			}
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
		   (type == YES_NO || type == OK_CANCEL || type == OK_ONLY || type == CLOSE_ONLY)) {

			if(text_widget_text != NULL && use_textbox)
				*text_widget_text = text_widget.text();

			if(menu_.height() == 0) {
				return 0;
			} else {
				return menu_.selection();
			}
		}

		if(!key_down && key[SDLK_ESCAPE] && type == MESSAGE) {
			return ESCAPE_DIALOG;
		}

		//escape quits from the dialog -- unless it's an "ok" dialog with a menu,
		//since such dialogs require a selection of some kind.
		if(!key_down && key[SDLK_ESCAPE] && (type != OK_ONLY || menu_.height() == 0)) {

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
			const bool pressed = check_buttons[n].process(mousex,mousey,left_button);
			check_buttons[n].draw();

			if(options != NULL && n < options->size()) {
				(*options)[n].checked = check_buttons[n].checked();
			} else if(pressed) {
				const size_t options_size = options == NULL ? 0 : options->size();
				assert(action_buttons != NULL && action_buttons->size() > n - options_size);

				const dialog_button_action::RESULT res = (*action_buttons)[n - options_size].handler->button_pressed(menu_.selection());
				if(res == dialog_button_action::DELETE_ITEM) {
					menu_.erase_item(menu_.selection());
					if(menu_.nitems() == 0) {
						return -1;
					}
				}
			}
		}

		if(action != NULL) {
			const int act = action->do_action();
			if(act != dialog_action::CONTINUE_DIALOG) {
				return act;
			}
		}

		disp.video().flip();
		SDL_Delay(10);
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
	cfg.clear();
	for(;;) {
		const std::pair<int,int> stats = network::current_transfer_stats();
		std::stringstream str;
		str << msg;
		if(stats.first != -1) {
			str << ": " << (stats.first/1024) << "/" << (stats.second/1024) << string_table["kilobytes"];
		}

		dialog_action_receive_network receiver(connection_num,cfg,stats);
		const int res = show_dialog(disp,NULL,"",str.str(),CANCEL_ONLY,NULL,NULL,"",NULL,&receiver);
		if(res != int(dialog_action_receive_network::CONNECTION_CONTINUING)) {
			return receiver.result();
		}
	}
}

} //end namespace gui
