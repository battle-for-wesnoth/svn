/* $Id$ */
/*
   Copyright (C) 2006 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef TERRAIN_TRANSLATION_H_INCLUDED
#define TERRAIN_TRANSLATION_H_INCLUDED

#include <SDL_types.h> //used for Uint32 definition
#include <string>
#include <vector>
#include <map>

namespace t_translation {

	typedef Uint32 t_layer;
	const t_layer WILDCARD = 0x2A000000;
	const t_layer NO_LAYER = 0xFFFFFFFF;
	
	// The definitions for a terrain
	/**
	 * A terrain string which is converted to a terrain is a string with 1 or 2 layers
	 * the layers are separated by a caret and each group consists of 2 to 4 characters
	 * if no second layer is defined it is stored as 0xFFFFFFFF, if the second layer 
	 * is empty (needed for matching) the layer has the value 0.
	 */
	struct t_letter {
		t_letter(const std::string& b);
		t_letter(const std::string& b, const std::string& o);
		t_letter(const std::string& b, const t_layer o);
		t_letter(const t_layer& b, const t_layer& o) : base(b), overlay(o) {};
		t_letter() : base(0), overlay(NO_LAYER) {}

		t_layer base;
		t_layer overlay;
	};
	const t_letter NONE_TERRAIN = t_letter();

	inline bool operator<(const t_letter& a, const t_letter& b) 
		{ return a.base < b.base ||  (a.base == b.base && a.overlay < b.overlay); } 
	
	inline bool operator==(const t_letter& a, const t_letter& b) 
		{ return a.base == b.base && a.overlay == b.overlay; }
	
	inline bool operator!=(const t_letter& a, const t_letter& b) 
		{ return a.base != b.base || a.overlay != b.overlay; }

	inline t_letter operator&(const t_letter& a, const t_letter& b)
		{ return t_letter(a.base & b.base, a.overlay & b.overlay); }

	inline t_letter operator|(const t_letter& a, const t_letter& b)
		{ return t_letter(a.base | b.base, a.overlay | b.overlay); }

	// operator<< is defined later
	
	typedef std::vector<t_letter> t_list;
	typedef std::vector<std::vector<t_letter> > t_map;

	/**
	 * This structure can be used for matching terrain strings
	 * it optimized for strings that need to be matched often
	 * and caches the wildcard info required for matching
	 */
	struct t_match{
		t_match();
		t_match(const std::string& str, const t_layer filler = NO_LAYER);
		t_match(const t_letter& letter);

		t_list terrain;	
		t_list mask;
		t_list masked_terrain;
		bool has_wildcard;
		bool is_empty;
	};
	
	/** 
	 * Contains an x and c coordinate used for starting positions
	 * in maps
	 */
	struct coordinate {
		size_t x; 
		size_t y;
	};

    //exception thrown if there's an error with the terrain
	//Note atm most thrown result in a crash but I like an 
	//uncatched exception better than an assert
	struct error {
		error(const std::string& msg) : message(msg) {}
		std::string message;
	};

	//some types of terrain which must be known, and can't just be loaded
	//in dynamically because they're special. It's asserted that there will
	//be corresponding entries for these types of terrain in the terrain
	//configuration file.
	extern const t_letter VOID_TERRAIN;
	extern const t_letter FOGGED;

	extern const t_letter HUMAN_CASTLE;
	extern const t_letter HUMAN_KEEP;
	extern const t_letter SHALLOW_WATER;
	extern const t_letter DEEP_WATER;
	extern const t_letter GRASS_LAND;
	extern const t_letter FOREST;
	extern const t_letter MOUNTAIN;
	extern const t_letter HILL;

	extern const t_letter CAVE_WALL;
	extern const t_letter CAVE;
	extern const t_letter UNDERGROUND_VILLAGE;
	extern const t_letter DWARVEN_CASTLE;
	extern const t_letter DWARVEN_KEEP;

	extern const t_letter PLUS; 	// +
	extern const t_letter MINUS; 	// -
	extern const t_letter NOT;		// !
	extern const t_letter STAR; 	// *
		
	/** 
	 * Reads a single terrain from a string
	 * @param str		The string which should contain 1 letter the new format 
	 * 					of a letter is 2 to 4 characters in the set
	 * 					[a-Z][A-Z]/|\_ The underscore is intended for internal
	 * 					use. Other letters and characters are not validated but
	 * 					users of these letters can get nasty surprices. The * 
	 * 					is used as wildcard in some cases. This letter can
	 * 					be two groups separated by a caret, the first group
	 * 					is the base terrain, the second the overlay terrain.
	 * 
	 * @param filler	if there's no layer this value will be used as the second layer
	 *
	 * @return			A single terrain letter
	 */
	t_letter read_letter(const std::string& str, const t_layer filler = NO_LAYER);
	
	/** 
	 * Writes a single letter to a string.
	 * The writers only support the new format
	 *
	 * @param letter	The letter to convert to a string
	 *
	 * @return			A string containing the letter
	 */
	std::string write_letter(const t_letter& letter);
	inline std::ostream &operator<<(std::ostream &s, const t_letter &a)
		{ s << write_letter(a); return s; }
	
	/** 
	 * Reads a list of terrain from a string, when reading the 
	 * @param str		A string with one or more terrain letters (see read_letter)
	 * @param filler	if there's no layer this value will be used as the second layer
	 *
	 * @returns			A vector which contains the letters found in the string
	 */
	 t_list read_list(const std::string& str, const t_layer filler = NO_LAYER);

	/** 
	 * Writes a list of terrains to a string, only writes the new format.
	 *
	 * @param list		A vector with one or more terrain letters
	 *
	 * @returns			A string with the terrain numbers, comma separated and 
	 * 					a space behind the comma's. Not padded.
	 */
	std::string write_list(const t_list& list);

	/** 
	 * Reads a gamemap string into a 2D vector
	 *
	 * @param str		A string containing the gamemap, the following rules 
	 * 					are stated for a gamemap:
	 * 					* The map is square
	 * 					* The map can be prefixed with one or more empty lines,
	 * 					  these lines are ignored
	 * 					* The map can be postfixed with one or more empty lines,
	 * 					  these lines are ignored
	 * 					* Every end of line can be followed by one or more empty
	 * 					  lines, these lines are ignored. NOTE it's deprecated
	 * 					  to use this feature.
	 * 					* Terrain strings are separated by comma's or an end of line
	 * 					  symbol, for the last terrain string in the row. For 
	 * 					  readability it's allowed to pad strings with either spaces
	 * 					  or tab, however the tab is deprecated.
	 * 					* A terrain string contains either a terrain or a terrain and
	 * 					  starting loction. The following format is used
	 * 					  [S ]T
	 * 					  S = starting location a positive non-zero number
	 * 					  T = terrain letter (see read_letter)
	 * @param starting_positions This parameter will be filled with the starting
	 * 					locations found. Starting locations can only occur once 
	 * 					if multiple definitions occur of the same position only
	 * 					the last is stored. The returned value is a map:
	 * 					* first		the starting locations
	 * 					* second	a coordinate structure where the location was found
	 *
	 * @returns			A 2D vector with the terrains found the vector data is stored
	 * 					like result[x][y] where x the column number is and y the row number.
	 */
	t_map read_game_map(const std::string& str, std::map<int, coordinate>& starting_positions);

	/** 
	 * Write a gamemap in to a vector string
	 *
	 * @param map		A terrain vector, as returned from read_game_map
	 * @param starting_positions A starting positions map, as returned from read_game_map
	 *
	 * @returns			A terrain string which can be read with read_game_map.
	 * 					For readability the map is padded to groups of 7 chars
	 * 					followed by a comma and space
	 */
	std::string write_game_map(const t_map& map, std::map<int, coordinate> starting_positions = std::map<int, coordinate>());

	/** 
	 * Tests whether a certain terrain matches a list of terrains the terrains can 
	 * use wildcard matching with *. It also has an inversion function. When a ! 
	 * is found the result of the match is inverted. The matching stops at the 
	 * first match (regardless of the ! found) the data is match from start to end.
	 *
	 * Example: 
	 * W*, Ww 		does match and returns true
	 * W*, {!, Ww}	does match and returns false (due to the !)
	 * Ww, WW		doesn't match and return false
	 *
	 * Multilayer rules:
	 * If a terrain has multiple layers a wildcard on the first layer also
	 * matches the following layer unless a caret is used, in this case 
	 * there is no need to put anything behind the caret
	 *
	 * Example:
	 * A*       matches Abcd but also Abcd^Abcd
	 * A*^*     matches Abcd but also Abcd^Abcd
	 * A*^      matches Abcd but *not* Abcd^Abcd
	 * A*^Abcd  does not match Abcd but matches Abcd^Abcd
	 *
	 * @param src	the value to match (may also contain the wildcard)
	 * @param dest	the list of values to match against
	 *
	 * @returns		the result of the match (depending on the !'s)
	 */
	bool terrain_matches(const t_letter& src, const t_list& dest);

	/** 
	 * Tests whether a certain terrain matches another terrain, for matching 
	 * rules see above.
	 *
	 * @param src	the value to match (may also contain the wildcard)
	 * @param dest 	the value to match against
	 *
	 * @returns		the result of the match (depending on the !'s)
	 */
	bool terrain_matches(const t_letter& src, const t_letter& dest);
	
	/** 
	 * Tests whether a certain terrain matches another terrain, for matching 
	 * rules see above. The matching requires some bit mask which impose a
	 * certain overhead. This version uses a cache to cache the masks so if
	 * a list needs to be matched often this version is preferred.
	 *
	 * @param src	the value to match (may also contain the wildcard)
	 * @param dest 	the value to match against
	 *
	 * @returns	the result of the match (depending on the !'s)
	 */
	bool terrain_matches(const t_letter& src, const t_match& dest);

	/** 
	 * Tests wither a terrain contains a wildcard
	 *
	 *  @param		the letter to test for a wildcard
	 *
	 *  @returns	true if wildcard found else false
	 */
	bool has_wildcard(const t_letter& letter);

	/** 
	 * Tests wither a terrain list contains at least
	 * one item with a wildcard
	 *
	 *  @param		the list to test for a wildcard
	 *
	 *  @returns	true if a wildcard found else false
	 */
	bool has_wildcard(const t_list& list);

	// these terrain letters are in the builder format, and 
	// not usable in other parts of the engine
	const t_layer TB_STAR = '*' << 24; //it can be assumed this is the equivalent of STAR
	const t_layer TB_DOT = '.' << 24;
	
	/** 
	 * Reads a builder map, a builder map differs much from a normal map hence
	 * the different functions
	 *
	 * @param str		The map data, a terrain letter is either a * or a . or a number as
	 * 					anchor. The star or dot are stored in the base part of the terrain
	 * 					and the anchor in the overlay part. If more letters are allowed as
	 * 					special case they will be stored in the base part. Anchor 0 is no 
	 * 					anchor
	 *
	 * @returns			A 2D vector with the data found the vector data is stored
	 * 					like result[y][x] where x the column number is and y the row number.
	 */
	t_map read_builder_map(const std::string& str); 

}
#endif
