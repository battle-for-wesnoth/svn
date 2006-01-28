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
#include "tstring.hpp"
#include "config.hpp"
#include "terrain.hpp"

#include <cstdlib>
#include <iostream>

terrain_type::terrain_type() : symbol_image_("void"), letter_(' '), type_(" "),
                               height_adjust_(0), submerge_(0.0),
                               heals_(false), village_(false), castle_(false), keep_(false)
{}

terrain_type::terrain_type(const config& cfg)
{
	symbol_image_ = cfg["symbol_image"];

	name_ = cfg["name"];
	id_ = cfg["id"];
	const std::string& letter = cfg["char"];

	if(letter == "") {
		letter_ = 0;
	} else {
		letter_ = letter[0];
	}

	const std::string& alias = cfg["aliasof"];
	if(alias.empty()) {
		type_.resize(1);
		type_[0] = letter_;
	} else {
		type_ = alias;
	}

	height_adjust_ = atoi(cfg["unit_height_adjust"].c_str());
	submerge_ = atof(cfg["submerge"].c_str());

	light_modification_ = atoi(cfg["light"].c_str());

	heals_ = cfg["heals"] == "true";
	village_ = cfg["gives_income"] == "true";
	castle_ = cfg["recruit_onto"] == "true";
	keep_ = cfg["recruit_from"] == "true";
}

const std::string& terrain_type::symbol_image() const
{
	return symbol_image_;
}

const t_string& terrain_type::name() const
{
	return name_;
}

const std::string& terrain_type::id() const
{
	return id_;
}

char terrain_type::letter() const
{
	return letter_;
}

bool terrain_type::is_nonnull() const
{
	return (letter_ != 0) && (letter_ != ' ');
}

const std::string& terrain_type::type() const
{
	return type_;
}

int terrain_type::light_modification() const
{
	return light_modification_;
}

bool terrain_type::is_alias() const
{
	return type_.size() != 1 || type_[0] != letter_;
}

int terrain_type::unit_height_adjust() const
{
	return height_adjust_;
}

double terrain_type::unit_submerge() const
{
	return submerge_;
}

bool terrain_type::gives_healing() const
{
	return heals_;
}

bool terrain_type::is_village() const
{
	return village_;
}

bool terrain_type::is_castle() const
{
	return castle_;
}

bool terrain_type::is_keep() const
{
	return keep_;
}

void create_terrain_maps(const std::vector<config*>& cfgs,
                         std::vector<char>& terrain_list,
                         std::map<char,terrain_type>& letter_to_terrain,
                         std::map<std::string,terrain_type>& str_to_terrain)
{
	for(std::vector<config*>::const_iterator i = cfgs.begin();
	    i != cfgs.end(); ++i) {
		terrain_type terrain(**i);
		terrain_list.push_back(terrain.letter());
		letter_to_terrain.insert(std::pair<char,terrain_type>(
		                              terrain.letter(),terrain));
		str_to_terrain.insert(std::pair<std::string,terrain_type>(
		                              terrain.id(),terrain));
	}
}
