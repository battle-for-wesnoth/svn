/* $Id$ */
/*
   Copyright (C) 2006 - 2007 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
   */
#ifndef UNIT_ANIMATION_H_INCLUDED
#define UNIT_ANIMATION_H_INCLUDED

#include "animated.hpp"
#include "map.hpp"
#include "config.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"

#include <string>
#include <vector>

#include "unit_frame.hpp"


class game_display;
class attack_type;

class unit_animation:public animated<unit_frame>
{
	public:
		typedef enum { MATCH_FAIL=-2 , DEFAULT_ANIM=-1};
		typedef enum { HIT, MISS, KILL, INVALID} hit_type;
		static config prepare_animation(const config &cfg,const std::string animation_tag);

		unit_animation(){};
		explicit unit_animation(const config& cfg,const std::string frame_string ="frame");
		explicit unit_animation(int start_time,const unit_frame &frame,const std::string& even="",const int variation=0);
		int matches(const game_display &disp,const gamemap::location& loc,const unit* my_unit,const std::string & event="",const int value=0,hit_type hit=INVALID,const attack_type* attack=NULL,const attack_type* second_attack = NULL, int swing_num =0) const;

	private:
		t_translation::t_list terrain_types_;
		std::vector<config> unit_filter_;
		std::vector<config> secondary_unit_filter_;
		std::vector<gamemap::location::DIRECTION> directions_;
		int frequency_;
		int base_score_;
		std::vector<std::string> event_;
		std::vector<int> value_;
		std::vector<config> primary_attack_filter_;
		std::vector<config> secondary_attack_filter_;
		std::vector<hit_type> hits_;
		std::vector<int> swing_num_;
};




class defensive_animation:public unit_animation
{
	public:
		explicit defensive_animation(const config& cfg):unit_animation(cfg){};
		explicit defensive_animation(int start_time,const unit_frame &frame):unit_animation(start_time,frame){};
};


class death_animation:public unit_animation
{
	public:
		explicit death_animation(const config& cfg):unit_animation(cfg){};
		explicit death_animation(int start_time,const unit_frame &frame):unit_animation(start_time,frame) {};
	private:
};

class attack_animation: public unit_animation
{
	public:
		explicit attack_animation(const config& cfg):unit_animation(cfg),missile_anim(cfg,"missile_frame"){};
		explicit attack_animation(int start_time,const unit_frame &frame):unit_animation(start_time,frame) {};
		const unit_animation &get_missile_anim() {return missile_anim;}
	private:
		unit_animation missile_anim;

};

class victory_animation:public unit_animation
{
	public:
		explicit victory_animation(const config& cfg):unit_animation(cfg){};
		explicit victory_animation(int start_time,const unit_frame &frame):
			unit_animation(start_time,frame){};

	private:
};

#endif
