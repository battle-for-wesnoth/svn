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
#ifndef AI_HPP_INCLUDED
#define AI_HPP_INCLUDED

#include "actions.hpp"
#include "ai_move.hpp"
#include "display.hpp"
#include "map.hpp"
#include "unit.hpp"
#include "unit_types.hpp"

#include <map>

class ai_interface {
public:

	///a convenient typedef for the often used 'location' object
	typedef gamemap::location location;

	///the standard way in which a map of possible moves is recorded
	typedef std::multimap<location,location> move_map;

	///info: a structure which holds references to all the important objects
	///that an AI might need access to in order to make and implement its decisions
	struct info {
		info(display& disp, const gamemap& map, const game_data& gameinfo, unit_map& units,
			std::vector<team>& teams, int team_num, const gamestatus& state, class turn_info& turn_data)
			: disp(disp), map(map), gameinfo(gameinfo), units(units), teams(teams),
			  team_num(team_num), state(state), turn_data_(turn_data)
		{}

		///the display object, used to draw the moves the AI makes.
		display& disp;

		///the map of the game -- use this object to find the terrain at any location
		const gamemap& map;

		///this object contains information about the types of units and races in the game
		const game_data& gameinfo;

		///the map of units - maps locations -> units
		unit_map& units;

		///a list of the teams in the game
		std::vector<team>& teams;

		///the number of the team the AI is. Note that this number is 1-based, so it
		///has to have 1 subtracted from it for it to be used as an index of 'teams'
		int team_num;

		///information about what turn it is, and what time of day
		const gamestatus& state;

		///the object that allows the player to interact with the game.
		///should not be used outside of ai_interface
		class turn_info& turn_data_;
	};

	///the constructor. All derived classes should take an argument of type info& which
	///they should pass to this constructor
	ai_interface(info& arg) : info_(arg), last_interact_(0) {}
	virtual ~ai_interface() {}

	///the function that is called when the AI must play its turn. Derived classes should
	///implement their AI algorithm in this function
	virtual void play_turn() = 0;

	///functions which return a reference to the 'team' object for the AI
	team& current_team();
	const team& current_team() const;

protected:
	///this function should be called to attack an enemy.
	///'attacking_unit': the location of the attacking unit
	///'target': the location of the target unit. This unit must be in range of the
	///attacking unit's weapon
	///'weapon': the number of the weapon (0-based) which should be used in the attack.
	///must be a valid weapon of the attacking unit
	void attack_enemy(const location& attacking_unit, const location& target, int weapon);

	///this function should be called to move a unit. Once the unit has been moved, its
	///movement allowance is set to 0.
	///'from': the location of the unit being moved.
	///'to': the location to be moved to. This must be a valid move for the unit
	///'possible_moves': the map of possible moves, as obtained from 'calculate_possible_moves'
	location move_unit(location from, location to, std::map<location,paths>& possible_moves);

	///this function is used to calculate the moves units may possibly make.
	///'possible_moves': a map which will be filled with the paths each unit can take to
	///get to every possible destination. You probably don't want to use this object at all,
	///except to pass to 'move_unit'.
	///'srcdst': a map of units to all their possible destinations
	///'dstsrc': a map of destinations to all the units that can move to that destination
	///'enemy': if true, a map of possible moves for enemies will be calculated. If false,
	///a map of possible moves for units on the AI's side will be calculated. The AI's own
	///leader will not be included in this map.
	///'assume_full_movement': if true, the function will operate on the assumption that all
	///units can move their full movement allotment.
	void calculate_possible_moves(std::map<location,paths>& possible_moves, move_map& srcdst, move_map& dstsrc, bool enemy, bool assume_full_movement=false);

	///this function is used to recruit a unit. It will recruit the unit with the given name,
	///at the given location, or at an available location to recruit units if 'loc' is not
	///a valid recruiting location.
	///
	///if recruitment cannot be performed, because there are no available tiles, or not enough
	///money, then the function will return false
	bool recruit(const std::string& unit_name, location loc=location());

	///functions to retrieve the 'info' object. Used by derived classes to discover all
	///necessary game information
	info& get_info() { return info_; }
	const info& get_info() const { return info_; }

	///function which should be called frequently to allow the user to interact with
	///the interface. This function will make sure that interaction doesn't occur
	///too often, so there is no problem with calling it very regularly
	void user_interact();

private:
	info info_;
	int last_interact_;
};

///this function is used to create a new AI object with the specified algorithm name
ai_interface* create_ai(const std::string& algorithm_name, ai_interface::info& info);

class sample_ai : public ai_interface {
public:
	sample_ai(info& i) : ai_interface(i) {}

	void play_turn() {
		do_attacks();
		get_villages();
		do_moves();
		do_recruitment();
	}

private:
	void do_attacks() {
		std::map<location,paths> possible_moves;
		move_map srcdst, dstsrc;
		calculate_possible_moves(possible_moves,srcdst,dstsrc,false);

		for(unit_map::const_iterator i = get_info().units.begin(); i != get_info().units.end(); ++i) {
			if(current_team().is_enemy(i->second.side())) {
				location adjacent_tiles[6];
				get_adjacent_tiles(i->first,adjacent_tiles);

				int best_defense = -1;
				std::pair<location,location> best_movement;

				for(size_t n = 0; n != 6; ++n) {
					typedef move_map::const_iterator Itor;
					std::pair<Itor,Itor> range = dstsrc.equal_range(adjacent_tiles[n]);
					while(range.first != range.second) {
						const location& dst = range.first->first;
						const location& src = range.first->second;
						const unit_map::const_iterator un = get_info().units.find(src);

						const gamemap::TERRAIN terrain = get_info().map.get_terrain(dst);

						const int chance_to_hit = un->second.defense_modifier(get_info().map,terrain);

						if(best_defense == -1 || chance_to_hit < best_defense) {
							best_defense = chance_to_hit;
							best_movement = *range.first;
                        }

						++range.first;
					}
				}

				if(best_defense != -1) {
					move_unit(best_movement.second,best_movement.first,possible_moves);
					const int weapon = choose_weapon(best_movement.first,i->first);
					attack_enemy(best_movement.first,i->first,weapon);
					do_attacks();
					return;
				}
			}
		}
	}

	int choose_weapon(const location& attacker, const location& defender) {
		const unit_map::const_iterator att = get_info().units.find(attacker);
        assert(att != get_info().units.end());
        
        const std::vector<attack_type>& attacks = att->second.attacks();

        int best_attack_rating = -1;
        int best_attack = -1;
        for(int n = 0; n != attacks.size(); ++n) {
			const battle_stats stats = evaluate_battle_stats(get_info().map,attacker,defender,n,get_info().units,get_info().state,get_info().gameinfo,0,false);
			const int attack_rating = stats.damage_defender_takes*stats.nattacks*stats.chance_to_hit_defender;
			if(best_attack == -1 || attack_rating > best_attack_rating) {
                 best_attack = n;
                 best_attack_rating = attack_rating;
            }
		}

		return best_attack;
	}

	void get_villages() {
        std::map<location,paths> possible_moves;
        move_map srcdst, dstsrc;
        calculate_possible_moves(possible_moves,srcdst,dstsrc,false);

        for(move_map::const_iterator i = dstsrc.begin(); i != dstsrc.end(); ++i) {
            if(get_info().map.underlying_terrain(get_info().map.get_terrain(i->first)) == gamemap::TOWER &&
               current_team().owns_tower(i->first) == false) {
                move_unit(i->second,i->first,possible_moves);
                get_villages();
                return;
            }
        }
	}

	void do_moves() {
		unit_map::const_iterator leader;
        for(leader = get_info().units.begin(); leader != get_info().units.end(); ++leader) {
            if(leader->second.can_recruit() && current_team().is_enemy(leader->second.side())) {
                break;
            }
        }

        if(leader == get_info().units.end())
            return;

        std::map<location,paths> possible_moves;
        move_map srcdst, dstsrc;
        calculate_possible_moves(possible_moves,srcdst,dstsrc,false);

        int closest_distance = -1;
        std::pair<location,location> closest_move;

        for(move_map::const_iterator i = dstsrc.begin(); i != dstsrc.end(); ++i) {
            const int distance = distance_between(i->first,leader->first);
            if(closest_distance == -1 || distance < closest_distance) {
                closest_distance = distance;
                closest_move = *i;
            }
        }

        if(closest_distance != -1) {
            move_unit(closest_move.second,closest_move.first,possible_moves);
            do_moves();
        }
    }

	void do_recruitment() {
		const std::set<std::string>& options = current_team().recruits();
        const int choice = (rand()%options.size());
        std::set<std::string>::const_iterator i = options.begin();
        std::advance(i,choice);

		const bool res = recruit(*i);
		if(res) {
			do_recruitment();
		}
	}
};

class ai : public ai_interface {
public:

	ai(ai_interface::info& info);

	void play_turn();

private:

	void do_move();

	bool do_combat(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc);
	bool get_villages(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader);
	bool get_healing(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc);
	bool retreat_units(std::map<gamemap::location,paths>& possible_moves, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader);
	bool move_to_targets(std::map<gamemap::location,paths>& possible_moves, move_map& srcdst, move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc, unit_map::const_iterator leader);

	bool should_retreat(const gamemap::location& loc, const move_map& srcdst, const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc) const;

	void do_recruitment();

	void move_leader_to_keep(const move_map& enemy_dstsrc);
	void move_leader_after_recruit(const move_map& enemy_dstsrc);
	void leader_attack();

	bool recruit_usage(const std::string& usage);

	struct attack_analysis
	{
		void analyze(const gamemap& map, std::map<location,unit>& units,
		             const gamestatus& status, const game_data& info, int sims,
					 class ai& ai_obj);

		double rating(double aggression) const;

		gamemap::location target;
		std::vector<std::pair<gamemap::location,gamemap::location> > movements;
		std::vector<int> weapons;

		//the value of the unit being targeted
		double target_value;

		//the value on average, of units lost in the combat
		double avg_losses;

		//estimated % chance to kill the unit
		double chance_to_kill;

		//the average hitpoints damage inflicted
		double avg_damage_inflicted;

		int target_starting_damage;

		//the average hitpoints damage taken
		double avg_damage_taken;

		//the sum of the values of units used in the attack
		double resources_used;

		//the weighted average of the % chance to hit each attacking unit
		double terrain_quality;

		//the ratio of the attacks the unit being attacked will get to
		//the strength of its most powerful attack
		double counter_strength_ratio;

		//the vulnerability is the power projection of enemy units onto the hex
		//we're standing on. support is the power projection of friendly units.
		double vulnerability, support;

		//is true if the unit is a threat to our leader
		bool leader_threat;
	};

	void do_attack_analysis(
	                 const location& loc,
	                 const move_map& srcdst, const move_map& dstsrc,
	                 const move_map& enemy_srcdst, const move_map& enemy_dstsrc,
					 const location* tiles, bool* used_locations,
	                 std::vector<location>& units,
	                 std::vector<attack_analysis>& result,
					 attack_analysis& cur_analysis
	                );


	double power_projection(const gamemap::location& loc, const move_map& srcdst, const move_map& dstsrc, bool use_terrain=true) const;

public:
	int choose_weapon(const location& att, const location& def,
					  battle_stats& cur_stats, gamemap::TERRAIN terrain);

private:
	std::vector<attack_analysis> analyze_targets(
	             const move_map& srcdst, const move_map& dstsrc,
	             const move_map& enemy_srcdst, const move_map& enemy_dstsrc
            );


	struct target {
		target(const location& pos, double val) : loc(pos), value(val)
		{}
		location loc;
		double value;
	};

	std::vector<target> find_targets(unit_map::const_iterator leader, const move_map& enemy_srcdst, const move_map& enemy_dstsrc);

	std::pair<location,location> choose_move(std::vector<target>& targets,const move_map& dstsrc, const move_map& enemy_srcdst, const move_map& enemy_dstsrc);

	display& disp_;
	const gamemap& map_;
	const game_data& gameinfo_;
	unit_map& units_;
	std::vector<team>& teams_;
	int team_num_;
	const gamestatus& state_;
	bool consider_combat_;
	std::vector<target> additional_targets_;
};

#endif
