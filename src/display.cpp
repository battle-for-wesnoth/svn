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

#include "actions.hpp"
#include "display.hpp"
#include "events.hpp"
#include "font.hpp"
#include "game.hpp"
#include "game_config.hpp"
#include "halo.hpp"
#include "hotkeys.hpp"
#include "image.hpp"
#include "language.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "show_dialog.hpp"
#include "sound.hpp"
#include "team.hpp"
#include "tooltips.hpp"
#include "unit_display.hpp"
#include "util.hpp"

#include "SDL_image.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

std::map<gamemap::location,double> display::debugHighlights_;

namespace {
	const int MinZoom = 36;
	const int DefaultZoom = 72;
	const int MaxZoom = 200;

	const size_t SideBarGameStatus_x = 16;
	const size_t SideBarGameStatus_y = 220;

	const SDL_Rect empty_rect = {0,0,0,0};
}

display::display(unit_map& units, CVideo& video, const gamemap& map,
		const gamestatus& status, const std::vector<team>& t, const config& theme_cfg,
		const config& config, const config& level) :
	screen_(video), xpos_(0), ypos_(0),
	zoom_(DefaultZoom), map_(map), units_(units),
	minimap_(NULL), redrawMinimap_(false),
	pathsList_(NULL), status_(status),
	teams_(t), lastDraw_(0), drawSkips_(0),
	invalidateAll_(true), invalidateUnit_(true),
	invalidateGameStatus_(true), panelsDrawn_(false),
	currentTeam_(0), activeTeam_(0), hideEnergy_(false),
	deadAmount_(0.0), advancingAmount_(0.0), updatesLocked_(0),
	turbo_(false), grid_(false), sidebarScaling_(1.0),
	theme_(theme_cfg,screen_area()), builder_(config, level, map),
	first_turn_(true), in_game_(false), map_labels_(*this,map),
	tod_hex_mask1(NULL), tod_hex_mask2(NULL), diagnostic_label_(0),
	help_string_(0)
{
	if(non_interactive())
		updatesLocked_++;

	std::fill(reportRects_,reportRects_+reports::NUM_REPORTS,empty_rect);

	image::set_zoom(zoom_);

	gameStatusRect_.w = 0;
	unitDescriptionRect_.w = 0;
	unitProfileRect_.w = 0;

	//inits the flag list
	flags_.reserve(teams_.size());
	for(size_t i = 0; i != teams_.size(); ++i) {
		std::string flag;
		if(teams_[i].flag().empty()) {
			flag = game_config::flag_image;
			std::string::size_type pos;
			while((pos = flag.find("%d")) != std::string::npos) {
				std::ostringstream s;
				s << teams_[i].map_colour_to();
				flag.replace(pos, 2, s.str());
			}
		} else {
			flag = teams_[i].flag();
		}

		std::cerr << "Adding flag for team " << i << " from animation " << flag << "\n";
		flags_.push_back(animated<image::locator>(flag));
		flags_.back().start_animation(0, animated<image::locator>::INFINITE_CYCLES);
	}

	//clear the screen contents
	surface const disp(screen_.getSurface());
	SDL_Rect area = screen_area();
	SDL_FillRect(disp,&area,SDL_MapRGB(disp->format,0,0,0));
}

display::~display()
{
	// SDL_FreeSurface(minimap_);
	prune_chat_messages(true);
}

Uint32 display::rgb(Uint8 red, Uint8 green, Uint8 blue)
{
	return 0xFF000000 | (red << 16) | (green << 8) | blue;
}

void display::new_turn()
{
	const time_of_day& tod = status_.get_time_of_day();

	if(!turbo() && !first_turn_) {
		image::set_image_mask("");

		const time_of_day& old_tod = status_.get_previous_time_of_day();

		if(old_tod.image_mask != tod.image_mask) {
			const surface old_mask(image::get_image(old_tod.image_mask,image::UNMASKED));
			const surface new_mask(image::get_image(tod.image_mask,image::UNMASKED));

			const int niterations = 10;
			const int frame_time = 30;
			const int starting_ticks = SDL_GetTicks();
			for(int i = 0; i != niterations; ++i) {

				if(old_mask != NULL) {
					const double proportion = 1.0 - double(i)/double(niterations);
					tod_hex_mask1.assign(adjust_surface_alpha(old_mask,proportion));
				}

				if(new_mask != NULL) {
					const double proportion = double(i)/double(niterations);
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
}

void display::adjust_colours(int r, int g, int b)
{
	const time_of_day& tod = status_.get_time_of_day();
	image::set_colour_adjustment(tod.red+r,tod.green+g,tod.blue+b);
}

gamemap::location display::hide_unit(const gamemap::location& loc, bool hide_energy)
{
	const gamemap::location res = hiddenUnit_;
	hiddenUnit_ = loc;
	hideEnergy_ = hide_energy;
	return res;
}

int display::x() const { return screen_.getx(); }
int display::mapx() const { return x() - 140; }
int display::y() const { return screen_.gety(); }

const SDL_Rect& display::map_area() const
{
	return theme_.main_map_location(screen_area());
}

const SDL_Rect& display::minimap_area() const
{
	return theme_.mini_map_location(screen_area());
}

SDL_Rect display::screen_area() const
{
	const SDL_Rect res = {0,0,x(),y()};
	return res;
}

void display::select_hex(gamemap::location hex)
{
	if(team_valid() && teams_[currentTeam_].fogged(hex.x,hex.y)) {
		return;
	}

	invalidate(selectedHex_);
	selectedHex_ = hex;
	invalidate(selectedHex_);
	invalidate_unit();
}

void display::highlight_hex(gamemap::location hex)
{
	const int has_unit = units_.count(mouseoverHex_) + units_.count(hex);

	invalidate(mouseoverHex_);
	mouseoverHex_ = hex;
	invalidate(mouseoverHex_);
	invalidate_game_status();

	if(has_unit)
		invalidate_unit();
}

gamemap::location display::hex_clicked_on(int xclick, int yclick)
{
	const SDL_Rect& rect = map_area();
	if(point_in_rect(xclick,yclick,rect) == false) {
		return gamemap::location();
	}

	xclick -= rect.x;
	yclick -= rect.y;

	const int tile_width = hex_width();

	const int xtile = (xpos_ + xclick)/tile_width;
	const int ytile = (ypos_ + yclick - (is_odd(xtile) ? zoom_/2 : 0))/zoom_; //(yclick + is_odd(xtile) ? -yclick/2 : 0)/zoom_;

	return gamemap::location(xtile,ytile);
}

int display::get_location_x(const gamemap::location& loc) const
{
	return map_area().x + loc.x*hex_width() - xpos_;
}

int display::get_location_y(const gamemap::location& loc) const
{
	return map_area().y + loc.y*zoom_ - ypos_ + (is_odd(loc.x) ? zoom_/2 : 0);
}

void display::get_visible_hex_bounds(gamemap::location &topleft, gamemap::location &bottomright) const
{
	const SDL_Rect& rect = map_area();
	const int tile_width = hex_width();

	topleft.x  = xpos_ / tile_width;
	topleft.y  = (ypos_ - (is_odd(topleft.x) ? zoom_/2 : 0)) / zoom_; 

	bottomright.x  = (xpos_ + rect.w) / tile_width;
	bottomright.y  = ((ypos_ + rect.h) - (is_odd(bottomright.x) ? zoom_/2 : 0)) / zoom_; 

	if(topleft.x > -1)
		topleft.x--;
	if(topleft.y > -1)
		topleft.y--;
	if(bottomright.x < map_.x())
		bottomright.x++;
	if(bottomright.y < map_.y())
		bottomright.y++;
}

gamemap::location display::minimap_location_on(int x, int y)
{
	const SDL_Rect rect = minimap_area();

	if(x < rect.x || y < rect.y ||
	   x >= rect.x + rect.w || y >= rect.y + rect.h) {
		return gamemap::location();
	}

	const double xdiv = double(rect.w) / double(map_.x());
	const double ydiv = double(rect.h) / double(map_.y());

	return gamemap::location(int((x - rect.x)/xdiv),int((y-rect.y)/ydiv));
}

void display::scroll(int xmove, int ymove)
{
	const int orig_x = xpos_;
	const int orig_y = ypos_;
	xpos_ += xmove;
	ypos_ += ymove;
	bounds_check_position();

	//only invalidate if we've actually moved
	if(orig_x != xpos_ || orig_y != ypos_) {
		map_labels_.scroll(orig_x - xpos_, orig_y - ypos_);
		font::scroll_floating_labels(orig_x - xpos_, orig_y - ypos_);
		invalidate_all();
	}
}

int display::hex_size() const
{
	return zoom_;
}

int display::hex_width() const
{
	return (zoom_*3)/4;
}

double display::zoom(int amount)
{
	if(amount == 0 || !team_valid()) {
		return double(zoom_)/double(DefaultZoom);
	}

	const int orig_xpos = xpos_;
	const int orig_ypos = ypos_;

	xpos_ /= zoom_;
	ypos_ /= zoom_;

	const int orig_zoom = zoom_;

	zoom_ += amount;
	if(zoom_ < MinZoom || zoom_ > MaxZoom) {
		zoom_ = orig_zoom;
		xpos_ = orig_xpos;
		ypos_ = orig_ypos;
		return double(zoom_)/double(DefaultZoom);
	}

	xpos_ *= zoom_;
	ypos_ *= zoom_;

	xpos_ += amount*2;
	ypos_ += amount*2;

	const int prev_zoom = zoom_;

	bounds_check_position();

	if(zoom_ != prev_zoom) {
		xpos_ = orig_xpos;
		ypos_ = orig_ypos;
		zoom_ = orig_zoom;
		image::set_zoom(zoom_);
		return double(zoom_)/double(DefaultZoom);
	}

	energy_bar_rects_.clear();

	image::set_zoom(zoom_);
	map_labels_.recalculate_labels();
	invalidate_all();

	return double(zoom_)/double(DefaultZoom);
}

void display::default_zoom()
{
	zoom(DefaultZoom - zoom_);
}

void display::scroll_to_tile(int x, int y, SCROLL_TYPE scroll_type, bool check_fogged)
{
	if(update_locked() || (check_fogged && fogged(x,y)))
		return;

	const gamemap::location loc(x,y);

	if(map_.on_board(loc) == false)
		return;

	const int xpos = get_location_x(loc);
	const int ypos = get_location_y(loc);

	const int speed = preferences::scroll_speed()*2;

	const SDL_Rect& area = map_area();
	const int desiredxpos = area.w/2 - zoom_/2;
	const int desiredypos = area.h/2 - zoom_/2;

	const int xmove = xpos - desiredxpos;
	const int ymove = ypos - desiredypos;

	int num_moves = (abs(xmove) > abs(ymove) ? abs(xmove):abs(ymove))/speed;

	if(scroll_type == WARP || turbo())
		num_moves = 1;

	for(int i = 0; i != num_moves; ++i) {
		events::pump();

		//accelerate scroll rate if either shift key is held down
		if((i%4) != 0 && i != num_moves-1 && turbo()) {
			continue;
		}

		scroll(xmove/num_moves,ymove/num_moves);
		draw();
	}

	invalidate_all();
	draw();
}

void display::scroll_to_tiles(int x1, int y1, int x2, int y2,
                              SCROLL_TYPE scroll_type, bool check_fogged)
{
	const gamemap::location loc1(x1,y1), loc2(x2,y2);
	const int xpos1 = get_location_x(loc1);
	const int ypos1 = get_location_y(loc1);
	const int xpos2 = get_location_x(loc2);;
	const int ypos2 = get_location_y(loc2);;

	const int diffx = abs(xpos1 - xpos2);
	const int diffy = abs(ypos1 - ypos2);

	if(diffx > map_area().w/hex_width() || diffy > map_area().h/zoom_) {
		scroll_to_tile(x1,y1,scroll_type,check_fogged);
	} else {
		scroll_to_tile((x1+x2)/2,(y1+y2)/2,scroll_type,check_fogged);
	}
}

void display::bounds_check_position()
{
	const int min_zoom1 = map_area().w/((map_.x()*3)/4);
	const int min_zoom2 = map_area().h/map_.y();
	const int min_zoom = maximum<int>(min_zoom1,min_zoom2);

	const int orig_zoom = zoom_;

	if(zoom_ < min_zoom) {
		zoom_ = min_zoom;
	}

	if(zoom_ > MaxZoom) {
		zoom_ = MaxZoom;
	}

	const int tile_width = hex_width();

	const int xend = tile_width*map_.x() + tile_width/3;
	const int yend = zoom_*map_.y() + zoom_/2;

	if(xpos_ + map_area().w > xend)
		xpos_ -= xpos_ + map_area().w - xend;

	if(ypos_ + map_area().h > yend)
		ypos_ -= ypos_ + map_area().h - yend;

	if(xpos_ < 0)
		xpos_ = 0;

	if(ypos_ < 0)
		ypos_ = 0;

	if(zoom_ != orig_zoom)
		image::set_zoom(zoom_);
}

void display::redraw_everything()
{
	if(update_locked() || teams_.empty())
		return;

	bounds_check_position();

	for(size_t n = 0; n != reports::NUM_REPORTS; ++n) {
		reportRects_[n] = empty_rect;
		reportSurfaces_[n].assign(NULL);
		reports_[n] = reports::report();
	}

	tooltips::clear_tooltips();

	theme_.set_resolution(screen_area());

	if(buttons_.empty() == false) {
		create_buttons();
	}

	panelsDrawn_ = false;

	map_labels_.recalculate_labels();

	invalidate_all();
	draw(true,true);
}

namespace {

void draw_panel(display& disp, const theme::panel& panel, std::vector<gui::button>& buttons)
{
	log_scope("draw panel");
	surface surf(image::get_image(panel.image(),image::UNSCALED));

	const SDL_Rect screen = disp.screen_area();
	SDL_Rect& loc = panel.location(screen);
	if(surf->w != loc.w || surf->h != loc.h) {
		surf.assign(scale_surface(surf,loc.w,loc.h));
	}

	disp.blit_surface(loc.x,loc.y,surf);
	update_rect(loc);

	for(std::vector<gui::button>::iterator b = buttons.begin(); b != buttons.end(); ++b) {
		if(rects_overlap(b->location(),loc)) {
			b->set_dirty(true);
		}
	}
}

void draw_label(display& disp, surface target, const theme::label& label)
{
	log_scope("draw label");

	const std::string& text = label.text();
	const std::string& icon = label.icon();
	SDL_Rect& loc = label.location(disp.screen_area());
	
	if(icon.empty() == false) {
		surface surf(image::get_image(icon,image::UNSCALED));
		if(surf->w != loc.w || surf->h != loc.h) {
			surf.assign(scale_surface(surf,loc.w,loc.h));
		}

		SDL_BlitSurface(surf,NULL,target,&loc);

		if(text.empty() == false) {
			tooltips::add_tooltip(loc,text);
		}
	} else if(text.empty() == false) {
		font::draw_text(&disp,loc,label.font_size(),font::NORMAL_COLOUR,text,loc.x,loc.y);
	}


	update_rect(loc);
}

}

void display::draw(bool update,bool force)
{	
	bool changed = false;

	//log_scope("Drawing");
	invalidate_animations();

	if(!panelsDrawn_) {
		surface const screen(screen_.getSurface());

		const std::vector<theme::panel>& panels = theme_.panels();
		for(std::vector<theme::panel>::const_iterator p = panels.begin(); p != panels.end(); ++p) {
			draw_panel(*this,*p,buttons_);
		}

		const std::vector<theme::label>& labels = theme_.labels();
		for(std::vector<theme::label>::const_iterator i = labels.begin(); i != labels.end(); ++i) {
			draw_label(*this,screen,*i);
		}
		
		//invalidate the reports so they are redrawn
		std::fill(reports_,reports_+sizeof(reports_)/sizeof(*reports_),reports::report());
		invalidateGameStatus_ = true;
		panelsDrawn_ = true;

		changed = true;
	}

	if(invalidateAll_ && !map_.empty()) {
		gamemap::location topleft;
		gamemap::location bottomright;
		get_visible_hex_bounds(topleft, bottomright);
		for(int x = topleft.x; x <= bottomright.x; ++x)
			for(int y = topleft.y; y <= bottomright.y; ++y) 
				draw_tile(x,y);
		invalidateAll_ = false;

		redrawMinimap_ = true;
		changed = true;
	} else if(!map_.empty()) {
		if(!invalidated_.empty())
			changed = true;

		for(std::set<gamemap::location>::const_iterator it =
		    invalidated_.begin(); it != invalidated_.end(); ++it) {
			draw_tile(it->x,it->y);
		}

		invalidated_.clear();
	}

	if(redrawMinimap_) {
		redrawMinimap_ = false;
		const SDL_Rect area = minimap_area();
		draw_minimap(area.x,area.y,area.w,area.h);
		changed = true;
	}


	if(!map_.empty()) {
		draw_sidebar();
		changed = true;
	}

	prune_chat_messages();

	const int max_skips = 5;
	const int time_between_draws = 20;
	const int current_time = SDL_GetTicks();
	const int wait_time = lastDraw_ + time_between_draws - current_time;

	//force a wait for 10 ms every frame.
	//TODO: review whether this is the correct thing to do
	SDL_Delay(maximum<int>(10,wait_time));

	if(update) {
		lastDraw_ = SDL_GetTicks();

		if(wait_time >= 0 || drawSkips_ >= max_skips || force) {
			if(changed)
				update_display();
		} else {
			drawSkips_++;
		}
	}
}

void display::update_display()
{
	if(updatesLocked_ > 0)
		return;

	screen_.flip();
}

void display::draw_sidebar()
{
	if(teams_.empty())
		return;

	if(invalidateUnit_) {
		//we display the unit the mouse is over if it is over a unit
		//otherwise we display the unit that is selected
		std::map<gamemap::location,unit>::const_iterator i = 
			find_visible_unit(units_,mouseoverHex_,
					map_,
					status_.get_time_of_day().lawful_bonus,
					teams_,teams_[viewing_team()]);

		if(i == units_.end() || fogged(i->first.x,i->first.y)) {
			i = find_visible_unit(units_,selectedHex_,
					map_,
					status_.get_time_of_day().lawful_bonus,
					teams_,teams_[viewing_team()]);
		}

		if(i != units_.end() && !fogged(i->first.x,i->first.y))
			for(size_t r = reports::UNIT_REPORTS_BEGIN; r != reports::UNIT_REPORTS_END; ++r)
				draw_report(reports::TYPE(r));

		invalidateUnit_ = false;
	}

	if(invalidateGameStatus_) {
		draw_game_status(mapx()+SideBarGameStatus_x,SideBarGameStatus_y);
		invalidateGameStatus_ = false;
	}
}

void display::draw_game_status(int x, int y)
{
	if(teams_.empty())
		return;

	for(size_t r = reports::STATUS_REPORTS_BEGIN; r != reports::STATUS_REPORTS_END; ++r) {
		draw_report(reports::TYPE(r));
	}	
}

void display::draw_image_for_report(surface& img, surface& surf, SDL_Rect& rect)
{
	SDL_Rect visible_area = get_non_transperant_portion(img);
	SDL_Rect target = rect;
	if(visible_area.x != 0 || visible_area.y != 0 || visible_area.w != img->w || visible_area.h != img->h) {
		if(visible_area.w == 0 || visible_area.h == 0) {
			return;
		}

		if(visible_area.w > rect.w || visible_area.h > rect.h) {
			img.assign(get_surface_portion(img,visible_area));
			img.assign(scale_surface(img,rect.w,rect.h));
			visible_area.x = 0;
			visible_area.y = 0;
			visible_area.w = img->w;
			visible_area.h = img->h;
		} else {
			target.x = rect.x + (rect.w - visible_area.w)/2;
			target.y = rect.y + (rect.h - visible_area.h)/2;
			target.w = visible_area.w;
			target.h = visible_area.h;
		}

		SDL_BlitSurface(img,&visible_area,screen_.getSurface(),&target);
	} else {
		if(img->w != rect.w || img->h != rect.h) {
			img.assign(scale_surface(img,rect.w,rect.h));
		}

		SDL_BlitSurface(img,NULL,screen_.getSurface(),&target);
	}
}

void display::draw_report(reports::TYPE report_num)
{
	if(!team_valid())
		return;

	const theme::status_item* const item = theme_.get_status_item(reports::report_name(report_num));
	if(item != NULL) {

		reports::report report = reports::generate_report(report_num,map_,
				units_, teams_,
		      teams_[viewing_team()],
				currentTeam_+1,activeTeam_+1,
				selectedHex_,mouseoverHex_,status_,observers_);

		SDL_Rect& rect = reportRects_[report_num];
		const SDL_Rect& new_rect = item->location(screen_area());

		//report and its location is unchanged since last time. Do nothing.
		if(rect == new_rect && reports_[report_num] == report) {
			return;
		}

		reports_[report_num] = report;

		surface& surf = reportSurfaces_[report_num];

		if(surf != NULL) {
			SDL_BlitSurface(surf,NULL,screen_.getSurface(),&rect);
			update_rect(rect);
		}

		//if the rectangle has just changed, assign the surface to it
		if(new_rect != rect || surf == NULL) {
			surf.assign(NULL);
			rect = new_rect;

			//if the rectangle is present, and we are blitting text, then
			//we need to backup the surface. (Images generally won't need backing
			//up unless they are transperant, but that is done later)
			if(rect.w > 0 && rect.h > 0) {
				surf.assign(get_surface_portion(screen_.getSurface(),rect));
				if(reportSurfaces_[report_num] == NULL) {
					std::cerr << "Could not backup background for report!\n";
				}
			}

			update_rect(rect);
		}

		tooltips::clear_tooltips(rect);

		SDL_Rect area = rect;

		int x = rect.x, y = rect.y;
		if(!report.empty()) {
			// Add prefix, postfix elements. Make sure that they get the same tooltip as the guys
			// around them.
			std::string str = item->prefix();
			if(str.empty() == false) {
				report.insert(report.begin(), reports::element(str,"",report.begin()->tooltip));
				}
			str = item->postfix();
			if(str.empty() == false) {
				report.push_back(reports::element(str,"",report.end()->tooltip));
			}

			// Loop through and display each report element
			size_t tallest = 0;
			for(reports::report::iterator i = report.begin(); i != report.end(); ++i) {
				if(i->text.empty() == false) {
					// Draw a text element
					area = font::draw_text(this,rect,item->font_size(),font::NORMAL_COLOUR,i->text,x,y);
					if(area.h > tallest) tallest = area.h;
					if(i->text[i->text.size() - 1] == '\n') {
						x = rect.x;
						y += tallest;
						tallest = 0;
					} else {
						x += area.w;
					}
				} else if(i->image.empty() == false) {
					// Draw an image element
					surface img(image::get_image(i->image,image::UNSCALED));

					if(img == NULL) {
						std::cerr << "could not find image for report: '" << i->image << "'\n";
						continue;
					}

					area.x = x;
					area.y = y;
					area.w = minimum<int>(rect.w + rect.x - x, img->w);
					area.h = minimum<int>(rect.h + rect.y - y, img->h);
					draw_image_for_report(img,surf,area);

					if(area.h > tallest) tallest = area.h;
					x += area.w;
				} else {
					// No text or image, skip this element
					continue;
				}
				if(i->tooltip.empty() == false) {
					tooltips::add_tooltip(area,i->tooltip);
				}
			}
		}
	} else {
		reportSurfaces_[report_num].assign(NULL);
	}
}

void display::draw_unit_details(int x, int y, const gamemap::location& loc,
         const unit& u, SDL_Rect& description_rect, int profilex, int profiley,
         SDL_Rect* clip_rect)
{
	if(teams_.empty())
		return;

	tooltips::clear_tooltips(description_rect);

	SDL_Rect clipRect = clip_rect != NULL ? *clip_rect : screen_area();

	const surface background(image::get_image(game_config::rightside_image,image::UNSCALED));
	const surface background_bot(image::get_image(game_config::rightside_image_bot,image::UNSCALED));

	if(background == NULL || background_bot == NULL)
		return;

	surface const screen(screen_.getSurface());

	if(description_rect.w > 0 && description_rect.x >= mapx()) {
		SDL_Rect srcrect = description_rect;
		srcrect.y -= background->h;
		srcrect.x -= mapx();

		SDL_BlitSurface(background_bot,&srcrect,screen,&description_rect);
		update_rect(description_rect);
	}

	std::string status = _("healthy");
	if(map_.on_board(loc) &&
	   u.invisible(map_.underlying_terrain(map_[loc.x][loc.y]), 
			status_.get_time_of_day().lawful_bonus,loc,
			units_,teams_)) {
		status = font::GOOD_TEXT + _("invisible");
	}

	if(u.has_flag("slowed")) {
		status = font::BAD_TEXT + _("slowed");
	}

	if(u.has_flag("poisoned")) {
		status = font::BAD_TEXT + _("poisoned");
	}

	std::stringstream details;
	details << font::LARGE_TEXT << u.description() << "\n"
	        << font::LARGE_TEXT << u.type().language_name()
			<< "\n" << font::SMALL_TEXT << "(" << _("level") << " "
			<< u.type().level() << ")\n"
			<< status << "\n"
			<< string_table[unit_type::alignment_description(u.type().alignment())]
			<< "\n"
			<< u.traits_description() << "\n";

	const std::vector<std::string>& abilities = u.type().abilities();
	for(std::vector<std::string>::const_iterator a = abilities.begin(); a != abilities.end(); ++a) {
		details << *a << "\n";
	}

	//display in green/white/red depending on hitpoints
	if(u.hitpoints() <= u.max_hitpoints()/3)
		details << font::BAD_TEXT;
	else if(u.hitpoints() > 2*(u.max_hitpoints()/3))
		details << font::GOOD_TEXT;

	details << _("HP") << ": " << u.hitpoints()
			<< "/" << u.max_hitpoints() << "\n";
	
	if(u.can_advance() == false) {
		details << _("XP") << ": " << u.experience() << "/-";
	} else {
		//if killing a unit the same level as us would level us up,
		//then display in green
		if(u.max_experience() - u.experience() < game_config::kill_experience) {
			details << font::GOOD_TEXT;
		}

		details << _("XP") << ": " << u.experience() << "/" << u.max_experience();
	}
	
	details << "\n"
			<< _("Moves") << ": " << u.movement_left() << "/"
			<< u.total_movement()
			<< "\n";

	const std::vector<attack_type>& attacks = u.attacks();
	for(std::vector<attack_type>::const_iterator at_it = attacks.begin();
	    at_it != attacks.end(); ++at_it) {

		const std::string& lang_weapon = at_it->name();
		const std::string& lang_type = at_it->type();
		const std::string& lang_special = at_it->special();
		details << "\n"
				<< (lang_weapon.empty() ? at_it->name():lang_weapon) << " ("
				<< (lang_type.empty() ? at_it->type():lang_type) << ")\n"
				<< (lang_special.empty() ? at_it->special():lang_special)<<"\n"
				<< at_it->damage() << "-" << at_it->num_attacks() << " -- "
		        << (at_it->range() == attack_type::SHORT_RANGE ?
		            _("melee") :
					_("ranged"));
	
		if(at_it->hexes() > 1) {
			details << " (" << at_it->hexes() << ")";
		}
					
		details << "\n\n";
	}

	//choose the font size based on how much room we have to play
	//with on the right-side panel
	const size_t font_size = this->y() >= 700 ? 13 : 10;

	description_rect = font::draw_text(this,clipRect,font_size,font::NORMAL_COLOUR,
	                                   details.str(),x,y);

	update_rect(description_rect);

	y += description_rect.h;

	const surface profile(image::get_image(u.type().image(),image::UNSCALED));

	if(profile == NULL)
		return;

	//blit the unit profile
	{
		const size_t profilew = 50;
		const size_t profileh = 50;
		SDL_Rect srcrect = { (profile->w-profilew)/2,(profile->h-profileh)/2,
		                     profilew,profileh };
		SDL_Rect dstrect = srcrect;
		dstrect.x = profilex;
		dstrect.y = profiley;
		SDL_BlitSurface(profile,&srcrect,video().getSurface(),&dstrect);

		update_rect(profilex,profiley,profilew,profileh);
	}
}

void display::draw_minimap(int x, int y, int w, int h)
{
	const surface surf(get_minimap(w,h));
	if(surf == NULL)
		return;

	SDL_Rect minimap_location = {x,y,w,h};

	clip_rect_setter clip_setter(video().getSurface(),minimap_location);

	SDL_Rect loc = minimap_location;
	SDL_BlitSurface(surf,NULL,video().getSurface(),&loc);

	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u) {
		if(fogged(u->first.x,u->first.y) || 
				(teams_[currentTeam_].is_enemy(u->second.side()) &&
				u->second.invisible(map_.underlying_terrain(map_[u->first.x][u->first.y]), 
				status_.get_time_of_day().lawful_bonus,u->first,
				units_,teams_)))
			continue;

		const int side = u->second.side();
		const SDL_Color& col = team::get_side_colour(side);
		const Uint32 mapped_col = SDL_MapRGB(video().getSurface()->format,col.r,col.g,col.b);
		SDL_Rect rect = {x + (u->first.x*w)/map_.x(),
		                 y + (u->first.y*h)/map_.y(),
						 w/map_.x(), h/map_.y() };
		SDL_FillRect(video().getSurface(),&rect,mapped_col);
	}

	const double xscaling = double(surf->w)/double(map_.x());
	const double yscaling = double(surf->h)/double(map_.y());

	const int xbox = static_cast<int>(xscaling*xpos_/(zoom_*0.75));
	const int ybox = static_cast<int>(yscaling*ypos_/zoom_);

	const int wbox = static_cast<int>(xscaling*map_area().w/(zoom_*0.75) - xscaling) + 3;
	const int hbox = static_cast<int>(yscaling*map_area().h/zoom_ - yscaling) + 3;

	const Uint32 boxcolour = SDL_MapRGB(surf->format,0xFF,0xFF,0xFF);
	const surface screen(screen_.getSurface());

	gui::draw_rectangle(x+xbox,y+ybox,wbox,hbox,boxcolour,screen);

	update_rect(minimap_location);
}

gamemap::TERRAIN display::get_terrain_on(int palx, int paly, int x, int y)
{
	const int height = 37;
	if(y < paly || x < palx)
		return 0;

	const std::vector<gamemap::TERRAIN>& terrains=map_.get_terrain_precedence();
	if(static_cast<size_t>(y) > paly+terrains.size()*height)
		return 0;

	const size_t index = (y - paly)/height;
	if(index >= terrains.size())
		return 0;

	return terrains[index];
}

void display::draw_halo_on_tile(int x, int y)
{
	const gamemap::location loc(x,y);
	int xpos = get_location_x(loc);
	int ypos = get_location_y(loc);

	const halo_map::iterator halo_it = haloes_.find(loc);

	//see if there is a unit on this tile
	const unit_map::const_iterator it = fogged(x,y) ? units_.end() : units_.find(gamemap::location(x,y));

	if(halo_it != haloes_.end() && it == units_.end()) {
		halo::remove(halo_it->second);
		haloes_.erase(halo_it);
	} else if(halo_it == haloes_.end() && it != units_.end()) {
		const std::string& halo = it->second.type().image_halo();
		if(halo.empty() == false) {
			haloes_.insert(std::pair<gamemap::location,int>(loc,halo::add(xpos+hex_width()/2,ypos+hex_size()/2,halo)));
		}
	}
}

void display::draw_unit_on_tile(int x, int y, surface unit_image_override,
		double highlight_ratio, Uint32 blend_with)
{
	if(updatesLocked_)
		return;

	const gamemap::location loc(x,y);
	int xpos = get_location_x(loc);
	int ypos = get_location_y(loc);

	//see if there is a unit on this tile
	const unit_map::const_iterator it = units_.find(loc);

	if(it == units_.end()) {
		return;
	}
	
	SDL_Rect clip_rect = map_area();

	if(xpos > clip_rect.x + clip_rect.w || ypos > clip_rect.y + clip_rect.h ||
	   xpos + zoom_ < clip_rect.x || ypos + zoom_ < clip_rect.y) {
		return;
	}

	surface const dst(screen_.getSurface());

	clip_rect_setter set_clip_rect(dst,clip_rect);

	double unit_energy = 0.0;

	SDL_Color energy_colour = {0,0,0,0};

	surface unit_image(unit_image_override);

	const std::string* energy_file = NULL;

	const unit& u = it->second;

	if(loc != hiddenUnit_ || !hideEnergy_) {
		if(unit_image == NULL) {
			unit_image.assign(image::get_image(it->second.image(),it->second.stone() ? image::GREYED : image::SCALED));
		}

		if(unit_image == NULL) {
			return;
		}

		const int unit_move = it->second.movement_left();
		const int unit_total_move = it->second.total_movement();

		if(size_t(u.side()) != currentTeam_+1) {
			if(team_valid() &&
			   teams_[currentTeam_].is_enemy(it->second.side())) {
				energy_file = &game_config::enemy_energy_image;
			} else {
				energy_file = &game_config::ally_energy_image;
			}
		} else {
			if(activeTeam_ == currentTeam_ && unit_move == unit_total_move && !it->second.user_end_turn()) {
				energy_file = &game_config::unmoved_energy_image;
			} else if(activeTeam_ == currentTeam_ && unit_can_move(loc,units_,map_,teams_) && !it->second.user_end_turn()) {
				energy_file = &game_config::partmoved_energy_image;
			} else {
				energy_file = &game_config::moved_energy_image;
			}
		}

		assert(energy_file != NULL);
		if(energy_file == NULL) {
			std::cerr << "energy file is NULL\n";
			return;
		}

		if(highlight_ratio == 1.0)
			highlight_ratio = it->second.alpha();

		if(u.invisible(map_.underlying_terrain(map_[x][y]), 
					status_.get_time_of_day().lawful_bonus,loc,
					units_,teams_) &&
		   highlight_ratio > 0.5) {
			highlight_ratio = 0.5;
		}

		if(loc == selectedHex_ && highlight_ratio == 1.0) {
			highlight_ratio = 1.5;
			// blend_with = rgb(255,255,255);
		}

		if(u.max_hitpoints() > 0) {
			unit_energy = double(u.hitpoints())/double(u.max_hitpoints());
		}

		if(unit_energy < 0.33) {
			energy_colour.r = 200;
			energy_colour.g = 0;
			energy_colour.b = 0;
		} else if(unit_energy < 0.66) {
			energy_colour.r = 200;
			energy_colour.g = 200;
			energy_colour.b = 0;
		} else {
			energy_colour.r = 0;
			energy_colour.g = 200;
			energy_colour.b = 0;
		}

		if(u.facing_left() == false) {
			//reverse the image here. image::reverse_image is more efficient, however
			//it can be used only if we are sure that unit_image came from image::get_image.
			//Since we aren't sure of that in the case of overrides, use the less efficient
			//flip_surface if the image has been over-ridden.
			if(unit_image_override == NULL) {
				unit_image.assign(image::reverse_image(unit_image));
			} else {
				unit_image.assign(flip_surface(unit_image));
			}
		}
	}

	if(deadUnit_ == gamemap::location(x,y)) {
		highlight_ratio = deadAmount_;
	}

	if(unit_image == NULL || fogged(x,y) ||
			(teams_[currentTeam_].is_enemy(it->second.side()) && 
			it->second.invisible(map_.underlying_terrain(map_[x][y]), 
					status_.get_time_of_day().lawful_bonus,loc,
					units_,teams_))) {
		return;
	}

	const gamemap::TERRAIN terrain = map_.get_terrain(loc);
	const int height_adjust = it->second.is_flying() ? 0 : int(map_.get_terrain_info(terrain).unit_height_adjust()*zoom());
	const double submerge = it->second.is_flying() ? 0.0 : map_.get_terrain_info(terrain).unit_submerge();

	double blend_ratio = 0.0;

	if(loc == advancingUnit_ && it != units_.end()) {
		//the unit is advancing - set the advancing colour to white if it's a
		//non-chaotic unit, otherwise black
		blend_with = it->second.type().alignment() == unit_type::CHAOTIC ?
		                                        rgb(16,16,16) : rgb(255,255,255);
		blend_ratio = 1 - advancingAmount_;
	} else if(it->second.poisoned() /* && highlight_ratio == 1.0 */) {
		//the unit is poisoned - draw with a green hue
		blend_with = rgb(0,255,0);
		blend_ratio = 0.25;
		//highlight_ratio *= 0.75;
	}

	if(loc != hiddenUnit_) {
		surface ellipse_front(NULL);
		surface ellipse_back(NULL);

		if(preferences::show_side_colours()) {
			const char* const selected = selectedHex_ == loc ? "selected-" : "";
			char buf[50];
			sprintf(buf,"misc/%sellipse-%d-top.png",selected,team::get_side_colour_index(it->second.side()));
			ellipse_back.assign(image::get_image(buf));
			sprintf(buf,"misc/%sellipse-%d-bottom.png",selected,team::get_side_colour_index(it->second.side()));
			ellipse_front.assign(image::get_image(buf));
		}

		draw_unit(xpos,ypos - height_adjust,unit_image,false,
		          highlight_ratio,blend_with,blend_ratio,submerge,ellipse_back,ellipse_front);
	}

	const double bar_alpha = highlight_ratio < 1.0 && blend_with == 0 ? highlight_ratio : 1.0;
	if(energy_file != NULL) {
		draw_bar(*energy_file,xpos,ypos,(u.max_hitpoints()*2)/3,unit_energy,energy_colour,bar_alpha);
	}

	if(u.experience() > 0 && u.can_advance()) {
		const double filled = double(u.experience())/double(u.max_experience());
		const int level = maximum<int>(u.type().level(),1);
		const SDL_Color normal_colour = {02,153,255,0}, near_advance_colour = {255,255,255,0};
		const bool near_advance = u.max_experience() - u.experience() <= game_config::kill_experience*level;
		const SDL_Color colour = near_advance ? near_advance_colour : normal_colour;

		draw_bar("misc/bar-energy-enemy.png",xpos+5,ypos,u.max_experience()/(level*2),filled,colour,bar_alpha);
	}

	const std::vector<std::string>& overlays = it->second.overlays();
	for(std::vector<std::string>::const_iterator ov = overlays.begin(); ov != overlays.end(); ++ov) {
		const surface img(image::get_image(*ov));
		if(img != NULL) {
			draw_unit(xpos,ypos,img);
		}
	}
}

void display::draw_bar(const std::string& image, int xpos, int ypos, size_t height, double filled, const SDL_Color& col, double alpha)
{
	filled = minimum<double>(maximum<double>(filled,0.0),1.0);

	surface surf(image::get_image(image,image::SCALED,image::NO_ADJUST_COLOUR));
	surface unmoved_surf(image::get_image("misc/bar-energy-unmoved.png",image::SCALED,image::NO_ADJUST_COLOUR));
	if(surf == NULL || unmoved_surf == NULL) {
		return;
	}

	const SDL_Rect& bar_loc = calculate_energy_bar(unmoved_surf);
	if(height > bar_loc.h) {
		height = bar_loc.h;
	}

	if(alpha != 1.0) {
		surf.assign(adjust_surface_alpha(surf,alpha));
		if(surf == NULL) {
			return;
		}
	}

	const size_t skip_rows = bar_loc.h - height;

	SDL_Rect top = {0,0,surf->w,bar_loc.y};
	SDL_Rect bot = {0,bar_loc.y+skip_rows,surf->w,0};
	bot.h = surf->w - bot.y;

	blit_surface(xpos,ypos,surf,&top);
	blit_surface(xpos,ypos+top.h,surf,&bot);

	const size_t unfilled = (const size_t)(height*(1.0 - filled));

	if(unfilled < height && alpha >= 0.3) {
		SDL_Rect filled_area = {xpos+bar_loc.x,ypos+bar_loc.y+unfilled,bar_loc.w,height-unfilled};
		const Uint32 colour = SDL_MapRGB(video().getSurface()->format,col.r,col.g,col.b);
		SDL_FillRect(video().getSurface(),&filled_area,colour);
	}
}

void display::draw_terrain_on_tile(int x, int y, image::TYPE image_type, ADJACENT_TERRAIN_TYPE type)
{

	const gamemap::location loc(x,y);
	int xpos = int(get_location_x(loc));
	int ypos = int(get_location_y(loc));

	SDL_Rect clip_rect = map_area();

	if(xpos > clip_rect.x + clip_rect.w || ypos > clip_rect.y + clip_rect.h ||
	   xpos + zoom_ < clip_rect.x || ypos + zoom_ < clip_rect.y) {
		return;
	}

	surface const dst(screen_.getSurface());

	clip_rect_setter set_clip_rect(dst,clip_rect);

	const std::vector<surface>& images = get_terrain_images(x,y,image_type,type);

	std::vector<surface>::const_iterator itor;
	for(itor = images.begin(); itor != images.end(); ++itor) {
		SDL_Rect dstrect = { xpos, ypos, 0, 0 };
		SDL_BlitSurface(*itor,NULL,dst,&dstrect);
	}
}

void display::draw_tile(int x, int y, surface unit_image, double alpha, Uint32 blend_to)
{
	if(updatesLocked_)
		return;

	draw_halo_on_tile(x,y);
	
	const gamemap::location loc(x,y);
	int xpos = int(get_location_x(loc));
	int ypos = int(get_location_y(loc));

	SDL_Rect clip_rect = map_area();

	if(xpos >= clip_rect.x + clip_rect.w || ypos >= clip_rect.y + clip_rect.h ||
	   xpos + zoom_ < clip_rect.x || ypos + zoom_ < clip_rect.y) {
		return;
	}

	surface const dst(screen_.getSurface());

	clip_rect_setter set_clip_rect(dst,clip_rect);

	const bool is_shrouded = shrouded(x,y);
	gamemap::TERRAIN terrain = gamemap::VOID_TERRAIN;

	if(!is_shrouded) {
		terrain = map_.get_terrain(loc);
	}

	image::TYPE image_type = image::SCALED;

	const time_of_day& tod = status_.get_time_of_day();
	const time_of_day& tod_at = timeofday_at(status_,units_,loc);
	std::string mask = tod_at.image_mask;
	if(tod_hex_mask1 != NULL || tod_hex_mask2 != NULL || tod.image_mask != tod_at.image_mask) {
		image_type = image::UNMASKED;
		mask = tod_at.image_mask;
	}

	//find if this tile should be greyed
	if(pathsList_ != NULL && pathsList_->routes.find(gamemap::location(x,y)) ==
					         pathsList_->routes.end()) {
		image_type = image::GREYED;
	}

	unit_map::iterator un = find_visible_unit(units_, loc, map_,
		status_.get_time_of_day().lawful_bonus,teams_,teams_[currentTeam_]);

	if(loc == mouseoverHex_ && map_.on_board(mouseoverHex_) ||
	   loc == selectedHex_ && (un != units_.end())) {
		image_type = image::BRIGHTENED;
	}
	else if (highlighted_locations_.find(loc) != highlighted_locations_.end()) {
		image_type = image::SEMI_BRIGHTENED;
	}

	if(!is_shrouded) {
		draw_terrain_on_tile(x,y,image_type,ADJACENT_BACKGROUND);

		surface flag(get_flag(terrain,x,y));
		if(flag != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(flag,NULL,dst,&dstrect);
		}

		typedef overlay_map::const_iterator Itor;

		for(std::pair<Itor,Itor> overlays = overlays_.equal_range(loc);
			overlays.first != overlays.second; ++overlays.first) {

			surface overlay_surface(image::get_image(overlays.first->second.image,image_type));
			
			//note that dstrect can be changed by SDL_BlitSurface and so a
			//new instance should be initialized to pass to each call to
			//SDL_BlitSurface
			if(overlay_surface != NULL) {
				SDL_Rect dstrect = { xpos, ypos, 0, 0 };
				SDL_BlitSurface(overlay_surface,NULL,dst,&dstrect);
			}
		}
		draw_footstep(loc,xpos,ypos);
	} else {
		//FIXME: shouldn't void.png and fog.png be in the program configuration?
		surface surface(image::get_image("terrain/void.png"));

		if(surface == NULL) {
			std::cerr << "Could not get void surface!\n";
			return;
		}

		SDL_Rect dstrect = { xpos, ypos, 0, 0 };
		SDL_BlitSurface(surface,NULL,dst,&dstrect);
	}

	draw_unit_on_tile(x,y,unit_image,alpha,blend_to);

	if(!shrouded(x,y)) {
		draw_terrain_on_tile(x,y,image_type,ADJACENT_FOREGROUND);
	}

	if(fogged(x,y) && shrouded(x,y) == false) {
		const surface fog_surface(image::get_image("terrain/fog.png"));
		if(fog_surface != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(fog_surface,NULL,dst,&dstrect);
		}
	}
	
	if(!shrouded(x,y)) {
		draw_terrain_on_tile(x,y,image_type,ADJACENT_FOGSHROUD);
	}

	//draw the time-of-day mask on top of the hex
	if(tod_hex_mask1 != NULL || tod_hex_mask2 != NULL) {
		if(tod_hex_mask1 != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(tod_hex_mask1,NULL,dst,&dstrect);
		}

		if(tod_hex_mask2 != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(tod_hex_mask2,NULL,dst,&dstrect);
		}
	} else if(mask != "") {
		const surface img(image::get_image(mask,image::UNMASKED,image::NO_ADJUST_COLOUR));
		if(img != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(img,NULL,dst,&dstrect);
		}
	}

	if(grid_) {
		surface grid_surface(image::get_image("terrain/grid.png"));
		if(grid_surface != NULL) {
			SDL_Rect dstrect = { xpos, ypos, 0, 0 };
			SDL_BlitSurface(grid_surface,NULL,dst,&dstrect);
		}
	}

	if(game_config::debug && debugHighlights_.count(gamemap::location(x,y))) {
		const surface cross(image::get_image(game_config::cross_image));
		if(cross != NULL)
			draw_unit(xpos,ypos,cross,false,debugHighlights_[loc],0);
	}

	update_rect(xpos,ypos,zoom_,zoom_);
}

void display::draw_footstep(const gamemap::location& loc, int xloc, int yloc)
{
	std::vector<gamemap::location>::const_iterator i =
	         std::find(route_.steps.begin(),route_.steps.end(),loc);

	if(i == route_.steps.begin() || i == route_.steps.end())
		return;

	const bool show_time = (i+1 == route_.steps.end());

	const bool left_foot = is_even(i - route_.steps.begin());

	//generally we want the footsteps facing toward the direction they're going
	//to go next.
	//if we're on the last step, then we want them facing according to where
	//they came from, so we move i back by one
	if(i+1 == route_.steps.end() && i != route_.steps.begin())
		--i;

	gamemap::location::DIRECTION direction = gamemap::location::NORTH;

	if(i+1 != route_.steps.end()) {
		for(int n = 0; n != 6; ++n) {
			direction = gamemap::location::DIRECTION(n);
			if(i->get_direction(direction) == *(i+1)) {
				break;
			}
		}
	}

	static const std::string left_nw(game_config::foot_left_nw);
	static const std::string left_n(game_config::foot_left_n);
	static const std::string right_nw(game_config::foot_right_nw);
	static const std::string right_n(game_config::foot_right_n);

	const std::string* image_str = &left_nw;

	if(left_foot) {
		if(direction == gamemap::location::NORTH ||
		   direction == gamemap::location::SOUTH) {
			image_str = &left_n;
		} else {
			image_str = &left_nw;
		}
	} else {
		if(direction == gamemap::location::NORTH ||
		   direction == gamemap::location::SOUTH) {
			image_str = &right_n;
		} else {
			image_str = &right_nw;
		}
	}

	surface image(image::get_image(*image_str));
	if(image == NULL) {
		std::cerr << "Could not find image: " << *image_str << "\n";
		return;
	}

	const bool hflip = !(direction > gamemap::location::NORTH &&
	                     direction <= gamemap::location::SOUTH);
	const bool vflip = (direction >= gamemap::location::SOUTH_EAST &&
	                    direction <= gamemap::location::SOUTH_WEST);

	if(!hflip) {
		image.assign(image::reverse_image(image));
	}

	draw_unit(xloc,yloc,image,vflip,0.5);

	if(show_time && route_.move_left > 0 && route_.move_left < 10) {
		const SDL_Rect& rect = map_area();
		std::string str(1,'x');
		str[0] = '1' + route_.move_left;
		const SDL_Rect& text_area = font::text_area(str,18);
		const int x = xloc + zoom_/2 - text_area.w/2;
		const int y = yloc + zoom_/2 - text_area.h/2;

		//draw the text with a black outline
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x-1,y-1);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x-1,y);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x-1,y+1);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x,y-1);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x+1,y-1);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x+1,y);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x+1,y+1);
		font::draw_text(this,rect,18,font::DARK_COLOUR,str,x,y+1);
		font::draw_text(this,rect,18,font::YELLOW_COLOUR,str,x,y);
	}
}

namespace {
const std::string& get_direction(size_t n)
{
	const static std::string dirs[6] = {"-n","-ne","-se","-s","-sw","-nw"};
	return dirs[n >= sizeof(dirs)/sizeof(*dirs) ? 0 : n];
}

}

bool angle_is_northern(size_t n)
{
	const static bool results[6] = {true,false,false,false,false,true};
	return results[n >= sizeof(results)/sizeof(*results) ? 0 : n];
}

const std::string& get_angle_direction(size_t n)
{
	const static std::string dirs[6] = {"-ne","-e","-se","-sw","-w","-nw"};
	return dirs[n >= sizeof(dirs)/sizeof(*dirs) ? 0 : n];
}

std::vector<std::string> display::get_fog_shroud_graphics(const gamemap::location& loc)
{
	std::vector<std::string> res;
  
	gamemap::location adjacent[6];   
	get_adjacent_tiles(loc,adjacent);
	int tiles[6];
	static const int terrain_types[] = { gamemap::FOGGED, gamemap::VOID_TERRAIN, 0 };

	for(int i = 0; i != 6; ++i) {
		if(shrouded(adjacent[i].x,adjacent[i].y))
			tiles[i] = gamemap::VOID_TERRAIN;
		else if(!fogged(loc.x,loc.y) && fogged(adjacent[i].x,adjacent[i].y))
			tiles[i] = gamemap::FOGGED;
		else
			tiles[i] = 0;
	}


	for(const int * terrain = terrain_types; *terrain != 0; terrain ++) {
		//find somewhere that doesn't have overlap to use as a starting point
		int start;
		for(start = 0; start != 6; ++start) {
			if(tiles[start] != *terrain)
				break;
		}

		if(start == 6) {
			start = 0;
		}

		//find all the directions overlap occurs from
		for(int i = (start+1)%6, n = 0; i != start && n != 6; ++n) {
			if(tiles[i] == *terrain) {
				std::ostringstream stream;
				std::string name;
				// if(*terrain == gamemap::VOID_TERRAIN)
				//	stream << "void";
				//else
				//	stream << "fog";
				stream << "terrain/" << map_.get_terrain_info(*terrain).default_image();

				for(int n = 0; *terrain == tiles[i] && n != 6; i = (i+1)%6, ++n) {
					stream << get_direction(i);

					if(!image::exists(stream.str() + ".png")) {
						//if we don't have any surface at all,
						//then move onto the next overlapped area
						if(name.empty())
							i = (i+1)%6;
						break;
					} else {
						name = stream.str();
					}
				}

				if(!name.empty()) {
					res.push_back(name + ".png");
				}
			} else {
				i = (i+1)%6;
			}
		}
	}

	return res;
}

std::vector<surface> display::get_terrain_images(int x, int y, image::TYPE image_type, ADJACENT_TERRAIN_TYPE terrain_type)
{
	std::vector<surface> res;
	gamemap::location loc(x,y);

	if(terrain_type == ADJACENT_FOGSHROUD) {
		const std::vector<std::string> fog_shroud = get_fog_shroud_graphics(gamemap::location(x,y));

		if(!fog_shroud.empty()) {
			for(std::vector<std::string>::const_iterator it = fog_shroud.begin(); it != fog_shroud.end(); ++it) {
				image::locator image(*it);
				// image.filename = "terrain/" + *it;

				const surface surface(get_terrain(image,image_type,x,y));
				if(surface != NULL) {
					res.push_back(surface);
				}
			}

		}

		return res;
	}

	const time_of_day& tod = status_.get_time_of_day();
	// const time_of_day& tod_at = timeofday_at(status_,units_,gamemap::location(x,y));

	terrain_builder::ADJACENT_TERRAIN_TYPE builder_terrain_type =
	      (terrain_type == ADJACENT_FOREGROUND ?
		  terrain_builder::ADJACENT_FOREGROUND : terrain_builder::ADJACENT_BACKGROUND);
	const terrain_builder::imagelist* const terrains = builder_.get_terrain_at(loc,
			tod.id, builder_terrain_type);

	if(terrains != NULL) {
		for(std::vector<animated<image::locator> >::const_iterator it = terrains->begin(); it != terrains->end(); ++it) {
			// it->update_current_frame();
			image::locator image = it->get_current_frame();
			// image.filename = "terrain/" + image.filename;

			const surface surface(get_terrain(image,image_type,x,y));
			if(surface != NULL) {
				res.push_back(surface);
			}
		}
	} 

	return res;
}

surface display::get_terrain(const image::locator& image, image::TYPE image_type,
                                 int x, int y)
{

#if 0
	//see if there is a time-of-day specific version of this image
	if(search_tod) {
		// image::locator tod_image = image;
		// tod_image.filename = image.filename + "-" + tod.id + ".png";
		const image::locator& tod_image = image::get_alternative(image, "-" + tod.id);
		im = image::get_image(tod_image,image_type);

		if(im != NULL) {
			return im;
		}
	}
#endif

	// image::locator tmp = image;
	// tmp.filename += ".png";

	const surface im(image::get_image(image, image_type));
	if(im == NULL) {
		return im;
	}

#if 0
	//see if this tile is illuminated to a different colour than it'd
	//normally be displayed as
	const int radj = tod_at.red - tod.red;
	const int gadj = tod_at.green - tod.green;
	const int badj = tod_at.blue - tod.blue;
	
	if((radj|gadj|badj) != 0 && im != NULL) {
		const surface backup(im);
		im = surface(adjust_surface_colour(im,radj,gadj,badj));
		if(im == NULL)
			std::cerr << "could not adjust surface..\n";
	}
#endif
	
	return im;	
}

surface display::get_flag(gamemap::TERRAIN terrain, int x, int y)
{
	const bool village = map_.is_village(terrain);
	if(!village)
		return surface(NULL);

	const gamemap::location loc(x,y);

	for(size_t i = 0; i != teams_.size(); ++i) {
		if(teams_[i].owns_village(loc) && (!fogged(x,y) || !shrouded(x,y) && !teams_[currentTeam_].is_enemy(i+1))) {
			return image::get_image(flags_[i].get_current_frame());
		}
	}

	return surface(NULL);
}

void display::blit_surface(int x, int y, surface surf, SDL_Rect* srcrect, SDL_Rect* clip_rect)
{
	const surface target(video().getSurface());
	SDL_Rect dst = {x,y,0,0};
	
	if(clip_rect != NULL) {
		const clip_rect_setter clip_setter(target,*clip_rect);
		SDL_BlitSurface(surf,srcrect,target,&dst);
	} else {
		SDL_BlitSurface(surf,srcrect,target,&dst);
	}
}

surface display::get_minimap(int w, int h)
{
	if(minimap_ != NULL && (minimap_->w != w || minimap_->h != h)) {
		minimap_ = NULL;
	}

	if(minimap_ == NULL) {
		minimap_ = image::getMinimap(w,h,map_,
				status_.get_time_of_day().lawful_bonus,
				team_valid() ? &teams_[currentTeam_] : NULL);
	}

	return minimap_;
}

void display::set_paths(const paths* paths_list)
{
	pathsList_ = paths_list;
	invalidate_all();
}

void display::invalidate_route()
{
	for(std::vector<gamemap::location>::const_iterator i = route_.steps.begin();
	    i != route_.steps.end(); ++i) {
		invalidate(*i);
	}
}

void display::set_route(const paths::route* route)
{
	invalidate_route();

	if(route != NULL)
		route_ = *route;
	else
		route_.steps.clear();

	invalidate_route();
}

void display::remove_footstep(const gamemap::location& loc)
{
	const std::vector<gamemap::location>::iterator it = std::find(route_.steps.begin(),route_.steps.end(),loc);
	if(it != route_.steps.end())
		route_.steps.erase(it);
}

void display::float_label(const gamemap::location& loc, const std::string& text,
						  int red, int green, int blue)
{
	if(preferences::show_floating_labels() == false || fogged(loc.x,loc.y)) {
		return;
	}

	const SDL_Color colour = {red,green,blue,255};
	font::add_floating_label(text,24,colour,get_location_x(loc)+zoom_/2,get_location_y(loc),
	                         0,-2,60,screen_area(),font::CENTER_ALIGN,NULL,0,font::ANCHOR_LABEL_MAP);
}

void display::draw_unit(int x, int y, surface image,
		bool upside_down, double alpha, Uint32 blendto, double blend_ratio, double submerged,
		surface ellipse_back, surface ellipse_front)
{
	//calculate the y position of the ellipse. It should be the same as the y position of the image, unless
	//the image is partially submerged, in which case the ellipse should appear to float 'on top of' the water
	const int ellipse_ypos = y - (ellipse_back != NULL && submerged > 0.0 ? int(double(ellipse_back->h)*submerged) : 0)/2;
	if(ellipse_back != NULL) {
		draw_unit(x,ellipse_ypos,ellipse_back,false,blendto == 0 ? alpha : 1.0,0,0.0);
	}

	surface surf(image);

	if(upside_down) {
		surf.assign(flop_surface(surf));
	}

	if(blend_ratio != 0) {
		surf = blend_surface(surf, blend_ratio, blendto);
	}
	if(alpha > 1.0) {
		surf = brighten_image(surf,alpha);
	//} else if(alpha != 1.0 && blendto != 0) {
	//	surf.assign(blend_surface(surf,1.0-alpha,blendto));
	} else if(alpha != 1.0) {
		surf = adjust_surface_alpha(surf,alpha);
	}

	if(surf == NULL) {
		std::cerr << "surface lost...\n";
		return;
	}

	const int submerge_height = minimum<int>(surf->h,maximum<int>(0,int(surf->h*(1.0-submerged))));

	SDL_Rect clip_rect = map_area();
	SDL_Rect srcrect = {0,0,surf->w,submerge_height};
	blit_surface(x,y,surf,&srcrect,&clip_rect);

	if(submerge_height != surf->h) {
		surf.assign(adjust_surface_alpha(surf,0.2));
		
		srcrect.y = submerge_height;
		srcrect.h = surf->h-submerge_height;
		y += submerge_height;

		blit_surface(x,y,surf,&srcrect,&clip_rect);
	}

	if(ellipse_front != NULL) {
		draw_unit(x,ellipse_ypos,ellipse_front,false,blendto == 0 ? alpha : 1.0,0,0.0);
	}
}

struct is_energy_colour {
	bool operator()(Uint32 colour) const { return (colour&0xFF000000) < 0x50000000 &&
	                                              (colour&0x00FF0000) > 0x00990000 &&
												  (colour&0x0000FF00) > 0x00009900 &&
												  (colour&0x000000FF) > 0x00000099; }
};

const SDL_Rect& display::calculate_energy_bar(surface surf)
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
			if(first_row == -1)
				first_row = y;

			first_col = itor - i1;
			last_col = first_col + count;
			last_row = y;
		}
	}

	const SDL_Rect res = {first_col,first_row,last_col-first_col,last_row+1-first_row};
	energy_bar_rects_.insert(std::pair<surface,SDL_Rect>(surf,res));
	return calculate_energy_bar(surf);
}

void display::invalidate(const gamemap::location& loc)
{
	if(!invalidateAll_) {
		invalidated_.insert(loc);
	}
}

void display::invalidate_all()
{
	invalidateAll_ = true;
	invalidated_.clear();
	update_rect(map_area());
}

void display::invalidate_unit()
{
	invalidateUnit_ = true;
}

void display::invalidate_animations()
{
	bool animate_flags = false;
	gamemap::location topleft;
	gamemap::location bottomright;
	get_visible_hex_bounds(topleft, bottomright);

	for(size_t i = 0; i < flags_.size(); ++i) {
		flags_[i].update_current_frame();
		if(flags_[i].frame_changed()) 
			animate_flags = true;
	}

	for(int x = topleft.x; x <= bottomright.x; ++x) {
		for(int y = topleft.y; y <= bottomright.y; ++y) {
			gamemap::location loc(x,y);
			if(builder_.update_animation(loc) || (map_.is_village(loc) && animate_flags))
				invalidated_.insert(loc);
		}
	}
}

void display::recalculate_minimap()
{
	if(minimap_ != NULL) {
		minimap_.assign(NULL);
	}

	redraw_minimap();
}

void display::redraw_minimap()
{
	redrawMinimap_ = true;
}

void display::invalidate_game_status()
{
	invalidateGameStatus_ = true;
}

void display::add_overlay(const gamemap::location& loc, const std::string& img, const std::string& halo)
{
	const int halo_handle = halo::add(get_location_x(loc)+hex_size()/2,get_location_y(loc)+hex_size()/2,halo);
	const overlay item(img,halo,halo_handle);
	overlays_.insert(overlay_map::value_type(loc,item));
}

void display::remove_overlay(const gamemap::location& loc)
{
	typedef overlay_map::const_iterator Itor;
	std::pair<Itor,Itor> itors = overlays_.equal_range(loc);
	while(itors.first != itors.second) {
		halo::remove(itors.first->second.halo_handle);
		++itors.first;
	}

	overlays_.erase(loc);
}

void display::write_overlays(config& cfg) const
{
	for(overlay_map::const_iterator i = overlays_.begin(); i != overlays_.end(); ++i) {
		config& item = cfg.add_child("item");
		i->first.write(item);
		item["image"] = i->second.image;
		item["halo"] = i->second.halo;
	}
}

void display::set_team(size_t team)
{
	assert(team < teams_.size());
	currentTeam_ = team;

	labels().recalculate_shroud();
}

void display::set_playing_team(size_t team)
{
	assert(team < teams_.size());
	activeTeam_ = team;
	invalidate_game_status();
}

void display::set_advancing_unit(const gamemap::location& loc, double amount)
{
	advancingUnit_ = loc;
	advancingAmount_ = amount;
	draw_tile(loc.x,loc.y);
}

void display::lock_updates(bool value)
{
	if(value == true)
		++updatesLocked_;
	else
		--updatesLocked_;
}

bool display::update_locked() const
{
	return updatesLocked_ > 0;
}

bool display::turbo() const
{
	bool res = turbo_;
	if(keys_[SDLK_LSHIFT] || keys_[SDLK_RSHIFT])
		res = !res;

	return res;
}

void display::set_turbo(bool turbo)
{
	turbo_ = turbo;
}

void display::set_grid(bool grid)
{
	grid_ = grid;
}

void display::debug_highlight(const gamemap::location& loc, double amount)
{
	assert(game_config::debug);
	debugHighlights_[loc] += amount;
}

void display::clear_debug_highlights()
{
	debugHighlights_.clear();
}

bool display::shrouded(int x, int y) const
{
	if(team_valid())
		return teams_[currentTeam_].shrouded(x,y);
	else
		return false;
}

bool display::fogged(int x, int y) const
{
	if(team_valid())
		return teams_[currentTeam_].fogged(x,y);
	else
		return false;
}

bool display::team_valid() const
{
	return currentTeam_ < teams_.size();
}

size_t display::viewing_team() const
{
	return currentTeam_;
}

size_t display::playing_team() const
{
	return activeTeam_;
}

const theme& display::get_theme() const
{
	return theme_;
}

const theme::menu* display::menu_pressed(int mousex, int mousey, bool button_pressed)
{

	for(std::vector<gui::button>::iterator i = buttons_.begin(); i != buttons_.end(); ++i) {
		if(i->pressed()) {
			const size_t index = i - buttons_.begin();
			assert(index < theme_.menus().size());
			return &theme_.menus()[index];
		}
	}

	return NULL;
}

void display::begin_game()
{
	in_game_ = true;
	create_buttons();
}

int display::set_help_string(const std::string& str)
{
	font::remove_floating_label(help_string_);

	const SDL_Color colour = {0x0,0x00,0x00,0x77};
	help_string_ = font::add_floating_label(str,18,font::NORMAL_COLOUR,x()/2,y(),0.0,0.0,-1,screen_area(),font::CENTER_ALIGN,&colour,5);
	const SDL_Rect& rect = font::get_floating_label_rect(help_string_);
	font::move_floating_label(help_string_,0.0,-double(rect.h));
	return help_string_;
}

void display::clear_help_string(int handle)
{
	if(handle == help_string_) {
		font::remove_floating_label(handle);
		help_string_ = 0;
	}
}

void display::clear_all_help_strings()
{
	clear_help_string(help_string_);
}

void display::create_buttons()
{
	buttons_.clear();

	const std::vector<theme::menu>& buttons = theme_.menus();
	for(std::vector<theme::menu>::const_iterator i = buttons.begin(); i != buttons.end(); ++i) {
		gui::button b(*this,i->title(),gui::button::TYPE_PRESS,i->image());
		const SDL_Rect& loc = i->location(screen_area());
		b.set_location(loc.x,loc.y);

		if(rects_overlap(b.location(),map_area())) {
			b.set_volatile(true);
		}

		buttons_.push_back(b);
	}
}

void display::add_observer(const std::string& name)
{
	observers_.insert(name);
}

void display::remove_observer(const std::string& name)
{
	observers_.erase(name);
}

namespace {
	const int max_chat_messages = 6;
	const int chat_message_border = 5;
	const int chat_message_x = 10;
	const int chat_message_y = 10;
	const SDL_Color chat_message_colour = {200,200,200,200};
	const SDL_Color chat_message_bg     = {0,0,0,100};
}

void display::add_chat_message(const std::string& speaker, int side, const std::string& message, display::MESSAGE_TYPE type)
{
	std::string msg = message;
	gui::text_to_lines(msg,80);

	int ypos = chat_message_x;
	for(std::vector<chat_message>::const_iterator m = chat_messages_.begin(); m != chat_messages_.end(); ++m) {
		ypos += font::get_floating_label_rect(m->handle).h;
	}

	std::stringstream str;
	if(type == MESSAGE_PUBLIC) {
		str << "<" << speaker << ">";
	} else {
		str << "*" << speaker << "*";
	}

	std::stringstream message_str;
	message_str << msg;

	SDL_Color speaker_colour = {255,255,255,255};
	if(side >= 1) {
		speaker_colour = team::get_side_colour(side);
	}

	const SDL_Rect rect = map_area();
	const int speaker_handle = font::add_floating_label(str.str(),12,speaker_colour,
	                                                   rect.x+chat_message_x,rect.y+ypos,
													   0,0,-1,rect,font::LEFT_ALIGN,&chat_message_bg,chat_message_border);

	const int message_handle = font::add_floating_label(message_str.str(),12,chat_message_colour,
		rect.x + chat_message_x + font::get_floating_label_rect(speaker_handle).w,rect.y+ypos,
		0,0,-1,rect,font::LEFT_ALIGN,&chat_message_bg,chat_message_border);

	chat_messages_.push_back(chat_message(speaker_handle,message_handle));

	prune_chat_messages();
}

void display::clear_chat_messages()
{
	prune_chat_messages(true);
}

void display::prune_chat_messages(bool remove_all)
{
	const int message_ttl = remove_all ? 0 : 1200000;
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

void display::set_diagnostic(const std::string& msg)
{
	if(diagnostic_label_ != 0) {
		font::remove_floating_label(diagnostic_label_);
		diagnostic_label_ = 0;
	}

	if(msg != "") {
		diagnostic_label_ = font::add_floating_label(msg,16,font::YELLOW_COLOUR,300.0,50.0,0.0,0.0,-1,map_area());
	}
}

void display::rebuild_terrain(const gamemap::location &loc) {
	builder_.rebuild_terrain(loc);
}

void display::rebuild_all() {
	builder_.rebuild_all();
}

void display::add_highlighted_loc(const gamemap::location &hex) {
	// Only invalidate and insert if this is a new addition, for
	// efficiency.
	if (highlighted_locations_.find(hex) == highlighted_locations_.end()) {
		highlighted_locations_.insert(hex);
		invalidate(hex);
	}
}

void display::clear_highlighted_locs() {
	for (std::set<gamemap::location>::const_iterator it = highlighted_locations_.begin();
		 it != highlighted_locations_.end(); it++) {
		invalidate(*it);
	}
	highlighted_locations_.clear();
}

void display::remove_highlighted_loc(const gamemap::location &hex) {
	std::set<gamemap::location>::iterator it = highlighted_locations_.find(hex);
	// Only invalidate and remove if the hex was found, for efficiency.
	if (it != highlighted_locations_.end()) {
		highlighted_locations_.erase(it);
		invalidate(hex);
	}
}
