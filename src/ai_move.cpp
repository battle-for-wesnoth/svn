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
#include "ai_move.hpp"
#include "display.hpp"
#include "game_config.hpp"
#include "log.hpp"
#include "util.hpp"

#include <iostream>

namespace ai {

struct move_cost_calculator
{
	move_cost_calculator(const unit& u, const gamemap& map,
	                     const game_data& data,
					 	 const std::map<location,unit>& units,
	                     const gamemap::location& loc,
	                     const std::multimap<location,location>& dstsrc)
	  : unit_(u), map_(map), data_(data), units_(units),
	    move_type_(u.type().movement_type()), loc_(loc), dstsrc_(dstsrc)
	{}

	double cost(const location& loc, double so_far) const
	{
		if(!map_.on_board(loc))
			return 1000.0;

		//if this unit can move to that location this turn, it has a very
		//very low cost
		typedef std::multimap<location,location>::const_iterator Itor;
		std::pair<Itor,Itor> range = dstsrc_.equal_range(loc);
		while(range.first != range.second) {
			if(range.first->first == loc)
				return 0.01;
			++range.first;
		}

		const gamemap::TERRAIN terrain =
		                    map_.underlying_terrain(map_[loc.x][loc.y]);

		const double modifier = 1.0;//move_type_.defense_modifier(map_,terrain);
		const double move_cost = move_type_.movement_cost(map_,terrain);

		double enemies = 0;
	/*	//is this stuff responsible for making it take a long time?
		location adj[7];
		adj[0] = loc;
		get_adjacent_tiles(loc,adj+1);
		for(int i = 0; i != 7; ++i) {
			const std::map<location,unit>::const_iterator en=units_.find(adj[i]);
			//at the moment, don't allow any units to be in the path
			if(i == 0 && en != units_.end()) {
				return 1000.0;
			}

			if(en != units_.end() && en->second.side() == enemy_) {
				enemies += 1.0;
			}
		}
*/
		const double res = modifier*move_cost + enemies*2.0;
		assert(res > 0);
		return res;
	}

private:
	const unit& unit_;
	const gamemap& map_;
	const game_data& data_;
	const std::map<location,unit>& units_;
	const unit_movement_type& move_type_;
	const gamemap::location loc_;
	const std::multimap<location,location> dstsrc_;

};

std::vector<target> find_targets(
                           const gamemap& map, std::map<location,unit>& units,
						   std::vector<team>& teams, int current_team
						  )
{
	log_scope("finding targets...");

	team& tm = teams[current_team-1];

	std::vector<target> targets;

	if(tm.village_value() > 0.0) {
		const std::vector<location>& towers = map.towers();
		for(std::vector<location>::const_iterator t = towers.begin();
		    t != towers.end(); ++t) {
			assert(map.on_board(*t));
			bool get_tower = true;
			for(size_t i = 0; i != teams.size(); ++i) {
				if(!tm.is_enemy(i+1) && teams[i].owns_tower(*t)) {
					get_tower = false;
					break;
				}
			}

			if(get_tower) {
				targets.push_back(target(*t,tm.village_value()));
			}
		}
	}

	std::vector<team::target>& team_targets = tm.targets();

	//find the enemy leaders and explicit targets
	std::map<location,unit>::const_iterator u;
	for(u = units.begin(); u != units.end(); ++u) {

		//is an enemy leader
		if(u->second.can_recruit() && tm.is_enemy(u->second.side())) {
			assert(map.on_board(u->first));
			targets.push_back(target(u->first,tm.leader_value()));
		}

		//explicit targets for this team
		for(std::vector<team::target>::iterator j = team_targets.begin();
		    j != team_targets.end(); ++j) {
			if(u->second.matches_filter(j->criteria)) {
				std::cerr << "found explicit target..." << j->value << "\n";
				targets.push_back(target(u->first,j->value));
			}
		}
	}

	std::vector<double> new_values;

	for(std::vector<target>::iterator i = targets.begin();
	    i != targets.end(); ++i) {

		new_values.push_back(i->value);

		for(std::vector<target>::const_iterator j = targets.begin();
		    j != targets.end(); ++j) {
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
		std::cerr << "target value: " << targets[n].value << " -> " << new_values[n] << "\n";
		targets[n].value = new_values[n];
	}

	return targets;
}

std::pair<location,location> choose_move(
                           std::vector<target>& targets,
                           const std::multimap<location,location>& dstsrc,
						   std::map<location,unit>& units,
						   const gamemap& map, const std::vector<team>& teams,
						   int current_team,
						   const game_data& data
						  )
{
	log_scope("choosing move");

	std::vector<target>::const_iterator ittg;
	for(ittg = targets.begin(); ittg != targets.end(); ++ittg) {
		assert(map.on_board(ittg->loc));
	}

	paths::route best_route;
	std::map<location,unit>::iterator best = units.end();
	double best_rating = 0.1;
	std::vector<target>::iterator best_target = targets.end();

	std::map<location,unit>::iterator u;

	//find the first eligible unit
	for(u = units.begin(); u != units.end(); ++u) {
		if(!(u->second.side() != current_team || u->second.can_recruit() ||
		   u->second.movement_left() <= 0)) {
			break;
		}
	}

	if(u == units.end()) {
		std::cout << "no eligible units found\n";
		return std::pair<location,location>();
	}

	//guardian units stay put
	if(u->second.is_guardian()) {
		std::cerr << u->second.type().name() << " is guardian, staying still\n";
		return std::pair<location,location>(u->first,u->first);
	}

	const move_cost_calculator cost_calc(u->second,map,data,units,
	                                     u->first,dstsrc);

	//choose the best target for that unit
	for(std::vector<target>::iterator tg = targets.begin();
	    tg != targets.end(); ++tg) {
		assert(map.on_board(tg->loc));
		const paths::route cur_route = a_star_search(u->first,tg->loc,
		                       minimum(tg->value/best_rating,100.0),cost_calc);
		const double rating = tg->value/cur_route.move_left;
		std::cerr << tg->value << "/" << cur_route.move_left << " = " << rating << "\n";
		if(best_target == targets.end() || rating > best_rating) {
			best_rating = rating;
			best_target = tg;
			best = u;
			best_route = cur_route;
		}
	}


	if(best_target == targets.end()) {
		std::cout << "no eligible targets found\n";
		return std::pair<location,location>();
	}

	//now see if any other unit can put a better bid forward
	for(++u; u != units.end(); ++u) {
		if(u->second.side() != current_team || u->second.can_recruit() ||
		   u->second.movement_left() <= 0 || u->second.is_guardian()) {
			continue;
		}

		const move_cost_calculator calc(u->second,map,data,units,
		                                u->first,dstsrc);
		const paths::route cur_route = a_star_search(u->first,best_target->loc,
			               minimum(best_target->value/best_rating,100.0),calc);
		const double rating = best_target->value/cur_route.move_left;
		if(best == units.end() || rating > best_rating) {
			best_rating = rating;
			best = u;
			best_route = cur_route;
		}
	}

	assert(best_target >= targets.begin() && best_target < targets.end());
	best_target->value -= best->second.type().cost()/20.0;
	if(best_target->value <= 0.0)
		targets.erase(best_target);

	for(ittg = targets.begin();
	    ittg != targets.end(); ++ittg) {
		assert(map.on_board(ittg->loc));
	}

	for(std::vector<location>::reverse_iterator ri =
	    best_route.steps.rbegin(); ri != best_route.steps.rend(); ++ri) {

		if(game_config::debug) {
			display::debug_highlight(*ri,0.2);
		}

		typedef std::multimap<location,location>::const_iterator Itor;
		std::pair<Itor,Itor> its = dstsrc.equal_range(*ri);
		while(its.first != its.second) {
			if(its.first->second == best->first) {
				return std::pair<location,location>(its.first->second,
				                                    its.first->first);
			}

			++its.first;
		}
	}

	if(best != units.end()) {
		std::cout << "Could not make good move, staying still\n";
		return std::pair<location,location>(best->first,best->first);
	}

	std::cout << "Could not find anywhere to move!\n";
	return std::pair<location,location>();
}

}
