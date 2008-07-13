/* $Id$ */
/*
   Copyright (C) 2008 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "brush.hpp"
#include "editor_map.hpp"

#include "../foreach.hpp"
#include "../pathutils.hpp"

#include <vector>

namespace editor2 {

brush::brush()
{
}

brush::brush(const config& cfg)
{
	int radius = lexical_cast_default<int>(cfg["radius"], 0);
	if (radius > 0) {
		std::vector<gamemap::location> in_radius;
		get_tiles_in_radius(gamemap::location(0, 0), radius, in_radius);
		foreach (gamemap::location& loc, in_radius) {
			add_relative_location(loc.x, loc.y);
		}
	}
	config::const_child_itors cfg_range = cfg.child_range("relative");
	while (cfg_range.first != cfg_range.second) {
		const config& relative = **cfg_range.first;
		int x = lexical_cast_default<int>(relative["x"], 0);
		int y = lexical_cast_default<int>(relative["y"], 0);
		add_relative_location(x, y);
		++cfg_range.first;
	}
}

void brush::add_relative_location(int relative_x, int relative_y)
{
	relative_tiles_.insert(gamemap::location(relative_x, relative_y));
}

std::set<gamemap::location> brush::project(const gamemap::location& hotspot) const
{
	std::set<gamemap::location> result;
	foreach (const gamemap::location& relative, relative_tiles_) {
		result.insert(hotspot + relative);
	}
	return result;
}


} //end namespace editor2
