/*
   Copyright (C) 2009 - 2011 by Eugen Jiresch
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
 */

#include "foreach.hpp"
#include "tod_manager.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "gamestatus.hpp"
#include "log.hpp"
#include "map.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "unit.hpp"
#include "unit_abilities.hpp"

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)

tod_manager::tod_manager(const config& time_cfg, int num_turns, game_state* state):
	savegame_config(),
	currentTime_(0),
	times_(),
	areas_(),
	turn_(1),
	num_turns_(num_turns)
{
	std::string turn_at = time_cfg["turn_at"];
	if (state)
	{
		turn_at = utils::interpolate_variables_into_string(turn_at, *state);

	}

	if(turn_at.empty() == false) {
		turn_ = atoi(turn_at.c_str());
	}

	time_of_day::parse_times(time_cfg,times_);

	set_start_ToD(const_cast<config&>(time_cfg), turn_);
}

tod_manager& tod_manager::operator=(const tod_manager& manager)
{
	if(this == &manager) {
		return *this;
	}

	currentTime_ = manager.currentTime_;
	times_ = manager.times_;
	areas_ = manager.areas_;

	turn_ = manager.turn_;
	num_turns_ = manager.num_turns_;

	return *this;
}

config tod_manager::to_config() const
{
	config cfg;
	cfg["turn_at"] = turn_;
	cfg["turns"] = num_turns_;
	cfg["current_tod"] = currentTime_;

	std::vector<time_of_day>::const_iterator t;
	for(t = times_.begin(); t != times_.end(); ++t) {
		t->write(cfg.add_child("time"));
	}
	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		config& area = cfg.add_child("time_area");
		// if no ranges, then use hexes to generate ranges
		if(i->xsrc.empty() && i->ysrc.empty()) {
			write_location_range(i->hexes, area);
		} else {
			area["x"] = i->xsrc;
			area["y"] = i->ysrc;
		}
		for(t = i->times.begin(); t != i->times.end(); ++t) {
			t->write(area.add_child("time"));
		}
		area["current_time"] = i->currentTime;
	}
	return cfg;
}

const time_of_day& tod_manager::get_time_of_day() const
{
	return times_[currentTime_];
}

const time_of_day& tod_manager::get_previous_time_of_day() const
{
	return get_time_of_day_turn(times_, turn_ - 1, currentTime_);
}


const time_of_day& tod_manager::get_time_of_day_of_next_turn(const int side, const map_location &loc) const
{
	int nturn = turn_;
	if (resources::controller->current_side() < side)
	{
		//that side went before current side, its next turn will be on next game turn
		nturn++;
	}
	if (loc != map_location::null_location)
	{
		return get_time_of_day(loc, nturn);
	}
	return get_time_of_day_turn(times_, nturn, currentTime_);
}

const time_of_day& tod_manager::get_time_of_day(const map_location& loc, int n_turn) const
{
	if(n_turn == 0)
		n_turn = turn_;

	for (std::vector<area_time_of_day>::const_reverse_iterator
		 i = areas_.rbegin(), i_end = areas_.rend(); i != i_end; ++i)
	{
		if (i->hexes.count(loc) != 1) continue;
		VALIDATE(i->times.size(), _("No time of day has been defined."));
		return get_time_of_day_turn(i->times, n_turn, i->currentTime);
	}

	return get_time_of_day_turn(times_, n_turn, currentTime_);
}

bool tod_manager::is_start_ToD(const std::string& random_start_time)
{
	return !random_start_time.empty()
		&& utils::string_bool(random_start_time, true);
}

void tod_manager::replace_schedule(const config& time_cfg)
{
	times_.clear();
	time_of_day::parse_times(time_cfg,times_);
	currentTime_ = 0;
}

void tod_manager::add_time_area(const config& cfg)
{
	areas_.push_back(area_time_of_day());
	area_time_of_day &area = areas_.back();
	area.id = cfg["id"].str();
	area.xsrc = cfg["x"].str();
	area.ysrc = cfg["y"].str();
	area.currentTime = cfg["current_time"].to_int(0);
	std::vector<map_location> const& locs = parse_location_range(area.xsrc, area.ysrc, true);
	std::copy(locs.begin(), locs.end(), std::inserter(area.hexes, area.hexes.end()));
	time_of_day::parse_times(cfg, area.times);
}

void tod_manager::add_time_area(const std::string& id, const std::set<map_location>& locs,
		const config& time_cfg)
{
	areas_.push_back(area_time_of_day());
	area_time_of_day& area = areas_.back();
	area.id = id;
	area.hexes = locs;
	time_of_day::parse_times(time_cfg, area.times);
}

void tod_manager::remove_time_area(const std::string& area_id)
{
	if(area_id.empty()) {
		areas_.clear();
	} else {
		// search for all time areas that match the id.
		std::vector<area_time_of_day>::iterator i = areas_.begin();
		while(i != areas_.end()) {
			if((*i).id == area_id) {
				i = areas_.erase(i);
			} else {
				++i;
			}
		}
	}
}

void tod_manager::set_start_ToD(config &level, int current_turn)
{
	if (!level["current_tod"].empty())
	{
		currentTime_ = calculate_current_time(times_.size(), current_turn, level["current_tod"], true);
		return;
	}
	std::string random_start_time = level["random_start_time"];
	if (tod_manager::is_start_ToD(random_start_time))
	{
		std::vector<std::string> start_strings =
			utils::split(random_start_time, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);

		if (utils::string_bool(random_start_time,false))
		{
			// We had boolean value
			currentTime_ = calculate_current_time(times_.size(), current_turn, rand(), true);
		}
		else
		{
			const int index = calculate_current_time(start_strings.size(),
				current_turn, rand(), true);
			currentTime_ = calculate_current_time(
					times_.size(),
					current_turn,
					lexical_cast_default<int, std::string>(start_strings[index], 1) - 1,
					true
				);
		}
	}
	else
	{
		// We have to set right ToD for oldsaves
		currentTime_ = calculate_current_time(times_.size(), current_turn, currentTime_);
	}
	// Setting ToD to level data

	level["current_tod"] = currentTime_;

}

const time_of_day& tod_manager::get_time_of_day_turn(const std::vector<time_of_day>& times, int nturn, const int current_time) const
{
	const int time = calculate_current_time(times.size(), nturn, current_time);
	return times[time];
}

time_of_day tod_manager::time_of_day_at(const map_location& loc) const
{
	const gamemap& map = *resources::game_map;
	const unit_map& units = *resources::units;
	int light_modif =  map.get_terrain_info(map.get_terrain(loc)).light_modification();

	time_of_day tod = get_time_of_day(loc);

	int light = tod.lawful_bonus + light_modif;
	int illum_light = light;

	if(loc.valid()) {
		map_location locs[7];
		locs[0] = loc;
		get_adjacent_tiles(loc,locs+1);

		for(int i = 0; i != 7; ++i) {
			const unit_map::const_iterator itor = units.find(locs[i]);
			if(itor != units.end() &&
			    itor->get_ability_bool("illuminates") &&
			    !itor->incapacitated())
			{
				unit_ability_list illum = itor->get_abilities("illuminates");
				unit_abilities::effect illum_effect(illum, light, false);

				illum_light = light + illum_effect.get_composite_value();
				//max_value and min_value control the final result
				//unless ToD + terrain effect is stronger
				int max = std::max(light, illum.highest("max_value").first);
				int min = std::min(light, illum.lowest("min_value").first);
				if(illum_light > max) {
					illum_light = max;
				} else if (illum_light < min) {
					illum_light = min;
				}

			}
		}
	}

	tod.lawful_bonus_modified = illum_light - tod.lawful_bonus;
	tod.lawful_bonus = illum_light;

	return tod;
}

void tod_manager::modify_turns(const std::string& mod)
{
	num_turns_ = std::max<int>(utils::apply_modifier(num_turns_,mod,0),-1);
}
void tod_manager::set_number_of_turns(int num)
{
	num_turns_ = std::max<int>(num, -1);
}

void tod_manager::set_turn(const int num)
{
	const int new_turn = std::max<int>(num, 1);
	LOG_NG << "changing current turn number from " << turn_ << " to " << new_turn << '\n';
	// Correct ToD
	set_new_current_times(new_turn);

	if((new_turn > num_turns_) && num_turns_ != -1) {
		set_number_of_turns(new_turn);
	}
	turn_ = new_turn;
}

void tod_manager::set_new_current_times(const int new_current_turn_number)
{
	currentTime_ = calculate_current_time(times_.size(), new_current_turn_number, currentTime_);
	foreach(area_time_of_day& area, areas_) {
		area.currentTime = calculate_current_time(
			area.times.size(),
			new_current_turn_number,
			area.currentTime);
	}
}

const int tod_manager::calculate_current_time(
	const int number_of_times,
	const int for_turn_number,
	const int current_time,
	const bool only_to_allowed_range) const
{
	int new_current_time = 0;
	if(only_to_allowed_range) new_current_time = current_time % number_of_times;
	else new_current_time = (current_time + for_turn_number - turn_) % number_of_times;
	while(new_current_time < 0) { new_current_time += number_of_times; }
	return new_current_time;
}

bool tod_manager::next_turn()
{
	set_new_current_times(turn_ + 1);
	++turn_;
	return is_time_left();
}


bool tod_manager::is_time_left()
{
	return num_turns_ == -1 || turn_ <= num_turns_;
}
