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
	const int dialog::max_menu_width = 300;
#else
	const int dialog::max_menu_width = -1;
#endif

}

namespace {

std::vector<std::string> empty_string_vector;

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

	bool can_execute_command(hotkey::HOTKEY_COMMAND cmd, int index=-1) const
	{
		return (topic_.empty() == false && cmd == hotkey::HOTKEY_HELP) || cmd == hotkey::HOTKEY_SCREENSHOT;
	}

	display& disp_;
	std::string topic_;
};

} //end anonymous namespace

namespace gui {

dialog::dimension_measurements::dimension_measurements() :
	x(-1), y(-1), frame(empty_rect), message(empty_rect), textbox(empty_rect)
{
	//note: this is not defined in the header file to C++ ODR (one-definition rule)
	//since each inclusion of the header file uses a different version of empty_rect 
	//(unnamed namespace and/or const object defined at declaration time). 
} 

dialog::dialog(display &disp, const std::string& title, const std::string& message,
				const DIALOG_TYPE type, const std::string& dialog_style,
				const std::string& help_topic) : disp_(disp), image_(NULL),
				title_(title), style_(dialog_style), message_(NULL),
				type_(type), menu_(NULL),
				help_button_(disp, help_topic),  text_widget_(NULL),
				action_(NULL), bg_restore_(NULL), result_(CONTINUE_DIALOG)
{
	CVideo& screen = disp_.video();

	help_button_.set_parent(this);
	switch(type)
	{
	case MESSAGE:
	default:
		break;
	case OK_ONLY:
		add_button(new standard_dialog_button(screen,_("OK"),0,true), BUTTON_STANDARD);
		break;
	case YES_NO:
		add_button(new standard_dialog_button(screen,_("Yes"),0,false), BUTTON_STANDARD);
		add_button(new standard_dialog_button(screen,_("No"),1,true), BUTTON_STANDARD);
		break;
	case OK_CANCEL:
		add_button(new standard_dialog_button(screen,_("OK"),0,false), BUTTON_STANDARD);
		add_button(new standard_dialog_button(screen,_("Cancel"),1,true), BUTTON_STANDARD);
		break;
	case CANCEL_ONLY:
		add_button(new standard_dialog_button(screen,_("Cancel"),0,true), BUTTON_STANDARD);
		break;
	case CLOSE_ONLY:
		add_button(new standard_dialog_button(screen,_("Close"),0,true), BUTTON_STANDARD);
		break;
	}
	//dialog creator should catch(button::error&) ?

	try {
		std::string msg = font::word_wrap_text(message, message_font_size, screen.getx() / 2, screen.gety() / 2);
		message_ = new label(screen, msg, message_font_size, font::NORMAL_COLOUR, false);
	} catch(utils::invalid_utf8_exception&) {
		ERR_DP << "Problem handling utf8 in message '" << message << "'\n";
		throw;
	}
}

dialog::~dialog()
{
	if(menu_ != empty_menu)
	{
		delete menu_;
	}
	delete text_widget_;
	delete image_;
//	delete action_;

	button_pool_iterator b;
	for (b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		delete b->first;
	}
//	pp_iterator p;
//	for (p = preview_panes_.begin(); p != preview_panes_.end(); ++p) {
//		delete (*p);
//	}
	delete bg_restore_;
	delete message_;
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
	switch(loc)
	{
	case BUTTON_EXTRA:
	case BUTTON_CHECKBOX:
	case BUTTON_CHECKBOX_LEFT:
		extra_buttons_.push_back(btn);
		btn->set_parent(this);
		break;
	case BUTTON_STANDARD:
		standard_buttons_.push_back(btn);
	default:
		btn->set_parent(this);
		break;
	}
}

void dialog::add_button(dialog_button_info btn_info, BUTTON_LOCATION loc)
{
	dialog_button *btn = new dialog_button(disp_.video(), btn_info.label, button::TYPE_PRESS, CONTINUE_DIALOG, btn_info.handler);
	add_button(btn, loc);
}

void dialog::add_option(const std::string& label, bool checked, BUTTON_LOCATION loc)
{
	gui::dialog_button *btn = new dialog_button(disp_.video(), label, button::TYPE_CHECK);
	btn->set_check(checked);
	add_button(btn, loc);
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

void dialog::set_menu(const std::vector<std::string> &menu_items)
{
	set_menu(new gui::menu(disp_.video(), menu_items, (type_==MESSAGE), -1,
		dialog::max_menu_width, NULL, &menu::default_style, false));
}

menu& dialog::get_menu() const
{
	if(menu_ == NULL)
	{
		if(empty_menu == NULL) {
			empty_menu = new gui::menu(disp_.video(),empty_string_vector,false,-1,-1,NULL,&menu::simple_style);
			empty_menu->leave();
		}
		menu_ = empty_menu; //no menu, so fake it
	}
	return *menu_;
}

int dialog::show(int xloc, int yloc)
{
	dimension_measurements dim = layout(xloc, yloc);
	return show(dim);
}

int dialog::show(const dimension_measurements &dim)
{
	if(disp_.update_locked()) {
		ERR_DP << "display locked ignoring dialog '" << title_ << "' '" << message_->get_text() << "'\n";
		return CLOSE_DIALOG;
	}

	LOG_DP << "showing dialog '" << title_ << "' '" << message_->get_text() << "'\n";

	//create the event context, remember to instruct any passed-in widgets to join it
	const events::event_context dialog_events_context;
	const dialog_manager manager;
	const events::resize_lock prevent_resizing;

	help_handler helper(disp_,help_button_.topic());
	hotkey::basic_handler help_dispatcher(&disp_,&helper);

	//draw
	draw_frame(dim);
	update_widget_positions(dim);
	draw_contents(dim);

	//process
	dialog_process_info dp_info;
	do
	{
		events::pump();
		set_result(process(dp_info));
		if(!done()) {
			refresh();
		}
		action(dp_info);
	} while(!done());

	return result();
}

void dialog::draw_contents(const dimension_measurements & /*dim*/)
{
	if(!preview_panes_.empty()) {
		for(pp_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
			preview_pane *pane = *i;
			if(!pane->handler_members().empty())
			{
				pane->draw();
				pane->needs_restore_ = false; //prevent panes from drawing over members
			}
		}
	}
	events::raise_draw_event(); //draw widgets

	disp_.flip();
	disp_.invalidate_all();
}

void dialog::draw_frame(const dimension_measurements &dim)
{
	CVideo& screen = disp_.video();
	std::vector<button*> frame_buttons;
	for(button_iterator b = standard_buttons_.begin(); b != standard_buttons_.end(); ++b)
	{
		frame_buttons.push_back(*b);
	}
	bg_restore_ = new surface_restorer;
	draw_dialog(dim.frame.x, dim.frame.y, dim.frame.w, dim.frame.h,
		screen, title_, &style_, &frame_buttons, bg_restore_,
		help_button_.topic().empty() ? NULL : &help_button_);
}

void dialog::update_widget_positions(const dimension_measurements &dim)
{
	if(!preview_panes_.empty()) {
		for(pp_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
			preview_pane *pane = *i;
			pane->join();
			pane->set_location(dim.panes.find(pane)->second);
		}
	}
	if(text_widget_) {
		text_widget_->join();
		text_widget_->set_location(dim.textbox);
		if(text_widget_->get_label()) {
			text_widget_->get_label()->set_location(dim.label_x, dim.label_y);
		}
	}
	if(get_menu().height() > 0) {
		menu_->join();
		menu_->set_numeric_keypress_selection(text_widget_ == NULL);
		menu_->set_width( dim.menu_width );
		menu_->set_max_width( dim.menu_width ); //lock the menu width
		menu_->set_location( dim.menu_x, dim.menu_y );
	}
	if(image_) {
		image_->join();
		image_->set_location(dim.image_x, dim.image_y);
		if(image_->caption()) {
			image_->caption()->set_location(dim.caption_x, dim.caption_y);
		}
	}
	button_iterator b;
	for(b = extra_buttons_.begin(); b != extra_buttons_.end(); ++b) {
		dialog_button *btn = *b;
		btn->join();
		std::pair<int,int> coords = dim.buttons.find(btn)->second;
		btn->set_location(coords.first, coords.second);
	}
	for(b = standard_buttons_.begin(); b != standard_buttons_.end(); ++b) {
		dialog_button *btn = *b;
		btn->join();
	}
	help_button_.join();

	message_->set_location(dim.message);
	message_->join();
}

void dialog::refresh()
{
	disp_.flip();
	disp_.delay(10);
}

dialog::dimension_measurements dialog::layout(int xloc, int yloc) const
{
	CVideo& screen = disp_.video();
	surface const scr = screen.getSurface();

	dimension_measurements dim;
	dim.x = xloc;
	dim.y = yloc;

	const bool use_textbox = (text_widget_ != NULL);
	int text_widget_width = 0;
	int text_widget_height = 0;
	if(use_textbox) {
		const SDL_Rect& area = font::text_area(text_widget_->text(),message_font_size);
		dim.textbox.w = minimum<size_t>(screen.getx()/2,maximum<size_t>(area.w,text_widget_->width()));
		dim.textbox.h = minimum<size_t>(screen.gety()/2,maximum<size_t>(area.h,text_widget_->height()));
		text_widget_width = dim.textbox.w;
		text_widget_width += (text_widget_->get_label() == NULL) ? 0 : text_widget_->get_label()->width();
		text_widget_height = dim.textbox.h + message_font_size;
	}

	const bool use_menu = (get_menu().height() > 0);
	if(!message_->get_text().empty()) {
		dim.message.w = message_->width();
		dim.message.h = message_->height();
	}
	unsigned int caption_width = 0;
	unsigned int caption_height = 0;
	if (image_ != NULL && image_->caption() != NULL) {
		caption_width = image_->caption()->width();
		caption_height = image_->caption()->height();
	}

	int check_button_height = 0;
	int left_check_button_height = 0;
	const int button_height_padding = 5;

	for(button_pool_const_iterator b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		dialog_button const *const btn = b->first;
		switch(b->second)
		{
		case BUTTON_EXTRA:
		case BUTTON_CHECKBOX:
			check_button_height += btn->height() + button_height_padding;
			break;
		case BUTTON_CHECKBOX_LEFT:
			left_check_button_height += btn->height() + button_height_padding;
			break;
		case BUTTON_STANDARD:
		default:
			break;
		}
	}
	check_button_height = maximum<int>(check_button_height, left_check_button_height);

	size_t above_preview_pane_height = 0, above_left_preview_pane_width = 0, above_right_preview_pane_width = 0;
	size_t preview_pane_height = 0, left_preview_pane_width = 0, right_preview_pane_width = 0;
	if(!preview_panes_.empty()) {
		for(pp_const_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
			preview_pane const *const pane = *i;
			const SDL_Rect& rect = pane->location();
			if(pane->show_above() == false) {
				preview_pane_height = maximum<size_t>(rect.h,preview_pane_height);
				if(pane->left_side()) {
					left_preview_pane_width += rect.w;
				} else {
					right_preview_pane_width += rect.w;
				}
			} else {
				above_preview_pane_height = maximum<size_t>(rect.h,above_preview_pane_height);
				if(pane->left_side()) {
					above_left_preview_pane_width += rect.w;
				} else {
					above_right_preview_pane_width += rect.w;
				}
			}
		}
	}

	const int menu_hpadding = font::relative_size((dim.message.h > 0 && use_menu) ? 10 : 0);
	const size_t image_h_padding = (image_ == NULL)? 0 : image_h_pad;
	const size_t padding_width = left_padding + right_padding + image_h_padding;
	const size_t padding_height = top_padding + bottom_padding + menu_hpadding;
	const size_t image_width = (image_ == NULL) ? 0 : image_->width();
	const size_t image_height = (image_ == NULL) ? 0 : image_->height();
	const size_t total_text_height = dim.message.h + caption_height;

	size_t text_width = dim.message.w;
	if(caption_width > text_width)
		text_width = caption_width;

	// Prevent the menu to be larger than the screen
	dim.menu_width = menu_->width();
	if(dim.menu_width + image_width + padding_width > size_t(scr->w))
		dim.menu_width = scr->w - image_width - padding_width;
	if(dim.menu_width > text_width)
		text_width = dim.menu_width;


	size_t total_width = image_width + text_width + padding_width;

	if(text_widget_width+left_padding+right_padding > total_width)
		total_width = text_widget_width+left_padding+right_padding;

	//Prevent the menu from being too skinny
	if(use_menu && preview_panes_.empty() &&
		total_width > dim.menu_width + image_width + padding_width) {
		dim.menu_width = total_width - image_width - padding_width;
	}

	const size_t text_and_image_height = image_height > total_text_height ? image_height : total_text_height;

	const int total_height = text_and_image_height +
	                         padding_height + menu_->height() +
							 text_widget_height + check_button_height;

	dim.frame.w = maximum<int>(total_width,above_left_preview_pane_width + above_right_preview_pane_width);
	dim.frame.h = maximum<int>(total_height,int(preview_pane_height));
	dim.frame.x = maximum<int>(0,dim.x >= 0 ? dim.x : scr->w/2 - (dim.frame.w + left_preview_pane_width + right_preview_pane_width)/2);
	dim.frame.y = maximum<int>(0,dim.y >= 0 ? dim.y : scr->h/2 - (dim.frame.h + above_preview_pane_height)/2);

	LOG_DP << "above_preview_pane_height: " << above_preview_pane_height << "; "
		<< "dim.frame.y: " << scr->h/2 << " - " << (dim.frame.h + above_preview_pane_height)/2 << " = "
		<< dim.frame.y << "; " << "dim.frame.h: " << dim.frame.h << "\n";

	if(dim.x <= -1 || dim.y <= -1) {
		dim.x = dim.frame.x + left_preview_pane_width;
		dim.y = dim.frame.y + above_preview_pane_height;
	}

	if(dim.x + dim.frame.w > scr->w) {
		dim.x = scr->w - dim.frame.w;
		if(dim.x < dim.frame.x) {
			dim.frame.x = dim.x;
		}
	}

	if(dim.y + dim.frame.h > scr->h) {
		dim.y = scr->h - dim.frame.h;
		if(dim.y < dim.frame.y) {
			dim.frame.y = dim.y;
		}
	}

	dim.frame.w += left_preview_pane_width + right_preview_pane_width;
	dim.frame.h += above_preview_pane_height;

	dim.message.x = dim.x + image_width + left_padding + image_h_padding;
	dim.message.y = dim.y + top_padding + caption_height;

	//calculate the positions of the preview panes to the sides of the dialog
	if(!preview_panes_.empty()) {

		int left_preview_pane = dim.frame.x;
		int right_preview_pane = dim.frame.x + total_width + left_preview_pane_width;
		int above_left_preview_pane = dim.frame.x + dim.frame.w/2;
		int above_right_preview_pane = above_left_preview_pane;

		for(pp_const_iterator i = preview_panes_.begin(); i != preview_panes_.end(); ++i) {
		preview_pane const *const pane = *i;
			SDL_Rect area = pane->location();

			if(pane->show_above() == false) {
				area.y = dim.y;
				if(pane->left_side()) {
					area.x = left_preview_pane;
					left_preview_pane += area.w;
				} else {
					area.x = right_preview_pane;
					right_preview_pane += area.w;
				}
			} else {
				area.y = dim.frame.y;
				if(pane->left_side()) {
					area.x = above_left_preview_pane - area.w;
					above_left_preview_pane -= area.w;
				} else {
					area.x = above_right_preview_pane;
					above_right_preview_pane += area.w;
				}
			}
			dim.panes[*i] = area;
		}
	}

	const int text_widget_y = dim.y+top_padding+text_and_image_height-6+menu_hpadding;

	if(use_textbox) {
		dim.textbox.x = dim.x + left_padding + text_widget_width - dim.textbox.w;
		dim.textbox.y = text_widget_y + (text_widget_height - dim.textbox.h)/2;
		dim.label_x = dim.x+left_padding;
		dim.label_y = dim.textbox.y;
	}

	dim.menu_x = dim.x+image_width+left_padding+image_h_padding;
	dim.menu_y = dim.y+top_padding+text_and_image_height+menu_hpadding+ (use_textbox ? text_widget_->location().h + top_padding : 0);

	dim.message.x = dim.x + left_padding;
	dim.message.y = dim.y + top_padding + caption_height;

	if(image_ != NULL) {
		const int x = dim.x + left_padding;
		const int y = dim.y + top_padding;
		dim.message.x += image_width + image_h_padding;
		dim.image_x = x;
		dim.image_y = y;
		dim.caption_x = dim.x + image_width + left_padding + image_h_padding;
		dim.caption_y = dim.y + top_padding;
	}

	//set the position of any tick boxes. by default, they go right below the menu,
	//slammed against the right side of the dialog
	if(extra_buttons_.empty() == false) {
		int options_y = text_widget_y + text_widget_height + menu_->height() + button_height_padding + menu_hpadding;
		int options_left_y = options_y;
		for(button_pool_const_iterator b = button_pool_.begin(); b != button_pool_.end(); ++b) {
		dialog_button const *const btn = b->first;
		std::pair<int,int> coords;
			switch(b->second)
			{
			case BUTTON_EXTRA:
			case BUTTON_CHECKBOX:
				coords.first = dim.x + total_width - btn->width() - ButtonHPadding;
				coords.second = options_y;
				dim.buttons[b->first] = coords;
				options_y += btn->height() + button_height_padding;
				break;
			case BUTTON_CHECKBOX_LEFT:
				coords.first = dim.x + ButtonHPadding;
				coords.second = options_left_y;
				dim.buttons[b->first] = coords;
				options_left_y += btn->height() + button_height_padding;
				break;
			case BUTTON_STANDARD:
			default:
				break;
			}
		}
	}
	return dim;
}

int dialog::process(dialog_process_info &info)
{

	int mousex, mousey;
	int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

	const bool new_right_button = (mouse_flags&SDL_BUTTON_RMASK) != 0;
	const bool new_left_button = (mouse_flags&SDL_BUTTON_LMASK) != 0;
	const bool new_key_down = info.key[SDLK_SPACE] || info.key[SDLK_RETURN] ||
					info.key[SDLK_ESCAPE] || info.key[SDLK_KP_ENTER];
	info.double_clicked = menu_->double_clicked();
	get_menu();
	const bool use_menu = (menu_ != empty_menu);

	if((!info.key_down && info.key[SDLK_RETURN] || info.key[SDLK_KP_ENTER] || info.double_clicked) &&
	   (type_ == YES_NO || type_ == OK_CANCEL || type_ == OK_ONLY || type_ == CLOSE_ONLY)) {

		return (use_menu ? menu_->selection() : 0);
	}

	//escape quits from the dialog -- unless it's an "ok" dialog with a menu,
	//since such dialogs require a selection of some kind.
	if(!info.key_down && info.key[SDLK_ESCAPE]) {
		if(type_ == MESSAGE) {
			//special return value for escaping game event messages
			return (ESCAPE_DIALOG);
		} else if(type_ != OK_ONLY || !use_menu) {
			return ((type_ == OK_ONLY && use_menu) ? 1 : CLOSE_DIALOG);
		}
	}

	//inform preview panes when there is a new menu selection
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
		//get any drop-down choice or context-menu click
		const int selection = menu_->process();
		if(selection != -1)
		{
			return (selection);
		}
	}

	events::raise_process_event();
	events::raise_draw_event();

	const SDL_Rect menu_rect = menu_->location();
	if(
		(
			//clicking outside of a drop-down or context-menu should close it
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
			//any keypress should close a dialog if it has one standard button (or less)
			//and no menu options.
			standard_buttons_.size() < 2 && new_key_down && !info.key_down && !use_menu
		)
	  )
	{
		return (CLOSE_DIALOG);
	}

	info.left_button = new_left_button;
	info.right_button = new_right_button;
	info.key_down = new_key_down;

	//now handle any button presses
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
		menu &menu_ref = parent_->get_menu();
		dialog_button_action::RESULT res = handler_->button_pressed(menu_ref.selection());

		if(res == DELETE_ITEM || res == CLOSE_DIALOG) {
			return res;
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

void dialog::action(dialog_process_info& info)
{
	//default way of handling a "delete item" request
	if(result() == DELETE_ITEM) {
		menu &menu_ref = get_menu();
		menu_ref.erase_item(menu_ref.selection());
		if(menu_ref.nitems() == 0) {
			set_result(CLOSE_DIALOG);
		} else {
			set_result(CONTINUE_DIALOG);
			info.first_time = true;
		}
	}

	//support for old-style dialog actions
	if(!done() && action_ != NULL) {
		set_result(action_->do_action());
	}
}

int standard_dialog_button::action(dialog_process_info &/*info*/) {
	//if the menu is not used, then return the index of the
	//button pressed, otherwise return the index of the menu
	//item selected if the last button is not pressed, and
	//cancel (-1) otherwise
	if(dialog()->get_menu().height() <= 0) {
		return simple_result_;
	} else if((simple_result_ == 0 && is_last_) || !is_last_) {
		return (dialog()->get_menu().selection());
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
void dialog::set_image(surface surf, const std::string &caption)
{
	label *label_ptr = NULL;
	if(!caption.empty()) {
		label_ptr = new label(disp_.video(), caption, caption_font_size, font::NORMAL_COLOUR, false);
	}
	set_image( new dialog_image(label_ptr, disp_.video(), surf ));
}

void dialog_image::draw_contents()
{
	video().blit_surface(location().x, location().y, surf_);
}

}//end namespace gui
