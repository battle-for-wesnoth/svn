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
#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <cmath>
#include <map>

//instead of playing with VC++'s crazy definitions of min and max,
//just define our own
template<typename T>
T& minimum(T& a, T& b) { return a < b ? a : b; }

template<typename T>
const T& minimum(const T& a, const T& b) { return a < b ? a : b; }

template<typename T>
T& maximum(T& a, T& b) { return a < b ? b : a; }

template<typename T>
const T& maximum(const T& a, const T& b) { return a < b ? b : a; }

template<typename T>
inline bool is_odd(T num) { return (static_cast<unsigned int>(num > 0 ? num : -num)&1) == 1; }

template<typename T>
inline bool is_even(T num) { return !is_odd(num); }

//place in our own namespace as to not clash with possible
//standard library definitions
namespace util {

template<typename T>
T round(T n) { return floor(n + 0.5); }

}

#endif
