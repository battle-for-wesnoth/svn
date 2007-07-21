/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2.
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

// This function use temporary the b&w cursors while the display is busy
// If used, the colours ones will be reused when display is ready again
void temporary_use_bw();

// use the default parameter to reset cursors
// e.g. after a change in color cursor preferences
void set(CURSOR_TYPE type = NUM_CURSORS);
void set_dragging(bool drag);
CURSOR_TYPE get();

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
