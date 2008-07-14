/* $Id$ */
/*
   Copyright (C) 2008 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "action.hpp"
#include "brush.hpp"
#include "editor_common.hpp"
#include "editor_display.hpp"
#include "editor_mode.hpp"
#include "mouse_action.hpp"

#include "../foreach.hpp"
#include "../pathutils.hpp"

namespace editor2 {

void mouse_action::move(editor_display& disp, int x, int y)
{
}

editor_action* mouse_action::drag(editor_display& disp, int x, int y)
{
	return NULL;
}

editor_action* mouse_action::drag_end(editor_display& disp, int x, int y)
{
	return NULL;
}

void mouse_action_paint::move(editor_display& disp, int x, int y)
{
	SCOPE_ED;
	disp.clear_highlighted_locs();
	if (mode_.get_brush() != NULL) {
		foreach (gamemap::location loc, mode_.get_brush()->project(disp.hex_clicked_on(x,y))) {
			disp.add_highlighted_loc(loc);
		}
	}
}

editor_action* mouse_action_paint::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	t_translation::t_terrain terrain = mode_.get_foreground_terrain();
	editor_action* a;
	if (mode_.get_brush() != NULL) {
		a = new editor_action_paint_area(mode_.get_brush()->project(hex), terrain);
	} else {
		a = new editor_action_paint_hex(hex, terrain);
	}
	previous_hex_ = hex;
	return a;
}

editor_action* mouse_action_paint::drag(editor_display& disp, int x, int y)
{
	move(disp, x, y);
	gamemap::location hex = disp.hex_clicked_on(x, y);
	if (hex != previous_hex_) {
		return click(disp, x, y);
	} else {
		return NULL;
	}
}
	
editor_action* mouse_action_paint::drag_end(editor_display& disp, int x, int y)
{
	return drag(disp, x, y);
}

void mouse_action_fill::move(editor_display& disp, int x, int y)
{
	disp.clear_highlighted_locs();
	std::set<gamemap::location> affected = 
		dynamic_cast<const editor_map&>(disp.get_map()).
		get_contigious_terrain_tiles(disp.hex_clicked_on(x, y));
	foreach (gamemap::location loc, affected) {
		disp.add_highlighted_loc(loc);
	}
}

editor_action* mouse_action_fill::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	t_translation::t_terrain terrain = mode_.get_foreground_terrain();
	editor_action_fill* a = new editor_action_fill(hex, terrain);
	previous_hex_ = hex;
	return a;
}

} //end namespace editor2
