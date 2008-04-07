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

#ifndef __GUI_WIDGETS_LABEL_HPP_INCLUDED__
#define __GUI_WIDGETS_LABEL_HPP_INCLUDED__

#include "gui/widgets/control.hpp"

#include "gui/widgets/settings.hpp"

namespace gui2 {

class tlabel : public tcontrol
{
public:
	
	tlabel() :
		tcontrol(COUNT)
	{}

	void set_active(const bool active) { set_state(active ? ENABLED : DISABLED); };
	bool get_active() const { return state_ == ENABLED; }
	unsigned get_state() const { return state_; }
	bool full_redraw() const { return false; /* FIXME IMPLEMENT */ }

	void mouse_hover(tevent_handler&);

	void draw(surface& surface);

	// note we should check whether the label fits in the label
	// Inherited from twidget.
	tpoint get_minimum_size() const;
	tpoint get_best_size() const;
	tpoint get_maximum_size() const;

	void set_best_size(const tpoint& origin);
private:
	//! Note the order of the states must be the same as defined in settings.hpp.
	enum tstate { ENABLED, DISABLED, COUNT };

	void set_state(tstate state);
	tstate state_;

	std::vector<tlabel_definition::tresolution>::const_iterator definition_;

	void resolve_definition();
};

} // namespace gui2

#endif

