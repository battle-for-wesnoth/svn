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

#include "move.hpp"

#include "visitor.hpp"
#include "manager.hpp"
#include "side_actions.hpp"
#include "utility.hpp"

#include "arrow.hpp"
#include "config.hpp"
#include "foreach.hpp"
#include "game_end_exceptions.hpp"
#include "mouse_events.hpp"
#include "play_controller.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "unit.hpp"
#include "unit_display.hpp"
#include "unit_map.hpp"

namespace wb {

std::ostream& operator<<(std::ostream &s, move_ptr move)
{
	assert(move);
	return move->print(s);
}

std::ostream& operator<<(std::ostream &s, move_const_ptr move)
{
	assert(move);
	return move->print(s);
}

std::ostream& move::print(std::ostream &s) const
{
	s << "Move for unit " << get_unit()->name() << " [" << get_unit()->id() << "] "
			<< "from (" << get_source_hex() << ") to (" << get_dest_hex() << ")";
	return s;
}

move::move(size_t team_index, bool hidden, const pathfind::marked_route& route,
		arrow_ptr arrow, fake_unit_ptr fake_unit)
: action(team_index,hidden),
  unit_(NULL),
  unit_id_(),
  route_(new pathfind::marked_route(route)),
  movement_cost_(0),
  arrow_(arrow),
  fake_unit_(fake_unit),
  valid_(true),
  arrow_brightness_(),
  arrow_texture_(),
  mover_(),
  fake_unit_hidden_(false)
{
	assert(!route_->steps.empty());

	if(hidden)
		fake_unit_->set_hidden(true);

	unit_ = wb::future_visible_unit(get_source_hex());

	this->init();
}

move::move(config const& cfg, bool hidden)
	: action(cfg,hidden)
	, unit_()
	, unit_id_()
	, route_(new pathfind::marked_route())
	, movement_cost_()
	, arrow_(new arrow(hidden))
	, fake_unit_()
	, valid_(true)
	, arrow_brightness_()
	, arrow_texture_()
	, mover_()
	, fake_unit_hidden_(false)
{
	// Construct and validate unit_
	unit_map::iterator unit_itor = resources::units->find(cfg["unit_"]);
	if(unit_itor == resources::units->end())
		throw action::ctor_err("move: Invalid underlying_id");
	unit_ = &*unit_itor;

	// Construct and validate route_
	config const& route_cfg = cfg.child("route_");
	if(!route_cfg)
		throw action::ctor_err("move: Invalid route_");
	route_->move_cost = route_cfg["move_cost"];
	foreach(config const& loc_cfg, route_cfg.child_range("step")) {
		route_->steps.push_back(map_location(loc_cfg["x"],loc_cfg["y"]));
	}
	foreach(config const& mark_cfg, route_cfg.child_range("mark")) {
		route_->marks[map_location(mark_cfg["x"],mark_cfg["y"])]
				= pathfind::marked_route::mark(mark_cfg["turns"],mark_cfg["pass_here"],mark_cfg["zoc"],mark_cfg["capture"],mark_cfg["invisible"]);
	}

	// Validate route_ some more
	std::vector<map_location> const& steps = route_->steps;
	if(steps.empty())
		throw action::ctor_err("move: Invalid route_");

	// Construct arrow_
	arrow_->set_color(team::get_side_color_index(side_number()));
	arrow_->set_style(arrow::STYLE_STANDARD);
	arrow_->set_path(route_->steps);

	// Construct fake_unit_
	fake_unit_.reset(new unit(*unit_),wb::fake_unit_deleter());
	if(hidden)
		fake_unit_->set_hidden(true);
	resources::screen->place_temporary_unit(fake_unit_.get());
	fake_unit_->set_ghosted(false);
	unit_display::move_unit(route_->steps, *fake_unit_, *resources::teams, false); //get facing right
	fake_unit_->set_location(route_->steps.back());

	this->init();
}

void move::init()
{
	/// @todo Why do we even have the unit_id_ field?
	assert(unit_);
	unit_id_ = unit_->id();

	//This action defines the future position of the unit, make its fake unit more visible
	//than previous actions' fake units
	if (fake_unit_)
	{
		fake_unit_->set_ghosted(true);
	}
	side_actions_ptr side_actions = resources::teams->at(team_index()).get_side_actions();
	side_actions::iterator action = side_actions->find_last_action_of(unit_);
	if (action != side_actions->end())
	{
		if (move_ptr move = boost::dynamic_pointer_cast<class move>(*action))
		{
			if (move->fake_unit_)
				move->fake_unit_->set_disabled_ghosted(false);
		}
	}

	this->calculate_move_cost();

	// Initialize arrow_brightness_ and arrow_texture_ using arrow_->style_
	arrow::STYLE arrow_style = arrow_->get_style();
	if(arrow_style == arrow::STYLE_STANDARD)
	{
		arrow_brightness_ = ARROW_BRIGHTNESS_STANDARD;
		arrow_texture_ = ARROW_TEXTURE_VALID;
	}
	else if(arrow_style == arrow::STYLE_HIGHLIGHTED)
	{
		arrow_brightness_ = ARROW_BRIGHTNESS_HIGHLIGHTED;
		arrow_texture_ = ARROW_TEXTURE_VALID;
	}
	else if(arrow_style == arrow::STYLE_FOCUS)
	{
		arrow_brightness_ = ARROW_BRIGHTNESS_FOCUS;
		arrow_texture_ = ARROW_TEXTURE_VALID;
	}
	else if(arrow_style == arrow::STYLE_FOCUS_INVALID)
	{
		arrow_brightness_ = ARROW_BRIGHTNESS_STANDARD;
		arrow_texture_ = ARROW_TEXTURE_INVALID;
	}
}

move::~move()
{
	// The ghost of the last fake unit in a chain of planned actions is supposed to look different
	// If the last remaining action of the unit that owned this move is a move as well,
	// adjust its appearance accordingly.
	if (resources::teams)
	{
		side_actions_ptr side_actions = resources::teams->at(team_index()).get_side_actions();

		side_actions::iterator action_it = side_actions->find_last_action_of(unit_);
		if (action_it != side_actions->end())
		{
			if (move_ptr move = boost::dynamic_pointer_cast<class move>(*action_it))
			{
				if (move->fake_unit_)
					move->fake_unit_->set_ghosted(true);
			}
		}

	}

	//reminder: here we rely on the ~arrow destructor to invalidate
	//its whole path.
}


void move::accept(visitor& v)
{
	v.visit_move(shared_from_this());
}

action::EXEC_RESULT move::execute()
{
	if (!valid_)
		return action::FAIL;

	if (get_source_hex() == get_dest_hex())
		return action::SUCCESS; //zero-hex move, used by attack subclass

	//Ensure destination hex is free
	if (get_visible_unit(get_dest_hex(),resources::teams->at(viewer_team())) != NULL)
		return action::FAIL;

	LOG_WB << "Executing: " << shared_from_this() << "\n";

	action::EXEC_RESULT result = action::FAIL;

	set_arrow_brightness(ARROW_BRIGHTNESS_HIGHLIGHTED);
	hide_fake_unit();

	events::mouse_handler const& mouse_handler = resources::controller->get_mouse_handler_base();
	std::set<map_location> adj_enemies = mouse_handler.get_adj_enemies(get_dest_hex(), side_number());

	map_location final_location;
	bool steps_finished;
	bool enemy_sighted;
	{
		events::mouse_handler& mouse_handler = resources::controller->get_mouse_handler_base();
		team const& owner_team = resources::teams->at(team_index());
		try {
			steps_finished = mouse_handler.move_unit_along_route(*route_, &final_location, owner_team.auto_shroud_updates(), &enemy_sighted);
		} catch (end_turn_exception&) {
			set_arrow_brightness(ARROW_BRIGHTNESS_STANDARD);
			throw; // we rely on the caller to delete this action
		}
		// final_location now contains the final unit location
		// if that isn't needed, pass NULL rather than &final_location
	}

	unit_map::const_iterator unit_it;

	if (final_location == route_->steps.front())
	{
		WRN_WB << "Move execution resulted in zero movement.\n";
	}
	else if (final_location.valid() &&
			(unit_it = resources::units->find(final_location)) != resources::units->end()
			&& unit_it->id() == unit_id_)
	{
		if (steps_finished && route_->steps.back() == final_location) //reached destination
		{
			//check if new enemies are now visible
			if(enemy_sighted
					|| mouse_handler.get_adj_enemies(final_location,side_number()) != adj_enemies)
			{
				LOG_WB << "Move completed, but interrupted on final hex. Halting.\n";
				//reset to a single-hex path, just in case *this is a wb::attack
				arrow_.reset();
				route_->steps = std::vector<map_location>(1,route_->steps.back());
				result = action::PARTIAL;
			}
			else // Everything went smoothly
				result = action::SUCCESS;
		}
		else // Move was interrupted, probably by enemy unit sighted
		{
			LOG_WB << "Move finished at (" << final_location << ") instead of at (" << get_dest_hex() << "), analyzing\n";
			std::vector<map_location>::iterator start_new_path;
			bool found = false;
			for (start_new_path = route_->steps.begin(); ((start_new_path != route_->steps.end()) && !found); ++start_new_path)
			{
				if (*start_new_path == final_location)
				{
					found = true;
				}
			}
			if (found)
			{
				get_source_hex() = final_location;
				--start_new_path; //since the for loop incremented the iterator once after we found the right one.
				std::vector<map_location> new_path(start_new_path, route_->steps.end());
				LOG_WB << "Setting new path for this move from (" << new_path.front()
						<< ") to (" << new_path.back() << ").\n";
				//FIXME: probably better to use the new calculate_new_route instead of doing this
				route_->steps = new_path;
				arrow_->set_path(new_path);
			}
			else //Unit ended up in location outside path, likely due to a WML event
			{
				result = action::SUCCESS;
				WRN_WB << "Unit ended up in location outside path during move execution.\n";
			}
		}
	}
	else //Unit disappeared from the map, likely due to a WML event
	{
		result = action::SUCCESS;
		WRN_WB << "Unit disappeared from map during move execution.\n";
	}

	if(result == action::FAIL)
	{
		set_arrow_brightness(ARROW_BRIGHTNESS_STANDARD);
		show_fake_unit();
	}
	return result;
}

map_location move::get_source_hex() const
{
	assert(route_ && !route_->steps.empty());
	return route_->steps.front();
}

map_location move::get_dest_hex() const
{
	assert(route_ && !route_->steps.empty());
	return route_->steps.back();
}

void move::set_route(const pathfind::marked_route& route)
{
	route_.reset(new pathfind::marked_route(route));
	this->calculate_move_cost();
	arrow_->set_path(route_->steps);
}

bool move::calculate_new_route(const map_location& source_hex, const map_location& dest_hex)
{
	pathfind::plain_route new_plain_route;
	pathfind::shortest_path_calculator path_calc(*get_unit(),
						resources::teams->at(team_index()), *resources::units,
						*resources::teams, *resources::game_map);
	new_plain_route = pathfind::a_star_search(source_hex,
						dest_hex, 10000, &path_calc, resources::game_map->w(), resources::game_map->h());
	if (new_plain_route.move_cost >= path_calc.getNoPathValue()) return false;
	route_.reset(new pathfind::marked_route(pathfind::mark_route(new_plain_route, std::vector<map_location>())));
	calculate_move_cost();
	return true;
}

void move::apply_temp_modifier(unit_map& unit_map)
{
	if (get_source_hex() == get_dest_hex())
		return; //zero-hex move, used by attack subclass

	//@todo: deal with multi-turn moves, which may for instance end their first turn
	// by capturing a village

	//@todo: we may need to change unit status here and change it back in remove_temp_modifier

	unit_map::iterator unit_it = unit_map.find(get_source_hex());
	assert(unit_it != unit_map.end());

	unit& unit = *unit_it;
	//Modify movement points
	DBG_WB <<"Move: Changing movement points for unit " << unit.name() << " [" << unit.id()
			<< "] from " << unit.movement_left() << " to "
			<< unit.movement_left() - movement_cost_ << ".\n";
	unit.set_movement(unit.movement_left() - movement_cost_);

	// Move the unit
	DBG_WB << "Move: Temporarily moving unit " << unit.name() << " [" << unit.id()
			<< "] from (" << get_source_hex() << ") to (" << get_dest_hex() <<")\n";
	mover_.reset(new temporary_unit_mover(unit_map,get_source_hex(), get_dest_hex()));
}

void move::remove_temp_modifier(unit_map&)
{
	if (get_source_hex() == get_dest_hex())
		return; //zero-hex move, probably used by attack subclass

	unit_map::iterator unit_it = resources::units->find(get_dest_hex());
	assert(unit_it != resources::units->end());
	unit& unit = *unit_it;

	// Restore movement points
	DBG_WB << "Move: Changing movement points for unit " << unit.name() << " [" << unit.id()
				<< "] from " << unit.movement_left() << " to "
				<< unit.movement_left() + movement_cost_ << ".\n";
	unit.set_movement(unit.movement_left() + movement_cost_);

	// Restore the unit to its original position
	mover_.reset();
}

void move::draw_hex(const map_location& hex)
{
	(void) hex; //temporary to avoid unused param warning
}

void move::do_hide()
{
	arrow_->hide();
	if(!fake_unit_hidden_)
		fake_unit_->set_hidden(true);
}

void move::do_show()
{
	arrow_->show();
	if(!fake_unit_hidden_)
		fake_unit_->set_hidden(false);
}

void move::hide_fake_unit()
{
	fake_unit_hidden_ = true;
	if(!hidden())
		fake_unit_->set_hidden(true);
}

void move::show_fake_unit()
{
	fake_unit_hidden_ = false;
	if(!hidden())
		fake_unit_->set_hidden(false);
}

map_location move::get_numbering_hex() const
{
	return get_dest_hex();
}

void move::set_valid(bool valid)
{
	if(valid_ != valid)
	{
		valid_ = valid;
		if(valid_)
			set_arrow_texture(ARROW_TEXTURE_VALID);
		else
			set_arrow_texture(ARROW_TEXTURE_INVALID);
	}
}

///@todo Find a better way to serialize unit_ because underlying_id isn't cutting it
config move::to_config() const
{
	config final_cfg = action::to_config();

	final_cfg["type"]="move";
	final_cfg["unit_"]=static_cast<int>(unit_->underlying_id());
//	final_cfg["movement_cost_"]=movement_cost_; //Unnecessary
//	final_cfg["unit_id_"]=unit_id_; //Unnecessary

	//Serialize route_
	config route_cfg;
	route_cfg["move_cost"]=route_->move_cost;
	foreach(map_location const& loc, route_->steps)
	{
		config loc_cfg;
		loc_cfg["x"]=loc.x;
		loc_cfg["y"]=loc.y;
		route_cfg.add_child("step",loc_cfg);
	}
	typedef std::pair<map_location,pathfind::marked_route::mark> pair_loc_mark;
	foreach(pair_loc_mark const& item, route_->marks)
	{
		config mark_cfg;
		mark_cfg["x"]=item.first.x;
		mark_cfg["y"]=item.first.y;
		mark_cfg["turns"]=item.second.turns;
		mark_cfg["pass_here"]=item.second.pass_here;
		mark_cfg["zoc"]=item.second.zoc;
		mark_cfg["capture"]=item.second.capture;
		mark_cfg["invisible"]=item.second.invisible;
		route_cfg.add_child("mark",mark_cfg);
	}
	final_cfg.add_child("route_",route_cfg);

	return final_cfg;
}

void move::calculate_move_cost()
{
	assert(unit_);
	assert(route_);
	if (get_source_hex().valid() && get_dest_hex().valid() && get_source_hex() != get_dest_hex())
	{

		// @todo: find a better treatment of movement points when defining moves out-of-turn
		if(get_unit()->movement_left() - route_->move_cost < 0
				&& resources::controller->current_side() == resources::screen->viewing_side()) {
			WRN_WB << "Move defined with insufficient movement left.\n";
		}

		// If unit finishes move in a village it captures, set the move cost to unit_.movement_left()
		 if (route_->marks[get_dest_hex()].capture)
		 {
			 movement_cost_ = get_unit()->movement_left();
		 }
		 else
		 {
			 movement_cost_ = route_->move_cost;
		 }
	}
}

//If you add more arrow styles, this will need to change
/* private */
void move::update_arrow_style()
{
	if(arrow_texture_ == ARROW_TEXTURE_INVALID)
	{
		arrow_->set_style(arrow::STYLE_FOCUS_INVALID);
		return;
	}

	switch(arrow_brightness_)
	{
	case ARROW_BRIGHTNESS_STANDARD:
		arrow_->set_style(arrow::STYLE_STANDARD);
		break;
	case ARROW_BRIGHTNESS_HIGHLIGHTED:
		arrow_->set_style(arrow::STYLE_HIGHLIGHTED);
		break;
	case ARROW_BRIGHTNESS_FOCUS:
		arrow_->set_style(arrow::STYLE_FOCUS);
		break;
	}
}

} // end namespace wb
