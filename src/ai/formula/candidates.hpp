/*
   Copyright (C) 2009 by Bartosz Waresiak <dragonking@o2.pl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai/formula/candidates.hpp
 * Defines formula ai candidate actions - headers
 * */

#ifndef AI_FORMULA_CANDIDATES_HPP_INCLUDED
#define	AI_FORMULA_CANDIDATES_HPP_INCLUDED

#include "../../global.hpp"

#include "../../formula_fwd.hpp"
#include "../../formula_function.hpp"

namespace game_logic {

class base_candidate_action;

typedef std::map< std::string, game_logic::const_formula_ptr > candidate_action_filters;
typedef boost::shared_ptr<game_logic::base_candidate_action> candidate_action_ptr;

//every new candidate action type should be derived from this class
//and should complete evaluate and update_callable_map methods
class base_candidate_action {
public:
	base_candidate_action(const std::string& name, const std::string& type,const config& cfg, function_symbol_table* function_table);

	virtual ~base_candidate_action() {}

	//evaluate candidate action using eval_ formula
	virtual void evaluate(formula_ai* /*ai*/, unit_map& /*units*/) {}

	//adds needed callable objects to callable map
	virtual void update_callable_map(game_logic::map_formula_callable& /*callable*/) {}


	//return score of last evaluation
	int get_score() const {return score_;}

	const_formula_ptr& get_action() {return action_;}

	const std::string& get_name() { return name_;}
	const std::string& get_type() { return type_;}

protected:
	int execute_formula(const const_formula_ptr& formula,
			const game_logic::formula_callable& callable, const formula_ai* ai);

	std::string name_;
	std::string type_;
	const_formula_ptr eval_;
	const_formula_ptr action_;
	int score_;
};

struct candidate_action_compare {
	bool operator() (const candidate_action_ptr laction,
			const candidate_action_ptr raction) const
	{
		return laction->get_score() > raction->get_score();
	}
};

typedef std::set<game_logic::candidate_action_ptr, game_logic::candidate_action_compare> candidate_action_set;

//this class is responsible for managing candidate actions
class candidate_action_manager {
public:
	candidate_action_manager()
		: evaluated_candidate_actions_()
		, candidate_actions_()
	{}

	//register candidate actions from config
	void load_config(const config& cfg, formula_ai* ai, function_symbol_table* function_table);

	//evaluate candidate action, return true if we have candidate action that have score > 0
	bool evaluate_candidate_actions(formula_ai* ai, unit_map& units);

	const_formula_ptr get_best_action_formula() {
		if( evaluated_candidate_actions_.empty() )
			return game_logic::formula_ptr();
		return (*evaluated_candidate_actions_.begin())->get_action();
	}

	//calls same method from best candidate action
	void update_callable_map(game_logic::map_formula_callable& callable){
		if( evaluated_candidate_actions_.empty() )
			return;
		(*evaluated_candidate_actions_.begin())->update_callable_map(callable);
	}

	void register_candidate_action(candidate_action_ptr& candidate_action){
		candidate_actions_.push_back(candidate_action);
	}

	bool has_candidate_actions() { return !candidate_actions_.empty(); }

	void clear() {
		candidate_actions_.clear();
		evaluated_candidate_actions_.clear();
	}

private:
	game_logic::candidate_action_set evaluated_candidate_actions_;
	std::vector<candidate_action_ptr> candidate_actions_;
};


class candidate_action_with_filters : public base_candidate_action {
public:
	candidate_action_with_filters(const std::string& name, const std::string& type,const config& cfg, function_symbol_table* function_table);
protected:

	game_logic::candidate_action_filters filter_map_;
};

class move_candidate_action : public candidate_action_with_filters {
public:
	move_candidate_action(const std::string& name, const std::string& type,const config& cfg, function_symbol_table* function_table);

	virtual void evaluate(formula_ai* ai, unit_map& units);

	virtual void update_callable_map(game_logic::map_formula_callable& callable);

protected:
	unit_map::unit_iterator my_unit_;
};

class attack_candidate_action : public candidate_action_with_filters {
public:
	attack_candidate_action(const std::string& name, const std::string& type,const config& cfg, function_symbol_table* function_table);

	virtual void evaluate(formula_ai* ai, unit_map& units);

	virtual void update_callable_map(game_logic::map_formula_callable& callable);
protected:
	unit_map::const_unit_iterator my_unit_;
	unit_map::const_unit_iterator enemy_unit_;
};

class support_candidate_action : public candidate_action_with_filters {
public:
	support_candidate_action(const std::string& name, const std::string& type,const config& cfg, function_symbol_table* function_table);
};
}

#endif	/* _FORMULA_CANDIDATES_HPP */

