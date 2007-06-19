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

#include "global.hpp"

#include "cursor.hpp"
#include "game_config.hpp"
#include "image.hpp"
#include "preferences_display.hpp"
#include "scoped_resource.hpp"
#include "sdl_utils.hpp"
#include "video.hpp"

#include "SDL.h"

#include <iostream>
#include <vector>

static bool use_colour_cursors()
{
	return game_config::editor == false && preferences::use_colour_cursors();
}

static SDL_Cursor* create_cursor(surface surf)
{
	const surface nsurf(make_neutral_surface(surf));
	if(nsurf == NULL) {
		return NULL;
	}

	//the width must be a multiple of 8 (SDL requirement)

#ifdef __APPLE__
	size_t cursor_width = 16;
#else
	size_t cursor_width = nsurf->w;
	if((cursor_width%8) != 0) {
		cursor_width += 8 - (cursor_width%8);
	}
#endif
	std::vector<Uint8> data((cursor_width*nsurf->h)/8,0);
	std::vector<Uint8> mask(data.size(),0);

	//see http://sdldoc.csn.ul.ie/sdlcreatecursor.php for documentation on
	//the format that data has to be in to pass to SDL_CreateCursor
	surface_lock lock(nsurf);
	const Uint32* const pixels = reinterpret_cast<Uint32*>(lock.pixels());
	for(int y = 0; y != nsurf->h; ++y) {
		for(int x = 0; x != nsurf->w; ++x) {
			Uint8 r,g,b,a;
			Uint8 trans = 0;
			Uint8 black = 0;

			const size_t index = y*cursor_width + x;

			if ((size_t)x < cursor_width) {
				SDL_GetRGBA(pixels[y*nsurf->w + x],nsurf->format,&r,&g,&b,&a);

				const size_t shift = 7 - (index%8);

				trans = (a < 128 ? 0 : 1) << shift;
				black = (trans == 0 || (r+g+b)/3 > 128 ? 0 : 1) << shift;

				data[index/8] |= black;
				mask[index/8] |= trans;
			}


		}
	}

	return SDL_CreateCursor(&data[0],&mask[0],cursor_width,nsurf->h,0,0);
}

namespace {

SDL_Cursor* cache[cursor::NUM_CURSORS] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

//this array must have members corresponding to cursor::CURSOR_TYPE enum members
//Apple need 16x16 b&w cursors
#ifdef __APPLE__
const std::string bw_images[cursor::NUM_CURSORS] = { "normal.png", "wait-alt.png", "move.png", "attack.png", "select.png", "move_drag_alt.png" , "attack_drag_alt.png", "no_cursor.png"};
#else
const std::string bw_images[cursor::NUM_CURSORS] = { "normal.png", "wait.png", "move.png", "attack.png", "select.png", "move_drag.png", "attack_drag.png", "no_cursor.png"};
#endif

const std::string colour_images[cursor::NUM_CURSORS] = { "normal.png", "wait.png", "move.png", "attack.png", "select.png", "move_drag.png", "attack_drag.png", ""};

// position of the click point from the normal topleft
const int shift_x[cursor::NUM_CURSORS] = {0, 0, 0, 0, 0, 2, 3, 0};
const int shift_y[cursor::NUM_CURSORS] = {0, 0, 0, 0, 0, 20, 22, 0};

// The cursor wanted
cursor::CURSOR_TYPE current_cursor = cursor::NUM_CURSORS;
// The cursor used by SDL
cursor::CURSOR_TYPE current_SDL_cursor = cursor::NUM_CURSORS;

int cursor_x = -1, cursor_y = -1;
surface cursor_buf = NULL;
bool have_focus = true;
bool hide_bw = false;
bool colour_ready = false;

}

static SDL_Cursor* get_cursor(cursor::CURSOR_TYPE type)
{
	if(cache[type] == NULL) {
		static const std::string prefix = "cursors-bw/";
		const surface surf(image::get_image(prefix + bw_images[type],image::UNSCALED));
		cache[type] = create_cursor(surf);
	}

	return cache[type];
}

static void clear_cache()
{
	for(size_t n = 0; n != cursor::NUM_CURSORS; ++n) {
		if(cache[n] != NULL) {
			SDL_FreeCursor(cache[n]);
			cache[n] = NULL;
		}
	}

	if(cursor_buf != NULL) {
		cursor_buf = NULL;
	}
}

namespace cursor
{

manager::manager()
{
	use_colour(use_colour_cursors());
}

manager::~manager()
{
	clear_cache();
	SDL_ShowCursor(SDL_ENABLE);
}

void use_colour(bool value)
{
	// NOTE: Disable the cursor seems to cause slow mouse in fullscreen.
	// So we just use a transparent one
	if(game_config::editor == false) {
		//SDL_ShowCursor(value ? SDL_DISABLE : SDL_ENABLE);
		hide_bw = value;
		//with this, we will force an update of the SDL_Cursor
		current_SDL_cursor = cursor::NUM_CURSORS;
		set(current_cursor);
	}
}

void temporary_use_bw()
{
	colour_ready = false;
	set(current_cursor);
}

void set(CURSOR_TYPE type)
{
	current_cursor = type;

	if(type == NUM_CURSORS) {
		return;
	}
	const CURSOR_TYPE new_cursor = hide_bw && colour_ready ? cursor::NO_CURSOR : type;
	if (new_cursor != current_SDL_cursor) {
		SDL_Cursor * cursor_image = get_cursor(new_cursor);
		if (cursor_image != NULL) {
			SDL_SetCursor(cursor_image);
			current_SDL_cursor = new_cursor;
		}
	}
}

void set_dragging(bool drag)
{
	switch(current_cursor) {
		case MOVE:
			if (drag) cursor::set(MOVE_DRAG);
			break;
		case ATTACK:
			if (drag) cursor::set(ATTACK_DRAG);
			break;
		case MOVE_DRAG:
			if (!drag) cursor::set(MOVE);
			break;
		case ATTACK_DRAG:
			if (!drag) cursor::set(ATTACK);
			break;
		default:
			break;
	}
}

CURSOR_TYPE get()
{
	return current_cursor;
}

void set_focus(bool focus)
{
	have_focus = focus;
}

setter::setter(CURSOR_TYPE type) : old_(current_cursor)
{
	set(type);
}

setter::~setter()
{
	set(old_);
}

void draw(surface screen)
{
	if(use_colour_cursors() == false) {
		return;
	}
	
	if (!colour_ready) {
		// display start to draw cursor
		// so it can now display colour cursor
		colour_ready = true;
		// just reset the cursor will hide the b&w
		set(current_cursor);
	}

	if(current_cursor == NUM_CURSORS) {
		return;
	}

	if(have_focus == false) {
		cursor_buf = NULL;
		return;
	}

	//FIXME: don't parse the file path every time
	const surface surf(image::get_image("cursors/" + colour_images[current_cursor],image::UNSCALED));
	if(surf == NULL) {
		//fall back to b&w cursors
		std::cerr << "could not load colour cursors. Falling back to hardware cursors\n";
		preferences::set_colour_cursors(false);
		return;
	}

	if(cursor_buf != NULL && (cursor_buf->w != surf->w || cursor_buf->h != surf->h)) {
		cursor_buf = NULL;
	}

	if(cursor_buf == NULL) {
		cursor_buf = create_compatible_surface(surf);
		if(cursor_buf == NULL) {
			std::cerr << "Could not allocate surface for mouse cursor\n";
			return;
		}
	}

	int new_cursor_x, new_cursor_y;
	SDL_GetMouseState(&new_cursor_x,&new_cursor_y);
	const bool must_update = new_cursor_x != cursor_x || new_cursor_y != cursor_y;
	cursor_x = new_cursor_x;
	cursor_y = new_cursor_y;

	//save the screen area where the cursor is being drawn onto the back buffer
	SDL_Rect area = {cursor_x - shift_x[current_cursor], cursor_y - shift_y[current_cursor],surf->w,surf->h};
	SDL_BlitSurface(screen,&area,cursor_buf,NULL);

	//blit the surface
	SDL_BlitSurface(surf,NULL,screen,&area);

	if(must_update) {
		update_rect(area);
	}
}

void undraw(surface screen)
{
	if(use_colour_cursors() == false) {
		return;
	}

	if(cursor_buf == NULL) {
		return;
	}

	SDL_Rect area = {cursor_x - shift_x[current_cursor], cursor_y - shift_y[current_cursor],cursor_buf->w,cursor_buf->h};
	SDL_BlitSurface(cursor_buf,NULL,screen,&area);
	update_rect(area);
}

}
