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

#include "gui/widgets/toggle_button.hpp"

#include "foreach.hpp"
#include "gui/widgets/window.hpp"


namespace gui2 {


void ttoggle_button::set_members(const std::map<std::string, t_string>& data)
{
	// Inherit
	tcontrol::set_members(data);

	std::map<std::string, t_string>::const_iterator itor = data.find("icon");
	if(itor != data.end()) {
		set_icon_name(itor->second);
	}
}

void ttoggle_button::mouse_enter(tevent_handler&) 
{ 
	DBG_G_E << "Toggle button: mouse enter.\n"; 

	if(get_value()) {
		set_state(FOCUSSED_SELECTED);
	} else {
		set_state(FOCUSSED);
	}
}

void ttoggle_button::mouse_leave(tevent_handler&) 
{ 
	DBG_G_E << "Toggle button: mouse leave.\n"; 

	if(get_value()) {
		set_state(ENABLED_SELECTED);
	} else {
		set_state(ENABLED);
	}
}

void ttoggle_button::mouse_left_button_click(tevent_handler&) 
{ 
	DBG_G_E << "Toggle button: left mouse button click.\n"; 

	if(get_value()) {
		set_state(ENABLED);
	} else {
		set_state(ENABLED_SELECTED);
	}

	if(callback_state_change_) {
		callback_state_change_(this);
	}
}

void ttoggle_button::mouse_left_button_double_click(tevent_handler&)
{
	DBG_G_E << "Toggle button: left mouse button double click.\n"; 

	assert(retval_ != 0);

	twindow* window = get_window();
	assert(window);

	window->set_retval(retval_);
}

void ttoggle_button::set_active(const bool active)
{
	if(active) {
		if(get_value()) {
			set_state(ENABLED_SELECTED);
		} else {
			set_state(ENABLED);
		}
	} else {
		if(get_value()) {
			set_state(DISABLED_SELECTED);
		} else {
			set_state(DISABLED);
		}
	}
}

void ttoggle_button::update_canvas()
{
	// Inherit.
	tcontrol::update_canvas();

	// set icon in canvases
	std::vector<tcanvas>& canvases = tcontrol::canvas();
	foreach(tcanvas& canvas, canvases) {
		canvas.set_variable("icon", variant(icon_name_));
	}

	set_dirty();
}

void ttoggle_button::set_value(const bool selected)
{
	if(selected == get_value()) {
		return;
	}

	if(selected) {
		set_state(static_cast<tstate>(state_ + ENABLED_SELECTED));
	} else {
		set_state(static_cast<tstate>(state_ - ENABLED_SELECTED));
	}
}

void ttoggle_button::set_retval(const int retval)
{
	if(retval == retval_) {
		return;
	}

	retval_ = retval;
	set_wants_mouse_left_double_click(retval_ != 0);
}

void ttoggle_button::set_state(const tstate state)
{
	if(state != state_) {
		state_ = state;
		set_block_easy_close(get_visible() && get_active());
		set_dirty(true);
	}
}

} // namespace gui2

