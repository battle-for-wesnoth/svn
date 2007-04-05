/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef CURSOR_HPP_INCLUDED
#define CURSOR_HPP_INCLUDED

#include "SDL.h"
#include "sdl_utils.hpp"

namespace cursor
{

struct manager
{
	manager();
	~manager();
};

enum CURSOR_TYPE { NORMAL, WAIT, MOVE, ATTACK, HYPERLINK, MOVE_DRAG, ATTACK_DRAG, NO_CURSOR, NUM_CURSORS };

void use_colour(bool value);
// This function use temporary the b&w cursors while the display is busy
// If used, the colours ones will be reused when display is ready again
void temporary_use_bw();

void set(CURSOR_TYPE type);
void set_dragging(bool drag);

void draw(surface screen);
void undraw(surface screen);

void set_focus(bool focus);

struct setter
{
	setter(CURSOR_TYPE type);
	~setter();

private:
	CURSOR_TYPE old_;
};

}

#endif
