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

#ifndef __GUI_WIDGETS_TOGGLE_BUTTON_HPP_INCLUDED__
#define __GUI_WIDGETS_TOGGLE_BUTTON_HPP_INCLUDED__

#include "gui/widgets/control.hpp"

namespace gui2 {

// Class for a toggle button
class ttoggle_button : public tcontrol, public tselectable_
{
public:
	ttoggle_button() : 
		tcontrol(COUNT),
		state_(ENABLED),
		callback_mouse_left_click_(0)
	{
		load_config();
	}

	void mouse_enter(tevent_handler&);
	void mouse_leave(tevent_handler&);

	void mouse_left_button_click(tevent_handler&);

	void set_active(const bool active);
	bool get_active() const
		{ return state_ != DISABLED && state_ != DISABLED_SELECTED; }
	unsigned get_state() const { return state_; }

	/** Inherited from tselectable_ */
	bool is_selected() const { return state_ >= ENABLED_SELECTED; }

	/** Inherited from tselectable_ */
	void set_selected(const bool selected = true);

	void set_callback_mouse_left_click(void (*callback) (twidget*)) 
		{ callback_mouse_left_click_ = callback; }
	
private:
	//! Note the order of the states must be the same as defined in settings.hpp.
	//! Also note the internals do assume the order for up and down to be the same
	//! and also that 'up' is before 'down'.
	enum tstate { 
		ENABLED,          DISABLED,          FOCUSSED, 
		ENABLED_SELECTED, DISABLED_SELECTED, FOCUSSED_SELECTED, 
		COUNT};

	void set_state(tstate state);
	tstate state_;
 
 	/** This callback is used when the control gets a left click. */
	void (*callback_mouse_left_click_) (twidget*);

	//! Inherited from tcontrol.
	const std::string& get_control_type() const 
		{ static const std::string type = "toggle_button"; return type; }

};


} // namespace gui2

#endif


