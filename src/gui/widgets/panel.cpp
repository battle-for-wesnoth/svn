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

#include "gui/widgets/panel.hpp"


namespace gui2 {

SDL_Rect tpanel::get_client_rect() const
{
	boost::intrusive_ptr<const tpanel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tpanel_definition::tresolution>(config());
	assert(conf);

	SDL_Rect result = get_rect();
	result.x += conf->left_border;
	result.y += conf->top_border;
	result.w -= conf->left_border + conf->right_border;
	result.h -= conf->top_border + conf->bottom_border;

	return result;
}

void tpanel::draw(surface& surface, const bool force, 
		const bool invalidate_background)
{
	// Need to preserve the state and inherited draw clear the flag.
	const bool dirty = is_dirty();

	tcontainer_::draw(surface, force, invalidate_background);

	// foreground
	if(dirty || force) {
		SDL_Rect rect = get_rect();
    	canvas(1).draw(true);
	    blit_surface(canvas(1).surf(), 0, surface, &rect);
	}
}

tpoint tpanel::border_space() const
{
	boost::intrusive_ptr<const tpanel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tpanel_definition::tresolution>(config());
	assert(conf);

	return tpoint(conf->left_border + conf->right_border,
		conf->top_border + conf->bottom_border);
}

} // namespace gui2

