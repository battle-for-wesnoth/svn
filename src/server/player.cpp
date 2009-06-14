/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "../global.hpp"

#include "player.hpp"

wesnothd::player::player(const std::string& n, simple_wml::node& cfg,
                         bool registered, const size_t max_messages,
                         const size_t time_period, const bool sp)
  : name_(n)
  , cfg_(cfg)
  , selective_ping_(sp)
  , registered_(false)
  , flood_start_(0)
  , messages_since_flood_start_(0)
  , MaxMessages(max_messages)
  , TimePeriod(time_period)
{
	cfg_.set_attr_dup("name", n.c_str());
	mark_available();
	mark_registered(registered);
}

// keep 'available' and game name ('location') for backward compatibility
void wesnothd::player::mark_available(const int game_id,
                                      const std::string location)
{
	cfg_.set_attr("available", (game_id == 0) ? "yes" : "no");
	cfg_.set_attr_dup("game_id", lexical_cast<std::string>(game_id).c_str());
	cfg_.set_attr_dup("location", location.c_str());
}

void wesnothd::player::mark_registered(bool registered)
{
    cfg_.set_attr("registered", registered ? "yes" : "no");
    registered_ = registered;
}

bool wesnothd::player::is_message_flooding()
{
	const time_t now = time(NULL);
	if (flood_start_ == 0) {
		flood_start_ = now;
		return false;
	}

	++messages_since_flood_start_;

	if (now - flood_start_ > TimePeriod) {
		messages_since_flood_start_ = 0;
		flood_start_ = now;
	} else if (messages_since_flood_start_ == MaxMessages) {
		return true;
	}
	return false;
}
