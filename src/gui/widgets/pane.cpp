/* $Id$ */
/*
   Copyright (C) 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/pane.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/widgets/grid.hpp"

#define LOG_SCOPE_HEADER "tpane [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

tpane::tpane(const tbuilder_grid_ptr item_builder)
	: twidget()
	, items_()
	, item_builder_(item_builder)
	, item_id_generator_(0)
{
}

unsigned tpane::create_item(
		  const std::map<std::string, string_map>& item_data
		, const std::map<std::string, std::string>& tags)
{
	titem item = { item_id_generator_++, tags, item_builder_->build() };

	item.grid->set_parent(this);

	typedef std::pair<std::string, string_map> hack ;
	BOOST_FOREACH(const hack& data, item_data) {
		tcontrol* control = find_widget<tcontrol>(
				  item.grid
				, data.first
				, false
				, false);

		if(control) {
			control->set_members(data.second);
		}
	}

	items_.push_back(item);
	return item.id;
}

void tpane::place(const tpoint& origin, const tpoint& size)
{
	DBG_GUI_L << LOG_HEADER << '\n';
	twidget::place(origin, size);

	place_children();
}

void tpane::impl_draw_children(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	DBG_GUI_D << LOG_HEADER << '\n';

	BOOST_FOREACH(titem& item, items_) {
		item.grid->draw_children(frame_buffer, x_offset, y_offset);
	}
}

void tpane::child_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack)
{
	BOOST_FOREACH(titem& item, items_) {
		std::vector<twidget*> child_call_stack = call_stack;
		item.grid->populate_dirty_list(caller, child_call_stack);
	}
}

void tpane::sort(const tcompare_functor& compare_functor)
{
	items_.sort(compare_functor);

	set_origin_children();
}

void tpane::filter(const tfilter_functor& filter_functor)
{
	BOOST_FOREACH(titem& item, items_) {
		item.grid->set_visible(
				filter_functor(item)
					? twidget::VISIBLE
					: twidget::INVISIBLE);
	}

	set_origin_children();
}

void tpane::request_reduce_width(const unsigned /*maximum_width*/)
{
}

tpoint tpane::calculate_best_size() const
{
	return tpoint(100, 100);
}

bool tpane::disable_click_dismiss() const
{
	return false;
}

iterator::twalker_* tpane::create_walker()
{
	/**
	 * @todo Implement properly.
	 */
	return NULL;
}

void tpane::place_children()
{
	unsigned y = 0;

	BOOST_FOREACH(titem& item, items_) {
		if(item.grid->get_visible() == twidget::INVISIBLE) {
			continue;
		}

		DBG_GUI_L << LOG_HEADER << " offset " << y << '\n';
		item.grid->place(tpoint(0, y), item.grid->get_best_size());
		y += item.grid->get_height();
	}
}

void tpane::set_origin_children()
{
	unsigned y = 0;

	BOOST_FOREACH(titem& item, items_) {
		if(item.grid->get_visible() == twidget::INVISIBLE) {
			continue;
		}

		DBG_GUI_L << LOG_HEADER << " offset " << y << '\n';
		item.grid->set_origin(tpoint(0, y));
		y += item.grid->get_height();
	}
}

} // namespace gui2
