/*
   Copyright (C) 2006 by Patrick Parker <patrick_x99@hotmail.com>
   wesnoth widget Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "construct_dialog.hpp"
#include "config.hpp"
#include "cursor.hpp"
#include "display.hpp"
#include "events.hpp"
#include "gettext.hpp"
#include "help.hpp"
#include "hotkeys.hpp"
#include "image.hpp"
#include "key.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "thread.hpp"
#include "language.hpp"
#include "sdl_utils.hpp"
#include "tooltips.hpp"
#include "util.hpp"
#include "video.hpp"
#include "widgets/button.hpp"
#include "widgets/menu.hpp"
#include "widgets/progressbar.hpp"
#include "widgets/textbox.hpp"
#include "wassert.hpp"

#include "sdl_ttf/SDL_ttf.h"

#include <iostream>
#include <numeric>

#define ERR_DP LOG_STREAM(err, display)
#define LOG_DP LOG_STREAM(info, display)
#define ERR_G  LOG_STREAM(err, general)

namespace gui {

//static initialization
const std::string dialog::default_style("menu");
const std::string dialog::no_help("");
const int dialog::message_font_size = font::SIZE_PLUS;
const int dialog::caption_font_size = font::SIZE_LARGE;
const size_t dialog::left_padding = font::relative_size(10);
const size_t dialog::right_padding = font::relative_size(10);
const size_t dialog::image_h_pad = font::relative_size(/*image_ == NULL ? 0 :*/ 10);
const size_t dialog::top_padding = font::relative_size(10);
const size_t dialog::bottom_padding = font::relative_size(10);

#ifdef USE_TINY_GUI
	const int dialog::max_menu_width = 150;
#else
	const int dialog::max_menu_width = -1;
#endif

}

namespace {

SDL_Rect empty_rect = { 0, 0, 0, 0 };

struct help_handler : public hotkey::command_executor
{
	help_handler(display& disp, const std::string& topic) : disp_(disp), topic_(topic)
	{}

private:
	void show_help()
	{
		if(topic_.empty() == false) {
			help::show_help(disp_,topic_);
		}
	}

	bool can_execute_command(hotkey::HOTKEY_COMMAND cmd) const
	{
		return (topic_.empty() == false && cmd == hotkey::HOTKEY_HELP) || cmd == hotkey::HOTKEY_SCREENSHOT;
	}

	display& disp_;
	std::string topic_;
};

} //end anonymous namespace

namespace gui {

dialog::dialog(display &disp, const std::string& title, const std::string& message,
				const DIALOG_TYPE type, const std::string& dialog_style,
				const std::string& help_topic) : disp_(disp),
				title_(title), message_(message), type_(type), image_(NULL),
				style_(dialog_style), help_button_(disp, help_topic), menu_(NULL),
				text_widget_(NULL), result_(CONTINUE_DIALOG), action_(NULL),
				x_(-1), y_(-1)
{
	switch(type)
	{
	case MESSAGE:
	default:
		break;
	case OK_ONLY:
		add_button(new standard_dialog_button(disp.video(),_("OK"),0,true), BUTTON_STANDARD);
		break;
	case YES_NO:
		add_button(new standard_dialog_button(disp.video(),_("Yes"),0,false), BUTTON_STANDARD);
		add_button(new standard_dialog_button(disp.video(),_("No"),1,true), BUTTON_STANDARD);
		break;
	case OK_CANCEL:
		add_button(new standard_dialog_button(disp.video(),_("OK"),0,false), BUTTON_STANDARD);
		add_button(new standard_dialog_button(disp.video(),_("Cancel"),1,true), BUTTON_STANDARD);
		break;
	case CANCEL_ONLY:
		add_button(new standard_dialog_button(disp.video(),_("Cancel"),0,true), BUTTON_STANDARD);
		break;
	case CLOSE_ONLY:
		add_button(new standard_dialog_button(disp.video(),_("Close"),0,true), BUTTON_STANDARD);
		break;
	}
	//dialog creator should catch(button::error&) ?

	message_rect_ = empty_rect;
	frame_rect_ = empty_rect;
}

dialog::~dialog()
{
	if(menu_ != empty_menu)
	{
		delete menu_;
	}
	delete text_widget_;
	delete image_;
	//delete action_;

	button_pool_iterator b;
	pp_iterator p;
	for (b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		delete b->first;
	}
	for (p = preview_panes_.begin(); p != preview_panes_.end(); ++p) {
		delete (*p);
	}
}

const bool dialog::option_checked(unsigned int option_index)
{
	unsigned int i = 0;
	button_pool_iterator b;
	for (b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		if(b->first->is_option()) {
			if(option_index == i++) {
				return b->first->checked();
			}
		}
	}
	return false;
}

void dialog::add_button(dialog_button *const btn, BUTTON_LOCATION loc)
{
	std::pair<dialog_button *, BUTTON_LOCATION> new_pair(btn,loc);
	button_pool_.push_back(new_pair);
}

void dialog::set_textbox(const std::string& text_widget_label,
				const std::string& text_widget_text,
				const int text_widget_max_chars, const unsigned int text_box_width)
{
	label *label_ptr = new label(disp_.video(), text_widget_label, message_font_size, font::NORMAL_COLOUR, false);
	const bool editable_textbox = std::find(text_widget_text.begin(),text_widget_text.end(),'\n') == text_widget_text.end();
	text_widget_ = new dialog_textbox(label_ptr, disp_.video(), text_box_width, text_widget_text, editable_textbox, text_widget_max_chars);
	text_widget_->set_wrap(!editable_textbox);
}

menu *dialog::get_menu()
{
	if(menu_ == NULL)
	{
		if(empty_menu == NULL) {
			empty_menu = new gui::menu(disp_.video(),empty_string_vector,false,-1,-1,NULL,&menu::simple_style);
			empty_menu->leave();
		}
		menu_ = empty_menu;
	}
	return menu_;
}

int dialog::show(int &xloc, int &yloc)
{
	x_ = xloc;
	y_ = yloc;
	show();
	xloc = x_;
	yloc = y_;
	return result();
}

int dialog::show()
{
	if(disp_.update_locked())
		return CLOSE_DIALOG;

	LOG_DP << "showing dialog '" << title_ << "' '" << message_ << "'\n";

	//create the event context, remember to instruct any passed-in widgets to join it
	const events::event_context outer_context;
	const events::event_context dialog_events_context;
	const dialog_manager manager;
	const events::resize_lock prevent_resizing;

	help_handler helper(disp_,help_button_.topic());
	hotkey::basic_handler help_dispatcher(&disp_,&helper);

	//layout
	layout(x_, y_);

	//draw
	surface_restorer restorer;
	draw(restorer);

	//process
	dialog_process_info dp_info;
	do
	{
		events::pump();
		set_result(process(dp_info));
		if(!done()) {
			refresh();
		}
		action();
	} while(!done());

	return result();
}

void dialog::draw(surface_restorer &restorer)
{
	CVideo& screen = disp_.video();
	surface const scr = screen.getSurface();

	unsigned int image_h_padding = 0;
	unsigned int image_width = 0;
	unsigned int caption_height = 0;
	if(image_ != NULL) {
		image_h_padding = image_h_pad;
		image_width = image_->width();
		caption_height = (image_->caption() == NULL)? 0 : image_->caption()->height();
	}
/*	const std::string& title = image_ == NULL ? caption_ : "";
*/
	std::vector<button*> frame_buttons;
	for(button_iterator b = standard_buttons_.begin(); b != standard_buttons_.end(); ++b)
	{
		frame_buttons.push_back(*b);
	}
	draw_dialog(frame_rect_.x, frame_rect_.y, frame_rect_.w, frame_rect_.h,
		screen, title_, &style_, &frame_buttons, &restorer,
		help_button_.topic().empty() ? NULL : &help_button_);


	//draw textbox, textbox label, image, and image caption
	events::raise_draw_event();
/*
	if(text_widget_ != NULL) {
		events::raise_draw_event();
		font::draw_text(&screen, clipRect, message_font_size,
		                font::NORMAL_COLOUR, text_widget_label_,
						xloc + left_padding,text_widget_y_unpadded);
	}
	if(image_ != NULL) {
		screen.blit_surface(x,y,image_);

		font::draw_text(&screen, clipRect, caption_font_size,
		                font::NORMAL_COLOUR, caption_,
		                xloc+image_width+left_padding+image_h_padding,
		                yloc+top_padding);
	}
*/

	font::draw_text(&screen, message_rect_, message_font_size,
	                font::NORMAL_COLOUR, message_,
	                x_+image_width+left_padding+image_h_padding,
	                y_+top_padding+caption_height);

	disp_.flip();
	disp_.invalidate_all();
}

void dialog::refresh()
{
	disp_.flip();
	disp_.delay(10);
}

void dialog::layout(int &xloc, int &yloc)
{
	if(frame_rect_ != empty_rect) //already finished layout
		return;

	CVideo& screen = disp_.video();
	surface const scr = screen.getSurface();
	SDL_Rect clipRect = disp_.screen_area();

	const bool use_textbox = (text_widget_ != NULL);
	int text_widget_width = 0;
	int text_widget_height = 0;
	if(use_textbox) {
		text_widget_->join();
		const SDL_Rect& area = font::text_area(text_widget_->text(),message_font_size);
		text_widget_->set_width(minimum<size_t>(screen.getx()/2,maximum<size_t>(area.w,text_widget_->width())));
		text_widget_->set_height(minimum<size_t>(screen.gety()/2,maximum<size_t>(area.h,text_widget_->height())));
		text_widget_width = text_widget_->width();
		text_widget_width += (text_widget_->caption() == NULL) ? 0 : text_widget_->caption()->width();
		text_widget_height = text_widget_->height() + message_font_size;
	}

//	const std::vector<std::string> &menu_items =
//		(menu_items_ptr == NULL) ? std::vector<std::string>() : *menu_items_ptr;
//	menu menu_(screen,menu_items,type == MESSAGE,-1,max_menu_width,sorter,menu_style);
	const bool use_menu = (get_menu()->height() > 0);
	if(use_menu)
	{
		menu_->join();
		menu_->set_numeric_keypress_selection(use_textbox == false);
	}

	try {
		message_ = font::word_wrap_text(message_, message_font_size, screen.getx() / 2, screen.gety() / 2);
	} catch(utils::invalid_utf8_exception&) {
		ERR_DP << "Problem handling utf8 in message '" << message_ << "'\n";
	}

	if(!message_.empty()) {
		message_rect_ = font::draw_text(NULL, clipRect, message_font_size,
		                            font::NORMAL_COLOUR, message_, 0, 0);
	}
	unsigned int caption_width = 0;
	unsigned int caption_height = 0;
	if (image_ != NULL && image_->caption() != NULL) {
		caption_width = image_->caption()->width();
		caption_height = image_->caption()->height();
	}

	int check_button_height = 0;
//	int check_button_width = 0;
	int left_check_button_height = 0;
//	int left_check_button_width = 0;
	const int button_height_padding = 5;

	for(button_pool_iterator b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		switch(b->second)
		{
		case BUTTON_EXTRA:
		case BUTTON_CHECKBOX:
			check_button_height += b->first->height() + button_height_padding;
//			check_button_width = maximum<int>(b->first->width(),check_button_width);
			extra_buttons_.push_back(b->first);
			b->first->set_parent(this);
			b->first->join();
			break;
		case BUTTON_CHECKBOX_LEFT:
			left_check_button_height += b->first->height() + button_height_padding;
//			left_check_button_width = maximum<int>(b->first->width(),left_check_button_width);
			extra_buttons_.push_back(b->first);
			b->first->set_parent(this);
			b->first->join();
			break;
		case BUTTON_STANDARD:
			standard_buttons_.push_back(b->first);
		default:
			b->first->set_parent(this);
			b->first->join();
			break;
		}
	}
	check_button_height = maximum<int>(check_button_height, left_check_button_height);

	size_t above_preview_pane_height = 0, above_left_preview_pane_width = 0, above_right_preview_pane_width = 0;
	size_t preview_pane_height = 0, left_preview_pane_width = 0, right_preview_pane_width = 0;
	if(!preview_panes_.empty()) {
		for(pp_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
			(**i).join(); //join the event context now, since it was created outside this scope
			const SDL_Rect& rect = (**i).location();

			if((**i).show_above() == false) {
				preview_pane_height = maximum<size_t>(rect.h,preview_pane_height);
				if((**i).left_side()) {
					left_preview_pane_width += rect.w;
				} else {
					right_preview_pane_width += rect.w;
				}
			} else {
				above_preview_pane_height = maximum<size_t>(rect.h,above_preview_pane_height);
				if((**i).left_side()) {
					above_left_preview_pane_width += rect.w;
				} else {
					above_right_preview_pane_width += rect.w;
				}
			}
		}
	}

	const int menu_hpadding = font::relative_size((message_rect_.h > 0 && use_menu) ? 10 : 0);
	const size_t image_h_padding = (image_ == NULL)? 0 : image_h_pad;
	const size_t padding_width = left_padding + right_padding + image_h_padding;
	const size_t padding_height = top_padding + bottom_padding + menu_hpadding;
	const size_t image_width = (image_ == NULL) ? 0 : image_->width();
	const size_t image_height = (image_ == NULL) ? 0 : image_->height();
	const size_t total_text_height = message_rect_.h + caption_height;

	size_t text_width = message_rect_.w;
	if(caption_width > text_width)
		text_width = caption_width;

	// Prevent the menu to be larger than the screen
	if(menu_->width() + image_width + padding_width > size_t(scr->w))
		menu_->set_width(scr->w - image_width - padding_width);
	if(menu_->width() > text_width)
		text_width = menu_->width();


	size_t total_width = image_width + text_width + padding_width;

	if(text_widget_width+left_padding+right_padding > total_width)
		total_width = text_widget_width+left_padding+right_padding;

	//Prevent the menu from being too skinny
	if(use_menu && preview_panes_.empty() &&
		total_width > menu_->width() + image_width + padding_width) {
		menu_->set_width(total_width - image_width - padding_width);
	}

	const size_t text_and_image_height = image_height > total_text_height ? image_height : total_text_height;

	const int total_height = text_and_image_height +
	                         padding_height + menu_->height() +
							 text_widget_height + check_button_height;


	int frame_width = maximum<int>(total_width,above_left_preview_pane_width + above_right_preview_pane_width);
	int frame_height = maximum<int>(total_height,int(preview_pane_height));
	int xframe = maximum<int>(0,xloc >= 0 ? xloc : scr->w/2 - (frame_width + left_preview_pane_width + right_preview_pane_width)/2);
	int yframe = maximum<int>(0,yloc >= 0 ? yloc : scr->h/2 - (frame_height + above_preview_pane_height)/2);

	LOG_DP << "above_preview_pane_height: " << above_preview_pane_height << "; "
		<< "yframe: " << scr->h/2 << " - " << (frame_height + above_preview_pane_height)/2 << " = "
		<< yframe << "; " << "frame_height: " << frame_height << "\n";

	if(xloc <= -1 || yloc <= -1) {
		xloc = xframe + left_preview_pane_width;
		yloc = yframe + above_preview_pane_height;
	}

	if(xloc + frame_width > scr->w) {
		xloc = scr->w - frame_width;
		if(xloc < xframe) {
			xframe = xloc;
		}
	}

	if(yloc + frame_height > scr->h) {
		yloc = scr->h - frame_height;
		if(yloc < yframe) {
			yframe = yloc;
		}
	}

	frame_width += left_preview_pane_width + right_preview_pane_width;
	frame_height += above_preview_pane_height;

	frame_rect_.x = xframe;
	frame_rect_.y = yframe;
	frame_rect_.w = frame_width;
	frame_rect_.h = frame_height;

	//calculate the positions of the preview panes to the sides of the dialog
	if(!preview_panes_.empty()) {

		int left_preview_pane = xframe;
		int right_preview_pane = xframe + total_width + left_preview_pane_width;
		int above_left_preview_pane = xframe + frame_width/2;
		int above_right_preview_pane = above_left_preview_pane;

		for(pp_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
			SDL_Rect area = (**i).location();

			if((**i).show_above() == false) {
				area.y = yloc;
				if((**i).left_side()) {
					area.x = left_preview_pane;
					left_preview_pane += area.w;
				} else {
					area.x = right_preview_pane;
					right_preview_pane += area.w;
				}
			} else {
				area.y = yframe;
				if((**i).left_side()) {
					area.x = above_left_preview_pane - area.w;
					above_left_preview_pane -= area.w;
				} else {
					area.x = above_right_preview_pane;
					above_right_preview_pane += area.w;
				}
			}

			(**i).set_location(area);
		}
	}

	const int text_widget_y = yloc+top_padding+text_and_image_height-6+menu_hpadding;

	if(use_textbox) {
		const int text_widget_y_unpadded = text_widget_y + (text_widget_height - text_widget_->location().h)/2;
		text_widget_->set_location(xloc + left_padding +
		                         text_widget_width - text_widget_->location().w,
		                         text_widget_y_unpadded);
		if(text_widget_->caption() != NULL) {
			text_widget_->caption()->set_location(xloc+left_padding, text_widget_y_unpadded);
		}
	}

	const int menu_xpos = xloc+image_width+left_padding+image_h_padding;
	const int menu_ypos = yloc+top_padding+text_and_image_height+menu_hpadding+ (use_textbox ? text_widget_->location().h + top_padding : 0);
	if(use_menu) {
		menu_->set_location(menu_xpos,menu_ypos);
	}

	message_rect_.x = xloc + left_padding;
	message_rect_.y = yloc + top_padding + caption_height;
	if(image_ != NULL) {
		const int x = xloc + left_padding;
		const int y = yloc + top_padding;
		message_rect_.x += image_width + image_h_padding;

		image_->set_location(x,y);

		if(image_->caption() != NULL) {
			image_->caption()->set_location(xloc+image_width+left_padding+image_h_padding, yloc+top_padding);
		}
		image_->join();
	}

	//set the position of any tick boxes. by default, they go right below the menu,
	//slammed against the right side of the dialog
	if(extra_buttons_.empty() == false) {
		int options_y = text_widget_y + text_widget_height + menu_->height() + button_height_padding + menu_hpadding;
		int options_left_y = options_y;
		for(button_pool_iterator b = button_pool_.begin(); b != button_pool_.end(); ++b) {
			switch(b->second)
			{
			case BUTTON_EXTRA:
			case BUTTON_CHECKBOX:
				b->first->set_location(xloc + total_width - b->first->width() - ButtonHPadding, options_y);
				options_y += b->first->height() + button_height_padding;
				break;
			case BUTTON_CHECKBOX_LEFT:
				b->first->set_location(xloc + ButtonHPadding, options_left_y);
				options_left_y += b->first->height() + button_height_padding;
				break;
			case BUTTON_STANDARD:
			default:
				break;
			}
		}
	}

/*			if(options != NULL && i < options->size()) {
				check_buttons[i].set_check((*options)[i].checked);
			*/
	help_button_.join();
}

int dialog::process(dialog_process_info &info)
{

	int mousex, mousey;
	const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

	const bool new_right_button = (mouse_flags&SDL_BUTTON_RMASK) != 0;
	const bool new_left_button = (mouse_flags&SDL_BUTTON_LMASK) != 0;
	const bool new_key_down = info.key[SDLK_SPACE] || info.key[SDLK_RETURN] ||
							  info.key[SDLK_ESCAPE];
	const bool use_menu = (get_menu()->height() > 0);

	if((!info.key_down && info.key[SDLK_RETURN] || menu_->double_clicked()) &&
	   (type_ == YES_NO || type_ == OK_CANCEL || type_ == OK_ONLY || type_ == CLOSE_ONLY)) {

/*		if(text_widget_out_ != NULL && use_textbox)
			*text_widget_out_ = text_widget_->text();*/
		return (use_menu ? menu_->selection() : 0);
	}

	if(!info.key_down && info.key[SDLK_ESCAPE] && type_ == MESSAGE) {
		return (ESCAPE_DIALOG);
	}

	//escape quits from the dialog -- unless it's an "ok" dialog with a menu,
	//since such dialogs require a selection of some kind.
	if(!info.key_down && info.key[SDLK_ESCAPE] && (type_ != OK_ONLY || !use_menu)) {
		return ((use_menu) ? 1 : CLOSE_DIALOG);
	}

	if((menu_->selection() != info.selection) || info.first_time) {
		info.selection = menu_->selection();

		int selection = info.selection;
		if(selection < 0) {
			selection = 0;
		}

		if(!preview_panes_.empty()) {
			for(pp_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
				(**i).set_selection(selection);
				if(info.first_time) {
					(**i).set_dirty();
				}
			}
		}
	}

	info.first_time = false;

	if(use_menu) {
		const int selection = menu_->process();
		if(selection != -1)
		{
			return (selection);
		}
	}

	events::raise_process_event();
	events::raise_draw_event();

//	const SDL_Rect menu_rect = {menu_xpos,menu_ypos,menu_->width(),menu_->height()};
	const SDL_Rect menu_rect = menu_->location();
	if(
		(
			standard_buttons_.empty() && 
			(
				(
					new_left_button && !info.left_button &&
					!point_in_rect(mousex,mousey,menu_rect)
				) || (
					new_right_button && !info.right_button
				)
			)
		) || (
			standard_buttons_.size() < 2 && new_key_down && !info.key_down && !use_menu
		)
	  )
	{
		return (CLOSE_DIALOG);
	}

	info.left_button = new_left_button;
	info.right_button = new_right_button;
	info.key_down = new_key_down;

	for(button_pool_iterator b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		if(b->first->pressed()) {
			return b->first->action(info);
		}
	}
	if(help_button_.pressed()) {
		return help_button_.action(info);
	}
	return CONTINUE_DIALOG;
}

int dialog_button::action(dialog_process_info &info) {
	if(handler_ != NULL) {
		menu &menu = *(parent_->get_menu());
		dialog_button_action::RESULT res = handler_->button_pressed(menu.selection());

		if(res == DELETE_ITEM) {
			info.first_time = true;
			menu.erase_item(menu.selection());
			if(menu.nitems() == 0) {
				return CLOSE_DIALOG;
			}
		} else if(res == CLOSE_DIALOG) {
			return CLOSE_DIALOG;
		}

		//reset button-tracking flags so that if the action displays a dialog, a button-press
		//at the end of the dialog won't be mistaken for a button-press in this dialog.
		//(We should eventually use a proper event-handling system instead of tracking
		//flags to avoid problems like this altogether).
		info.clear_buttons();
		return CONTINUE_DIALOG;
	}
	return simple_result_;
}

int standard_dialog_button::action(dialog_process_info &/*info*/) {
	//if the menu is not used, then return the index of the
	//button pressed, otherwise return the index of the menu
	//item selected if the last button is not pressed, and
	//cancel (-1) otherwise
	if(dialog()->get_menu()->height() <= 0) {
		return simple_result_;
	} else if((simple_result_ == 0 && is_last_) || !is_last_) {
		return (dialog()->get_menu()->selection());
	}
	return CLOSE_DIALOG;
}


dialog::help_button::help_button(display& disp, const std::string &help_topic) 
	: dialog_button(disp.video(), _("Help")), disp_(disp), topic_(help_topic)
{}

int dialog::help_button::action(dialog_process_info &info) {
	if(!topic_.empty()) {
		help::show_help(disp_,topic_);
		info.clear_buttons();
	}
	return CONTINUE_DIALOG;
}

void dialog_image::draw_contents()
{
	video().blit_surface(location().x, location().y, surf_);
}

}//end namespace gui
