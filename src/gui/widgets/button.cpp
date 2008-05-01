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

#include "gui/widgets/button.hpp"

#include "gui/widgets/window.hpp"
#include "log.hpp"

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


void tbutton::mouse_enter(tevent_handler&) 
{ 
	DBG_G_E << "Button: mouse enter.\n"; 

	set_state(FOCUSSED);
}

void tbutton::mouse_leave(tevent_handler&) 
{ 
	DBG_G_E << "Button: mouse leave.\n"; 

	set_state(ENABLED);
}

void tbutton::mouse_left_button_down(tevent_handler& event) 
{ 
	DBG_G_E << "Button: left mouse button down.\n"; 

	event.mouse_capture();

	set_state(PRESSED);
}

void tbutton::mouse_left_button_up(tevent_handler&) 
{ 
	DBG_G_E << "Button: left mouse button up.\n";

	set_state(FOCUSSED);
}

void tbutton::mouse_left_button_click(tevent_handler&) 
{ 
	DBG_G_E << "Button: left mouse button click.\n"; 

	// If a button has a retval do the default handling.
	if(retval_ != 0) {
		twindow* window = get_window();
		if(window) {
			window->set_retval(retval_);
			return;
		}
	}

	// Do the custom handling (not implemented yet) FIXME
}

tbutton::RETVAL tbutton::get_retval_by_id(const std::string& id)
{
	//! Note it might change to a map later depending on the number
	//! of items.
	if(id == "ok") {
		return OK;
	} else if(id == "cancel") {
		return CANCEL;
	} else {
		return NONE;
	}
}

void tbutton::set_active(const bool active)
{
	if(active && state_ == DISABLED) {
		set_state(ENABLED);
	} else if(!active && state_ != DISABLED) {
		set_state(DISABLED);
	}
}

bool tbutton::get_active() const
{
	return state_ != DISABLED;
}

void tbutton::set_state(tstate state)
{
	if(state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

} // namespace gui2
