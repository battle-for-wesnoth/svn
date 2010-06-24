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
 * @file manager.cpp
 */

#include "manager.hpp"

#include "action.hpp"
#include "find_visitor.hpp"
#include "highlight_visitor.hpp"
#include "mapbuilder_visitor.hpp"
#include "move.hpp"

#include "arrow.hpp"
#include "foreach.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "unit_display.hpp"

namespace wb {

typedef boost::shared_ptr<side_actions> side_actions_ptr;

manager::manager():
		active_(false),
		mapbuilder_(),
		route_(),
		move_arrow_(),
		fake_unit_(),
		selected_unit_(NULL),
		highlighted_hex_(map_location::null_location),
		ignore_mouse_motion_(false),
		stacked_modifiers_calls_(0)
{
}

manager::~manager()
{
	if (resources::screen && fake_unit_)
	{
		resources::screen->remove_temporary_unit(fake_unit_.get());
	}
}

static side_actions_ptr get_current_side_actions()
{
	int current_side = resources::controller->current_side();
	team& current_team = (*resources::teams)[current_side - 1];
	side_actions_ptr side_actions = current_team.get_side_actions();
	return side_actions;
}

void manager::push_temp_modifiers()
{
	if (active_)
	{
		if (stacked_modifiers_calls_ == 0)
		{
			mapbuilder_.reset(new mapbuilder_visitor(*resources::units));
			const action_set& actions = get_current_side_actions()->actions();
			foreach (const action_ptr &action, actions)
			{
				assert(action);
				action->accept(*mapbuilder_);
			}
		}
		DBG_WB << "Increasing stacked modifiers calls count to " << stacked_modifiers_calls_ << "\n";
		++stacked_modifiers_calls_;
	}
}

void manager::pop_temp_modifiers()
{
	if (active_)
	{
		if (stacked_modifiers_calls_ > 0)
		{
			DBG_WB << "Decreasing stacked modifiers calls count to " << stacked_modifiers_calls_ << "\n";
			--stacked_modifiers_calls_;
			if (stacked_modifiers_calls_ == 0)
			{
				DBG_WB << "Removing temporary modifiers.\n";
				mapbuilder_.reset();
			}
		}
	}
}

void manager::clear_temp_modifiers()
{
	if (active_)
	{
		DBG_WB << "Removing temporary modifiers and erasing stack marker.\n";
		stacked_modifiers_calls_ = 0;
		mapbuilder_.reset();
	}
}

void manager::mouseover_hex(const map_location& hex)
{
	if (active_ && !selected_unit_)
	{
		remove_highlight();
		highlight_hex(hex);
	}
}

void manager::highlight_hex(const map_location& hex)
{
	highlighted_hex_ = hex;

	unit_map::iterator highlighted_unit = resources::units->find(hex);

	highlight_visitor highlighter(true);

	action_set actions = get_current_side_actions()->actions();
	foreach(action_ptr action, actions)
	{
		if (action->is_related_to(hex)
			|| (highlighted_unit.valid() && action->is_related_to(*highlighted_unit)))
		{
			action->accept(highlighter);
		}
	}
}

void manager::remove_highlight()
{
	highlight_visitor unhighlighter(false);

	action_set actions = get_current_side_actions()->actions();
	foreach(action_ptr action, actions)
	{
		action->accept(unhighlighter);
	}
}

void manager::select_unit(unit& unit)
{
	erase_temp_move();
	remove_highlight();
	action_set actions = get_current_side_actions()->actions();
	highlight_visitor highlighter(true);
	foreach(action_ptr action, actions)
	{
		if (action->is_related_to(unit))
		{
			action->accept(highlighter);
		}
	}
	selected_unit_ = &unit;
	DBG_WB << "Selected unit " << selected_unit_->name() << " [" << selected_unit_->id() << "]\n";
}

void manager::deselect_unit()
{
	if (selected_unit_)
	{
		DBG_WB << "Deselecting unit " << selected_unit_->name() << " [" << selected_unit_->id() << "]\n";
		erase_temp_move();
		selected_unit_ = NULL;
	}
}

void manager::create_temp_move(const std::vector<map_location> &steps)
{
	//NOTE: route.back() = dst, and route.front() = src
	route_ = steps;
	if (route_.size() > 1 && selected_unit_ != NULL)
	{
		if (!move_arrow_) //TODO: create new arrow if turn ended
		{
			// Create temp arrow
			move_arrow_.reset(new arrow());
			int current_side = resources::controller->current_side();
			move_arrow_->set_color(team::get_side_color_index(current_side));
			move_arrow_->set_alpha(move::ALPHA_HIGHLIGHT);
			resources::screen->add_arrow(*move_arrow_);

		}
		move_arrow_->set_path(route_);

		// Create temp ghost unit (erases previous one if there was one)
		if (fake_unit_)
			resources::screen->remove_temporary_unit(fake_unit_.get());
		fake_unit_.reset(new unit(*selected_unit_));
		unit_display::move_unit(route_, *fake_unit_, *resources::teams, false); //get facing right
		fake_unit_->set_location(route_.back());
		resources::screen->place_temporary_unit(fake_unit_.get());
		fake_unit_->set_ghosted(true);

		//this commented-out code is the alternative of moving fake unit instead of recreating it
		//unit_display::move_unit doesn't seem to work properly with this method
//		fake_unit_->set_location(route_.back());
//		unit_display::move_unit(route_, *fake_unit_, *resources::teams, false); //get facing right
//		fake_unit_->set_ghosted(true);
	}
}

bool manager::during_move_creation() const
{
	bool during_move_creation = selected_unit_ != NULL;
	return during_move_creation;
}

void manager::erase_temp_move()
{
	if (move_arrow_)
	{
		move_arrow_.reset(); //auto-removes itself from display

		//reset src unit back to normal, if it lacks any planned action
		if (selected_unit_ && !has_action(*selected_unit_))
		{
				selected_unit_->set_standing(true);
		}
	}
	if (fake_unit_)
	{
		resources::screen->remove_temporary_unit(fake_unit_.get());
		fake_unit_.reset();
	}
}

void manager::save_temp_move()
{
	LOG_WB << "Creating move for unit " << selected_unit_->name() << " [" << selected_unit_->id() << "]"
			<< " from " << route_.front()
			<< " to " << route_.back() << "\n";
	ignore_mouse_motion_ = true;

	scoped_modifiers wb_modifiers;

	selected_unit_->set_ghosted(false);
	unit_display::move_unit(route_, *fake_unit_, *resources::teams, true);
	fake_unit_->set_standing(true);

	get_current_side_actions()->queue_move(*selected_unit_, route_.front(), route_.back(), move_arrow_, fake_unit_);
	move_arrow_.reset();
	fake_unit_.reset();
	//selected_unit_->set_standing(true);
	selected_unit_ = NULL;

	ignore_mouse_motion_ = false;
}

void manager::execute_next()
{
	//TODO: catch end_turn_exception somewhere here?
	//TODO: switch display to "prototype A", i.e. dst as ghost
	get_current_side_actions()->execute_next();
}

void manager::delete_last()
{
	get_current_side_actions()->remove_action(get_current_side_actions()->end() - 1);
}

action_ptr manager::has_action(const unit& unit) const
{
	find_visitor finder;
	action_ptr action = finder.find_first_action_of(unit, get_current_side_actions()->actions());
	return action;
}

scoped_modifiers::scoped_modifiers()
{
	resources::whiteboard->push_temp_modifiers();
}

scoped_modifiers::~scoped_modifiers()
{
	resources::whiteboard->pop_temp_modifiers();
}

scoped_modifiers_remover::scoped_modifiers_remover()
{
	resources::whiteboard->pop_temp_modifiers();
}

scoped_modifiers_remover::~scoped_modifiers_remover()
{
	resources::whiteboard->push_temp_modifiers();
}

} // end namespace wb
