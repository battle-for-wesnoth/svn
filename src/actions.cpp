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
#include "display.hpp"
#include "events.hpp"
#include "game.hpp"
#include "game_config.hpp"
#include "game_events.hpp"
#include "key.hpp"
#include "language.hpp"
#include "log.hpp"
#include "map.hpp"
#include "pathfind.hpp"
#include "playlevel.hpp"
#include "playturn.hpp"
#include "replay.hpp"
#include "sound.hpp"
#include "statistics.hpp"
#include "unit_display.hpp"
#include "util.hpp"

#include <cmath>
#include <set>
#include <string>
#include <sstream>

struct castle_cost_calculator
{
	castle_cost_calculator(const gamemap& map) : map_(map)
	{}

	double cost(const gamemap::location& loc, double cost_so_far) const
	{
		if(!map_.is_castle(loc))
			return 10000;

		return 1;
	}

private:
	const gamemap& map_;
};

// Conditions placed on victory must be accessible from the global function
// check_victory, but shouldn't be passed to that function as parameters,
// since it is called from a variety of places.
namespace victory_conditions
{ 
	bool when_enemies_defeated = true;

	void set_victory_when_enemies_defeated(bool on) 
	{
		when_enemies_defeated = on;
	}

	bool victory_when_enemies_defeated() 
	{
		return when_enemies_defeated;
	}
}

std::string recruit_unit(const gamemap& map, int side,
       std::map<gamemap::location,unit>& units, unit& new_unit,
       gamemap::location& recruit_location, display* disp, bool need_castle, bool full_movement)
{
	std::cerr << "recruiting unit for side " << side << "\n";
	typedef std::map<gamemap::location,unit> units_map;

	//find the unit that can recruit
	units_map::const_iterator u;

	for(u = units.begin(); u != units.end(); ++u) {
		if(u->second.can_recruit() && u->second.side() == side) {
			break;
		}
	}

	if(u == units.end())
		return string_table["no_leader_to_recruit"];

	if(map.is_keep(u->first) == false) {
		std::cerr << "Leader not on start: leader is on " << (u->first.x+1) << "," << (u->first.y+1) << "\n";
		return string_table["leader_not_on_start"];
	}

	if(map.on_board(recruit_location)) {
		const paths::route& rt = a_star_search(u->first,recruit_location,
		                                   100.0,castle_cost_calculator(map));
		if(rt.steps.empty() || units.find(recruit_location) != units.end() ||
		   !map.is_castle(recruit_location))
			recruit_location = gamemap::location();
	}

	if(!map.on_board(recruit_location)) {
		recruit_location = find_vacant_tile(map,units,u->first,
		                                    need_castle ? VACANT_CASTLE : VACANT_ANY);
	}

	if(!map.on_board(recruit_location)) {
		return string_table["no_recruit_location"];
	}

	if(full_movement) {
		new_unit.set_movement(new_unit.total_movement());
	} else {
		new_unit.set_movement(0);
		new_unit.set_attacked();
	}

	units.insert(std::pair<gamemap::location,unit>(
							recruit_location,new_unit));

	if(disp != NULL && !disp->turbo() &&
	   !disp->fogged(recruit_location.x,recruit_location.y)) {
		disp->draw(true,true);

		for(double alpha = 0.0; alpha <= 1.0; alpha += 0.1) {
			events::pump();
			disp->draw_tile(recruit_location.x,recruit_location.y,NULL,alpha);
			disp->update_display();
			SDL_Delay(20);
		}
	}

	return std::string();
}

gamemap::location under_leadership(const std::map<gamemap::location,unit>& units,
                                   const gamemap::location& loc)
{
	gamemap::location adjacent[6];
	get_adjacent_tiles(loc,adjacent);
	const unit_map::const_iterator un = units.find(loc);
	if(un == units.end()) {
		return gamemap::location::null_location;
	}

	const int side = un->second.side();
	const int level = un->second.type().level();

	for(int i = 0; i != 6; ++i) {
		const unit_map::const_iterator it = units.find(adjacent[i]);
		if(it != units.end() && it->second.side() == side &&
			it->second.type().is_leader() && it->second.type().level() > level &&
			it->second.incapacitated() == false) {
			return adjacent[i];
		}
	}

	return gamemap::location::null_location;
}

battle_stats evaluate_battle_stats(
                   const gamemap& map,
                   const gamemap::location& attacker,
                   const gamemap::location& defender,
				   int attack_with,
				   std::map<gamemap::location,unit>& units,
				   const gamestatus& state,
				   const game_data& info,
				   gamemap::TERRAIN attacker_terrain_override,
				   bool include_strings)
{
	//if these are both genuine positions, work out the range
	//combat is taking place at
	const int combat_range = attacker_terrain_override == 0 ? distance_between(attacker,defender) : 1;

	battle_stats res;

	res.attack_with = attack_with;

	if(include_strings)
		res.defend_name = string_table["weapon_none"];

	const std::map<gamemap::location,unit>::iterator a = units.find(attacker);
	const std::map<gamemap::location,unit>::iterator d = units.find(defender);

	assert(a != units.end());
	assert(d != units.end());

	const gamemap::TERRAIN attacker_terrain = attacker_terrain_override ?
	                 attacker_terrain_override : map[attacker.x][attacker.y];
	const gamemap::TERRAIN defender_terrain = map[defender.x][defender.y];

	res.chance_to_hit_attacker = a->second.defense_modifier(map,attacker_terrain);
	res.chance_to_hit_defender = d->second.defense_modifier(map,defender_terrain);

	const std::vector<attack_type>& attacker_attacks = a->second.attacks();
	const std::vector<attack_type>& defender_attacks = d->second.attacks();

	assert(attack_with >= 0 && attack_with < int(attacker_attacks.size()));
	const attack_type& attack = attacker_attacks[attack_with];

	static const std::string charge_string("charge");
	const bool charge = (attack.special() == charge_string);

	bool backstab = false;

	static const std::string to_the_death_string("berserk");
	res.to_the_death = attack.special() == to_the_death_string;

	static const std::string backstab_string("backstab");
	if(attack.special() == backstab_string) {
		gamemap::location adj[6];
		get_adjacent_tiles(defender,adj);
		int i;
		for(i = 0; i != 6; ++i) {
			if(adj[i] == attacker)
				break;
		}

		if(i != 6) {
			const std::map<gamemap::location,unit>::const_iterator u =
			                    units.find(adj[(i+3)%6]);
			if(u != units.end() && u->second.side() == a->second.side()) {
				backstab = true;
			}
		}
	}

	static const std::string plague_string("plague");
	res.attacker_plague = !d->second.type().not_living() && (attack.special() == plague_string);
	res.defender_plague = false;

	res.attack_name    = attack.name();
	res.attack_type    = attack.type();

	if(include_strings) {
		res.attack_special = attack.special();
		res.attack_icon = attack.icon();

		//don't show backstabbing unless it's actually happening
		if(res.attack_special == "backstab" && !backstab)
			res.attack_special = "";

		res.range = (attack.range() == attack_type::SHORT_RANGE ?
		             "Melee" : "Ranged");
	}

	res.nattacks = attack.num_attacks();
	double best_defend_rating = 0.0;
	int defend = -1;
	res.ndefends = 0;
	for(int defend_option = 0; defend_option != int(defender_attacks.size()); ++defend_option) {
		if(defender_attacks[defend_option].range() == attack.range() &&
			defender_attacks[defend_option].hexes() >= combat_range) {
			const double rating = a->second.damage_against(defender_attacks[defend_option])
			                      *defender_attacks[defend_option].num_attacks()
				                  *defender_attacks[defend_option].defense_weight();
			if(defend == -1 || rating > best_defend_rating) {
				best_defend_rating = rating;
				defend = defend_option;
			}
		}
	}

	res.defend_with = defend;

	const bool counterattack = defend != -1;

	static const std::string drain_string("drain");
	static const std::string magical_string("magical");

	res.damage_attacker_takes = 0;
	if(counterattack) {
		if(defender_attacks[defend].special() == to_the_death_string) {
			res.to_the_death = true;
		}

		//magical attacks always have a 70% chance to hit
		if(defender_attacks[defend].special() == magical_string) {
			res.chance_to_hit_attacker = 70;
		}

		int percent = 0;

		const int base_damage = defender_attacks[defend].damage();
		const int resistance_modifier = a->second.damage_against(defender_attacks[defend]);
		
		//res.damage_attacker_takes = (base_damage * (100+modifier))/100;

		if(include_strings) {
			std::stringstream str_base;
			str_base << string_table["base_damage"] << "," << base_damage << ",";
			res.defend_calculations.push_back(str_base.str());
		}

		const int resist = resistance_modifier - 100;
		percent += resist;

		if(include_strings && resist != 0) {
			std::stringstream str_resist;

			str_resist << string_table[resist < 0 ? "attacker_resistance" : "attacker_vulnerability"] << " " << translate_string(defender_attacks[defend].type())
				<< ", ,^" << (resist > 0 ? "+" : "") << resist << "%";
			res.defend_calculations.push_back(str_resist.str());
		}

		const int tod_modifier = combat_modifier(state,units,d->first,d->second.type().alignment());
		percent += tod_modifier;

		if(include_strings && tod_modifier != 0) {
			std::stringstream str_mod;
			const time_of_day& tod = timeofday_at(state,units,d->first);
			str_mod << translate_string_default(tod.id,tod.name) << ", ,^"
			        << (tod_modifier > 0 ? "+" : "") << tod_modifier << "%";
			res.defend_calculations.push_back(str_mod.str());
		}

		if(charge) {
			percent += 100;

			if(include_strings) {
				std::stringstream str;
				str << translate_string("charge") << ", ,^+100%";
				res.defend_calculations.push_back(str.str());
			}
		}

		if(under_leadership(units,defender).valid()) {
			percent += 25;

			if(include_strings) {
				std::stringstream str;
				str << translate_string("leadership") << ", ,^+25%";
				res.defend_calculations.push_back(str.str());
			}
		}

		//we want to round the absolute value of the difference down
		//at 0.5 and below
		const int is_negative = percent < 0 ? -1 : 1;

		if(percent < 0) {
			percent *= -1;
		}

		int difference = percent*base_damage;

		//round up if greater than half
		if((difference%100) > 50) {
			difference += 50;
		}

		difference /= 100*is_negative;

		res.damage_attacker_takes = maximum<int>(1,base_damage + difference);

		if(include_strings) {
			percent *= is_negative;

			std::stringstream str;
			str << translate_string("total_damage") << "," << res.damage_attacker_takes
				<< ",^" << (percent >= 0 ? "+" : "") << percent << "% (" << (difference >= 0 ? "+" : "") << difference << ")";
			res.defend_calculations.push_back(str.str());
		}

		res.ndefends = defender_attacks[defend].num_attacks();

		res.defend_name    = defender_attacks[defend].name();
		res.defend_type    = defender_attacks[defend].type();

		if(include_strings) {
			res.defend_special = defender_attacks[defend].special();
			res.defend_icon = defender_attacks[defend].icon();
		}

		//if the defender drains, and the attacker is a living creature, then
		//the defender will drain for half the damage it does
		if(defender_attacks[defend].special() == drain_string && !a->second.type().not_living()) {
			res.amount_defender_drains = res.damage_attacker_takes/2;
		} else {
			res.amount_defender_drains = 0;
		}

		res.defender_plague = (defender_attacks[defend].special() == plague_string);
	}

	if(attack.special() == magical_string)
		res.chance_to_hit_defender = 70;

	static const std::string marksman_string("marksman");

	//offensive marksman attacks always have at least 60% chance to hit
	if(res.chance_to_hit_defender < 60 && attack.special() == marksman_string)
		res.chance_to_hit_defender = 60;

	int percent = 0;

	const int base_damage = attack.damage();
	const int resistance_modifier = d->second.damage_against(attack);

	if(include_strings) {
		std::stringstream str_base;

		str_base << string_table["base_damage"] << "," << base_damage << ",";
		res.attack_calculations.push_back(str_base.str());
	}

	const int resist = resistance_modifier - 100;
	percent += resist;

	if(include_strings && resist != 0) {
		std::stringstream str_resist;

		str_resist << string_table[resist < 0 ? "defender_resistance" : "defender_vulnerability"] << " " << translate_string(attack.type())
			       << ", ,^" << (resist > 0 ? "+" : "") << resist << "%";
		res.attack_calculations.push_back(str_resist.str());
	}

	const int tod_modifier = combat_modifier(state,units,a->first,a->second.type().alignment());

	percent += tod_modifier;

	if(include_strings && tod_modifier != 0) {
		std::stringstream str_mod;
		const time_of_day& tod = timeofday_at(state,units,a->first);
		str_mod << translate_string_default(tod.id,tod.name) << ", ,^"
		        << (tod_modifier > 0 ? "+" : "") << tod_modifier << "%";
		res.attack_calculations.push_back(str_mod.str());
	}

	if(charge) {
		percent += 100;

		if(include_strings) {
			std::stringstream str;
			str << translate_string("charge") << ", ,^+100%";
			res.attack_calculations.push_back(str.str());
		}
	}

	if(backstab) {
		percent += 100;
		if(include_strings) {
			std::stringstream str;
			str << translate_string("backstab") << ", ,^+100%";
			res.attack_calculations.push_back(str.str());
		}
	}

	if(under_leadership(units,attacker).valid()) {
		percent += 25;
		
		if(include_strings) {
			std::stringstream str;
			str << translate_string("leadership") << ", ,^+25%";
			res.attack_calculations.push_back(str.str());
		}
	}

	//we want to round the absolute value of the difference down
	//at 0.5 and below
	const int is_negative = percent < 0 ? -1 : 1;

	if(percent < 0) {
		percent *= -1;
	}

	int difference = percent*base_damage;

	//round up if greater than half
	if((difference%100) > 50) {
		difference += 50;
	}

	difference /= 100*is_negative;

	res.damage_defender_takes = maximum<int>(1,base_damage + difference);
	if(include_strings) {
		percent *= is_negative;

		std::stringstream str;
		str << translate_string("total_damage") << "," << res.damage_defender_takes
			<< ",^" << (percent >= 0 ? "+" : "") << percent << "% (" << (difference >= 0 ? "+" : "") << difference << ")";
		res.attack_calculations.push_back(str.str());
	}

	//if the attacker drains, and the defender is a living creature, then
	//the attacker will drain for half the damage it does
	if(attack.special() == drain_string && !d->second.type().not_living()) {
		res.amount_attacker_drains = res.damage_defender_takes/2;
	} else {
		res.amount_attacker_drains = 0;
	}

	static const std::string slowed_string("slowed");
	if(a->second.has_flag(slowed_string) && res.nattacks > 1)
		--res.nattacks;

	if(d->second.has_flag(slowed_string) && res.ndefends > 1)
		--res.ndefends;

	return res;
}

void attack(display& gui, const gamemap& map, 
				std::vector<team>& teams,
            const gamemap::location& attacker,
            const gamemap::location& defender,
			int attack_with,
			std::map<gamemap::location,unit>& units,
			const gamestatus& state,
			const game_data& info, bool player_is_attacker)
{
	//stop the user from issuing any commands while the units are fighting
	const command_disabler disable_commands(&gui);

	std::map<gamemap::location,unit>::iterator a = units.find(attacker);
	std::map<gamemap::location,unit>::iterator d = units.find(defender);

	assert(a != units.end());
	assert(d != units.end());

	int attackerxp = d->second.type().level();
	int defenderxp = a->second.type().level();

	a->second.set_attacked();
	d->second.set_resting(false);

	//if the attacker was invisible, she isn't anymore!
	static const std::string forest_invisible("ambush");
	a->second.remove_flag(forest_invisible);
	static const std::string night_invisible("nightstalk");
	a->second.remove_flag(night_invisible);

	battle_stats stats = evaluate_battle_stats(map,attacker,defender,
	                                           attack_with,units,state,info);

	statistics::attack_context attack_stats(a->second,d->second,stats);

	const int orig_attacks = stats.nattacks;
	const int orig_defends = stats.ndefends;
	int to_the_death = stats.to_the_death ? 30 : 0;

	static const std::string poison_string("poison");

	while(stats.nattacks > 0 || stats.ndefends > 0) {
		std::cerr << "start of attack loop...\n";

		if(stats.nattacks > 0) {
			const int ran_num = get_random();
			bool hits = (ran_num%100) < stats.chance_to_hit_defender;

			//make sure that if we're serializing a game here,
			//we got the same results as the game did originally
			const config* ran_results = get_random_results();
			if(ran_results != NULL) {
				const int results_chance = atoi((*ran_results)["chance"].c_str());
				const bool results_hits = (*ran_results)["hits"] == "yes";
				const int results_damage = atoi((*ran_results)["damage"].c_str());

				if(results_chance != stats.chance_to_hit_defender) {
					std::cerr << "SYNC ERROR: In attack " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": chance to hit defender is inconsistent. Data source: "
							  << results_chance << "; Calculation: " << stats.chance_to_hit_defender
							  << " (over-riding game calculations with data source results)\n";
					hits = results_hits;
				} else if(hits != results_hits) {
					std::cerr << "SYNC ERROR: In attack " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source says the hit was "
							  << (results_hits ? "successful" : "unsuccessful") << ", while in-game calculations say the hit was "
							  << (hits ? "successful" : "unsuccessful")
							  << " random number: " << ran_num << " = " << (ran_num%100) << "/" << results_chance
							  << " (over-riding game calculations with data source results)\n";
					hits = results_hits;
				} else if(results_damage != stats.damage_defender_takes) {
					std::cerr << "SYNC ERROR: In attack " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source says the hit did "
							  << results_damage << " damage, while in-game calculations show the hit doing "
							  << stats.damage_defender_takes << " damage (over-riding game calculations with data source results)\n";
					stats.damage_defender_takes = results_damage;
				}
			}

			bool dies = unit_display::unit_attack(gui,units,map,attacker,defender,
				            hits ? stats.damage_defender_takes : 0,
							a->second.attacks()[attack_with]);

			std::cerr << "done attacking\n";

			attack_stats.attack_result(hits ? (dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			                           : statistics::attack_context::MISSES);

			if(ran_results == NULL) {
				log_scope("setting random results");
				config cfg;
				cfg["hits"] = (hits ? "yes" : "no");
				cfg["dies"] = (dies ? "yes" : "no");

				cfg["damage"] = lexical_cast<std::string>(stats.damage_defender_takes);
				cfg["chance"] = lexical_cast<std::string>(stats.chance_to_hit_defender);

				set_random_results(cfg);
			} else {
				const bool results_dies = (*ran_results)["dies"] == "yes";
				if(results_dies != dies) {
					std::cerr << "SYNC ERROR: In attack" << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source the unit "
							  << (results_dies ? "perished" : "survived") << " while in-game calculations show the unit "
							  << (dies ? "perished" : "survived") << " (over-riding game calculations with data source results)\n";
					dies = results_dies;
				}
			}

			if(dies) {
				attackerxp = game_config::kill_experience*d->second.type().level();
				if(d->second.type().level() == 0)
					attackerxp = game_config::kill_experience/2;

				a->second.get_experience(attackerxp);
				attackerxp = 0;
				defenderxp = 0;

				gamemap::location loc = d->first;
				gamemap::location attacker_loc = a->first;
				const int defender_side = d->second.side();
				std::cerr << "firing die event\n";
				game_events::fire("die",loc,a->first);
				d = units.end();
				a = units.end();

				//the handling of the event may have removed the object
				//so we have to find it again
				units.erase(loc);

				//plague units make clones of themselves on the target hex
				//units on villages that die cannot be plagued
				if(stats.attacker_plague && !map.is_village(loc)) {
					a = units.find(attacker_loc);
					if(a != units.end()) {
						units.insert(std::pair<gamemap::location,unit>(loc,a->second));
						gui.draw_tile(loc.x,loc.y);
					}
				}
				recalculate_fog(map,state,info,units,teams,defender_side-1);
				gui.recalculate_minimap();
				gui.update_display();
				break;
			} else if(hits) {
				if(stats.attack_special == poison_string &&
				   d->second.has_flag("poisoned") == false &&
				   !d->second.type().not_living()) {
					gui.float_label(d->first,translate_string("poisoned"),255,0,0);
					d->second.set_flag("poisoned");
				}

				static const std::string slow_string("slow");
				if(stats.attack_special == slow_string &&
				   d->second.has_flag("slowed") == false) {
					gui.float_label(d->first,translate_string("slowed"),255,0,0);
					d->second.set_flag("slowed");
					if(stats.ndefends > 1)
						--stats.ndefends;
				}

				if(stats.amount_attacker_drains > 0) {
					char buf[50];
					sprintf(buf,"%d",stats.amount_attacker_drains);
					gui.float_label(a->first,buf,0,255,0);
					a->second.gets_hit(-stats.amount_attacker_drains);
				}

				//if the defender is turned to stone, the fight stops immediately
				static const std::string stone_string("stone");
				if(stats.attack_special == stone_string) {
					gui.float_label(d->first,translate_string("stone"),255,0,0);
					d->second.set_flag(stone_string);
					stats.ndefends = 0;
					stats.nattacks = 0;
					game_events::fire(stone_string,d->first,a->first);
				}
			}

			--stats.nattacks;
		}

		if(stats.ndefends > 0) {
			std::cerr << "doing defender attack...\n";

			const int ran_num = get_random();
			bool hits = (ran_num%100) < stats.chance_to_hit_attacker;

			//make sure that if we're serializing a game here,
			//we got the same results as the game did originally
			const config* ran_results = get_random_results();
			if(ran_results != NULL) {
				const int results_chance = atoi((*ran_results)["chance"].c_str());
				const bool results_hits = (*ran_results)["hits"] == "yes";
				const int results_damage = atoi((*ran_results)["damage"].c_str());

				if(results_chance != stats.chance_to_hit_attacker) {
					std::cerr << "SYNC ERROR: In defend " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": chance to hit attacker is inconsistent. Data source: "
							  << results_chance << "; Calculation: " << stats.chance_to_hit_attacker
							  << " (over-riding game calculations with data source results)\n";
					hits = results_hits;
				} else if(hits != results_hits) {
					std::cerr << "SYNC ERROR: In defend " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source says the hit was "
							  << (results_hits ? "successful" : "unsuccessful") << ", while in-game calculations say the hit was "
							  << (hits ? "successful" : "unsuccessful")
							  << " random number: " << ran_num << " = " << (ran_num%100) << "/" << results_chance
							  << " (over-riding game calculations with data source results)\n";
					hits = results_hits;
				} else if(results_damage != stats.damage_attacker_takes) {
					std::cerr << "SYNC ERROR: In defend " << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source says the hit did "
							  << results_damage << " damage, while in-game calculations show the hit doing "
							  << stats.damage_attacker_takes << " damage (over-riding game calculations with data source results)\n";
					stats.damage_attacker_takes = results_damage;
				}
			}

			bool dies = unit_display::unit_attack(gui,units,map,defender,attacker,
			               hits ? stats.damage_attacker_takes : 0,
						   d->second.attacks()[stats.defend_with]);

			attack_stats.defend_result(hits ? (dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			                           : statistics::attack_context::MISSES);

			if(ran_results == NULL) {
				config cfg;
				cfg["hits"] = (hits ? "yes" : "no");
				cfg["dies"] = (dies ? "yes" : "no");
				cfg["damage"] = lexical_cast<std::string>(stats.damage_attacker_takes);
				cfg["chance"] = lexical_cast<std::string>(stats.chance_to_hit_attacker);

				set_random_results(cfg);
			} else {
				const bool results_dies = (*ran_results)["dies"] == "yes";
				if(results_dies != dies) {
					std::cerr << "SYNC ERROR: In defend" << a->second.type().name() << " vs "
					          << d->second.type().name() << ": the data source the unit "
							  << (results_dies ? "perished" : "survived") << " while in-game calculations show the unit "
							  << (dies ? "perished" : "survived") << " (over-riding game calculations with data source results)\n";
					dies = results_dies;
				}
			}

			if(dies) {
				defenderxp = game_config::kill_experience*a->second.type().level();
				if(a->second.type().level() == 0)
					defenderxp = game_config::kill_experience/2;

				d->second.get_experience(defenderxp);
				defenderxp = 0;
				attackerxp = 0;

				gamemap::location loc = a->first;
				gamemap::location defender_loc = d->first;
				const int attacker_side = a->second.side();
				game_events::fire("die",loc,d->first);
				a = units.end();
				d = units.end();

				//the handling of the event may have removed the object
				//so we have to find it again
				units.erase(loc);

				//plague units make clones of themselves on the target hex.
				//units on villages that die cannot be plagued
				if(stats.defender_plague && !map.is_village(loc)) {
					d = units.find(defender_loc);
					if(d != units.end()) {
						units.insert(std::pair<gamemap::location,unit>(
						                                 loc,d->second));
						gui.draw_tile(loc.x,loc.y);
					}
				}
				gui.recalculate_minimap();
				gui.update_display();
				recalculate_fog(map,state,info,units,teams,attacker_side-1);
				break;
			} else if(hits) {
				if(stats.defend_special == poison_string &&
				   a->second.has_flag("poisoned") == false &&
				   !a->second.type().not_living()) {
					gui.float_label(a->first,translate_string("poisoned"),255,0,0);
					a->second.set_flag("poisoned");
				}

				static const std::string slow_string("slow");
				if(stats.defend_special == slow_string &&
				   a->second.has_flag("slowed") == false) {
					gui.float_label(a->first,translate_string("slowed"),255,0,0);
					a->second.set_flag("slowed");
					if(stats.nattacks > 1)
						--stats.nattacks;
				}

				if(stats.amount_defender_drains > 0) {
					char buf[50];
					sprintf(buf,"%d",stats.amount_defender_drains);
					gui.float_label(d->first,buf,0,255,0);

					d->second.gets_hit(-stats.amount_defender_drains);
				}

				//if the attacker is turned to stone, the fight stops immediately
				static const std::string stone_string("stone");
				if(stats.defend_special == stone_string) {
					gui.float_label(a->first,translate_string("stone"),255,0,0);
					a->second.set_flag(stone_string);
					stats.ndefends = 0;
					stats.nattacks = 0;
					game_events::fire(stone_string,a->first,d->first);
				}
			}

			--stats.ndefends;
		}

		if(to_the_death > 0 && stats.ndefends == 0 && stats.nattacks == 0) {
			stats.nattacks = orig_attacks;
			stats.ndefends = orig_defends;
			--to_the_death;
		}
	}

	if(attackerxp) {
		a->second.get_experience(attackerxp);
	}

	if(defenderxp) {
		d->second.get_experience(defenderxp);
	}

	gui.invalidate_unit();
}

int village_owner(const gamemap::location& loc, std::vector<team>& teams)
{
	for(size_t i = 0; i != teams.size(); ++i) {
		if(teams[i].owns_village(loc))
			return i;
	}

	return -1;
}


void get_village(const gamemap::location& loc, std::vector<team>& teams,
                 size_t team_num, const unit_map& units)
{
	if(team_num < teams.size() && teams[team_num].owns_village(loc)) {
		return;
	}

	const bool has_leader = find_leader(units,int(team_num+1)) != units.end();

	//we strip the village off all other sides, unless it is held by an ally
	//and we don't have a leader (and thus can't occupy it)
	for(std::vector<team>::iterator i = teams.begin(); i != teams.end(); ++i) {
		const int side = i - teams.begin() + 1;
		if(team_num >= teams.size() || has_leader || teams[team_num].is_enemy(side)) {
			i->lose_village(loc);
		}
	}

	if(team_num >= teams.size()) {
		return;
	}

	if(has_leader) {
		teams[team_num].get_village(loc);
	}
}

unit_map::iterator find_leader(unit_map& units, int side)
{
	for(unit_map::iterator i = units.begin(); i != units.end(); ++i) {
		if(i->second.side() == side && i->second.can_recruit())
			return i;
	}

	return units.end();
}

unit_map::const_iterator find_leader(const unit_map& units, int side)
{
	for(unit_map::const_iterator i = units.begin(); i != units.end(); ++i) {
		if(i->second.side() == side && i->second.can_recruit())
			return i;
	}

	return units.end();
}

namespace {

//function which returns true iff the unit at 'loc' will heal a unit from side 'side'
//on this turn.
//
//units heal other units if they are (1) on the same side as them; or (2) are on a
//different but allied side, and there are no 'higher priority' sides also adjacent
//to the healer
bool will_heal(const gamemap::location& loc, int side, const std::vector<team>& teams,
			   const unit_map& units)
{
	const unit_map::const_iterator healer_it = units.find(loc);
	if(healer_it == units.end() || healer_it->second.type().heals() == false) {
		return false;
	}

	const unit& healer = healer_it->second;

	if(healer.incapacitated()) {
		return false;
	}

	if(healer.side() == side) {
		return true;
	}

	if(size_t(side-1) >= teams.size() || size_t(healer.side()-1) >= teams.size()) {
		return false;
	}

	//if the healer is an enemy, it won't heal
	if(teams[healer.side()-1].is_enemy(side)) {
		return false;
	}

	gamemap::location adjacent[6];
	get_adjacent_tiles(loc,adjacent);
	for(int n = 0; n != 6; ++n) {
		const unit_map::const_iterator u = units.find(adjacent[n]);
		if(u != units.end() && u->second.hitpoints() < u->second.max_hitpoints()) {
			const int unit_side = u->second.side();

			//the healer won't heal an ally if there is a wounded unit on the same
			//side next to her
			if(unit_side == healer.side())
				return false;

			//choose an arbitrary order for healing
			if(unit_side > side)
				return false;
		}
	}

	//there's no-one of higher priority nearby, so the ally will heal
	return true;
}

}

void calculate_healing(display& disp, const gamestatus& status, const gamemap& map,
                       std::map<gamemap::location,unit>& units, int side,
					   const std::vector<team>& teams)
{
	std::map<gamemap::location,int> healed_units, max_healing;

	//a map of healed units to their healers
	std::multimap<gamemap::location,gamemap::location> healers;

	std::map<gamemap::location,unit>::iterator i;
	int amount_healed;
	for(i = units.begin(); i != units.end(); ++i) {
		amount_healed = 0;

		//the unit heals if it's on this side, and it's on a village or
		//it has regeneration, and it is wounded
		if(i->second.side() == side) {
			if(i->second.hitpoints() < i->second.max_hitpoints()){
				if(map.gives_healing(i->first) || i->second.type().regenerates()) {
					amount_healed = game_config::cure_amount;
				}
			}
			if(amount_healed != 0)
				healed_units.insert(std::pair<gamemap::location,int>(
			                            i->first, amount_healed));
		}

		//otherwise find the maximum healing for the unit
		if(amount_healed == 0) {
			int max_heal = 0;
			gamemap::location adjacent[6];
			get_adjacent_tiles(i->first,adjacent);
			for(int j = 0; j != 6; ++j) {
				if(will_heal(adjacent[j],i->second.side(),teams,units)) {
					const unit_map::const_iterator healer = units.find(adjacent[j]);
					max_heal = maximum(max_heal,healer->second.type().max_unit_healing());

					healers.insert(std::pair<gamemap::location,gamemap::location>(i->first,adjacent[j]));
				}
			}

			if(max_heal > 0) {
				max_healing.insert(std::pair<gamemap::location,int>(i->first,max_heal));
			}
		}
	}

	//now see about units that can heal other units
	for(i = units.begin(); i != units.end(); ++i) {

		if(will_heal(i->first,side,teams,units)) {
			gamemap::location adjacent[6];
			bool gets_healed[6];
			get_adjacent_tiles(i->first,adjacent);

			int nhealed = 0;
			int j;
			for(j = 0; j != 6; ++j) {
				const std::map<gamemap::location,unit>::const_iterator adj =
				                                   units.find(adjacent[j]);
				if(adj != units.end() &&
				   adj->second.hitpoints() < adj->second.max_hitpoints() &&
				   adj->second.side() == side &&
				   healed_units[adj->first] < max_healing[adj->first]) {
					++nhealed;
					gets_healed[j] = true;
				} else {
					gets_healed[j] = false;
				}
			}

			if(nhealed == 0)
				continue;

			const int healing_per_unit = i->second.type().heals()/nhealed;

			for(j = 0; j != 6; ++j) {
				if(!gets_healed[j])
					continue;

				assert(units.find(adjacent[j]) != units.end());

				healed_units[adjacent[j]]
				        = minimum(max_healing[adjacent[j]],
				                  healed_units[adjacent[j]]+healing_per_unit);
			}
		}
	}

	//poisoned units will take the same amount of damage per turn, as
	//curing heals until they are reduced to 1 hitpoint. If they are
	//cured on a turn, they recover 0 hitpoints that turn, but they
	//are no longer poisoned
	for(i = units.begin(); i != units.end(); ++i) {

		if(i->second.side() == side && i->second.has_flag("poisoned")) {
			const int damage = minimum<int>(game_config::cure_amount,
			                                i->second.hitpoints()-1);

			if(damage > 0) {
				healed_units.insert(std::pair<gamemap::location,int>(i->first,-damage));
			}
		}
	}

	for(i = units.begin(); i != units.end(); ++i) {
		if(i->second.side() == side) {
			if(i->second.hitpoints() < i->second.max_hitpoints() || 
					i->second.has_flag("poisoned")){
				if(i->second.is_resting()) {
					const std::map<gamemap::location,int>::iterator u =
						healed_units.find(i->first);
					if(u != healed_units.end()) {
						healed_units[i->first] += game_config::rest_heal_amount;
					} else {
						healed_units.insert(std::pair<gamemap::location,int>(i->first,game_config::rest_heal_amount));
					}
				}
			}
			i->second.set_resting(true);
		}
	}

	for(std::map<gamemap::location,int>::iterator h = healed_units.begin();
	    h != healed_units.end(); ++h) {

		const gamemap::location& loc = h->first;

		assert(units.count(loc) == 1);

		unit& u = units.find(loc)->second;

		const bool show_healing = !disp.turbo() && !recorder.skipping() &&
		                          !disp.fogged(loc.x,loc.y) &&
								  (!u.invisible(map.underlying_terrain(map[h->first.x][h->first.y]),
								                status.get_time_of_day().lawful_bonus,h->first,units,teams) ||
								                teams[disp.viewing_team()].is_enemy(side) == false);

		typedef std::multimap<gamemap::location,gamemap::location>::const_iterator healer_itor;
		const std::pair<healer_itor,healer_itor> healer_itors = healers.equal_range(loc);

		if(show_healing) {
			disp.scroll_to_tile(loc.x,loc.y,display::WARP);
			disp.select_hex(loc);


			//iterate over any units that are healing this unit, and make them
			//enter their healing frame
			for(healer_itor i = healer_itors.first; i != healer_itors.second; ++i) {
				assert(units.count(i->second));
				unit& healer = units.find(i->second)->second;
				healer.set_healing(true);

				disp.draw_tile(i->second.x,i->second.y);
			}

			disp.update_display();
		}

		const int DelayAmount = 50;

		std::cerr << "unit is poisoned? " << (u.has_flag("poisoned") ? "yes" : "no") << "," << h->second << "," << max_healing[h->first] << "\n";

		if(u.has_flag("poisoned") && h->second > 0) {

			//poison is purged only if we are on a village or next to a curer
			if(h->second >= game_config::cure_amount ||
			   max_healing[h->first] >= game_config::cure_amount) {
				u.remove_flag("poisoned");

				if(show_healing) {
					events::pump();

					sound::play_sound("heal.wav");
					SDL_Delay(DelayAmount);
					disp.invalidate_unit();
					disp.update_display();
				}
			}

			h->second = 0;
		} else {
			if(h->second > 0 && h->second > u.max_hitpoints()-u.hitpoints()) {
				h->second = u.max_hitpoints()-u.hitpoints();
				if(h->second <= 0)
					continue;
			}
			
			if(h->second < 0) {
				if(show_healing) {
					sound::play_sound("groan.wav");
					disp.float_label(h->first,lexical_cast<std::string>(h->second*-1),255,0,0);
				}
			} else if(h->second > 0) {
				if(show_healing) {
					sound::play_sound("heal.wav");
					disp.float_label(h->first,lexical_cast<std::string>(h->second),0,255,0);
				}
			}
		}

		while(h->second > 0) {
			const Uint16 heal_colour = disp.rgb(0,0,200);
			u.heal(1);

			if(show_healing) {
				if(is_odd(h->second))
					disp.draw_tile(loc.x,loc.y,NULL,0.5,heal_colour);
				else
					disp.draw_tile(loc.x,loc.y);

				events::pump();
				SDL_Delay(DelayAmount);
				disp.update_display();
			}

			--h->second;
		}

		while(h->second < 0) {
			const Uint16 damage_colour = disp.rgb(200,0,0);
			u.gets_hit(1);

			if(show_healing) {
				if(is_odd(h->second))
					disp.draw_tile(loc.x,loc.y,NULL,0.5,damage_colour);
				else
					disp.draw_tile(loc.x,loc.y);

				events::pump();

				SDL_Delay(DelayAmount);
				disp.update_display();
			}

			++h->second;
		}

		if(show_healing) {
			for(healer_itor i = healer_itors.first; i != healer_itors.second; ++i) {
				assert(units.count(i->second));
				unit& healer = units.find(i->second)->second;
				healer.set_healing(false);

				events::pump();

				disp.draw_tile(i->second.x,i->second.y);
			}
			
			disp.draw_tile(loc.x,loc.y);
			disp.update_display();
		}
	}
}

unit get_advanced_unit(const game_data& info,
                  std::map<gamemap::location,unit>& units,
                  const gamemap::location& loc, const std::string& advance_to)
{
	const std::map<std::string,unit_type>::const_iterator new_type = info.unit_types.find(advance_to);
	const std::map<gamemap::location,unit>::iterator un = units.find(loc);
	if(new_type != info.unit_types.end() && un != units.end()) {
		return unit(&(new_type->second),un->second);
	} else {
		throw gamestatus::game_error("Could not find the unit being advanced"
		                             " to: " + advance_to);
	}
}

void advance_unit(const game_data& info,
                  std::map<gamemap::location,unit>& units,
                  gamemap::location loc, const std::string& advance_to)
{
	const unit& new_unit = get_advanced_unit(info,units,loc,advance_to);

	statistics::advance_unit(new_unit);
	
	units.erase(loc);
	units.insert(std::pair<gamemap::location,unit>(loc,new_unit));
}

void check_victory(std::map<gamemap::location,unit>& units,
                   std::vector<team>& teams)
{
	std::vector<int> seen_leaders;
	for(std::map<gamemap::location,unit>::const_iterator i = units.begin();
	    i != units.end(); ++i) {
		if(i->second.can_recruit()) {
			std::cerr << "seen leader for side " << i->second.side() << "\n";
			seen_leaders.push_back(i->second.side());
		}
	}

	//clear villages for teams that have no leader
	for(std::vector<team>::iterator tm = teams.begin(); tm != teams.end(); ++tm) {
		if(std::find(seen_leaders.begin(),seen_leaders.end(),tm-teams.begin() + 1) == seen_leaders.end()) {
			tm->clear_villages();
		}
	}

	bool found_enemies = false;
	bool found_human = false;

	for(size_t n = 0; n != seen_leaders.size(); ++n) {
		const size_t side = seen_leaders[n]-1;

		assert(side < teams.size());

		for(size_t m = n+1; m != seen_leaders.size(); ++m) {
			if(side < teams.size() && teams[side].is_enemy(seen_leaders[m])) {
				found_enemies = true;
			}
		}

		if(side < teams.size() && teams[side].is_human()) {
			found_human = true;
		}
	}

	if(found_enemies == false) {
		if(found_human) {
			game_events::fire("enemies defeated");
			if (victory_conditions::victory_when_enemies_defeated() == false) {
				// this level has asked not to be ended by this condition
				return;
			}
		}


		if(non_interactive()) {
			std::cout << "winner: ";
			for(std::vector<int>::const_iterator i = seen_leaders.begin(); i != seen_leaders.end(); ++i) {
				std::cout << *i << " ";
			}

			std::cout << "\n";
		}

		std::cerr << "throwing end level exception...\n";
		throw end_level_exception(found_human ? VICTORY : DEFEAT);
	}

	//remove any units which are leaderless
	//this code currently removed, to try not removing leaderless enemies
	/*
	std::map<gamemap::location,unit>::iterator j = units.begin();
	while(j != units.end()) {
		if(std::find(seen_leaders.begin(),seen_leaders.end(),j->second.side()) == seen_leaders.end()) {
			units.erase(j);
			j = units.begin();
		} else {
			++j;
		}
	}*/
}

const time_of_day& timeofday_at(const gamestatus& status,const unit_map& units,const gamemap::location& loc)
{
	bool lighten = false;

	if(loc.valid()) {
		gamemap::location locs[7];
		locs[0] = loc;
		get_adjacent_tiles(loc,locs+1);

		for(int i = 0; i != 7; ++i) {
			const unit_map::const_iterator itor = units.find(locs[i]);
			if(itor != units.end() &&
			   itor->second.type().illuminates() && itor->second.incapacitated() == false) {
				lighten = true;
			}
		}
	}

	return status.get_time_of_day(lighten,loc);
}

int combat_modifier(const gamestatus& status,
                    const std::map<gamemap::location,unit>& units,
					const gamemap::location& loc,
					unit_type::ALIGNMENT alignment)
{
	const time_of_day& tod = timeofday_at(status,units,loc);

	int bonus = tod.lawful_bonus;

	if(alignment == unit_type::NEUTRAL)
		bonus = 0;
	else if(alignment == unit_type::CHAOTIC)
		bonus = -bonus;

	return bonus;
}

namespace {

bool clear_shroud_loc(const gamemap& map, team& tm,
                      const gamemap::location& loc,
                      std::vector<gamemap::location>* cleared)
{
	bool result = false;
	gamemap::location adj[7];
	get_adjacent_tiles(loc,adj);
	adj[6] = loc;
	for(int i = 0; i != 7; ++i) {
		if(map.on_board(adj[i])) {
			const bool res = tm.clear_shroud(adj[i].x,adj[i].y) ||
								tm.clear_fog(adj[i].x,adj[i].y);

			if(res && cleared != NULL) {
				cleared->push_back(adj[i]);
			}

			result |= res;
		}
	}

	return result;
}

//returns true iff some shroud is cleared
//seen_units will return new units that have been seen by this unit
//if known_units is NULL, seen_units can be NULL and will not be changed
bool clear_shroud_unit(const gamemap& map, 
		                 const gamestatus& status,
							  const game_data& gamedata,
                       const unit_map& units, const gamemap::location& loc,
                       std::vector<team>& teams, int team,
					   const std::set<gamemap::location>* known_units,
					   std::set<gamemap::location>* seen_units)
{
	std::vector<gamemap::location> cleared_locations;

	paths p(map,status,gamedata,units,loc,teams,true,false);
	for(paths::routes_map::const_iterator i = p.routes.begin();
	    i != p.routes.end(); ++i) {
		clear_shroud_loc(map,teams[team],i->first,&cleared_locations);
	}

	//clear the location the unit is at
	clear_shroud_loc(map,teams[team],loc,&cleared_locations);

	for(std::vector<gamemap::location>::const_iterator it =
	    cleared_locations.begin(); it != cleared_locations.end(); ++it) {
		if(units.count(*it)) {
			if(seen_units == NULL || known_units == NULL) {
				static const std::string sighted("sighted");
				game_events::fire(sighted,*it,loc);
			} else if(known_units->count(*it) == 0) {
				seen_units->insert(*it);
			}
		}
	}

	return cleared_locations.empty() == false;
}

}

void recalculate_fog(const gamemap& map, const gamestatus& status,
		const game_data& gamedata, const unit_map& units, 
		std::vector<team>& teams, int team) {

	teams[team].refog();

	for(unit_map::const_iterator i = units.begin(); i != units.end(); ++i) {
		if(i->second.side() == team+1) {

			//we're not really going to mutate the unit, just temporarily
			//set its moves to maximum, but then switch them back
			unit& mutable_unit = const_cast<unit&>(i->second);
			const unit_movement_resetter move_resetter(mutable_unit);

			clear_shroud_unit(map,status,gamedata,units,i->first,teams,team,NULL,NULL);
		}
	}
}

bool clear_shroud(display& disp, const gamestatus& status, 
		            const gamemap& map, const game_data& gamedata,
                  const unit_map& units, std::vector<team>& teams, int team)
{
	if(teams[team].uses_shroud() == false && teams[team].uses_fog() == false)
		return false;

	bool result = false;

	unit_map::const_iterator i;
	for(i = units.begin(); i != units.end(); ++i) {
		if(i->second.side() == team+1) {

			//we're not really going to mutate the unit, just temporarily
			//set its moves to maximum, but then switch them back
			unit& mutable_unit = const_cast<unit&>(i->second);
			const unit_movement_resetter move_resetter(mutable_unit);

			result |= clear_shroud_unit(map,status,gamedata,units,i->first,teams,team,NULL,NULL);
		}
	}

	recalculate_fog(map,status,gamedata,units,teams,team);

	disp.labels().recalculate_shroud();

	return result;
}

size_t move_unit(display* disp, const game_data& gamedata, 
                 const gamestatus& status, const gamemap& map,
                 unit_map& units, std::vector<team>& teams,
                 std::vector<gamemap::location> route,
                 replay* move_recorder, undo_list* undo_stack,
                 gamemap::location *next_unit, bool continue_move)
{
	//stop the user from issuing any commands while the unit is moving
	const command_disabler disable_commands(disp);

	assert(!route.empty());

	unit_map::iterator ui = units.find(route.front());

	assert(ui != units.end());

	ui->second.set_goto(gamemap::location());

	unit u = ui->second;

	const size_t team_num = u.side()-1;

	const bool skirmisher = u.type().is_skirmisher();

	team& team = teams[team_num];
	const bool check_shroud = team.auto_shroud_updates() &&
		(team.uses_shroud() || team.uses_fog());
	
	//if we use shroud/fog of war, count out the units we can currently see
	std::set<gamemap::location> known_units;
	if(check_shroud) {
		for(unit_map::const_iterator u = units.begin(); u != units.end(); ++u) {
			if(team.fogged(u->first.x,u->first.y) == false) {
				known_units.insert(u->first);
			}
		}
	}

	//see how far along the given path we can move
	const int starting_moves = u.movement_left();
	int moves_left = starting_moves;
	std::set<gamemap::location> seen_units;
	bool discovered_unit = false;
	bool should_clear_stack = false;
	std::vector<gamemap::location>::const_iterator step;
	for(step = route.begin()+1; step != route.end(); ++step) {
		const gamemap::TERRAIN terrain = map[step->x][step->y];

		const unit_map::const_iterator enemy_unit = units.find(*step);
			
		const int mv = u.movement_cost(map,terrain);
		if(discovered_unit || continue_move == false && seen_units.empty() == false ||
		   mv > moves_left || enemy_unit != units.end() && team.is_enemy(enemy_unit->second.side())) {
			break;
		} else {
			moves_left -= mv;
		}

		if(!skirmisher && enemy_zoc(map,status,units,teams,*step,team,u.side())) {
			moves_left = 0;
		}

		//if we use fog or shroud, see if we have sighted an enemy unit, in
		//which case we should stop immediately.
		if(check_shroud) {
			if(units.count(*step) == 0 && !map.is_village(*step)) {
				std::cerr << "checking for units from " << (step->x+1) << "," << (step->y+1) << "\n";

				//temporarily reset the unit's moves to full
				const unit_movement_resetter move_resetter(ui->second);

				//we have to swap out any unit that is already in the hex, so we can put our
				//unit there, then we'll swap back at the end.
				const temporary_unit_placer unit_placer(units,*step,ui->second);

				should_clear_stack |= clear_shroud_unit(map,status,gamedata,units,*step,teams,
				                                        ui->second.side()-1,&known_units,&seen_units);
			}
		}

		//check if we have discovered an invisible enemy unit
		gamemap::location adjacent[6];
		get_adjacent_tiles(*step,adjacent);

		for(int i = 0; i != 6; ++i) {
			//check if we are checking ourselves
			if(adjacent[i] == ui->first)
				continue;

			const std::map<gamemap::location,unit>::const_iterator it = units.find(adjacent[i]);
			if(it != units.end() && teams[u.side()-1].is_enemy(it->second.side()) &&
			   it->second.invisible(map.underlying_terrain(map[it->first.x][it->first.y]),
			   status.get_time_of_day().lawful_bonus,it->first,units,teams)) {
				discovered_unit = true;
				should_clear_stack = true;
				break;
			}
		}
	}

	//make sure we don't tread on another unit
	std::vector<gamemap::location>::const_iterator begin = route.begin();

	std::vector<gamemap::location> steps(begin,step);
	while(!steps.empty() && units.count(steps.back()) != 0) {
		steps.pop_back();
	}

	assert(steps.size() <= route.size());

	if (next_unit != NULL)
		*next_unit = steps.back();

	//if we can't get all the way there and have to set a go-to.
	if(steps.size() != route.size() && discovered_unit == false) {
		if(seen_units.empty() == false) {
			u.set_interrupted_move(route.back());
		} else {
			u.set_goto(route.back());
		}
	} else {
		u.set_interrupted_move(gamemap::location());
	}
	
	if(steps.size() < 2) {
		return 0;
	}

	units.erase(ui);
	if(disp != NULL) {
		unit_display::move_unit(*disp,map,steps,u);
	}

	u.set_movement(moves_left);

	ui = units.insert(std::pair<gamemap::location,unit>(steps.back(),u)).first;
	if(disp != NULL) {
		disp->invalidate_unit();
		disp->invalidate(steps.back());
	}

	int orig_village_owner = -1;
	if(map.is_village(steps.back())) {
		orig_village_owner = village_owner(steps.back(),teams);

		if(orig_village_owner != team_num) {
			get_village(steps.back(),teams,team_num,units);
			ui->second.set_movement(0);
		}
	}

	if(move_recorder != NULL) {
		move_recorder->add_movement(steps.front(),steps.back());
	}

	const bool event_mutated = game_events::fire("moveto",steps.back());

	if(undo_stack != NULL) {
		if(event_mutated || should_clear_stack) {
			apply_shroud_changes(*undo_stack,disp,status,map,gamedata,units,teams,team_num);
			undo_stack->clear();
		} else {
			undo_stack->push_back(undo_action(u,steps,starting_moves,orig_village_owner));
		}
	}

	if(disp != NULL) {
		disp->set_route(NULL);

		//show messages on the screen here
		if(discovered_unit) {
			//we've been ambushed, so display an appropriate message
			font::add_floating_label(string_table["ambushed"],24,font::BAD_COLOUR,
			                         disp->map_area().w/2,disp->map_area().h/3,
									 0.0,0.0,100,disp->map_area(),font::CENTER_ALIGN);
		}

		if(continue_move == false && seen_units.empty() == false) {
			//the message depends on how many units have been sighted, and whether
			//they are allies or enemies, so calculate that out here
			int nfriends = 0, nenemies = 0;
			for(std::set<gamemap::location>::const_iterator i = seen_units.begin(); i != seen_units.end(); ++i) {
				std::cerr << "processing unit at " << (i->x+1) << "," << (i->y+1) << "\n";
				const unit_map::const_iterator u = units.find(*i);
				assert(u != units.end());
				if(team.is_enemy(u->second.side())) {
					++nenemies;
				} else {
					++nfriends;
				}

				std::cerr << "processed...\n";
			}

			std::string msg_id;

			//the message we display is different depending on whether units sighted
			//were enemies or friends, and whether there is one or more
			if(seen_units.size() == 1) {
				if(nfriends == 1) {
					msg_id = "friendly_unit_sighted";
				} else {
					msg_id = "enemy_unit_sighted";
				}
			}
			else if(nfriends == 0 || nenemies == 0) {
				if(nfriends > 0) {
					msg_id = "friendly_units_sighted";
				} else {
					msg_id = "enemy_units_sighted";
				}
			}
			else {
				msg_id = "units_sighted";
			}

			string_map symbols;
			symbols["friends"] = lexical_cast<std::string>(nfriends);
			symbols["enemies"] = lexical_cast<std::string>(nenemies);

			std::stringstream msg;
			msg << string_table[msg_id];
			
			if(u.movement_left() > 0) {
				//see if the "Continue Move" action has an associated hotkey
				const std::vector<hotkey::hotkey_item>& hotkeys = hotkey::get_hotkeys();
				std::vector<hotkey::hotkey_item>::const_iterator hk;
				for(hk = hotkeys.begin(); hk != hotkeys.end(); ++hk) {
					if(hk->action == hotkey::HOTKEY_CONTINUE_MOVE) {
						break;
					}
				}
				if(hk != hotkeys.end()) {
					symbols["hotkey"] = hotkey::get_hotkey_name(*hk);
					msg << '\n' << string_table["press_to_continue"];
				}
			}
			std::cerr << "formatting string...\n";
			const std::string message = config::interpolate_variables_into_string(msg.str(),&symbols);

			std::cerr << "displaying label...\n";
			font::add_floating_label(message,24,font::BAD_COLOUR,
			                         disp->map_area().w/2,disp->map_area().h/3,
									 0.0,0.0,100,disp->map_area(),font::CENTER_ALIGN);
		}

		disp->draw();
		disp->recalculate_minimap();
	}

	assert(steps.size() <= route.size());

	return steps.size();
}

bool unit_can_move(const gamemap::location& loc, const unit_map& units,
                   const gamemap& map, const std::vector<team>& teams)
{
	const unit_map::const_iterator u_it = units.find(loc);
	assert(u_it != units.end());
	
	const unit& u = u_it->second;
	const team& current_team = teams[u.side()-1];

	if(!u.can_attack())
		return false;

	//units with goto commands that have already done their gotos this turn
	//(i.e. don't have full movement left) should be red
	if(u.movement_left() < u.total_movement() && u.get_goto().valid()) {
		return false;
	}

	gamemap::location locs[6];
	get_adjacent_tiles(loc,locs);
	for(int n = 0; n != 6; ++n) {
		if(map.on_board(locs[n])) {
			const unit_map::const_iterator i = units.find(locs[n]);
			if(i != units.end()) {
				if(current_team.is_enemy(i->second.side())) {
					return true;
				}
			}
			
			if(u.movement_cost(map,map[locs[n].x][locs[n].y]) <= u.movement_left()) {
				return true;
			}
		}
	}

	return false;
}

void apply_shroud_changes(undo_list& undos, display* disp, const gamestatus& status, const gamemap& map,
	const game_data& gamedata, const unit_map& units, std::vector<team>& teams, int team)
{
	// No need to do this if the team isn't using fog or shroud.
	if(!teams[team].uses_shroud() && !teams[team].uses_fog())
		return;
	/*
		This function works thusly:
		1. run through the list of undo_actions
		2. for each one, play back the unit's move
		3. for each location along the route, clear any "shrouded" squares that the unit can see
		4. clear the undo_list
		5. call clear_shroud to update the fog of war for each unit.
		6. fix up associated display stuff (done in a similar way to turn_info::undo())
	*/
	for(undo_list::const_iterator un = undos.begin(); un != undos.end(); ++un) {
		if(un->is_recall()) continue;
		//we're not really going to mutate the unit, just temporarily
		//set its moves to maximum, but then switch them back
		const unit_movement_resetter move_resetter(const_cast<unit&>(un->affected_unit));
		
		std::vector<gamemap::location>::const_iterator step;
		for(step = un->route.begin(); step != un->route.end(); ++step) {
			//we have to swap out any unit that is already in the hex, so we can put our
			//unit there, then we'll swap back at the end.
			const temporary_unit_placer unit_placer(const_cast<unit_map&>(units),*step,un->affected_unit);
			clear_shroud_unit(map,status,gamedata,units,*step,teams,team,NULL,NULL);
		}
	}
	if(disp != NULL) {
		disp->invalidate_unit();
		disp->invalidate_game_status();
		clear_shroud(*disp,status,map,gamedata,units,teams,team);
		disp->recalculate_minimap();
		disp->set_paths(NULL);
		disp->set_route(NULL);
	} else {
		recalculate_fog(map,status,gamedata,units,teams,team);
	}
}
