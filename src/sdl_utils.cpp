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

#include <algorithm>
#include <cmath>
#include <iostream>

#include "game.hpp"
#include "sdl_utils.hpp"
#include "util.hpp"
#include "video.hpp"

int sdl_add_ref(SDL_Surface* surface)
{
	if(surface != NULL)
		return surface->refcount++;
	else
		return 0;
}

void draw_unit_ellipse(SDL_Surface* target, short colour, const SDL_Rect& clip, int unitx, int unity,
                       SDL_Surface* behind, bool image_reverse)
{
	const int xloc = unitx + (behind->w*15)/100;
	const int yloc = unity + (behind->h*7)/10;
	const int width = (behind->w*70)/100;
	const int height = behind->h/6;

	const double centerx = xloc + double(width)*0.5;
	const double centery = yloc + double(height)*0.5;
	const double r = double(width)*0.5;

	const double yratio = double(height)/double(width);

	surface_lock lock(behind);
	const short* const pixels = lock.pixels();
	const int pad = is_odd(behind->w) ? 1 : 0;

	int last_y = 0;
	for(int xit = xloc; xit != xloc+width; ++xit) {
		//r^2 = x^2 + y^2
		//y^2 = r^2 - x^2
		const double x = double(xit) - centerx;
		const int y = int(sqrt(r*r - x*x)*yratio);

		const int direction = y > last_y ? 1 : -1;
		for(int i = last_y; i != y+direction; i += direction) {
			int yit = yloc+height/2-y;
			int xpos = xit - unitx;
			if(image_reverse)
				xpos = behind->w - xpos - 1;

			int ypos = yit - unity;
			if(xit >= clip.x && yit >= clip.y && xit < clip.x + clip.w && yit+1 < clip.y + clip.h &&
				xpos >= 0 && ypos >= 0 && xpos < behind->w && ypos+1 < behind->h &&
			    pixels[ypos*(behind->w+pad) + xpos] == 0) {
				SDL_Rect rect = {xit,yit,1,2};
				SDL_FillRect(target,&rect,colour);
			}

			yit = yloc+height/2+y;
			ypos = yit - unity;
			if(xit >= clip.x && yit >= clip.y && xit < clip.x + clip.w && yit+1 < clip.y + clip.h &&
				xpos >= 0 && ypos >= 0 && xpos < behind->w && ypos+1 < behind->h) {
				SDL_Rect rect = {xit,yit,1,2};
				SDL_FillRect(target,&rect,colour);
			}
		}

		last_y = y;
	}
}

SDL_Surface* clone_surface(SDL_Surface* surface)
{
	if(surface == NULL)
		return NULL;

	return scale_surface(surface,surface->w,surface->h);
}

SDL_Surface* scale_surface(SDL_Surface* surface, int w, int h)
{
	if(surface == NULL)
		return NULL;

	SDL_Surface* dest = SDL_CreateRGBSurface(SDL_SWSURFACE,w,h,
					                     surface->format->BitsPerPixel,
										 surface->format->Rmask,
										 surface->format->Gmask,
										 surface->format->Bmask,
										 surface->format->Amask);

	if(dest == NULL) {
		std::cerr << "Could not create surface to scale onto\n";
		return NULL;
	}

	SDL_SetColorKey(dest,SDL_SRCCOLORKEY,SDL_MapRGB(dest->format,0,0,0));

	const double xratio = static_cast<double>(surface->w)/
			              static_cast<double>(w);
	const double yratio = static_cast<double>(surface->h)/
			              static_cast<double>(h);

	double ysrc = 0.0;
	for(int ydst = 0; ydst != h; ++ydst, ysrc += yratio) {
		double xsrc = 0.0;
		for(int xdst = 0; xdst != w; ++xdst, xsrc += xratio) {
			const int xsrcint = static_cast<int>(xsrc);
			const int ysrcint = static_cast<int>(ysrc);

			SDL_Rect srcrect = { xsrcint, ysrcint, 1, 1 };
			SDL_Rect dstrect = { xdst, ydst, 1, 1 };
			SDL_BlitSurface(surface,&srcrect,dest,&dstrect);
		}
	}

	return dest;
}

void adjust_surface_colour(SDL_Surface* surface, int r, int g, int b)
{
	if(r == 0 && g == 0 && b == 0)
		return;

	const int xpad = is_odd(surface->w) ? 1:0;

	short* pixel = reinterpret_cast<short*>(surface->pixels);
	for(int y = 0; y != surface->h; ++y, pixel += xpad) {
		const short* const end = pixel + surface->w;
		while(pixel != end) {
			if(*pixel != 0 && *pixel != short(0xFFFF)) {
				Uint8 red, green, blue;
				SDL_GetRGB(*pixel,surface->format,&red,&green,&blue);

				red = maximum<int>(8,minimum<int>(255,int(red)+r));
				green = maximum<int>(0,minimum<int>(255,int(green)+g));
				blue  = maximum<int>(0,minimum<int>(255,int(blue)+b));

				*pixel = SDL_MapRGB(surface->format,red,green,blue);
			}

			++pixel;
		}
	}
}

SDL_Surface* get_surface_portion(SDL_Surface* src, SDL_Rect& area)
{
	if(area.x + area.w > src->w || area.y + area.h > src->h)
		return NULL;

	const SDL_PixelFormat* const fmt = src->format;
	SDL_Surface* const dst = SDL_CreateRGBSurface(0,area.w,area.h,
	                                              fmt->BitsPerPixel,fmt->Rmask,
	                                              fmt->Gmask,fmt->Bmask,
	                                              fmt->Amask);
	SDL_Rect dstarea = {0,0,0,0};

	SDL_BlitSurface(src,&area,dst,&dstarea);

	return dst;
}

namespace {

const unsigned short max_pixel = 0xFFFF;

struct not_alpha
{
	not_alpha(SDL_PixelFormat& format) : fmt_(format) {}

	bool operator()(unsigned short pixel) const {
		return pixel != 0;
	}

private:
	SDL_PixelFormat& fmt_;
};

}

SDL_Rect get_non_transperant_portion(const SDL_Surface* surf)
{
	const not_alpha calc(*(surf->format));
	const unsigned short* const pixels = reinterpret_cast<const unsigned short*>(surf->pixels);
	const size_t padding = is_odd(surf->w) ? 1:0;
	SDL_Rect res = {0,0,0,0};
	size_t n;
	for(n = 0; n != surf->h; ++n) {
		const unsigned short* const start_row = pixels + n*(surf->w+padding);
		const unsigned short* const end_row = start_row + surf->w;

		if(std::find_if(start_row,end_row,calc) != end_row)
			break;
	}

	res.y = n;

	for(n = 0; n != surf->h-res.y; ++n) {
		const unsigned short* const start_row = pixels + (surf->h-n-1)*(surf->w+padding);
		const unsigned short* const end_row = start_row + surf->w;

		if(std::find_if(start_row,end_row,calc) != end_row)
			break;
	}

	//the height is the height of the surface, minus the distance from the top and the
	//distance from the bottom
	res.h = surf->h - res.y - n;

	for(n = 0; n != surf->w; ++n) {
		size_t y;
		for(y = 0; y != surf->h; ++y) {
			const unsigned short pixel = pixels[y*(surf->w+padding) + n];
			if(calc(pixel))
				break;
		}

		if(y != surf->h)
			break;
	}

	res.x = n;

	for(n = 0; n != surf->w-res.x; ++n) {
		size_t y;
		for(y = 0; y != surf->h; ++y) {
			const unsigned short pixel = pixels[y*(surf->w+padding) + surf->w - n - 1];
			if(calc(pixel))
				break;
		}

		if(y != surf->h)
			break;
	}

	res.w = surf->w - res.x - n;

	return res;
}

bool operator==(const SDL_Rect& a, const SDL_Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool operator!=(const SDL_Rect& a, const SDL_Rect& b)
{
	return !operator==(a,b);
}

namespace {
	const SDL_Rect empty_rect = {0,0,0,0};
}

surface_restorer::surface_restorer() : target_(NULL), rect_(empty_rect), surface_(NULL)
{}

surface_restorer::surface_restorer(CVideo* target, const SDL_Rect& rect)
: target_(target), rect_(rect), surface_(NULL)
{
	update();
}

surface_restorer::~surface_restorer()
{
	restore();
}

void surface_restorer::restore()
{
	if(surface_ != NULL) {
		::SDL_BlitSurface(surface_,NULL,target_->getSurface(),&rect_);
		update_rect(rect_);
	}
}

void surface_restorer::update()
{
	if(rect_.w == 0 || rect_.h == 0)
		surface_.assign(NULL);
	else
		surface_.assign(::get_surface_portion(target_->getSurface(),rect_));
}

void surface_restorer::cancel()
{
	surface_.assign(NULL);
}