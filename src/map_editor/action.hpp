/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file action.hpp
//! Editor action classes

#include "action_base.hpp"
#include "../map.hpp"
#include "../terrain.hpp"

namespace map_editor {
	
//Replace contents of the entirem map action
//useful as a fallback undo method when something else would be impractical
class editor_action_whole_map : public editor_action
{
    public:
        editor_action_whole_map(editor_map& m)
        : m_(m)
        {
        }
        editor_action_whole_map* perform(editor_map& m);
        void perform_without_undo(editor_map& m);
    protected:
        editor_map m_;
};

//container wrapping several actions as one
class editor_action_chain : public editor_action
{
	public:
		explicit editor_action_chain(std::vector<editor_action*> actions)
		: actions_(actions)
		{
		}
		~editor_action_chain();
		editor_action_chain* perform(editor_map& m);
	    void perform_without_undo(editor_map& m);
    protected:
        std::vector<editor_action*> actions_;
};

//common base classes for actions with common behaviour

//actions which act on a specified location (and possibly on other locations 
//that can be derived from the staring hex)
class editor_action_location : public editor_action
{
  public:
        editor_action_location(gamemap::location loc)
        : loc_(loc)
        {
        }
    protected:
        gamemap::location loc_;
};

//actions which in addition to acting on a hex, act with one terrain type.
//Mostly paint-related actions.
class editor_action_location_terrain : public editor_action_location
{
    public:
        editor_action_location_terrain(gamemap::location loc, 
			t_translation::t_terrain t)
        : editor_action_location(loc), t_(t)
        {
        }
    protected:
        t_translation::t_terrain t_;
};

//paste a region into the map.
//The paste data will most likely be a gamemap with some sort of mask
class editor_action_paste : public editor_action_location
{
    public:
        editor_action_paste(gamemap::location loc, gamemap paste)
        : editor_action_location(loc), paste_(paste)
        {
        }
        editor_action_paste* perform(editor_map& map);
        void perform_without_undo(editor_map& map);
    protected:
        gamemap paste_;
};

//replace a hex at a given location with a given terrain
//since this is a lot simpler than a brush paint, it is separate at least for now
class editor_action_paint_hex : public editor_action_location_terrain
{
    public:
        editor_action_paint_hex(gamemap::location loc, t_translation::t_terrain t)
        : editor_action_location_terrain(loc, t)
        {
        }
        editor_action_paint_hex* perform(editor_map& m);            
        void perform_without_undo(editor_map& map);
};

//paint a terrain on the map with a brush. The brush is a special mask type
//note that undo in this case is a paste not a brush paint.
class editor_action_paint_brush : public editor_action_location_terrain
{
    public:
        editor_action_paint_brush(gamemap::location loc, 
			t_translation::t_terrain t, brush b)
        : editor_action_location_terrain(loc, t), b_(b)
        {
        }
        editor_action_paste* perform(editor_map& map);
        void perform_without_undo(editor_map& map);
    protected:
        brush b_;
};

//flood fill
class editor_action_fill : public editor_action_location_terrain
{
    public:
        editor_action_fill(gamemap::location loc, 
			t_translation::t_terrain t, brush b)
        : editor_action_location_terrain(loc, t), b_(b)
        {
        }
        editor_action_fill* perform(editor_map& map);
        void perform_without_undo(editor_map& map);
    protected:
		brush b_;
};

//resize map (streching / clipping behaviour?)
class editor_action_resize_map : public editor_action
{
	public:
		editor_action_resize_map(int to_x_size, int to_y_size)
		: to_x_size_(to_x_size), to_y_size_(to_y_size)
		{
		}
		editor_action_whole_map* perform(editor_map& map);
		void perform_without_undo(editor_map& map);
	protected:
		int to_x_size_;
		int to_y_size_;
};

//basic rotations. angle is multiplied by 60 degrees so 3 does a 180 turn
class editor_action_rotate_map : public editor_action
{
	public:
		editor_action_rotate_map(int angle)
		: angle_(angle)
		{
		}
		editor_action_rotate_map* perform(editor_map& map);
		void perform_without_undo(editor_map& map);
	protected:
		int angle_;
};

//mirror. angle is multiplied by 30 degrees
//e.g. 0 is mirror by x axis, 3 is by y axis.
class editor_action_mirror_map : public editor_action
{
	public:
		editor_action_mirror_map(int angle)
		: angle_(angle)
		{
		}
		editor_action_mirror_map* perform(editor_map& map);
		void perform_without_undo(editor_map& map);
	protected:
		int angle_;
};

//plot a route between two points
class editor_action_plot_route : public editor_action_location_terrain
{
	public:
		editor_action_plot_route(gamemap::location l1, 
			t_translation::t_terrain t, gamemap::location l2)
		: editor_action_location_terrain(l1, t)
		, loc2_(l2)
		{
		}
		editor_action_paste* perform(editor_map& map);
		void perform_without_undo(editor_map& map);
	protected:
		gamemap::location loc2_;
};

} //namespace map_editor
