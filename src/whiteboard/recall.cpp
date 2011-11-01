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

#include "recall.hpp"

#include "manager.hpp"
#include "side_actions.hpp"
#include "utility.hpp"
#include "visitor.hpp"

#include "foreach.hpp"
#include "game_display.hpp"
#include "menu_events.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "unit.hpp"

namespace wb
{

std::ostream& operator<<(std::ostream& s, recall_ptr recall)
{
	assert(recall);
	return recall->print(s);
}
std::ostream& operator<<(std::ostream& s, recall_const_ptr recall)
{
	assert(recall);
	return recall->print(s);
}

std::ostream& recall::print(std::ostream &s) const
{
	s << "Recalling " << fake_unit_->name() << " [" << fake_unit_->id() << "] on hex " << recall_hex_;
	return s;
}

recall::recall(size_t team_index, bool hidden, const unit& unit, const map_location& recall_hex):
		action(team_index,hidden),
		temp_unit_(new class unit(unit)),
		recall_hex_(recall_hex),
		valid_(true),
		fake_unit_(new class game_display::fake_unit(unit) ),
		temp_cost_(0)
{
	this->init();
}

recall::recall(config const& cfg, bool hidden)
	: action(cfg,hidden)
	, temp_unit_(NULL)
	, recall_hex_(cfg.child("recall_hex_")["x"],cfg.child("recall_hex_")["y"])
	, valid_(true)
	, fake_unit_()
	, temp_cost_(0)
{
	// Construct and validate temp_unit_
	size_t underlying_id = cfg["temp_unit_"];
	foreach(unit const& recall_unit, resources::teams->at(team_index()).recall_list())
	{
		if(recall_unit.underlying_id()==underlying_id)
		{
			temp_unit_=new unit(recall_unit);
			break;
		}
	}
	if(temp_unit_==NULL)
		throw action::ctor_err("recall: Invalid underlying_id");

	fake_unit_.reset(new game_display::fake_unit(*temp_unit_) );

	this->init();
}

void recall::init()
{
	temp_unit_->set_movement(0);
	temp_unit_->set_attacks(0);

	fake_unit_->set_location(recall_hex_);
	fake_unit_->set_movement(0);
	fake_unit_->set_attacks(0);
	fake_unit_->set_ghosted(false);
	fake_unit_->place_on_game_display( resources::screen);
}

recall::~recall()
{
	delete temp_unit_;
}

void recall::accept(visitor& v)
{
	v.visit_recall(shared_from_this());
}

void recall::execute(bool& success, bool& complete)
{
	assert(valid_);
	assert(temp_unit_);
	temporary_unit_hider const raii(*fake_unit_);
	bool const result = resources::controller->get_menu_handler().do_recall(*temp_unit_, team_index() + 1, recall_hex_, map_location::null_location);
	success = complete = result;
}

void recall::apply_temp_modifier(unit_map& unit_map)
{
	assert(valid_);
	temp_unit_->set_location(recall_hex_);

	DBG_WB << "Inserting future recall " << temp_unit_->name() << " [" << temp_unit_->id()
			<< "] at position " << temp_unit_->get_location() << ".\n";

	//temporarily remove unit from recall list
	std::vector<unit>& recalls = resources::teams->at(team_index()).recall_list();
	std::vector<unit>::iterator it = std::find_if(recalls.begin(), recalls.end(),
					unit_comparator_predicate(*temp_unit_));
	assert(it != recalls.end());
	recalls.erase(it);

	// Temporarily insert unit into unit_map
	unit_map.insert(temp_unit_);
	//unit map takes ownership of temp_unit
	temp_unit_ = NULL;

	//Add cost to money spent on recruits.
	temp_cost_ = resources::teams->at(team_index()).recall_cost();
	resources::teams->at(team_index()).get_side_actions()->change_gold_spent_by(temp_cost_);

	// Update gold in top bar
	resources::screen->invalidate_game_status();
}

void recall::remove_temp_modifier(unit_map& unit_map)
{
	temp_unit_ = unit_map.extract(recall_hex_);
	assert(temp_unit_);

	//Put unit back into recall list
	resources::teams->at(team_index()).recall_list().push_back(*temp_unit_);

	/*
	 * Remove cost from money spent on recruits.
	 */
	resources::teams->at(team_index()).get_side_actions()->change_gold_spent_by(-temp_cost_);
	temp_cost_ = 0;
	resources::screen->invalidate_game_status();
}

void recall::draw_hex(map_location const& hex)
{
	if (hex == recall_hex_)
	{
		const double x_offset = 0.5;
		const double y_offset = 0.7;
		//position 0,0 in the hex is the upper left corner
		std::stringstream number_text;
		number_text << utils::unicode_minus << resources::teams->at(team_index()).recall_cost();
		size_t font_size = 16;
		SDL_Color color; color.r = 255; color.g = 0; color.b = 0; //red
		resources::screen->draw_text_in_hex(hex, display::LAYER_ACTIONS_NUMBERING,
						number_text.str(), font_size, color, x_offset, y_offset);
	}
}

///@todo Find a better way to serialize unit_ because underlying_id isn't cutting it
config recall::to_config() const
{
	config final_cfg = action::to_config();

	final_cfg["type"] = "recall";
	final_cfg["temp_unit_"] = static_cast<int>(temp_unit_->underlying_id());
//	final_cfg["temp_cost_"] = temp_cost_; //Unnecessary

	config loc_cfg;
	loc_cfg["x"]=recall_hex_.x;
	loc_cfg["y"]=recall_hex_.y;
	final_cfg.add_child("recall_hex_",loc_cfg);

	return final_cfg;
}

void recall::do_hide() {fake_unit_->set_hidden(true);}
void recall::do_show() {fake_unit_->set_hidden(false);}

} //end namespace wb
