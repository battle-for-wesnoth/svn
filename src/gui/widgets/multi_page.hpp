/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


#ifndef GUI_WIDGETS_MULTI_PAGE_HPP_INCLUDED
#define GUI_WIDGETS_MULTI_PAGE_HPP_INCLUDED

#include "gui/widgets/container.hpp"

namespace gui2 {

class tgenerator_;

/** The multi page class. */
class tmulti_page
		: public tcontainer_
{
	friend struct tbuilder_multi_page;
	friend class tdebug_layout_graph;

public:

	tmulti_page();

	/***** ***** ***** ***** Page handling. ***** ***** ****** *****/

	/**
	 * Adds single page to the grid.
	 *
	 * This function expect a page to one multiple widget.
	 *
	 * @param item                The data to send to the set_members of the
	 *                            widget.
	 */
	void add_page(const string_map& item);

	/**
	 * Adds single page to the grid.
	 *
	 * This function expect a page to have multiple widgets (either multiple
	 * columns or one column with multiple widgets).
	 *
	 *
	 * @param data                The data to send to the set_members of the
	 *                            widgets. If the member id is not an empty
	 *                            string it is only send to the widget that
	 *                            has the wanted id (if any). If the member
	 *                            id is an empty string, it is send to all
	 *                            members. Having both empty and non-empty
	 *                            id's gives undefined behaviour.
	 */
	void add_page(const std::map<std::string /* widget id */,
			string_map>& data);

	/** Returns the number of pages. */
	unsigned get_page_count() const;

	/**
	 * Selectes a page.
	 *
	 * @param page                The page to select.
	 * @param select              Select or deselect the page.
	 */
	void select_page(const unsigned page, const bool select = true);

	/***** ***** ***** inherited ***** ****** *****/

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_page_builder(tbuilder_grid_ptr page_builder)
		{ page_builder_ = page_builder; }

private:

	/**
	 * Finishes the building initialization of the widget.
	 *
	 * @param page_data           The initial data to fill the widget with.
	 */
	void finalize(const std::vector<string_map>& page_data);

	/**
	 * Contains a pointer to the generator.
	 *
	 * The pointer is not owned by this variable.
	 */
	tgenerator_* generator_;

	/** Contains the builder for the new items. */
	tbuilder_grid_const_ptr page_builder_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const
		{ static const std::string type = "multi_page"; return type; }

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/) {}
};

} // namespace gui2

#endif


