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

#include "map_fragment.hpp"

#include "../foreach.hpp"

#include <vector>

namespace editor2 {

map_fragment::map_fragment()
	: items_()
	, area_()
{
}

map_fragment::map_fragment(const gamemap& map, const std::set<gamemap::location>& area)
	: items_()
	, area_()
{
	add_tiles(map, area);
}

void map_fragment::add_tile(const gamemap& map, const gamemap::location& loc)
{
	if (area_.find(loc) == area_.end()) {
		items_.push_back(tile_info(map, loc));
		area_.insert(loc);
	}
}

void map_fragment::add_tiles(const gamemap& map, const std::set<gamemap::location>& locs)
{
	foreach (const gamemap::location& loc, locs) {
		add_tile(map, loc);
	}
}

std::set<gamemap::location> map_fragment::get_area() const
{
	return area_;
}

std::set<gamemap::location> map_fragment::get_offset_area(const gamemap::location& loc) const
{
	std::set<gamemap::location> result;
	foreach (const tile_info& i, items_) {
		result.insert(i.offset.vector_sum(loc));
	}
	return result;
}

void map_fragment::paste_into(gamemap& map, const gamemap::location& loc) const
{
	foreach (const tile_info& i, items_) {
		map.set_terrain(i.offset.vector_sum(loc), i.terrain);
	}
}

void map_fragment::shift(const gamemap::location& offset)
{
	foreach (tile_info& ti, items_) {
		ti.offset.vector_sum_assign(offset);
	}	
}

gamemap::location map_fragment::center_of_bounds() const
{
	if (empty()) return gamemap::location();
	gamemap::location top_left = items_[0].offset;
	gamemap::location bottom_right = items_[0].offset;
	for (size_t i = 1; i < items_.size(); ++i) {
		const gamemap::location& loc = items_[i].offset;
		if (loc.x < top_left.x) top_left.x = loc.x;
		else if (loc.x > bottom_right.x) bottom_right.x = loc.x;
		if (loc.y < top_left.y) top_left.y = loc.y;
		else if (loc.y > bottom_right.y) bottom_right.y = loc.y;
	}
	gamemap::location c((top_left.x + bottom_right.x) / 2, 
		(top_left.y + bottom_right.y) / 2);
	return c;
}

gamemap::location map_fragment::center_of_mass() const
{
	gamemap::location sum(0, 0);
	foreach (const tile_info& ti, items_) {
		sum.vector_sum_assign(ti.offset);
	}
	sum.x /= items_.size();
	sum.y /= items_.size();
	return sum;
}

void map_fragment::center_by_bounds()
{
	shift(center_of_bounds().vector_negation());
}

void map_fragment::center_by_mass()
{
	shift(center_of_mass().vector_negation());
}

void map_fragment::rotate_60_cw()
{
	area_.clear();
	foreach (tile_info& ti, items_) {
		gamemap::location l(0,0);
		int x = ti.offset.x;
		int y = ti.offset.y;
		// rotate the X-Y axes to SOUTH/SOUTH_EAST - SOUTH_WEST axes
		// but if x is odd, simply using x/2 + x/2 will lack a step
		l = l.get_direction(gamemap::location::SOUTH, (x+is_odd(x))/2);
		l = l.get_direction(gamemap::location::SOUTH_EAST, (x-is_odd(x))/2 );
		l = l.get_direction(gamemap::location::SOUTH_WEST, y);
		ti.offset = l;
		area_.insert(l);
	}
	if (get_area().size() != items_.size()) {
		throw editor_exception("Map fragment rotation resulted in duplicate entries");
	}
}

void map_fragment::rotate_60_ccw()
{
	area_.clear();
	foreach (tile_info& ti, items_) {
		gamemap::location l(0,0);
		int x = ti.offset.x;
		int y = ti.offset.y;
		// rotate the X-Y axes to NORTH/NORTH_EAST - SOUTH_EAST axes'
		// reverse of what the cw rotation does
		l = l.get_direction(gamemap::location::NORTH, (x-is_odd(x))/2);
		l = l.get_direction(gamemap::location::NORTH_EAST, (x+is_odd(x))/2 );
		l = l.get_direction(gamemap::location::SOUTH_EAST, y);
		ti.offset = l;
		area_.insert(l);
	}
	if (get_area().size() != items_.size()) {
		throw editor_exception("Map fragment rotation resulted in duplicate entries");
	}
}


bool map_fragment::empty() const
{
	return items_.empty();
}

} //end namespace editor2
