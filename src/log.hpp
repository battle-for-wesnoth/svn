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
#ifndef LOG_HPP_INCLUDED
#define LOG_HPP_INCLUDED

#define LOG_DATA

#ifdef LOG_DATA

#include <iostream>
#include <string>

#include "SDL.h"

struct scope_logger
{
	scope_logger(const std::string& str) : ticks_(SDL_GetTicks()), str_(str) {
		for(int i = 0; i != indent; ++i)
			std::cerr << "  ";
		++indent;
		std::cerr << "BEGIN: " << str_ << "\n";
	}

	~scope_logger() {
		const int ticks = SDL_GetTicks() - ticks_;
		--indent;
		for(int i = 0; i != indent; ++i)
			std::cerr << "  ";
		std::cerr << "END: " << str_ << " (took " << ticks << "ms)\n";
	}

private:
	int ticks_;
	std::string str_;
	static int indent;
};

#define log0(a) std::cerr << a << "\n";
#define log1(a,b) std::cerr << a << " info: " << b << "\n";
#define log2(a,b,c) std::cerr << a << " info: " << b << ", " << c << "\n";

#define log_scope(a) scope_logger scope_logging_object__(a);

#else
#define log0(a)
#define log1(a,b)
#define log2(a,b,c)

#define log_scope(a)
#endif

#endif
