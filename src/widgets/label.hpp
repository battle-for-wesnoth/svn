/* $Id$ */
/*
   Copyright (C) 2004 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LABEL_HPP_INCLUDED
#define LABEL_HPP_INCLUDED

#include "../font.hpp"
#include "widget.hpp"
#include <string>

namespace gui {

class label : public widget
{
public:
	label(display& d, const std::string& text, int size=font::SIZE_NORMAL, 
			const SDL_Colour& colour=font::NORMAL_COLOUR);
	const std::string& set_text(const std::string& text);
	const std::string& get_text() const;

	int set_size(int size);
	int get_size() const;

	const SDL_Colour& set_colour(const SDL_Colour& colour);
	const SDL_Colour& get_colour() const;

	virtual void draw_contents();
	virtual void set_location(const SDL_Rect& rect);
	using widget::set_location;

private:
	void update_label_size();

	std::string text_;
	int size_;
	SDL_Colour colour_;
};

}

#endif

