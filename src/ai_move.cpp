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

#include "global.hpp"

#include "ai_move.hpp"
#include "display.hpp"
#include "game_config.hpp"
#include "log.hpp"
#include "map.hpp"
#include "util.hpp"
#include "wassert.hpp"

#include <iostream>

#define LOG_AI lg::info(lg::ai)

struct move_cost_calculator : cost_calculator
{
	move_cost_calculator(const unit& u, const gamemap& map,
	                     const game_data& data,
					 	 const unit_map& units,
	                     const gamemap::location& loc,
						 const ai::move_map& dstsrc,
						 const ai::move_map& enemy_dstsrc)
	  : unit_(u), map_(map), data_(data), units_(units),
	    move_type_(u.type().movement_type()), loc_(loc), dstsrc_(dstsrc), enemy_dstsrc_(enemy_dstsrc),
		avoid_enemies_(u.type().usage() == "scout")
	{}

	virtual double cost(const gamemap::location& loc, const double, const bool) const
	{
		/*
		if(!map_.on_board(loc))
			return 1000.0;

		// if this unit can move to that location this turn, it has a very very low cost
		typedef std::multimap<gamemap::location,gamemap::location>::const_iterator Itor;
		std::pair<Itor,Itor> range = dstsrc_.equal_range(loc);
		while(range.first != range.second) {
			if(range.first->second == loc_) {
				return 0.01;
			}
			++range.first;
		}
		*/
		wassert(map_.on_board(loc));

		const gamemap::TERRAIN terrain = map_[loc.x][loc.y];

		const double modifier = 1.0; //move_type_.defense_modifier(map_,terrain);
		const double move_cost = move_type_.movement_cost(map_,terrain);

		const int enemies = 1 + (avoid_enemies_ ? enemy_dstsrc_.count(loc) : 0);
		double res = modifier*move_cost*double(enemies);

		//if there is a unit (even a friendly one) on this tile, we increase the cost to
		//try discourage going through units, to thwart the 'single file effect'
		if (units_.count(loc))
			res *= 4.0;

		wassert(res > 0);
		return res;
	}

private:
	const unit& unit_;
	const gamemap& map_;
	const game_data& data_;
	const unit_map& units_;
	const unit_movement_type& move_type_;
	const gamemap::location loc_;
	const ai::move_map dstsrc_, enemy_dstsrc_;
	const bool avoid_enemies_;
};

std::vector<ai::target> ai::find_targets(unit_map::const_iterator leader, const move_map& enemy_srcdst, const move_map& enemy_dstsrc)
{
	log_scope2(ai, "finding targets...");

	const bool has_leader = leader != units_.end();

	std::vector<target> targets;

	//if enemy units are in range of the leader, then we target the enemies who are in range.
	if(has_leader) {
		const double threat = power_projection(leader->first,enemy_srcdst,enemy_dstsrc);
		if(threat > 0.0) {
			//find the location of enemy threats
			std::set<gamemap::location> threats;

			gamemap::location adj[6];
			get_adjacent_tiles(leader->first,adj);
			for(size_t n = 0; n != 6; ++n) {
				std::pair<move_map::const_iterator,move_map::const_iterator> itors = enemy_dstsrc.equal_range(adj[n]);
				while(itors.first != itors.second) {
					if(units_.count(itors.first->second)) {
						threats.insert(itors.first->second);
					}

					++itors.first;
				}
			}

			wassert(threats.empty() == false);

			const double value = threat/double(threats.size());
			for(std::set<gamemap::location>::const_iterator i = threats.begin(); i != threats.end(); ++i) {
				targets.push_back(target(*i,value,target::THREAT));
			}
		}
	}

	if(has_leader && current_team().village_value() > 0.0) {
		const std::vector<location>& villages = map_.villages();
		for(std::vector<location>::const_iterator t = villages.begin(); t != villages.end(); ++t) {
			wassert(map_.on_board(*t));
			bool get_village = true;
			for(size_t i = 0; i != teams_.size(); ++i) {
				if(!current_team().is_enemy(i+1) && teams_[i].owns_village(*t)) {
					get_village = false;
					break;
				}
			}

			if(get_village) {
				targets.push_back(target(*t,current_team().village_value(),target::VILLAGE));
			}
		}
	}

	std::vector<team::target>& team_targets = current_team().targets();

	//find the enemy leaders and explicit targets
	unit_map::const_iterator u;
	for(u = units_.begin(); u != units_.end(); ++u) {

		//is an enemy leader
		if(u->second.can_recruit() && current_team().is_enemy(u->second.side())) {
			wassert(map_.on_board(u->first));
			targets.push_back(target(u->first,current_team().leader_value(),target::LEADER));
		}

		//explicit targets for this team
		for(std::vector<team::target>::iterator j = team_targets.begin();
		    j != team_targets.end(); ++j) {
			if(u->second.matches_filter(j->criteria)) {
				LOG_AI << "found explicit target..." << j->value << "\n";
				targets.push_back(target(u->first,j->value,target::EXPLICIT));
			}
		}
	}

	std::vector<double> new_values;

	for(std::vector<target>::iterator i = targets.begin();
	    i != targets.end(); ++i) {

		new_values.push_back(i->value);

		for(std::vector<target>::const_iterator j = targets.begin(); j != targets.end(); ++j) {
			if(i->loc == j->loc) {
				continue;
			}

			const double distance = abs(j->loc.x - i->loc.x) +
			                        abs(j->loc.y - i->loc.y);
			new_values.back() += j->value/(distance*distance);
		}
	}

	wassert(new_values.size() == targets.size());
	for(size_t n = 0; n != new_values.size(); ++n) {
		LOG_AI << "target value: " << targets[n].value << " -> " << new_values[n] << "\n";
		targets[n].value = new_values[n];
	}

	return targets;
}

gamemap::location ai::form_group(const std::vector<location>& route, const move_map& dstsrc, const move_map& srcdst, std::set<location>& res)
{
	if(route.empty()) {
		return location();
	}

	std::vector<location>::const_iterator i;
	for(i = route.begin(); i != route.end(); ++i) {
		if(units_.count(*i) > 0) {
			continue;
		}

		size_t n = 0, nunits = res.size();

		const std::pair<move_map::const_iterator,move_map::const_iterator> itors = dstsrc.equal_range(*i);
		for(move_map::const_iterator j = itors.first; j != itors.second; ++j) {
			if(res.count(j->second) != 0) {
				++n;
			} else {
				const unit_map::const_iterator un = units_.find(j->second);
				if(un == units_.end() || un->second.can_recruit() || un->second.movement_left() < un->second.total_movement()) {
					continue;
				}

				res.insert(j->second);
			}
		}

		//if not all our units can reach this position.
		if(n < nunits) {
			break;
		}
	}

	if(i != route.begin()) {
		--i;
	}

	return *i;
}

void ai::enemies_along_path(const std::vector<location>& route, const move_map& dstsrc, const move_map& srcdst, std::set<location>& res)
{
	for(std::vector<location>::const_iterator i = route.begin(); i != route.end(); ++i) {
		gamemap::location adj[6];
		get_adjacent_tiles(*i,adj);
		for(size_t n = 0; n != 6; ++n) {
			const std::pair<move_map::const_iterator,move_map::const_iterator> itors = dstsrc.equal_range(adj[n]);
			for(move_map::const_iterator j = itors.first; j != itors.second; ++j) {
				res.insert(j->second);
			}
		}
	}
}

bool ai::move_group(const location& dst, const std::vector<location>& route, const std::set<location>& units)
{
	const std::vector<location>::const_iterator itor = std::find(route.begin(),route.end(),dst);
	if(itor == route.end()) {
		return false;
	}

	LOG_AI << "group has " << units.size() << " members\n";

	location next;

	size_t direction = 0;

	//find the direction the group is moving in
	if(itor+1 != route.end()) {
		next = *(itor+1);
	} else if(itor != route.begin()) {
		next = *(itor-1);
	}

	if(next.valid()) {
		location adj[6];
		get_adjacent_tiles(dst,adj);

		direction = std::find(adj,adj+6,next) - adj;
	}

	std::deque<location> preferred_moves;
	preferred_moves.push_back(dst);

	std::map<location,paths> possible_moves;
	move_map srcdst, dstsrc;
	calculate_possible_moves(possible_moves,srcdst,dstsrc,false);

	bool res = false;

	for(std::set<location>::const_iterator i = units.begin(); i != units.end(); ++i) {
		const unit_map::const_iterator un = units_.find(*i);
		if(un == units_.end()) {
			continue;
		}

		location best_loc;
		int best_defense = -1;
		for(std::deque<location>::const_iterator j = preferred_moves.begin(); j != preferred_moves.end(); ++j) {
			if(units_.count(*j)) {
				continue;
			}

			const std::pair<move_map::const_iterator,move_map::const_iterator> itors = dstsrc.equal_range(*j);
			move_map::const_iterator m;
			for(m = itors.first; m != itors.second; ++m) {
				if(m->second == *i) {
					break;
				}
			}

			if(m == itors.second) {
				continue;
			}

			const int defense = un->second.defense_modifier(map_,map_.get_terrain(*j));
			if(best_loc.valid() == false || defense < best_defense) {
				best_loc = *j;
				best_defense = defense;
			}
		}

		if(best_loc.valid()) {
			res = true;
			const location res = move_unit(*i,best_loc,possible_moves);

			//if we were ambushed, abort the group's movement.
			if(res != best_loc) {
				return true;
			}

			preferred_moves.erase(std::find(preferred_moves.begin(),preferred_moves.end(),best_loc));

			//find locations that are 'perpendicular' to the direction of movement for further units to move to.
			location adj[6];
			get_adjacent_tiles(best_loc,adj);
			for(size_t n = 0; n != 6; ++n) {
				if(n != direction && ((n+3)%6) != direction && map_.on_board(adj[n]) &&
				   units_.count(adj[n]) == 0 && std::count(preferred_moves.begin(),preferred_moves.end(),adj[n]) == 0) {
					preferred_moves.push_front(adj[n]);
					LOG_AI << "added moves: " << adj[n].x + 1 << "," << adj[n].y + 1 << "\n";
				}
			}
		} else {
			LOG_AI << "Could not move group member to any of " << preferred_moves.size() << " locations\n";
		}
	}

	return res;
}

double ai::rate_group(const std::set<location>& group, const std::vector<location>& battlefield) const
{
	double strength = 0.0;
	for(std::set<location>::const_iterator i = group.begin(); i != group.end(); ++i) {
		const unit_map::const_iterator u = units_.find(*i);
		if(u == units_.end()) {
			continue;
		}

		const unit& un = u->second;

		int defense = 0;
		for(std::vector<location>::const_iterator j = battlefield.begin(); j != battlefield.end(); ++j) {
			defense += un.defense_modifier(map_,map_.get_terrain(*j));
		}

		defense /= battlefield.size();

		int best_attack = 0;
		const std::vector<attack_type>& attacks = un.attacks();
		for(std::vector<attack_type>::const_iterator a = attacks.begin(); a != attacks.end(); ++a) {
			const int strength = a->num_attacks()*a->damage();
			best_attack = maximum<int>(strength,best_attack);
		}

		const int rating = (defense*best_attack*un.hitpoints())/(100*un.max_hitpoints());
		strength += double(rating);
	}

	return strength;
}

double ai::compare_groups(const std::set<location>& our_group, const std::set<location>& their_group, const std::vector<location>& battlefield) const
{
	const double a = rate_group(our_group,battlefield);
	const double b = maximum<double>(rate_group(their_group,battlefield),0.01);
	return a/b;
}

std::pair<gamemap::location,gamemap::location> ai::choose_move(std::vector<target>& targets, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc)
{
	log_scope2(ai, "choosing move");

	user_interact();

	std::vector<target>::const_iterator ittg;
	for(ittg = targets.begin(); ittg != targets.end(); ++ittg) {
		wassert(map_.on_board(ittg->loc));
	}

	paths::route best_route;
	unit_map::iterator best = units_.end();
	double best_rating = 0.1;
	std::vector<target>::iterator best_target = targets.end();

	unit_map::iterator u;

	//find the first eligible unit
	for(u = units_.begin(); u != units_.end(); ++u) {
		if(!(u->second.side() != team_num_ || u->second.can_recruit() || u->second.movement_left() <= 0 || u->second.incapacitated())) {
			break;
		}
	}

	if(u == units_.end()) {
		LOG_AI  << "no eligible units found\n";
		return std::pair<location,location>();
	}

	//guardian units stay put
	if(u->second.is_guardian()) {
		LOG_AI << u->second.type().name() << " is guardian, staying still\n";
		return std::pair<location,location>(u->first,u->first);
	}

	const move_cost_calculator cost_calc(u->second, map_, gameinfo_, units_, u->first, dstsrc, enemy_dstsrc);

	//choose the best target for that unit
	for(std::vector<target>::iterator tg = targets.begin(); tg != targets.end(); ++tg) {
		if(avoided_locations().count(tg->loc) > 0) {
			continue;
		}

		user_interact();

		wassert(map_.on_board(tg->loc));
		const double locStopValue = minimum(tg->value / best_rating, 500.0);
		paths::route cur_route = a_star_search(u->first, tg->loc, locStopValue, &cost_calc, map_.x(), map_.y());

		if (cur_route.move_left < locStopValue)
		{
			// if this unit can move to that location this turn, it has a very very low cost
			typedef std::multimap<gamemap::location,gamemap::location>::const_iterator multimapItor;
			std::pair<multimapItor,multimapItor> locRange = dstsrc.equal_range(u->first);
			while (locRange.first != locRange.second) {
				if (locRange.first->second == u->first) {
					cur_route.move_left = 0;
				}
				++locRange.first;
			}
		}

		double rating = tg->value/maximum<int>(1,cur_route.move_left);

		//for 'support' targets, they are rated much higher if we can get there within two turns,
		//otherwise they are worthless to go for at all.
		if(tg->type == target::SUPPORT) {
			if(cur_route.move_left <= u->second.movement_left()*2) {
				rating *= 10.0;
			} else {
				rating = 0.0;
			}
		}

		//scouts do not like encountering enemies on their paths
		if(u->second.type().usage() == "scout") {
			std::set<location> enemies_guarding;
			enemies_along_path(cur_route.steps,enemy_dstsrc,enemy_srcdst,enemies_guarding);

			if(enemies_guarding.size() > 1) {
				rating /= enemies_guarding.size();
			} else {
				//scouts who can travel on their route without coming in range of many enemies
				//get a massive bonus, so that they can be processed first, and avoid getting
				//bogged down in lots of grouping
				rating *= 100;
			}

			//scouts also get a bonus for going after villages
			if(tg->type == target::VILLAGE) {
				rating *= lexical_cast_default<int>(current_team().ai_parameters()["scout_village_targetting"],3);
			}
		}

		LOG_AI << tg->value << "/" << cur_route.move_left << " = " << rating << "\n";
		if(best_target == targets.end() || rating > best_rating) {
			best_rating = rating;
			best_target = tg;
			best = u;
			best_route = cur_route;
		}
	}

	LOG_AI << "chose target...\n";


	if(best_target == targets.end()) {
		LOG_AI << "no eligible targets found\n";
		return std::pair<location,location>();
	}

	//if we have the 'simple_targetting' flag set, then we don't see if any other
	//units can put a better bid forward for this target
	const bool dumb_ai = current_team().ai_parameters()["simple_targetting"] == "yes";

	if(dumb_ai == false) {
		LOG_AI << "complex targetting...\n";
		//now see if any other unit can put a better bid forward
		for(++u; u != units_.end(); ++u) {
			if(u->second.side() != team_num_ || u->second.can_recruit() ||
			   u->second.movement_left() <= 0 || u->second.is_guardian() || u->second.incapacitated()) {
				continue;
			}
	
			user_interact();
	
			const move_cost_calculator calc(u->second, map_, gameinfo_, units_, u->first, dstsrc, enemy_dstsrc);
			const double locStopValue = minimum(best_target->value / best_rating, 100.0);
			paths::route cur_route = a_star_search(u->first, best_target->loc, locStopValue, &calc, map_.x(), map_.y());
			
			if (cur_route.move_left < locStopValue)
			{
				// if this unit can move to that location this turn, it has a very very low cost
				typedef std::multimap<gamemap::location,gamemap::location>::const_iterator multimapItor;
				std::pair<multimapItor,multimapItor> locRange = dstsrc.equal_range(u->first);
				while (locRange.first != locRange.second) {
					if (locRange.first->second == u->first) {
						cur_route.move_left = 0;
					}
					++locRange.first;
				}
			}
			
			double rating = best_target->value/maximum<int>(1,cur_route.move_left);

			//for 'support' targets, they are rated much higher if we can get there within two turns,
			//otherwise they are worthless to go for at all.
			if(best_target->type == target::SUPPORT) {
				if(cur_route.move_left <= u->second.movement_left()*2) {
					rating *= 10.0;
				} else {
					rating = 0.0;
				}
			}

			//scouts do not like encountering enemies on their paths
			if(u->second.type().usage() == "scout") {
				std::set<location> enemies_guarding;
				enemies_along_path(cur_route.steps,enemy_dstsrc,enemy_srcdst,enemies_guarding);

				if(enemies_guarding.size() > 1) {
					rating /= enemies_guarding.size();
				} else {
					rating *= 100;
				}
			}

			if(best == units_.end() || rating > best_rating) {
				best_rating = rating;
				best = u;
				best_route = cur_route;
			}
		}

		LOG_AI << "done complex targetting...\n";
	} else {
		u = units_.end();
	}

	LOG_AI << "best unit: " << (best->first.x+1) << "," << (best->first.y+1) << "\n";

	wassert(best_target >= targets.begin() && best_target < targets.end());

	for(ittg = targets.begin(); ittg != targets.end(); ++ittg) {
		wassert(map_.on_board(ittg->loc));
	}

	//if our target is a position to support, then we
	//see if we can move to a position in support of this target
	if(best_target->type == target::SUPPORT) {
		LOG_AI << "support...\n";

		std::vector<location> locs;
		access_points(srcdst,best->first,best_target->loc,locs);

		if(locs.empty() == false) {
			LOG_AI << "supporting unit at " << best_target->loc.x + 1 << "," << best_target->loc.y + 1 << "\n";
			location best_loc;
			int best_defense = 0;
			double best_vulnerability = 0.0;
			int best_distance = 0;

			for(std::vector<location>::const_iterator i = locs.begin(); i != locs.end(); ++i) {
				const int distance = distance_between(*i,best_target->loc);
				const int defense = best->second.defense_modifier(map_,map_.get_terrain(*i));
				const double vulnerability = power_projection(*i,enemy_srcdst,enemy_dstsrc);
				
				if(best_loc.valid() == false || defense < best_defense || defense == best_defense && vulnerability < best_vulnerability) {
					best_loc = *i;
					best_defense = defense;
					best_vulnerability = vulnerability;
					best_distance = distance;
				}
			}

			LOG_AI << "returning support...\n";
			return std::pair<location,location>(best->first,best_loc);
		}
	}

	std::map<gamemap::location,paths> dummy_possible_moves;
	move_map fullmove_srcdst;
	move_map fullmove_dstsrc;
	calculate_possible_moves(dummy_possible_moves,fullmove_srcdst,fullmove_dstsrc,false,true);

	bool dangerous = false;

	if(current_team().ai_parameters()["grouping"] != "no") {
		LOG_AI << "grouping...\n";
		const unit_map::const_iterator unit_at_target = units_.find(best_target->loc);
		int movement = best->second.movement_left();

		const bool defensive_grouping = current_team().ai_parameters()["grouping"] == "defensive";

		//we stop and consider whether the route to this
		//target is dangerous, and whether we need to group some units to move in unison toward the target
		//if any point along the path is too dangerous for our single unit, then we hold back
		for(std::vector<location>::const_iterator i = best_route.steps.begin(); i != best_route.steps.end() && movement > 0; ++i) {
			
			const double threat = power_projection(*i,enemy_srcdst,enemy_dstsrc);
			if(threat >= double(best->second.hitpoints()) && threat > power_projection(*i,fullmove_srcdst,fullmove_dstsrc) ||
			   i >= best_route.steps.end()-2 && unit_at_target != units_.end() && current_team().is_enemy(unit_at_target->second.side())) {
				dangerous = true;
				break;
			}

			if(!defensive_grouping) {
				movement -= best->second.movement_cost(map_,map_.get_terrain(*i));
			}
		}

		LOG_AI << "done grouping...\n";
	}

	if(dangerous) {
		LOG_AI << "dangerous path\n";
		std::set<location> group, enemies;
		const location dst = form_group(best_route.steps,dstsrc,srcdst,group);
		enemies_along_path(best_route.steps,enemy_dstsrc,enemy_srcdst,enemies);

		const double our_strength = compare_groups(group,enemies,best_route.steps);

		if(our_strength > 0.5 + current_team().caution()) {
			LOG_AI << "moving group\n";
			const bool res = move_group(dst,best_route.steps,group);
			if(res) {
				return std::pair<location,location>(location(1,1),location());
			} else {
				LOG_AI << "group didn't move " << group.size() << "\n";

				//the group didn't move, so end the first unit in the group's turn, to prevent an infinite loop
				return std::pair<location,location>(best->first,best->first);

			}
		} else {
			LOG_AI << "massing to attack " << best_target->loc.x + 1 << "," << best_target->loc.y + 1
				<< " " << our_strength << "\n";

			const double value = best_target->value;
			const location target_loc = best_target->loc;
			const location loc = best->first;
			const unit& un = best->second;

			targets.erase(best_target);

			//find the best location to mass units at for an attack on the enemies
			location best_loc;
			double best_threat = 0.0;
			int best_distance = 0;

			const double max_acceptable_threat = un.hitpoints()/4;

			std::set<location> mass_locations;

			const std::pair<move_map::const_iterator,move_map::const_iterator> itors = srcdst.equal_range(loc);
			for(move_map::const_iterator i = itors.first; i != itors.second; ++i) {
				const int distance = distance_between(target_loc,i->second);
				const int defense = un.defense_modifier(map_,map_.get_terrain(i->second));
				const double threat = (power_projection(i->second,enemy_srcdst,enemy_dstsrc)*defense)/100;

				if(best_loc.valid() == false || threat < maximum<double>(best_threat,max_acceptable_threat) && distance < best_distance) {
					best_loc = i->second;
					best_threat = threat;
					best_distance = distance;
				}

				if(threat < max_acceptable_threat) {
					mass_locations.insert(i->second);
				}
			}

			for(std::set<location>::const_iterator j = mass_locations.begin(); j != mass_locations.end(); ++j) {
				if(*j != best_loc && distance_between(*j,best_loc) < 3) {
					targets.push_back(target(*j,value*4.0,target::MASS));
				}
			}
			
			return std::pair<location,location>(loc,best_loc);
		}
	}

	for(std::vector<location>::reverse_iterator ri =
	    best_route.steps.rbegin(); ri != best_route.steps.rend(); ++ri) {

		if(game_config::debug) {
			//display::debug_highlight(*ri,0.2);
		}

		//this is set to 'true' if we are hesitant to proceed because of enemy units,
		//to rally troops around us.
		bool is_dangerous = false;

		typedef std::multimap<location,location>::const_iterator Itor;
		std::pair<Itor,Itor> its = dstsrc.equal_range(*ri);
		while(its.first != its.second) {
			if(its.first->second == best->first) {
				if(!should_retreat(its.first->first,best,fullmove_srcdst,fullmove_dstsrc,enemy_srcdst,enemy_dstsrc)) {
					const double value = best_target->value - best->second.type().cost()/20.0;

					if(value > 0.0 && best_target->type != target::MASS) {
						//there are enemies ahead. Rally troops around us to
						//try to take the target
						if(is_dangerous) {
							targets.push_back(target(its.first->first,value*2.0,target::BATTLE_AID));
						}

						best_target->value = value;
					} else {
						targets.erase(best_target);
					}

					LOG_AI << "Moving to " << its.first->first.x + 1 << "," << its.first->first.y + 1 << "\n";
				
					return std::pair<location,location>(its.first->second,its.first->first);
				} else {
					LOG_AI << "dangerous!\n";
					is_dangerous = true;
				}
			}

			++its.first;
		}
	}

	if(best != units_.end()) {
		LOG_AI << "Could not make good move, staying still\n";

		//this sounds like the road ahead might be dangerous, and that's why we don't advance.
		//create this as a target, attempting to rally units around
		targets.push_back(target(best->first,best_target->value));
		return std::pair<location,location>(best->first,best->first);
	}

	LOG_AI << "Could not find anywhere to move!\n";
	return std::pair<location,location>();
}

void ai::access_points(const move_map& srcdst, const location& u, const location& dst, std::vector<location>& out)
{
	const unit_map::const_iterator u_it = units_.find(u);
	if(u_it == units_.end()) {
		return;
	}

	unit_map single_unit;
	single_unit.insert(*u_it);

	const std::pair<move_map::const_iterator,move_map::const_iterator> locs = srcdst.equal_range(u);
	for(move_map::const_iterator i = locs.first; i != locs.second; ++i) {
		const location& loc = i->second;
		if (int(distance_between(loc,dst)) <= u_it->second.total_movement()) {
			shortest_path_calculator calc(u_it->second, current_team(), units_, teams_, map_, state_);
			const paths::route& rt = a_star_search(loc, dst, u_it->second.total_movement(), &calc, map_.x(), map_.y());
			if(rt.steps.empty() == false) {
				out.push_back(loc);
			}
		}
	}
}
