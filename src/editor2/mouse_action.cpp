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
#include "mouse_action.hpp"

#include "../construct_dialog.hpp"
#include "../foreach.hpp"
#include "../gettext.hpp"
#include "../pathutils.hpp"

namespace editor2 {

void mouse_action::move(editor_display& disp, int x, int y)
{
}

editor_action* mouse_action::drag(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo)
{
	return NULL;
}

editor_action* mouse_action::drag_end(editor_display& disp, int x, int y)
{
	return NULL;
}


void brush_drag_mouse_action::move(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	if (hex != previous_move_hex_) {
		disp.set_brush_locs(get_brush().project(disp.hex_clicked_on(x,y)));
		previous_move_hex_ = hex;
	}
}

editor_action* brush_drag_mouse_action::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	previous_drag_hex_ = hex;
	return click_perform(disp, hex);
}

editor_action* brush_drag_mouse_action::drag(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo)
{
	move(disp, x, y);
	gamemap::location hex = disp.hex_clicked_on(x, y);
	if (hex != previous_drag_hex_) {
		editor_action* a = click_perform(disp, hex);
		previous_drag_hex_ = hex;
		return a;
	} else {
		return NULL;
	}
}
	
editor_action* brush_drag_mouse_action::drag_end(editor_display& disp, int x, int y)
{
	return NULL;
}

const brush& brush_drag_mouse_action::get_brush()
{
	assert(brush_);
	assert(*brush_);
	return **brush_;
}


editor_action* mouse_action_paint::click_perform(editor_display& disp, const gamemap::location& hex)
{
	bool one_layer = (key_[SDLK_RALT] || key_[SDLK_LALT]);
	return new editor_action_paint_area(get_brush().project(hex), terrain_, one_layer);
}

editor_action* mouse_action_select::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	selecting_ = !disp.map().in_selection(hex);
	return brush_drag_mouse_action::click(disp, x, y);
}

editor_action* mouse_action_select::click_perform(editor_display& disp, const gamemap::location& hex)
{
	editor_action* a(NULL);
	if (selecting_) {
		a = new editor_action_select(get_brush().project(hex));
	} else {
		a = new editor_action_deselect(get_brush().project(hex));
	}
	return a;
}


void mouse_action_paste::move(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	std::set<gamemap::location> affected = paste_.get_offset_area(hex);
	disp.set_brush_locs(affected);
}

editor_action* mouse_action_paste::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	editor_action_paste* a = new editor_action_paste(hex, paste_);
	return a;
}


void mouse_action_fill::move(editor_display& disp, int x, int y)
{
	std::set<gamemap::location> affected = 
		disp.map().get_contigious_terrain_tiles(disp.hex_clicked_on(x, y));
	disp.set_brush_locs(affected);
}

editor_action* mouse_action_fill::click(editor_display& disp, int x, int y)
{
	bool one_layer = (key_[SDLK_RALT] || key_[SDLK_LALT]);
	gamemap::location hex = disp.hex_clicked_on(x, y);
	//TODO only take the base terrain into account when searching for contigious terrain when painting base only
	editor_action_fill* a = new editor_action_fill(hex, terrain_, one_layer);
	return a;
}


void mouse_action_starting_position::move(editor_display& disp, int x, int y)
{
	disp.clear_brush_locs();
	disp.add_brush_loc(disp.hex_clicked_on(x, y));
}

editor_action* mouse_action_starting_position::click(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	int player_starting_at_hex = disp.map().is_starting_position(hex) + 1;
	std::vector<std::string> players;
	players.push_back(_("(Player)^None"));
	for (int i = 1; i <= gamemap::MAX_PLAYERS; i++) {
		std::stringstream str;
		str << _("Player") << " " << i;
		players.push_back(str.str());
	}
	gui::dialog pmenu = gui::dialog(disp,
				       _("Which Player?"),
				       _("Which player should start here?"),
				       gui::OK_CANCEL);
	pmenu.set_menu(players);
	int res = pmenu.show();
	editor_action* a = NULL;
	if (res == 0 && player_starting_at_hex != -1) {
		a = new editor_action_starting_position(gamemap::location(), player_starting_at_hex);
	} else if (res > 0) {
		a = new editor_action_starting_position(hex, res);
	}
	return a;
}

} //end namespace editor2
