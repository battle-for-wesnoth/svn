/* $Id: mp_game_settings.cpp 55412 2012 - 2013-09-25 17:42:18Z lipk $ */
/*
   Copyright (C) 2012 - 2013 by Boldizsár Lipka <lipka.boldizsar@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/mp_depcheck_select_new.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gettext.hpp"

namespace gui2
{

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_mp_depcheck_select_new
 *
 * == MP Dependency Check: Select New ==
 *
 * Offers a list of compatible items if a currently selected one is
 * incompatible. Currently used for switching era or map.
 *
 * @begin{table}{dialog_widgets}
 *
 * message & & label & m &
 * 		displays the details of the required changes $
 *
 * itemlist & & listbox & m &
 * 		displays the available items to choose from $
 *
 * cancel & & button & m &
 * 		refuse to apply any changes $
 *
 * ok & & button & m &
 * 		select the chosen item $
 *
 * @end{table}
 *
 */

REGISTER_DIALOG(mp_depcheck_select_new)

tmp_depcheck_select_new::tmp_depcheck_select_new
				(	mp::depcheck::component_type	name,
					const std::vector<std::string>&	items	)
	: items_(items)
	, result_(-1)
{

	std::string message;

	switch (name)
	{
		case mp::depcheck::SCENARIO:
			message = _(	"The currently chosen scenario " \
							"is not compatible with your setup." \
							"\nPlease select a compatible one.");
			break;
		case mp::depcheck::ERA:
			message = _(	"The currently chosen era " \
							"is not compatible with your setup." \
							"\nPlease select a compatible one.");
			break;
		case mp::depcheck::MODIFICATION:
		//currently this can't happen, but be prepared for anything...
			message = _(	"The currently chosen modification " \
							"is not compatible with your setup." \
							"\nPlease select a compatible one.");
	}

	register_label("message", false, message);

}

void tmp_depcheck_select_new::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox& listbox = find_widget<tlistbox>(&window, "itemlist", false);

	BOOST_FOREACH(const std::string& item, items_) {
		string_map current;
		current.insert(std::make_pair("label", item));

		listbox.add_row(current);
	}

	listbox.select_row(0);
}

void tmp_depcheck_select_new::post_show(twindow& window)
{
	if (get_retval() == twindow::OK) {
		tlistbox& listbox = find_widget<tlistbox>(&window, "itemlist", false);
		result_ = listbox.get_selected_row();
	}
}

}
