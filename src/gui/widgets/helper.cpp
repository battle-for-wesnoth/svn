/* $Id$ */
/*
   copyright (C) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/widgets/helper.hpp"

#include "gettext.hpp"
#include "gui/widgets/settings.hpp"
#include "sdl_utils.hpp"
#include "serialization/string_utils.hpp"
#include "tstring.hpp"
#include "log.hpp"

#include "SDL_ttf.h"

namespace gui2 {

namespace {
	static bool initialized_ = false;
}

bool init() {
	if(initialized_) {
		return true;
	}
	
	load_settings();

	initialized_ = true;

	return initialized_;
}

SDL_Rect create_rect(const tpoint& origin, const tpoint& size) 
{ 
	return ::create_rect(origin.x, origin.y, size.x, size.y); 
}

tpoint& tpoint::operator+=(const tpoint& point)
{ 
	x += point.x; 
	y += point.y;
	return *this;
}

tpoint& tpoint::operator-=(const tpoint& point)
{ 
	x -= point.x; 
	y -= point.y;
	return *this;
}

std::ostream &operator<<(std::ostream &stream, const tpoint& point)
{
	stream << point.x << ',' << point.y;
	return stream;
}

unsigned decode_font_style(const std::string& style)
{
	if(style == "bold") {
		return TTF_STYLE_BOLD;
	} else if(style == "italic") {
		return TTF_STYLE_ITALIC;
	} else if(style == "underline") {
		return TTF_STYLE_UNDERLINE;
	} else if(style.empty() || style == "normal") {
		return TTF_STYLE_NORMAL;
	}

	ERR_GUI << "Unknown style '" << style << "' using 'normal' instead.\n";

	return TTF_STYLE_NORMAL;
}

Uint32 decode_colour(const std::string& colour)
{
	std::vector<std::string> fields = utils::split(colour);

	// make sure we have four fields
	while(fields.size() < 4) fields.push_back("0");

	Uint32 result = 0;
	for(int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		result |= lexical_cast_default<int>(fields[i]);
	}

	return result;
}

surface save_background(const surface& background, const SDL_Rect& rect)
{
	assert(background);
	assert((background->flags & SDL_RLEACCEL) == 0);
	assert(rect.x + rect.w <= background->w);
	assert(rect.y + rect.h <= background->h);

	surface result(create_neutral_surface(rect.w, rect.h));

	{
		// Extra scoping used for the surface_lock.
		surface_lock src_lock(background);
		surface_lock dst_lock(result);

		Uint32* src_pixels = reinterpret_cast<Uint32*>(src_lock.pixels());
		Uint32* dst_pixels = reinterpret_cast<Uint32*>(dst_lock.pixels());

		unsigned offset = rect.y * background->w + rect.x;
		for(unsigned y = 0; y < rect.h; ++y) {
			for(unsigned x = 0; x < rect.w; ++x) {

				*dst_pixels++ = src_pixels[offset + x];
			
			}
			offset += background->w;
		}
	}

	return result;
}

void restore_background(const surface& restorer, 
		surface& background, const SDL_Rect& rect)
{
	assert(background);
	assert(restorer);
	assert((background->flags & SDL_RLEACCEL) == 0);
	assert((restorer->flags & SDL_RLEACCEL) == 0);
	assert(rect.x + rect.w <= background->w);
	assert(rect.y + rect.h <= background->h);

	{
		// Extra scoping used for the surface_lock.
		surface_lock src_lock(restorer);
		surface_lock dst_lock(background);

		Uint32* src_pixels = reinterpret_cast<Uint32*>(src_lock.pixels());
		Uint32* dst_pixels = reinterpret_cast<Uint32*>(dst_lock.pixels());

		unsigned offset = rect.y * background->w + rect.x;
		for(unsigned y = 0; y < rect.h; ++y) {
			for(unsigned x = 0; x < rect.w; ++x) {

				dst_pixels[offset + x] = *src_pixels++;

			}
			offset += background->w;
		}
	}
}

t_string missing_widget(const std::string& id)
{
	utils::string_map symbols;
	symbols["id"] = id;

	return t_string(vgettext("Mandatory widget '$id' hasn't been defined.", symbols));
}

std::string get_uid()
{
	static unsigned id = 0;
	++id;
	assert(id); // avoid wrapping.

	return "____" + lexical_cast<std::string>(id);

}

} // namespace gui2

