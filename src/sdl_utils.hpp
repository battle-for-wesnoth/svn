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
#ifndef SDL_UTILS_INCLUDED
#define SDL_UTILS_INCLUDED

#include "scoped_resource.hpp"

#include "SDL.h"

#include <cstdlib>
#include <iostream>
#include <string>

//older versions of SDL don't define the
//mouse wheel macros, so define them ourselves
//if necessary.
#ifndef SDL_BUTTON_WHEELUP
#define SDL_BUTTON_WHEELUP 4
#endif

#ifndef SDL_BUTTON_WHEELDOWN
#define SDL_BUTTON_WHEELDOWN 5
#endif

bool point_in_rect(int x, int y, const SDL_Rect& rect);

bool rects_overlap(const SDL_Rect& rect1, const SDL_Rect& rect2);

struct surface
{
private:
	static void sdl_add_ref(SDL_Surface *surf)
	{
		if (surf != NULL)
			++surf->refcount;
	}

	struct free_sdl_surface {
		void operator()(SDL_Surface *surf) const
		{
			if (surf != NULL)
				 SDL_FreeSurface(surf); 
		}
	};

	typedef util::scoped_resource<SDL_Surface*,free_sdl_surface> scoped_sdl_surface;
public:
	surface() : surface_(NULL) 
	{}

	surface(SDL_Surface *surf) : surface_(surf)
	{}

	surface(const surface& o) : surface_(o.surface_.get())
	{
		sdl_add_ref(surface_.get());
	}

	void assign(const surface& o)
	{
		SDL_Surface *surf = o.surface_.get();
		sdl_add_ref(surf); // need to be done before assign to avoid corruption on "a=a;"
		surface_.assign(surf);
	}

	surface& operator=(const surface& o)
	{
		assign(o);
		return *this;
	}

	operator SDL_Surface*() const { return surface_.get(); }

	SDL_Surface* get() const { return surface_.get(); }

	SDL_Surface* operator->() const { return surface_.get(); }

	void assign(SDL_Surface* surf) { surface_.assign(surf); }

	bool null() const { return surface_.get() == NULL; }

private:
	scoped_sdl_surface surface_;
};

bool operator<(const surface& a, const surface& b);

surface make_neutral_surface(surface surf);
surface create_optimized_surface(surface surface);
surface scale_surface(surface surface, int w, int h);
surface scale_surface_blended(surface surface, int w, int h);
surface adjust_surface_colour(surface surface, int r, int g, int b);
surface greyscale_image(surface surface);
surface brighten_image(surface surface, double amount);
surface get_surface_portion(surface src, SDL_Rect& rect);
surface adjust_surface_alpha(surface surface, double amount);
surface adjust_surface_alpha_add(surface surface, int amount);
surface mask_surface(surface surf, surface mask);
surface blur_surface(surface surf, int depth=1);
surface cut_surface(surface surface, const SDL_Rect& r);
surface blend_surface(surface surface, double amount, Uint32 colour);
surface flip_surface(surface surface);
surface flop_surface(surface surface);
surface create_compatible_surface(surface surf, int width=-1, int height=-1);

void fill_rect_alpha(SDL_Rect& rect, Uint32 colour, Uint8 alpha, surface target);

SDL_Rect get_non_transperant_portion(surface surf);

bool operator==(const SDL_Rect& a, const SDL_Rect& b);
bool operator!=(const SDL_Rect& a, const SDL_Rect& b);

class config; // no need to include config.hpp

struct pixel_data
{
	pixel_data() : r(0), g(0), b(0)
	{}

	pixel_data(int red, int green, int blue) : r(red), g(green), b(blue)
	{}

	pixel_data(int pixel, SDL_PixelFormat* fmt) {
		unformat(pixel, fmt);
	}

	pixel_data(config& cfg) {
		read(cfg);
	}

	int format(SDL_PixelFormat* fmt) const {
		return SDL_MapRGB(fmt,r,g,b);
	}

	void unformat(int pixel, SDL_PixelFormat* fmt) {
		r = ((pixel&fmt->Rmask) >> fmt->Rshift);
		g = ((pixel&fmt->Gmask) >> fmt->Gshift);
		b = ((pixel&fmt->Bmask) >> fmt->Bshift);
	}

	void read(const config& cfg);

	int r, g, b;
};

struct surface_lock
{
	surface_lock(surface surface) : surface_(surface), locked_(false)
	{
		if(SDL_MUSTLOCK(surface_)) {
			const int res = SDL_LockSurface(surface_);
			if(res == 0) {
				locked_ = true;
			}
		}
	}

	~surface_lock()
	{
		if(locked_) {
			SDL_UnlockSurface(surface_);
		}
	}

	Uint32* pixels() { return reinterpret_cast<Uint32*>(surface_->pixels); }
private:
	surface const surface_;
	bool locked_;
};

struct surface_restorer
{
	surface_restorer();
	surface_restorer(class CVideo* target, const SDL_Rect& rect);
	~surface_restorer();

	void restore();
	void update();
	void cancel();

	const SDL_Rect& area() const { return rect_; }

private:
	class CVideo* target_;
	SDL_Rect rect_;
	surface surface_;
};

struct clip_rect_setter
{
	clip_rect_setter(surface surf, SDL_Rect& r) : surface_(surf)
	{
		SDL_GetClipRect(surface_,&rect);
		SDL_SetClipRect(surface_,&r);
	}

	~clip_rect_setter() { SDL_SetClipRect(surface_,&rect); }

private:
	surface surface_;
	SDL_Rect rect;
};

#endif
