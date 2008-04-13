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

#ifndef __GUI_WIDGETS_TOOLTIP_HPP_INCLUDED__
#define __GUI_WIDGETS_TOOLTIP_HPP_INCLUDED__

#include "gui/widgets/control.hpp"

namespace gui2 {

class ttooltip : public tcontrol
{
public:

	ttooltip() :
		tcontrol(1)
		{}

	//! Inherited from tcontrol.
	// We are always active, might not be visible but always active.
	void set_active(const bool) {}
	bool get_active() const { return true; }
	unsigned get_state() const { return 0; }

	//! Inherited from twidget.
	void load_config();
};

} // namespace gui2

#endif
