/* $Id$ */
/*
   copyright (C) 2008 - 2009 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/vertical_scrollbar.hpp"



namespace gui2 {

unsigned tvertical_scrollbar::minimum_positioner_length() const
{
	boost::intrusive_ptr<const tvertical_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tvertical_scrollbar_definition::tresolution>(config());
	assert(conf);
	return conf->minimum_positioner_length;
}

unsigned tvertical_scrollbar::maximum_positioner_length() const
{
	boost::intrusive_ptr<const tvertical_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tvertical_scrollbar_definition::tresolution>(config());
	assert(conf);
	return conf->maximum_positioner_length;
}

unsigned tvertical_scrollbar::offset_before() const
{
	boost::intrusive_ptr<const tvertical_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tvertical_scrollbar_definition::tresolution>(config());
	assert(conf);
	return conf->top_offset;
}

unsigned tvertical_scrollbar::offset_after() const
{
	boost::intrusive_ptr<const tvertical_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tvertical_scrollbar_definition::tresolution>(config());
	assert(conf);
	return conf->bottom_offset;
}

bool tvertical_scrollbar::on_positioner(const tpoint& coordinate) const
{
	// Note we assume the positioner is over the entire width of the widget.
	return coordinate.y >= static_cast<int>(get_positioner_offset())
		&& coordinate.y < static_cast<int>(get_positioner_offset() + get_positioner_length())
		&& coordinate.x > 0
		&& coordinate.x < static_cast<int>(get_width());
}

int tvertical_scrollbar::on_bar(const tpoint& coordinate) const
{
	// Not on the widget, leave.
	if(static_cast<size_t>(coordinate.x) > get_width()
			|| static_cast<size_t>(coordinate.y) > get_height()) {
		return 0;
	}

	// we also assume the bar is over the entire width of the widget.
	if(static_cast<size_t>(coordinate.y) < get_positioner_offset()) {
		return -1;
	} else if(static_cast<size_t>(coordinate.y) >get_positioner_offset() + get_positioner_length()) {
		return 1;
	} else {
		return 0;
	}
}

} // namespace gui2


