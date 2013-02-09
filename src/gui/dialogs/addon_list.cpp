/* $Id$ */
/*
   Copyright (C) 2008 - 2013 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/addon_list.hpp"

#include "gettext.hpp"
#include "gui/auxiliary/filter.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/pane.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "utils/foreach.tpp"
#include "serialization/string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_addon_list
 *
 * == Addon list ==
 *
 * This shows the dialog with the addons to install. This dialog is under
 * construction and only used with --new-widgets.
 *
 * @begin{table}{dialog_widgets}
 *
 * addons & & listbox & m &
 *        A listbox that will contain the info about all addons on the server. $
 *
 * -name & & control & o &
 *        The name of the addon. $
 *
 * -version & & control & o &
 *        The version number of the addon. $
 *
 * -author & & control & o &
 *        The author of the addon. $
 *
 * -downloads & & control & o &
 *        The number of times the addon has been downloaded. $
 *
 * -size & & control & o &
 *        The size of the addon. $
 *
 * @end{table}
 */

REGISTER_DIALOG(addon_list)

void taddon_list::collapse(tgrid& grid)
{
	find_widget<ttoggle_button>(&grid, "expand", false)
			.set_visible(twidget::VISIBLE);

	find_widget<ttoggle_button>(&grid, "collapse", false)
			.set_visible(twidget::INVISIBLE);

	find_widget<tlabel>(&grid, "description", false)
			.set_visible(twidget::INVISIBLE);
}

void taddon_list::expand(tgrid& grid)
{
	find_widget<ttoggle_button>(&grid, "expand", false)
			.set_visible(twidget::HIDDEN);

	find_widget<ttoggle_button>(&grid, "collapse", false)
			.set_visible(twidget::VISIBLE);

	find_widget<tlabel>(&grid, "description", false)
			.set_visible(twidget::VISIBLE);
}

void taddon_list::pre_show(CVideo& /*video*/, twindow& window)
{
	if(new_widgets) {

		/***** ***** Init buttons. ***** *****/

		tpane& pane = find_widget<tpane>(&window, "addons", false);


		tpane::tcompare_functor ascending_name_functor =
				boost::bind(&sort<std::string>, _1, _2, "name", true);

		tpane::tcompare_functor descending_name_functor =
				boost::bind(&sort<std::string>, _1, _2, "name", false);

		tpane::tcompare_functor ascending_size_functor =
				boost::bind(&sort<int>, _1, _2, "size", true);

		tpane::tcompare_functor descending_size_functor =
				boost::bind(&sort<int>, _1, _2, "size", false);


		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "sort_name_ascending", false)
				, boost::bind(
					  &tpane::sort
					, &pane
					, ascending_name_functor));

		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "sort_name_descending", false)
				, boost::bind(
					  &tpane::sort
					, &pane
					, descending_name_functor));

		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "sort_size_ascending", false)
				, boost::bind(
					  &tpane::sort
					, &pane
					, ascending_size_functor));

		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "sort_size_descending", false)
				, boost::bind(
					  &tpane::sort
					, &pane
					, descending_size_functor));

		/***** ***** Init the filter text box. ***** *****/

		ttext_box& filter_box =
				find_widget<ttext_box>(&window, "filter", false);

		tpane::tfilter_functor filter_functor =
				boost::bind(&contains, _1, "filter", boost::cref(filter_box));

		connect_signal_notify_modified(
				  filter_box
				, boost::bind(
					  &tpane::filter
					, &pane
					, filter_functor));

		/***** ***** Fill the listbox. ***** *****/

		tbutton* load_button = find_widget<tbutton>(
				  &window
				, "load_campaign"
				, false
				, false);
		if(load_button) {
			connect_signal_mouse_left_click(
					  *load_button
					, boost::bind(
						  &taddon_list::load
						, this
						, boost::ref(pane)));
			load(pane);
		} else {
			while(cfg_iterators_.first != cfg_iterators_.second) {
				create_campaign(pane, *cfg_iterators_.first);
				++cfg_iterators_.first;
			}
		}

	} else {
		tlistbox& list = find_widget<tlistbox>(&window, "addons", false);

		/**
		 * @todo do we really want to keep the length limit for the various
		 * items?
		 */
		FOREACH(const AUTO& c, cfg_.child_range("campaign")) {
			std::map<std::string, string_map> data;
			string_map item;

			item["label"] = c["icon"];
			data.insert(std::make_pair("icon", item));

			std::string tmp = c["name"];
			utils::truncate_as_wstring(tmp, 20);
			item["label"] = tmp;
			data.insert(std::make_pair("name", item));

			tmp = c["version"].str();
			utils::truncate_as_wstring(tmp, 12);
			item["label"] = tmp;
			data.insert(std::make_pair("version", item));

			tmp = c["author"].str();
			utils::truncate_as_wstring(tmp, 16);
			item["label"] = tmp;
			data.insert(std::make_pair("author", item));

			item["label"] = c["downloads"];
			data.insert(std::make_pair("downloads", item));

			item["label"] = c["size"];
			data.insert(std::make_pair("size", item));

			list.add_row(data);
		}
	}
}

void taddon_list::create_campaign(tpane& pane, const config& campaign)
{
			/***** Determine the data for the widgets. *****/

			std::map<std::string, string_map> data;
			string_map item;

			item["label"] = campaign["icon"];
			data.insert(std::make_pair("icon", item));

			std::string tmp = campaign["name"];
			utils::truncate_as_wstring(tmp, 20);
			item["label"] = tmp;
			data.insert(std::make_pair("name", item));

			tmp = campaign["version"].str();
			utils::truncate_as_wstring(tmp, 12);
			item["label"] = tmp;
			data.insert(std::make_pair("version", item));

			tmp = campaign["author"].str();
			utils::truncate_as_wstring(tmp, 16);
			item["label"] = tmp;
			data.insert(std::make_pair("author", item));

			item["label"] = campaign["downloads"];
			data.insert(std::make_pair("downloads", item));

			item["label"] = utils::si_string(
					  campaign["size"]
					, true
					, _("unit_byte^B"));

			data.insert(std::make_pair("size", item));

			item["label"] = campaign["description"];
			data.insert(std::make_pair("description", item));

			/***** Determine the tags for the campaign. *****/

			std::map<std::string, std::string> tags;
			tags.insert(std::make_pair("name", campaign["name"]));
			tags.insert(std::make_pair("size", campaign["size"]));

			std::stringstream filter;
			filter << campaign["version"] << '\n'
					<< campaign["author"] << '\n'
					<< campaign["type"] << '\n'
					<< campaign["description"];

			tags.insert(std::make_pair(
					  "filter"
					, utils::lowercase(filter.str())));

			/***** Add the campaign. *****/

			const unsigned id = pane.create_item(data, tags);

			tgrid* grid = pane.grid(id);
			assert(grid);

			ttoggle_button* collapse = find_widget<ttoggle_button>(
					  grid
					, "collapse"
					, false
					, false);

			if(collapse) {
				collapse->set_visible(twidget::INVISIBLE);
				collapse->set_callback_state_change(boost::bind(
						  &taddon_list::collapse
						, this
						, boost::ref(*grid)));

				find_widget<ttoggle_button>(grid, "expand", false)
						.set_callback_state_change(boost::bind(
							  &taddon_list::expand
							, this
							, boost::ref(*grid)));

				find_widget<tlabel>(grid, "description", false)
						.set_visible(twidget::INVISIBLE);
			}

}

void taddon_list::load(tpane& pane)
{
	if(cfg_iterators_.first != cfg_iterators_.second) {
		create_campaign(pane, *cfg_iterators_.first);
		++cfg_iterators_.first;
	}

	find_widget<tbutton>(pane.get_window(), "load_campaign", false)
			.set_active(cfg_iterators_.first != cfg_iterators_.second);
}

} // namespace gui2

