/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MAP_LABEL_HPP_INCLUDED
#define MAP_LABEL_HPP_INCLUDED

class config;
#include "map.hpp"
#include "font.hpp"


#include <map>
#include <string>

class display;
class team;
class terrain_label;
class replay;
class game_state;



class map_labels
{
public:
	typedef std::map<gamemap::location,const terrain_label*> label_map;
	typedef std::map<std::string,label_map> team_label_map;
	
	map_labels(const display& disp, const gamemap& map, const team*);
	map_labels(const display& disp, const config& cfg, const gamemap& map, const team*);
	~map_labels();

	void write(config& res) const;
	void read(const config& cfg);

	static size_t get_max_chars();

	const terrain_label* get_label(const gamemap::location& loc);
	const terrain_label* set_label(const gamemap::location& loc, 
							   const std::string& text,
							   replay* = 0,
							   const std::string team = "",
							   const SDL_Color colour = font::NORMAL_COLOUR);
	
	void add_label(const gamemap::location&,
				   const terrain_label*);
	
	void clear(const std::string&, replay*);

	void scroll(double xmove, double ymove);

	void recalculate_labels();
	bool visible_global_label(const gamemap::location&) const;

	void recalculate_shroud();

	
	const label_map& labels();
	
	const display& disp() const;
	
	const std::string& team_name() const;
	
	void set_team(const team*);

private:
	void clear_map(const label_map&);
	void clear_all();
	map_labels(const map_labels&);
	void operator=(const map_labels&);

	const display& disp_;
	const team* team_;
	const gamemap& map_;

	team_label_map labels_;
	label_map label_cache_;
	bool changed_;
};

/// To store label data
/// Class implements logic for rendering
class terrain_label
{
public:
	terrain_label(const std::string&,
				  const std::string&, 
				  const gamemap::location&, 
				  const map_labels&, 
				  const SDL_Color colour = font::NORMAL_COLOUR);
	
	terrain_label(const map_labels&, 
				  const config&);
	
	terrain_label(const map_labels&);
	
	~terrain_label();
	
	void write(config& res) const;
	void read(const config& cfg);
	
	const std::string& text() const;
	const std::string& team_name() const;
	const gamemap::location& location() const;
	const SDL_Colour& colour() const;
	
	void set_text(const std::string&);
				
	void update_info(const std::string&,
					 const std::string&,
 					 const SDL_Color);
	
	void scroll(double xmove, double ymove) const;

	void recalculate();
	void calculate_shroud() const;
	void invalidate_handle();

private:
	terrain_label(const terrain_label&);
	const terrain_label& operator=(const terrain_label&);
	void clear();
	void draw();
	bool visible() const;
	void check_text_length();
	const std::string cfg_colour() const;
	
	int handle_;
	
	std::string text_;
	std::string team_name_;
	SDL_Color	colour_;
	
	const map_labels* parent_;
	gamemap::location loc_;
	
};

#endif
