/* $Id$ */
/*
   Copyright (C) 2005 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "util.hpp"
#include <cstdlib>

template<>
int lexical_cast<int, const std::string&>(const std::string& a)
{
	char* endptr;
	int res = strtol(a.c_str(), &endptr, 10);

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
int lexical_cast<int, const char*>(const char* a)
{
	char* endptr;
	int res = strtol(a, &endptr, 10);

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
int lexical_cast_default<int, const std::string&>(const std::string& a, int def)
{
	char* endptr;
	int res = strtol(a.c_str(), &endptr, 10);

	if (a.empty() || *endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

template<>
int lexical_cast_default<int, const char*>(const char* a, int def)
{
	char* endptr;
	int res = strtol(a, &endptr, 10);

	if (*a == '\0' || *endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

