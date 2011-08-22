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

#ifndef WB_MAPBUILDER_HPP_
#define WB_MAPBUILDER_HPP_

#include <boost/ptr_container/ptr_vector.hpp>

#include "action.hpp"
#include "utility.hpp"
#include "visitor.hpp"

struct unit_movement_resetter;

namespace wb
{

/**
 * Visitor that collects and applies unit_map modifications from the actions it visits
 * and reverts all changes on destruction.
 */
class mapbuilder
	: private enable_visit_all<mapbuilder>
{
	friend class enable_visit_all<mapbuilder>;

public:
	mapbuilder(unit_map& unit_map);
	virtual ~mapbuilder();

	///builds every team's actions as far into the future as possible, in the correct order
	void build_map();

private:
	//"Inherited" from enable_visit_all
	bool visit(size_t team_index, team&, side_actions&, side_actions::iterator);
	bool post_visit_team(size_t team_index, team&, side_actions&);

	//helper for visit(), above
	bool visit_helper(side_actions::iterator const&, action_ptr const&);

	//For validate_visitor to override
	virtual void validate(side_actions::iterator const&) {}

	//Helper fcn: Arranges for all units' MP to be restored upon destruction
	void reset_moves();

	void restore_normal_map();

	unit_map& unit_map_;

	action_queue applied_actions_;

	//Used by reset_moves()
	boost::ptr_vector<unit_movement_resetter> resetters_;

	//Used by visit()
	std::set<unit const*> acted_this_turn_;
};

}

#endif /* WB_MAPBUILDER_HPP_ */
