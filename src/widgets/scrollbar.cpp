/* $Id$*/
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "scrollbar.hpp"
#include "../display.hpp"
#include "../image.hpp"
#include "../video.hpp"

#include <algorithm>
#include <iostream>

namespace {
	const std::string scrollbar_top = "buttons/scrolltop.png";
	const std::string scrollbar_bottom = "buttons/scrollbottom.png";
	const std::string scrollbar_mid = "buttons/scrollmid.png";

	const std::string scrollbar_top_hl = "buttons/scrolltop-active.png";
	const std::string scrollbar_bottom_hl = "buttons/scrollbottom-active.png";
	const std::string scrollbar_mid_hl = "buttons/scrollmid-active.png";
	
	const std::string groove_top = "buttons/scrollgroove-top.png";
	const std::string groove_mid = "buttons/scrollgroove-mid.png";
	const std::string groove_bottom = "buttons/scrollgroove-bottom.png";

}

namespace gui {

scrollbar::scrollbar(display& d, widget const &pane, scrollable* callback)
	: widget(d), pane_(pane), mid_scaled_(NULL), groove_scaled_(NULL), callback_(callback),
	  uparrow_(d, "", button::TYPE_TURBO, "uparrow-button"),
	  downarrow_(d, "", button::TYPE_TURBO, "downarrow-button"),
	  state_(NORMAL), grip_position_(0), old_position_(0), grip_height_(0), full_height_(0)
{
	static const surface img(image::get_image(scrollbar_mid, image::UNSCALED));
	
	if (img != NULL) {
		set_width(img->w);
		// this is a bit rough maybe
		minimum_grip_height_ = 2 * img->h;
	}
}

void scrollbar::set_location(SDL_Rect const &rect)
{
	widget::set_location(rect);
	int uh = uparrow_.height(), dh = downarrow_.height();
	uparrow_.set_location(rect.x, rect.y);
	downarrow_.set_location(rect.x, rect.y + rect.h - dh);
	SDL_Rect r = rect;
	r.y += uh;
	r.h -= uh + dh;
	register_rectangle(r);
}

void scrollbar::hide(bool value)
{
	widget::hide(value);
	uparrow_.hide(value);
	downarrow_.hide(value);
}

unsigned scrollbar::get_position() const
{
	return grip_position_;
}

void scrollbar::set_position(unsigned pos)
{
	if (pos > full_height_ - grip_height_)
		pos = full_height_ - grip_height_;
	if (pos == grip_position_)
		return;
	grip_position_ = pos;
	uparrow_.enable(grip_position_ != 0);
	downarrow_.enable(grip_position_ < full_height_ - grip_height_);
	set_dirty();
}

void scrollbar::adjust_position(unsigned pos)
{
	if (pos < grip_position_)
		set_position(pos);
	else if (pos >= grip_position_ + grip_height_)
		set_position(pos - (grip_height_ - 1));
}

void scrollbar::move_position(int dep)
{
	int pos = grip_position_ + dep;
	if (pos > 0)
		set_position(pos);
	else
		set_position(0);
}

void scrollbar::set_shown_size(unsigned h)
{
	if (h > full_height_)
		h = full_height_;
	if (h == grip_height_)
		return;
	grip_height_ = h;
	set_position(grip_position_);
	set_dirty(true);
}

void scrollbar::set_full_size(unsigned h)
{
	if (h == full_height_)
		return;
	full_height_ = h;
	set_shown_size(grip_height_);
	set_position(grip_position_);
	set_dirty(true);
}

void scrollbar::process_event()
{
	if (uparrow_.pressed())
		move_position(-1);
	if (downarrow_.pressed())
		move_position(1);

	if (grip_position_ == old_position_)
		return;
	old_position_ = grip_position_;
	if (callback_)
		callback_->scroll(grip_position_);
}

SDL_Rect scrollbar::groove_area() const
{
	SDL_Rect loc = location();
	int uh = uparrow_.height();
	loc.y += uh;
	loc.h -= uh + downarrow_.height();
	return loc;
}

SDL_Rect scrollbar::grip_area() const
{
	SDL_Rect const &loc = groove_area();
	if (full_height_ == grip_height_)
		return loc;
	int h = (int)loc.h * grip_height_ / full_height_;
	if (h < minimum_grip_height_)
		h = minimum_grip_height_;
	int y = loc.y + ((int)loc.h - h) * grip_position_ / (full_height_ - grip_height_);
	SDL_Rect res = { loc.x, y, loc.w, h };
	return res;
}

void scrollbar::draw_contents()
{
	const surface mid_img(image::get_image(state_ != NORMAL ? 
					scrollbar_mid_hl : scrollbar_mid, image::UNSCALED));
	const surface bottom_img(image::get_image(state_ != NORMAL ? 
					scrollbar_bottom_hl : scrollbar_bottom, image::UNSCALED));
	const surface top_img(image::get_image(state_ != NORMAL ?
					scrollbar_top_hl : scrollbar_top, image::UNSCALED));

	const surface top_grv(image::get_image(groove_top,image::UNSCALED));
	const surface mid_grv(image::get_image(groove_mid,image::UNSCALED));
	const surface bottom_grv(image::get_image(groove_bottom,image::UNSCALED));

	if (mid_img == NULL || bottom_img == NULL || top_img == NULL
	 || top_grv == NULL || bottom_grv == NULL || mid_grv == NULL) {
		std::cerr << "Failure to load scrollbar image.\n";
		return;
	}

	SDL_Rect grip = grip_area();
	int mid_height = grip.h - top_img->h - bottom_img->h;
	if (mid_height <= 0) {
		// for now, minimum size of the middle piece is 1. This should
		// never really be encountered, and if it is, it's a symptom
		// of a larger problem, I think.
		mid_height = 1;
	}

	if(mid_scaled_.null() || mid_scaled_->h != mid_height) {
		mid_scaled_.assign(scale_surface_blended(mid_img,mid_img->w,mid_height));
	}

	SDL_Rect groove = groove_area();
	int groove_height = groove.h - top_grv->h - bottom_grv->h;
	if (groove_height <= 0) {
		groove_height = 1;
	}

	if (groove_scaled_.null() || groove_scaled_->h != groove_height) {
		groove_scaled_.assign(scale_surface_blended(mid_grv,mid_grv->w,groove_height));
	}

	if (mid_scaled_.null() || groove_scaled_.null()) {
		std::cerr << "Failure during scrollbar image scale.\n";
		return;
	}

	if (grip.h > groove.h) {
		std::cerr << "abort draw scrollbar: grip too large\n";
		return;
	}

	surface const screen = disp().video().getSurface();

	// draw scrollbar "groove"
	disp().blit_surface(groove.x, groove.y, top_grv);
	disp().blit_surface(groove.x, groove.y + top_grv->h, groove_scaled_);
	disp().blit_surface(groove.x, groove.y + top_grv->h + groove_height, bottom_grv);

	// draw scrollbar "grip"
	disp().blit_surface(grip.x, grip.y, top_img);
	disp().blit_surface(grip.x, grip.y + top_img->h, mid_scaled_);
	disp().blit_surface(grip.x, grip.y + top_img->h + mid_height, bottom_img);

	update_rect(groove);
}	

void scrollbar::handle_event(const SDL_Event& event)
{
	if (hidden())
		return;

	STATE new_state = state_;
	SDL_Rect const &grip = grip_area();
	SDL_Rect const &groove = groove_area();

	switch (event.type) {
	case SDL_MOUSEBUTTONUP:
	{
		SDL_MouseButtonEvent const &e = event.button;
		bool on_grip = point_in_rect(e.x, e.y, grip);
		new_state = on_grip ? ACTIVE : NORMAL;
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
	{
		SDL_MouseButtonEvent const &e = event.button;
		bool on_grip = point_in_rect(e.x, e.y, grip);
		bool on_groove = point_in_rect(e.x, e.y, groove);
		bool on_scrollable = point_in_rect(e.x, e.y, pane_.location()) || on_groove;
		if (on_scrollable && e.button == SDL_BUTTON_WHEELDOWN) {
			move_position(1);
		} else if (on_scrollable && e.button == SDL_BUTTON_WHEELUP) {
			move_position(-1);
		} else if (on_grip && e.button == SDL_BUTTON_LEFT) {
			mousey_on_grip_ = e.y - grip.y;
			new_state = DRAGGED;
		} else if (on_groove && e.button == SDL_BUTTON_LEFT && groove.h != grip.h) {
			if (e.y < grip.y)
				move_position(-grip_height_);
			else
				move_position(grip_height_);
		}
		break;
	}
	case SDL_MOUSEMOTION:
	{
		SDL_MouseMotionEvent const &e = event.motion;
		if (state_ == NORMAL || state_ == ACTIVE) {
			bool on_grip = point_in_rect(e.x, e.y, grip);
			new_state = on_grip ? ACTIVE : NORMAL;
		} else if (state_ == DRAGGED && groove.h != grip.h) {
			int y_dep = e.y - grip.y - mousey_on_grip_;
			int dep = y_dep * (full_height_ - grip_height_) / (int)(groove.h - grip.h);
			move_position(dep);
		}
		break;
	}
	default:
		break;
	}

	if ((new_state == NORMAL) ^ (state_ == NORMAL)) {
		set_dirty();
		mid_scaled_.assign(NULL);
	}
	state_ = new_state;
}

}
