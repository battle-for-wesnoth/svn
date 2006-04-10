/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef REPORTS_HPP_INCLUDED
#define REPORTS_HPP_INCLUDED

#include <set>
#include <string>
#include <vector>

class gamestatus;

#include "map.hpp"
#include "team.hpp"
#include "unit.hpp"

//this module is responsible for outputting textual reports of
//various game and unit statistics
namespace reports {

       enum TYPE { UNIT_DESCRIPTION, UNIT_TYPE, UNIT_LEVEL, UNIT_AMLA,
		    UNIT_TRAITS, UNIT_STATUS,
	            UNIT_ALIGNMENT, UNIT_ABILITIES, UNIT_HP, UNIT_XP,
		    UNIT_ADVANCEMENT_OPTIONS, UNIT_MOVES, UNIT_WEAPONS,
		    UNIT_IMAGE, UNIT_PROFILE, TIME_OF_DAY,
		    TURN, GOLD, VILLAGES, NUM_UNITS, UPKEEP, 
		    INCOME, TERRAIN, POSITION, SIDE_PLAYING, OBSERVERS,
		    REPORT_COUNTDOWN, REPORT_CLOCK, SELECTED_TERRAIN,
		    EDIT_LEFT_BUTTON_FUNCTION, NUM_REPORTS};

	enum { UNIT_REPORTS_BEGIN=UNIT_DESCRIPTION, UNIT_REPORTS_END=UNIT_PROFILE+1 };
	enum { STATUS_REPORTS_BEGIN=TIME_OF_DAY, STATUS_REPORTS_END=NUM_REPORTS};

	const std::string& report_name(TYPE type);

	struct element {
		// Invariant: either text or image should be empty
		// It would be okay to create a class for this, but it's a pretty simple
		// invariant so I left it like the original report class.
	        image::locator image;
		std::string text;

		std::string tooltip;

		element() {}
		explicit element(const std::string& text) : text(text) {}
		element(const std::string& text, const std::string& image, const std::string& tooltip) :
			image(image), text(text), tooltip(tooltip) {}

		element(const std::string& text, const image::locator& image, const std::string& tooltip) :
			image(image), text(text), tooltip(tooltip) {}
		element(const std::string& text, const char* image, const std::string& tooltip) :
			image(image), text(text), tooltip(tooltip) {}

		bool operator==(const element& o) const {
			return o.text == text && o.image == image && o.tooltip == tooltip;
		}
		bool operator!=(const element& o) const { return !(o == *this); }
	};
	struct report : public std::vector<element> {
		report() {}
		explicit report(const std::string& text) { this->push_back(element(text)); }
		report(const std::string& text, const std::string& image, const std::string& tooltip) {
			this->push_back(element(text, image, tooltip));
		}
		report(const std::string& text, const char* image, const std::string& tooltip) {
			this->push_back(element(text, image, tooltip));
		}
		report(const std::string& text, const image::locator& image, const std::string& tooltip) {
			this->push_back(element(text, image, tooltip));
		}

		// Convenience functions
		void add_text(std::stringstream& text, std::stringstream& tooltip);
		void add_text(const std::string& text, const std::string& tooltip);
		void add_image(const std::string& image, const std::string& tooltip);
		void add_image(std::stringstream& image, std::stringstream& tooltip);
	};

	report generate_report(TYPE type, const gamemap& map, unit_map& units,
	                       const std::vector<team>& teams, const team& current_team,
	                       int current_side, int active_side,
	                       const gamemap::location& loc, const gamemap::location& mouseover,
	                       const gamestatus& status, const std::set<std::string>& observers);
	// Set what will be shown for the report with type
	// which_report. Note that this only works for some reports,
	// i.e. reports that can not be deducted from the supplied arguments
	// to generate_report.
	// Currently: SELECTED_TERRAIN, EDIT_LEFT_BUTTON_FUNCTION
	void set_report_content(const TYPE which_report, const std::string &content);
}

#endif
