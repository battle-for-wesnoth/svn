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
#ifndef BUTTON_H_INCLUDED
#define BUTTON_H_INCLUDED

#include "SDL.h"

#include "widget.hpp"

#include "../sdl_utils.hpp"

#include <string>
#include <vector>
#include <string>

class display;

namespace gui {

class button : public widget
{
public:
	struct error {};

	enum TYPE { TYPE_PRESS, TYPE_CHECK };

	button(display& disp, const std::string& label, TYPE type=TYPE_PRESS,
	       std::string button_image="");

	virtual ~button() {}

	void set_check(bool check);
	bool checked() const;

	void draw();

	void set_label(const std::string& val);

	bool process(int mousex, int mousey, bool button);

	void enable(bool new_val);
	bool enabled() const;

protected:
	virtual void handle_event(const SDL_Event& event);
	virtual void mouse_motion(const SDL_MouseMotionEvent& event);
	virtual void mouse_down(const SDL_MouseButtonEvent& event);
	virtual void mouse_up(const SDL_MouseButtonEvent& event);

private:

	std::string label_;
	display* display_;
	shared_sdl_surface image_, pressedImage_, activeImage_, pressedActiveImage_;
	SDL_Rect textRect_;

	bool button_;

	enum STATE { UNINIT, NORMAL, ACTIVE, PRESSED, PRESSED_ACTIVE };
	STATE state_;

	TYPE type_;

	bool enabled_;

	bool pressed_;

	bool hit(int x, int y) const;
}; //end class button

}

#endif
