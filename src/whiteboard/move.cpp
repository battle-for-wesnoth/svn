/* $Id$ */
/*
 Copyright (C) 2010 by Gabriel Morin <gabrielmorin (at) gmail (dot) com>
 Part of the Battle for Wesnoth Project http://www.wesnoth.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2
 or at your option any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY.

 See the COPYING file for more details.
 */

/**
 * @file move.cpp
 */

#include "move.hpp"
#include "unit.hpp"
#include "config.hpp"

namespace wb {

move::move(unit& subject, const map_location& target_hex)
: unit_(subject), target_hex_(target_hex)
{
	//TODO: create arrow here
}

move::~move()
{
	//TODO: delete arrow here
}

void move::accept(visitor& v)
{
	v.visit_move(*this);
}

} // end namespace wb
