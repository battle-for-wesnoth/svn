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

#include "../global.hpp"

#include "textbox.hpp"
#include "../clipboard.hpp"
#include "../font.hpp"
#include "../language.hpp"
#include "../log.hpp"
#include "../show_dialog.hpp"
#include "../util.hpp"
#include "../video.hpp"
#include "SDL.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace gui {

const int font_size = font::SIZE_PLUS;

textbox::textbox(display& d, int width, const std::string& text, bool editable, size_t max_size, double alpha, double alpha_focus)
	   : scrollarea(d), max_size_(max_size), text_(utils::string_to_wstring(text)),
	     cursor_(text_.size()), selstart_(-1), selend_(-1),
	     grabmouse_(false), text_pos_(0), editable_(editable),
	     show_cursor_(true), show_cursor_at_(0), text_image_(NULL),
	     wrap_(false), line_height_(0), yscroll_(0), alpha_(alpha),
		  alpha_focus_(alpha_focus)
{
	// static const SDL_Rect area = d.screen_area();
	// const int height = font::draw_text(NULL,area,font_size,font::NORMAL_COLOUR,"ABCD",0,0).h;
	set_measurements(width, font::get_max_height(font_size));
	set_scroll_rate(font::get_max_height(font_size) / 2);
	update_text_cache(true);
}

void textbox::set_inner_location(SDL_Rect const &rect)
{
	bg_register(rect);
}

const std::string textbox::text() const
{
	const std::string &ret = utils::wstring_to_string(text_);
	return ret;
}

// set_text does not respect max_size_
void textbox::set_text(const std::string& text)
{
	text_ = utils::string_to_wstring(text);
	cursor_ = text_.size();
	text_pos_ = 0;
	selstart_ = -1;
	selend_ = -1;
	set_dirty(true);
	update_text_cache(true);
}

void textbox::append_text(const std::string& text)
{
	if(text_image_.get() == NULL) {
		set_text(text);
		return;
	}

	const wide_string& wtext = utils::string_to_wstring(text);

	const surface new_text = add_text_line(wtext);
	const surface new_surface = create_compatible_surface(text_image_,maximum<size_t>(text_image_->w,new_text->w),text_image_->h+new_text->h);

	SDL_SetAlpha(new_text.get(),0,0);
	SDL_SetAlpha(text_image_.get(),0,0);

	SDL_BlitSurface(text_image_,NULL,new_surface,NULL);

	SDL_Rect target = {0,text_image_->h,new_text->w,new_text->h};
	SDL_BlitSurface(new_text,NULL,new_surface,&target);
	text_image_.assign(new_surface);

	text_.resize(text_.size() + wtext.size());
	std::copy(wtext.begin(),wtext.end(),text_.end()-wtext.size());

	set_dirty(true);
	update_text_cache(false);
}

void textbox::clear()
{
	text_.clear();
	cursor_ = 0;
	cursor_pos_ = 0;
	text_pos_ = 0;
	selstart_ = -1;
	selend_ = -1;
	set_dirty(true);
	update_text_cache(true);
}

void textbox::draw_cursor(int pos, CVideo &video) const
{
	if(show_cursor_ && editable_) {
		SDL_Rect rect = {location().x + pos, location().y, 1, location().h };
		surface const frame_buffer = video.getSurface();
		SDL_FillRect(frame_buffer,&rect,SDL_MapRGB(frame_buffer->format,255,255,255));
	}
}

void textbox::draw_contents()
{
	SDL_Rect const &loc = inner_location();

	surface surf = video().getSurface();
	gui::draw_solid_tinted_rectangle(loc.x,loc.y,loc.w,loc.h,0,0,0,
	                          focus() ? alpha_focus_ : alpha_, surf);
	
	SDL_Rect src;

	if(text_image_ != NULL) {
		src.y = yscroll_;
		src.w = minimum<size_t>(loc.w,text_image_->w);
		src.h = minimum<size_t>(loc.h,text_image_->h);
		src.x = text_pos_;
		SDL_Rect dest = screen_area();
		dest.x = loc.x;
		dest.y = loc.y;

		// Fills the selected area
		if(is_selection()) {
			const int start = minimum<int>(selstart_,selend_);
			const int end = maximum<int>(selstart_,selend_);
			int startx = char_x_[start];
			int starty = char_y_[start];
			const int endx = char_x_[end];
			const int endy = char_y_[end];

			while(starty <= endy) {
				const size_t right = starty == endy ? endx : text_image_->w;
				if(right <= size_t(startx)) {
					break;
				}

				SDL_Rect rect = { loc.x + startx, loc.y + starty - src.y, right - startx, line_height_ };

				const clip_rect_setter clipper(surf, loc);

				Uint32 colour = SDL_MapRGB(surf->format, 160, 0, 0);
				fill_rect_alpha(rect, colour, 140, surf);

				starty += int(line_height_);
				startx = 0;
			}
		}

		SDL_BlitSurface(text_image_, &src, surf, &dest);
	}

	draw_cursor((cursor_pos_ == 0 ? 0 : cursor_pos_ - 1), video());

	update_rect(loc);
}

void textbox::process()
{
	if(editable_) {
		if(focus()) {
			const int ticks = SDL_GetTicks();
			if(ticks > show_cursor_at_+500) {
				show_cursor_ = !show_cursor_;
				show_cursor_at_ = ticks;
				set_dirty();
			}
		} else if(show_cursor_ == true) {
			show_cursor_ = false;
			set_dirty();
		}
	}

	draw();
}

void textbox::set_editable(bool value)
{
	editable_ = value;
}

bool textbox::editable() const
{
	return editable_;
}

void textbox::scroll_to_bottom()
{
	set_position(get_max_position());
}

void textbox::set_wrap(bool val)
{
	if(wrap_ != val) {
		wrap_ = val;
		update_text_cache(true);
		set_dirty(true);
	}
}

void textbox::set_location(const SDL_Rect& rect)
{
	text_pos_ = 0;

	scrollarea::set_location(rect);
}

void textbox::scroll(int pos)
{
	yscroll_ = pos;
	set_dirty(true);
}

surface textbox::add_text_line(const wide_string& text)
{
	line_height_ = font::get_max_height(font_size);

	if(char_y_.empty()) {
		char_y_.push_back(0);
	} else {
		char_y_.push_back(char_y_.back() + line_height_);
	}

	char_x_.push_back(0);
	
	// Re-calculate the position of each glyph. We approximate this by asking the
	// width of each substring, but this is a flawed assumption which won't work with
	// some more complex scripts (that is, RTL languages). This part of the work should
	// actually be done by the font-rendering system.
	std::string visible_string;
	wide_string wrapped_text;
	 
	wide_string::const_iterator backup_itor = text.end();
	
	wide_string::const_iterator itor = text.begin();
	while(itor != text.end()) {
		//If this is a space, save copies of the current state so we can roll back
		if(char(*itor) == ' ') {
			backup_itor = itor;
		}
		visible_string.append(utils::wchar_to_string(*itor));

		if(char(*itor) == '\n') {
			backup_itor = text.end();
			visible_string = "";
		}

		int w = font::line_width(visible_string, font_size);

		if(wrap_ && w >= inner_location().w) {
			if(backup_itor != text.end()) {
				int backup = itor - backup_itor;
				itor = backup_itor + 1;
				if(backup > 0) {
					char_x_.erase(char_x_.end()-backup, char_x_.end());
					char_y_.erase(char_y_.end()-backup, char_y_.end());
					wrapped_text.erase(wrapped_text.end()-backup, wrapped_text.end());
				}
			}
			backup_itor = text.end();
			wrapped_text.push_back(wchar_t('\n'));
			char_x_.push_back(0);
			char_y_.push_back(char_y_.back() + line_height_);
			visible_string = "";
		} else {
			wrapped_text.push_back(*itor);
			char_x_.push_back(w);
			char_y_.push_back(char_y_.back() + (char(*itor) == '\n' ? line_height_ : 0));
			++itor;
		}
	}

	const std::string s = utils::wstring_to_string(wrapped_text);
	const surface res(font::get_rendered_text(s, font_size, font::NORMAL_COLOUR));		

	return res;
}


void textbox::update_text_cache(bool changed)
{
	if(changed) {
		char_x_.clear();
		char_y_.clear();

		text_image_.assign(add_text_line(text_));
	}

	int cursor_x = char_x_[cursor_];

	if(cursor_x - text_pos_ > location().w) {
		text_pos_ = cursor_x - location().w;
	} else if(cursor_x - text_pos_ < 0) {
		text_pos_ = cursor_x;
	}
	cursor_pos_ = cursor_x - text_pos_;

	if (!text_image_.null()) {
		set_full_size(text_image_->h);
		set_shown_size(location().h);
	}
}

bool textbox::is_selection() 
{
	return (selstart_ != -1) && (selend_ != -1) && (selstart_ != selend_);
}

void textbox::erase_selection()
{
	if(!is_selection())
		return;
	
	wide_string::iterator itor = text_.begin() + minimum(selstart_, selend_);
	text_.erase(itor, itor + abs(selend_ - selstart_));
	cursor_ = minimum(selstart_, selend_);
	selstart_ = selend_ = -1;
}

void textbox::handle_event(const SDL_Event& event)
{
	bool changed = false;
	
	//Sanity check: verify that selection start and end are within text
	//boundaries
	if(is_selection() && !(size_t(selstart_) <= text_.size() && size_t(selend_) <= text_.size())) {
		lg::warn(lg::display) << "out-of-boundary selection\n";
		selstart_ = selend_ = -1;
	}

	int mousex, mousey;
	const Uint8 mousebuttons = SDL_GetMouseState(&mousex,&mousey);
	if(!(mousebuttons & SDL_BUTTON(1))) {
		grabmouse_ = false;
	}

	SDL_Rect const &loc = inner_location();

	if ((grabmouse_ && (event.type == SDL_MOUSEMOTION)) ||
	    (event.type == SDL_MOUSEBUTTONDOWN && (mousebuttons & SDL_BUTTON(1)) &&
	     point_in_rect(mousex, mousey, loc))) {
		const int x = mousex - loc.x + text_pos_;
		const int y = mousey - loc.y;
		int pos = 0;
		int distance = x;

		for(int i = 1; i < int(char_x_.size()); ++i) {
			if(yscroll_ + y < char_y_[i]) {
				break;
			}

			// Check individually each distance (if, one day, we support
			// RTL languages, char_x_[c] may not be monotonous.)
			if(abs(x - char_x_[i]) < distance && yscroll_ + y < char_y_[i] + line_height_) {
				pos = i;
				distance = abs(x - char_x_[i]);
			}
		}

		cursor_ = pos;

		if(grabmouse_)
			selend_ = cursor_;
		
		update_text_cache(false);

		if(!grabmouse_ && mousebuttons & SDL_BUTTON(1)) {
			grabmouse_ = true;
			selstart_ = selend_ = cursor_;
		} else if (! (mousebuttons & SDL_BUTTON(1))) {
			grabmouse_ = false;
		}

		set_dirty();
	}

	if(editable_ == false) {
		return;
	}

	//if we don't have the focus, then see if we gain the focus,
	//otherwise return
	if(focus() == false) {
		if (event.type == SDL_MOUSEMOTION && point_in_rect(mousex, mousey, loc))
			events::focus_handler(this);
		   
		return;
	}

	if(event.type != SDL_KEYDOWN || focus() != true) {
		draw();
		return;
	}

	const SDL_keysym& key = reinterpret_cast<const SDL_KeyboardEvent&>(event).keysym;
	const SDLMod modifiers = SDL_GetModState();
	
	const int c = key.sym;
	int old_cursor = cursor_;
	
	if(c == SDLK_LEFT && cursor_ > 0) 
		--cursor_;

	if(c == SDLK_RIGHT && cursor_ < text_.size())
		++cursor_;

	if(c == SDLK_END)
		cursor_ = text_.size();
	
	if(c == SDLK_HOME)
		cursor_ = 0;

	if((old_cursor != cursor_) && (modifiers & KMOD_SHIFT)) {
		if(selstart_ == -1) 
			selstart_ = old_cursor;
		selend_ = cursor_;
	} 
	
	if(c == SDLK_BACKSPACE) {
		changed = true;
		if(is_selection()) {
			erase_selection();
		} else if(cursor_ > 0) {
			--cursor_;
			text_.erase(text_.begin()+cursor_);
		}
	}

	if(c == SDLK_DELETE && !text_.empty()) {
		changed = true;
		if(is_selection()) {
			erase_selection();
		} else {
			if(cursor_ == text_.size()) {
				text_.resize(text_.size()-1);
				--cursor_;
			} else {
				text_.erase(text_.begin()+cursor_);
			}
		}
	}

	wchar_t character = key.unicode;

	//movement characters may have a "Unicode" field on some platforms, so ignore it.
	if(!(c == SDLK_UP || c == SDLK_DOWN || c == SDLK_LEFT || c == SDLK_RIGHT ||
	   c == SDLK_DELETE || c == SDLK_BACKSPACE || c == SDLK_END || c == SDLK_HOME)) {
		if(character != 0)
			lg::info(lg::display) << "Char: " << character << ", c = " << c << "\n";
	
		if(event.key.keysym.mod & KMOD_CTRL) {
			switch(c) {
			case SDLK_v:
				{
				changed = true;
				if(is_selection())
					erase_selection();

				wide_string s = utils::string_to_wstring(copy_from_clipboard());
				if(text_.size() < max_size_) {
					if(s.size() + text_.size() > max_size_) {
						s.resize(max_size_ - text_.size());
					}
					text_.insert(text_.begin()+cursor_, s.begin(), s.end());
					cursor_ += s.size();
				}
				}

				break;

			case SDLK_c:
				{
				const size_t beg = minimum<size_t>(size_t(selstart_),size_t(selend_));
				const size_t end = maximum<size_t>(size_t(selstart_),size_t(selend_));

				wide_string ws = wide_string(text_.begin() + beg, text_.begin() + end);
				std::string s = utils::wstring_to_string(ws);
				copy_to_clipboard(s);
				} 
				break;
			}
		} else {
			if(character >= 32 && character != 127) {
				changed = true;
				if(is_selection()) 
					erase_selection();

				if(text_.size() + 1 <= max_size_) {
					text_.insert(text_.begin()+cursor_,character);
					++cursor_;		
				}
			}
		}
	}

	if(is_selection() && (selend_ != cursor_))
		selstart_ = selend_ = -1;

	//since there has been cursor activity, make the cursor appear for
	//at least the next 500ms.
	show_cursor_ = true;
	show_cursor_at_ = SDL_GetTicks();

	update_text_cache(changed);
	set_dirty(true);
	draw();
}

} //end namespace gui
