/* $Id:$ */
/*
   Copyright (C) 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef COMBO_DRAG_H_INCLUDED
#define COMBO_DRAG_H_INCLUDED

#include "SDL.h"

#include "widgets/combo.hpp"
#include "widgets/drop_target.hpp"

#include <boost/shared_ptr.hpp>
class display;


namespace gui {

	class combo_drag; 
	typedef boost::shared_ptr<combo_drag> combo_drag_ptr;

	class combo_drag : public combo, public drop_target
	{
		public:
			combo_drag(display& disp, const std::vector<std::string>& items, const drop_target_group& group);

			int get_drag_target();
		protected:
			virtual void process_event();
			virtual void mouse_motion(const SDL_MouseMotionEvent& event);
			virtual void mouse_down(const SDL_MouseButtonEvent& event);
			virtual void mouse_up(const SDL_MouseButtonEvent& event);

		private:
			void handle_move(const SDL_MouseMotionEvent& event);
			void handle_drop();
			int drag_target_, old_drag_target_;
			SDL_Rect old_location_;
			int mouse_x, mouse_y;
			enum drag_state {
				NONE,
				PRESSED,
				PRESSED_MOVE,
				MOVED,
				DROP_DOWN
			};
			drag_state drag_;
			static const int MIN_DRAG_DISTANCE;
	}; //end class combo

}

#endif
