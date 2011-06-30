/* $Id$ */
/*
 Copyright (C) 2010 - 2011 by Gabriel Morin <gabrielmorin (at) gmail (dot) com>
 Part of the Battle for Wesnoth Project http://www.wesnoth.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY.

 See the COPYING file for more details.
 */

/**
 * @file
 */

#ifndef WB_ACTION_HPP_
#define WB_ACTION_HPP_

#include "config.hpp"

#include "typedefs.hpp"

namespace wb {

class visitor;

/**
 * Abstract base class for all the whiteboard planned actions.
 */
class action
{
public:
	action(size_t team_index);
	explicit action(config const&); // For deserialization
	virtual ~action();

	virtual std::ostream& print(std::ostream& s) const = 0;

	virtual void accept(visitor& v) = 0;

	/** Returns true if the action has been completely executed and can be deleted */
	virtual bool execute() = 0;

	/** Applies temporarily the result of this action to the specified unit map. */
	virtual void apply_temp_modifier(unit_map& unit_map) = 0;
	/** Removes the result of this action from the specified unit map. */
	virtual void remove_temp_modifier(unit_map& unit_map) = 0;

	/** Gets called by display when drawing a hex, to allow actions to draw to the screen. */
	virtual void draw_hex(const map_location& hex) = 0;

	/** Indicates whether this hex is the preferred hex to draw the numbering for this action. */
	virtual bool is_numbering_hex(const map_location& hex) const = 0;

	/** Return the unit targeted by this action. Null if unit doesn't exist. */
	virtual unit* get_unit() const = 0;
	/** Returns the index of the team that owns this action */
	size_t team_index() const { return team_index_; }
	/** Returns the number of the side that owns this action, i.e. the team index + 1. */
	int side_number() const { return (int) team_index_ + 1; }

	/**
	 * Indicates to an action whether its status is invalid, and whether it should change its
	 * display (and avoid any change to the game state) accordingly
	 */
	virtual void set_valid(bool valid) = 0;
	virtual bool is_valid() = 0;

	/** Constructs and returns a config object representing this object. */
	virtual config to_config() const;
	/** Constructs an object of a subclass of wb::action using a config. Current behavior is to return a null pointer for unrecognized config. */
	static action_ptr from_config(config const&);

private:
	size_t team_index_;
};

std::ostream& operator<<(std::ostream& s, action_ptr action);
std::ostream& operator<<(std::ostream& s, action_const_ptr action);

} // end namespace wb

#endif /* WB_ACTION_HPP_ */
