/* $Id$ */
/*
   Copyright (C) 2009 - 2010 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file ai/default/contexts.hpp
 * Default AI contexts
 */

#ifndef AI_DEFAULT_CONTEXTS_HPP_INCLUDED
#define AI_DEFAULT_CONTEXTS_HPP_INCLUDED

#include "../../global.hpp"

#include "../contexts.hpp"
#include "formula_callable.hpp"
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

//============================================================================
namespace ai {


struct target {
	enum TYPE { VILLAGE, LEADER, EXPLICIT, THREAT, BATTLE_AID, MASS, SUPPORT };

	target(const map_location& pos, double val, TYPE target_type=VILLAGE) : loc(pos), value(val), type(target_type)
	{}
	map_location loc;
	double value;

	TYPE type;
	//@todo 1.7: ai goals: this is a 'target' marker class which should be expanded with additional information which is generic enough to apply to all targets.
};


class attack_analysis : public game_logic::formula_callable
{
public:
	attack_analysis() :
		game_logic::formula_callable(),
		target(),
		movements(),
		target_value(0.0),
		avg_losses(0.0),
		chance_to_kill(0.0),
		avg_damage_inflicted(0.0),
		target_starting_damage(0),
		avg_damage_taken(0.0),
		resources_used(0.0),
		terrain_quality(0.0),
		alternative_terrain_quality(0.0),
		vulnerability(0.0),
		support(0.0),
		leader_threat(false),
		uses_leader(false),
		is_surrounded(false)
	{
	}

	void analyze(const gamemap& map, unit_map& units,
				 const readonly_context& ai_obj,
				 const move_map& dstsrc, const move_map& srcdst,
				 const move_map& enemy_dstsrc, double aggression);

	double rating(double aggression, const readonly_context& ai_obj) const;
	variant get_value(const std::string& key) const;
	void get_inputs(std::vector<game_logic::formula_input>* inputs) const;

	bool attack_close(const map_location& loc) const;

	map_location target;
	std::vector<std::pair<map_location,map_location> > movements;

	/** The value of the unit being targeted. */
	double target_value;

	/** The value on average, of units lost in the combat. */
	double avg_losses;

	/** Estimated % chance to kill the unit. */
	double chance_to_kill;

	/** The average hitpoints damage inflicted. */
	double avg_damage_inflicted;

	int target_starting_damage;

	/** The average hitpoints damage taken. */
	double avg_damage_taken;

	/** The sum of the values of units used in the attack. */
	double resources_used;

	/** The weighted average of the % chance to hit each attacking unit. */
	double terrain_quality;

	/**
	 * The weighted average of the % defense of the best possible terrain
	 * that the attacking units could reach this turn, without attacking
	 * (good for comparison to see just how good/bad 'terrain_quality' is).
	 */
	double alternative_terrain_quality;

	/**
	 * The vulnerability is the power projection of enemy units onto the hex
	 * we're standing on. support is the power projection of friendly units.
	 */
	double vulnerability, support;

	/** Is true if the unit is a threat to our leader. */
	bool leader_threat;

	/** Is true if this attack sequence makes use of the leader. */
	bool uses_leader;

	/** Is true if the units involved in this attack sequence are surrounded. */
	bool is_surrounded;

};


class default_ai_context;
class default_ai_context : public virtual readwrite_context{
public:

	virtual int count_free_hexes_in_castle(const map_location& loc, std::set<map_location> &checked_hexes) = 0;


	/** Constructor */
	default_ai_context();


	/** Destructor */
	virtual ~default_ai_context();


	virtual const std::vector<target>& additional_targets() const = 0;


	virtual void add_target(const target& t) const = 0;


	virtual void clear_additional_targets() const = 0;


	virtual default_ai_context& get_default_ai_context() = 0;


	virtual std::vector<target> find_targets(const move_map& enemy_dstsrc) = 0;


	virtual bool multistep_move_possible(const map_location& from,
		const map_location& to, const map_location& via,
		const moves_map& possible_moves) const = 0;


	virtual int rate_terrain(const unit& u, const map_location& loc) const = 0;


	virtual config to_default_ai_context_config() const = 0;


};


// proxies
class default_ai_context_proxy : public virtual default_ai_context, public virtual readwrite_context_proxy {
public:

	int count_free_hexes_in_castle(const map_location& loc, std::set<map_location> &checked_hexes)
	{
		return target_->count_free_hexes_in_castle(loc, checked_hexes);
	}


	default_ai_context_proxy()
		: target_(NULL)
	{
	}


	virtual	~default_ai_context_proxy();


	virtual const std::vector<target>& additional_targets() const
	{
		return target_->additional_targets();
	}


	virtual void add_target(const target& t) const
	{
		target_->add_target(t);
	}


	virtual void clear_additional_targets() const
	{
		target_->clear_additional_targets();
	}


	virtual default_ai_context& get_default_ai_context()
	{
		return target_->get_default_ai_context();
	}


	virtual std::vector<target> find_targets(const move_map& enemy_dstsrc)
	{
		return target_->find_targets(enemy_dstsrc);
	}


	void init_default_ai_context_proxy(default_ai_context &target);


	virtual bool multistep_move_possible(const map_location& from,
		const map_location& to, const map_location& via,
		const moves_map& possible_moves) const
	{
		return target_->multistep_move_possible(from,to,via,possible_moves);
	}


	virtual int rate_terrain(const unit& u, const map_location& loc) const
	{
		return target_->rate_terrain(u,loc);
	}


	virtual config to_default_ai_context_config() const
	{
		return target_->to_default_ai_context_config();
	}

private:
	default_ai_context *target_;
};


class default_ai_context_impl : public virtual readwrite_context_proxy, public default_ai_context {
public:

	int count_free_hexes_in_castle(const map_location& loc, std::set<map_location> &checked_hexes);


	default_ai_context_impl(readwrite_context &context, const config &/*cfg*/)
		: recursion_counter_(context.get_recursion_count()),additional_targets_()
	{
		init_readwrite_context_proxy(context);
	}


	virtual ~default_ai_context_impl();


	virtual default_ai_context& get_default_ai_context();


	virtual const std::vector<target>& additional_targets() const;


	virtual void add_target(const target& t) const;


	virtual void clear_additional_targets() const;


	int get_recursion_count() const
	{
		return recursion_counter_.get_count();
	}


	virtual std::vector<target> find_targets(const move_map& enemy_dstsrc);


	virtual bool multistep_move_possible(const map_location& from,
		const map_location& to, const map_location& via,
		const moves_map& possible_moves) const;


	virtual int rate_terrain(const unit& u, const map_location& loc) const;


	virtual config to_default_ai_context_config() const;

private:
	recursion_counter recursion_counter_;
	mutable std::vector<target> additional_targets_;//@todo 1.9 refactor this


};

} //end of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#endif
