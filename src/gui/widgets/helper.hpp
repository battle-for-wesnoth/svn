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

#ifndef GUI_WIDGETS_HELPER_HPP_INCLUDED
#define GUI_WIDGETS_HELPER_HPP_INCLUDED

#include "SDL.h"

#include <string>

class surface;
class t_string;

namespace gui2 {

/**
 * Initializes the gui subsystems.
 *
 * This function needs to be called before other parts of the gui engine are
 * used.
 */
bool init();

/** Holds a 2D point. */
struct tpoint
{
	tpoint(const int x_, const int y_) : 
		x(x_),
		y(y_) 
		{}

	/** x coodinate. */
	int x;

	/** y coodinate. */
	int y;

	bool operator==(const tpoint& point) const { return x == point.x && y == point.y; }
	bool operator!=(const tpoint& point) const { return x != point.x || y != point.y; }
	bool operator<(const tpoint& point) const 
		{ return x < point.x || (x == point.x && y < point.y); }

	bool operator<=(const tpoint& point) const 
		{ return x < point.x || (x == point.x && y <= point.y); }
		
	tpoint operator+(const tpoint& point) const 
		{ return tpoint(x + point.x, y + point.y); }

	tpoint& operator+=(const tpoint& point);

	tpoint operator-(const tpoint& point) const 
		{ return tpoint(x - point.x, y - point.y); }

	tpoint& operator-=(const tpoint& point);
};

std::ostream &operator<<(std::ostream &stream, const tpoint& point);

/**
 * Creates a rectangle.
 *
 * @param origin                  The top left corner.
 * @param size                    The width (x) and height (y).
 *
 * @returns                       SDL_Rect with the proper rectangle.
 */
SDL_Rect create_rect(const tpoint& origin, const tpoint& size);

/**
 * Converts a colour string to a colour.
 *
 * @param colour                  A colour string see
 *                                http://www.wesnoth.org/wiki/GUIVariable for
 *                                more info.
 *
 * @returns                       The colour.
 */
Uint32 decode_colour(const std::string& colour);

/**
 * Converts a font style string to a font style.
 *
 * @param colour                  A font style string see
 *                                http://www.wesnoth.org/wiki/GUIVariable for
 *                                more info.
 *
 * @returns                       The font style.
 */
int decode_font_style(const std::string& style);

/**
 * Copies a portion of a surface.
 *
 * Unlike get_surface_portion it copies rather then using SDL_Blit. Using 
 * SDL_Blit gives problems with transparent surfaces.
 *
 * @param background              The surface to safe a portion from, this
 *                                surface shouldn't be a RLE surface.
 * @param rect                    The part of the surface to copy, the part of
 *                                the rect should be entirely on the surface.
 *
 * @returns                       A copy of the wanted part of the background.
 */
surface save_background(const surface& background, const SDL_Rect& rect);

/**
 * Copies one surface unto another one.
 *
 * @param restore                The surface to copy to the background.
 * @param background             The surface to copy unto.
 * @param rect                   The area to copy to on the background.
 */
void restore_background(const surface& restorer, 
		surface& background,const SDL_Rect& rect);

/**
 * Returns a default error message if a mandatory widget is ommited.
 *
 * @param id                      The id of the omitted widget.
 * @returns                       The error message.
 */
t_string missing_widget(const std::string& id);

} // namespace gui2

#endif
