/* $Id$ */
/*
   Copyright (C) 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/campaign_selection.hpp"

#include "foreach.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/multi_page.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "serialization/string_utils.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_selection
 *
 * == Campaign selection ==
 *
 * This shows the dialog choose which campaign the user wants to play.
 *
 * @start_table = grid
 *     (campaign_list) (listbox) ()
 *                                A listbox that contains all available
 *                                campaigns.
 *     -[icon] (image) ()         The icon for the campaign.
 *     -[name] (control) ()       The name of the campaign.
 *     -[victory] (image) ()      The icon to show when the user finished the
 *                                campaign. The engine determines whether or
 *                                not the user has finished the campaign and
 *                                sets the visible flag for the widget
 *                                accordingly.
 *     (campaign_details) (multi_page) ()
 *                                A multi page widget that shows more details
 *                                for the selected campaign.
 *     -[image] (image) ()        The image for the campaign.
 *     -[description] (control) ()
 *                                The description of the campaign.
 * @end_table
 */

void tcampaign_selection::campaign_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "campaign_list", false);

	tmulti_page& multi_page = find_widget<tmulti_page>(
			&window, "campaign_details", false);

	multi_page.select_page(list.get_selected_row());
}

twindow* tcampaign_selection::build_window(CVideo& video)
{
	return build(video, get_id(CAMPAIGN_SELECTION));
}

void tcampaign_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	/***** Setup campaign list. *****/
	tlistbox& list = find_widget<tlistbox>(&window, "campaign_list", false);

	list.set_callback_value_change(dialog_callback
			<tcampaign_selection, &tcampaign_selection::campaign_selected>);

	window.keyboard_capture(&list);

	/***** Setup campaign details. *****/
	tmulti_page& multi_page = find_widget<tmulti_page>(
			&window, "campaign_details", false);

	foreach (const config &c, campaigns_) {

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = c["icon"];
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = c["name"];
		list_item_item.insert(std::make_pair("name", list_item));

		list.add_row(list_item_item);

		tgrid* grid = list.get_row_grid(list.get_item_count() - 1);
		assert(grid);

		twidget* widget = grid->find("victory", false);
		if(widget && !utils::string_bool(c["completed"], false)) {
			widget->set_visible(twidget::HIDDEN);
		}

		/*** Add detail item ***/
		string_map detail_item;
		std::map<std::string, string_map> detail_page;

		detail_item["label"] = c["description"];
		detail_page.insert(std::make_pair("description", detail_item));

		detail_item["label"] = c["image"];
		detail_page.insert(std::make_pair("image", detail_item));

		multi_page.add_page(detail_page);
	}

	campaign_selected(window);
}

void tcampaign_selection::post_show(twindow& window)
{
		choice_ = find_widget<tlistbox>(
				&window, "campaign_list", false).get_selected_row();
}

} // namespace gui2
