/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai/formula_ai.cpp
 * Defines formula ai candidate actions - headers
 * */


#include <boost/lexical_cast.hpp>
#include <stack>
#include <vector>

#include "ai.hpp"
#include "candidates.hpp"
#include "callable_objects.hpp"
#include "function_table.hpp"

#include "../manager.hpp"

#include "../../foreach.hpp"
#include "../../log.hpp"
#include "../../menu_events.hpp"

static lg::log_domain log_formula_ai("ai/formula_ai");
#define LOG_AI LOG_STREAM(info, log_formula_ai)
#define WRN_AI LOG_STREAM(warn, log_formula_ai)
#define ERR_AI LOG_STREAM(err, log_formula_ai)


using namespace game_logic;

namespace ai {

game_logic::candidate_action_ptr formula_ai::load_candidate_action_from_config(const config& cfg)
{
	return candidate_action_manager_.load_candidate_action_from_config(cfg,this,&function_table);
}

std::string formula_ai::describe_self(){
	return "[formula_ai]";
}

int formula_ai::get_recursion_count() const{
	return recursion_counter_.get_count();
}


formula_ai::formula_ai(default_ai_context &context, const config &cfg) :
	ai_default(context,cfg),
	cfg_(cfg),
	recursion_counter_(context.get_recursion_count()),
	recruit_formula_(),
	move_formula_(),
	outcome_positions_(),
	possible_moves_(),
	move_maps_valid_(false),
	srcdst_(),
	dstsrc_(),
	full_srcdst_(),
	full_dstsrc_(),
	enemy_srcdst_(),
	enemy_dstsrc_(),
	attacks_cache_(),
	keeps_cache_(),
	infinite_loop_guardian_(),
	vars_(),
	function_table(*this),
	candidate_action_manager_()
{
	add_ref();
}

void formula_ai::handle_exception(game_logic::formula_error& e) const
{
	handle_exception(e, "Error while parsing formula");
}

void formula_ai::handle_exception(game_logic::formula_error& e, const std::string& failed_operation) const
{
	LOG_AI << failed_operation << ": " << e.formula << std::endl;
	display_message(failed_operation + ": " + e.formula);
	//if line number = 0, don't display info about filename and line number
	if (e.line != 0) {
		LOG_AI << e.type << " in " << e.filename << ":" << e.line << std::endl;
		display_message(e.type + " in " + e.filename + ":" + boost::lexical_cast<std::string>(e.line));
	} else {
		LOG_AI << e.type << std::endl;
		display_message(e.type);
	}
}

void formula_ai::display_message(const std::string& msg) const
{
	get_info().disp.add_chat_message(time(NULL), "fai", get_side(), msg,
				events::chat_handler::MESSAGE_PUBLIC, false);

}

formula_ptr formula_ai::create_optional_formula(const std::string& formula_string){
	try{
		return game_logic::formula::create_optional_formula(formula_string, &function_table);
	}
	catch(formula_error& e) {
		handle_exception(e);
		return game_logic::formula_ptr();
	}
}

void formula_ai::new_turn()
{
	move_maps_valid_ = false;
	ai_default::new_turn();
}

void formula_ai::play_turn()
{
	//execute units formulas first

        unit_formula_set units_with_formulas;

	for(unit_map::unit_iterator i = units_.begin() ; i != units_.end() ; ++i)
	{
            if ( (i->second.side() == get_side())  )
            {
                if ( i->second.has_formula() || i->second.has_loop_formula()) {
                    int priority = 0;
                    if( i->second.has_priority_formula() ) {
                        try {
                            game_logic::const_formula_ptr priority_formula(new game_logic::formula(i->second.get_priority_formula(), &function_table));
                            game_logic::map_formula_callable callable(this);
                            callable.add_ref();
                            callable.add("me", variant(new unit_callable(*i)));
                            priority = (formula::evaluate(priority_formula, callable)).as_int();
                        } catch(formula_error& e) {
                                if(e.filename == "formula")
                                        e.line = 0;
                                handle_exception( e, "Unit priority formula error for unit: '" + i->second.type_id() + "' standing at (" + boost::lexical_cast<std::string>(i->first.x+1) + "," + boost::lexical_cast<std::string>(i->first.y+1) + ")");

                                priority = 0;
                        } catch(type_error& e) {
                                priority = 0;
                                ERR_AI << "formula type error while evaluating unit priority formula  " << e.message << "\n";
                        }
                    }

                    units_with_formulas.insert( unit_formula_pair( i, priority ) );
                }
            }
        }

	for(unit_formula_set::iterator pair_it = units_with_formulas.begin() ; pair_it != units_with_formulas.end() ; ++pair_it)
	{
            unit_map::iterator i = pair_it->first;

            if( i.valid() ) {

                if ( i->second.has_formula() ) {
                    try {
                            game_logic::const_formula_ptr formula(new game_logic::formula(i->second.get_formula(), &function_table));
                            game_logic::map_formula_callable callable(this);
                            callable.add_ref();
                            callable.add("me", variant(new unit_callable(*i)));
                            make_action(formula, callable);
                    }
                    catch(formula_error& e) {
                            if(e.filename == "formula")
                                    e.line = 0;
                            handle_exception( e, "Unit formula error for unit: '" + i->second.type_id() + "' standing at (" + boost::lexical_cast<std::string>(i->first.x+1) + "," + boost::lexical_cast<std::string>(i->first.y+1) + ")");
                    }
                }
            }

            if( i.valid() ) {
                if( i->second.has_loop_formula() )
                {
                        try {
                                game_logic::const_formula_ptr loop_formula(new game_logic::formula(i->second.get_loop_formula(), &function_table));
                                game_logic::map_formula_callable callable(this);
                                callable.add_ref();
                                callable.add("me", variant(new unit_callable(*i)));
                                while ( !make_action(loop_formula, callable).is_empty() && i.valid() ) {}
                        }
                        catch(formula_error& e) {
                                if(e.filename == "formula")
                                        e.line = 0;
                                handle_exception( e, "Unit loop formula error for unit: '" + i->second.type_id() + "' standing at (" + boost::lexical_cast<std::string>(i->first.x+1) + "," + boost::lexical_cast<std::string>(i->first.y+1) + ")");
                        }
                }
            }
	}

	try {
		if( candidate_action_manager_.has_candidate_actions() ) {
			move_maps_valid_ = false;
			while( candidate_action_manager_.evaluate_candidate_actions(this, units_) )
			{
				game_logic::map_formula_callable callable(this);
				callable.add_ref();

				candidate_action_manager_.update_callable_map( callable );

				const_formula_ptr move_formula(candidate_action_manager_.get_best_action_formula());

				make_action(move_formula, callable);

				move_maps_valid_ = false;
			}
		}
	}
	catch(formula_error& e) {
		if(e.filename == "formula")
			e.line = 0;
			handle_exception( e, "Formula error in RCA loop");
	}

	game_logic::map_formula_callable callable(this);
	callable.add_ref();
	try {
		while( !make_action(move_formula_,callable).is_empty() ) { }
	}
	catch(formula_error& e) {
		if(e.filename == "formula")
			e.line = 0;
			handle_exception( e, "Formula error");
	}

}

std::string formula_ai::evaluate(const std::string& formula_str)
{
	try{
		move_maps_valid_ = false;

		game_logic::formula f(formula_str, &function_table);

		game_logic::map_formula_callable callable(this);
		callable.add_ref();

		const variant v = f.execute(callable);

                outcome_positions_.clear();

		variant var = execute_variant(v, true );

		if (  !var.is_empty() )
			return "Made move: " + var.to_debug_string();

		return v.to_debug_string();
	}
	catch(formula_error& e) {
		e.line = 0;
		handle_exception(e);
		throw;
	}
}

void formula_ai::store_outcome_position(const variant& var)
{
    outcome_positions_.push_back(var);
}

void formula_ai::swap_move_map(move_map_backup& backup)
{
	std::swap(move_maps_valid_, backup.move_maps_valid);
	std::swap(backup.attacks_cache, attacks_cache_);
	backup.move_maps_valid = move_maps_valid_;
	backup.srcdst.swap(srcdst_);
	backup.dstsrc.swap(dstsrc_);
	backup.full_srcdst.swap(full_srcdst_);
	backup.full_dstsrc.swap(full_dstsrc_);
	backup.enemy_srcdst.swap(enemy_srcdst_);
	backup.enemy_dstsrc.swap(enemy_dstsrc_);
}

void formula_ai::prepare_move() const
{
	if(move_maps_valid_) {
		return;
	}

	possible_moves_.clear();
	srcdst_.clear();
	dstsrc_.clear();

	calculate_possible_moves(possible_moves_, srcdst_, dstsrc_, false);

	full_srcdst_.clear();
	full_dstsrc_.clear();

	std::map<location,paths> possible_moves_dummy;
	calculate_possible_moves(possible_moves_dummy, full_srcdst_, full_dstsrc_, false, true);

	enemy_srcdst_.clear();
	enemy_dstsrc_.clear();
	possible_moves_dummy.clear();
	calculate_possible_moves(possible_moves_dummy, enemy_srcdst_, enemy_dstsrc_, true);

	attacks_cache_ = variant();
	move_maps_valid_ = true;
}

variant formula_ai::make_action(game_logic::const_formula_ptr formula_, const game_logic::formula_callable& variables)
{
	if(!formula_) {
		if(get_recursion_count()<recursion_counter::MAX_COUNTER_VALUE) {
			LOG_AI << "Falling back to default AI.\n";
			ai_ptr fallback( manager::create_transient_ai(manager::AI_TYPE_DEFAULT, config(), this));
			if (fallback){
				fallback->play_turn();
			}
		}
		return variant();
	}

	move_maps_valid_ = false;

	LOG_AI << "do move...\n";
	const variant var = formula_->execute(variables);

        variant res = execute_variant(var);

        //remove outcome_positions
        outcome_positions_.clear();

	return res;
}

plain_route formula_ai::shortest_path_calculator(const map_location &src,
	const map_location &dst, unit_map::iterator &unit_it,
	std::set<map_location> & allowed_teleports) const
{
    map_location destination = dst;

    ::shortest_path_calculator calc(unit_it->second, current_team(), units_, get_info().teams, get_info().map);

    unit_map::const_iterator dst_un = units_.find(destination);

    map_location res;

    if( dst_un != units_.end() ) {
        //there is unit standing at dst, let's try to find free hex to move to
        const map_location::DIRECTION preferred = destination.get_relative_dir(src);

        int best_rating = 100;//smaller is better
        map_location adj[6];
        get_adjacent_tiles(destination,adj);

        for(size_t n = 0; n != 6; ++n) {
                if(map_.on_board(adj[n]) == false) {
                        continue;
                }

                if(units_.find(adj[n]) != units_.end()) {
                        continue;
                }

                static const size_t NDIRECTIONS = map_location::NDIRECTIONS;
                unsigned int difference = abs(int(preferred - n));
                if(difference > NDIRECTIONS/2) {
                        difference = NDIRECTIONS - difference;
                }

                const int rating = difference * 2;
                if(rating < best_rating || res.valid() == false) {
                       best_rating = rating;
                       res = adj[n];
                }
        }
    }

    if( res != map_location() ) {
        destination = res;
    }

	plain_route route = a_star_search(src, destination, 1000.0, &calc,
            get_info().map.w(), get_info().map.h(), &allowed_teleports);

    return route;
}

std::set<map_location> formula_ai::get_allowed_teleports(unit_map::iterator& unit_it) const {
    std::set<map_location> allowed_teleports;

	if (unit_it->second.get_ability_bool("teleport"))
	{
            for(std::set<map_location>::const_iterator i = current_team().villages().begin();
                            i != current_team().villages().end(); ++i) {
                    //if (viewing_team().is_enemy(unit_it->second.side()) && viewing_team().fogged(*i))
                    //        continue;

                    unit_map::const_iterator occupant = units_.find(*i);
                    if (occupant != units_.end() && occupant != unit_it)
                            continue;

                    allowed_teleports.insert(*i);
            }
    }

    return allowed_teleports;
}

map_location formula_ai::path_calculator(const map_location& src, const map_location& dst, unit_map::iterator& unit_it) const{
    std::map<map_location,paths>::iterator path = possible_moves_.find(src);

    map_location destination = dst;

    //check if destination is within unit's reach, if not, calculate where to move
	if (!path->second.destinations.contains(dst))
	{
            std::set<map_location> allowed_teleports = get_allowed_teleports(unit_it);
            //destination is too far, check where unit can go

		plain_route route = shortest_path_calculator( src, dst, unit_it, allowed_teleports );

            if( route.steps.size() == 0 ) {
                emergency_path_calculator em_calc(unit_it->second, get_info().map);

                route = a_star_search(src, dst, 1000.0, &em_calc, get_info().map.w(), get_info().map.h(), &allowed_teleports);

                if( route.steps.size() < 2 ) {
                    return map_location();
                }
            }

            destination = map_location();

            for (std::vector<map_location>::const_iterator loc_iter = route.steps.begin() + 1 ; loc_iter !=route.steps.end(); ++loc_iter) {
		typedef move_map::const_iterator Itor;
		std::pair<Itor,Itor> range = srcdst_.equal_range(src);

                bool found = false;
		for(Itor i = range.first; i != range.second; ++i) {
			if (i->second == *loc_iter ) {
                            found = true;
                            break;
                        }
		}
                if ( !found ) {
                    continue;
                }

                destination = *loc_iter;
            }
            return destination;
    }

    return destination;
}

//commandline=true when we evaluate formula from commandline, false otherwise (default)
variant formula_ai::execute_variant(const variant& var, bool commandline)
{
	std::stack<variant> vars;
	if(var.is_list()) {
		for(size_t n = 1; n <= var.num_elements() ; ++n) {
			vars.push(var[ var.num_elements() - n ]);
		}
	} else {
		vars.push(var);
	}

	std::vector<variant> made_moves;

	variant error;

	while( !vars.empty() ) {

		if(vars.top().is_null()) {
			vars.pop();
			continue;
		}

		variant action = vars.top();
		vars.pop();

		game_logic::safe_call_callable* safe_call = try_convert_variant<game_logic::safe_call_callable>(action);

		if(safe_call) {
		    action = safe_call->get_main();
		}

		const move_callable* move = try_convert_variant<move_callable>(action);
		const move_partial_callable* move_partial = try_convert_variant<move_partial_callable>(action);
		const attack_callable* attack = try_convert_variant<attack_callable>(action);
		const attack_analysis* _attack_analysis = try_convert_variant<attack_analysis>(action);
		const recruit_callable* recruit_command = try_convert_variant<recruit_callable>(action);
		const set_var_callable* set_var_command = try_convert_variant<set_var_callable>(action);
		const set_unit_var_callable* set_unit_var_command = try_convert_variant<set_unit_var_callable>(action);
		const fallback_callable* fallback_command = try_convert_variant<fallback_callable>(action);

		prepare_move();
		
		if( move || move_partial ) {
			move_result_ptr move_result;

			if(move)
				move_result = execute_move_action(move->src(), move->dst(), true);
			else
				move_result = execute_move_action(move_partial->src(), move_partial->dst(), false);

			if ( !move_result->is_ok() ) {
				if( move ) {
					LOG_AI << "ERROR #" << move_result->get_status() << " while executing 'move' formula function\n\n";

					if(safe_call) {
						//safe_call was called, prepare error information
						error = variant(new safe_call_result(move,
									move_result->get_status(), move_result->get_unit_location()));
					}
				} else {
					LOG_AI << "ERROR #" << move_result->get_status() << " while executing 'move_partial' formula function\n\n";

					if(safe_call) {
						//safe_call was called, prepare error information
						error = variant(new safe_call_result(move_partial,
									move_result->get_status(), move_result->get_unit_location()));
					}
				}
			}

			if( move_result->is_gamestate_changed() )
				made_moves.push_back(action);		
		} else if(attack) {
			bool gamestate_changed = false;
			move_result_ptr move_result;

			if( attack->move_from() != attack->src() ) {
				move_result = execute_move_action(attack->move_from(), attack->src(), false);
				gamestate_changed |= move_result->is_gamestate_changed();

				if (!move_result->is_ok()) {
					//move part failed
					LOG_AI << "ERROR #" << move_result->get_status() << " while executing 'attack' formula function\n\n";

					if(safe_call) {
						//safe_call was called, prepare error information
						error = variant(new safe_call_result(attack,
								move_result->get_status(), move_result->get_unit_location()));
					}
				}
			}

			if (!move_result || move_result->is_ok() ) {
				//if move wasn't done at all or was done successfully
				attack_result_ptr attack_result = execute_attack_action(attack->src(), attack->dst(), attack->weapon() );
				gamestate_changed |= attack_result->is_gamestate_changed();
				if (!attack_result->is_ok()) {
					//attack failed

					LOG_AI << "ERROR #" << attack_result->get_status() << " while executing 'attack' formula function\n\n";
					
					if(safe_call) {
						//safe_call was called, prepare error information
						error = variant(new safe_call_result(attack, attack_result->get_status()));
					}
				}
			}

			if (gamestate_changed) {
			      made_moves.push_back(action);
			}
		} else if(_attack_analysis) {
			//If we get an attack analysis back we will do the first attack.
			//Then the AI can get run again and re-choose.
			assert(_attack_analysis->movements.empty() == false);

			//make sure that unit which has to attack is at given position and is able to attack
			unit_map::const_iterator unit = units_.find(_attack_analysis->movements.front().first);
			if ( ( unit == units_.end() ) || (unit->second.attacks_left() == 0) )
				continue;

			const map_location& move_from = _attack_analysis->movements.front().first;
			const map_location& att_src = _attack_analysis->movements.front().second;
			const map_location& att_dst = _attack_analysis->target;

			//check if target is still valid
			unit = units_.find(att_dst);
			if ( unit == units_.end() )
				continue;

                        //check if we need to move
                        if( move_from != att_src ) {
                            //now check if location to which we want to move is still unoccupied
                            unit = units_.find(att_src);
                            if ( unit != units_.end() )
                                    continue;

                            move_unit(move_from, att_src, possible_moves_);
                        }

			if(get_info().units.count(att_src)) {
				battle_context bc(get_info().units,
				                  att_src, att_dst, -1, -1, 1.0, NULL,
								  &get_info().units.find(att_src)->second);
				attack_enemy(_attack_analysis->movements.front().second,
				             _attack_analysis->target,
							 bc.get_attacker_stats().attack_num,
							 bc.get_defender_stats().attack_num);
			}
			made_moves.push_back(action);
		} else if(recruit_command) {
			recruit_result_ptr recruit_result = execute_recruit_action(recruit_command->type(), recruit_command->loc());

			//is_ok()==true means that the action is successful (eg. no unexpected events)
			//is_ok() must be checked or the code will complain :)
			if (!recruit_result->is_ok()) {
				//get_status() can be used to fetch the error code
				LOG_AI << "ERROR #" <<recruit_result->get_status() << " while executing 'recruit' formula function\n"<<std::endl;

				if(safe_call) {
					//safe call was called, prepare error information
					error = variant(new safe_call_result(recruit_command,
									recruit_result->get_status()));
				}
			}
			//is_gamestate_changed()==true means that the game state was somehow changed by action.
			//it is believed that during a turn, a game state can change only a finite number of times
			if( recruit_result->is_gamestate_changed() )
				made_moves.push_back(action);

		} else if(set_var_command) {
			if( infinite_loop_guardian_.set_var_check() ) {
				LOG_AI << "Setting variable: " << set_var_command->key() << " -> " << set_var_command->value().to_debug_string() << "\n";
				vars_.add(set_var_command->key(), set_var_command->value());
				made_moves.push_back(action);
			} else {
				//too many calls in a row - possible infinite loop
				LOG_AI << "ERROR #" << 5001 << " while executing 'set_var' formula function\n";

				if( safe_call )
					error = variant(new safe_call_result(set_var_command, 5001));
			}
		} else if(set_unit_var_command) {
			int status = 0;
			unit_map::iterator unit;

			if( infinite_loop_guardian_.set_unit_var_check() ) {
			    status = 5001; //exceeded nmber of calls in a row - possible infinite loop
			} else if( (unit = units_.find(set_unit_var_command->loc())) == units_.end() ) {
			    status = 5002; //unit not found
			} else if( unit->second.side() != get_side() ) {
			    status = 5003;//unit does not belong to our side
			}

			if( status == 0 ){
				LOG_AI << "Setting unit variable: " << set_unit_var_command->key() << " -> " << set_unit_var_command->value().to_debug_string() << "\n";
				unit->second.add_formula_var(set_unit_var_command->key(), set_unit_var_command->value());
				made_moves.push_back(action);
			} else {
				LOG_AI << "ERROR #" << status << " while executing 'set_unit_var' formula function\n";
				if(safe_call)
				    error = variant(new safe_call_result(set_unit_var_command,
									status));
			}
			
		} else if( action.is_string() && action.as_string() == "recruit") {
			if( do_recruitment() )
				made_moves.push_back(action);
		} else if( action.is_string() && action.as_string() == "continue") {
			if( infinite_loop_guardian_.continue_check() ) {
				made_moves.push_back(action);
			} else {
				//too many calls in a row - possible infinite loop
				LOG_AI << "ERROR #" << 5001 << " while executing 'continue' formula keyword\n";

				if( safe_call )
					error = variant(new safe_call_result(NULL, 5001));
			}
		} else if( action.is_string() && (action.as_string() == "end_turn" || action.as_string() == "end" )  ) {
			return variant();
		} else if(fallback_command) {
			if(get_recursion_count()<recursion_counter::MAX_COUNTER_VALUE) {
				if(fallback_command->key() == "human")
				{
					//we want give control of the side to human for the rest of this turn
					throw fallback_ai_to_human_exception();
				} else
				{
					LOG_AI << "Explicit fallback to: " << fallback_command->key() << std::endl;
					ai_ptr fallback( manager::create_transient_ai(fallback_command->key(), config(), this));
					if(fallback) {
						fallback->play_turn();
					}
				}
			}
			return variant();
		} else {
			//this information is unneded when evaluating formulas form commandline
			if (!commandline) {
				ERR_AI << "UNRECOGNIZED MOVE: " << action.to_debug_string() << "\n";
			}
		}

		if( safe_call && (error != variant() || made_moves.empty() || made_moves.back() != action) ){
		    /*if we have safe_call formula and either error occured, or current action
		     *was not reckognized, then evaluate backup formula from safe_call and execute it
		     *during the next loop
		     */

			prepare_move();

			game_logic::map_formula_callable callable(this);
			callable.add_ref();

			if(error != variant())
				callable.add("error", error);

			variant backup_result = safe_call->get_backup()->evaluate(callable);

			if(backup_result.is_list()) {
				for(size_t n = 1; n <= backup_result.num_elements() ; ++n) {
					vars.push(backup_result[ backup_result.num_elements() - n ]);
				}
			} else {
				vars.push(backup_result);
			}

			//store the result in safe_call_callable case we would like to display it to the user
			//for example if this formula was executed from commandline	    
			safe_call->set_backup_result(backup_result);

			error = variant();
		}
	}

	return variant(&made_moves);
}


bool formula_ai::do_recruitment()
{
	if(!recruit_formula_) {
		return false;
	}

	bool ret = false;

	game_logic::map_formula_callable callable(this);
	callable.add_ref();
	try {
		while( !make_action(recruit_formula_,callable).is_empty() )
		{
		    ret = true;
		}
	}
	catch(formula_error& e) {
		if(e.filename == "recruit formula")
			e.line = 0;
			handle_exception( e, "Formula error");
	}

	return ret;
}

namespace {
template<typename Container>
variant villages_from_set(const Container& villages,
				          const std::set<map_location>* exclude=NULL) {
	std::vector<variant> vars;
	foreach(const map_location& loc, villages) {
		if(exclude && exclude->count(loc)) {
			continue;
		}
		vars.push_back(variant(new location_callable(loc)));
	}

	return variant(&vars);
}
}

variant formula_ai::get_value(const std::string& key) const
{
	if(key == "attacks")
	{
		prepare_move();
		if(attacks_cache_.is_null() == false) {
			return attacks_cache_;
		}

		const std::vector<attack_analysis> attacks = get_attacks();
		std::vector<variant> vars;
		for(std::vector<attack_analysis>::const_iterator i = attacks.begin(); i != attacks.end(); ++i) {
			vars.push_back(variant(new attack_analysis(*i)));
		}

		attacks_cache_ = variant(&vars);
		return attacks_cache_;

	} else if(key == "turn")
	{
		return variant(get_info().tod_manager_.turn());

	} else if(key == "time_of_day")
	{
		return variant(get_info().tod_manager_.get_time_of_day().id);

	} else if(key == "my_side")
	{
		return variant(new team_callable((get_info().teams)[get_side()-1]));

	} else if(key == "my_side_number")
	{
		return variant(get_side()-1);

	} else if(key == "teams")
	{
		std::vector<variant> vars;
		for(std::vector<team>::const_iterator i = get_info().teams.begin(); i != get_info().teams.end(); ++i) {
			vars.push_back(variant(new team_callable(*i)));
		}
		return variant(&vars);

	} else if(key == "allies")
	{
		std::vector<variant> vars;
		for( size_t i = 0; i < get_info().teams.size(); ++i) {
			if ( !current_team().is_enemy( i+1 ) )
				vars.push_back(variant( i ));
		}
		return variant(&vars);

	} else if(key == "enemies")
	{
		std::vector<variant> vars;
		for( size_t i = 0; i < get_info().teams.size(); ++i) {
			if ( current_team().is_enemy( i+1 ) )
				vars.push_back(variant( i ));
		}
		return variant(&vars);

	} else if(key == "my_recruits")
	{
		std::vector<variant> vars;

		unit_type_data::types().build_all(unit_type::FULL);

		const std::set<std::string>& recruits = current_team().recruits();
		if(recruits.size()==0)
			return variant( &vars );
		for(std::set<std::string>::const_iterator i = recruits.begin(); i != recruits.end(); ++i)
		{
			std::map<std::string,unit_type>::const_iterator unit_it = unit_type_data::types().find_unit_type(*i);
			if (unit_it != unit_type_data::types().end() )
			{
				vars.push_back(variant(new unit_type_callable(unit_it->second) ));
			}
		}
		return variant( &vars );

	} else if(key == "recruits_of_side")
	{
		std::vector<variant> vars;
		std::vector< std::vector< variant> > tmp;

		unit_type_data::types().build_all(unit_type::FULL);

		for( size_t i = 0; i<get_info().teams.size(); ++i)
		{
			std::vector<variant> v;
			tmp.push_back( v );

			const std::set<std::string>& recruits = get_info().teams[i].recruits();
			if(recruits.size()==0)
				continue;
			for(std::set<std::string>::const_iterator str_it = recruits.begin(); str_it != recruits.end(); ++str_it)
			{
				std::map<std::string,unit_type>::const_iterator unit_it = unit_type_data::types().find_unit_type(*str_it);
				if (unit_it != unit_type_data::types().end() )
				{
					tmp[i].push_back(variant(new unit_type_callable(unit_it->second) ));
				}
			}
		}

		for( size_t i = 0; i<tmp.size(); ++i)
			vars.push_back( variant( &tmp[i] ));
		return variant(&vars);

	} else if(key == "units")
	{
		std::vector<variant> vars;
		for(unit_map::const_iterator i = get_info().units.begin(); i != get_info().units.end(); ++i) {
			vars.push_back(variant(new unit_callable(*i)));
		}
		return variant(&vars);

	} else if(key == "units_of_side")
	{
		std::vector<variant> vars;
		std::vector< std::vector< variant> > tmp;
		for( size_t i = 0; i<get_info().teams.size(); ++i)
		{
			std::vector<variant> v;
			tmp.push_back( v );
		}
		for(unit_map::const_iterator i = get_info().units.begin(); i != get_info().units.end(); ++i) {
			tmp[ i->second.side()-1 ].push_back( variant(new unit_callable(*i)) );
		}
		for( size_t i = 0; i<tmp.size(); ++i)
			vars.push_back( variant( &tmp[i] ));
		return variant(&vars);

	} else if(key == "my_units")
	{
		std::vector<variant> vars;
		for(unit_map::const_iterator i = get_info().units.begin(); i != get_info().units.end(); ++i) {
			if(i->second.side() == get_side()) {
				vars.push_back(variant(new unit_callable(*i)));
			}
		}
		return variant(&vars);

	} else if(key == "enemy_units")
	{
		std::vector<variant> vars;
		for(unit_map::const_iterator i = get_info().units.begin(); i != get_info().units.end(); ++i) {
			if(current_team().is_enemy(i->second.side())) {
				if (!i->second.incapacitated()) {
					vars.push_back(variant(new unit_callable(*i)));
				}
			}
		}
		return variant(&vars);

	} else if(key == "my_moves")
	{
		prepare_move();
		return variant(new move_map_callable(srcdst_, dstsrc_, get_info().units));

	} else if(key == "my_attacks")
	{
		prepare_move();
		return variant(new attack_map_callable(*this, srcdst_, get_info().units));
	} else if(key == "enemy_moves")
	{
		prepare_move();
		return variant(new move_map_callable(enemy_srcdst_, enemy_dstsrc_, get_info().units));

	} else if(key == "my_leader")
	{
		unit_map::const_iterator i = get_info().units.find_leader(get_side());
		if(i == get_info().units.end()) {
			return variant();
		}
		return variant(new unit_callable(*i));

	} else if(key == "vars")
	{
		return variant(&vars_);
	} else if(key == "keeps")
	{
		return get_keeps();

	} else if(key == "villages")
	{
		return villages_from_set(get_info().map.villages());

	} else if(key == "villages_of_side")
	{
		std::vector<variant> vars;
		for(size_t i = 0; i<get_info().teams.size(); ++i)
		{
			vars.push_back( variant() );
		}
		for(size_t i = 0; i<vars.size(); ++i)
		{
			vars[i] = villages_from_set(get_info().teams[i].villages());
		}
		return variant(&vars);

	} else if(key == "my_villages")
	{
		return villages_from_set(current_team().villages());

	} else if(key == "enemy_and_unowned_villages")
	{
		return villages_from_set(get_info().map.villages(), &current_team().villages());
	}

	return ai_default::get_value(key);
}

void formula_ai::get_inputs(std::vector<formula_input>* inputs) const
{
	using game_logic::FORMULA_READ_ONLY;
	inputs->push_back(game_logic::formula_input("attacks", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_side", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("teams", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("turn", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("time_of_day", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("keeps", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("vars", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("allies", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("enemies", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_moves", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_attacks", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("enemy_moves", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_leader", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_recruits", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("recruits_of_side", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("units", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("units_of_side", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_units", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("enemy_units", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("villages", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("my_villages", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("villages_of_side", FORMULA_READ_ONLY));
	inputs->push_back(game_logic::formula_input("enemy_and_unowned_villages", FORMULA_READ_ONLY));

	ai_default::get_inputs(inputs);
}

variant formula_ai::get_keeps() const
{
	if(keeps_cache_.is_null()) {
		std::vector<variant> vars;
		for(size_t x = 0; x != size_t(get_info().map.w()); ++x) {
			for(size_t y = 0; y != size_t(get_info().map.h()); ++y) {
				const map_location loc(x,y);
				if(get_info().map.is_keep(loc)) {
					map_location adj[6];
					get_adjacent_tiles(loc,adj);
					for(size_t n = 0; n != 6; ++n) {
						if(get_info().map.is_castle(adj[n])) {
							vars.push_back(variant(new location_callable(loc)));
							break;
						}
					}
				}
			}
		}
		keeps_cache_ = variant(&vars);
	}

	return keeps_cache_;
}

bool formula_ai::can_reach_unit(map_location unit_A, map_location unit_B) const {
        prepare_move();
	move_map::iterator i;
	std::pair<move_map::iterator,
			  move_map::iterator> unit_moves;

	unit_moves = srcdst_.equal_range(unit_A);
	for(i = unit_moves.first; i != unit_moves.second; ++i) {
		map_location diff((((*i).second)).vector_difference(unit_B));

		if( ( ( diff.y == -1 || diff.y == 0 ) && (abs(diff.x) <= 1) ) ||
			( diff.y == 1 && diff.x == 0 ) ) {
			return true;
		}
	}
	return false;
}

void formula_ai::on_create(){
	//make sure we don't run out of refcount
	vars_.add_ref();
	const config& ai_param = cfg_;

	// load candidate actions from config
	candidate_action_manager_.load_config(ai_param, this, &function_table);

	foreach (const config &func, ai_param.child_range("function"))
	{
		const t_string &name = func["name"];
		const t_string &inputs = func["inputs"];
		const t_string &formula_str = func["formula"];

		std::vector<std::string> args = utils::split(inputs);

		try {
			function_table.add_formula_function(name,
				game_logic::const_formula_ptr(new game_logic::formula(formula_str, &function_table)),
				game_logic::formula::create_optional_formula(func["precondition"], &function_table),
				args);
			}
			catch(formula_error& e) {
				handle_exception(e, "Error while registering function '" + name + "'");
			}
		}


        try{
                recruit_formula_ = game_logic::formula::create_optional_formula(cfg_["recruitment"], &function_table);
        }
        catch(formula_error& e) {
                handle_exception(e);
                recruit_formula_ = game_logic::formula_ptr();
        }

        try{
                move_formula_ = game_logic::formula::create_optional_formula(cfg_["move"], &function_table);
        }
        catch(formula_error& e) {
                handle_exception(e);
                move_formula_ = game_logic::formula_ptr();
        }

}

void formula_ai::evaluate_candidate_action(game_logic::candidate_action_ptr fai_ca)
{
	fai_ca->evaluate(this,get_info().units);

}

bool formula_ai::execute_candidate_action(game_logic::candidate_action_ptr fai_ca)
{
	move_maps_valid_ = false;//@todo 1.7 think about optimizing this
	game_logic::map_formula_callable callable(this);
	callable.add_ref();
	fai_ca->update_callable_map( callable );
	const_formula_ptr move_formula(fai_ca->get_action());
	return !make_action(move_formula, callable).is_empty();
}

formula_ai::gamestate_change_observer::gamestate_change_observer() :
	set_var_counter_(), set_unit_var_counter_(), continue_counter_()
{
	ai::manager::add_gamestate_observer(this);
}

formula_ai::gamestate_change_observer::~gamestate_change_observer() {
	ai::manager::remove_gamestate_observer(this);
}

void formula_ai::gamestate_change_observer::handle_generic_event(const std::string& /*event_name*/) {
	set_var_counter_ = 0;
	set_unit_var_counter_ = 0;
	continue_counter_ = 0;
}

//return false if number of calls exceeded MAX_CALLS
bool formula_ai::gamestate_change_observer::set_var_check() {
	if(set_var_counter_ >= MAX_CALLS)
	    return false;

	set_var_counter_++;
	return true;
}

bool formula_ai::gamestate_change_observer::set_unit_var_check() {
	if(set_unit_var_counter_ >= MAX_CALLS)
	    return false;

	set_unit_var_counter_++;
	return true;
}

bool formula_ai::gamestate_change_observer::continue_check() {
	if(continue_counter_ >= MAX_CALLS)
	    return false;

	continue_counter_++;
	return true;
}

config formula_ai::to_config() const
{
	return cfg_;//@todo 1.7 add a proper serialization
}

} // end of namespace ai
