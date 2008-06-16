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

//! @file gamestatus.cpp
//! Maintain status of a game, load&save games.

#include "global.hpp"

#include "time_of_day.hpp"



time_of_day::time_of_day(const config& cfg)
                 : lawful_bonus(atoi(cfg["lawful_bonus"].c_str())),
                   bonus_modified(0),
                   image(cfg["image"]), name(cfg["name"]), id(cfg["id"]),
			       image_mask(cfg["mask"]),
                   red(atoi(cfg["red"].c_str())),
                   green(atoi(cfg["green"].c_str())),
                   blue(atoi(cfg["blue"].c_str())),
		   sounds(cfg["sound"])
{
}

time_of_day::time_of_day()
: lawful_bonus(0)
, bonus_modified(0)
, name("NULL_TOD")
, id("nulltod")
, red(0)
, green(0)
, blue(0)
{
}

void time_of_day::write(config& cfg) const
{
	char buf[50];
	snprintf(buf,sizeof(buf),"%d",lawful_bonus);
	cfg["lawful_bonus"] = buf;

	snprintf(buf,sizeof(buf),"%d",red);
	cfg["red"] = buf;

	snprintf(buf,sizeof(buf),"%d",green);
	cfg["green"] = buf;

	snprintf(buf,sizeof(buf),"%d",blue);
	cfg["blue"] = buf;

	cfg["image"] = image;
	cfg["name"] = name;
	cfg["id"] = id;
	cfg["mask"] = image_mask;
}

void time_of_day::parse_times(const config& cfg, std::vector<time_of_day>& normal_times)
{
	const config::child_list& times = cfg.get_children("time");
	config::child_list::const_iterator t;
	for(t = times.begin(); t != times.end(); ++t) {
		normal_times.push_back(time_of_day(**t));
	}

	if(normal_times.empty())
	{
		// Make sure we have at least default time
		config dummy_cfg;
		normal_times.push_back(time_of_day(dummy_cfg));
	}
}

