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

#include "global.hpp"

#include "actions.hpp"
#include "cursor.hpp"
#include "game_display.hpp"
#include "events.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "minimap.hpp"
#include "game_preferences.hpp"
#include "sdl_utils.hpp"
#include "sound.hpp"
#include "team.hpp"
#include "theme.hpp"
#include "tooltips.hpp"
#include "unit_display.hpp"
#include "util.hpp"
#include "wassert.hpp"

#include "SDL_image.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

#define ERR_DP LOG_STREAM(err, display)
#define INFO_DP LOG_STREAM(info, display)

std::map<gamemap::location,fixed_t> game_display::debugHighlights_;

game_display::game_display(unit_map& units, CVideo& video, const gamemap& map,
		const gamestatus& status, const std::vector<team>& t,
		const config& theme_cfg, const config& cfg, const config& level) :
	display(video, map, theme_cfg, cfg, level),
	units_(units),
	temp_unit_(NULL),
	status_(status),
	teams_(t),
	level_(level),
	invalidateUnit_(true),
	currentTeam_(0), activeTeam_(0),
	sidebarScaling_(1.0),
	first_turn_(true), in_game_(false), 
	tod_hex_mask1(NULL), tod_hex_mask2(NULL), reach_map_changed_(true)
{
	singleton_ = this;

	//inits the flag list and the team colors used by ~TC
	flags_.reserve(teams_.size());

	std::vector<std::string> side_colors;
	side_colors.reserve(teams_.size());

	for(size_t i = 0; i != teams_.size(); ++i) {
		std::string side_color = team::get_side_colour_index(i+1);
		side_colors.push_back(side_color);

		std::string flag = teams_[i].flag();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = side_color;

		if(flag.empty()) {
			flag = game_config::flag_image;
		}

		LOG_STREAM(info, display) << "Adding flag for team " << i << " from animation " << flag << "\n";

		//must recolor flag image
		animated<image::locator> temp_anim;

		std::vector<std::string> items = utils::split(flag);
		std::vector<std::string>::const_iterator itor = items.begin();
		for(; itor != items.end(); ++itor) {
			const std::vector<std::string>& items = utils::split(*itor, ':');
			std::string str;
			int time;

			if(items.size() > 1) {
				str = items.front();
				time = atoi(items.back().c_str());
			} else {
				str = *itor;
				time = 100;
			}
			std::stringstream temp;
			temp << str << "~RC(" << old_rgb << ">"<< new_rgb << ")";
			image::locator flag_image(temp.str());
			temp_anim.add_frame(time, flag_image);
		}
		flags_.push_back(temp_anim);

		flags_.back().start_animation(rand()%flags_.back().get_end_time(), true);
	}
	image::set_team_colors(side_colors);

	//clear the screen contents
	surface const disp(screen_.getSurface());
	SDL_Rect area = screen_area();
	SDL_FillRect(disp,&area,SDL_MapRGB(disp->format,0,0,0));
}

game_display::~game_display()
{
	// SDL_FreeSurface(minimap_);
	prune_chat_messages(true);
	singleton_ = NULL;
}

void game_display::new_turn()
{
	const time_of_day& tod = status_.get_time_of_day();

	if( !first_turn_) {
		image::set_image_mask("");

		const time_of_day& old_tod = status_.get_previous_time_of_day();

		if(old_tod.image_mask != tod.image_mask) {
			const surface old_mask(image::get_image(old_tod.image_mask,image::UNMASKED));
			const surface new_mask(image::get_image(tod.image_mask,image::UNMASKED));

			const int niterations = (int)(10/turbo_speed());
			const int frame_time = 30;
			const int starting_ticks = SDL_GetTicks();
			for(int i = 0; i != niterations; ++i) {

				if(old_mask != NULL) {
					const fixed_t proportion = ftofxp(1.0) - fxpdiv(i,niterations);
					tod_hex_mask1.assign(adjust_surface_alpha(old_mask,proportion));
				}

				if(new_mask != NULL) {
					const fixed_t proportion = fxpdiv(i,niterations);
					tod_hex_mask2.assign(adjust_surface_alpha(new_mask,proportion));
				}

				invalidate_all();
				draw();

				const int cur_ticks = SDL_GetTicks();
				const int wanted_ticks = starting_ticks + i*frame_time;
				if(cur_ticks < wanted_ticks) {
					SDL_Delay(wanted_ticks - cur_ticks);
				}
			}
		}

		tod_hex_mask1.assign(NULL);
		tod_hex_mask2.assign(NULL);
	}

	first_turn_ = false;

	image::set_colour_adjustment(tod.red,tod.green,tod.blue);
	image::set_image_mask(tod.image_mask);

	invalidate_all();
	draw();
}

void game_display::adjust_colours(int r, int g, int b)
{
	const time_of_day& tod = status_.get_time_of_day();
	image::set_colour_adjustment(tod.red+r,tod.green+g,tod.blue+b);
}

void game_display::select_hex(gamemap::location hex)
{
	if(fogged(hex)) {
		return;
	}
	display::select_hex(hex);
	invalidate_unit();
}

void game_display::highlight_hex(gamemap::location hex)
{
	const int has_unit = units_.count(mouseoverHex_) + units_.count(hex);

	display::highlight_hex(hex);
	invalidate_game_status();

	if(has_unit) {
		invalidate_unit();
	}
}

void game_display::scroll_to_leader(unit_map& units, int side)
{
	const unit_map::iterator leader = find_leader(units,side);

	if(leader != units_.end()) {
		/* YogiHH: I can't see why we need another key_handler
		here, therefore i will comment it out const
		hotkey::basic_handler key_events_handler(gui_);
		*/
		scroll_to_tile(leader->first, ONSCREEN);
	}
}


void game_display::draw(bool update,bool force)
{
	if (screen_.update_locked()) {
		return;
	}

	bool changed = display::draw_init();

	//log_scope("Drawing");
	invalidate_animations();

	process_reachmap_changes();

	//FIXME: must modify changed, but best to do it at the
	//floating_label level 
	prune_chat_messages();

	if(map_.empty()) {
		display::draw_wrap(update, force, changed);
		return;
	}

	halo::unrender(invalidated_);

	//int simulate_delay = 0;
	if(!invalidated_.empty()) {
		changed = true;

		// z-ordered set to store invalidated units
		std::set<gamemap::location, ordered_draw> unit_invals;

		const time_of_day& tod = status_.get_time_of_day();
		const std::string shroud_image = "terrain/" +
			map_.get_terrain_info(t_translation::VOID_TERRAIN).symbol_image() + ".png";
		const std::string fog_image = "terrain/" +
			map_.get_terrain_info(t_translation::FOGGED).symbol_image() + ".png";
			
		SDL_Rect clip_rect = map_area();
		surface const dst(screen_.getSurface());
		clip_rect_setter set_clip_rect(dst, clip_rect);

		std::set<gamemap::location>::const_iterator it;
		for(it = invalidated_.begin(); it != invalidated_.end(); ++it) {
			int xpos = get_location_x(*it);
			int ypos = get_location_y(*it);

			//store invalidated units
			if ((temp_unit_ && temp_unit_loc_==*it) || units_.find(*it) != units_.end()) {
				unit_invals.insert(*it);
			}

			SDL_Rect hex_rect = {xpos, ypos, zoom_, zoom_};
			if(!rects_overlap(hex_rect,clip_rect)) {
				continue;
			}

			// If the terrain is off the map it shouldn't
			// be included for reachmap, fog, shroud and
			// the grid.  In the future it may not depend
			// on whether the location is on the map but
			// whether it's an _off^* terrain.  (atm not
			// too happy with how the grid looks) (the
			// shroud has some glitches due to commented
			// out code but enabling it looks worse)
			const bool on_map = map_.on_board(*it);
			const bool is_shrouded = shrouded(*it);

			image::TYPE image_type = image::SCALED_TO_HEX;

			//we higlight hex under the mouse, origin of
			//attack or under a selected unit
			if (on_map && (*it == mouseoverHex_ || *it == attack_indicator_from_)) {
				image_type = image::BRIGHTENED;
			} else if (on_map && *it == selectedHex_) {
				unit_map::iterator un = find_visible_unit(units_, *it, map_,
				teams_,teams_[currentTeam_]);
				if (un != units_.end()) {
					image_type = image::BRIGHTENED;
				}
			}

			//currently only used in editor
			/*
				else if (highlighted_locations_.find(*it) != highlighted_locations_.end()) {
				image_type = image::SEMI_BRIGHTENED;
			}
			*/

			tile_stack_clear();

			if(!is_shrouded /*|| !on_map*/) {
				// unshrouded terrain (the normal case)
				tile_stack_terrains(*it,tod.id, image_type, ADJACENT_BACKGROUND);

				// village-control flags.
				tile_stack_append(get_flag(*it));

				typedef overlay_map::const_iterator Itor;

				for(std::pair<Itor,Itor> overlays = overlays_.equal_range(*it);
					overlays.first != overlays.second; ++overlays.first) {

					tile_stack_append(image::get_image(overlays.first->second.image,image_type));
				}
			}

			if(!is_shrouded /*|| !on_map*/) {
				tile_stack_terrains(*it,tod.id,image_type,ADJACENT_FOREGROUND);
			}

			//draw the time-of-day mask on top of the
			//terrain in the hex
			// tod may differ from tod if hex is illuminated
			std::string tod_hex_mask = timeofday_at(status_,units_,*it,map_).image_mask;
			if(tod_hex_mask1 != NULL || tod_hex_mask2 != NULL) {
				tile_stack_append(tod_hex_mask1);
				tile_stack_append(tod_hex_mask2);
			} else if(tod_hex_mask != "") {
				tile_stack_append(image::get_image(tod_hex_mask,image::UNMASKED));
			}

			// draw the grid, if that's been enabled 
			if(grid_ && !is_shrouded && on_map) {
				tile_stack_append(image::get_image(game_config::grid_image, image::SCALED_TO_HEX));
			}

			// draw reach_map information
			if (!reach_map_.empty() && !is_shrouded) {
				reach_map::iterator reach = reach_map_.find(*it);
				if (reach == reach_map_.end()) {
					tile_stack_append(image::get_image(game_config::unreachable_image,image::UNMASKED));
				} else if (reach->second > 1) {
					//FIXME: this is a temporary hack to render/flush
					//the stack, so we can draw text
					tile_stack_render(xpos, ypos);
					const std::string num = lexical_cast<std::string>(reach->second);
					draw_text_in_hex(*it, num, font::SIZE_PLUS, font::YELLOW_COLOUR);
				}
			}

			// draw cross images for debug highlights
			if(game_config::debug && debugHighlights_.count(*it)) {
				tile_stack_append(image::get_image(game_config::cross_image, image::SCALED_TO_HEX));
			}

			// Add the top layer overlay surfaces
			if(!hex_overlay_.empty()) {
				std::map<gamemap::location, surface>::const_iterator itor = hex_overlay_.find(*it);
				if(itor != hex_overlay_.end())
					tile_stack_append(itor->second);
			}

			// footsteps indicating a movement path
			tile_stack_append(footstep_image(*it));

			// paint selection and mouseover overlays
			if(*it == selectedHex_ && on_map && selected_hex_overlay_ != NULL)
				tile_stack_append(selected_hex_overlay_);
			if(*it == mouseoverHex_ && on_map && mouseover_hex_overlay_ != NULL)
				tile_stack_append(mouseover_hex_overlay_);

			// draw the attack direction indicator
			if(*it == attack_indicator_to_) {
				tile_stack_append(image::get_image(attack_indicator_image_, image::UNMASKED));
			}

			// apply shroud anf fog
			if(is_shrouded) {
				// we apply void also on off-map tiles to
				// shroud the half-hexes too
				tile_stack_append(image::get_image(shroud_image, image::SCALED_TO_HEX));
			} else if(fogged(*it)) {
				tile_stack_append(image::get_image(fog_image, image::SCALED_TO_HEX));
			}

			if(!is_shrouded) {
				tile_stack_terrains(*it,tod.id,image_type,ADJACENT_FOGSHROUD);
			}

			tile_stack_render(xpos, ypos);

			// show def% and turn to reach infos
			if(!is_shrouded && on_map) {
				draw_movement_info(*it);
			}
			//simulate_delay += 1;
			
			// if the tile is at the border we start to blend it
			// NOTE we assume a half hex border!!!
			// NOTE everything is hardcoded but it will be moved to the theme code
			// in the near future and then also become a separate function
			if(!on_map && map_.get_terrain(*it) != t_translation::OFF_MAP_USER) {
				// first handle the corners 
				if(it->x == -1 && it->y == -1) { // top left corner
					SDL_Rect rect = { xpos + zoom_/4, ypos, 3 * zoom_/4, zoom_ } ;
					const surface border(image::get_image("terrain/off-map/fade_corner_top_left.png", image::SCALED_TO_ZOOM));

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				} else if(it->x == map_.x() && it->y == -1) { // top right corner
					SDL_Rect rect = { xpos, -1, 3 * zoom_/4, zoom_ } ;
					surface border;
					if(it->x%2 == 0) {
						rect.y = ypos + zoom_/2;
						rect.h = zoom_/2;
						// we use the map idea of odd and even and map coords are internal coords + 1
						border = image::get_image("terrain/off-map/fade_corner_top_right_odd.png", image::SCALED_TO_ZOOM);
					} else {
						rect.y = ypos;
						border = image::get_image("terrain/off-map/fade_corner_top_right_even.png", image::SCALED_TO_ZOOM);
					}

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				} else if(it->x == -1 && it->y == map_.y()) { // bottom left corner
					SDL_Rect rect = { xpos + zoom_/4, ypos, 3 * zoom_/4, zoom_/2 } ;

					const surface border(image::get_image("terrain/off-map/fade_corner_bottom_left.png", image::SCALED_TO_ZOOM));

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);
					
				} else if(it->x == map_.x() && it->y == map_.y()) { // bottom right corner
					SDL_Rect rect = { xpos, ypos, 3 * zoom_/4, zoom_/2 } ;
					surface border;
					if(it->x%2 == 1) {
						// we use the map idea of odd and even and map coords are internal coords + 1
						border = image::get_image("terrain/off-map/fade_corner_bottom_right_even.png", image::SCALED_TO_ZOOM);
					} else {
						border = image::get_image("terrain/off-map/fade_corner_bottom_right_odd.png", image::SCALED_TO_ZOOM);
					}

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				// now handle the sides
				} else if(it->x == -1) { // left side
					SDL_Rect rect = { xpos + zoom_/4 , ypos, zoom_/2, zoom_ } ;
					const surface border(image::get_image("terrain/off-map/fade_border_left.png", image::SCALED_TO_ZOOM));

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				} else if(it->x == map_.x()) { // right side
					SDL_Rect rect = { xpos + zoom_/4 , ypos, zoom_/2, zoom_ } ;
					const surface border(image::get_image("terrain/off-map/fade_border_right.png", image::SCALED_TO_ZOOM));

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				} else if(it->y == -1) { // top side
					SDL_Rect rect = { xpos, -1, zoom_, zoom_/2 } ;
					surface border;

					if(it->x%2 == 1) {
						rect.y = ypos;
						// we use the map idea of odd and even and map coords are internal coords + 1
						border = image::get_image("terrain/off-map/fade_border_top_odd.png", image::SCALED_TO_ZOOM);
					} else {
						rect.y = ypos + zoom_/2;
						border = image::get_image("terrain/off-map/fade_border_top_even.png", image::SCALED_TO_ZOOM);
					}

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);

				} else if(it->y == map_.y()) { // bottom side
					SDL_Rect rect = { xpos, -1, zoom_, zoom_/2 } ;
					surface border;

					if(it->x%2 == 1) {
						rect.y = ypos;
						// NOTE here is the internal idea off odd and even used
						border = image::get_image("terrain/off-map/fade_border_bottom_odd.png", image::SCALED_TO_ZOOM);
					} else {
						rect.y = ypos + zoom_/2;
						border = image::get_image("terrain/off-map/fade_border_bottom_even.png", image::SCALED_TO_ZOOM);
					}

					SDL_BlitSurface( border, NULL, screen_.getSurface(), &rect);
				}
			}
		}

		// Units can overlap multiple hexes, so we need to
		// redraw them last and in the good sequence.
		std::set<gamemap::location, struct display::ordered_draw>::const_iterator it2;
		for(it2 = unit_invals.begin(); it2 != unit_invals.end(); ++it2) {
			unit_map::iterator u_it = units_.find(*it2);
			if (u_it != units_.end()) {
				u_it->second.redraw_unit(*this, *it2);
				//simulate_delay += 1;
			}

			if (temp_unit_ && temp_unit_loc_ == *it2) {
				temp_unit_->redraw_unit(*this, temp_unit_loc_);
				//simulate_delay += 1;
			}
		}

		invalidated_.clear();
	}

	halo::render();

	draw_sidebar();
	//FIXME: This changed can probably be smarter
	changed = true;  

	// simulate slow pc
	//SDL_Delay(2*simulate_delay + rand() % 20);

	display::draw_wrap(update, force, changed);
}

void game_display::draw_report(reports::TYPE report_num)
{
	bool brighten;

	if(!team_valid()) {
		return;
	}

	reports::report report = reports::generate_report(report_num,report_,map_,
							  units_, teams_,
							  teams_[viewing_team()],
							  size_t(currentTeam_+1),size_t(activeTeam_+1),
							  selectedHex_,mouseoverHex_,status_,observers_,level_);

	brighten = false;
	if(report_num == reports::TIME_OF_DAY) {
		time_of_day tod = timeofday_at(status_,units_,mouseoverHex_,map_);
		// don't show illuminated time on fogged/shrouded tiles
		if (teams_[viewing_team()].fogged(mouseoverHex_.x, mouseoverHex_.y) || 
				teams_[viewing_team()].shrouded(mouseoverHex_.x, mouseoverHex_.y)) {

			tod = status_.get_time_of_day(false,mouseoverHex_);
		}
		brighten = (tod.bonus_modified > 0);
	}

	refresh_report(report_num, report, brighten);
}

void game_display::draw_game_status()
{

	if(teams_.empty()) {
		return;
	}

	for(size_t r = reports::STATUS_REPORTS_BEGIN; r != reports::STATUS_REPORTS_END; ++r) {
		draw_report(reports::TYPE(r));
	}

	// the mouse-over needs to update the visibility icon
	draw_report(reports::UNIT_STATUS);
}

void game_display::draw_sidebar()
{
	draw_report(reports::REPORT_CLOCK);
	draw_report(reports::REPORT_COUNTDOWN);

	if(teams_.empty()) {
		return;
	}

	if(invalidateUnit_) {
		//we display the unit the mouse is over if it is over a unit
		//otherwise we display the unit that is selected
		unit_map::const_iterator i =
			find_visible_unit(units_,mouseoverHex_,
					map_,
					teams_,teams_[viewing_team()]);

		if(i == units_.end()) {
			i = find_visible_unit(units_,selectedHex_,
					map_,
					teams_,teams_[viewing_team()]);
		}

		if(i != units_.end()) {
			for(size_t r = reports::UNIT_REPORTS_BEGIN; r != reports::UNIT_REPORTS_END; ++r) {
				draw_report(reports::TYPE(r));
			}
		}

		invalidateUnit_ = false;
	}

	if(invalidateGameStatus_) {
		draw_game_status();
		invalidateGameStatus_ = false;
	}
}

void game_display::draw_minimap_units(int x, int y, int w, int h)
{
	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u) {
		if(fogged(u->first) ||
				(teams_[currentTeam_].is_enemy(u->second.side()) &&
				u->second.invisible(u->first,units_,teams_))) {
			continue;
		}

		const int side = u->second.side();
		const SDL_Color col = team::get_minimap_colour(side);
		const Uint32 mapped_col = SDL_MapRGB(video().getSurface()->format,col.r,col.g,col.b);
		SDL_Rect rect = { x + (u->first.x * w) / map_.x(),
		                  y + (u->first.y * h + (is_odd(u->first.x) ? h / 2 : 0)) / map_.y(),
		                  w / map_.x(), h / map_.y() };
		SDL_FillRect(video().getSurface(),&rect,mapped_col);
	}
}

void game_display::draw_bar(const std::string& image, int xpos, int ypos, size_t height, double filled, const SDL_Color& col, fixed_t alpha)
{
	filled = minimum<double>(maximum<double>(filled,0.0),1.0);
	height = static_cast<size_t>(height*get_zoom_factor());
#ifdef USE_TINY_GUI
	height /= 2;
#endif

	surface surf(image::get_image(image,image::UNMASKED));

	// we use UNSCALED because scaling (and bilinear interpolaion )
	// is bad for calculate_energy_bar. But we will do a geometric scaling later
	surface bar_surf(image::get_image(image));
	if(surf == NULL || bar_surf == NULL) {
		return;
	}

	// calculate_energy_bar returns incorrect results if the surface colors
	// have changed (for example, due to bilinear interpolaion)
	const SDL_Rect& unscaled_bar_loc = calculate_energy_bar(bar_surf);

	SDL_Rect bar_loc;
	if (surf->w == bar_surf->w && surf->h == bar_surf->h)
	  bar_loc = unscaled_bar_loc;
	else {
	  const fixed_t xratio = fxpdiv(surf->w,bar_surf->w);
	  const fixed_t yratio = fxpdiv(surf->h,bar_surf->h);
	  const SDL_Rect scaled_bar_loc = {fxptoi(unscaled_bar_loc. x * xratio),
					   fxptoi(unscaled_bar_loc. y * yratio + 127),
					   fxptoi(unscaled_bar_loc. w * xratio + 255),
					   fxptoi(unscaled_bar_loc. h * yratio + 255)};
	  bar_loc = scaled_bar_loc;
	}

	if(height > bar_loc.h) {
		height = bar_loc.h;
	}

	//if(alpha != ftofxp(1.0)) {
	//	surf.assign(adjust_surface_alpha(surf,alpha));
	//	if(surf == NULL) {
	//		return;
	//	}
	//}

	const size_t skip_rows = bar_loc.h - height;

	SDL_Rect top = {0,0,surf->w,bar_loc.y};
	SDL_Rect bot = {0,bar_loc.y+skip_rows,surf->w,0};
	bot.h = surf->w - bot.y;

	video().blit_surface(xpos,ypos,surf,&top);
	video().blit_surface(xpos,ypos+top.h,surf,&bot);

	const size_t unfilled = (const size_t)(height*(1.0 - filled));

	if(unfilled < height && alpha >= ftofxp(0.3)) {
		SDL_Rect filled_area = {xpos+bar_loc.x,ypos+bar_loc.y+unfilled,bar_loc.w,height-unfilled};
		const Uint32 colour = SDL_MapRGB(video().getSurface()->format,col.r,col.g,col.b);
		const Uint8 r_alpha = minimum<unsigned>(unsigned(fxpmult(alpha,255)),255);
		fill_rect_alpha(filled_area,colour,r_alpha,video().getSurface());
	}
}

void game_display::draw_movement_info(const gamemap::location& loc)
{

	//check if there is a path and if we are not at its start
	//because we don't want to display movement info on the unit's
	//hex (useless and unreadable)
	if (route_.steps.empty() || route_.steps.front() == loc) return;

	//search if there is a turn waypoint here
	std::map<gamemap::location, int>::iterator turn_waypoint_iter = route_.turn_waypoints.find(loc);
	if(turn_waypoint_iter == route_.turn_waypoints.end()) return;

	//display the def% of this terrain
#ifndef USE_TINY_GUI
	const unit_map::const_iterator un = units_.find(route_.steps.front());
	if(un != units_.end()) {
		const int def =  100 - un->second.defense_modifier(map_.get_terrain(loc));
		std::stringstream def_text;
		def_text << def << "%";

		// with 11 colors, the last one will be used only for def=100
		int val = (game_config::defense_color_scale.size()-1) * def/100;
		SDL_Color color = int_to_color(game_config::defense_color_scale[val]);

		draw_text_in_hex(loc, def_text.str(), font::SIZE_LARGE, color, 0.5, 0.5);
	}
#endif

	//display the number of turn to reach if > 0
	int turns_to_reach = turn_waypoint_iter->second;
	if(turns_to_reach > 0 && turns_to_reach < 10) {
		std::stringstream turns_text;
		turns_text << "(" << turns_to_reach << ")";
		const SDL_Color turns_color = font::NORMAL_COLOUR;
#ifndef USE_TINY_GUI
		draw_text_in_hex(loc, turns_text.str(), font::SIZE_PLUS, turns_color, 0.5, 0.8);
#else
		draw_text_in_hex(loc, turns_text.str(), font::SIZE_PLUS, turns_color, 0.5, 0.5);
#endif
	}
}

surface game_display::footstep_image(const gamemap::location& loc)
{
	std::vector<gamemap::location>::const_iterator i =
	         std::find(route_.steps.begin(),route_.steps.end(),loc);

	if(i == route_.steps.begin() || i == route_.steps.end()) {
		return NULL;
	}

	const bool left_foot = is_even(i - route_.steps.begin());

	//generally we want the footsteps facing toward the direction
	//they're going to go next.  if we're on the last step, then
	//we want them facing according to where they came from, so we
	//move i back by one
	if(i+1 == route_.steps.end() && i != route_.steps.begin()) {
		--i;
	}

	gamemap::location::DIRECTION direction = gamemap::location::NORTH;

	if(i+1 != route_.steps.end()) {
		for(int n = 0; n != 6; ++n) {
			direction = gamemap::location::DIRECTION(n);
			if(i->get_direction(direction) == *(i+1)) {
				break;
			}
		}
	}

	const std::vector<std::string>* image_category = NULL;


	if(left_foot) {
		if(direction == gamemap::location::NORTH ||
		   direction == gamemap::location::SOUTH) {
			image_category = &game_config::foot_left_n;
		} else {
			image_category = &game_config::foot_left_nw;
		}
	} else {
		if(direction == gamemap::location::NORTH ||
		   direction == gamemap::location::SOUTH) {
			image_category = &game_config::foot_right_n;
		} else {
			image_category = &game_config::foot_right_nw;
		}
	}

	if(image_category == NULL || image_category->empty()) {
		return NULL;
	}

	const std::string* image_str = &image_category->front();
	const unit_map::const_iterator un = units_.find(route_.steps.front());
	if(un != units_.end()) {
		const int move_cost = un->second.movement_cost(map_.get_terrain(loc)) - 1;
		if(move_cost >= int(image_category->size())) {
			image_str = &image_category->back();
		} else if(move_cost > 0) {
			image_str = &(*image_category)[move_cost];
		}
	}

	surface image(image::get_image(*image_str, image::UNMASKED));
	if(image == NULL) {
		ERR_DP << "Could not find image: " << *image_str << "\n";
		return NULL;
	}

	const bool hflip = !(direction > gamemap::location::NORTH &&
	                     direction <= gamemap::location::SOUTH);
	const bool vflip = (direction >= gamemap::location::SOUTH_EAST &&
	                    direction <= gamemap::location::SOUTH_WEST);

	if(!hflip) {
		image.assign(image::reverse_image(image));
	}
	if(vflip) {
		image.assign(flop_surface(image));
	}

	return image;
}

surface game_display::get_flag(const gamemap::location& loc)
{
	t_translation::t_letter terrain = map_.get_terrain(loc);

	if(!map_.is_village(terrain)) {
		return surface(NULL);
	}

	for(size_t i = 0; i != teams_.size(); ++i) {
		if(teams_[i].owns_village(loc) &&
		  (!fogged(loc) || !teams_[currentTeam_].is_enemy(i+1)))
		{
			flags_[i].update_last_draw_time();
			return image::get_image(flags_[i].get_current_frame(), image::SCALED_TO_HEX);
		}
	}

	return surface(NULL);
}

void game_display::highlight_reach(const paths &paths_list)
{
	unhighlight_reach();
	highlight_another_reach(paths_list);
}

void game_display::highlight_another_reach(const paths &paths_list)
{
	paths::routes_map::const_iterator r;

	// Fold endpoints of routes into reachability map.
	for (r = paths_list.routes.begin(); r != paths_list.routes.end(); ++r) {
		reach_map_[r->first]++;
	}
	reach_map_changed_ = true;
}

void game_display::unhighlight_reach()
{
	reach_map_ = reach_map();
	reach_map_changed_ = true;
}

void game_display::process_reachmap_changes()
{
	if (!reach_map_changed_) return;
	if (reach_map_.empty() != reach_map_old_.empty()) {
		// invalidate everything except the non-darkened tiles
		reach_map &full = reach_map_.empty() ? reach_map_old_ : reach_map_;
		gamemap::location topleft;
		gamemap::location bottomright;
		get_visible_hex_bounds(topleft, bottomright);
		for(int x = topleft.x; x <= bottomright.x; ++x) {
			for(int y = topleft.y; y <= bottomright.y; ++y) {
				gamemap::location loc(x, y);
				reach_map::iterator reach = full.find(loc);
				if (reach == full.end()) {
					// location needs to be darkened or brightened
					invalidate(loc);
				} else if (reach->second != 1) {
					// number needs to be displayed or cleared
					invalidate(loc);
				}
			}
		}
	} else if (!reach_map_.empty()) {
		// invalidate only changes
		reach_map::iterator reach, reach_old;
		for (reach = reach_map_.begin(); reach != reach_map_.end(); ++reach) {
			reach_old = reach_map_old_.find(reach->first);
			if (reach_old == reach_map_old_.end()) {
				invalidate(reach->first);
			} else {
				if (reach_old->second != reach->second) {
					invalidate(reach->first);
				}
				reach_map_old_.erase(reach_old);
			}
		}
		for (reach_old = reach_map_old_.begin(); reach_old != reach_map_old_.end(); ++reach_old) {
			invalidate(reach_old->first);
		}
	}
	reach_map_old_ = reach_map_;
	reach_map_changed_ = false;
}

void game_display::invalidate_route()
{
	for(std::vector<gamemap::location>::const_iterator i = route_.steps.begin();
	    i != route_.steps.end(); ++i) {
		invalidate(*i);
	}
}

void game_display::set_route(const paths::route* route)
{
	invalidate_route();

	if(route != NULL) {
		route_ = *route;
	} else {
		route_.steps.clear();
		route_.turn_waypoints.clear();
	}

	invalidate_route();
}

void game_display::remove_footstep(const gamemap::location& loc)
{
	const std::vector<gamemap::location>::iterator it = std::find(route_.steps.begin(),route_.steps.end(),loc);
	if(it != route_.steps.end()) {
		route_.steps.erase(it);
	}
}

void game_display::float_label(const gamemap::location& loc, const std::string& text,
						  int red, int green, int blue)
{
	if(preferences::show_floating_labels() == false || fogged(loc)) {
		return;
	}

	const SDL_Color color = {red,green,blue,255};
	int lifetime = static_cast<int>(60/turbo_speed());
	font::add_floating_label(text,font::SIZE_XLARGE,color,get_location_x(loc)+zoom_/2,get_location_y(loc),
	                         0,-2*turbo_speed(),lifetime,screen_area(),font::CENTER_ALIGN,NULL,0,font::ANCHOR_LABEL_MAP);
}

struct is_energy_colour {
	bool operator()(Uint32 colour) const { return (colour&0xFF000000) < 0x50000000 &&
	                                              (colour&0x00FF0000) > 0x00990000 &&
												  (colour&0x0000FF00) > 0x00009900 &&
												  (colour&0x000000FF) > 0x00000099; }
};

const SDL_Rect& game_display::calculate_energy_bar(surface surf)
{
	const std::map<surface,SDL_Rect>::const_iterator i = energy_bar_rects_.find(surf);
	if(i != energy_bar_rects_.end()) {
		return i->second;
	}

	int first_row = -1, last_row = -1, first_col = -1, last_col = -1;

	surface image(make_neutral_surface(surf));

	surface_lock image_lock(image);
	const Uint32* const begin = image_lock.pixels();

	for(int y = 0; y != image->h; ++y) {
		const Uint32* const i1 = begin + image->w*y;
		const Uint32* const i2 = i1 + image->w;
		const Uint32* const itor = std::find_if(i1,i2,is_energy_colour());
		const int count = std::count_if(itor,i2,is_energy_colour());

		if(itor != i2) {
			if(first_row == -1) {
				first_row = y;
			}

			first_col = itor - i1;
			last_col = first_col + count;
			last_row = y;
		}
	}

	const SDL_Rect res = {first_col,first_row,last_col-first_col,last_row+1-first_row};
	energy_bar_rects_.insert(std::pair<surface,SDL_Rect>(surf,res));
	return calculate_energy_bar(surf);
}

void game_display::invalidate(const gamemap::location& loc)
{
	if(!invalidateAll_) {
		if (invalidated_.insert(loc).second) {
			// Units can overlap adjacent tiles.
			unit_map::iterator u = units_.find(loc);

			if (u != units_.end()) {
				std::set<gamemap::location> overlaps = u->second.overlaps(u->first);
				for (std::set<gamemap::location>::iterator i = overlaps.begin(); i != overlaps.end(); i++) {
					invalidate(*i);
				}
			}
			if (temp_unit_  && temp_unit_loc_ == loc ) {
				std::set<gamemap::location> overlaps = temp_unit_->overlaps(temp_unit_loc_);
				for (std::set<gamemap::location>::iterator i = overlaps.begin(); i != overlaps.end(); i++) {
					invalidate(*i);
				}
			}
			// if neighbour has a unit which overlaps us invalidate him
			gamemap::location adjacent[6];
			get_adjacent_tiles(loc, adjacent);
			for (unsigned int i = 0; i < 6; i++) {
				u = units_.find(adjacent[i]);
				if (u != units_.end()) {
					std::set<gamemap::location> overlaps = u->second.overlaps(u->first);
					if (overlaps.find(loc) != overlaps.end()) {
						invalidate(u->first);
					}
				}
				if (temp_unit_  && temp_unit_loc_ == adjacent[i] ) {
					std::set<gamemap::location> overlaps = temp_unit_->overlaps(temp_unit_loc_);
					if (overlaps.find(loc) != overlaps.end()) {
						invalidate(temp_unit_loc_);
					}
				}
			}
		}
	}
}

void game_display::invalidate_animations()
{
	new_animation_frame();
	gamemap::location topleft;
	gamemap::location bottomright;
	get_visible_hex_bounds(topleft, bottomright);
	
	for(int x = topleft.x; x <= bottomright.x; ++x) {
		for(int y = topleft.y; y <= bottomright.y; ++y) {
			const gamemap::location loc(x,y);
			if (!shrouded(loc)) {
				if (builder_.update_animation(loc)) {
					invalidate(loc);
				} else if (map_.is_village(loc)) {
					const int owner = player_teams::village_owner(loc);
					if (owner >= 0 && flags_[owner].need_update() && (!fogged(loc) || !teams_[currentTeam_].is_enemy(owner+1)))
						invalidate(loc);
				}
			}
		}
	}
	unit_map::iterator unit;
	for(unit=units_.begin() ; unit != units_.end() ; unit++) {
		if (unit->second.get_animation() && unit->second.get_animation()->need_update())
			invalidate(unit->first);
		unit->second.refresh(*this,unit->first);
	}
	if (temp_unit_ ) {
		if (temp_unit_->get_animation() && temp_unit_->get_animation()->need_update())
			invalidate(temp_unit_loc_);
		temp_unit_->refresh(*this, temp_unit_loc_);
	}


}

void game_display::debug_highlight(const gamemap::location& loc, fixed_t amount)
{
	wassert(game_config::debug);
	debugHighlights_[loc] += amount;
}

void game_display::place_temporary_unit(unit &u, const gamemap::location& loc)
{
	temp_unit_ = &u;
	temp_unit_loc_ = loc;
	invalidate(loc);
}

void game_display::remove_temporary_unit()
{
	if(!temp_unit_) return;

	invalidate(temp_unit_loc_);
	// redraw with no location to get rid of haloes
	temp_unit_->clear_haloes();
	temp_unit_ = NULL;
}

void game_display::set_attack_indicator(const gamemap::location& from, const gamemap::location& to)
{
	if (attack_indicator_from_ != from || attack_indicator_to_ != to) {
		invalidate(attack_indicator_from_);
		invalidate(attack_indicator_to_);

		attack_indicator_from_ = from;
		attack_indicator_to_ = to;
		std::string dir = gamemap::location::write_direction(to.get_relative_dir(from));
		attack_indicator_image_ = "misc/attack-from-" + dir + ".png";

		invalidate(attack_indicator_from_);
		invalidate(attack_indicator_to_);
	}
}

void game_display::clear_attack_indicator()
{
	set_attack_indicator(gamemap::location::null_location, gamemap::location::null_location);
}

void game_display::add_overlay(const gamemap::location& loc, const std::string& img, const std::string& halo)
{
	const int halo_handle = halo::add(get_location_x(loc) + hex_size() / 2, 
			get_location_y(loc) + hex_size() / 2, halo, loc);

	const overlay item(img,halo,halo_handle);
	overlays_.insert(overlay_map::value_type(loc,item));
}

void game_display::remove_overlay(const gamemap::location& loc)
{
	typedef overlay_map::const_iterator Itor;
	std::pair<Itor,Itor> itors = overlays_.equal_range(loc);
	while(itors.first != itors.second) {
		halo::remove(itors.first->second.halo_handle);
		++itors.first;
	}

	overlays_.erase(loc);
}

void game_display::write_overlays(config& cfg) const
{
	for(overlay_map::const_iterator i = overlays_.begin(); i != overlays_.end(); ++i) {
		config& item = cfg.add_child("item");
		i->first.write(item);
		item["image"] = i->second.image;
		item["halo"] = i->second.halo;
	}
}

const std::string game_display::current_team_name() const
{
	if (team_valid())
	{
		return teams_[currentTeam_].team_name();
	}
	return std::string();
}

void game_display::set_team(size_t teamindex, bool observe)
{
	wassert(teamindex < teams_.size());
	currentTeam_ = teamindex;
	if (!observe)
	{
		labels().set_team(&teams_[teamindex]);
		viewpoint_ = &teams_[teamindex];
	}
	else
	{
		labels().set_team(0);
		viewpoint_ = NULL;
	}
	labels().recalculate_labels();
}

void game_display::set_playing_team(size_t teamindex)
{
	wassert(teamindex < teams_.size());
	activeTeam_ = teamindex;
	invalidate_game_status();
}


// timestring() returns the current date as a string.
// Uses preferences::clock_format() for formatting.
static std::string timestring ()
{
    time_t now = time (NULL);
    struct tm *lt = localtime(&now);
    char buf[10];
    strftime(buf,sizeof(buf),preferences::clock_format().c_str(),lt);

    return buf;
}

void game_display::begin_game()
{
	in_game_ = true;
	create_buttons();
	invalidate_all();
}

namespace {
	const int chat_message_border = 5;
	const int chat_message_x = 10;
	const int chat_message_y = 10;
	const SDL_Color chat_message_colour = {255,255,255,255};
	const SDL_Color chat_message_bg     = {0,0,0,140};
}

void game_display::add_chat_message(const std::string& speaker, int side, const std::string& message, game_display::MESSAGE_TYPE type, bool bell)
{
	config* cignore;
	bool ignored = false;
	if ((cignore = preferences::get_prefs()->child("relationship"))){
		for(std::map<std::string,t_string>::const_iterator i = cignore->values.begin();
		i != cignore->values.end(); ++i){
			 if(speaker == i->first || speaker == "whisper: " + i->first){
				if (i->second == "ignored"){
					ignored = true;
				}
			}
		}
	}

	if (!ignored){
		bool action;
		std::string msg;

		if (bell) {
			sound::play_UI_sound(game_config::sounds::receive_message);
		}

		if(message.find("/me ") == 0) {
			msg.assign(message,4,message.size());
			action = true;
		} else {
			msg = message;
			action = false;
		}
		msg = font::word_wrap_text(msg,font::SIZE_SMALL,map_outside_area().w*3/4);

		int ypos = chat_message_x;
		for(std::vector<chat_message>::const_iterator m = chat_messages_.begin(); m != chat_messages_.end(); ++m) {
			ypos += font::get_floating_label_rect(m->handle).h;
		}
		SDL_Color speaker_colour = {255,255,255,255};
		if(side >= 1) {
			speaker_colour = int_to_color(team::get_side_color_range(side).mid());
		}

		SDL_Color message_colour = chat_message_colour;
		std::stringstream str;
		std::stringstream message_str;
		if(type == MESSAGE_PUBLIC) {
			if(action) {
				str << "<" << speaker << " " << msg << ">";
				message_colour = speaker_colour;
				message_str << " ";
			} else {
				str << "<" << speaker << ">";
				message_str << msg;
			}
		} else {
			if(action) {
				str << "*" << speaker << " " << msg << "*";
				message_colour = speaker_colour;
				message_str << " ";
			} else {
				str << "*" << speaker << "*";
				message_str << msg;
			}
		}

		// prepend message with timestamp
		std::stringstream message_complete;
		if (preferences::chat_timestamp()) {
			message_complete << timestring() << " ";
		}
		message_complete << str.str();

		const SDL_Rect rect = map_outside_area();
		const int speaker_handle = font::add_floating_label(message_complete.str(),font::SIZE_SMALL,speaker_colour,
			rect.x+chat_message_x,rect.y+ypos,
			0,0,-1,rect,font::LEFT_ALIGN,&chat_message_bg,chat_message_border);

		const int message_handle = font::add_floating_label(message_str.str(),font::SIZE_SMALL,message_colour,
			rect.x + chat_message_x + font::get_floating_label_rect(speaker_handle).w,rect.y+ypos,
			0,0,-1,rect,font::LEFT_ALIGN,&chat_message_bg,chat_message_border);

		chat_messages_.push_back(chat_message(speaker_handle,message_handle));

		prune_chat_messages();
	}
}

void game_display::prune_chat_messages(bool remove_all)
{
	const unsigned int message_ttl = remove_all ? 0 : 1200000;
	const unsigned int max_chat_messages = preferences::chat_lines();
	if(chat_messages_.empty() == false && (chat_messages_.front().created_at+message_ttl < SDL_GetTicks() || chat_messages_.size() > max_chat_messages)) {
		const int movement = font::get_floating_label_rect(chat_messages_.front().handle).h;

		font::remove_floating_label(chat_messages_.front().speaker_handle);
		font::remove_floating_label(chat_messages_.front().handle);
		chat_messages_.erase(chat_messages_.begin());

		for(std::vector<chat_message>::const_iterator i = chat_messages_.begin(); i != chat_messages_.end(); ++i) {
			font::move_floating_label(i->speaker_handle,0,-movement);
			font::move_floating_label(i->handle,0,-movement);
		}

		prune_chat_messages(remove_all);
	}
}

game_display *game_display::singleton_ = NULL;
