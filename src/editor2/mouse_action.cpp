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

void mouse_action::move(editor_display& disp, const gamemap::location& hex)
{
	if (hex != previous_move_hex_) {
		update_brush_highlights(disp, hex);
		previous_move_hex_ = hex;
	}
}

void mouse_action::update_brush_highlights(editor_display& disp, const gamemap::location& hex)
{
	disp.set_brush_locs(affected_hexes(disp, hex));
}

std::set<gamemap::location> mouse_action::affected_hexes(
		editor_display& /*disp*/, const gamemap::location& hex)
{
	std::set<gamemap::location> res;
	res.insert(hex);
	return res;
}

editor_action* mouse_action::click_left(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

editor_action* mouse_action::click_right(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

editor_action* mouse_action::drag_left(editor_display& /*disp*/, 
		int /*x*/, int /*y*/, bool& /*partial*/, editor_action* /*last_undo*/)
{
	return NULL;
}

editor_action* mouse_action::drag_right(editor_display& /*disp*/, 
		int /*x*/, int /*y*/, bool& /*partial*/, editor_action* /*last_undo*/)
{
	return NULL;
}

editor_action* mouse_action::drag_end(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

editor_action* mouse_action::up_right(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

editor_action* mouse_action::up_left(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

editor_action* mouse_action::key_event(
	editor_display& disp, const SDL_Event& event)
{
	if (!has_alt_modifier() && (event.key.keysym.sym >= '1' && event.key.keysym.sym <= '9')) {
		int side = event.key.keysym.sym - '0';
		if (side >= 1 && side <= gamemap::MAX_PLAYERS) {
			gamemap::location pos = disp.get_map().starting_position(side);
			if (pos.valid()) {
				disp.scroll_to_tile(pos, display::WARP);
			}
		}
		return NULL;
	}
	if (!disp.map().on_board(previous_move_hex_) || event.type != SDL_KEYUP) {
		return NULL;
	}
	editor_action* a = NULL;
	if ((has_alt_modifier() && (event.key.keysym.sym >= '1' && event.key.keysym.sym <= '9'))
	|| event.key.keysym.sym == SDLK_DELETE) {
		int res = event.key.keysym.sym - '0';
		if (res > gamemap::MAX_PLAYERS || event.key.keysym.sym == SDLK_DELETE) res = 0;
		int player_starting_at_hex = disp.map().is_starting_position(previous_move_hex_) + 1;
		if (res == 0 && player_starting_at_hex != -1) {
			a = new editor_action_starting_position(gamemap::location(), player_starting_at_hex);
		} else if (res > 0 && res != player_starting_at_hex) {
			a = new editor_action_starting_position(previous_move_hex_, res);
		}
	}
	return a;
}

void mouse_action::set_mouse_overlay(editor_display& disp)
{
	disp.set_mouseover_hex_overlay(NULL);
}

bool mouse_action::has_alt_modifier() const 
{
	return key_[SDLK_RALT] || key_[SDLK_LALT];
}

bool mouse_action::has_shift_modifier() const 
{
	return key_[SDLK_RSHIFT] || key_[SDLK_LSHIFT];
}

void mouse_action::set_terrain_mouse_overlay(editor_display& disp, t_translation::t_terrain fg,
		t_translation::t_terrain bg)
{
	surface image_fg(image::get_image("terrain/" + disp.get_map().get_terrain_info(
				fg).editor_image() + ".png"));
	surface image_bg(image::get_image("terrain/" + disp.get_map().get_terrain_info(
				bg).editor_image() + ".png"));

	if (image_fg == NULL || image_bg == NULL) {
		ERR_ED << "Missing terrain icon\n";
		disp.set_mouseover_hex_overlay(NULL);
		return; 
	}

	// Create a transparent surface of the right size.
	surface image = create_compatible_surface(image_fg, image_fg->w, image_fg->h);
	SDL_FillRect(image,NULL,SDL_MapRGBA(image->format,0,0,0, 0));

	// For efficiency the size of the tile is cached.
	// We assume all tiles are of the same size.
	// The zoom factor can change, so it's not cached.
	// NOTE: when zooming and not moving the mouse, there are glitches.
	// Since the optimal alpha factor is unknown, it has to be calculated
	// on the fly, and caching the surfaces makes no sense yet.
	static const Uint8 alpha = 196;
	static const int size = image_fg->w;
	static const int half_size = size / 2;
	static const int quarter_size = size / 4;
	static const int offset = 2;
	static const int new_size = half_size - 2;
	const int zoom = static_cast<int>(size * disp.get_zoom_factor());

	// Blit left side
	image_fg = scale_surface(image_fg, new_size, new_size);
	SDL_Rect rcDestLeft = { offset, quarter_size, 0, 0 };
	SDL_BlitSurface ( image_fg, NULL, image, &rcDestLeft );

	// Blit left side
	image_bg = scale_surface(image_bg, new_size, new_size);
	SDL_Rect rcDestRight = { half_size, quarter_size, 0, 0 };
	SDL_BlitSurface ( image_bg, NULL, image, &rcDestRight );

	// Add the alpha factor and scale the image
	image = scale_surface(adjust_surface_alpha(image, alpha), zoom, zoom);

	// Set as mouseover
	disp.set_mouseover_hex_overlay(image);
}

std::set<gamemap::location> brush_drag_mouse_action::affected_hexes(
	editor_display& /*disp*/, const gamemap::location& hex)
{
	return get_brush().project(hex);
}

editor_action* brush_drag_mouse_action::click_left(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	previous_drag_hex_ = hex;
	return click_perform_left(disp, affected_hexes(disp, hex));
}

editor_action* brush_drag_mouse_action::click_right(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	previous_drag_hex_ = hex;
	return click_perform_right(disp, affected_hexes(disp, hex));
}

editor_action* brush_drag_mouse_action::drag_left(editor_display& disp, 
		int x, int y, bool& partial, editor_action* last_undo)
{
	return drag_generic<&brush_drag_mouse_action::click_perform_left>(disp, x, y, partial, last_undo);
}

editor_action* brush_drag_mouse_action::drag_right(editor_display& disp, 
		int x, int y, bool& partial, editor_action* last_undo)
{
	return drag_generic<&brush_drag_mouse_action::click_perform_right>(disp, x, y, partial, last_undo);
}

editor_action* brush_drag_mouse_action::drag_end(
		editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

template <editor_action* (brush_drag_mouse_action::*perform_func)(editor_display&, const std::set<gamemap::location>&)>
editor_action* brush_drag_mouse_action::drag_generic(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	move(disp, hex);
	if (hex != previous_drag_hex_) {
		std::set<gamemap::location> current_step_locs = affected_hexes(disp, hex);
		editor_action_extendable* last_undo_x = dynamic_cast<editor_action_extendable*>(last_undo);
		LOG_ED << "Last undo is " << last_undo << " and as x " << last_undo_x << "\n";
		partial = true;
		editor_action* a = (this->*perform_func)(disp, affected_hexes(disp, hex));
		previous_drag_hex_ = hex;
		return a;
	} else {
		return NULL;
	}
}

const brush& brush_drag_mouse_action::get_brush()
{
	assert(brush_);
	assert(*brush_);
	return **brush_;
}


editor_action* mouse_action_paint::click_perform_left(
		editor_display& /*disp*/, const std::set<gamemap::location>& hexes)
{
	return new editor_action_chain(new editor_action_paint_area(hexes, terrain_left_, has_alt_modifier()));
}

editor_action* mouse_action_paint::click_perform_right(
		editor_display& /*disp*/, const std::set<gamemap::location>& hexes)
{
	return new editor_action_chain(new editor_action_paint_area(hexes, terrain_right_, has_alt_modifier()));
}

void mouse_action_paint::set_mouse_overlay(editor_display& disp)
{
	set_terrain_mouse_overlay(disp, terrain_left_, terrain_right_);
}


std::set<gamemap::location> mouse_action_select::affected_hexes(
	editor_display& disp, const gamemap::location& hex)
{
	if (has_shift_modifier()) {
		return disp.map().get_contigious_terrain_tiles(hex);
	} else {
		return brush_drag_mouse_action::affected_hexes(disp, hex);
	}
}

editor_action* mouse_action_select::key_event(
		editor_display& disp, const SDL_Event& event)
{
	editor_action* ret = mouse_action::key_event(disp, event);
	update_brush_highlights(disp, previous_move_hex_);
	return ret;
}

editor_action* mouse_action_select::click_perform_left(
		editor_display& /*disp*/, const std::set<gamemap::location>& hexes)
{
	return new editor_action_chain(new editor_action_select(hexes));
}

editor_action* mouse_action_select::click_perform_right(
		editor_display& /*disp*/, const std::set<gamemap::location>& hexes)
{
	return new editor_action_chain(new editor_action_deselect(hexes));
}

void mouse_action_select::set_mouse_overlay(editor_display& disp)
{
	surface image;
	if (has_shift_modifier()) {
		image = image::get_image("editor/tool-overlay-select-wand.png");
	} else {
		image = image::get_image("editor/tool-overlay-select-brush.png");
	}
	Uint8 alpha = 196;
	int size = image->w;
	int zoom = static_cast<int>(size * disp.get_zoom_factor());

	// Add the alpha factor and scale the image
	image = scale_surface(adjust_surface_alpha(image, alpha), zoom, zoom);
	disp.set_mouseover_hex_overlay(image);
}


std::set<gamemap::location> mouse_action_paste::affected_hexes(
	editor_display& /*disp*/, const gamemap::location& hex)
{
	return paste_.get_offset_area(hex);
}

editor_action* mouse_action_paste::click_left(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	editor_action_paste* a = new editor_action_paste(paste_, hex);
	return a;
}

editor_action* mouse_action_paste::click_right(editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

void mouse_action_paste::set_mouse_overlay(editor_display& disp)
{
	disp.set_mouseover_hex_overlay(NULL); //TODO
}


std::set<gamemap::location> mouse_action_fill::affected_hexes(
	editor_display& disp, const gamemap::location& hex)
{
	return disp.map().get_contigious_terrain_tiles(hex);
}

editor_action* mouse_action_fill::click_left(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	//TODO only take the base terrain into account when searching for contigious terrain when painting base only
	//or use a different key modifier for that
	editor_action_fill* a = new editor_action_fill(hex, terrain_left_, has_alt_modifier());
	return a;
}

editor_action* mouse_action_fill::click_right(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	//TODO only take the base terrain into account when searching for contigious terrain when painting base only
	//or use a different key modifier for that
	editor_action_fill* a = new editor_action_fill(hex, terrain_right_, has_alt_modifier());
	return a;
}

void mouse_action_fill::set_mouse_overlay(editor_display& disp)
{
	set_terrain_mouse_overlay(disp, terrain_left_, terrain_right_);
}


editor_action* mouse_action_starting_position::up_left(editor_display& disp, int x, int y)
{
	if (!click_) return NULL;
	click_ = false;
	gamemap::location hex = disp.hex_clicked_on(x, y);
	if (!disp.map().on_board(hex)) {
		return NULL;
	}
	int player_starting_at_hex = disp.map().is_starting_position(hex) + 1;
	std::vector<std::string> players;
	players.push_back(_("(Player)^None"));
	for (int i = 1; i <= gamemap::MAX_PLAYERS; i++) {
		std::stringstream str;
		str << _("Player") << " " << i;
		players.push_back(str.str());
	}
	gui::dialog pmenu = gui::dialog(disp,
				       _("Choose player"),
				       _("Which player should start here? You can also use the 1-9 and delete keys to set/clear staring positions."),
				       gui::OK_CANCEL);
	pmenu.set_menu(players);
	int res = pmenu.show();
	editor_action* a = NULL;
	if (res == 0 && player_starting_at_hex != -1) {
		a = new editor_action_starting_position(gamemap::location(), player_starting_at_hex);
	} else if (res > 0 && res != player_starting_at_hex) {
		a = new editor_action_starting_position(hex, res);
	}
	update_brush_highlights(disp, hex);
	return a;
}

editor_action* mouse_action_starting_position::click_left(editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	click_ = true;
	return NULL;
}

editor_action* mouse_action_starting_position::up_right(editor_display& disp, int x, int y)
{
	gamemap::location hex = disp.hex_clicked_on(x, y);
	int player_starting_at_hex = disp.map().is_starting_position(hex) + 1;
	if (player_starting_at_hex != -1) {
		return new editor_action_starting_position(gamemap::location(), player_starting_at_hex);
	} else {
		return NULL;
	}
}

editor_action* mouse_action_starting_position::click_right(editor_display& /*disp*/, int /*x*/, int /*y*/)
{
	return NULL;
}

void mouse_action_starting_position::set_mouse_overlay(editor_display& disp)
{
	surface image = image::get_image("editor/tool-overlay-starting-position.png");
	Uint8 alpha = 196;
	int size = image->w;
	int zoom = static_cast<int>(size * disp.get_zoom_factor());

	// Add the alpha factor and scale the image
	image = scale_surface(adjust_surface_alpha(image, alpha), zoom, zoom);
	disp.set_mouseover_hex_overlay(image);
}


} //end namespace editor2
