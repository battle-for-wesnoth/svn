/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MAP_H_INCLUDED
#define MAP_H_INCLUDED

class config;
class gamestatus;
class unit;
class vconfig;
class unit_map;

#include "terrain.hpp"

#include "serialization/string_utils.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <set>

#define MAX_MAP_AREA	65536

//class which encapsulates the map of the game. Although the game is hexagonal,
//the map is stored as a grid. Each type of terrain is represented by a letter.
class gamemap
{
public:

	//the name of the terrain is the terrain itself, the underlying terrain
	//is the name of the terrain for game-logic purposes. I.e. if the terrain
	//is simply an alias, the underlying terrain name is the name of the
	//terrain that it's aliased to
	const t_translation::t_list& underlying_mvt_terrain(t_translation::t_letter terrain) const;
	const t_translation::t_list& underlying_def_terrain(t_translation::t_letter terrain) const;
	const t_translation::t_list& underlying_union_terrain(t_translation::t_letter terrain) const;

	//exception thrown if the map file is not in the correct format.
	struct incorrect_format_exception {
		incorrect_format_exception(const char* msg) : msg_(msg) {}
		const char* const msg_;
	};

	//structure which represents a location on the map.
	struct location {
		//any valid direction which can be moved in in our hexagonal world.
		enum DIRECTION { NORTH, NORTH_EAST, SOUTH_EAST, SOUTH,
		                 SOUTH_WEST, NORTH_WEST, NDIRECTIONS };

		static DIRECTION parse_direction(const std::string& str);
		static std::string write_direction(DIRECTION dir);

		location() : x(-1), y(-1) {}
		location(int x, int y) : x(x), y(y) {}
		location(const config& cfg, const variable_set *variables);

		void write(config& cfg) const;

		inline bool valid() const { return x >= 0 && y >= 0; }

		inline bool valid(const int parWidth, const int parHeight) const
		{
			return ((x >= 0) && (y >= 0) && (x < parWidth) && (y < parHeight));
		}

		int x, y;
		bool matches_range(const std::string& xloc, const std::string& yloc) const;

		// Inlining those for performance reasons
		bool operator<(const location& a) const { return x < a.x || x == a.x && y < a.y; }
		bool operator==(const location& a) const { return x == a.x && y == a.y; }
		bool operator!=(const location& a) const { return !operator==(a); }

		// Adds an absolute location to a "delta" location
		location operator-() const;
		location operator+(const location &a) const;
		location &operator+=(const location &a);
		location operator-(const location &a) const;
		location &operator-=(const location &a);

		location get_direction(DIRECTION d) const;
		DIRECTION get_relative_dir(location loc) const;
		DIRECTION get_opposite_dir(DIRECTION d) const;

		static location null_location;
	};
	const t_translation::t_list& underlying_mvt_terrain(const location& loc) const
		{ return underlying_mvt_terrain(get_terrain(loc)); }
	const t_translation::t_list& underlying_def_terrain(const location& loc) const
		{ return underlying_def_terrain(get_terrain(loc)); }
	const t_translation::t_list& underlying_union_terrain(const location& loc) const
		{ return underlying_union_terrain(get_terrain(loc)); }

	bool is_village(t_translation::t_letter terrain) const
		{ return get_terrain_info(terrain).is_village(); }
	int gives_healing(t_translation::t_letter terrain) const
		{ return get_terrain_info(terrain).gives_healing(); }
	bool is_castle(t_translation::t_letter terrain) const
		{ return get_terrain_info(terrain).is_castle(); }
	bool is_keep(t_translation::t_letter terrain) const
		{ return get_terrain_info(terrain).is_keep(); }

	bool is_village(const location& loc) const
		{ return on_board(loc) && is_village(get_terrain(loc)); }
	int gives_healing(const location& loc) const
		{ return on_board(loc) ?  gives_healing(get_terrain(loc)) : 0; }
	bool is_castle(const location& loc) const
		{ return on_board(loc) && is_castle(get_terrain(loc)); }
	bool is_keep(const location& loc) const
		{ return on_board(loc) && is_keep(get_terrain(loc)); }

	//loads a map, with the given terrain configuration.
	//data should be a series of lines, with each character representing
	//one hex on the map. Starting locations are represented by numbers,
	//and will be of type keep.
	gamemap(const config& terrain_cfg, const std::string& data); //throw(incorrect_format_exception)
	void read(const std::string& data);

	std::string write() const;

	//overlays another map onto this one at the given position.
	void overlay(const gamemap& m, const config& rules, int x=0, int y=0);

	//dimensions of the map.
	int w() const { return w_; }
	int h() const { return h_; }

	//allows lookup of terrain at a particular location.
	const t_translation::t_list& operator[](int index) const 
		{ return tiles_[index]; }

	//looks up terrain at a particular location. Hexes off the map
	//may be looked up, and their 'emulated' terrain will also be returned.
	//this allows proper drawing of the edges of the map
	t_translation::t_letter get_terrain(const location& loc) const;

	//writes the terrain at loc to cfg
	void write_terrain(const gamemap::location &loc, config& cfg) const;


	//functions to manipulate starting positions of the different sides.
	const location& starting_position(int side) const;
	int is_starting_position(const location& loc) const;
	int num_valid_starting_positions() const;

	void set_starting_position(int side, const location& loc);

	//function which, given a location, will tell if that location is
	//on the map. Should be called before indexing using []
	bool on_board(const location& loc) const
	{
		return loc.valid() && loc.x < w() && loc.y < h();
	}

	//function to tell if the map is of 0 size.
	bool empty() const
	{
		return w() == 0 || h() == 0;
	}

	//function to return a list of the locations of villages on the map
	const std::vector<location>& villages() const { return villages_; }

	//function to get the corresponding terrain_type information object
	//for a given type of terrain
	const terrain_type& get_terrain_info(const t_translation::t_letter terrain) const;

	//shortcut to get_terrain_info(get_terrain(loc))
	const terrain_type& get_terrain_info(const location &loc) const
		{ return get_terrain_info(get_terrain(loc)); }

	//gets the list of terrains
	const t_translation::t_list& get_terrain_list() const
		{ return terrainList_; }

	//clobbers over the terrain at location 'loc', with the given terrain
	void set_terrain(const location& loc, const t_translation::t_letter terrain);

	//function which returns a list of the frequencies of different terrain
	//types on the map, with terrain nearer the center getting weighted higher
	const std::map<t_translation::t_letter, size_t>& get_weighted_terrain_frequencies() const;
	//remove the cached border terrain at loc. Needed by the editor
	//to make tiles at the border update correctly when drawing
	//other tiles.
	void remove_from_border_cache(const location &loc)
		{ borderCache_.erase(loc); }

	//the size of the starting positions array
	//the positions themselves are numbered from
	//1. Thus max_starting positions is
	//STARTING_POSITIONS - 1
	enum { STARTING_POSITIONS = 10 };

protected:
	t_translation::t_map tiles_;
	location startingPositions_[STARTING_POSITIONS];
	
	/**
	 * Clears the border cache, needed for the editor
	 */
	void clear_border_cache() { borderCache_.clear(); }
private:
	int num_starting_positions() const
		{ return sizeof(startingPositions_)/sizeof(*startingPositions_); }

	t_translation::t_list terrainList_;
	std::map<t_translation::t_letter, terrain_type> letterToTerrain_;
	std::vector<location> villages_;

	mutable std::map<location, t_translation::t_letter> borderCache_;
	mutable std::map<t_translation::t_letter, size_t> terrainFrequencyCache_;

	int w_;
	int h_;
};

class viewpoint
{
public:
	virtual bool shrouded(int x, int y) const = 0;
	virtual bool fogged(int x, int y) const = 0;
	virtual ~viewpoint() {};
};

//a utility function which parses ranges of locations
//into a vector of locations
std::vector<gamemap::location> parse_location_range(const std::string& xvals,
	const std::string& yvals, const gamemap *const map=NULL);

//dump a position on a stream for debug purposes
std::ostream &operator<<(std::ostream &s, gamemap::location const &l);

#endif
