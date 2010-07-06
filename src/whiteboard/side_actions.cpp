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
 * @file side_actions.cpp
 */

#include "side_actions.hpp"
#include "action.hpp"
#include "manager.hpp"
#include "move.hpp"
#include "validate_visitor.hpp"

#include "foreach.hpp"
#include "resources.hpp"
#include <set>

namespace wb
{

side_actions::side_actions()
	: actions_()
{
}

side_actions::~side_actions()
{
}

const action_queue& side_actions::actions() const
{
	return actions_;
}

side_actions::iterator side_actions::execute_next()
{
	if (!actions_.empty())
	{
		execute(begin());
		return begin();
	}
	return end();
}

side_actions::iterator side_actions::execute(side_actions::iterator position)
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Modifying action queue while temp modifiers are applied!!!\n";
	}

	if (!actions_.empty() && validate_iterator(position))
	{
		size_t distance = std::distance(begin(), position);
		action_ptr action = *position;
		bool finished = action->execute();
		if (finished)
		{
			actions_.erase(position);
			validate_actions();
			return begin() + distance;
		}
		else
		{
			actions_.erase(position);
			actions_.insert(end(), action);
			validate_actions();
			return end() - 1;
		}
	}
	else
	{
		return end();
	}
}

side_actions::iterator side_actions::insert_move(unit& subject, const map_location& source_hex, const map_location& target_hex, side_actions::iterator position,
		arrow_ptr arrow,	fake_unit_ptr fake_unit)
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Modifying action queue while temp modifiers are applied!!!\n";
	}

	action_ptr action(new move(subject, source_hex, target_hex, arrow, fake_unit));
	assert(position < end());
	iterator valid_position = actions_.insert(position, action);
	validate_actions();
	return valid_position;
}

side_actions::iterator side_actions::queue_move(unit& subject, const map_location& source_hex, const map_location& target_hex,
		arrow_ptr arrow,	fake_unit_ptr fake_unit)
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Modifying action queue while temp modifiers are applied!!!\n";
	}

	action_ptr action(new move(subject, source_hex, target_hex, arrow, fake_unit));
	actions_.push_back(action);
	// Contrary to insert_move, no need to validate actions here since we're adding to the end of the queue
	return end() - 1;
}

side_actions::iterator side_actions::move_earlier(side_actions::iterator position, size_t increment)
{
	return move_in_queue(position, - (int) increment);
}

side_actions::iterator side_actions::move_later(side_actions::iterator position, size_t increment)
{
	return move_in_queue(position, (int) increment);
}

side_actions::iterator side_actions::remove_action(side_actions::iterator position)
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Modifying action queue while temp modifiers are applied!!!\n";
	}

	assert((!actions_.empty() && validate_iterator(position)));
	size_t distance = std::distance(begin(), position);
	if (!actions_.empty() && validate_iterator(position))
	{
		actions_.erase(position);
		validate_actions();
	}
	return begin() + distance;
}

side_actions::iterator side_actions::get_position_of(action_ptr action)
{
	if (!actions_.empty())
	{
		action_queue::iterator position;
		for (position = actions_.begin(); position != actions_.end(); ++position)
		{
			if (*position == action)
			{
				return position;
			}
		}
	}
	return end();
}

side_actions::iterator side_actions::find_first_action_of(const unit& unit, side_actions::iterator start_position)
{

	if (start_position == side_actions::iterator())
	{
		start_position = begin();
	}

	if (validate_iterator(start_position))
	{
		side_actions::iterator position;
		for (position = start_position; position != end(); ++position)
		{
			action_ptr action = *position;
			if (&action->get_unit() == &unit)
			{
				return position;
			}
		}
	}
	return end();
}

side_actions::iterator side_actions::find_last_action_of(const unit& unit, side_actions::iterator start_position)
{
	if (start_position == side_actions::iterator())
	{
		start_position = end() - 1;
	}

	if (validate_iterator(start_position))
	{
		side_actions::iterator position;
		for (position = start_position; position != begin() - 1; --position)
		{
			action_ptr action = *position;
			if (&action->get_unit() == &unit)
			{
				return position;
			}
		}
	}
	return end();
}

void side_actions::validate_actions()
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Validating action queue while temp modifiers are applied!!!\n";
	}

	validate_visitor validator(*resources::units, shared_from_this());
	validator.validate_actions();
}

side_actions::iterator side_actions::move_in_queue(side_actions::iterator position, int increment)
{
	if (resources::whiteboard->has_planned_unit_map())
	{
		ERR_WB << "Modifying action queue while temp modifiers are applied!!!\n";
	}

	assert(!actions_.empty());
	assert(validate_iterator(position));
	if (actions_.empty() || !validate_iterator(position))
		return end();

	action_ptr action = *position;
	action_queue::iterator after = actions_.erase(position);
	//be careful, previous iterators have just been invalidated by erase()
	action_queue::iterator destination = after + increment;
	action_queue::iterator valid_position = actions_.insert(destination, action);
	validate_actions();
	return valid_position;
}


} //end namespace wb
