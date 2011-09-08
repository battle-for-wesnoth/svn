/* $Id$ */
/*
   Copyright (C) 2003 - 2011 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef UNIT_H_INCLUDED
#define UNIT_H_INCLUDED

#include "formula_callable.hpp"
#include "unit_types.hpp"
#include "unit_map.hpp"

class game_display;
class game_state;
class vconfig;
class team;
typedef  std::vector<team>  t_teams;

namespace{
	static const config::t_token z_description("description", false);
	//	static const config::t_token z_advancement("advancement", false);
	static const config::t_token z_ellipse("ellipse", false);
	static const config::t_token z_image("image", false);
	static const config::t_token z_halo("halo", false);
	static const config::t_token z_usage("usage", false);
	//static const config::t_token z_("", false);
}

class unit_ability_list
{
public:
	unit_ability_list() :
		cfgs()
	{
	}

	bool empty() const;

	std::pair<int,map_location> highest(const config::t_token& key, int def=0) const;
	std::pair<int,map_location> lowest(const config::t_token& key, int def=0) const;

	std::vector<std::pair<const config *, map_location> > cfgs;
};


class unit
{
public:
	/**
	 * Clear the unit status cache for all units. Currently only the hidden
	 * status of units is cached this way.
	 */
	static void clear_status_caches();

	friend struct unit_movement_resetter;
	// Copy constructor
	unit(const unit& u);
	/** Initializes a unit from a config */
	unit(const config& cfg, bool use_traits = false, game_state *state = NULL);
	/**
	  * Initializes a unit from a unit type
	  * only real_unit may have random traits, name and gender
	  * (to prevent OOS caused by RNG calls)
	  */
	unit(const unit_type* t, int side, bool real_unit,
		unit_race::GENDER gender = unit_race::NUM_GENDERS);
	virtual ~unit();
	unit& operator=(const unit&);

	/** In case the unit carries EventWML, apply it */
	void set_game_context();

	/** Advances this unit to another type */
	void advance_to(const unit_type *t, bool use_traits = false,
		game_state *state = 0)
	{
		advance_to(cfg_, t, use_traits, state);
	}
	const std::vector<std::string>& advances_to() const { return advances_to_; }
	void set_advances_to(const std::vector<std::string>& advances_to);

	/** The type id of the unit */
	const config::t_token& type_id() const { return type_; }
	const unit_type* type() const;

	/** id assigned by wml */
	const config::t_token& id() const { if (id_.empty()){ return type_name_.token();} else return id_; }
	/** The unique internal ID of the unit */
	size_t underlying_id() const { return underlying_id_; }

	/** The unit type name */
	const t_string& type_name() const {return type_name_;}
	const config::t_token& undead_variation() const {return undead_variation_;}

	/** The unit name for display */
	const t_string &name() const { return name_; }
	void set_name(const t_string &name) { name_ = name; }
	void rename(const std::string& name) {if (!unrenamable_) name_= t_string(name);}

	/** The unit's profile */
	config::t_token const & small_profile() const;
	config::t_token const & big_profile() const;
	/** Information about the unit -- a detailed description of it */
	t_string unit_description() const { return cfg_[z_description]; }

	int hitpoints() const { return hit_points_; }
	int max_hitpoints() const { return max_hit_points_; }
	void set_hitpoints(int hp) { hit_points_ = hp; }
	int experience() const { return experience_; }
	int max_experience() const { return max_experience_; }
	void set_experience(int xp) { experience_ = xp; }
	int level() const { return level_; }
	void remove_movement_ai();
	void remove_attacks_ai();

	/** Colors for the unit's hitpoints. */
	SDL_Color hp_color() const;
	/** Colors for the unit's XP. */
	SDL_Color xp_color() const;
	/** Set to true for some scenario-specific units which should not be renamed */
	bool unrenamable() const { return unrenamable_; }
	int side() const { return side_; }
	std::string side_id() const;
	const config::t_token& team_color() const { return flag_rgb_; }
	unit_race::GENDER gender() const { return gender_; }
	void set_side(unsigned int new_side) { side_ = new_side; }
	fixed_t alpha() const { return alpha_; }

	bool can_recruit() const { return canrecruit_; }
	const std::vector<std::string>& recruits() const
		{ return recruit_list_; }
	void set_recruits(const std::vector<std::string>& recruits);
	const config& recall_filter() const { return cfg_.child("filter_recall"); }

	bool incapacitated() const { return get_state(STATE_PETRIFIED); }
	int total_movement() const { return max_movement_; }
	int movement_left() const { return (movement_ == 0 || incapacitated()) ? 0 : movement_; }
	void set_hold_position(bool value) { hold_position_ = value; }
	bool hold_position() const { return hold_position_; }
	void set_user_end_turn(bool value=true) { end_turn_ = value; }
	bool user_end_turn() const { return end_turn_; }
	int attacks_left() const { return (attacks_left_ == 0 || incapacitated()) ? 0 : attacks_left_; }
	int max_attacks() const { return max_attacks_; }
	void set_movement(int moves);
	void set_attacks(int left) { attacks_left_ = std::max<int>(0, left); }
	void unit_hold_position() { hold_position_ = end_turn_ = true; }
	void new_turn();
	void end_turn();
	void new_scenario();
	/** Called on every draw */
	void refresh();

	bool take_hit(int damage) { hit_points_ -= damage; return hit_points_ <= 0; }
	void heal(int amount);
	void heal_all() { hit_points_ = max_hitpoints(); }
	bool resting() const { return resting_; }
	void set_resting(bool rest) { resting_ = rest; }

	const std::map<config::t_token,config::t_token> get_states() const;
	bool get_state(const config::t_token& state) const;
	void set_state(const config::t_token &state, bool value);
	enum state_t { STATE_SLOWED = 0, STATE_POISONED, STATE_PETRIFIED,
		STATE_UNCOVERED, STATE_NOT_MOVED, STATE_UNHEALABLE, STATE_UNKNOWN = -1 };
	void set_state(state_t state, bool value);
	bool get_state(state_t state) const;
	static state_t get_known_boolean_state_id(const config::t_token &state);
	static std::map<config::t_token, state_t> get_known_boolean_state_names();

	bool has_moved() const { return movement_left() != total_movement(); }
	bool has_goto() const { return get_goto().valid(); }
	bool emits_zoc() const { return emit_zoc_ && !incapacitated();}
	bool matches_idt(const config::t_token& unit_id) const{	return id_ == unit_id; }
	bool matches_id(const std::string& unit_id) const {return matches_idt(config::t_token(unit_id));}
	/* cfg: standard unit filter */
	bool matches_filter(const vconfig& cfg,const map_location& loc,bool use_flat_tod=false
						, gamemap const & game_map = *resources::game_map
						, unit_map const & units =*resources::units
						, t_teams const & teams = *resources::teams
						, LuaKernel & lua_kernel = *resources::lua_kernel
						, tod_manager const & tod_manager = *resources::tod_manager) const;
	const std::vector<std::string>& overlays() const { return overlays_; }

	void write(config& cfg) const;

	void set_role(const config::t_token& role) { role_ = role; }
	void set_role(const std::string& role) { set_role(role);}
	const config::t_token &get_role() const { return role_; }

	void assign_ai_special(const config::t_token& s) { ai_special_ = s;}
            config::t_token get_ai_special() const { return(ai_special_); }
	const std::vector<attack_type>& attacks() const { return attacks_; }
	std::vector<attack_type>& attacks() { return attacks_; }

	int damage_from(const attack_type& attack,bool attacker,const map_location& loc) const { 
		return resistance_against(attack,attacker,loc); }

	/** A SDL surface, ready for display for place where we need a still-image of the unit. */
	const surface still_image(bool scaled = false) const;

	/** draw a unit.  */
	void redraw_unit();
	/** Clear unit_halo_  */
	void clear_haloes();

	void set_standing(bool with_bars = true);

	void set_ghosted(bool with_bars = true);
	void set_disabled_ghosted(bool with_bars = true);

	void set_idling();
	void set_selecting();
	unit_animation* get_animation() {  return anim_;};
	const unit_animation* get_animation() const {  return anim_;};
	void set_facing(map_location::DIRECTION dir);
	map_location::DIRECTION facing() const { return facing_; }

	bool invalidate(const map_location &loc);
	const std::vector<t_string>& trait_names() const { return trait_names_; }
	const std::vector<t_string>& trait_descriptions() const { return trait_descriptions_; }
	std::vector<config::t_token> get_traits_list() const;

	int cost () const { return unit_value_; }

	const map_location &get_location() const { return loc_; }
	/** To be called by unit_map or for temporary units only. */
	void set_location(const map_location &loc) { loc_ = loc; }

	const map_location& get_goto() const { return goto_; }
	void set_goto(const map_location& new_goto) { goto_ = new_goto; }

	int upkeep() const;
	bool loyal() const;

	void set_hidden(bool state);
	bool get_hidden() const { return hidden_; }
	bool is_flying() const { return flying_; }
	bool is_fearless() const { return is_fearless_; }
	bool is_healthy() const { return is_healthy_; }
	int movement_cost(const t_translation::t_terrain terrain, gamemap const & game_map = *resources::game_map) const;
	int defense_modifier(t_translation::t_terrain terrain, gamemap const & game_map = *resources::game_map ) const;
	int resistance_against(const std::string& damage_name,bool attacker,const map_location& loc) const;
	int resistance_against(const attack_type& damage_type,bool attacker,const map_location& loc) const {
		return resistance_against(damage_type.type(), attacker, loc);};

	//return resistances without any abilities applied
	utils::string_map get_base_resistances() const;
//		std::map<terrain_type::TERRAIN,int> movement_type() const;

	bool can_advance() const { return advances_to_.empty()==false || get_modification_advances().empty() == false; }
	bool advances() const { return experience_ >= max_experience() && can_advance(); }

    std::map<std::string,std::string> advancement_icons() const;
    std::vector<std::pair<std::string,std::string> > amla_icons() const;

	std::vector<config> get_modification_advances() const;
	config::const_child_itors modification_advancements() const
	{ return cfg_.child_range(z_advancement); }

	size_t modification_count(const config::t_token& type, const std::string& id) const;

	void add_modification(const config::t_token& type, const config& mod, bool no_add=false
						  , gamemap const & game_map = *resources::game_map
						  , unit_map const & units =*resources::units
						  , t_teams const & teams = *resources::teams
						  , LuaKernel & lua_kernel = *resources::lua_kernel
						  , tod_manager const & tod_manager = *resources::tod_manager);

	bool move_interrupted() const { return movement_left() > 0 && interrupted_move_.x >= 0 && interrupted_move_.y >= 0; }
	const map_location& get_interrupted_move() const { return interrupted_move_; }
	void set_interrupted_move(const map_location& interrupted_move) { interrupted_move_ = interrupted_move; }

	/** States for animation. */
	enum STATE {
		STATE_STANDING,   /** anim must fit in a hex */
		STATE_FORGET,     /** animation will be automatically replaced by a standing anim when finished */
		STATE_ANIM};      /** normal anims */
	void start_animation(int start_time, const unit_animation *animation,
		bool with_bars,  const std::string &text = "",
		Uint32 text_color = 0, STATE state = STATE_ANIM);

	/** The name of the file to game_display (used in menus). */
	config::t_token const & absolute_image() const { return cfg_[z_image]; }
	config::t_token const & image_halo() const { return cfg_[z_halo]; }

	config::t_token const & image_ellipse() const { return cfg_[z_ellipse]; }

	config &variables() { return variables_; }
	const config &variables() const { return variables_; }

	config::t_token usage() const { return cfg_[z_usage]; }
	unit_type::ALIGNMENT alignment() const { return alignment_; }
	const unit_race* race() const { return race_; }

	const unit_animation* choose_animation(const game_display& disp,
		       	const map_location& loc, const std::string& event,
		       	const map_location& second_loc = map_location::null_location,
			const int damage=0,
			const unit_animation::hit_type hit_type = unit_animation::INVALID,
			const attack_type* attack=NULL,const attack_type* second_attack = NULL,
			int swing_num =0) const;

	bool get_ability_bool(const config::t_token& ability, const map_location& loc 
						  , gamemap const & game_map = *resources::game_map
						  , unit_map const & units =*resources::units
						  , t_teams const & teams = *resources::teams
						  , LuaKernel & lua_kernel = *resources::lua_kernel
						  , tod_manager const & tod_manager = *resources::tod_manager) const;
	bool get_ability_bool(const config::t_token& ability 
						  , gamemap const & game_map = *resources::game_map
						  , unit_map const & units =*resources::units
						  , t_teams const & teams = *resources::teams
						  , LuaKernel & lua_kernel = *resources::lua_kernel
						  , tod_manager const & tod_manager = *resources::tod_manager) const { 
		return get_ability_bool(ability, loc_, game_map, units, teams, lua_kernel, tod_manager);}
	inline bool get_ability_bool(const std::string& ability, const map_location& loc ) const {
		return get_ability_bool(config::t_token(ability), loc); }
	inline bool get_ability_bool(const std::string& ability) const {
		return get_ability_bool(config::t_token(ability), loc_); }
	unit_ability_list get_abilities(const config::t_token &ability, const map_location& loc) const;
	unit_ability_list get_abilities(const config::t_token &ability) const {return get_abilities(ability, loc_); }
	unit_ability_list get_abilities(const std::string &ability, const map_location& loc) const {
		return get_abilities(config::t_token(ability), loc);}
	unit_ability_list get_abilities(const std::string &ability) const {return get_abilities(config::t_token(ability), loc_); }
	std::vector<std::string> ability_tooltips(bool force_active = false) const;
	std::vector<std::string> get_ability_list() const;
	bool has_ability_type(const config::t_token& ability) const;
	bool has_ability_type(const std::string& ability) const {return has_ability_type(config::t_token(ability));}

	const game_logic::map_formula_callable_ptr& formula_vars() const { return formula_vars_; }
	void add_formula_var(std::string str, variant var);
	bool has_formula() const { return !unit_formula_.empty(); }
	bool has_loop_formula() const { return !unit_loop_formula_.empty(); }
	bool has_priority_formula() const { return !unit_priority_formula_.empty(); }
	const std::string& get_formula() const { return unit_formula_; }
	const std::string& get_loop_formula() const { return unit_loop_formula_; }
	const std::string& get_priority_formula() const { return unit_priority_formula_; }

	void backup_state();
	void apply_modifications();
	void generate_traits(bool musthaveonly=false, game_state* state = 0);
	void generate_name(rand_rng::simple_rng *rng = 0);

	// Only see_all=true use caching
	bool invisible(const map_location& loc, bool see_all=true,unit_map const & units = *resources::units, t_teams const & teams = *resources::teams) const;

	/** Mark this unit as clone so it can be inserted to unit_map
	 * @returns                   self (for convenience)
	 **/
	unit& clone(bool is_temporary=true);

	std::string TC_image_mods() const;
	std::string image_mods() const;

	/**
	 * Gets the portrait for a unit.
	 *
	 * @param size                The size of the portrait.
	 * @param side                The side the portrait is shown on.
	 *
	 * @returns                   The portrait with the wanted size.
	 * @retval NULL               The wanted portrait doesn't exist.
	 */
	const tportrait* portrait(
		const unsigned size, const tportrait::tside side) const;

private:
	void advance_to(const config &old_cfg, const unit_type *t,
		bool use_traits, game_state *state);
	bool internal_matches_filter(const vconfig& cfg, const map_location& loc, bool use_flat_tod
						  , gamemap const & game_map = *resources::game_map
						  , unit_map const & units =*resources::units
						  , t_teams const & teams = *resources::teams
						  , LuaKernel & lua_kernel = *resources::lua_kernel
						  , tod_manager const & tod_manager = *resources::tod_manager) const;

	bool internal_matches_filter(const vconfig& cfg,const map_location& loc,
		bool use_flat_tod) const;
	/*
	 * cfg: an ability WML structure
	 */
	bool ability_active(const std::string& ability,const config& cfg,const map_location& loc
						  , gamemap const & game_map = *resources::game_map
						  , unit_map const & units =*resources::units
						  , t_teams const & teams = *resources::teams
						  , LuaKernel & lua_kernel = *resources::lua_kernel
						  , tod_manager const & tod_manager = *resources::tod_manager) const;

	bool ability_affects_adjacent(const std::string& ability,const config& cfg,int dir,const map_location& loc
								  , gamemap const & game_map = *resources::game_map
								  , unit_map const & units =*resources::units
								  , t_teams const & teams = *resources::teams
								  , LuaKernel & lua_kernel = *resources::lua_kernel
								  , tod_manager const & tod_manager = *resources::tod_manager) const;
	bool ability_affects_self(const std::string& ability,const config& cfg,const map_location& loc
								  , gamemap const & game_map = *resources::game_map
								  , unit_map const & units =*resources::units
								  , t_teams const & teams = *resources::teams
								  , LuaKernel & lua_kernel = *resources::lua_kernel
								  , tod_manager const & tod_manager = *resources::tod_manager) const;
	bool resistance_filter_matches(const config& cfg,bool attacker,const std::string& damage_name, int res) const;

	bool has_ability_by_id(const std::string& ability) const;
	void remove_ability_by_id(const std::string& ability);

	/** register a trait's name and its description for UI's use*/
	void add_trait_description(const config& trait, const t_string& description);

	void set_underlying_id();

	config cfg_;
	map_location loc_;

	std::vector<std::string> advances_to_;
	config::t_token type_;
	const unit_race* race_;
	config::t_token id_;
	t_string name_;
	size_t underlying_id_;
	t_string type_name_;
	config::t_token undead_variation_;
	config::t_token variation_;

	int hit_points_;
	int max_hit_points_;
	int experience_;
	int max_experience_;
	int level_;
	bool canrecruit_;
	std::vector<std::string> recruit_list_;
	unit_type::ALIGNMENT alignment_;
	config::t_token flag_rgb_;
	std::string image_mods_;

	bool unrenamable_;
	int side_;
	const unit_race::GENDER gender_;

	fixed_t alpha_;

	std::string unit_formula_;
	std::string unit_loop_formula_;
	std::string unit_priority_formula_;
	game_logic::map_formula_callable_ptr formula_vars_;

	int movement_;
	int max_movement_;
	mutable std::map<t_translation::t_terrain, int> movement_costs_; // movement cost cache
	mutable defense_cache defense_mods_; // defense modifiers cache
	bool hold_position_;
	bool end_turn_;
	bool resting_;
	int attacks_left_;
	int max_attacks_;

	std::set<config::t_token> states_;
	std::vector<bool> known_boolean_states_;
	static std::map<config::t_token, state_t> known_boolean_state_names_;
	config variables_;
	bool emit_zoc_;
	STATE state_;

	std::vector<std::string> overlays_;

	config::t_token role_;
	config::t_token ai_special_;
	std::vector<attack_type> attacks_;
	map_location::DIRECTION facing_;

	std::vector<t_string> trait_names_;
	std::vector<t_string> trait_descriptions_;

	int unit_value_;
	map_location goto_, interrupted_move_;

	bool flying_, is_fearless_, is_healthy_;

	utils::string_map modification_descriptions_;
	// Animations:
	std::vector<unit_animation> animations_;

	unit_animation *anim_;
	int next_idling_;
	int frame_begin_time_;


	int unit_halo_;
	bool getsHit_;
	bool refreshing_; // avoid infinite recursion
	bool hidden_;
	bool draw_bars_;

	config modifications_;

	friend void attack_type::set_specials_context(const map_location& loc, const map_location&, const unit& un, bool) const;

	/** Hold the visibility status cache for a unit, mutable since it's a cache. */
	mutable boost::unordered_map<map_location, bool> invisibility_cache_;

	/**
	 * Clears the cache.
	 *
	 * Since we don't change the state of the object we're marked const (also
	 * required since the objects in the cache need to be marked const).
	 */
	void clear_visibility_cache() const { invisibility_cache_.clear(); }
};

/** Object which temporarily resets a unit's movement */
struct unit_movement_resetter
{
	unit_movement_resetter(unit& u, bool operate=true);
	~unit_movement_resetter();

private:
	unit& u_;
	int moves_;
};

/** Returns the number of units of the side @a side_num. */
int side_units(int side_num, unit_map const & units = *resources::units) ;

/** Returns the total cost of units of side @a side_num. */
int side_units_cost(int side_num, unit_map const & units = *resources::units);

int side_upkeep(int side_num, unit_map const & units = *resources::units);

//The ackward orderding here allows the linker to disambiguate the return type using the passed in unit_map
unit_map::const_iterator find_visible_unit(unit_map const & units, const map_location &loc, const team &current_team, bool const see_all = false, gamemap const & map = *resources::game_map);
unit_map::iterator find_visible_unit(unit_map & units, const map_location &loc, const team &current_team, bool const see_all = false, gamemap const & map = *resources::game_map);

inline unit_map::iterator find_visible_unit(const map_location &loc, const team &current_team, bool const see_all = false) {
	return find_visible_unit( *resources::units, loc, current_team, see_all, *resources::game_map); }

unit *get_visible_unit(const map_location &loc, const team &current_team, bool const see_all = false, gamemap const & map = *resources::game_map, unit_map & units = *resources::units);

struct team_data
{
	team_data() :
		units(0),
		upkeep(0),
		villages(0),
		expenses(0),
		net_income(0),
		gold(0),
		teamname()
	{
	}

	int units, upkeep, villages, expenses, net_income, gold;
	std::string teamname;
};

team_data calculate_team_data(const class team& tm, int side);

/**
 * This object is used to temporary place a unit in the unit map, swapping out
 * any unit that is already there.  On destruction, it restores the unit map to
 * its original.
 */
struct temporary_unit_placer
{
	temporary_unit_placer(unit_map& m, const map_location& loc, unit& u);
	virtual  ~temporary_unit_placer();

private:
	unit_map& m_;
	const map_location loc_;
	unit *temp_;
};

/**
 * This object is used to temporary move a unit in the unit map, swapping out
 * any unit that is already there.  On destruction, it restores the unit map to
 * its original.
 */
struct temporary_unit_mover
{
	temporary_unit_mover(unit_map& m, const map_location& src,  const map_location& dst);
	virtual  ~temporary_unit_mover();

private:
	unit_map& m_;
	const map_location src_;
	const map_location dst_;
	unit *temp_;
};

/**
 * Gets a checksum for a unit.
 *
 * In MP games the descriptions are locally generated and might differ, so it
 * should be possible to discard them.  Not sure whether replays suffer the
 * same problem.
 *
 *  @param u                    the unit
 *
 *  @returns                    the checksum for a unit
 */
std::string get_checksum(const unit& u);

#endif
