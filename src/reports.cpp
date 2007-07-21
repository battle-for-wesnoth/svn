/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "reports.hpp"
#include "wassert.hpp"

#include <map>

namespace {
	const std::string report_names[] = { "unit_description", "unit_type",
		"unit_level","unit_amla","unit_traits","unit_status",
		"unit_alignment","unit_abilities","unit_hp","unit_xp",
		"unit_advancement_options","unit_moves","unit_weapons",
		"unit_image","unit_profile","time_of_day",
		"turn","gold","villages","num_units","upkeep", "expenses",
		"income", "terrain", "position", "side_playing", "observers",
		"report_countdown", "report_clock",
		"selected_terrain","edit_left_button_function"
	};
}

namespace reports {

const std::string& report_name(TYPE type)
{
	wassert(sizeof(report_names)/sizeof(*report_names) == NUM_REPORTS);
	wassert(type < NUM_REPORTS);

	return report_names[type];
}

void report::add_text(std::stringstream& text, std::stringstream& tooltip) {
	add_text(text.str(), tooltip.str());
	// Clear the streams
	text.str("");
	tooltip.str("");
}

void report::add_text(const std::string& text, const std::string& tooltip) {
	this->push_back(element(text,"",tooltip));
}

void report::add_image(std::stringstream& image, std::stringstream& tooltip) {
	add_image(image.str(), tooltip.str());
	// Clear the streams
	image.str("");
	tooltip.str("");
}

void report::add_image(const std::string& image, const std::string& tooltip) {
	this->push_back(element("",image,tooltip));
}

}
