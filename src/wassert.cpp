/* $Id$ */
/*
   Copyright (C) 2005 - 2007 by Isaac Clerencia <isaac@sindominio.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file wassert.cpp
//! Write assert-message.

#ifdef STANDARD_ASSERT_DOES_NOT_WORK

#include "log.hpp"

#include <iostream>

void wassert(bool expression)
{
	// crash if expression is false
	if(! expression) {
		LOG_STREAM(err, general) << "Assertion failure" << "\n";
		*reinterpret_cast<int*>(0) = 5;
	}
}

#endif
