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
#ifndef UNIT_TYPES_H_INCLUDED
#define UNIT_TYPES_H_INCLUDED

#include "config.hpp"
#include "map.hpp"
#include "race.hpp"
#include "team.hpp"

#include <string>
#include <vector>

//a class to describe a unit's animation sequence
class unit_animation
{
public:
	unit_animation(const config& cfg);

	enum FRAME_TYPE { UNIT_FRAME, MISSILE_FRAME };
	enum FRAME_DIRECTION { VERTICAL, DIAGONAL };

	int get_first_frame(FRAME_TYPE type=UNIT_FRAME) const;
	int get_last_frame(FRAME_TYPE type=UNIT_FRAME) const;

	//function which gets an attack animation frame. The argument
	//is 0 for the frame at the time of impact, and negative for
	//frames before the time of impact
	const std::string* get_frame(int milliseconds, int* xoffset=NULL,
	                             FRAME_TYPE type=UNIT_FRAME,
								 FRAME_DIRECTION direction=VERTICAL,
								 const std::string** halo=NULL,
								 int* halo_x=NULL, int* halo_y=NULL) const;

	struct sfx {
		int time;
		std::string on_hit, on_miss;
	};

	const std::vector<sfx>& sound_effects() const;

private:
	struct frame {
		frame(int i1, int i2, const std::string& img, const std::string& halo, int offset, int halo_x, int halo_y)
		      : start(i1), end(i2), xoffset(offset), image(img), halo(halo), halo_x(halo_x), halo_y(halo_y)
		{}

		frame(int i1, int i2, const std::string& img, const std::string& diag,
		      const std::string& halo, int offset, int halo_x, int halo_y)
		      : start(i1), end(i2), xoffset(offset),
		        image(img), image_diagonal(diag), halo(halo), halo_x(halo_x), halo_y(halo_y)
		{}

		int start, end;
		int xoffset;
		std::string image;
		std::string image_diagonal;
		std::string halo;
		int halo_x, halo_y;
	};

	std::vector<frame> frames_[2];

	std::vector<sfx> sfx_;
};

//the 'attack type' is the type of attack, how many times it strikes,
//and how much damage it does.
class attack_type
{
public:
	enum RANGE { SHORT_RANGE, LONG_RANGE };

	attack_type(const config& cfg);
	const std::string& name() const;
	const std::string& type() const;
	const std::string& special() const;
	const std::string& icon() const;
	RANGE range() const;
	int hexes() const;
	int damage() const;
	int num_attacks() const;
	double attack_weight() const;
	double defense_weight() const;

	bool backstab() const;

	const unit_animation& animation() const { return animation_; }

	bool matches_filter(const config& cfg) const;
	bool apply_modification(const config& cfg,std::string* description);
private:
	unit_animation animation_;
	std::string name_;
	std::string type_;
	std::string special_;
	std::string icon_;
	RANGE range_;
	int hexes_;
	int damage_;
	int num_attacks_;
	double attack_weight_;
	double defense_weight_;

	//caches whether the unit can backstab. This is important
	//because the AI queries it alot.
	bool backstab_;
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

	const std::string& name() const;
	int movement_cost(const gamemap& map, gamemap::TERRAIN terrain, int recurse_count=0) const;
	int defense_modifier(const gamemap& map, gamemap::TERRAIN terrain, int recurse_count=0) const;
	int damage_against(const attack_type& attack) const;
	int resistance_against(const attack_type& attack) const;

	string_map damage_table() const;

	void set_parent(const unit_movement_type* parent);

	bool is_flying() const;

private:
	const config& cfg_;

	mutable std::map<gamemap::TERRAIN,int> moveCosts_;
	mutable std::map<gamemap::TERRAIN,int> defenseMods_;

	const unit_movement_type* parent_;
};

typedef std::map<std::string,unit_movement_type> movement_type_map;

class unit_type
{
public:
	//this class assumes that the passed in references will remain valid
	//for at least as long as the class instance
	unit_type(const config& cfg, const movement_type_map& movement_types,
	          const race_map& races, const std::vector<config*>& traits);

	int num_traits() const;

	std::string generate_description() const;

	//the name of the unit in the current language setting
	std::string language_name() const;

	//unique identifier that doesn't have any whitespace
	std::string id() const;
	const std::string& name() const;
	const std::string& image() const;
	const std::string& image_halo() const;
	const std::string& image_moving() const;
	const std::string& image_profile() const;
	const std::string& image_fighting(attack_type::RANGE range) const;
	const std::string& image_defensive(attack_type::RANGE range) const;
	const std::string& image_leading() const;
	const std::string& image_healing() const;
	const std::string& image_halo_healing() const;
	const std::string& unit_description() const;
	const std::string& get_hit_sound() const;
	const std::string& die_sound() const;
	int hitpoints() const;
	std::vector<attack_type> attacks() const;
	const unit_movement_type& movement_type() const;

	int experience_needed() const;
	std::vector<std::string> advances_to() const;
	const config::child_list& modification_advancements() const { return cfg_.get_children("advancement"); }
	const std::string& usage() const;

	struct experience_accelerator {
		experience_accelerator(int modifier);
		~experience_accelerator();
	private:
		int old_value_;
	};

	int level() const;
	int movement() const;
	int cost() const;

	enum ALIGNMENT { LAWFUL, NEUTRAL, CHAOTIC };

	ALIGNMENT alignment() const;
	static const std::string& alignment_description(ALIGNMENT align);

	double alpha() const;

	const std::vector<std::string>& abilities() const;

	//max_unit_healing returns the maximum hitpoints a unit next to this
	//unit can heal per turn. heals returns the total amount of hitpoints
	//this unit can heal out of all adjacent units
	int max_unit_healing() const;
	int heals() const;
	bool regenerates() const;
	bool is_leader() const;
	bool illuminates() const;
	bool is_skirmisher() const;
	bool teleports() const;
	bool nightvision() const;
	bool not_living() const;
	bool can_advance() const;

	bool has_ability(const std::string& ability) const;

	const std::vector<config*>& possible_traits() const;

	unit_race::GENDER gender() const;

	const std::string& race() const;

	const unit_animation* defend_animation(bool hits, attack_type::RANGE range) const;

private:
	const config& cfg_;

	const unit_race* race_;

	double alpha_;

	std::vector<std::string> abilities_;

	int max_heals_;
	int heals_;
	bool regenerates_;
	bool leadership_;
	bool illuminates_;
	bool skirmish_;
	bool teleport_;
	bool nightvision_;
	bool can_advance_;

	unit_movement_type movementType_;

	const std::vector<config*>& possibleTraits_;

	unit_race::GENDER gender_;

	struct defensive_animation
	{
		defensive_animation(const config& cfg);
		bool matches(bool hits, attack_type::RANGE range) const;

		enum { HIT, MISS, HIT_OR_MISS } hits;
		enum { SHORT, LONG, SHORT_OR_LONG } range;
		unit_animation animation;
	};

	std::vector<defensive_animation> defensive_animations_;
};

struct game_data
{
	game_data(const config& cfg);
	movement_type_map movement_types;
	typedef std::map<std::string,unit_type> unit_type_map;
	unit_type_map unit_types;
	race_map races;
};

#endif
