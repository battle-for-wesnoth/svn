/* $Id$ */
/*
   Copyright (C) 2006 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef FLOATING_TEXTBOX_H_INCLUDED
#define FLOATING_TEXTBOX_H_INCLUDED

#include "global.hpp"

#include "display.hpp"
#include "scoped_resource.hpp"
#include "team.hpp"
#include "widgets/textbox.hpp"

class unit_map;

namespace gui{
	enum TEXTBOX_MODE { TEXTBOX_NONE, TEXTBOX_SEARCH, TEXTBOX_MESSAGE,
		        TEXTBOX_COMMAND };

	class floating_textbox{
	public:
		floating_textbox();

		const TEXTBOX_MODE mode() const { return mode_; }
		const util::scoped_ptr<gui::button>& check() const { return check_; }
		const util::scoped_ptr<gui::textbox>& box() const { return box_; }

		void close(display& gui);
		void update_location(display& gui);
		void show(gui::TEXTBOX_MODE mode, const std::string& label,
			const std::string& check_label, bool checked, display& gui);
		void tab(std::vector<team>& teams, const unit_map& units, display& gui);
		bool active() const { return box_.get() != NULL; }

	private:
		util::scoped_ptr<gui::textbox> box_;
		util::scoped_ptr<gui::button> check_;

		TEXTBOX_MODE mode_;

		std::string label_string_;
		int label_;
	};
}

#endif
