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
#ifndef INTRO_HPP_INCLUDED
#define INTRO_HPP_INCLUDED

#include "SDL.h"
#include "config.hpp"
#include "display.hpp"

#include <string>

//function to show an introduction sequence specified by data.
//the format of data is like,
//[part]
//id='id'
//story='story'
//image='img'
//[/part]
//where 'id' is a unique identifier, 'story' is text describing
//storyline, and 'img' is an image.
//
//each part of the sequence will be displayed in turn, with the
//user able to go to the next part, or skip it entirely.
void show_intro(display& screen, config& data);

//function to show the map before each scenario.
//data is in a format that looks like,
//[bigmap]
//image='map-image'
//	[dot]
//	x='x'
//	y='y'
//	type=cross (optional)
//	[/dot]
//  ... more 'dot' nodes'
//[/bigmap]
//
//where 'map-image' is the image of the map. dots are displayed
//at 'x','y' on the image in sequence. type=cross should be used
//for the last dot, to show where the battle takes place.
void show_map_scene(display& screen, config& data);

#endif
