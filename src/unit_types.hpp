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
#ifndef UNIT_TYPES_H_INCLUDED
#define UNIT_TYPES_H_INCLUDED

#include "unit_animation.hpp"
#include "config.hpp"
#include "map.hpp"
#include "race.hpp"
#include "util.hpp"

#include <string>
#include <vector>


class unit_type;
class unit;
class unit_ability_list;
class unit_type_data;
class gamestatus;
class team;


//and how much damage it does.
class attack_type
{
public:

	attack_type(const config& cfg);
	const t_string& name() const { return description_; }
	const std::string& id() const { return id_; }
	const std::string& type() const { return type_; }
	const std::string& icon() const { return icon_; }
	const std::string& range() const { return range_; }
	std::string accuracy_parry_description() const;
	int accuracy() const { return accuracy_; }
	int parry() const { return parry_; }
	int damage() const { return damage_; }
	int num_attacks() const { return num_attacks_; }
	double attack_weight() const { return attack_weight_; }
	double defense_weight() const { return defense_weight_; }

	bool get_special_bool(const std::string& special,bool force=false) const;
	unit_ability_list get_specials(const std::string& special) const;
	std::vector<std::string> special_tooltips(bool force=false) const;
	std::string weapon_specials(bool force=false) const;
	void set_specials_context(const gamemap::location& aloc,const gamemap::location& dloc,
                              const unit_map* unitmap,
							  const gamemap* map, const gamestatus* game_status,
							  const std::vector<team>* teams,bool attacker,const attack_type* other_attack) const;
	void set_specials_context(const gamemap::location& loc,const gamemap::location& dloc, const unit& un, bool attacker =true) const;

	bool has_special_by_id(const std::string& special) const;
	//this function returns a random animation out of the possible
	//animations for this attack. It will not return the same attack
	//each time.
	bool matches_filter(const config& cfg,bool self=false) const;
	bool apply_modification(const config& cfg,std::string* description);
	bool describe_modification(const config& cfg,std::string* description);

	int movement_used() const { return cfg_["movement_used"] == "" ? 100000 : lexical_cast_default<int>(cfg_["movement_used"]); }

	config& get_cfg() { return cfg_; }
	const config& get_cfg() const { return cfg_; }
	mutable gamemap::location aloc_,dloc_;
	mutable bool attacker_;
	mutable const unit_map* unitmap_;
	mutable const gamemap* map_;
	mutable const gamestatus* game_status_;
	mutable const std::vector<team>* teams_;
	mutable const attack_type* other_attack_;
	/*
	 * cfg: a weapon special WML structure
	 */
	bool special_active(const config& cfg,bool self,bool report=false) const;
	bool special_affects_opponent(const config& cfg) const;
	bool special_affects_self(const config& cfg) const;

	config cfg_;
	const unit_animation* animation(const game_display& disp, const gamemap::location& loc,const unit* my_unit,const unit_animation::hit_type hit,const attack_type* secondary_attack,int swing_num,int damage) const;
	// made public to ease backward compatibility for WML syntax
	// to be removed (with all corresponding code once 1.3.6 is reached
	std::vector<unit_animation> animation_;
private:
	t_string description_;
	std::string id_;
	std::string type_;
	std::string icon_;
	std::string range_;
	int damage_;
	int num_attacks_;
	double attack_weight_;
	double defense_weight_;

	int accuracy_;
	int parry_;
};

class unit_movement_type;

//the 'unit movement type' is the basic size of the unit - flying, small land,
//large land, etc etc.
class unit_movement_type
{
public:
	//this class assumes that the passed in reference will remain valid
	//for at least as long as the class instance
	unit_movement_type(const config& cfg, const unit_movement_type* parent=NULL);
	unit_movement_type();

	const t_string& name() const;
	int movement_cost(const gamemap& map, t_translation::t_terrain terrain, int recurse_count=0) const;
	int defense_modifier(const gamemap& map, t_translation::t_terrain terrain, int recurse_count=0) const;
	int damage_against(const attack_type& attack) const { return resistance_against(attack); }
	int resistance_against(const attack_type& attack) const;

	string_map damage_table() const;

	void set_parent(const unit_movement_type* parent) { parent_ = parent; }

	bool is_flying() const;
	const std::map<t_translation::t_terrain, int>& movement_costs() const { return moveCosts_; }
	const std::map<t_translation::t_terrain, int>& defense_mods() const { return defenseMods_; }

	const config& get_cfg() const { return cfg_; }
	const unit_movement_type* get_parent() const { return parent_; }
private:
	mutable std::map<t_translation::t_terrain, int> moveCosts_;
	mutable std::map<t_translation::t_terrain, int> defenseMods_;

	const unit_movement_type* parent_;

	config cfg_;
};

typedef std::map<std::string,unit_movement_type> movement_type_map;

class unit_type
{
public:
	friend class unit;
	friend class unit_type_data;

	unit_type();
	unit_type(const config& cfg, const movement_type_map& movement_types,
	          const race_map& races, const std::vector<config*>& traits);
	unit_type(const unit_type& o);

	~unit_type();

	/** Load data into an empty unit_type */
	void build_full(const config& cfg, const movement_type_map& movement_types,
	          const race_map& races, const std::vector<config*>& traits);
	/** Partially load data into an empty unit_type */
    void build_help_index(const config& cfg, const race_map& races);

	/**
	 * Adds an additional advancement path to a unit type.
	 * This is used to implement the [advancefrom] tag.
	 */
	void add_advancement(const unit_type &advance_to,int experience);

	/** Adds units that this unit advances from, for help file purposes. */
	void add_advancesfrom(const std::string& unit_id);

	const unit_type& get_gender_unit_type(unit_race::GENDER gender) const;
	const unit_type& get_variation(const std::string& name) const;
	/** Info on the type of unit that the unit reanimates as. */
	const std::string& undead_variation() const { return undead_variation_; }

	unsigned int num_traits() const { return (num_traits_ ? num_traits_ : race_->num_traits()); }

	/** The name of the unit in the current language setting. */
	const t_string& type_name() const { return type_name_; }

	const std::string& id() const { return id_; }
	const t_string& unit_description() const;
	int hitpoints() const { return hitpoints_; }
	int level() const { return level_; }
	int movement() const { return movement_; }
	int max_attacks() const { return max_attacks_; }
	int cost() const { return cost_; }
	const std::string& usage() const { return usage_; }
	const std::string& image() const { return image_; }
	const std::string& image_profile() const;

	const std::vector<unit_animation>& animations() const;

	const std::string& flag_rgb() const { return flag_rgb_; }

	std::vector<attack_type> attacks() const;
	const unit_movement_type& movement_type() const { return movementType_; }

	int experience_needed(bool with_acceleration=true) const;
	std::vector<std::string> advances_to() const { return advances_to_; }
	std::vector<std::string> advances_from() const { return advances_from_; }
	const config::child_list& modification_advancements() const { return cfg_.get_children("advancement"); }

	struct experience_accelerator {
		experience_accelerator(int modifier);
		~experience_accelerator();
		static int get_acceleration();
	private:
		int old_value_;
	};

	enum ALIGNMENT { LAWFUL, NEUTRAL, CHAOTIC };

	ALIGNMENT alignment() const { return alignment_; }
	static const char* alignment_description(ALIGNMENT align);
	static const char* alignment_id(ALIGNMENT align);

	fixed_t alpha() const { return alpha_; }

	const std::vector<std::string>& abilities() const { return abilities_; }
	const std::vector<std::string>& ability_tooltips() const { return ability_tooltips_; }

	bool can_advance() const { return !advances_to_.empty(); }

        bool not_living() const;

	bool has_zoc() const { return zoc_; }

	bool has_ability(const std::string& ability) const;
	bool has_ability_by_id(const std::string& ability) const;

	const std::vector<config*> possible_traits() const { return possibleTraits_.get_children("trait"); }
	bool has_random_traits() const { return (num_traits() > 0 && possible_traits().size() > 1); }

	const std::vector<unit_race::GENDER>& genders() const { return genders_; }

	const std::string& race() const;
	bool hide_help() const { return hide_help_; }

    enum BUILD_STATUS {NOT_BUILT, HELP_INDEX, WITHOUT_ANIMATIONS, FULL};

    BUILD_STATUS build_status() const { return build_status_; }
private:
	void operator=(const unit_type& o);

	config cfg_;

	std::string id_;
    t_string type_name_;
    t_string description_;
    int hitpoints_;
    int level_;
    int movement_;
    int max_attacks_;
    int cost_;
    std::string usage_;
    std::string undead_variation_;

    std::string image_;
    std::string image_profile_;
	std::string flag_rgb_;

    unsigned int num_traits_;

	unit_type* gender_types_[2];

	typedef std::map<std::string,unit_type*> variations_map;
	variations_map variations_;

	const unit_race* race_;

	fixed_t alpha_;

	std::vector<std::string> abilities_;
	std::vector<std::string> ability_tooltips_;

	bool zoc_, hide_help_;

	std::vector<std::string> advances_to_;
	std::vector<std::string> advances_from_;
	int experience_needed_;


	ALIGNMENT alignment_;

	unit_movement_type movementType_;

	config possibleTraits_;

	std::vector<unit_race::GENDER> genders_;

	// animations are loaded only after the first animations() call
	mutable std::vector<unit_animation> animations_;

	BUILD_STATUS build_status_;
};

class unit_type_data
{
public:
    static unit_type_data& instance() {
        if (instance_ == NULL)
            instance_ = new unit_type_data();

        return *instance_;
    }

    typedef std::map<std::string,unit_type> unit_type_map;

	class unit_type_map_wrapper
	{
        friend class unit_type_data;

		public:
            const race_map& races() const { return races_; }
            void set_config(const config& cfg);

			unit_type_map::const_iterator begin() const { return types_.begin(); }
			unit_type_map::const_iterator end() const { return types_.end(); }
            unit_type_map::const_iterator find(const std::string& key, unit_type::BUILD_STATUS status = unit_type::FULL) const;

            void build_all(unit_type::BUILD_STATUS status) const;

        private:
            unit_type_map_wrapper();
            unit_type_map_wrapper(unit_type_map_wrapper& /*utf*/) :
				types_(),
				dummy_unit_map_(),
				movement_types_(),
				races_(), 
				unit_traits_(),
				unit_cfg_(0)
			{}

            void set_unit_config(const config& unit_cfg) { unit_cfg_ = &unit_cfg; }
            void set_unit_traits(const config::child_list unit_traits) { unit_traits_ = unit_traits; }

			const config& find_config(const std::string& key) const;
			std::pair<unit_type_map::iterator, bool> insert(const std::pair<std::string,unit_type>& utype) { return types_.insert(utype); }
			void clear() {
			    types_.clear();
			    movement_types_.clear();
			    races_.clear();
            }

            unit_type& build_unit_type(const std::string& key, unit_type::BUILD_STATUS status) const;
            void add_advancefrom(const config& unit_cfg) const;
            void add_advancement(const config& cfg, unit_type& to_unit) const;

            mutable unit_type_map types_;
            mutable unit_type_map dummy_unit_map_;
            movement_type_map movement_types_;
            race_map races_;
            config::child_list unit_traits_;
            const config* unit_cfg_;
	};

    static unit_type_map_wrapper& types() { return instance().unit_types_; }

private:
    static unit_type_data* instance_;
    mutable unit_type_map_wrapper unit_types_;

    unit_type_data();
};

#endif
