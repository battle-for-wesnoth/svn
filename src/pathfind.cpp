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

#include "game.hpp"
#include "log.hpp"
#include "pathfind.hpp"
#include "util.hpp"
#include "wassert.hpp"

#include <cmath>
#include <iostream>

#define LOG_PF lg::info(lg::engine)

namespace {
struct node {
	static double heuristic(const gamemap::location& src,
	                        const gamemap::location& dst) {
		return distance_between(src,dst);
	}

	node(const gamemap::location& pos, const gamemap::location& dst,
		double cost, const gamemap::location& parent,
	     const std::set<gamemap::location>* teleports)
	    : loc(pos), parent(parent), g(cost), h(heuristic(pos,dst))
	{

		//if there are teleport locations, correct the heuristic to
		//take them into account
		if(teleports != NULL) {
			double srch = h, dsth = h;
			std::set<gamemap::location>::const_iterator i;
			for(i = teleports->begin(); i != teleports->end(); ++i) {
				const double new_srch = heuristic(pos,*i);
				const double new_dsth = heuristic(*i,dst);
				if(new_srch < srch) {
					srch = new_srch;
				}

				if(new_dsth < dsth) {
					dsth = new_dsth;
				}
			}

			if(srch + dsth + 1.0 < h) {
				h = srch + dsth + 1.0;
			}
		}

		f = g + h;
	}

	gamemap::location loc, parent;
	// g: already traveled time, h: estimated time still to travel, f: estimated full time
	double g, h, f;
};

}

paths::route a_star_search(gamemap::location const &src, gamemap::location const &dst,
                           double stop_at, cost_calculator const *obj,
                           std::set<gamemap::location> const *teleports)
{
	LOG_PF << "a* search: " << src.x << ", " << src.y << " -> " << dst.x << ", " << dst.y << "\n";
	typedef gamemap::location location;

	typedef std::map<location,node> list_map;
	typedef std::pair<location,node> list_type;

	std::multimap<double,location> open_list_ordered;
	list_map open_list, closed_list;

	open_list.insert(list_type(src,node(src,dst,0.0,location(),teleports)));
	open_list_ordered.insert(std::pair<double,location>(0.0,src));

	std::vector<location> locs;
	while(!open_list.empty()) {

		wassert(open_list.size() == open_list_ordered.size());

		const list_map::iterator lowest_in_open = open_list.find(open_list_ordered.begin()->second);
		wassert(lowest_in_open != open_list.end());

		//move the lowest element from the open list to the closed list
		closed_list.erase(lowest_in_open->first);
		const list_map::iterator lowest = closed_list.insert(*lowest_in_open).first;

		open_list.erase(lowest_in_open);
		open_list_ordered.erase(open_list_ordered.begin());

		//find nodes we can go to from this node
		locs.resize(6);
		get_adjacent_tiles(lowest->second.loc,&locs[0]);
		if(teleports != NULL && teleports->count(lowest->second.loc) != 0) {
			std::copy(teleports->begin(),teleports->end(),std::back_inserter(locs));
		}

		double const lowest_gcost = lowest->second.g;

		for(size_t j = 0; j != locs.size(); ++j) {

			//if we have found a solution
			if(locs[j] == dst) {
				LOG_PF << "found solution; calculating it...\n";
				paths::route rt;

				for(location loc = lowest->second.loc; loc.valid(); ) {
					rt.steps.push_back(loc);
					list_map::const_iterator itor = open_list.find(loc);
					if(itor == open_list.end()) {
						itor = closed_list.find(loc);
						wassert(itor != closed_list.end());
					}

					loc = itor->second.parent;
				}
				
				std::reverse(rt.steps.begin(),rt.steps.end());
				rt.steps.push_back(dst);
				rt.move_left = int(lowest_gcost + obj->cost(dst, lowest_gcost));

				wassert(rt.steps.front() == src);

				LOG_PF << "exiting a* search (solved)\n";

				return rt;
			}

			list_map::iterator current_best = open_list.find(locs[j]);
			const bool in_open = current_best != open_list.end();
			bool in_lists = in_open;
			if (!in_lists) {
				current_best = closed_list.find(locs[j]);
				in_lists = current_best != closed_list.end();
			}
			double current_gcost = 0;
			if (in_lists) {
				current_gcost = current_best->second.g;
				if (current_gcost <= lowest_gcost + 1.0)
					continue;
			}

			double const new_cost = obj->cost(locs[j], lowest_gcost);

			node const nd(locs[j], dst, lowest_gcost + new_cost,
			              lowest->second.loc,teleports);

			if (in_lists) {
				if (current_gcost <= nd.g) {
					continue;
				} else if(in_open) {
					typedef std::multimap<double,location>::iterator Itor;
					std::pair<Itor,Itor> itors = open_list_ordered.equal_range(current_best->second.f);
					while(itors.first != itors.second) {
						if(itors.first->second == current_best->second.loc) {
							open_list_ordered.erase(itors.first);
							break;
						}
						++itors.first;
					}

					open_list.erase(current_best);
				} else {
					closed_list.erase(current_best);
				}
			}

			if(nd.f < stop_at) {
				open_list.insert(list_type(nd.loc,nd));
				open_list_ordered.insert(std::pair<double,location>(nd.f,nd.loc));
			} else {
				closed_list.insert(list_type(nd.loc,nd));
			}
		}
	}

	LOG_PF << "aborted a* search\n";
	paths::route val;
	val.move_left = 100000;
	return val;
}

namespace {
gamemap::location find_vacant(const gamemap& map,
                              const std::map<gamemap::location,unit>& units,
							  const gamemap::location& loc, int depth,
                              VACANT_TILE_TYPE vacancy,
                              std::set<gamemap::location>& touched)
{
	if(touched.count(loc))
		return gamemap::location();

	touched.insert(loc);

	if(map.on_board(loc) && units.find(loc) == units.end() &&
	   (vacancy == VACANT_ANY || map.is_castle(loc))) {
		return loc;
	} else if(depth == 0) {
		return gamemap::location();
	} else {
		gamemap::location adj[6];
		get_adjacent_tiles(loc,adj);
		for(int i = 0; i != 6; ++i) {
			if(!map.on_board(adj[i]) || vacancy == VACANT_CASTLE && !map.is_castle(adj[i]))
				continue;

			const gamemap::location res =
			       find_vacant(map,units,adj[i],depth-1,vacancy,touched);

			if(map.on_board(res))
				return res;
		}

		return gamemap::location();
	}
}

}

gamemap::location find_vacant_tile(const gamemap& map,
                                const std::map<gamemap::location,unit>& units,
                                const gamemap::location& loc,
                                VACANT_TILE_TYPE vacancy)
{
	for(int i = 1; i != 50; ++i) {
		std::set<gamemap::location> touch;
		const gamemap::location res = find_vacant(map,units,loc,i,vacancy,touch);
		if(map.on_board(res))
			return res;
	}

	return gamemap::location();
}

namespace {

void get_tiles_radius_internal(const gamemap::location& a, size_t radius, std::set<gamemap::location>& res, std::map<gamemap::location,int>& visited)
{
	visited[a] = radius;
	res.insert(a);

	if(radius == 0) {
		return;
	}

	gamemap::location adj[6];
	get_adjacent_tiles(a,adj);
	for(size_t i = 0; i != 6; ++i) {
		if(visited.count(adj[i]) == 0 || visited[adj[i]] < radius-1) {
			get_tiles_radius_internal(adj[i],radius-1,res,visited);
		}
	}
}

}

void get_tiles_radius(const gamemap::location& a, size_t radius, std::set<gamemap::location>& res)
{
	std::map<gamemap::location,int> visited;
	get_tiles_radius_internal(a,radius,res,visited);
}

void get_tiles_radius(const gamemap& map, const std::vector<gamemap::location>& locs, size_t radius,
	std::set<gamemap::location>& res)
{
	typedef std::set<gamemap::location> location_set;
	location_set not_visited(locs.begin(), locs.end()), must_visit;
	++radius;

	for(;;) {
		location_set::const_iterator it = not_visited.begin(), it_end = not_visited.end();
		std::copy(it,it_end,std::inserter(res,res.end()));
		for(; it != it_end; ++it) {
			gamemap::location adj[6];
			get_adjacent_tiles(*it, adj);
			for(size_t i = 0; i != 6; ++i) {
				gamemap::location const &loc = adj[i];
				if(map.on_board(loc) && res.count(loc) == 0) {
					must_visit.insert(loc);
				}
			}
		}

		if(--radius == 0 || must_visit.empty()) {
			break;
		}

		not_visited.swap(must_visit);
		must_visit.clear();
	}
}

bool enemy_zoc(const gamemap& map, const gamestatus& status, 
					const std::map<gamemap::location,unit>& units,
					const std::vector<team>& teams,
               const gamemap::location& loc, const team& current_team, int side)
{
	gamemap::location locs[6];
	get_adjacent_tiles(loc,locs);
	for(int i = 0; i != 6; ++i) {
		const std::map<gamemap::location,unit>::const_iterator it
				= find_visible_unit(units,locs[i],
						map,
						status.get_time_of_day().lawful_bonus,
						teams,current_team);
		if(it != units.end() && it->second.side() != side &&
		   current_team.is_enemy(it->second.side()) && it->second.emits_zoc()) {
			return true;
		}
	}

	return false;
}

namespace {

void find_routes(const gamemap& map, const gamestatus& status,
		       const game_data& gamedata,
				 const std::map<gamemap::location,unit>& units,
				 const unit& u,
				 const gamemap::location& loc,
				 int move_left,
				 std::map<gamemap::location,paths::route>& routes,
				 std::vector<team>& teams,
				 bool ignore_zocs, bool allow_teleport, int turns_left, bool starting_pos)
{
	if(size_t(u.side()-1) >= teams.size()) {
		return;
	}

	team& current_team = teams[u.side()-1];

	//find adjacent tiles
	std::vector<gamemap::location> locs(6);
	get_adjacent_tiles(loc,&locs[0]);

	//check for teleporting units -- we must be on a vacant (or occupied by this unit)
	//village, that is controlled by our team to be able to teleport.
	if(allow_teleport && map.is_village(loc) &&
	   current_team.owns_village(loc) && (starting_pos || units.count(loc) == 0)) {
		const std::vector<gamemap::location>& villages = map.villages();

		//if we are on a village, see all friendly villages that we can
		//teleport to
		for(std::vector<gamemap::location>::const_iterator t = villages.begin();
		    t != villages.end(); ++t) {
			if(!current_team.owns_village(*t) || units.count(*t))
				continue;

			locs.push_back(*t);
		}
	}

	//iterate over all adjacent tiles
	for(size_t i = 0; i != locs.size(); ++i) {
		const gamemap::location& currentloc = locs[i];

		//check if the adjacent location is off the board
		if(currentloc.x < 0 || currentloc.y < 0 ||
		   currentloc.x >= map.x() || currentloc.y >= map.y())
			continue;

		//see if the tile is on top of an enemy unit
		const std::map<gamemap::location,unit>::const_iterator unit_it =
				find_visible_unit(units,locs[i],map,
						status.get_time_of_day().lawful_bonus,
						teams,current_team);

		if(unit_it != units.end() &&
		   current_team.is_enemy(unit_it->second.side()))
			continue;

		//find the terrain of the adjacent location
		const gamemap::TERRAIN terrain = map[currentloc.x][currentloc.y];

		//find the movement cost of this type onto the terrain
		const int move_cost = u.movement_cost(map,terrain);
		if(move_cost <= move_left ||
		   turns_left > 0 && move_cost <= u.total_movement()) {
			int new_move_left = move_left - move_cost;
			int new_turns_left = turns_left;
			if(new_move_left < 0) {
				--new_turns_left;
				new_move_left = u.total_movement() - move_cost;
			}

			const int total_movement = new_turns_left*u.total_movement() +
			                           new_move_left;

			const std::map<gamemap::location,paths::route>::const_iterator
					rtit = routes.find(currentloc);

			//if a better route to that tile has already been found
			if(rtit != routes.end() && rtit->second.move_left >= total_movement)
				continue;

			const bool zoc = !ignore_zocs && enemy_zoc(map,status,units,teams,currentloc,
			                                           current_team,u.side());
			paths::route new_route = routes[loc];
			new_route.steps.push_back(loc);

			const int zoc_move_left = zoc ? 0 : new_move_left;
			new_route.move_left = u.total_movement() * new_turns_left +
			                      zoc_move_left;
			routes[currentloc] = new_route;

			if(new_route.move_left > 0) {
				find_routes(map,status,gamedata,units,u,currentloc,
				            zoc_move_left,routes,teams,ignore_zocs,
							allow_teleport,new_turns_left,false);
			}
		}
	}
}

} //end anon namespace

paths::paths(const gamemap& map, const gamestatus& status,
		       const game_data& gamedata,
             const std::map<gamemap::location,unit>& units,
             const gamemap::location& loc,
			 std::vector<team>& teams,
			 bool ignore_zocs, bool allow_teleport, int additional_turns)
{
	const std::map<gamemap::location,unit>::const_iterator i = units.find(loc);
	if(i == units.end()) {
		std::cerr << "unit not found\n";
		return;
	}

	routes[loc].move_left = i->second.movement_left();
	find_routes(map,status,gamedata,units,i->second,loc,
	            i->second.movement_left(),routes,teams,
				ignore_zocs,allow_teleport,additional_turns,true);
}

int route_turns_to_complete(const unit& u, const gamemap& map,
                            const paths::route& rt)
{
	if(rt.steps.empty())
		return 0;

	int turns = 0, movement = u.movement_left();
	for(std::vector<gamemap::location>::const_iterator i = rt.steps.begin()+1;
	    i != rt.steps.end(); ++i) {
		wassert(map.on_board(*i));
		const int move_cost = u.movement_cost(map,map[i->x][i->y]);
		movement -= move_cost;
		if(movement < 0) {
			++turns;
			movement = u.total_movement() - move_cost;
			if(movement < 0) {
				return -1;
			}
		}
	}

	return turns;
}


shortest_path_calculator::shortest_path_calculator(const unit& u, const team& t,
                                                   const unit_map& units,
																	const std::vector<team>& teams,
                                                   const gamemap& map,
																	const gamestatus& status)
      : unit_(u), team_(t), units_(units), teams_(teams), 
	map_(map), status_(status)
{
}

double shortest_path_calculator::cost(const gamemap::location& loc, double so_far) const
{
	if(!map_.on_board(loc) || team_.shrouded(loc.x,loc.y))
		return 100000.0;

	const unit_map::const_iterator enemy_unit = find_visible_unit(units_,
			loc,map_,
			status_.get_time_of_day().lawful_bonus,teams_,team_);

	if(enemy_unit != units_.end() && team_.is_enemy(enemy_unit->second.side()))
		return 100000.0;

	if(unit_.type().is_skirmisher() == false) {
		gamemap::location adj[6];
		get_adjacent_tiles(loc,adj);

		for(size_t i = 0; i != 6; ++i) {
			const unit_map::const_iterator u = find_visible_unit(units_,
					adj[i],map_,
					status_.get_time_of_day().lawful_bonus,teams_,team_);

			if(u != units_.end() && team_.is_enemy(u->second.side()) && !team_.fogged(adj[i].x,adj[i].y) && u->second.emits_zoc()) {
				return 100000.0;
			}
		}
	}

	int const base_cost = unit_.movement_cost(map_, map_[loc.x][loc.y]);

	//supposing we had 2 movement left, and wanted to move onto a hex which
	//takes 3 movement, it's going to cost us 5 movement in total, since we
	//sacrifice this turn's movement. Take that into account here.
	const int current_cost(static_cast<int>(so_far));

	const int starting_movement = unit_.movement_left();
	int remaining_movement = starting_movement - current_cost;
	if (remaining_movement < 0) {
		int total = unit_.total_movement();
		remaining_movement = total - (-remaining_movement) % total;
	}

	int additional_cost = base_cost > remaining_movement ? remaining_movement : 0;

	return base_cost + additional_cost;
}
