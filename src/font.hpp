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
#ifndef FONT_HPP_INCLUDED
#define FONT_HPP_INCLUDED

#include "SDL.h"
#include "SDL_ttf.h"

#include "display.hpp"
#include "video.hpp"

#include <string>

namespace font {

//object which initializes and destroys structures needed for fonts
struct manager {
	manager();
	~manager();
	struct error {};
};

//function which sets the name of the font to use
void set_font(const std::string& name);

//various standard colours
extern const SDL_Color NORMAL_COLOUR, GOOD_COLOUR, BAD_COLOUR, BLACK_COLOUR,
                       DARK_COLOUR, YELLOW_COLOUR, BUTTON_COLOUR;

enum MARKUP { USE_MARKUP, NO_MARKUP };

//standard markups
extern const char LARGE_TEXT, SMALL_TEXT, GOOD_TEXT, BAD_TEXT, NORMAL_TEXT, BLACK_TEXT, BOLD_TEXT, IMAGE, NULL_MARKUP;

//function to draw text on the screen. The text will be clipped to area.
//If the text runs outside of area horizontally, an ellipsis will be displayed
//at the end of it. If use_tooltips is true, then text with an ellipsis will
//have a tooltip set for it equivalent to the entire contents of the text.
//
//if use_markup is equal to USE_MARKUP, then some very basic 'markup' will
//be done on the text:
// - any line beginning in # will be displayed in BAD_COLOUR
// - any line beginning in @ will be displayed in GOOD_COLOUR
// - any line beginning in + will be displayed with size increased by 2
// - any line beginning in - will be displayed with size decreased by 2
// - any line beginning with 0x0n will be displayed in the colour of side n
//
//the above special characters can be quoted using a C-style backslash.
//
//a bounding rectangle of the text is returned. If gui is NULL, then the
//text will not be drawn, and a bounding rectangle only will be returned.

SDL_Rect draw_text(display* gui, const SDL_Rect& area, int size,
                   const SDL_Color& colour, const std::string& text,
                   int x, int y, surface bg=NULL,
                   bool use_tooltips=false, MARKUP use_markup=USE_MARKUP, int style=0);

//function which returns the size of text if it were to be drawn.
SDL_Rect text_area(const std::string& text, int size, int style=0);

// Returns a SDL surface containing the text rendered in a given colour.
surface get_rendered_text(const std::string& text, int size, const SDL_Color& colour, int style=0);
	
// Returns the maximum height of a font, in pixels
int get_max_height(int size);
	
bool is_format_char(char c);

///
/// Determine the width of a line of text given a certain font size.
/// The font type used is the default wesnoth font type.
///
int line_width(const std::string line, int font_size, int style=TTF_STYLE_NORMAL);

///
/// If the text exceedes the specified max width, wrap it one a word basis.
/// If the is not possible, e.g. the word is too big to fit, wrap it on a
/// char basis.
///
std::string word_wrap_text(const std::string& unwrapped_text, int font_size, int max_width);

///
/// If the text excedes the specified max width, end it with an ellipsis (...)
///
std::string make_text_ellipsis(const std::string& text, int font_size, int max_width);
	

///
/// Draw text on the screen. This method makes sure that the text
/// fits within a given maximum width. If a line exceedes this width it
/// will be wrapped on a word basis if possible, otherwise on a char
/// basis. This method is otherwise similar to the draw_text method,
/// but it doesn't support special markup or tooltips.
///
/// @return a bounding rectangle of the text.
/// 
SDL_Rect draw_wrapped_text(display* gui, const SDL_Rect& area, int font_size,
			     const SDL_Color& colour, const std::string& text,
			     int x, int y, int max_width, surface bg = NULL);


/// structure which will hide all current floating labels, and cause floating labels
/// instantiated after it is created to be displayed
struct floating_label_context
{
	floating_label_context();
	~floating_label_context();
};

enum ALIGN { LEFT_ALIGN, CENTER_ALIGN, RIGHT_ALIGN };

enum LABEL_SCROLL_MODE { ANCHOR_LABEL_SCREEN, ANCHOR_LABEL_MAP };

/// add a label floating on the screen above everything else.
/// 'text': the text to display
/// 'font_size': the size to display the text in
/// 'colour': the colour of the text
/// 'xpos,ypos': the location on the screen to display the text.
/// 'xmove,ymove': the amount to move the text each frame
/// 'lifetime': the number of frames to display the text for, or -1 to display until removed
/// 'clip_rect': the rectangle to clip the label to.
///
/// @returns a handle to the label which can be used with other label functions
int add_floating_label(const std::string& text, int font_size, const SDL_Color& colour,
					   double xpos, double ypos, double xmove, double ymove, int lifetime, const SDL_Rect& clip_rect, ALIGN alignment=CENTER_ALIGN,
					   const SDL_Color* bg_colour=NULL, int border_size=0, LABEL_SCROLL_MODE scroll_mode=ANCHOR_LABEL_SCREEN);

/// moves the floating label given by 'handle' by (xmove,ymove)
void move_floating_label(int handle, double xmove, double ymove);

/// moves all floating labels that have 'scroll_mode' set to ANCHOR_LABEL_MAP
void scroll_floating_labels(double xmove, double ymove);

/// removes the floating label given by 'handle' from the screen
void remove_floating_label(int handle);

/// hides or shows a floating label
void show_floating_label(int handle, bool show);

const std::string& get_floating_label_text(int handle);

SDL_Rect get_floating_label_rect(int handle);

void draw_floating_labels(surface screen);
void undraw_floating_labels(surface screen);

}

#endif
