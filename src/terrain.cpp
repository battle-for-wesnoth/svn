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

#include "terrain.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

terrain_type::terrain_type() : images_(1,"void"), type_(' '), letter_(' '),
                               equal_precedence_(false)
{}

terrain_type::terrain_type(const config& cfg)
{
	images_ = config::split(cfg["image"]);
	name_ = cfg["name"];
	const std::string& letter = cfg["char"];
	assert(!letter.empty());
	letter_ = letter[0];

	const std::string& alias = cfg["aliasof"];
	if(alias.empty())
		type_ = letter_;
	else
		type_ = alias[0];

	colour_.read(cfg);

	equal_precedence_ = cfg["no_overlay"] == "true";
}

const std::string& terrain_type::image(int x, int y) const
{
	assert(!images_.empty());

	return images_[(((x<<8)^3413402)+y^34984 + x*y)%images_.size()];
}

const std::string& terrain_type::default_image() const
{
	assert(!images_.empty());
	return images_.front();
}

const std::string& terrain_type::name() const
{
	return name_;
}

char terrain_type::letter() const
{
	return letter_;
}

char terrain_type::type() const
{
	return type_;
}

pixel_data terrain_type::get_rgb() const
{
	return colour_;
}

bool terrain_type::is_alias() const
{
	return type_ != letter_;
}

bool terrain_type::equal_precedence() const
{
	return equal_precedence_;
}

void create_terrain_maps(const std::vector<config*>& cfgs,
                         std::vector<char>& terrain_precedence,
                         std::map<char,terrain_type>& letter_to_terrain,
                         std::map<std::string,terrain_type>& str_to_terrain)
{
	for(std::vector<config*>::const_iterator i = cfgs.begin();
	    i != cfgs.end(); ++i) {
		terrain_type terrain(**i);
		terrain_precedence.push_back(terrain.letter());
		letter_to_terrain.insert(std::pair<char,terrain_type>(
		                              terrain.letter(),terrain));
		str_to_terrain.insert(std::pair<std::string,terrain_type>(
		                              terrain.name(),terrain));
	}
}
