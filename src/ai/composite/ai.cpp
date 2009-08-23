/* $Id$ */
/*
   Copyright (C) 2009 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * Composite AI with turn sequence which is a vector of stages
 * @file ai/composite/ai.cpp
 */

#include "ai.hpp"
#include "stage.hpp"
#include "../manager.hpp"
#include "../../foreach.hpp"
#include "../../log.hpp"

namespace ai {

static lg::log_domain log_ai_composite("ai/composite");
#define DBG_AI_COMPOSITE LOG_STREAM(debug, log_ai_composite)
#define LOG_AI_COMPOSITE LOG_STREAM(info, log_ai_composite)
#define ERR_AI_COMPOSITE LOG_STREAM(err, log_ai_composite)

// =======================================================================
// COMPOSITE AI
// =======================================================================
std::string ai_composite::describe_self(){
	return "[composite_ai]";
}

ai_composite::ai_composite( default_ai_context &context, const config &cfg)
	: cfg_(cfg),stages_(),recursion_counter_(context.get_recursion_count())
{
	init_default_ai_context_proxy(context);
}

void ai_composite::on_create()
{
	LOG_AI_COMPOSITE << "side "<< get_side() << " : "<<" created AI with id=["<<
		cfg_["id"]<<"]"<<std::endl;

	// init the composite ai stages
	foreach(const config &cfg_element, cfg_.child_range("stage")){
		engine::parse_stage_from_config(*this,cfg_element,std::back_inserter(stages_));
	}

	config cfg;
	cfg["engine"] = "fai";
	engine_ptr e_ptr = get_engine(cfg);
	if (e_ptr) {
		e_ptr->set_ai_context(this);
	}

}

ai_composite::~ai_composite()
{
}


void ai_composite::play_turn(){
	foreach(stage_ptr &s, stages_){
		s->play_stage();
	}
}


std::string ai_composite::evaluate(const std::string& str)
{
	config cfg;
	cfg["engine"] = "fai";//@todo 1.9 : consider allowing other engines to evaluate
	engine_ptr e_ptr = get_engine(cfg);
	if (!e_ptr) {
		return interface::evaluate(str);
	}
	return e_ptr->evaluate(str);
}


void ai_composite::new_turn()
{
	//@todo 1.7 replace with event system
	recalculate_move_maps();
	invalidate_defensive_position_cache();
	invalidate_keeps_cache();
	unit_stats_cache().clear();
}


int ai_composite::get_recursion_count() const
{
	return recursion_counter_.get_count();
}

void ai_composite::switch_side(side_number side)
{
	set_side(side);
}

ai_context& ai_composite::get_ai_context()
{
	return *this;
}


config ai_composite::to_config() const
{
	config cfg;

	//serialize the composite ai stages
	foreach(const stage_ptr &s, stages_){
		cfg.add_child("stage",s->to_config());
	}

	return cfg;
}

component* ai_composite::get_child(const path_element &child)
{
	//@todo 1.7.4 implement
	if (child.property=="aspect") {
		//ASPECT
		if (child.id.empty()) {
			return NULL;
		}

		aspect_map::const_iterator a = get_aspects().find(child.id);
		if (a==get_aspects().end()){
			return NULL;
		} else {
			return &*a->second;
		}
	} else if (child.property=="stage") {
		//STAGE
		if (child.id.empty()) {
			if ( (child.position<0) || (child.position>=static_cast<int>(stages_.size())) ) {
				return NULL;
			} else {
				//@todo 1.7.4 implement
				return NULL;
				//return &stages_.at(pos);
			}
		}
		//foreach(const stage_ptr &s, stages_) {
			//if (s->get_id()==child.id) {
				//@todo 1.7.4 implement
				//return &*s;
				//}
				//}
		return NULL;
	} else if (child.property=="engine") {
		//ENGINE
		return NULL;
	} else if (child.property=="goal") {
		//GOAL
		return NULL;
	}

	//OOPS
	return NULL;
}


bool ai_composite::add_child(const path_element &child, const config &cfg)
{
	//@todo 1.7.4 implement
	return false;
}


bool ai_composite::change_child(const path_element &child, const config &cfg)
{
	//@todo 1.7.4 implement
	return false;
}


bool ai_composite::delete_child(const path_element &child)
{
	//@todo 1.7.4 implement
	return false;
}



} //end of namespace ai
