/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "theme.hpp"
#include "tooltips.hpp"
#include "util.hpp"
#include "wassert.hpp"

#include "SDL_image.h"

editor_display::editor_display(CVideo& video, const gamemap& map,
		const config& theme_cfg, const config& cfg, 
		const config& level) :
	display(video, map, theme_cfg, cfg, level)
{
	//clear the screen contents
	surface const disp(screen_.getSurface());
	SDL_Rect area = screen_area();
	SDL_FillRect(disp,&area,SDL_MapRGB(disp->format,0,0,0));
}

void editor_display::draw(bool update,bool force)
{
	bool changed = display::draw_init();

	//int simulate_delay = 0;
	if(!map_.empty() && !invalidated_.empty()) {
		changed = true;
		
		SDL_Rect clip_rect = map_area();
		surface const dst(screen_.getSurface());
		clip_rect_setter set_clip_rect(dst, clip_rect);

		std::set<gamemap::location>::const_iterator it;
		for(it = invalidated_.begin(); it != invalidated_.end(); ++it) {
			image::TYPE image_type = image::SCALED_TO_HEX;

			if(*it == mouseoverHex_ && map_.on_board(mouseoverHex_)) {
				image_type = image::BRIGHTENED;
			}
			else if (highlighted_locations_.find(*it) != highlighted_locations_.end()) {
				image_type = image::SEMI_BRIGHTENED;
			}

			if(screen_.update_locked()) {
				continue;
			}

			int xpos = int(get_location_x(*it));
			int ypos = int(get_location_y(*it));

			if(xpos >= clip_rect.x + clip_rect.w || ypos >= clip_rect.y + clip_rect.h ||
			   xpos + zoom_ < clip_rect.x || ypos + zoom_ < clip_rect.y) {
				continue;
			}

			tile_stack_clear();

			const std::string nodarken = "morning";
			tile_stack_terrains(*it,nodarken,image_type,ADJACENT_BACKGROUND);
			tile_stack_terrains(*it,nodarken,image_type,ADJACENT_FOREGROUND);

			// draw the grid, if that's been enabled 
			if(grid_) {
				tile_stack_append(image::get_image(game_config::grid_image));
			}

			// paint selection and mouseover overlays
			if(*it == selectedHex_ && map_.on_board(selectedHex_) && selected_hex_overlay_ != NULL)
				tile_stack_append(selected_hex_overlay_);
			if(*it == mouseoverHex_ && map_.on_board(mouseoverHex_) && mouseover_hex_overlay_ != NULL)
				tile_stack_append(mouseover_hex_overlay_);

			tile_stack_render(xpos, ypos);
		}

		invalidated_.clear();
	} else if (!map_.empty()) {
		wassert(invalidated_.empty());
	}

	display::draw_wrap(update, force, changed);
}


