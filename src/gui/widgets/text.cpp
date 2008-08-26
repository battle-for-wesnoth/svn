/* $Id$ */
/*
   copyright (C) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/widgets/text.hpp"

#include "clipboard.hpp"
#include "gui/widgets/event_handler.hpp"
#include "log.hpp"
#include "serialization/string_utils.hpp"

#define DBG_G LOG_STREAM_INDENT(debug, gui)
#define LOG_G LOG_STREAM_INDENT(info, gui)
#define WRN_G LOG_STREAM_INDENT(warn, gui)
#define ERR_G LOG_STREAM_INDENT(err, gui)

#define DBG_G_D LOG_STREAM_INDENT(debug, gui_draw)
#define LOG_G_D LOG_STREAM_INDENT(info, gui_draw)
#define WRN_G_D LOG_STREAM_INDENT(warn, gui_draw)
#define ERR_G_D LOG_STREAM_INDENT(err, gui_draw)

#define DBG_G_E LOG_STREAM_INDENT(debug, gui_event)
#define LOG_G_E LOG_STREAM_INDENT(info, gui_event)
#define WRN_G_E LOG_STREAM_INDENT(warn, gui_event)
#define ERR_G_E LOG_STREAM_INDENT(err, gui_event)

#define DBG_G_P LOG_STREAM_INDENT(debug, gui_parse)
#define LOG_G_P LOG_STREAM_INDENT(info, gui_parse)
#define WRN_G_P LOG_STREAM_INDENT(warn, gui_parse)
#define ERR_G_P LOG_STREAM_INDENT(err, gui_parse)

namespace gui2 {

void ttext_::mouse_move(tevent_handler&)
{
	DBG_G_E << "Text: mouse move.\n"; 

	// if in select mode select text and move cursor
}

void ttext_::mouse_left_button_down(tevent_handler& event) 
{ 
	DBG_G_E << "Text: left mouse button down.\n"; 

	event.keyboard_capture(this);
	event.mouse_capture();
}

void ttext_::mouse_left_button_up(tevent_handler&) 
{ 
	// reset select  mode
	DBG_G_E << "Text: left mouse button up.\n";
}

void ttext_::mouse_left_button_double_click(tevent_handler&) 
{ 
	DBG_G_E << "Text: left mouse button double click.\n";

	selection_start_ = 0;
	selection_length_ = text_.size();

}

void ttext_::mouse_middle_button_click(tevent_handler&)
{
	DBG_G_E << "Text: middle mouse button click.\n";
#ifdef __unix__
		// pastes on UNIX systems.
		paste_selection(true);
#endif

}

void ttext_::key_press(tevent_handler& /*event*/, 
		bool& handled, SDLKey key, SDLMod modifier, Uint16 unicode)
{
	DBG_G_E << "Text: key press.\n";

// For copy/paste we use a different key on the MAC. Other ctrl modifiers won't 
// be modifed seems not to be required when I read the comment in 
// widgets/textbox.cpp:516. Would be nice if somebody on a MAC would test it.
#ifdef __APPLE__
	const unsigned copypaste_modifier = KMOD_LMETA | KMOD_RMETA;
#else
	const unsigned copypaste_modifier = KMOD_CTRL;
#endif

	switch(key) {

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;

		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;

		case SDLK_a :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}
			
			// If ctrl-a is used for home drop the control modifier
			modifier = static_cast<SDLMod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;
			
		case SDLK_e :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}
			
			// If ctrl-e is used for end drop the control modifier
			modifier = static_cast<SDLMod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;

		case SDLK_BACKSPACE :
			handle_key_backspace(modifier, handled);
			break;

		case SDLK_u :
			if(modifier & KMOD_CTRL) {
				handle_key_clear_line(modifier, handled);
			} else {
				handle_key_default(handled, key, modifier, unicode);
			}
			break;

		case SDLK_DELETE :
			handle_key_delete(modifier, handled);
			break;

		case SDLK_c :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}
			
			// atm we don't care whether there is something to copy or paste
			// if nothing is there we still don't want to be chained.
			copy_selection(false);
			handled = true;
			break;

		case SDLK_x :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}
			
			copy_selection(false);
			delete_selection();
			handled = true;
			break;

		case SDLK_v :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}
			
			paste_selection(false);
			handled = true;
			break;

		default :
			handle_key_default(handled, key, modifier, unicode);

	}
}

void ttext_::set_value(const std::string& text)
{ 
	if(text != text_) { 
		text_ = text; 
		calculate_char_offset(); 

		// default to put the cursor at the end of the buffer.
		selection_start_ = text_.size();
		selection_length_ = 0;
		set_canvas_text();
		set_dirty(); 
	} 
}

void ttext_::set_cursor(const size_t offset, const bool select)
{
	if(select) {

		if(selection_start_ == offset) {
			selection_length_ = 0;
		} else {
			selection_length_ = - (selection_start_ - offset);
		}

#ifdef __unix__
		// selecting copies on UNIX systems.
		copy_selection(true);
#endif
		set_canvas_text();
		set_dirty();

	} else {
		assert(offset <= text_.size());
		selection_start_ = offset;
		selection_length_ = 0;

		set_canvas_text();
		set_dirty();
	}
}

void ttext_::copy_selection(const bool mouse)
{
	int length = selection_length_;
	unsigned start = selection_start_;

	if(length < 0) {
		length = - length;
		start -= length;
	}

	const wide_string& wtext = utils::string_to_wstring(text_);
	const std::string& text = utils::wstring_to_string(
		wide_string(wtext.begin() + start, wtext.begin() + start + length));

	copy_to_clipboard(text, mouse);
}

void ttext_::paste_selection(const bool mouse)
{
	const std::string& text = copy_from_clipboard(mouse);
	if(text.empty()) {
		return;
	}

	delete_selection();

	text_.insert(selection_start_, text);

	selection_start_ += utils::string_to_wstring(text).size();

	calculate_char_offset(); 
	set_canvas_text();
	set_dirty(); 
}

void  ttext_::set_selection_start(const size_t selection_start)
{
	if(selection_start != selection_start_) {
		selection_start_ = selection_start;
		set_dirty();
	}
}

void ttext_::set_selection_length(const unsigned selection_length) 
{ 
	if(selection_length != selection_length_) {
		selection_length_ = selection_length;
		set_dirty();
	}
}

void ttext_::set_state(const tstate state)
{
	if(state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

void ttext_::handle_key_left_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_G_E << "Text: key press: left arrow.\n";

	handled = true;
	const int offset = selection_start_ - 1 + selection_length_;
	if(offset >= 0) {
		set_cursor(offset, modifier & KMOD_SHIFT);
	}
}

void ttext_::handle_key_right_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_G_E << "Text: key press: right arrow.\n";

	handled = true;
	const int offset = selection_start_ + 1 + selection_length_;
	if(offset <= text_.size()) {
		set_cursor(offset, modifier & KMOD_SHIFT);
	}
}

void ttext_::handle_key_home(SDLMod modifier, bool& handled)
{
	DBG_G_E << "Text: key press: home.\n";

	handled = true;
	if(modifier & KMOD_CTRL) {
		goto_start_of_data(modifier & KMOD_SHIFT);
	} else {
		goto_start_of_line(modifier & KMOD_SHIFT);
	}
}

void ttext_::handle_key_end(SDLMod modifier, bool& handled)
{
	DBG_G_E << "Text: key press: end.\n";

	handled = true;
	if(modifier & KMOD_CTRL) {
		goto_end_of_data(modifier & KMOD_SHIFT);
	} else {
		goto_end_of_line(modifier & KMOD_SHIFT);
	}
}

void ttext_::handle_key_backspace(SDLMod /*modifier*/, bool& handled)
{
	DBG_G_E << "Text: key press: backspace.\n";

	handled = true;
	if(selection_start_){
		delete_char(true);
	}
}

void ttext_::handle_key_delete(SDLMod /*modifier*/, bool& handled)
{
	DBG_G_E << "Text: key press: delete.\n";

	handled = true;
	if(selection_length_ != 0) {
		delete_selection();
	} else if (selection_start_ < text_.size()) {
		delete_char(false);
	}
}

void ttext_::handle_key_default(
		bool& handled, SDLKey /*key*/, SDLMod /*modifier*/, Uint16 unicode)
{
	DBG_G_E << "Text: key press: default.\n";

	if(unicode >= 32 && unicode != 127) {
		handled = true;
		insert_char(unicode);
	}
}

} // namespace gui2

