/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef SLIDER_HPP_INCLUDED
#define SLIDER_HPP_INCLUDED

#include "SDL.h"

#include "../display.hpp"
#include "../sdl_utils.hpp"

#include <vector>

namespace gui {

class slider
{
	display& disp_;
	SDL_Surface* image_, *selectedImage_;
	scoped_sdl_surface buffer_;
	SDL_Rect area_;

	double value_;

	bool drawn_;

	bool highlight_, clicked_, dragging_;

	SDL_Rect slider_area() const;

public:
	slider(display& disp, SDL_Rect& rect, double value);

	static int height(display& disp);

	void draw();

	double process(int mousex, int mousey, bool button);

	const SDL_Rect& area() const;

	void background_changed();
};

}

#endif
