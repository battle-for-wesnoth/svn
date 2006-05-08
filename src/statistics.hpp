/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef STATISTICS_HPP_INCLUDED
#define STATISTICS_HPP_INCLUDED

class unit;
#include "actions.hpp"

namespace statistics
{
	struct stats
	{
		stats();
		explicit stats(const config& cfg);

		config write() const;
		void read(const config& cfg);

		typedef std::map<std::string,int> str_int_map;
		str_int_map recruits, recalls, advanced_to, deaths, killed;
		int recruit_cost, recall_cost;

		//a type that will map a string of hit/miss to the number of times
		//that sequence has occurred
		typedef str_int_map battle_sequence_frequency_map;

		//a type that will map different % chances to hit to different results
		typedef std::map<int,battle_sequence_frequency_map> battle_result_map;

		battle_result_map attacks, defends;

		int damage_inflicted, damage_taken;

		//expected value for damage inflicted/taken * 100, based on probability
		//to hit, Use this long term to see how lucky a side is
		// FIXME: Since integers are used, rounding errors accumulate. Also,
		// slow isn't accounted for properly. Rusty's simulator could be used
		// obtain valid values.
		int expected_damage_inflicted, expected_damage_taken;
	};

	int sum_str_int_map(const stats::str_int_map& m);

	struct disabler
	{
		disabler();
		~disabler();
	};

	
	struct scenario_context
	{
		scenario_context(const std::string& name);
		~scenario_context();
	};

	struct attack_context
	{
		attack_context(const unit& a, const unit& d, int a_cth, int d_cth);
		~attack_context();

		enum ATTACK_RESULT { MISSES, HITS, KILLS };

		void attack_result(ATTACK_RESULT res, int damage);
		void defend_result(ATTACK_RESULT res, int damage);

	private:

		std::string attacker_type, defender_type;
		int attacker_side, defender_side;
		int chance_to_hit_defender, chance_to_hit_attacker;
		std::string attacker_res, defender_res;

		stats& attacker_stats();
		stats& defender_stats();
	};

	void recruit_unit(const unit& u);
	void recall_unit(const unit& u);
	void un_recall_unit(const unit& u);
	void un_recruit_unit(const unit& u);

	void advance_unit(const unit& u);

	config write_stats();
	void read_stats(const config& cfg);
	void fresh_stats();
	void clear_current_scenario();

	std::vector<std::string> get_categories();
	stats calculate_stats(int category, int side);
}

#endif
