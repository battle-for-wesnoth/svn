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

#include "visitor.hpp"

#include "arrow.hpp"
#include "unit.hpp"
#include "config.hpp"

namespace wb {

move::move(unit& subject, const map_location& target_hex, arrow& arrow)
: unit_(subject), orig_hex_(subject.get_location()), dest_hex_(target_hex), arrow_(arrow)
{
}

move::~move()
{
}

void move::accept(visitor& v)
{
	v.visit_move(*this);
}

modifier_ptr move::apply_temp_modifier(unit_map& unit_map)
{
	assert(unit_.get_location() == orig_hex_);
	LOG_WB << "Adding temp unit mover for unit " << unit_.name() << " [" << unit_.underlying_id() << "] "
			<< " from (" << orig_hex_ << ") to (" << dest_hex_ <<")\n";
	modifier_ptr modifier(new temporary_unit_mover(unit_map, orig_hex_, dest_hex_));
	return modifier;
}

} // end namespace wb
