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
 * @file action.hpp
 */

#ifndef WB_ACTION_HPP_
#define WB_ACTION_HPP_

#include "log.hpp"

#include <boost/shared_ptr.hpp>

static lg::log_domain log_whiteboard("whiteboard");
#define ERR_WB LOG_STREAM(err, log_whiteboard)
#define WRN_WB LOG_STREAM(warn, log_whiteboard)
#define LOG_WB LOG_STREAM(info, log_whiteboard)
#define DBG_WB LOG_STREAM(debug, log_whiteboard)

struct map_location;
struct temporary_unit_map_modifier;
class unit;
class unit_map;

namespace wb {

class action;
class visitor;

typedef boost::shared_ptr<temporary_unit_map_modifier> modifier_ptr;

typedef boost::shared_ptr<action> action_ptr;


/**
 * Superclass for all the whiteboard planned actions.
 */
class action
{
public:
	action();
	virtual ~action();

	virtual void accept(visitor& v) = 0;

	virtual void execute() = 0;

	/**
	 * Applies temporarily the result of this action to the unit map, and returns
	 * the corresponding modifier. Dropping the returned reference will revert
	 * the changes.
	 */
	virtual modifier_ptr apply_temp_modifier(unit_map& unit_map) = 0;

	/**
	 * Indicates whether this action is related to the specified hex.
	 * "Related" means the action affects this hex or draws a visual symbol in it
	 * at some point. Ex.: a move is related to this hex if its path goes through it.
	 */
	virtual bool is_related_to(const map_location& hex) const = 0;

	/**
	 * Indicates whether this actions targets the specified unit.
	 */
	virtual bool is_related_to(const unit& unit) const = 0;
};

} // end namespace wb

#endif /* WB_ACTION_HPP_ */
