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
#include "game.hpp"
#include "map.hpp"
#include "pathfind.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

gamemap::location gamemap::location::null_location;

const std::string& gamemap::terrain_name(gamemap::TERRAIN terrain) const
{
	static const std::string default_val;
	const std::map<TERRAIN,terrain_type>::const_iterator i =
	                                      letterToTerrain_.find(terrain);
	if(i == letterToTerrain_.end())
		return default_val;
	else
		return i->second.name();
}

const std::string& gamemap::underlying_terrain_name(gamemap::TERRAIN terrain) const
{
	static const std::string default_val;
	const std::map<TERRAIN,terrain_type>::const_iterator i =
	                                       letterToTerrain_.find(terrain);
	if(i == letterToTerrain_.end()) {
		return default_val;
	} else {
		if(i->second.is_alias()) {
			//we could call underlying_terrain_name, but that could allow
			//infinite recursion with bad data files, so we call terrain_name
			//to be safe
			return terrain_name(i->second.type());
		} else {
			return i->second.name();
		}
	}
}

gamemap::TERRAIN gamemap::underlying_terrain(TERRAIN terrain) const
{
	const std::map<TERRAIN,terrain_type>::const_iterator i =
	                                       letterToTerrain_.find(terrain);
	if(i == letterToTerrain_.end()) {
		return terrain;
	} else {
		return i->second.type();
	}
}

gamemap::location::location(const config& cfg) : x(-1), y(-1)
{
	const std::string& xstr = cfg["x"];
	const std::string& ystr = cfg["y"];

	//the co-ordinates in config files will be 1-based, while we
	//want them as 0-based
	if(xstr.empty() == false)
		x = atoi(xstr.c_str()) - 1;

	if(ystr.empty() == false)
		y = atoi(ystr.c_str()) - 1;
}

void gamemap::location::write(config& cfg) const
{
	char buf[50];
	sprintf(buf,"%d",x+1);
	cfg["x"] = buf;
	sprintf(buf,"%d",y+1);
	cfg["y"] = buf;
}

bool gamemap::location::operator==(const gamemap::location& a) const
{
	return x == a.x && y == a.y;
}

bool gamemap::location::operator!=(const gamemap::location& a) const
{
	return !operator==(a);
}

bool gamemap::location::operator<(const gamemap::location& a) const
{
	return x < a.x || x == a.x && y < a.y;
}

gamemap::location gamemap::location::get_direction(
                                     gamemap::location::DIRECTION dir) const
{
	switch(dir) {
		case NORTH:      return gamemap::location(x,y-1);
		case NORTH_EAST: return gamemap::location(x+1,y-is_even(x));
		case SOUTH_EAST: return gamemap::location(x+1,y+is_odd(x));
		case SOUTH:      return gamemap::location(x,y+1);
		case SOUTH_WEST: return gamemap::location(x-1,y+is_odd(x));
		case NORTH_WEST: return gamemap::location(x-1,y+is_even(x));
		default:
			assert(false);
			return gamemap::location();
	}
}

gamemap::gamemap(const config& cfg, const std::string& data) : tiles_(1)
{
	std::cerr << "loading map: '" << data << "'\n";
	const config::child_list& terrains = cfg.get_children("terrain");
	create_terrain_maps(terrains,terrainPrecedence_,letterToTerrain_,terrain_);

	size_t x = 0, y = 0;
	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		char c = *i;
		if(c == '\r') {
			continue;
		} if(c == '\n') {
			x = 0;
			++y;
		} else {
			if(letterToTerrain_.count(c) == 0) {
				if(isdigit(*i)) {
					startingPositions_[c - '0'] = location(x,y);
					c = KEEP;
				} else {
					std::cerr << "Illegal character in map: (" << int(c) << ") '" << c << "'\n";
					throw incorrect_format_exception("Illegal character");
				}
			}

			if(underlying_terrain(c) == TOWER) {
				towers_.push_back(location(int(x),int(y)));
			}

			if(x >= tiles_.size()) {
				tiles_.resize(x+1);
			}

			tiles_[x].push_back(c);

			++x;
		}
	}

	if(tiles_.empty())
		throw incorrect_format_exception("empty map");

	for(size_t n = 0; n != tiles_.size(); ++n) {
		if(tiles_[n].size() != size_t(this->y())) {
			std::cerr << "Map is not rectangular!\n";
			tiles_.erase(tiles_.begin()+n);
			--n;
		}
	}

	std::cerr << "loaded map: " << this->x() << "," << this->y() << "\n";
}

std::string gamemap::write() const
{
	std::stringstream str;
	for(int j = 0; j != y(); ++j) {
		for(int i = 0; i != x(); ++i) {
			int n;
			for(n = 0; n != 10; ++n) {
				if(startingPositions_[n] == location(i,j))
					break;
			}

			if(n < 10)
				str << n;
			else
				str << tiles_[i][j];
		}

		str << "\n";
	}

	return str.str();
}

int gamemap::x() const { return tiles_.size(); }
int gamemap::y() const { return tiles_[0].size(); }

const std::vector<gamemap::TERRAIN>& gamemap::operator[](int index) const
{
	return tiles_[index];
}

gamemap::TERRAIN gamemap::get_terrain(const gamemap::location& loc) const
{
	if(on_board(loc))
		return tiles_[loc.x][loc.y];

	const std::map<location,TERRAIN>::const_iterator itor = borderCache_.find(loc);
	if(itor != borderCache_.end())
		return itor->second;

	//if not on the board, decide based on what surrounding terrain is
	TERRAIN items[6];
	int nitems = 0;
	
	location adj[6];
	get_adjacent_tiles(loc,adj);
	for(int n = 0; n != 6; ++n) {
		if(on_board(adj[n])) {
			items[nitems] = tiles_[adj[n].x][adj[n].y];
			++nitems;
		}
	}

	//count all the terrain types found, and see which one
	//is the most common, and use it.
	TERRAIN used_terrain = 0;
	int terrain_count = 0;
	for(int i = 0; i != nitems; ++i) {
		if(items[i] != used_terrain && underlying_terrain(items[i]) != TOWER) {
			const int c = std::count(items+i+1,items+nitems,items[i]) + 1;
			if(c > terrain_count) {
				used_terrain = items[i];
				terrain_count = c;
			}
		}
	}

	borderCache_.insert(std::pair<location,TERRAIN>(loc,used_terrain));

	return used_terrain;
}

const gamemap::location& gamemap::starting_position(int n) const
{
	return startingPositions_[n];
}

int gamemap::num_valid_starting_positions() const
{
	const int res = is_starting_position(gamemap::location());
	if(res == -1)
		return num_starting_positions();
	else
		return res;
}

int gamemap::num_starting_positions() const
{
	return sizeof(startingPositions_)/sizeof(*startingPositions_);
}

int gamemap::is_starting_position(const gamemap::location& loc) const
{
	const gamemap::location* const beg = startingPositions_+1;
	const gamemap::location* const end = startingPositions_+num_starting_positions();
	const gamemap::location* const pos = std::find(beg,end,loc);

	return pos == end ? -1 : pos - beg;
}

void gamemap::set_starting_position(int side, const gamemap::location& loc)
{
	if(side >= 0 && side < num_starting_positions()) {
		startingPositions_[side] = loc;
	}
}

const terrain_type& gamemap::get_terrain_info(TERRAIN terrain) const
{
	static const terrain_type default_terrain;
	const std::map<TERRAIN,terrain_type>::const_iterator i =
	                                letterToTerrain_.find(terrain);
	if(i != letterToTerrain_.end())
		return i->second;
	else
		return default_terrain;
}

const std::vector<gamemap::TERRAIN>& gamemap::get_terrain_precedence() const
{
	return terrainPrecedence_;
}

void gamemap::set_terrain(const gamemap::location& loc, gamemap::TERRAIN ter)
{
	if(!on_board(loc))
		return;

	tiles_[loc.x][loc.y] = ter;
}
