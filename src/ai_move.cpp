/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "ai.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "variable.hpp"
#include "wml_exception.hpp"

#include <cassert>
#include <iostream>

#define LOG_AI LOG_STREAM(info, ai)
#define DBG_AI LOG_STREAM(debug, ai)
#define ERR_AI LOG_STREAM(err, ai)

struct move_cost_calculator : cost_calculator
{
	move_cost_calculator(const unit& u, const gamemap& map,
	                     const unit_map& units,
	                     const map_location& loc,
						 const ai::move_map& dstsrc,
						 const ai::move_map& enemy_dstsrc)
	  : unit_(u), map_(map), units_(units),
	    loc_(loc), dstsrc_(dstsrc), enemy_dstsrc_(enemy_dstsrc),
		avoid_enemies_(u.usage() == "scout")
	{}

	virtual double cost(const map_location&, const map_location& loc, const double) const
	{
		/*
		if(!map_.on_board(loc))
			return 1000.0;

		// if this unit can move to that location this turn, it has a very very low cost
		typedef std::multimap<map_location,map_location>::const_iterator Itor;
		std::pair<Itor,Itor> range = dstsrc_.equal_range(loc);
		while(range.first != range.second) {
			if(range.first->second == loc_) {
				return 0.01;
			}
			++range.first;
		}
		*/
		assert(map_.on_board(loc));

		const t_translation::t_terrain terrain = map_[loc];

		const double modifier = 1.0; //move_type_.defense_modifier(map_,terrain);
		const double move_cost = unit_.movement_cost(terrain);//move_type_[terrain];

		const int enemies = 1 + (avoid_enemies_ ? enemy_dstsrc_.count(loc) : 0);
		double res = modifier*move_cost*double(enemies);

		//if there is a unit (even a friendly one) on this tile, we increase the cost to
		//try discourage going through units, to thwart the 'single file effect'
		if (units_.count(loc))
			res *= 4.0;

		VALIDATE(res > 0,
			_("Movement cost is 0, probably a terrain with movement cost of 0."));
		return res;
	}

private:
	const unit& unit_;
	const gamemap& map_;
	const unit_map& units_;
//	mutable std::map<t_translation::t_terrain,int> move_type_;
	const map_location loc_;
	const ai::move_map dstsrc_, enemy_dstsrc_;
	const bool avoid_enemies_;
};

std::vector<ai::target> ai::find_targets(unit_map::const_iterator leader, const move_map& enemy_dstsrc)
{
	log_scope2(ai, "finding targets...");

	const bool has_leader = leader != units_.end();

	std::vector<target> targets;

	std::map<location,paths> friends_possible_moves;
	move_map friends_srcdst, friends_dstsrc;
	calculate_possible_moves(friends_possible_moves,friends_srcdst,friends_dstsrc,false,true);

	//if enemy units are in range of the leader, then we target the enemies who are in range.
	if(has_leader) {
		const double threat = power_projection(leader->first,enemy_dstsrc);
		if(threat > 0.0) {
			//find the location of enemy threats
			std::set<map_location> threats;

			map_location adj[6];
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

			assert(threats.empty() == false);

#ifdef SUOKKO
			//FIXME: sukko's veraion 29531 included this change.  Correct?
			const double value = threat*lexical_cast_default<double>(current_team().ai_parameters()["protect_leader"], 3.0)/leader->second.hitpoints();
#else
			const double value = threat/double(threats.size());
#endif
			for(std::set<map_location>::const_iterator i = threats.begin(); i != threats.end(); ++i) {
				LOG_AI << "found threat target... " << *i << " with value: " << value << "\n";
				targets.push_back(target(*i,value,target::THREAT));
			}
		}
	}

	double corner_distance = distance_between(map_location(0,0), map_location(map_.w(),map_.h()));
	if(has_leader && current_team().village_value() > 0.0) {
		const std::vector<location>& villages = map_.villages();
		for(std::vector<location>::const_iterator t =
				villages.begin(); t != villages.end(); ++t) {

			assert(map_.on_board(*t));
			bool get_village = true;
			for(size_t i = 0; i != teams_.size(); ++i) {
				if(!current_team().is_enemy(i+1) && teams_[i].owns_village(*t)) {
					// check if our village is threatened
					if (is_accessible(*t, enemy_dstsrc))
					{
						// Our village is threated by enemy
						// We calculate enemy_power/our_power and multiple village value with that
						// to get value for adding more defense
						const double enemy = power_projection(*t, enemy_dstsrc)*1.7;
						const double our = power_projection(*t, friends_dstsrc);
						const double value = current_team().village_value()*our/enemy;
						add_target(target(*t, value, target::SUPPORT));
					}
					get_village = false;
					break;
				}
			}

			if(get_village) {
				double value = current_team().village_value();
				value *= 1.0 - static_cast<double>(distance_between(*t,leader->first))/corner_distance;
				LOG_AI << "found village target... " << *t << " with value: " << value << " distance: " << static_cast<double>(distance_between(*t,leader->first)) << "\n";
				targets.push_back(target(*t,value,target::VILLAGE));
			}
		}
	}

	std::vector<team::target>& team_targets = current_team().targets();

	//find the enemy leaders and explicit targets
	unit_map::const_iterator u;
	for(u = units_.begin(); u != units_.end(); ++u) {

		//is a visible enemy leader
		if (u->second.can_recruit() && current_team().is_enemy(u->second.side())
		&& !u->second.invisible(u->first, units_, teams_)) {
			assert(map_.on_board(u->first));
			LOG_AI << "found enemy leader (side: " << u->second.side() << ") target... " << u->first << " with value: " << current_team().leader_value() << "\n";
			targets.push_back(target(u->first,current_team().leader_value(),target::LEADER));
		}

		//explicit targets for this team
		for(std::vector<team::target>::iterator j = team_targets.begin();
		    j != team_targets.end(); ++j) {
			if(u->second.matches_filter(&(j->criteria),u->first)) {
				LOG_AI << "found explicit target... " << u->first << " with value: " << j->value << "\n";
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

	assert(new_values.size() == targets.size());
	for(size_t n = 0; n != new_values.size(); ++n) {
		LOG_AI << "target value: " << targets[n].value << " -> " << new_values[n] << "\n";
		targets[n].value = new_values[n];
	}

	return targets;
}

map_location ai::form_group(const std::vector<location>& route, const move_map& dstsrc, std::set<location>& res)
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

void ai::enemies_along_path(const std::vector<location>& route, const move_map& dstsrc, std::set<location>& res)
{
	for(std::vector<location>::const_iterator i = route.begin(); i != route.end(); ++i) {
		map_location adj[6];
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

			const int defense = un->second.defense_modifier(map_.get_terrain(*j));
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

			// FIXME: suokko's r29531 included the following line.  Correct?
			// units_.find(best_loc)->second.set_movement(0);

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
			defense += un.defense_modifier(map_.get_terrain(*j));
		}

		defense /= battlefield.size();

		int best_attack = 0;
		const std::vector<attack_type>& attacks = un.attacks();
		for(std::vector<attack_type>::const_iterator a = attacks.begin(); a != attacks.end(); ++a) {
			const int strength = a->num_attacks()*a->damage();
			best_attack = std::max<int>(strength,best_attack);
		}

		const int rating = (defense*best_attack*un.hitpoints())/(100*un.max_hitpoints());
		strength += double(rating);
	}

	return strength;
}

double ai::compare_groups(const std::set<location>& our_group, const std::set<location>& their_group, const std::vector<location>& battlefield) const
{
	const double a = rate_group(our_group,battlefield);
	const double b = std::max<double>(rate_group(their_group,battlefield),0.01);
	return a/b;
}

std::pair<map_location,map_location> ai::choose_move(std::vector<target>& targets, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_dstsrc)
{
	log_scope2(ai, "choosing move");

	raise_user_interact();

	std::vector<target>::const_iterator ittg;
	for(ittg = targets.begin(); ittg != targets.end(); ++ittg) {
		assert(map_.on_board(ittg->loc));
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
	if(utils::string_bool(u->second.get_state("guardian"))) {
		LOG_AI << u->second.type_id() << " is guardian, staying still\n";
		return std::pair<location,location>(u->first,u->first);
	}

	const move_cost_calculator cost_calc(u->second, map_, units_, u->first, dstsrc, enemy_dstsrc);

	//choose the best target for that unit
	for(std::vector<target>::iterator tg = targets.begin(); tg != targets.end(); ++tg) {
		if(avoided_locations().count(tg->loc) > 0) {
			continue;
		}
		LOG_AI << "Considering target at: " << tg->loc <<"\n";

		raise_user_interact();

		assert(map_.on_board(tg->loc));

		const double locStopValue = std::min(tg->value / best_rating, 500.0);
		paths::route cur_route = a_star_search(u->first, tg->loc, locStopValue, &cost_calc, map_.w(), map_.h());

		if (cur_route.move_left == cost_calc.getNoPathValue())
			continue;

		if (cur_route.move_left < locStopValue)
		{
			// if this unit can move to that location this turn, it has a very very low cost
			typedef std::multimap<map_location,map_location>::const_iterator multimapItor;
			std::pair<multimapItor,multimapItor> locRange = dstsrc.equal_range(u->first);
			while (locRange.first != locRange.second) {
				if (locRange.first->second == u->first) {
					cur_route.move_left = 0;
				}
				++locRange.first;
			}
		}

		double rating = tg->value/std::max<int>(1,cur_route.move_left);

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
		if(u->second.usage() == "scout") {
			std::set<location> enemies_guarding;
			enemies_along_path(cur_route.steps,enemy_dstsrc,enemies_guarding);

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
			if(best_rating == 0)
			{
				best_rating = 0.000000001; //prevent divivion by zero
			}
		}
	}

	LOG_AI << "chose target...\n";


	if(best_target == targets.end()) {
		LOG_AI << "no eligible targets found\n";
		return std::pair<location,location>();
	}

	//if we have the 'simple_targetting' flag set, then we don't see if any other
	//units can put a better bid forward for this target
	const bool dumb_ai = utils::string_bool(current_team().ai_parameters()["simple_targetting"]);

	if(dumb_ai == false) {
		LOG_AI << "complex targetting...\n";
		//now see if any other unit can put a better bid forward
		for(++u; u != units_.end(); ++u) {
			if(u->second.side() != team_num_ || u->second.can_recruit() ||
			   u->second.movement_left() <= 0 || utils::string_bool(u->second.get_state("guardian")) || u->second.incapacitated()) {
				continue;
			}

			raise_user_interact();

			const move_cost_calculator calc(u->second, map_, units_, u->first, dstsrc, enemy_dstsrc);
			const double locStopValue = std::min(best_target->value / best_rating, 100.0);
			paths::route cur_route = a_star_search(u->first, best_target->loc, locStopValue, &calc, map_.w(), map_.h());

			if (cur_route.move_left < locStopValue)
			{
				// if this unit can move to that location this turn, it has a very very low cost
				typedef std::multimap<map_location,map_location>::const_iterator multimapItor;
				std::pair<multimapItor,multimapItor> locRange = dstsrc.equal_range(u->first);
				while (locRange.first != locRange.second) {
					if (locRange.first->second == u->first) {
						cur_route.move_left = 0;
					}
					++locRange.first;
				}
			}

			double rating = best_target->value/std::max<int>(1,cur_route.move_left);

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
			if(u->second.usage() == "scout") {
				std::set<location> enemies_guarding;
				enemies_along_path(cur_route.steps,enemy_dstsrc,enemies_guarding);

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

	LOG_AI << "best unit: " << best->first << '\n';

	assert(best_target >= targets.begin() && best_target < targets.end());

	for(ittg = targets.begin(); ittg != targets.end(); ++ittg) {
		assert(map_.on_board(ittg->loc));
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
				const int defense = best->second.defense_modifier(map_.get_terrain(*i));
				//FIXME: suokko multiplied by 10 * current_team().caution(). ?
				const double vulnerability = power_projection(*i,enemy_dstsrc);

				if(best_loc.valid() == false || defense < best_defense || (defense == best_defense && vulnerability < best_vulnerability)) {
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

	std::map<map_location,paths> dummy_possible_moves;
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

			//FIXME: suokko multiplied by 10 * current_team().caution(). ?
			const double threat = power_projection(*i,enemy_dstsrc);
			//FIXME: sukko doubled the power-projection them in the second test.  ?
			if((threat >= double(best->second.hitpoints()) && threat > power_projection(*i,fullmove_dstsrc)) ||
			   (i >= best_route.steps.end()-2 && unit_at_target != units_.end() && current_team().is_enemy(unit_at_target->second.side()))) {
				dangerous = true;
				break;
			}

			if(!defensive_grouping) {
				movement -= best->second.movement_cost(map_.get_terrain(*i));
			}
		}

		LOG_AI << "done grouping...\n";
	}

	if(dangerous) {
		LOG_AI << "dangerous path\n";
		std::set<location> group, enemies;
		const location dst = form_group(best_route.steps,dstsrc,group);
		enemies_along_path(best_route.steps,enemy_dstsrc,enemies);

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
				const int defense = un.defense_modifier(map_.get_terrain(i->second));
				//FIXME: suokko multiplied by 10 * current_team().caution(). ?
				const double threat = (power_projection(i->second,enemy_dstsrc)*defense)/100;

				if(best_loc.valid() == false || (threat < std::max<double>(best_threat,max_acceptable_threat) && distance < best_distance)) {
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
					LOG_AI << "found mass-to-attack target... " << *j << " with value: " << value*4.0 << "\n";
					targets.push_back(target(*j,value*4.0,target::MASS));
					best_target = targets.end() - 1;
				}
			}

			return std::pair<location,location>(loc,best_loc);
		}
	}

	for(std::vector<location>::reverse_iterator ri =
	    best_route.steps.rbegin(); ri != best_route.steps.rend(); ++ri) {

		if(game_config::debug) {
			//game_display::debug_highlight(*ri,static_cast<size_t>(0.2));
		}

		//this is set to 'true' if we are hesitant to proceed because of enemy units,
		//to rally troops around us.
		bool is_dangerous = false;

		typedef std::multimap<location,location>::const_iterator Itor;
		std::pair<Itor,Itor> its = dstsrc.equal_range(*ri);
		while(its.first != its.second) {
			if(its.first->second == best->first) {
				if(!should_retreat(its.first->first,best,fullmove_srcdst,fullmove_dstsrc,enemy_dstsrc,
								   current_team().caution())) {
					const double value = best_target->value - best->second.cost()/20.0;

					if(value > 0.0 && best_target->type != target::MASS) {
						//there are enemies ahead. Rally troops around us to
						//try to take the target
						if(is_dangerous) {
							LOG_AI << "found reinforcement target... " << its.first->first << " with value: " << value*2.0 << "\n";
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
		best_target = targets.end() - 1;
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

	// unit_map single_unit(u_it->first, u_it->second);

	const std::pair<move_map::const_iterator,move_map::const_iterator> locs = srcdst.equal_range(u);
	for(move_map::const_iterator i = locs.first; i != locs.second; ++i) {
		const location& loc = i->second;
		if (int(distance_between(loc,dst)) <= u_it->second.total_movement()) {
			shortest_path_calculator calc(u_it->second, current_team(), units_, teams_, map_);
			const paths::route& rt = a_star_search(loc, dst, u_it->second.total_movement(), &calc, map_.w(), map_.h());
			if(rt.steps.empty() == false) {
				out.push_back(loc);
			}
		}
	}
}

namespace {
        struct match_turn{
                match_turn(int turn) : turn_(turn)
                {
                }
                bool operator()(const std::map<map_location, paths::route::waypoint>::value_type& way)
                {
                        return way.second.turns == turn_;
                }
                private:
                int& turn_;
        };
}

void ai::move_leader_to_keep(const move_map& enemy_dstsrc)
{
	const unit_map::iterator leader = find_leader(units_,team_num_);
	if(leader == units_.end()
		|| leader->second.incapacitated()
		|| leader->second.movement_left() == 0) {
		return;
	}

	std::map<map_location,paths> possible_moves;
	std::map<map_location,paths>::iterator path_itor = possible_moves.insert(std::make_pair(leader->first, paths())).first;

	// Guess how many units we want to recruit
	const int number_of_recruit = current_team().gold() / current_team().average_recruit_price();

	// If the leader is not on his starting location, move him there.
	if (number_of_recruit)
	{
		{
			// Make a map of the possible locations the leader can move to,
			// ordered by the distance from the keep.
			int best_value = INT_MAX - 1;
			map_location best_target;

			const shortest_path_calculator cost_calc(leader->second,
					current_team(),
					units_, teams_,
					map_);

			std::set<map_location> allowed_teleports;

			if(leader->second.get_ability_bool("teleport",leader->first)) {
				// search all known empty friendly villages
				for(std::set<map_location>::const_iterator vil = current_team().villages().begin();
						vil != current_team().villages().end(); ++vil) {

					unit_map::const_iterator occupant = units_.find(*vil);
					if (occupant != units_.end() && occupant != leader)
						continue;

					allowed_teleports.insert(*vil);
				}
			}

			// The leader can't move to his keep, try to move to the closest location
			// to the keep where there are no enemies in range.
			for(std::set<map_location>::iterator i = keeps().begin();
					i != keeps().end(); ++i) {

				const double limit = std::min(10000, best_value + 1);
				paths::routes_map::iterator route = path_itor->second.routes.insert(
						std::make_pair(*i,
							a_star_search(leader->first, *i, limit, &cost_calc,map_.w(), map_.h(), &allowed_teleports
								))).first;
				if (route->second.steps.empty())
				{
					path_itor->second.routes.erase(route);
					continue;
				}

				std::set<map_location> checked_hexes;
				checked_hexes.insert(*i);
				const int free_slots = count_free_hexes_in_castle(*i, checked_hexes);
				map_location center(map_.w()/2, map_.h()/2);
				const int tactical_value = distance_between(center, *i);
				const int empty_slots = leader->second.total_movement() * std::max(number_of_recruit - free_slots,0);
				unit_map::const_iterator u = units_.find(*i);
				const int reserved_penalty = leader->second.total_movement() *
					(u != units_.end()
					 && &teams_[u->second.side()-1] != &current_team()
					 && !u->second.invisible(u->first, units_, teams_)
					 ?(current_team().is_enemy(u->second.side())?6:3)
					 :0);

				int value = empty_slots + tactical_value + reserved_penalty;

				// do enemy power projection so we know where enemy is
				int enemy = leader->second.hitpoints() - (power_projection(*i, enemy_dstsrc) * 3);
				if (enemy > 0)
					enemy = ((leader->second.hitpoints() - enemy)*leader->second.total_movement()*4 / leader->second.hitpoints())*-1;
				else
					enemy = (enemy * leader->second.total_movement() * -4)/leader->second.hitpoints();

				value += enemy;


				// Make first quess if this is worth calculateing
				if (value + static_cast<int>(route->second.steps.size()) - 1 >= best_value)
					continue;

				int distance = route_turns_to_complete(leader->second,
						route->second,
						current_team(),
						units_,
						teams_,
						map_) * leader->second.total_movement();

				map_location target;

				std::map<map_location, paths::route::waypoint>::iterator target_it = std::find_if(route->second.waypoints.begin(),
						route->second.waypoints.end(),
						match_turn(1));
				if(target_it != route->second.waypoints.end()) {
					target = target_it->first;
				} else if(*i == leader->first) {
					target = leader->first;
				} else {
					ERR_AI << "ai::move_leader_to_keep - leader at " << leader->first
					<< " could not find path to keep at " << *i << std::endl;
					continue;
				}

				int multiturn_move_penalty = 0;
				if (recruiting_preferred_)
					multiturn_move_penalty = 2;

				const int distance_value = (distance > leader->second.movement_left()?
						((distance - leader->second.movement_left())/leader->second.total_movement()+multiturn_move_penalty)*leader->second.total_movement() : 0);
				value += distance_value;

				if (value >= best_value)
					continue;

				best_value = value;
				best_target = target;
				DBG_AI << "Considering keep: " << *i <<
					" empty slots: " << empty_slots <<
					" distance: " << distance <<
					" enemy: " << enemy <<
					" tactical_value: " << tactical_value <<
					" reserved_penalty: " << reserved_penalty <<
					" target: " << target <<
					" value: " << value <<
					" route: " << route->second.steps.size() << " " << route->second.move_left <<
					"\n";
			}

			// Find the location with the best value
			if (leader->first != best_target
					&& best_target != location::null_location)
				move_unit(leader->first,best_target,possible_moves);
		}
	}
}

int ai::count_free_hexes_in_castle(const map_location& loc, std::set<map_location>& checked_hexes)
{
	int ret = 0;
	location adj[6];
	get_adjacent_tiles(loc,adj);
	for(size_t n = 0; n != 6; ++n) {
		if (checked_hexes.find(adj[n]) != checked_hexes.end())
			continue;
		checked_hexes.insert(adj[n]);
		if (map_.is_castle(adj[n])) {
			const unit_map::const_iterator u = units_.find(adj[n]);
			ret += count_free_hexes_in_castle(adj[n], checked_hexes);
			if (u == units_.end()
				|| (current_team().is_enemy(u->second.side())
					&& u->second.invisible(adj[n], units_, teams_))
				|| ((&teams_[u->second.side()-1]) == &current_team()
					&& u->second.movement_left() > 0)) {
				ret += 1;
			}
		}
	}
	return ret;
}


