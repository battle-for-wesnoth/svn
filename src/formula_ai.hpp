/* $Id$ */
/*
   Copyright (C) 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef FORMULA_AI_HPP_INCLUDED
#define FORMULA_AI_HPP_INCLUDED

#include "ai.hpp"
#include "ai_interface.hpp"
#include "formula_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"

// Forward declaration needed for ai function symbol table
class formula_ai;

namespace game_logic {

typedef	std::map<const std::string, const_formula_ptr> candidate_move_map;

class ai_function_symbol_table : public function_symbol_table {

public:
	explicit ai_function_symbol_table(formula_ai& ai) : 
		ai_(ai),
		move_functions(),
		candidate_move_evals()
	{
	}

	void register_candidate_move(const std::string name, 
			const_formula_ptr formula, const_formula_ptr eval, 
			const_formula_ptr precondition, const std::vector<std::string>& args);

private:
	formula_ai& ai_;
	std::set<std::string> move_functions;
	candidate_move_map candidate_move_evals;
	expression_ptr create_function(const std::string& fn,
	                               const std::vector<expression_ptr>& args) const; 
};

}

class formula_ai : public ai {
public:
	explicit formula_ai(info& i);
        virtual ~formula_ai() {} ;
	virtual void play_turn();
	virtual void new_turn();

	using ai_interface::get_info;
	using ai_interface::current_team;
	using ai_interface::move_map;

	const move_map& srcdst() const { if(!move_maps_valid_) { prepare_move(); } return srcdst_; }

	std::string evaluate(const std::string& formula_str);

	struct move_map_backup {
		move_map_backup() : 
			move_maps_valid(false),
			srcdst(),
			dstsrc(),
			full_srcdst(),
			full_dstsrc(),
			enemy_srcdst(),
			enemy_dstsrc(),
			attacks_cache()
		{
		}

		bool move_maps_valid;
		move_map srcdst, dstsrc, full_srcdst, full_dstsrc, enemy_srcdst, enemy_dstsrc;
		variant attacks_cache;
	};

        // If the AI manager should manager the AI once constructed.
        virtual bool manager_manage_ai() const { return true ; } ;

	void swap_move_map(move_map_backup& backup);

	variant get_keeps() const;

	const variant& get_keeps_cache() const { return keeps_cache_; }

private:
	void do_recruitment();
	bool make_move(game_logic::const_formula_ptr formula_, const game_logic::formula_callable& variables);
	bool execute_variant(const variant& var, bool commandline=false);
	virtual variant get_value(const std::string& key) const;
	virtual void get_inputs(std::vector<game_logic::formula_input>* inputs) const;
	game_logic::const_formula_ptr recruit_formula_;
	game_logic::const_formula_ptr move_formula_;

	mutable std::map<location,paths> possible_moves_;

	void prepare_move() const;
	mutable bool move_maps_valid_;
	mutable move_map srcdst_, dstsrc_, full_srcdst_, full_dstsrc_, enemy_srcdst_, enemy_dstsrc_;
	mutable variant attacks_cache_;
	mutable variant keeps_cache_;

	game_logic::map_formula_callable vars_;
	game_logic::ai_function_symbol_table function_table;
};

#endif

