
/* $Id$ */
/*
   Copyright (C) 2006 by Dominic Bolin <dominic.bolin@exong.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


#ifndef UNIT_ABILITIES_HPP_INCLUDED
#define UNIT_ABILITIES_HPP_INCLUDED




namespace unit_abilities
{


enum value_modifier {NOT_USED,SET,ADD,MUL};

struct individual_effect
{
	individual_effect(value_modifier t,int val,config* abil,const gamemap::location& l);
	void set(value_modifier t,int val,config* abil,const gamemap::location& l);
	value_modifier type;
	int value;
	config* ability;
	gamemap::location loc;
};

typedef std::vector<individual_effect> effect_list;

class effect
{
	public:
		effect(const unit_ability_list& list, int def, bool backstab);
		
		int get_composite_value() const;
		
		effect_list::const_iterator begin() const;
		effect_list::const_iterator end() const;
	private:
		effect_list effect_list_;
		int composite_value_;
};













}



#endif




