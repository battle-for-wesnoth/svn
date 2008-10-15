/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file map.cpp
 * Routines related to game-maps, terrain, locations, directions. etc.
 */

#include "global.hpp"

#include "config.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_location.hpp"
#include "pathfind.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/parser.hpp"
#include "wml_exception.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

#define ERR_CF LOG_STREAM(err, config)
#define LOG_G LOG_STREAM(info, general)
#define DBG_G LOG_STREAM(debug, general)

std::ostream &operator<<(std::ostream &s, map_location const &l) {
	s << (l.x + 1) << ',' << (l.y + 1);
	return s;
}

const map_location map_location::null_location;

map_location::DIRECTION map_location::parse_direction(const std::string& str)
{
	if(!str.empty()) {
		if(str == "n") {
			return NORTH;
		} else if(str == "ne") {
			return NORTH_EAST;
		} else if(str == "se") {
			return SOUTH_EAST;
		} else if(str == "s") {
			return SOUTH;
		} else if(str == "sw") {
			return SOUTH_WEST;
		} else if(str == "nw") {
			return NORTH_WEST;
		} else if(str[0] == '-' && str.length() <= 10) {
			// A minus sign reverses the direction
			return get_opposite_dir(parse_direction(str.substr(1)));
		}
	}
	return NDIRECTIONS;
}

std::vector<map_location::DIRECTION> map_location::parse_directions(const std::string& str)
{
	map_location::DIRECTION temp;
	std::vector<map_location::DIRECTION> to_return;
	std::vector<std::string> dir_strs = utils::split(str);
	std::vector<std::string>::const_iterator i, i_end=dir_strs.end();
	for(i = dir_strs.begin(); i != i_end; ++i) {
		temp = map_location::parse_direction(*i);
		// Filter out any invalid directions
		if(temp != NDIRECTIONS) {
			to_return.push_back(temp);
		}
	}
	return to_return;
}

std::string map_location::write_direction(map_location::DIRECTION dir)
{
	switch(dir) {
		case NORTH:
			return std::string("n");
		case NORTH_EAST:
			return std::string("ne");
		case NORTH_WEST:
			return std::string("nw");
		case SOUTH:
			return std::string("s");
		case SOUTH_EAST:
			return std::string("se");
		case SOUTH_WEST:
			return std::string("sw");
		default:
			return std::string();

	}
}

map_location::map_location(const config& cfg, const variable_set *variables) :
		x(-1000),
		y(-1000)
{
	std::string xs = cfg["x"], ys = cfg["y"];
	if (variables)
	{
		xs = utils::interpolate_variables_into_string( xs, *variables);
		ys = utils::interpolate_variables_into_string( ys, *variables);
	}
	// The co-ordinates in config files will be 1-based,
	// while we want them as 0-based.
	if(xs.empty() == false && xs != "recall")
		x = atoi(xs.c_str()) - 1;

	if(ys.empty() == false && ys != "recall")
		y = atoi(ys.c_str()) - 1;
}

void map_location::write(config& cfg) const
{
	char buf[50];
	snprintf(buf,sizeof(buf),"%d",x+1);
	cfg["x"] = buf;
	snprintf(buf,sizeof(buf),"%d",y+1);
	cfg["y"] = buf;
}

map_location map_location::legacy_negation() const
{
	return map_location(-x, -y);
}

map_location map_location::legacy_sum(const map_location& a) const
{
	return map_location(*this).legacy_sum_assign(a);
}

map_location& map_location::legacy_sum_assign(const map_location &a)
{
	bool parity = (x & 1) != 0;
	x += a.x;
	y += a.y;
	if((a.x > 0) && (a.x % 2) && parity)
		y++;
	if((a.x < 0) && (a.x % 2) && !parity)
		y--;

	return *this;
}

map_location map_location::legacy_difference(const map_location &a) const
{
	return legacy_sum(a.legacy_negation());
}

map_location& map_location::legacy_difference_assign(const map_location &a)
{
	return legacy_sum_assign(a.legacy_negation());
}

map_location map_location::vector_negation() const
{
	return map_location(-x, -y - (x & 1)); //subtract one if we're on an odd x coordinate
}

map_location map_location::vector_sum(const map_location& a) const
{
	return map_location(*this).vector_sum_assign(a);
}

map_location& map_location::vector_sum_assign(const map_location &a)
{
	y += (x & 1) * (a.x & 1); //add one if both x coords are odd
	x += a.x;
	y += a.y;
	return *this;
}

map_location map_location::vector_difference(const map_location &a) const
{
	return vector_sum(a.vector_negation());
}

map_location& map_location::vector_difference_assign(const map_location &a)
{
	return vector_sum_assign(a.vector_negation());
}

map_location map_location::get_direction(
			map_location::DIRECTION dir, int n) const
{
	if (n < 0 ) {
		dir = get_opposite_dir(dir);
		n = -n;
	}
	switch(dir) {
		case NORTH:      return map_location(x, y - n);
		case SOUTH:      return map_location(x, y + n);
		case SOUTH_EAST: return map_location(x + n, y + (n+is_odd(x))/2 );
		case SOUTH_WEST: return map_location(x - n, y + (n+is_odd(x))/2 );
		case NORTH_EAST: return map_location(x + n, y - (n+is_even(x))/2 );
		case NORTH_WEST: return map_location(x - n, y - (n+is_even(x))/2 );
		default:
			assert(false);
			return map_location();
	}
}

map_location::DIRECTION map_location::get_relative_dir(map_location loc) const {
	map_location diff = loc.legacy_difference(*this);
	if(diff == map_location(0,0)) return NDIRECTIONS;
	if( diff.y < 0 && diff.x >= 0 && abs(diff.x) >= abs(diff.y)) return NORTH_EAST;
	if( diff.y < 0 && diff.x <  0 && abs(diff.x) >= abs(diff.y)) return NORTH_WEST;
	if( diff.y < 0 && abs(diff.x) < abs(diff.y)) return NORTH;

	if( diff.y >= 0 && diff.x >= 0 && abs(diff.x) >= abs(diff.y)) return SOUTH_EAST;
	if( diff.y >= 0 && diff.x <  0 && abs(diff.x) >= abs(diff.y)) return SOUTH_WEST;
	if( diff.y >= 0 && abs(diff.x) < abs(diff.y)) return SOUTH;

	// Impossible
	assert(false);
	return NDIRECTIONS;


}
map_location::DIRECTION map_location::get_opposite_dir(map_location::DIRECTION d) {
	switch (d) {
		case NORTH:
			return SOUTH;
		case NORTH_EAST:
			return SOUTH_WEST;
		case SOUTH_EAST:
			return NORTH_WEST;
		case SOUTH:
			return NORTH;
		case SOUTH_WEST:
			return NORTH_EAST;
		case NORTH_WEST:
			return SOUTH_EAST;
		case NDIRECTIONS:
		default:
			return NDIRECTIONS;
	}
}

bool map_location::matches_range(const std::string& xloc, const std::string &yloc) const
{
	if(std::find(xloc.begin(),xloc.end(),',') != xloc.end()
	|| std::find(yloc.begin(),yloc.end(),',') != yloc.end()) {
		std::vector<std::string> xlocs = utils::split(xloc);
		std::vector<std::string> ylocs = utils::split(yloc);

		size_t size;
		for(size = xlocs.size(); size < ylocs.size(); ++size) {
			xlocs.push_back("");
		}
		while(size > ylocs.size()) {
			ylocs.push_back("");
		}
		for(size_t i = 0; i != size; ++i) {
			if(matches_range(xlocs[i],ylocs[i]))
				return true;
		}
		return false;
	}
	if(!xloc.empty()) {
		const std::string::const_iterator dash =
		             std::find(xloc.begin(),xloc.end(),'-');
		if(dash != xloc.end()) {
			const std::string beg(xloc.begin(),dash);
			const std::string end(dash+1,xloc.end());

			const int bot = atoi(beg.c_str()) - 1;
			const int top = atoi(end.c_str()) - 1;

			if(x < bot || x > top)
				return false;
		} else {
			const int xval = atoi(xloc.c_str()) - 1;
			if(xval != x)
				return false;
		}
	}
	if(!yloc.empty()) {
		const std::string::const_iterator dash =
		             std::find(yloc.begin(),yloc.end(),'-');

		if(dash != yloc.end()) {
			const std::string beg(yloc.begin(),dash);
			const std::string end(dash+1,yloc.end());

			const int bot = atoi(beg.c_str()) - 1;
			const int top = atoi(end.c_str()) - 1;

			if(y < bot || y > top)
				return false;
		} else {
			const int yval = atoi(yloc.c_str()) - 1;
			if(yval != y)
				return false;
		}
	}
	return true;
}

std::vector<map_location> parse_location_range(const std::string& x, const std::string& y,
													const gamemap *const map)
{
	std::vector<map_location> res;
	const std::vector<std::string> xvals = utils::split(x);
	const std::vector<std::string> yvals = utils::split(y);

	for(unsigned int i = 0; i < xvals.size() || i < yvals.size(); ++i) {
		std::pair<int,int> xrange, yrange;

		// x
		if(i < xvals.size()) {
			xrange = utils::parse_range(xvals[i]);
		} else if (map != NULL) {
			xrange.first = 1;
			xrange.second = map->w();
		} else {
			break;
		}

		// y
		if(i < yvals.size()) {
			yrange = utils::parse_range(yvals[i]);
		} else if (map != NULL) {
			yrange.first = 1;
			yrange.second = map->h();
		} else {
			break;
		}

		for(int x = xrange.first; x <= xrange.second; ++x) {
			for(int y = yrange.first; y <= yrange.second; ++y) {
				res.push_back(map_location(x-1,y-1));
			}
		}
	}
	return res;
}
