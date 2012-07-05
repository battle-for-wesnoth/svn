/* $Id$ */
/*
   Copyright (C) 2003 - 2012 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various pathfinding functions and utilities.
 */

#include "global.hpp"

#include "pathutils.hpp"

#include "map.hpp"


/**
 * Function which will add to @a result all locations in a ring of distance
 * @a radius from @a center. @a result must be a std::vector of locations.
 */
void get_tile_ring(const map_location& center, const int radius,
                   std::vector<map_location>& result)
{
	if ( radius <= 0 ) {
		return;
	}

	map_location loc = center.get_direction(map_location::SOUTH_WEST, radius);

	for(int n = 0; n != 6; ++n) {
		const map_location::DIRECTION dir = static_cast<map_location::DIRECTION>(n);
		for(int i = 0; i != radius; ++i) {
			result.push_back(loc);
			loc = loc.get_direction(dir, 1);
		}
	}
}


/**
 * Function which will add to @a result all locations within a distance of
 * @a radius from @a center (excluding @a center itself). @a result must be
 * a std::vector of locations.
 */
void get_tiles_in_radius(const map_location& center, const int radius,
                         std::vector<map_location>& result)
{
	for(int n = 0; n <= radius; ++n) {
		get_tile_ring(center, n, result);
	}
}


static void get_tiles_radius_internal(const map_location& a, size_t radius,
	std::set<map_location>& res, std::map<map_location,int>& visited)
{
	visited[a] = radius;
	res.insert(a);

	if(radius == 0) {
		return;
	}

	map_location adj[6];
	get_adjacent_tiles(a,adj);
	for(size_t i = 0; i != 6; ++i) {
		if(visited.count(adj[i]) == 0 || visited[adj[i]] < int(radius)-1) {
			get_tiles_radius_internal(adj[i],radius-1,res,visited);
		}
	}
}

/**
 * Function which will add to @a result all locations within a distance of
 * @a radius from @a center (including @a center itself). @a result must be
 * a std::set of locations.
 */
void get_tiles_radius(const map_location& center, size_t radius,
                      std::set<map_location>& result)
{
	std::map<map_location,int> visited;
	get_tiles_radius_internal(center, radius, result, visited);
}

/**
 * Function which will add to @a result all elements of @a locs, plus all
 * on-board locations matching @a pred that are connected to elements of
 * locs by a chain of at most @a radius tiles, each of which matches @a pred.
 * @a result must be a std::set of locations.
 */
void get_tiles_radius(gamemap const &map, std::vector<map_location> const &locs,
                      size_t radius, std::set<map_location> &result,
                      bool with_border, xy_pred const *pred)
{
	typedef std::set<map_location> location_set;
	location_set not_visited(locs.begin(), locs.end()), must_visit, filtered_out;
	++radius;

	for(;;) {
		location_set::const_iterator it = not_visited.begin(), it_end = not_visited.end();
		std::copy(it,it_end,std::inserter(result,result.end()));
		for(; it != it_end; ++it) {
			map_location adj[6];
			get_adjacent_tiles(*it, adj);
			for(size_t i = 0; i != 6; ++i) {
				map_location const &loc = adj[i];
				if ( with_border ? map.on_board_with_border(loc) :
				                   map.on_board(loc) ) {
					if ( !result.count(loc) && !filtered_out.count(loc) ) {
						if ( !pred || (*pred)(loc) )
							must_visit.insert(loc);
						else
							filtered_out.insert(loc);
					}
				}
			}
		}

		if(--radius == 0 || must_visit.empty()) {
			break;
		}

		not_visited.swap(must_visit);
		must_visit.clear();
	}
}

