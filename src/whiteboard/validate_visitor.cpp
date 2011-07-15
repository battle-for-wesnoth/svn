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

#include "validate_visitor.hpp"
#include "attack.hpp"
#include "manager.hpp"
#include "move.hpp"
#include "recall.hpp"
#include "recruit.hpp"
#include "side_actions.hpp"
#include "suppose_dead.hpp"
#include "utility.hpp"

#include "arrow.hpp"
#include "foreach.hpp"
#include "pathfind/pathfind.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "team.hpp"

namespace wb
{

validate_visitor::validate_visitor(unit_map& unit_map)
	: mapbuilder_visitor(unit_map, true)
	, viewer_actions_(*viewer_actions())
	, actions_to_erase_()
	, arg_itor_()
{
	assert(!resources::whiteboard->has_planned_unit_map());
}

validate_visitor::~validate_visitor()
{
	//~mapbuilder_visitor() gets called here automatically
}

bool validate_visitor::validate_actions()
{
	//Temporarily reset all units' moves to full EXCEPT for the ones on viewer_side().
	reset_moves(); //< protected fcn inherited from mapbuilder_visitor

	//Apply modifiers from every team's action_queue, beginning with the current_team.
	size_t current_team = resources::controller->current_side() - 1;
	size_t num_teams = resources::teams->size();
	for(size_t iteration = 0; iteration < num_teams; ++iteration)
	{
		size_t team_index = (current_team+iteration) % num_teams;

		side_actions& actions = *resources::teams->at(team_index).get_side_actions();
		arg_itor_ = actions.begin(); //< Parameter for the visit_***() fcns
		side_actions::iterator end  = actions.end();
		for(; arg_itor_!=end; ++arg_itor_)
			(*arg_itor_)->accept(*this);
	}

	//FIXME: by reverse iterating this can be done in a more efficiant way
	// by using the iterator returned by remove_action it could even be done in the loop above
	if (!actions_to_erase_.empty())
	{
		int side_actions_size_before = viewer_actions_.actions().size();
		LOG_WB << "Erasing " << actions_to_erase_.size() << " invalid actions.\n";
		foreach(action_ptr action, actions_to_erase_)
		{
			viewer_actions_.remove_action(viewer_actions_.get_position_of(action), false);
		}
		assert(side_actions_size_before - viewer_actions_.actions().size() == actions_to_erase_.size());
		actions_to_erase_.clear();
		return false;
	}
	else
	{
		return true;
	}
}

/* private */
validate_visitor::VALIDITY validate_visitor::evaluate_move_validity(move_ptr m_ptr)
{
	move& m = *m_ptr;

	if (!(m.get_source_hex().valid() && m.get_dest_hex().valid()))
		return WORTHLESS;

	//Check that the unit still exists in the source hex
	unit_map::const_iterator unit_it;
	unit_it = resources::units->find(m.get_source_hex());
	if (unit_it == resources::units->end())
		return WORTHLESS;

	//check if the unit in the source hex has the same unit id as before,
	//i.e. that it's the same unit
	if (m.unit_id_ != unit_it->id())
		return WORTHLESS;

	//Now that we've reliably identified the unit owning this planned move, update the
	//pointer in case there has been some funny business in the unit map
	m.unit_ = &*unit_it;

	//check that the path is good
	if (m.get_source_hex() != m.get_dest_hex()) //allow for zero-hex, move, in which case we skip pathfinding
	{
		//verify that the destination hex is free
		if (resources::units->find(m.get_dest_hex()) != resources::units->end())
			return OBSTRUCTED;

		pathfind::marked_route new_route;
		//@todo: use something else than empty vector for waypoints?
		new_route = resources::controller->get_mouse_handler_base().get_route(m.get_unit(),m.get_dest_hex(),
											std::vector<map_location>(), resources::teams->at(viewer_actions_.team_index()));

		/**
		 * @todo Is the comparison with getNoPathValue really necessary? An empty route (with cost = 0) is returned
		 *       when no path exists.
		 */
		if(new_route.steps.empty() || new_route.move_cost >= pathfind::cost_calculator::getNoPathValue())
			return OBSTRUCTED; //no path exists

		if(m.team_index() == viewer_team()) //< Don't mess with any other team's queue -- only our own
		{
			if(new_route.steps != m.get_route().steps || new_route.move_cost != m.get_route().move_cost)
			{
				//new valid path differs from the previous one, replace
				m.set_route(new_route);

				//send updated path to allies
				resources::whiteboard->queue_net_cmd(viewer_actions_.make_net_cmd_replace(arg_itor_,m_ptr));

				//@todo: Since this might lengthen the path, we probably need a special conflict state
				// to warn the player that the initial path is no longer possible.
			}
		}

		//Check that the unit still has enough movement to do this move
		if (unit_it->movement_left() < m.movement_cost_)
			return OBSTRUCTED;
	}

	return VALID;
}

// This helper function determines whether there are any invalid actions planned for m_ptr->get_unit()
// that occur earlier in viewer_actions_ than m_ptr.
/* private */
bool validate_visitor::no_previous_invalids(move_ptr m_ptr)
{
	//arg_itor_ is a protected member of mapbuilder_visitor
	if(arg_itor_ == viewer_actions_.begin())
		return true;
	side_actions::iterator prev_action_of_unit = viewer_actions_.find_last_action_of(m_ptr->get_unit(),arg_itor_-1);
	if(prev_action_of_unit == viewer_actions_.end())
		return true;
	return (*prev_action_of_unit)->is_valid();
}

void validate_visitor::visit_move(move_ptr move)
{
	DBG_WB <<"visiting move from " << move->get_source_hex()
			<< " to " << move->get_dest_hex() << "\n";
	//invalidate start and end hexes so number display is updated properly
	resources::screen->invalidate(move->get_source_hex());
	resources::screen->invalidate(move->get_dest_hex());

	switch(evaluate_move_validity(move)) //< private helper fcn
	{
	case VALID:
		// Now call the superclass to apply the result of this move to the unit map,
		// so that further pathfinding takes it into account.
		move->set_valid(true);
		mapbuilder_visitor::visit_move(move);
		break;
	case OBSTRUCTED:
		move->set_valid(false);
		break;
	case WORTHLESS:
		// Erase only if no previous invalid actions are planned for this unit -- otherwise, just mark it invalid.
		// Otherwise, we wouldn't be able to keep invalid actions that depend on previous invalid actions.
		if(viewer_team() == move->team_index() //< Don't mess with any other team's queue -- only our own
				&& no_previous_invalids(move)) //< private helper fcn
		{
			LOG_WB << "Worthless invalid move detected, adding to actions_to_erase_.\n";
			actions_to_erase_.insert(move);
		}
		else move->set_valid(false);
		break;
	}
}

void validate_visitor::visit_attack(attack_ptr attack)
{
	DBG_WB <<"visiting attack from " << attack->get_dest_hex()
			<< " to " << attack->target_hex_ << "\n";
	//invalidate target hex to make sure attack indicators are updated
	resources::screen->invalidate(attack->get_dest_hex());
	resources::screen->invalidate(attack->target_hex_);

	//Verify that the target hex is still valid
	//and Verify that the target hex isn't empty
	if (!attack->target_hex_.valid()
			|| resources::units->find(attack->target_hex_) == resources::units->end())
	{
		if(viewer_team() == attack->team_index()) //< Don't mess with any other team's queue -- only our own
		{
			LOG_WB << "Worthless invalid attack detected, adding to actions_to_erase_.\n";
			actions_to_erase_.insert(attack);
		}
	}
	else //All checks pass, so call the visitor on the superclass
	{
		//@todo: verify that the target hex contains the same unit that before,
		// comparing for example the unit ID

		//@todo: Verify that the target unit is our enemy

		visit_move(boost::static_pointer_cast<move>(attack));
	}
}

void validate_visitor::visit_recruit(recruit_ptr recruit)
{
	DBG_WB << "visiting recruit on hex " << recruit->recruit_hex_ << "\n";
	//invalidate recruit hex so number display is updated properly
	resources::screen->invalidate(recruit->recruit_hex_);

	int team_index = viewer_actions_.team_index();

	//Check that destination hex is still free
	if(resources::units->find(recruit->recruit_hex_) != resources::units->end())
	{
		LOG_WB << "Recruit set as invalid because target hex is occupied.\n";
		recruit->set_valid(false);
	}
	//Check that unit to recruit is still in side's recruit list
	if (recruit->is_valid())
	{
		const std::set<std::string>& recruits = (*resources::teams)[team_index].recruits();
		if (recruits.find(recruit->unit_name_) == recruits.end())
		{
			recruit->set_valid(false);
			LOG_WB << " Validate visitor: Planned recruit invalid since unit is not in recruit list anymore.\n";
		}
	}
	//Check that there is still enough gold to recruit this unit
	if (recruit->is_valid() && recruit->temp_unit_->cost() > (*resources::teams)[team_index].gold())
	{
		LOG_WB << "Recruit set as invalid, team doesn't have enough gold.\n";
		recruit->set_valid(false);
	}

	if (recruit->is_valid())
	{
		mapbuilder_visitor::visit_recruit(recruit);
	}
	else
	{
		if(viewer_team() == recruit->team_index()) //< Don't mess with any other team's queue -- only our own
		{
			LOG_WB << "Invalid recruit detected, adding to actions_to_erase_.\n";
			actions_to_erase_.insert(recruit);
		}
	}
}

void validate_visitor::visit_recall(recall_ptr recall)
{
	DBG_WB << "visiting recall on hex " << recall->recall_hex_ << "\n";
	//invalidate recall hex so number display is updated properly
	resources::screen->invalidate(recall->recall_hex_);

	int team_index = viewer_actions_.team_index();

	//Check that destination hex is still free
	if(resources::units->find(recall->recall_hex_) != resources::units->end())
	{
		LOG_WB << "Recall set as invalid because target hex is occupied.\n";
		recall->set_valid(false);
	}
	//Check that unit to recall is still in side's recall list
	if (recall->is_valid())
	{
		const std::vector<unit>& recalls = (*resources::teams)[team_index].recall_list();
		if (std::find_if(recalls.begin(), recalls.end(),
				unit_comparator_predicate(*recall->temp_unit_)) == recalls.end())
		{
			recall->set_valid(false);
			LOG_WB << " Validate visitor: Planned recall invalid since unit is not in recall list anymore.\n";
		}
	}
	//Check that there is still enough gold to recall this unit
	if (recall->is_valid()
			&& (*resources::teams)[team_index].recall_cost() > (*resources::teams)[team_index].gold())
	{
		LOG_WB << "Recall set as invalid, team doesn't have enough gold.\n";
		recall->set_valid(false);
	}

	if (recall->is_valid())
	{
		mapbuilder_visitor::visit_recall(recall);
	}
	else
	{
		if(viewer_team() == recall->team_index()) //< Don't mess with any other team's queue -- only our own
		{
			LOG_WB << "Invalid recall detected, adding to actions_to_erase_.\n";
			actions_to_erase_.insert(recall);
		}
	}
}

void validate_visitor::visit_suppose_dead(suppose_dead_ptr sup_d)
{
	DBG_WB << "visiting suppose_dead on hex " << sup_d->loc_ << "\n";
	//invalidate suppose-dead hex so number display is updated properly
	resources::screen->invalidate(sup_d->loc_);

	if(!sup_d->get_source_hex().valid())
		sup_d->set_valid(false);

	unit_map::const_iterator unit_it;
	//Check that the unit still exists in the source hex
	if(sup_d->valid_)
	{
		unit_it = resources::units->find(sup_d->get_source_hex());

		if(unit_it == resources::units->end())
		{
			sup_d->set_valid(false);
			sup_d->unit_ = NULL;
		}
	}

	//check if the unit in the source hex has the same unit id as before,
	//i.e. that it's the same unit
	if(sup_d->valid_ && sup_d->unit_id_ != unit_it->id())
	{
		sup_d->set_valid(false);
		sup_d->unit_ = NULL;
	}

	if(sup_d->valid_)
	{
		// Now call the superclass to apply the result of this move to the unit map,
		// so that further pathfinding takes it into account.
		mapbuilder_visitor::visit_suppose_dead(sup_d);
	}
	else
	{
		if(viewer_team() == sup_d->team_index()) //< Don't mess with any other team's queue -- only our own
		{
			LOG_WB << "Invalid suppose_dead detected, adding to actions_to_erase_.\n";
			actions_to_erase_.insert(sup_d);
		}
	}
}

}//end namespace wb
