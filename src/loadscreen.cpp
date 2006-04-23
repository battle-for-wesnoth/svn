/*
   Copyright (C) 2005 by Joeri Melis <joeri_melis@hotmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "loadscreen.hpp"

#include "font.hpp"
#include "marked-up_text.hpp"

//#include <iostream>

#define MIN_PERCENTAGE   0
#define MAX_PERCENTAGE 100

void loadscreen::set_progress(const int percentage, const std::string &text, const bool commit)
{
	/* Saturate percentage. */
	prcnt_ = percentage < MIN_PERCENTAGE ? MIN_PERCENTAGE: percentage > MAX_PERCENTAGE ? MAX_PERCENTAGE: percentage;	
	/* Set progress bar parameters. */
	int fcr =   103, fcg =   11, fcb = 10; /* Finished piece. */
	int lcr =   24, lcg =   22, lcb =  21; /* Leftover piece. */
	int bcr = 188, bcg = 176, bcb = 136; /* Border color. */
	int bw = 1; /* Border width. */
	int bispw = 1; /* Border inner spacing width. */
	bw = 2*(bw+bispw) > screen_.getx() ? 0: 2*(bw+bispw) > screen_.gety() ? 0: bw;
	int scrx = screen_.getx() - 2*(bw+bispw); /* Available width. */
	int scry = screen_.gety() - 2*(bw+bispw); /* Available height. */
	int pbw = scrx/2; /* Used width. */
	int pbh = scry/16; /* Used heigth. */
	int pbx = (scrx - pbw)/2; /* Horizontal location. */
	int pby = (scry - pbh)/2; /* Vertical location. */
	surface const gdis = screen_.getSurface();
	SDL_Rect area;
	/* Draw top border. */
	area.x = pbx; area.y = pby;
	area.w = pbw + 2*(bw+bispw); area.h = bw;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,bcr,bcg,bcb));
	/* Draw bottom border. */
	area.x = pbx; area.y = pby + pbh + bw + 2*bispw;
	area.w = pbw + 2*(bw+bispw); area.h = bw;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,bcr,bcg,bcb));
	/* Draw left border. */
	area.x = pbx; area.y = pby + bw;
	area.w = bw; area.h = pbh + 2*bispw;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,bcr,bcg,bcb));
	/* Draw right border. */
	area.x = pbx + pbw + bw + 2*bispw; area.y = pby + bw;
	area.w = bw; area.h = pbh + 2*bispw;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,bcr,bcg,bcb));
	/* Draw the finished bar area. */
	area.x = pbx + bw + bispw; area.y = pby + bw + bispw;
	area.w = (prcnt_ * pbw) / (MAX_PERCENTAGE - MIN_PERCENTAGE); area.h = pbh;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,fcr,fcg,fcb));
	/* Draw the leftover bar area. */
	area.x = pbx + bw + bispw + (prcnt_ * pbw) / (MAX_PERCENTAGE - MIN_PERCENTAGE); area.y = pby + bw + bispw;
	area.w = ((MAX_PERCENTAGE - prcnt_) * pbw) / (MAX_PERCENTAGE - MIN_PERCENTAGE); area.h = pbh;
	SDL_FillRect(gdis,&area,SDL_MapRGB(gdis->format,lcr,lcg,lcb));
	/* Clear the last text and draw new if text is provided. */
	if(text.length()>0)
	{
		SDL_FillRect(gdis,&textarea_,SDL_MapRGB(gdis->format,0,0,0));
		textarea_ = font::line_size(text, font::SIZE_NORMAL);
		textarea_.x = scrx/2 + bw + bispw - textarea_.w / 2;
		textarea_.y = pby + pbh + 4*(bw + bispw);
		textarea_ = font::draw_text(&screen_,textarea_,font::SIZE_NORMAL,font::NORMAL_COLOUR,text,textarea_.x,textarea_.y);
	}
 	/* Flip the double buffering so the change becomes visible */
	if(commit)
	{
		SDL_Flip(gdis);
	}
}

void loadscreen::increment_progress(const int percentage, const std::string &text, const bool commit) {
	set_progress(prcnt_ + percentage, text, commit);
}

void loadscreen::clear_screen(const bool commit)
{
	int scrx = screen_.getx(); /* Screen width. */
	int scry = screen_.gety(); /* Screen height. */
	SDL_Rect area = {0, 0, scrx, scry}; /* Screen area. */
	surface const disp(screen_.getSurface()); /* Screen surface. */
	/* Make everything black. */
	SDL_FillRect(disp,&area,SDL_MapRGB(disp->format,0,0,0));
	if(commit)
	{
		SDL_Flip(disp); /* Flip the double buffering. */
	}
}

loadscreen *loadscreen::global_loadscreen = 0;

#define CALLS_TO_FILESYSTEM 112
#define PRCNT_BY_FILESYSTEM  20
#define CALLS_TO_BINARYWML 9561
#define PRCNT_BY_BINARYWML   20
#define CALLS_TO_SETCONFIG  306
#define PRCNT_BY_SETCONFIG   30
#define CALLS_TO_PARSER   50448
#define PRCNT_BY_PARSER      20

void increment_filesystem_progress () {
	unsigned newpct, oldpct;
	// Only do something if the variable is filled in.
	// I am assuming non parallel access here!
	if (loadscreen::global_loadscreen != 0) {
		if (loadscreen::global_loadscreen->filesystem_counter == 0) {
			loadscreen::global_loadscreen->increment_progress(0, "Verifying cache.");
		}
		oldpct = (PRCNT_BY_FILESYSTEM * loadscreen::global_loadscreen->filesystem_counter) / CALLS_TO_FILESYSTEM;
		newpct = (PRCNT_BY_FILESYSTEM * ++(loadscreen::global_loadscreen->filesystem_counter)) / CALLS_TO_FILESYSTEM;
		//std::cerr << "Calls " << num;
		if(oldpct != newpct) {
			//std::cerr << " percent " << newpct;
			loadscreen::global_loadscreen->increment_progress(newpct - oldpct);
		}
		//std::cerr << std::endl;
	}
}

void increment_binary_wml_progress () {
	unsigned newpct, oldpct;
	// Only do something if the variable is filled in.
	// I am assuming non parallel access here!
	if (loadscreen::global_loadscreen != 0) {
		if (loadscreen::global_loadscreen->binarywml_counter == 0) {
			loadscreen::global_loadscreen->increment_progress(0, "Reading cache.");
		}
		oldpct = (PRCNT_BY_BINARYWML * loadscreen::global_loadscreen->binarywml_counter) / CALLS_TO_BINARYWML;
		newpct = (PRCNT_BY_BINARYWML * ++(loadscreen::global_loadscreen->binarywml_counter)) / CALLS_TO_BINARYWML;
		//std::cerr << "Calls " << num;
		if(oldpct != newpct) {
			//std::cerr << " percent " << newpct;
			loadscreen::global_loadscreen->increment_progress(newpct - oldpct);
		}
		//std::cerr << std::endl;
	}
}

void increment_set_config_progress () {
	unsigned newpct, oldpct;
	// Only do something if the variable is filled in.
	// I am assuming non parallel access here!
	if (loadscreen::global_loadscreen != 0) {
		if (loadscreen::global_loadscreen->setconfig_counter == 0) {
			loadscreen::global_loadscreen->increment_progress(0, "Reading unit files.");
		}
		oldpct = (PRCNT_BY_SETCONFIG * loadscreen::global_loadscreen->setconfig_counter) / CALLS_TO_SETCONFIG;
		newpct = (PRCNT_BY_SETCONFIG * ++(loadscreen::global_loadscreen->setconfig_counter)) / CALLS_TO_SETCONFIG;
		//std::cerr << "Calls " << num;
		if(oldpct != newpct) {
			//std::cerr << " percent " << newpct;
			loadscreen::global_loadscreen->increment_progress(newpct - oldpct);
		}
		//std::cerr << std::endl;
	}
}

void increment_parser_progress () {
	unsigned newpct, oldpct;
	// Only do something if the variable is filled in.
	// I am assuming non parallel access here!
	if (loadscreen::global_loadscreen != 0) {
		if (loadscreen::global_loadscreen->parser_counter == 0) {
			loadscreen::global_loadscreen->increment_progress(0, "Reading files and creating cache.");
		}
		oldpct = (PRCNT_BY_PARSER * loadscreen::global_loadscreen->parser_counter) / CALLS_TO_PARSER;
		newpct = (PRCNT_BY_PARSER * ++(loadscreen::global_loadscreen->parser_counter)) / CALLS_TO_PARSER;
		//std::cerr << "Calls " << loadscreen::global_loadscreen->parser_counter;
		if(oldpct != newpct) {
		//	std::cerr << " percent " << newpct;
			loadscreen::global_loadscreen->increment_progress(newpct - oldpct);
		}
		//std::cerr << std::endl;
	}
}
