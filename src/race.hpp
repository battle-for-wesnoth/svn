/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef RACE_HPP_INCLUDED
#define RACE_HPP_INCLUDED

#include <map>
#include <string>
#include <vector>

#include "serialization/string_utils.hpp"
#include "config.hpp"

typedef std::map<wide_string, std::vector<wchar_t> > markov_prefix_map;

class unit_race
{
public:
	enum GENDER { MALE, FEMALE, NUM_GENDERS };

	unit_race();
	unit_race(const config& cfg);

	const std::string& id() const { return id_; };
	const t_string& name() const { return name_; };
	const t_string& description() const { return description_; };

	std::string generate_name(GENDER gender) const;

	bool uses_global_traits() const;

	const config::child_list& additional_traits() const;
	unsigned int num_traits() const;

private:
	std::string id_;
	t_string name_;
	t_string description_;
	unsigned int ntraits_;
	std::vector<std::string> names_[NUM_GENDERS];
	markov_prefix_map next_[NUM_GENDERS];
	int chain_size_;

	const config::child_list* traits_;
	bool global_traits_;
};

unit_race::GENDER string_gender(const std::string& str,unit_race::GENDER def=unit_race::MALE);
std::string const& gender_string(unit_race::GENDER gender);

typedef std::map<std::string,unit_race> race_map;

#endif
