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
#ifndef VIDEO_HPP_INCLUDED
#define VIDEO_HPP_INCLUDED

#include "SDL.h"

//possible flags when setting video modes
#define FULL_SCREEN SDL_FULLSCREEN
#define VIDEO_MEMORY SDL_HWSURFACE
#define SYSTEM_MEMORY SDL_SWSURFACE

SDL_Surface* display_format_alpha(SDL_Surface* surf);
SDL_Surface* get_video_surface();

bool non_interactive();


void update_rect(size_t x, size_t y, size_t w, size_t h);
void update_rect(const SDL_Rect& rect);
void update_whole_screen();

class CVideo {
     public:
	CVideo();
	CVideo(int x, int y, int bits_per_pixel, int flags);
	~CVideo();

	int modePossible( int x, int y, int bits_per_pixel, int flags );
	int setMode( int x, int y, int bits_per_pixel, int flags );

	int setGamma(float gamma);

	//functions to get the dimensions of the current video-mode
	int getx() const;
	int gety() const;
	int getBitsPerPixel();
	int getBytesPerPixel();
	int getRedMask();
	int getGreenMask();
	int getBlueMask();

	//functions to access the screen
	void lock();
	void unlock();
	int mustLock();

	void flip();

	SDL_Surface* getSurface( void );

	bool isFullScreen() const;

	struct error {};

	struct quit {};

	//functions to allow changing video modes when 16BPP is emulated
	void setBpp( int bpp );
	int getBpp();

	void make_fake();

private:

	int bpp;	// Store real bits per pixel

	//if there is no display at all, but we 'fake' it for clients
	bool fake_screen;
};

//a structure which will detect if the resolution or fullscreen mode has changed
struct video_change_detector {
	video_change_detector(CVideo& video) : video_(video), full_(video.isFullScreen()), x_(video.getx()), y_(video.gety())
	{}

	bool changed() const { return full_ != video_.isFullScreen() || x_ != video_.getx() || y_ != video_.gety(); }

private:
	CVideo& video_;
	bool full_;
	int x_, y_;
};

#endif
