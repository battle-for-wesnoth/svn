/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "actions.hpp"
#include "ai.hpp"
#include "ai_attack.hpp"
#include "ai_move.hpp"
#include "dialogs.hpp"
#include "game_config.hpp"
#include "log.hpp"
#include "pathfind.hpp"
#include "playlevel.hpp"
#include "playturn.hpp"
#include "replay.hpp"
#include "show_dialog.hpp"
#include "statistics.hpp"
#include "unit_display.hpp"

#include <iostream>

ai_interface* create_ai(const std::string& name, ai_interface::info& info)
{
	//to add an AI of your own, put
	//if(name == "my_ai")
	//	return new my_ai(info);
	//at the top of this function

	if(name == "sample_ai")
		return new sample_ai(info);
	else if(name == "idle_ai")
		return new idle_ai(info);
	else if(name != "")
		std::cerr << "ERROR: AI not found: '" << name << "'\n";

	return new ai(info);
}

ai::ai(ai_interface::info& info)
	   : ai_interface(info), disp_(info.disp), map_(info.map), gameinfo_(info.gameinfo), units_(info.units),
	     teams_(info.teams), team_num_(info.team_num), state_(info.state),
		 consider_combat_(true)
{}

bool ai::recruit_usage(const std::string& usage)
{
	const int min_gold = 0;
	
	log_scope("recruiting troops");
	std::cerr << "recruiting " << usage << "\n";

	std::vector<std::string> options;

	//find an available unit that can be recruited, matches the desired
	//usage type, and comes in under budget
	const std::set<std::string>& recruits = current_team().recruits();
	for(std::map<std::string,unit_type>::const_iterator i =
	    gameinfo_.unit_types.begin(); i != gameinfo_.unit_types.end(); ++i) {

		const std::string& name = i->second.name();

		if(i->second.usage() == usage && recruits.count(name)
		   && current_team().gold() - i->second.cost() > min_gold
		   && not_recommended_units_.count(name) == 0) {
			options.push_back(name);
		}
	}

	//from the available options, choose one at random
	if(options.empty() == false) {
		const int option = rand()%options.size();
		return recruit(options[option]);
	}

	return false;
}

bool ai_interface::recruit(const std::string& unit_name, location loc)
{
	const std::set<std::string>& recruits = current_team().recruits();
	const std::set<std::string>::const_iterator i = std::find(recruits.begin(),recruits.end(),unit_name);
	if(i == recruits.end()) {
		return false;
	}

	const int num = std::distance(recruits.begin(),i);

	recorder.add_recruit(num,loc);
	replay_undo replay_guard(recorder);

	game_data::unit_type_map::const_iterator u = info_.gameinfo.unit_types.find(unit_name);
	if(u == info_.gameinfo.unit_types.end()) {
		return false;
	}

	//check we have enough money
	if(current_team().gold() < u->second.cost()) {
		return false;
	}

	unit new_unit(&u->second,info_.team_num,true);

	//see if we can actually recruit (i.e. have enough room etc)
	if(recruit_unit(info_.map,info_.team_num,info_.units,new_unit,loc,&info_.disp).empty()) {
		statistics::recruit_unit(new_unit);
		current_team().spend_gold(u->second.cost());

		//confirm the transaction - i.e. don't undo recruitment
		replay_guard.confirm_transaction();
		return true;
	} else {
		return false;
	}
}

void ai_interface::user_interact()
{
	const int interact_time = 30;
	const int time_since_interact = SDL_GetTicks() - last_interact_;
	if(time_since_interact < interact_time) {
		return;
	}

	const int ncommand = recorder.ncommands();
	info_.turn_data_.turn_slice();
	info_.turn_data_.send_data(ncommand);
	info_.disp.draw();

	last_interact_ = SDL_GetTicks();
}

team& ai_interface::current_team()
{
	return info_.teams[info_.team_num-1];
}

const team& ai_interface::current_team() const
{
	return info_.teams[info_.team_num-1];
}


gamemap::location ai_interface::move_unit(location from, location to, std::map<location,paths>& possible_moves)
{
	//stop the user from issuing any commands while the unit is moving
	const command_disabler disable_commands(&info_.disp);

	assert(info_.units.find(to) == info_.units.end() || from == to);

	info_.disp.select_hex(from);
	info_.disp.update_display();

	log_scope("move_unit");
	unit_map::iterator u_it = info_.units.find(from);
	if(u_it == info_.units.end()) {
		std::cerr  << "Could not find unit at " << from.x << ", "
		          << from.y << "\n";
		assert(false);
		return location();
	}

	if(from == to) {
		u_it->second.set_movement(0);
		return to;
	}

	const bool ignore_zocs = u_it->second.type().is_skirmisher();
	const bool teleport = u_it->second.type().teleports();
	paths current_paths = paths(info_.map,info_.state,info_.gameinfo,info_.units,from,info_.teams,ignore_zocs,teleport);
	paths_wiper wiper(info_.disp);

	if(!info_.disp.fogged(from.x,from.y))
		info_.disp.set_paths(&current_paths);

	info_.disp.scroll_to_tiles(from.x,from.y,to.x,to.y);

	unit current_unit = u_it->second;
	info_.units.erase(u_it);

	const std::map<location,paths>::iterator p_it = possible_moves.find(from);

	if(p_it != possible_moves.end()) {
		paths& p = p_it->second;
		std::map<location,paths::route>::iterator rt = p.routes.begin();
		for(; rt != p.routes.end(); ++rt) {
			if(rt->first == to) {
				break;
			}
		}

		if(rt != p.routes.end()) {
			std::vector<location> steps = rt->second.steps;

			if(steps.empty() == false) {
				//check if there are any invisible units that we uncover
				for(std::vector<location>::iterator i = steps.begin()+1; i != steps.end(); ++i) {
					location adj[6];
					get_adjacent_tiles(*i,adj);
					size_t n;
					for(n = 0; n != 6; ++n) {

						//see if there is an enemy unit next to this tile. Note that we don't
						//actually have to check if it's invisible, since it being invisible is
						//the only possibility
						const unit_map::const_iterator invisible_unit = info_.units.find(adj[n]);
						if(!ignore_zocs && invisible_unit != info_.units.end() && invisible_unit->second.stone() == false &&
						   current_team().is_enemy(invisible_unit->second.side())) {
							to = *i;
							steps.erase(i,steps.end());
							break;
						}
					}

					if(n != 6) {
						break;
					}
				}
			}

			steps.push_back(to); //add the destination to the steps
			unit_display::move_unit(info_.disp,info_.map,steps,current_unit);
		}
	}

	current_unit.set_movement(0);
	info_.units.insert(std::pair<location,unit>(to,current_unit));
	if(info_.map.is_village(to)) {
		get_village(to,info_.teams,info_.team_num-1,info_.units);
	}

	info_.disp.draw_tile(to.x,to.y);
	info_.disp.draw();

	game_events::fire("moveto",to);

	if((info_.teams.front().uses_fog() || info_.teams.front().uses_shroud()) && !info_.teams.front().fogged(to.x,to.y)) {
		game_events::fire("sighted",to);
	}

	recorder.add_movement(from,to);

	return to;
}

void ai_interface::calculate_possible_moves(std::map<location,paths>& res, move_map& srcdst, move_map& dstsrc, bool enemy, bool assume_full_movement)
{
	for(std::map<gamemap::location,unit>::iterator un_it = info_.units.begin(); un_it != info_.units.end(); ++un_it) {
		//if we are looking for the movement of enemies, then this unit must be an enemy unit
		//if we are looking for movement of our own units, it must be on our side.
		//if we are assuming full movement, then it may be a unit on our side, or allied
		if(enemy && current_team().is_enemy(un_it->second.side()) == false ||
		   !enemy && !assume_full_movement && un_it->second.side() != info_.team_num ||
		   !enemy && assume_full_movement && current_team().is_enemy(un_it->second.side())) {
			continue;
		}

		//discount our own leader, and our units that have been turned to stone
		if(!enemy && un_it->second.can_recruit() || un_it->second.stone()) {
			continue;
		}

		//we can't see where invisible enemy units might move
		if(enemy &&
		   un_it->second.invisible(info_.map.underlying_terrain(info_.map.get_terrain(un_it->first)), 
		   info_.state.get_time_of_day().lawful_bonus,un_it->first,info_.units,info_.teams)) {
			continue;
		}

		//if it's an enemy unit, reset its moves while we do the calculations
		const unit_movement_resetter move_resetter(un_it->second,enemy || assume_full_movement);

		//insert the trivial moves of staying on the same location
		if(un_it->second.movement_left() == un_it->second.total_movement()) {
			std::pair<location,location> trivial_mv(un_it->first,un_it->first);
			srcdst.insert(trivial_mv);
			dstsrc.insert(trivial_mv);
		}

		const bool ignore_zocs = un_it->second.type().is_skirmisher();
		const bool teleports = un_it->second.type().teleports();
		res.insert(std::pair<gamemap::location,paths>(
		                un_it->first,paths(info_.map,info_.state,info_.gameinfo,info_.units,
		                un_it->first,info_.teams,ignore_zocs,teleports)));
	}

	for(std::map<location,paths>::iterator m = res.begin(); m != res.end(); ++m) {
		for(paths::routes_map::iterator rtit =
		    m->second.routes.begin(); rtit != m->second.routes.end(); ++rtit) {
			const location& src = m->first;
			const location& dst = rtit->first;

			bool friend_owns = false;

			//don't take friendly villages
			if(!enemy && info_.map.is_village(dst)) {
				for(size_t n = 0; n != info_.teams.size(); ++n) {
					if(info_.teams[n].owns_village(dst)) {
						if(n+1 != info_.team_num && current_team().is_enemy(n+1) == false) {
							friend_owns = true;
						}

						break;
					}
				}
			}

			if(friend_owns) {
				continue;
			}

			if(src != dst && info_.units.find(dst) == info_.units.end()) {
				srcdst.insert(std::pair<location,location>(src,dst));
				dstsrc.insert(std::pair<location,location>(dst,src));
			}
		}
	}
}

void ai::play_turn()
{
	consider_combat_ = true;
	do_move();
}

void ai::do_move()
{
	log_scope("doing ai move");

	user_interact();

	typedef paths::route route;

	typedef std::map<location,paths> moves_map;
	moves_map possible_moves, enemy_possible_moves;

	move_map srcdst, dstsrc, enemy_srcdst, enemy_dstsrc;

	calculate_possible_moves(possible_moves,srcdst,dstsrc,false);
	calculate_possible_moves(enemy_possible_moves,enemy_srcdst,enemy_dstsrc,true);

	unit_map::iterator leader = find_leader(units_,team_num_);

	int ticks = SDL_GetTicks();

	std::vector<attack_analysis> analysis;

	if(consider_combat_) {
		std::cerr << "combat...\n";
		consider_combat_ = do_combat(possible_moves,srcdst,dstsrc,enemy_srcdst,enemy_dstsrc);
		if(consider_combat_) {
			do_move();
			return;
		}
	}

	std::cerr << "villages...\n";
	const bool got_village = get_villages(possible_moves,srcdst,dstsrc,enemy_srcdst,enemy_dstsrc,leader);
	if(got_village) {
		do_move();
		return;
	}

	std::cerr << "healing...\n";
	const bool healed_unit = get_healing(possible_moves,srcdst,dstsrc,enemy_srcdst,enemy_dstsrc);
	if(healed_unit) {
		do_move();
		return;
	}

	std::cerr << "retreating...\n";
	const bool retreated_unit = retreat_units(possible_moves,srcdst,dstsrc,enemy_srcdst,enemy_dstsrc,leader);
	if(retreated_unit) {
		do_move();
		return;
	}

	const bool met_invisible_unit = move_to_targets(possible_moves,srcdst,dstsrc,enemy_srcdst,enemy_dstsrc,leader);
	if(met_invisible_unit) {
		do_move();
		return;
	}

	//recruitment phase and leader movement phase
	if(leader != units_.end()) {
		move_leader_to_keep(enemy_dstsrc);
		do_recruitment();
		move_leader_after_recruit(enemy_dstsrc);
	}

	recorder.end_turn();
}

bool ai::do_combat(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc)
{
	int ticks = SDL_GetTicks();

	std::vector<attack_analysis> analysis = analyze_targets(srcdst,dstsrc,enemy_srcdst,enemy_dstsrc);

	int time_taken = SDL_GetTicks() - ticks;
	std::cerr  << "took " << time_taken << " ticks for " << analysis.size() << " positions. Analyzing...\n";

	ticks = SDL_GetTicks();

	const int max_sims = 50000;
	int num_sims = analysis.empty() ? 0 : max_sims/analysis.size();
	if(num_sims < 20)
		num_sims = 20;
	if(num_sims > 40)
		num_sims = 40;

	std::cerr  << "simulations: " << num_sims << "\n";

	const int max_positions = 30000;
	const int skip_num = analysis.size()/max_positions;

	std::vector<attack_analysis>::iterator choice_it = analysis.end();
	double choice_rating = -1000.0;
	for(std::vector<attack_analysis>::iterator it = analysis.begin(); it != analysis.end(); ++it) {
		if(skip_num > 0 && ((it - analysis.begin())%skip_num) && it->movements.size() > 1)
			continue;

		const double rating = it->rating(current_team().aggression());
		std::cerr  << "attack option rated at " << rating << " (" << current_team().aggression() << ")\n";
		if(rating > choice_rating) {
			choice_it = it;
			choice_rating = rating;
		}
	}

	time_taken = SDL_GetTicks() - ticks;
	std::cerr  << "analysis took " << time_taken << " ticks\n";

	if(choice_rating > 0.0) {
		location from   = choice_it->movements[0].first;
		location to     = choice_it->movements[0].second;
		location target_loc = choice_it->target;
		const int weapon = choice_it->weapons[0];

		const unit_map::const_iterator tgt = units_.find(target_loc);

		const bool defender_human = (tgt != units_.end()) ?
		             teams_[tgt->second.side()-1].is_human() : false;

		const location arrived_at = move_unit(from,to,possible_moves);
		if(arrived_at != to) {
			std::cerr << "unit moving to attack has ended up unexpectedly at " << (arrived_at.x+1) << "," << (arrived_at.y+1) << " when moving to "
			          << (to.x+1) << "," << (to.y+1) << " moved from " << (from.x+1) << "," << (from.y+1) << "\n";
			return true;
		}

		attack_enemy(to,target_loc,weapon);

		//if this is the only unit in the planned attack, and the target
		//is still alive, then also summon reinforcements
		if(choice_it->movements.size() == 1 && units_.count(target_loc)) {
			additional_targets_.push_back(target(target_loc,3.0));
		}

		return true;

	} else {
		log_scope("summoning reinforcements...\n");

		std::set<gamemap::location> already_done;

		for(std::vector<attack_analysis>::iterator it = analysis.begin(); it != analysis.end(); ++it) {
			assert(it->movements.empty() == false);
			const gamemap::location& loc = it->movements.front().first;
			if(already_done.count(loc) > 0)
				continue;

			additional_targets_.push_back(target(loc,3.0));
			already_done.insert(loc);
		}

		return false;
	}
}

void ai_interface::attack_enemy(const location& u, const location& target, int weapon)
{
	//stop the user from issuing any commands while the unit is attacking
	const command_disabler disable_commands(&info_.disp);

	if(info_.units.count(u) && info_.units.count(target)) {
		if(info_.units.find(target)->second.stone()) {
			std::cerr << "ERROR: attempt to attack unit that is turned to stone\n";
			return;
		}

		recorder.add_attack(u,target,weapon);
		game_events::fire("attack",u,target);

		attack(info_.disp,info_.map,info_.teams,u,target,weapon,info_.units,info_.state,info_.gameinfo,false);
		check_victory(info_.units,info_.teams);
		dialogs::advance_unit(info_.gameinfo,info_.units,u,info_.disp,true);

		const unit_map::const_iterator defender = info_.units.find(target);
		if(defender != info_.units.end()) {
			const size_t defender_team = size_t(defender->second.side()) - 1;
			if(defender_team < info_.teams.size()) {
				dialogs::advance_unit(info_.gameinfo,info_.units,target,info_.disp,!info_.teams[defender_team].is_human());
			}
		}
	}
}

bool ai::get_villages(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader)
{
	//try to acquire villages
	for(move_map::const_iterator i = dstsrc.begin(); i != dstsrc.end(); ++i) {
		if(map_.is_village(i->first) == false) {
			continue;
		}

		bool want_village = true, owned = false;
		for(size_t j = 0; j != teams_.size(); ++j) {
			owned = teams_[j].owns_village(i->first);
			if(owned && !current_team().is_enemy(j+1)) {
				want_village = false;
			}
			
			if(owned) {
				break;
			}
		}

		//if it's a neutral village, and we have no leader, then the village is no use to us,
		//and we don't want it.
		if(!owned && leader == units_.end())
			want_village = false;

		if(want_village) {
			std::cerr << "trying to acquire village: " << i->first.x
			          << ", " << i->first.y << "\n";

			const std::map<location,unit>::iterator un = units_.find(i->second);
			if(un == units_.end()) {
				assert(false);
				return false;
			}

			if(un->second.is_guardian())
				continue;

			move_unit(i->second,i->first,possible_moves);
			return true;
		}
	}

	return false;
}

bool ai::get_healing(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc)
{
	//find units in need of healing
	unit_map::iterator u_it = units_.begin();
	for(; u_it != units_.end(); ++u_it) {
		unit& u = u_it->second;

		//if the unit is on our side, has lost as many or more than 1/2 round
		//worth of healing, and doesn't regenerate itself, then try to
		//find a vacant village for it to rest in
		if(u.side() == team_num_ &&
		   u.type().hitpoints() - u.hitpoints() >= game_config::cure_amount/2 &&
		   !u.type().regenerates()) {

			//look for the village which is the least vulnerable to enemy attack
			typedef std::multimap<location,location>::const_iterator Itor;
			std::pair<Itor,Itor> it = srcdst.equal_range(u_it->first);
			double best_vulnerability = 100000.0;
			Itor best_loc = it.second;
			while(it.first != it.second) {
				const location& dst = it.first->second;
				if(map_.gives_healing(dst) && units_.find(dst) == units_.end()) {
					const double vuln = power_projection(it.first->first,
					                    enemy_srcdst,enemy_dstsrc);
					std::cerr << "found village with vulnerability: " << vuln << "\n";
					if(vuln < best_vulnerability || best_loc == it.second) {
						best_vulnerability = vuln;
						best_loc = it.first;
						std::cerr << "chose village " << (dst.x+1) << "," << (dst.y+1) << "\n";
					}
				}
				
				++it.first;
			}

			//if we have found an eligible village
			if(best_loc != it.second) {
				const location& src = best_loc->first;
				const location& dst = best_loc->second;

				std::cerr << "moving unit to village for healing...\n";

				move_unit(src,dst,possible_moves);
				return true;
			}
		}
	}

	return false;
}

bool ai::should_retreat(const gamemap::location& loc, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc) const
{
	const double caution = current_team().caution();
	if(caution <= 0.0) {
		return false;
	}

	const double our_power = power_projection(loc,srcdst,dstsrc);
	const double their_power = power_projection(loc,enemy_srcdst,enemy_dstsrc);
	return caution*their_power > 2.0*our_power + our_power*current_team().aggression();
}

bool ai::retreat_units(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader)
{
	//get versions of the move map that assume that all units are at full movement
	std::map<gamemap::location,paths> dummy_possible_moves;
	move_map fullmove_srcdst;
	move_map fullmove_dstsrc;
	calculate_possible_moves(dummy_possible_moves,fullmove_srcdst,fullmove_dstsrc,false,true);

	gamemap::location leader_adj[6];
	if(leader != units_.end()) {
		get_adjacent_tiles(leader->first,leader_adj);
	}

	for(unit_map::iterator i = units_.begin(); i != units_.end(); ++i) {
		if(i->second.side() == team_num_ && i->second.movement_left() == i->second.total_movement() && unit_map::const_iterator(i) != leader) {

			//this unit still has movement left, and is a candidate to retreat. We see the amount
			//of power of each side on the situation, and decide whether it should retreat.
			if(should_retreat(i->first,fullmove_srcdst,fullmove_dstsrc,enemy_srcdst,enemy_dstsrc)) {

				bool can_reach_leader = false;

				//time to retreat. Look for the place where the power balance is most in our favor.
				//If we can't find anywhere where we like the power balance, just try to
				//get to the best defensive hex
				typedef move_map::const_iterator Itor;
				std::pair<Itor,Itor> itors = srcdst.equal_range(i->first);
				gamemap::location best_pos, best_defensive;
				double best_rating = 0.0;
				int best_defensive_rating = 100;
				while(itors.first != itors.second) {

					if(leader != units_.end() && std::count(leader_adj,leader_adj+6,itors.first->second)) {
						can_reach_leader = true;
						break;
					}

					//we rate the power balance of a hex based on our power projection compared
					//to theirs, multiplying their power projection by their chance to hit us
					//on the hex we're planning to flee to.
					const gamemap::location& hex = itors.first->second;
					const int defense = i->second.type().movement_type().defense_modifier(map_,map_.get_terrain(hex));
					const double our_power = power_projection(hex,srcdst,dstsrc);
					const double their_power = power_projection(hex,enemy_srcdst,enemy_dstsrc) * double(defense)/100.0;
					const double rating = our_power - their_power;
					if(rating > best_rating) {
						best_pos = hex;
						best_rating = rating;
					}

					//give a bonus for getting to a village.
					const int modified_defense = defense - (map_.is_village(hex) ? 10 : 0);

					if(modified_defense < best_defensive_rating) {
						best_defensive_rating = modified_defense;
						best_defensive = hex;
					}

					++itors.first;
				}

				//if the unit is in range of its leader, it should never retreat --
				//it has to defend the leader instead
				if(can_reach_leader) {
					continue;
				}

				if(!best_pos.valid()) {
					best_pos = best_defensive;
				}

				if(best_pos.valid()) {
					std::cerr << "retreating '" << i->second.type().name() << "' " << i->first.x << "," << i->first.y << " -> " << best_pos.x << "," << best_pos.y << "\n";
					move_unit(i->first,best_pos,possible_moves);
					return true;
				}
			}
		}
	}

	return false;
}

bool ai::move_to_targets(std::map<gamemap::location,paths>& possible_moves, move_map& srcdst, move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader)
{
	std::cerr << "finding targets...\n";
	std::vector<target> targets;
	for(;;) {
		if(targets.empty()) {
			targets = find_targets(leader,enemy_srcdst,enemy_dstsrc);
			targets.insert(targets.end(),additional_targets_.begin(),
			                             additional_targets_.end());
			if(targets.empty()) {
				return false;
			}
		}

		std::cerr << "choosing move...\n";
		std::pair<location,location> move = choose_move(targets,dstsrc,enemy_srcdst,enemy_dstsrc);
		for(std::vector<target>::const_iterator ittg = targets.begin(); ittg != targets.end(); ++ittg) {
			assert(map_.on_board(ittg->loc));
		}

		if(move.first.valid() == false)
			break;

		std::cerr << "move: " << move.first.x << ", " << move.first.y << " - " << move.second.x << ", " << move.second.y << "\n";

		//search to see if there are any enemy units next
		//to the tile which really should be attacked once the move is done
		gamemap::location adj[6];
		get_adjacent_tiles(move.second,adj);
		battle_stats bat_stats;
		gamemap::location target;
		int weapon = -1;
		for(int n = 0; n != 6; ++n) {
			const unit_map::iterator enemy = find_visible_unit(units_,adj[n],
					map_,
					state_.get_time_of_day().lawful_bonus,
					teams_,current_team());

			if(enemy != units_.end() &&
			   current_team().is_enemy(enemy->second.side()) && enemy->second.stone() == false) {
				target = adj[n];
				weapon = choose_weapon(move.first,target,bat_stats,
				                       map_[move.second.x][move.second.y]);
				break;
			}
		}

		const location arrived_at = move_unit(move.first,move.second,possible_moves);

		//we didn't arrive at our intended destination. We return true, meaning that
		//the AI algorithm should be recalculated from the start.
		if(arrived_at != move.second) {
			return true;
		}

		const unit_map::const_iterator u_it = units_.find(move.second);

		//if we're going to attack someone
		if(u_it != units_.end() && u_it->second.stone() == false && weapon != -1) {
			attack_enemy(move.second,target,weapon);			
		}

		//don't allow any other units to move onto the tile our unit
		//just moved onto
		typedef std::multimap<location,location>::iterator Itor;
		std::pair<Itor,Itor> del = dstsrc.equal_range(arrived_at);
		dstsrc.erase(del.first,del.second);
	}

	return false;
}

int ai::average_resistance_against(const unit_type& a, const unit_type& b) const
{
	int weighting_sum = 0, defense = 0;
	const std::map<gamemap::TERRAIN,size_t>& terrain = map_.get_weighted_terrain_frequencies();
	for(std::map<gamemap::TERRAIN,size_t>::const_iterator j = terrain.begin(); j != terrain.end(); ++j) {
		defense += a.movement_type().defense_modifier(map_,j->first)*j->second;
		weighting_sum += j->second;
	}

	defense /= weighting_sum;

	std::cerr << "average defense of '" << a.name() << "': " << defense << "\n";

	int sum = 0, weight_sum = 0;

	const std::vector<attack_type>& attacks = b.attacks();
	for(std::vector<attack_type>::const_iterator i = attacks.begin(); i != attacks.end(); ++i) {
		const int resistance = a.movement_type().resistance_against(*i);
		const int weight = i->damage()*i->num_attacks();

		sum += defense*resistance*weight;
		weight_sum += weight;
	}

	return sum/weight_sum;
}

int ai::compare_unit_types(const unit_type& a, const unit_type& b) const
{
	const int a_effectiveness_vs_b = average_resistance_against(b,a);
	const int b_effectiveness_vs_a = average_resistance_against(a,b);

	std::cerr << "comparison of '" << a.name() << " vs " << b.name() << ": "
	          << a_effectiveness_vs_b << " - " << b_effectiveness_vs_a << " = "
			  << (a_effectiveness_vs_b - b_effectiveness_vs_a) << "\n";
	return a_effectiveness_vs_b - b_effectiveness_vs_a;
}

void ai::analyze_potential_recruit_combat()
{
	if(unit_combat_scores_.empty() == false || current_team().ai_parameters()["recruitment_ignore_bad_combat"] == "yes") {
		return;
	}

	log_scope("analyze_potential_recruit_combat()");

	//records the best combat analysis for each usage type
	std::map<std::string,int> best_usage;

	const std::set<std::string>& recruits = current_team().recruits();
	std::set<std::string>::const_iterator i;
	for(i = recruits.begin(); i != recruits.end(); ++i) {
		const game_data::unit_type_map::const_iterator info = gameinfo_.unit_types.find(*i);
		if(info == gameinfo_.unit_types.end() || not_recommended_units_.count(*i)) {
			continue;
		}

		int score = 0, weighting = 0;

		for(unit_map::const_iterator j = units_.begin(); j != units_.end(); ++j) {
			if(j->second.can_recruit() || current_team().is_enemy(j->second.side()) == false) {
				continue;
			}

			weighting += j->second.type().cost();
			score += compare_unit_types(info->second,j->second.type())*j->second.type().cost();
		}

		if(weighting != 0) {
			score /= weighting;
		}

		std::cerr << "combat score of '" << *i << "': " << score << "\n";
		unit_combat_scores_[*i] = score;

		if(best_usage.count(info->second.usage()) == 0 || score > best_usage[info->second.usage()]) {
			best_usage[info->second.usage()] = score;
		}
	}

	//recommend not to use units of a certain usage type if they have a score more than 1000
	//below the best unit of that usage type
	for(i = recruits.begin(); i != recruits.end(); ++i) {
		const game_data::unit_type_map::const_iterator info = gameinfo_.unit_types.find(*i);
		if(info == gameinfo_.unit_types.end() || not_recommended_units_.count(*i)) {
			continue;
		}

		if(unit_combat_scores_[*i] + 1000 < best_usage[info->second.usage()]) {
			std::cerr << "recommending not to use '" << *i << "' because of poor combat performance "
				      << unit_combat_scores_[*i] << "/" << best_usage[info->second.usage()] << "\n";
			not_recommended_units_.insert(*i);
		}
	}
}

namespace {

struct target_comparer_distance {
	target_comparer_distance(const gamemap::location& loc) : loc_(loc) {}

	bool operator()(const ai::target& a, const ai::target& b) const {
		return distance_between(a.loc,loc_) < distance_between(b.loc,loc_);
	}

private:
	gamemap::location loc_;
};

}

void ai::analyze_potential_recruit_movements()
{
	if(unit_movement_scores_.empty() == false || current_team().ai_parameters()["recruitment_ignore_bad_movement"] == "yes") {
		return;
	}

	const location& start = map_.starting_position(team_num_);
	if(map_.on_board(start) == false) {
		return;
	}

	log_scope("analyze_potential_recruit_movements()");

	const unit_map::const_iterator leader = units_.find(start);

	const int max_targets = 5;

	const move_map srcdst, dstsrc;
	std::vector<target> targets = find_targets(leader,srcdst,dstsrc);
	if(targets.size() > max_targets) {
		std::sort(targets.begin(),targets.end(),target_comparer_distance(start));
		targets.erase(targets.begin()+max_targets,targets.end());
	}

	const std::set<std::string>& recruits = current_team().recruits();

	std::cerr << "targets: " << targets.size() << "\n";

	int best_score = -1;
	
	for(std::set<std::string>::const_iterator i = recruits.begin(); i != recruits.end(); ++i) {
		const game_data::unit_type_map::const_iterator info = gameinfo_.unit_types.find(*i);
		if(info == gameinfo_.unit_types.end()) {
			continue;
		}

		const unit temp_unit(&info->second,team_num_);
		unit_map units;
		const temporary_unit_placer placer(units,start,temp_unit);

		int cost = 0;
		int targets_reached = 0;
		int targets_missed = 0;

		const shortest_path_calculator calc(temp_unit,current_team(),units,teams_,map_,state_);
		for(std::vector<target>::const_iterator t = targets.begin(); t != targets.end(); ++t) {
			std::cerr << "analyzing '" << *i << "' getting to target...\n";
			const paths::route& route = a_star_search(start,t->loc,100.0,calc);
			if(route.steps.empty() == false) {
				std::cerr << "made it: " << route.move_left << "\n";
				cost += route.move_left;
				++targets_reached;
			} else {
				std::cerr << "failed\n";
				++targets_missed;
			}
		}

		if(targets_reached == 0 || targets_missed >= targets_reached*2) {
			unit_movement_scores_[*i] = 100000;
			not_recommended_units_.insert(*i);
		} else {
			const int average_cost = cost/targets_reached;
			const int score = (average_cost * (targets_reached+targets_missed))/targets_reached;
			unit_movement_scores_[*i] = score;
			if(best_score == -1 || score < best_score) {
				best_score = score;
			}
		}
	}

	for(std::map<std::string,int>::iterator j = unit_movement_scores_.begin(); j != unit_movement_scores_.end(); ++j) {
		if(best_score > 0) {
			j->second = (j->second*10)/best_score;
			if(j->second > 15) {
				std::cerr << "recommending against recruiting '" << j->first << "' (score: " << j->second << ")\n";
				not_recommended_units_.insert(j->first);
			} else {
				std::cerr << "recommending recruit of '" << j->first << "' (score: " << j->second << ")\n";
			}
		}
	}

	if(not_recommended_units_.size() == unit_movement_scores_.size()) {
		not_recommended_units_.clear();
	}
}

void ai::do_recruitment()
{
	analyze_potential_recruit_movements();
	analyze_potential_recruit_combat();

	//currently just spend all the gold we can!
	const int min_gold = 0;

	size_t neutral_villages = 0;

	//we recruit the initial allocation of scouts based on how many neutral villages
	//there are that are closer to us than to other keeps.
	const std::vector<location>& villages = map_.villages();
	for(std::vector<location>::const_iterator v = villages.begin(); v != villages.end(); ++v) {
		const int owner = village_owner(*v,teams_);
		if(owner == -1) {
			const size_t distance = distance_between(map_.starting_position(team_num_),*v);

			bool closest = true;
			for(std::vector<team>::const_iterator i = teams_.begin(); i != teams_.end(); ++i) {
				const int index = i - teams_.begin() + 1;
				if(team_num_ != index) {
					const gamemap::location& loc = map_.starting_position(index);
					if(distance_between(loc,*v) < distance) {
						closest = false;
						break;
					}
				}
			}

			if(closest) {
				++neutral_villages;
			}
		}
	}

	//the villages per scout is for a two-side battle, accounting for all neutral villages
	//on the map. We only look at villages closer to us, so we halve it, making us get
	//twice as many scouts
	const int villages_per_scout = current_team().villages_per_scout()/2;

	//get scouts depending on how many neutral villages there are
	int scouts_wanted = villages_per_scout > 0 ? neutral_villages/villages_per_scout : 0;

	std::map<std::string,int> unit_types;
	while(unit_types["scout"] < scouts_wanted) {
		if(recruit_usage("scout") == false)
			break;

		++unit_types["scout"];
	}

	const std::vector<std::string>& options = current_team().recruitment_pattern();

	if(options.empty()) {
		assert(false);
		return;
	}

	//buy units as long as we have room and can afford it
	while(recruit_usage(options[rand()%options.size()])) {
	}
}

void ai::move_leader_to_keep(const move_map& enemy_dstsrc)
{
	const unit_map::iterator leader = find_leader(units_,team_num_);
	if(leader == units_.end() || leader->second.stone())
		return;

	//find where the leader can move
	const paths leader_paths(map_,state_,gameinfo_,units_,leader->first,teams_,false,false);
	const gamemap::location& start_pos = map_.starting_position(leader->second.side());

	std::map<gamemap::location,paths> possible_moves;
	possible_moves.insert(std::pair<gamemap::location,paths>(leader->first,leader_paths));

	bool leader_moved = false;
	//if the leader is not on his starting location, move him there.
	if(leader->first != start_pos) {
		leader_moved = true;
		const paths::routes_map::const_iterator itor = leader_paths.routes.find(start_pos);
		if(itor != leader_paths.routes.end() && units_.count(start_pos) == 0) {
			move_unit(leader->first,start_pos,possible_moves);
		}
	}
}

void ai::leader_attack()
{
	std::cerr << "leader attack analysis...\n";
	const unit_map::iterator leader = find_leader(units_,team_num_);
	if(leader == units_.end() || leader->second.stone())
		return;

	gamemap::location choice;
	double rating = 0.0;
	int weapon = -1;

	gamemap::location adj[6];
	get_adjacent_tiles(leader->first,adj);
	for(size_t n = 0; n != 6; ++n) {
		const unit_map::const_iterator u = units_.find(adj[n]);
		if(u != units_.end() && current_team().is_enemy(u->second.side()) && u->second.stone() == false) {
			attack_analysis analysis;
			analysis.target = adj[n];
			analysis.movements.push_back(std::pair<location,location>(leader->first,leader->first));
			analysis.analyze(map_,units_,state_,gameinfo_,20,*this);
			const double value = analysis.chance_to_kill*analysis.target_value - analysis.avg_losses*10.0 - analysis.avg_damage_taken;
			if(value >= rating) {
				rating = value;
				choice = adj[n];

				assert(analysis.weapons.size() == 1);
				weapon = analysis.weapons.front();
			}
		}
	}

	if(choice.valid()) {
		attack_enemy(leader->first,choice,weapon);
	}

	std::cerr << "end leader attack analysis...\n";
}

void ai::move_leader_after_recruit(const move_map& enemy_dstsrc)
{
	std::cerr << "moving leader after recruit...\n";
	leader_attack();

	const unit_map::iterator leader = find_leader(units_,team_num_);
	if(leader == units_.end() || leader->second.stone())
		return;

	const paths leader_paths(map_,state_,gameinfo_,units_,leader->first,teams_,false,false);

	std::map<gamemap::location,paths> possible_moves;
	possible_moves.insert(std::pair<gamemap::location,paths>(leader->first,leader_paths));

	//see if the leader can capture a village safely
	//if the keep is accessible by an enemy unit, we don't want to leave it
	gamemap::location adj[6];
	get_adjacent_tiles(leader->first,adj);
	for(size_t n = 0; n != 6; ++n) {
		if(enemy_dstsrc.count(adj[n])) {
			return;
		}
	}

	//search through villages finding one to capture
	const std::vector<gamemap::location>& villages = map_.villages();
	for(std::vector<gamemap::location>::const_iterator v = villages.begin();
	    v != villages.end(); ++v) {
		const paths::routes_map::const_iterator itor = leader_paths.routes.find(*v);
		if(itor == leader_paths.routes.end() || units_.count(*v) != 0) {
			continue;
		}

		const int owner = village_owner(*v,teams_);
		if(owner == -1 || current_team().is_enemy(owner+1) || leader->second.hitpoints() < leader->second.max_hitpoints()) {

			//check that no enemies can reach the village
			gamemap::location adj[6];
			get_adjacent_tiles(*v,adj);
			size_t n;
			for(n = 0; n != 6; ++n) {
				if(enemy_dstsrc.count(adj[n]))
					break;
			}

			if(n != 6)
				continue;

			move_unit(leader->first,*v,possible_moves);

			break;
		}
	}
}

int ai::rate_terrain(const unit& u, const gamemap::location& loc)
{
	const gamemap::TERRAIN terrain = map_.get_terrain(loc);
	const int defense = u.defense_modifier(map_,terrain);
	int rating = 100 - defense;

	const int healing_value = 10;
	const int friendly_village_value = 5;
	const int neutral_village_value = 10;
	const int enemy_village_value = 15;

	if(map_.gives_healing(terrain) && u.type().regenerates() == false) {
		rating += healing_value;
	}

	if(map_.is_village(terrain)) {
		const int owner = village_owner(loc,teams_);

		if(owner+1 == team_num_) {
			rating += friendly_village_value;
		} else if(owner == -1) {
			rating += neutral_village_value;
		} else {
			rating += enemy_village_value;
		}
	}

	return rating;
}
