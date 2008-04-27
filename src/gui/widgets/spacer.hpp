/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef __GUI_WIDGETS_SPACER_HPP_INCLUDED__
#define __GUI_WIDGETS_SPACER_HPP_INCLUDED__

#include "gui/widgets/control.hpp"

#include "gui/widgets/settings.hpp"

namespace gui2 {

// Class for a spacer
class tspacer : public tcontrol
{
public:
	tspacer() : 
		tcontrol(0)
		{
		}

	//! Inherited from tcontrol.
	// We are always active, might not be visible but always active.
	void set_active(const bool) {}
	bool get_active() const { return true; }
	unsigned get_state() const { return 0; }

	void draw(surface&) {}
	
private:

	//! Inherited from tcontrol.
	const std::string& get_control_type() const 
		{ static const std::string type = "spacer"; return type; }
};


} // namespace gui2

#endif


