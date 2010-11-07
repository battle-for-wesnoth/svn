/* $Id$ */
/*
   Copyright (C) 2010 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef SIDE_FILTER_H_INCLUDED
#define SIDE_FILTER_H_INCLUDED

#include "variable.hpp"

class config;
class unit;
class unit_map;
class team;

//side_filter: a class that implements the Standard Side Filter
class side_filter {
public:

#ifdef _MSC_VER
	// This constructor is required for MSVC 9 SP1 due to a bug there
	// see http://social.msdn.microsoft.com/forums/en-US/vcgeneral/thread/34473b8c-0184-4750-a290-08558e4eda4e
	// other compilers don't need it.
	side_filter();
#endif

	side_filter(const vconfig& cfg, const bool flat_tod = false);
	side_filter(const vconfig& cfg, const side_filter& original);
	/** Default implementation, but defined out-of-line for efficiency reasons. */
	virtual ~side_filter();

	side_filter(const side_filter &other);
	side_filter& operator=(const side_filter &other);

	//match: returns true if and only if the given team matches this filter
	bool match(const team& t) const;
	bool match(const int side) const;
	virtual bool operator()(const team& t) { return this->match(t); }
	virtual bool operator()(const int side) { return this->match(side); }

	//get_teams: gets all teams on the map that match this filter
	// @param teams - out parameter containing the results
	void get_teams(std::set<team*> &teams) const;

	config to_config() const;
private:
	bool match_internal(const team& t) const;

	const vconfig cfg_; //config contains WML for a Standard Side Filter

	bool flat_;
};

#endif

