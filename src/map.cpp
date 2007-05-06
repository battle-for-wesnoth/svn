/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "actions.hpp"
#include "config.hpp"
#include "gamestatus.hpp"
#include "log.hpp"
#include "map.hpp"
#include "pathfind.hpp"
#include "util.hpp"
#include "variable.hpp"
#include "wassert.hpp"
#include "game_events.hpp"
#include "serialization/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

#define ERR_CF LOG_STREAM(err, config)
#define LOG_G LOG_STREAM(info, general)

std::ostream &operator<<(std::ostream &s, gamemap::location const &l) {
	s << (l.x + 1) << ',' << (l.y + 1);
	return s;
}

gamemap::location gamemap::location::null_location;

const t_translation::t_list& gamemap::underlying_mvt_terrain(t_translation::t_letter terrain) const
{
	const std::map<t_translation::t_letter,terrain_type>::const_iterator i = 
		letterToTerrain_.find(terrain);

	if(i == letterToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.mvt_type();
	}
}

const t_translation::t_list& gamemap::underlying_def_terrain(t_translation::t_letter terrain) const
{
	const std::map<t_translation::t_letter, terrain_type>::const_iterator i = 
		letterToTerrain_.find(terrain);

	if(i == letterToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.def_type();
	} 
}

const t_translation::t_list& gamemap::underlying_union_terrain(t_translation::t_letter terrain) const
{
	const std::map<t_translation::t_letter,terrain_type>::const_iterator i = 
		letterToTerrain_.find(terrain);
	
	if(i == letterToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.union_type();
	}
}

void gamemap::write_terrain(const gamemap::location &loc, config& cfg) const
{
	cfg["terrain"] = t_translation::write_letter(get_terrain(loc));
}

gamemap::location::DIRECTION gamemap::location::parse_direction(const std::string& str)
{
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
	} else {
		return NDIRECTIONS;
	}
}

std::string gamemap::location::write_direction(gamemap::location::DIRECTION dir)
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

void gamemap::location::init(const std::string &xstr, const std::string &ystr)
{
	std::string xs = xstr, ys = ystr;
	if (game_events::get_state_of_game())
	{
		xs = utils::interpolate_variables_into_string( xs, *game_events::get_state_of_game());
		ys = utils::interpolate_variables_into_string( ys, *game_events::get_state_of_game());
	}
	//the co-ordinates in config files will be 1-based, while we
	//want them as 0-based
	if(xs.empty() == false)
		x = atoi(xs.c_str()) - 1;

	if(ys.empty() == false)
		y = atoi(ys.c_str()) - 1;
}

gamemap::location::location(const config& cfg) : x(-1), y(-1)
{
	init(cfg["x"], cfg["y"]);
}

gamemap::location::location(const vconfig& cfg) : x(-1), y(-1)
{
	init(cfg["x"], cfg["y"]);
}

void gamemap::location::write(config& cfg) const
{
	char buf[50];
	snprintf(buf,sizeof(buf),"%d",x+1);
	cfg["x"] = buf;
	snprintf(buf,sizeof(buf),"%d",y+1);
	cfg["y"] = buf;
}

gamemap::location gamemap::location::operator-() const
{
	location ret;
	ret.x = -x;
	ret.y = -y;

	return ret;
}

gamemap::location gamemap::location::operator+(const gamemap::location& a) const
{
	gamemap::location ret = *this;
	ret += a;
	return ret;
}

gamemap::location& gamemap::location::operator+=(const gamemap::location &a)
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

gamemap::location gamemap::location::operator-(const gamemap::location &a) const
{
	return operator+(-a);
}

gamemap::location& gamemap::location::operator-=(const gamemap::location &a)
{
	return operator+=(-a);
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
		case NORTH_WEST: return gamemap::location(x-1,y-is_even(x));
		default:
			wassert(false);
			return gamemap::location();
	}
}

gamemap::location::DIRECTION gamemap::location::get_relative_dir(gamemap::location loc) const {
	location diff = loc -*this;
	if(diff == location(0,0)) return NDIRECTIONS;
	if( diff.y < 0 && diff.x >= 0 && abs(diff.x) >= abs(diff.y)) return NORTH_EAST;
	if( diff.y < 0 && diff.x < 0 && abs(diff.x) >= abs(diff.y)) return NORTH_WEST;
	if( diff.y < 0 && abs(diff.x) < abs(diff.y)) return NORTH;

	if( diff.y >= 0 && diff.x >= 0 && abs(diff.x) >= abs(diff.y)) return SOUTH_EAST;
	if( diff.y >= 0 && diff.x < 0 && abs(diff.x) >= abs(diff.y)) return SOUTH_WEST;
	if( diff.y >= 0 && abs(diff.x) < abs(diff.y)) return SOUTH;

	//impossible
	wassert(false);
	return NDIRECTIONS;


}
gamemap::location::DIRECTION gamemap::location::get_opposite_dir(gamemap::location::DIRECTION d) const {
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

gamemap::gamemap(const config& cfg, const std::string& data) : tiles_(1), x_(-1), y_(-1)
{
	LOG_G << "loading map: '" << data << "'\n";
	const config::child_list& terrains = cfg.get_children("terrain");
	create_terrain_maps(terrains,terrainList_,letterToTerrain_);

	read(data);
}

void gamemap::read(const std::string& data)
{
	tiles_.clear();
	villages_.clear();
	std::fill(startingPositions_,startingPositions_+sizeof(startingPositions_)/sizeof(*startingPositions_),location());
	std::map<int, t_translation::coordinate> starting_positions;

	try {
		tiles_ = t_translation::read_game_map(data, starting_positions);
	} catch(t_translation::error& e) {
		// we re-throw the error but as map error, since all codepaths test 
		// for this, it's the least work
		throw incorrect_format_exception(e.message.c_str());
	}

	//convert the starting positions to the array
	std::map<int, t_translation::coordinate>::const_iterator itor = 
		starting_positions.begin();

	for(; itor != starting_positions.end(); ++itor) {

		// check for valid position, the first valid position is
		// 1 so the offset 0 in the array is never used
		if(itor->first < 1 || itor->first >= STARTING_POSITIONS) { 
			ERR_CF << "Starting position " << itor->first << " out of range\n"; 
			throw incorrect_format_exception("Illegal starting position found in map. The scenario cannot be loaded.");
		}

		// add to the starting position array
		startingPositions_[itor->first] = location(itor->second.x, itor->second.y);
	}
	
	// post processing on the map
	const int width = tiles_.size();
	const int height = tiles_[0].size();
    x_ = width;
    y_ = height;
	for(int x = 0; x < width; ++x) {
		for(int y = 0; y < height; ++y) {
			
			// is the terrain valid? 
			if(letterToTerrain_.count(tiles_[x][y]) == 0) {
				ERR_CF << "Illegal character in map: (" << t_translation::write_letter(tiles_[x][y]) 
					<< ") '" << tiles_[x][y] << "'\n"; 
				throw incorrect_format_exception("Illegal character found in map. The scenario cannot be loaded.");
			} 
			
			// is it a village
			if(is_village(tiles_[x][y])) {
				villages_.push_back(location(x, y));
			}
		}
	}
}

std::string gamemap::write() const
{
	std::map<int, t_translation::coordinate> starting_positions = std::map<int, t_translation::coordinate>();

	// convert the starting positions to a map
	for(int i = 0; i < STARTING_POSITIONS; ++i) {
    	if(on_board(startingPositions_[i])) {
			const struct t_translation::coordinate position = 
				{startingPositions_[i].x, startingPositions_[i].y};
			
			 starting_positions.insert(std::pair<int, t_translation::coordinate>(i, position));
		}
	}
	
	// let the low level convertor do the conversion
	return t_translation::write_game_map(tiles_, starting_positions);
}

void gamemap::overlay(const gamemap& m, const config& rules_cfg, const int xpos, const int ypos)
{
	const config::child_list& rules = rules_cfg.get_children("rule");

	const int xstart = maximum<int>(0, -xpos);
	const int ystart = maximum<int>(0, -ypos-((xpos & 1) ? 1 : 0));
	const int xend = minimum<int>(m.x(),x()-xpos);
	const int yend = minimum<int>(m.y(),y()-ypos);
	for(int x1 = xstart; x1 < xend; ++x1) {
		for(int y1 = ystart; y1 < yend; ++y1) {
			const int x2 = x1 + xpos;
			const int y2 = y1 + ypos +
				((xpos & 1) && (x1 & 1) ? 1 : 0);
			if (y2 < 0 || y2 >= y()) {
				continue;
			}
			const t_translation::t_letter t = m[x1][y1];
			const t_translation::t_letter current = (*this)[x2][y2];

			if(t == t_translation::FOGGED || t == t_translation::VOID_TERRAIN) {
				continue;
			}

			//see if there is a matching rule
			config::child_list::const_iterator rule = rules.begin();
			for( ; rule != rules.end(); ++rule) {
				static const std::string src_key = "old", src_not_key = "old_not",
				                         dst_key = "new", dst_not_key = "new_not";
				const config& cfg = **rule;
				const t_translation::t_list& src = t_translation::read_list(cfg[src_key]);

				if(!src.empty() && t_translation::terrain_matches(current, src) == false) {
					continue;
				}

				const t_translation::t_list& src_not = t_translation::read_list(cfg[src_not_key]);

				if(!src_not.empty() && t_translation::terrain_matches(current, src_not)) {
					continue;
				}

				const t_translation::t_list& dst = t_translation::read_list(cfg[dst_key]);

				if(!dst.empty() && t_translation::terrain_matches(current, dst) == false) {
					continue;
				}

				const t_translation::t_list& dst_not = t_translation::read_list(cfg[dst_not_key]);

				if(!dst_not.empty() && t_translation::terrain_matches(current, dst_not)) {
					continue;
				}

				break;
			}


			if(rule != rules.end()) {
				const config& cfg = **rule;
				const t_translation::t_list& terrain = t_translation::read_list(cfg["terrain"]);

				if(!terrain.empty()) {
					set_terrain(location(x2,y2),terrain[0]);
				} else if(cfg["use_old"] != "yes") {
					set_terrain(location(x2,y2),t);
				}
			} else {
				set_terrain(location(x2,y2),t);
			}
		}
	}

	for(const location* pos = m.startingPositions_; 
			pos != m.startingPositions_ + sizeof(m.startingPositions_)/sizeof(*m.startingPositions_); 
			++pos) {

		if(pos->valid()) {
			startingPositions_[pos - m.startingPositions_] = *pos;
		}
	}
}

t_translation::t_letter gamemap::get_terrain(const gamemap::location& loc) const
{
	if(on_board(loc))
		return tiles_[loc.x][loc.y];

	const std::map<location, t_translation::t_letter>::const_iterator itor = borderCache_.find(loc);
	if(itor != borderCache_.end())
		return itor->second;

	//if not on the board, decide based on what surrounding terrain is
	t_translation::t_letter items[6];
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
	t_translation::t_letter used_terrain;
	int terrain_count = 0;
	for(int i = 0; i != nitems; ++i) {
		if(items[i] != used_terrain && !is_village(items[i]) && !is_keep(items[i])) {
			const int c = std::count(items+i+1,items+nitems,items[i]) + 1;
			if(c > terrain_count) {
				used_terrain = items[i];
				terrain_count = c;
			}
		}
	}

	borderCache_.insert(std::pair<location, t_translation::t_letter>(loc,used_terrain));
	return used_terrain;
}

const gamemap::location& gamemap::starting_position(int n) const
{
	if(size_t(n) < sizeof(startingPositions_)/sizeof(*startingPositions_)) {
		return startingPositions_[n];
	} else {
		static const gamemap::location null_loc;
		return null_loc;
	}
}

int gamemap::num_valid_starting_positions() const
{
	const int res = is_starting_position(gamemap::location());
	if(res == -1)
		return num_starting_positions()-1;
	else
		return res;
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

const terrain_type& gamemap::get_terrain_info(const t_translation::t_letter terrain) const
{
	static const terrain_type default_terrain;
	const std::map<t_translation::t_letter,terrain_type>::const_iterator i =
		letterToTerrain_.find(terrain);

	if(i != letterToTerrain_.end())
		return i->second;
	else
		return default_terrain;
}

bool gamemap::location::matches_range(const std::string& xloc, const std::string &yloc) const
{
	if(std::find(xloc.begin(),xloc.end(),',') != xloc.end()) {
		std::vector<std::string> xlocs = utils::split(xloc);
		std::vector<std::string> ylocs = utils::split(yloc);

		const int size = xlocs.size() < ylocs.size()?xlocs.size():ylocs.size();
		for(int i = 0; i != size; ++i) {
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

namespace {

	struct terrain_cache_manager {
		terrain_cache_manager() : ptr(NULL) {}
		~terrain_cache_manager() { delete ptr; }
		t_translation::t_match *ptr;
	};

	struct cfg_isor {
		bool operator() (std::pair<const std::string*,const config*> val) {
			return *(val.first) == "or";
		}
	};

} //end anonymous namespace

bool gamemap::terrain_matches_filter(const gamemap::location& loc, const vconfig& cfg, 
		const gamestatus& game_status, const unit_map& units, const bool flat_tod,
		const size_t max_loop) const
{
	//handle radius
	const size_t radius = minimum<size_t>(max_loop,
		lexical_cast_default<size_t>(cfg["radius"], 0));
	std::set<gamemap::location> hexes;
	std::vector<gamemap::location> loc_vec(1, loc);
	get_tiles_radius(*this, loc_vec, radius, hexes);

	size_t loop_count = 0;
	bool matches = false;
	std::set<gamemap::location>::const_iterator i;
	terrain_cache_manager tcm;
	for(i = hexes.begin(); i != hexes.end() && loop_count <= max_loop && !matches; ++i) {
		matches = terrain_matches_internal(*i, cfg, game_status, units, flat_tod, false, tcm.ptr);
		++loop_count;
	}

	//handle [and], [or], and [not] with in-order precedence
	config::all_children_iterator cond = cfg.get_config().ordered_begin();
	config::all_children_iterator cond_end = cfg.get_config().ordered_end();
	int ors_left = std::count_if(cond, cond_end, cfg_isor());
	while(cond != cond_end)
	{
		//if there are no matches or [or] conditions left, go ahead and return false
		if(!matches && ors_left <= 0) {
			return false;
		}

		const std::string& cond_name = *((*cond).first);
		const vconfig cond_filter(&(*((*cond).second)));

		//handle [and]
		if(cond_name == "and")
		{
			matches = matches &&
				terrain_matches_filter(loc, cond_filter, game_status, units, flat_tod, max_loop);
		}
		//handle [or]
		else if(cond_name == "or")
		{
			matches = matches ||
				terrain_matches_filter(loc, cond_filter, game_status, units, flat_tod, max_loop);
			--ors_left;
		}
		//handle [not]
		else if(cond_name == "not")
		{
			matches = matches &&
				!terrain_matches_filter(loc, cond_filter, game_status, units, flat_tod, max_loop);
		}

		++cond;
	}

	return matches;
}

bool gamemap::terrain_matches_internal(const gamemap::location& loc, const vconfig& cfg, 
		const gamestatus& game_status, const unit_map& units, const bool flat_tod, 
		const bool ignore_xy, t_translation::t_match*& parsed_terrain) const
{

	const int terrain_format = lexical_cast_default(cfg["terrain_format"], -1);
	if(terrain_format != -1) {
		lg::wml_error << "key terrain_format in filter_location is no longer used, this message will disappear in 1.3.5\n";
	}

	if(cfg.has_attribute("terrain")) {
		if(parsed_terrain == NULL) {
			parsed_terrain = new t_translation::t_match(cfg["terrain"]);
		}
		if(!parsed_terrain->is_empty) {
			const t_translation::t_letter letter = get_terrain_info(loc).number();
			if(!t_translation::terrain_matches(letter, *parsed_terrain)) {
				return false;
			}
		}
	}
	
	//Allow filtering on location ranges 
	if(!ignore_xy && !(cfg["x"].empty() && cfg["y"].empty())){
		if(!loc.matches_range(cfg["x"], cfg["y"])) {
			return false;
		}
	}

	//Allow filtering on unit
	if(cfg.has_child("filter")) {
		const vconfig& unit_filter = cfg.child("filter");
		const unit_map::const_iterator u = units.find(loc);
		if (u == units.end() || !u->second.matches_filter(unit_filter, loc, flat_tod))
			return false;
	}

	const t_string& t_tod_type = cfg["time_of_day"];
	const t_string& t_tod_id = cfg["time_of_day_id"];
	const std::string& tod_type = t_tod_type;
	const std::string& tod_id = t_tod_id;
	static config const dummy_cfg;
	time_of_day tod(dummy_cfg);
	if(!tod_type.empty() || !tod_id.empty()) {
		if(flat_tod) {
			tod = game_status.get_time_of_day(0,loc);
		} else {
			tod = timeofday_at(game_status,units,loc,*this);
		}
	}
	if(!tod_type.empty()) {
		const std::vector<std::string>& vals = utils::split(tod_type);
		if(tod.lawful_bonus<0) {
			if(std::find(vals.begin(),vals.end(),"chaotic") == vals.end()) {
				return false;
			}
		} else if(tod.lawful_bonus>0) {
			if(std::find(vals.begin(),vals.end(),"lawful") == vals.end()) {
				return false;
			}
		} else {
			if(std::find(vals.begin(),vals.end(),"neutral") == vals.end()) {
				return false;
			}
		}
	}
	if(!tod_id.empty()) {
		if(tod_id != tod.id) {
			if(std::find(tod_id.begin(),tod_id.end(),',') != tod_id.end() &&
				std::search(tod_id.begin(),tod_id.end(),
				tod.id.begin(),tod.id.end()) != tod_id.end()) {
				const std::vector<std::string>& vals = utils::split(tod_id);
				if(std::find(vals.begin(),vals.end(),tod.id) == vals.end()) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	//allow filtering on owner (for villages)
	const t_string& t_owner_side = cfg["owner_side"];
	const std::string& owner_side = t_owner_side;
	if(!owner_side.empty()) {
		const int side_index = lexical_cast_default<int>(owner_side,0) - 1;
		wassert(game_status.teams != NULL);
		if(village_owner(loc, *(game_status.teams)) != side_index) {
			return false;
		}
	}
	return true; 
}

void gamemap::set_terrain(const gamemap::location& loc, const t_translation::t_letter terrain)
{
	if(!on_board(loc))
		return;

	const bool old_village = is_village(loc);
	const bool new_village = is_village(terrain);

	if(old_village && !new_village) {
		villages_.erase(std::remove(villages_.begin(),villages_.end(),loc),villages_.end());
	} else if(!old_village && new_village) {
		villages_.push_back(loc);
	}

	tiles_[loc.x][loc.y] = terrain;

	location adj[6];
	get_adjacent_tiles(loc,adj);

	for(int n = 0; n < 6; ++n)
		remove_from_border_cache(adj[n]);
}

std::vector<gamemap::location> parse_location_range(const std::string& x, const std::string& y,
													const gamemap *const map)
{
	std::vector<gamemap::location> res;
	const std::vector<std::string> xvals = utils::split(x);
	const std::vector<std::string> yvals = utils::split(y);

	for(unsigned int i = 0; i < xvals.size() || i < yvals.size(); ++i) {
		std::pair<int,int> xrange, yrange;

		//x
		if(i < xvals.size()) {
			xrange = utils::parse_range(xvals[i]);
		} else if (map != NULL) {
			xrange.first = 1;
			xrange.second = map->x();
		} else {
			break;
		}

		//y
		if(i < yvals.size()) {
			yrange = utils::parse_range(yvals[i]);
		} else if (map != NULL) {
			yrange.first = 1;
			yrange.second = map->y();
		} else {
			break;
		}

		for(int x = xrange.first; x <= xrange.second; ++x) {
			for(int y = yrange.first; y <= yrange.second; ++y) {
				res.push_back(gamemap::location(x-1,y-1));
			}
		}
	}
	return res;
}

const std::map<t_translation::t_letter, size_t>& gamemap::get_weighted_terrain_frequencies() const
{
	if(terrainFrequencyCache_.empty() == false) {
		return terrainFrequencyCache_;
	}

	const location center(x()/2,y()/2);

	const size_t furthest_distance = distance_between(location(0,0),center);

	const size_t weight_at_edge = 100;
	const size_t additional_weight_at_center = 200;

	for(size_t i = 0; i != size_t(x()); ++i) {
		for(size_t j = 0; j != size_t(y()); ++j) {
			const size_t distance = distance_between(location(i,j),center);
			terrainFrequencyCache_[(*this)[i][j]] += weight_at_edge + (furthest_distance-distance)*additional_weight_at_center;
		}
	}

	return terrainFrequencyCache_;
}

void gamemap::get_locations(std::set<gamemap::location>& locs, const vconfig& filter,
		const gamestatus& game_status, const unit_map& units, const bool flat_tod,
		const size_t max_loop) const
{
	std::vector<gamemap::location> xy_locs = parse_location_range(filter["x"],filter["y"],this);
	if(xy_locs.empty()) {
		//consider all locations on the map
		for(int x=0; x < x_; x++) {
			for(int y=0; y < y_; y++) {
				xy_locs.push_back(location(x,y));
			}
		}
	}

	//handle location filter
	terrain_cache_manager tcm;
	std::vector<gamemap::location>::iterator loc_itor = xy_locs.begin();
	while(loc_itor != xy_locs.end()) {
		if(terrain_matches_internal(*loc_itor, filter, game_status, units, flat_tod, true, tcm.ptr)) {
			++loc_itor;
		} else {
			loc_itor = xy_locs.erase(loc_itor);
		}
	}

	//handle radius
	const size_t radius = minimum<size_t>(max_loop,
		lexical_cast_default<size_t>(filter["radius"], 0));
	get_tiles_radius(*this, xy_locs, radius, locs);

	//handle [and], [or], and [not] with in-order precedence
	config::all_children_iterator cond = filter.get_config().ordered_begin();
	config::all_children_iterator cond_end = filter.get_config().ordered_end();
	int ors_left = std::count_if(cond, cond_end, cfg_isor());
	while(cond != cond_end)
	{
		//if there are no locations or [or] conditions left, go ahead and return empty
		if(locs.empty() && ors_left <= 0) {
			return;
		}

		const std::string& cond_name = *((*cond).first);
		const vconfig cond_filter(&(*((*cond).second)));

		//handle [and]
		if(cond_name == "and") {
			std::set<gamemap::location> intersect_hexes;
			get_locations(intersect_hexes, cond_filter, game_status, units, flat_tod, max_loop);
			std::set<gamemap::location>::iterator intersect_itor = locs.begin();
			while(intersect_itor != locs.end()) {
				if(intersect_hexes.find(*intersect_itor) == locs.end()) {
					locs.erase(*intersect_itor++);
				} else {
					++intersect_itor;
				}
			}
		}
		//handle [or]
		else if(cond_name == "or") {
			std::set<gamemap::location> union_hexes;
			get_locations(union_hexes, cond_filter, game_status, units, flat_tod, max_loop);
			//locs.insert(union_hexes.begin(), union_hexes.end()); //doesn't compile on MSVC
			std::set<gamemap::location>::iterator insert_itor = union_hexes.begin();
			while(insert_itor != union_hexes.end()) {
				locs.insert(*insert_itor++);
			}
			--ors_left;
		}
		//handle [not]
		else if(cond_name == "not") {
			std::set<gamemap::location> removal_hexes;
			get_locations(removal_hexes, cond_filter, game_status, units, flat_tod, max_loop);
			std::set<gamemap::location>::iterator erase_itor = removal_hexes.begin();
			while(erase_itor != removal_hexes.end()) {
				locs.erase(*erase_itor++);
			}
		}

		++cond;
	}

	//restrict the potential number of locations to be returned
	if(locs.size() > max_loop + 1) {
		std::set<gamemap::location>::iterator erase_itor = locs.begin();
		for(unsigned i=0; i < max_loop + 1; ++i) {
			++erase_itor;
		}
		locs.erase(erase_itor, locs.end());
	}
}
