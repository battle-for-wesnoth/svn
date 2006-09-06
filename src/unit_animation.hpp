/* $Id: unit_types.hpp 9700 2006-01-15 12:00:53Z noyga $ */
/*
   Copyright (C) 2006 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
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



class unit_animation:public animated<unit_frame>
{
	public:
		static config prepare_animation(const config &cfg,const std::string animation_tag);

		unit_animation(){};
		explicit unit_animation(const config& cfg,const std::string frame_string ="frame");
		explicit unit_animation(const unit_frame &frame);
		int matches(const std::string &terrain,const gamemap::location::DIRECTION dir) const;

		enum FRAME_DIRECTION { VERTICAL, DIAGONAL };

	private:
		std::vector<std::string> terrain_types;
		std::vector<gamemap::location::DIRECTION> directions;
};

class attack_type;


class fighting_animation:public unit_animation
{
	public:
		typedef enum { HIT, MISS, KILL} hit_type;

		explicit fighting_animation(const config& cfg);
		explicit fighting_animation(const unit_frame &frame, const std::string &range=""):
			unit_animation(frame),range(utils::split(range)) {};
		int matches(const std::string &terrain,gamemap::location::DIRECTION dir,hit_type hit,const attack_type* attack,int swing_num) const;

	private:
		std::vector<hit_type> hits;
		std::vector<int> swing_num;
		std::vector<std::string> range;
		std::vector<std::string> damage_type, special;
};

class defensive_animation:public fighting_animation
{
	public:
		explicit defensive_animation(const config& cfg):fighting_animation(cfg){};
		explicit defensive_animation(const unit_frame &frame, const std::string &range=""):fighting_animation(frame,range){};
};


class death_animation:public fighting_animation
{
	public:
		explicit death_animation(const config& cfg):fighting_animation(cfg){};
		explicit death_animation(const unit_frame &frame,const std::string &range=""):fighting_animation(frame,range) {};
	private:
};

class attack_animation: public fighting_animation
{
	public:
		explicit attack_animation(const config& cfg):fighting_animation(cfg),missile_anim(cfg,"missile_frame"){};
		explicit attack_animation(const unit_frame &frame):fighting_animation(frame) {};
		const unit_animation &get_missile_anim() {return missile_anim;}
	private:
		unit_animation missile_anim;

};

class movement_animation:public unit_animation
{
	public:
		explicit movement_animation(const config& cfg):unit_animation(cfg){};
		explicit movement_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};

class standing_animation:public unit_animation
{
	public:
		explicit standing_animation(const config& cfg):unit_animation(cfg){};
		explicit standing_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};

class leading_animation:public unit_animation
{
	public:
		explicit leading_animation(const config& cfg):unit_animation(cfg){};
		explicit leading_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};

class healing_animation:public unit_animation
{
	public:
		explicit healing_animation(const config& cfg):unit_animation(cfg){};
		explicit healing_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};

class recruit_animation:public unit_animation
{
	public:
		explicit recruit_animation(const config& cfg):unit_animation(cfg){};
		explicit recruit_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};

class idle_animation:public unit_animation
{
	public:
		explicit idle_animation(const config& cfg):unit_animation(cfg){};
		explicit idle_animation(const unit_frame &frame):
			unit_animation(frame){};

	private:
};


#endif
