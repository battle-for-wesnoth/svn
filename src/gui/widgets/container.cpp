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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/container.hpp"

#include "log.hpp"

static lg::log_domain log_gui_layout("gui_layout");
#define DBG_G_L LOG_STREAM_INDENT(debug, log_gui_layout)

namespace gui2 {

void tcontainer_::layout_init()
{
	// Inherited.
	tcontrol::layout_init();

	grid_.layout_init();
}

void tcontainer_::layout_init2(const bool full_initialization)
{
	// Inherited.
	tcontrol::layout_init2(full_initialization);

	grid_.layout_init2(full_initialization);
}

void tcontainer_::NEW_layout_init(const bool full_initialization)
{
	// Inherited.
	tcontrol::NEW_layout_init(full_initialization);

	grid_.NEW_layout_init(full_initialization);
}

void tcontainer_::NEW_reduce_width(const unsigned maximum_width)
{
	grid_.NEW_reduce_width(maximum_width);
}

void tcontainer_::NEW_reduce_height(const unsigned maximum_height)
{
	grid_.NEW_reduce_height(maximum_height);
}

void tcontainer_::layout_wrap(const unsigned maximum_width)
{
	// Inherited.
	twidget::layout_wrap(maximum_width);

	log_scope2(log_gui_layout, "tcontainer(" + get_control_type() + ") " + __func__);

	// We need a copy and adjust if for the borders, no use to ask the grid for
	// the best size if it won't fit in the end due to our borders.
	const tpoint border_size = border_space();

	// Calculate the best size
	grid_.layout_wrap(maximum_width - border_space().x);
	tpoint size = grid_.get_best_size();

	// If the best size has a value of 0 it's means no limit so don't add the
	// border_size might set a very small best size.
	if(size.x) {
		size.x += border_size.x;
	}

	if(size.y) {
		size.y += border_size.y;
	}

	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " maximum_width " << maximum_width
		<< " border size " << border_size
		<< " returning " << size
		<< ".\n";

	set_layout_size(size);
}

void tcontainer_::layout_use_vertical_scrollbar(const unsigned maximum_height)
{
	// Inherited.
	twidget::layout_use_vertical_scrollbar(maximum_height);

	log_scope2(log_gui_layout, "tcontainer(" + get_control_type() + ") " + __func__);

	// We need a copy and adjust if for the borders, no use to ask the grid for
	// the best size if it won't fit in the end due to our borders.
	const tpoint border_size = border_space();

	// Calculate the best size
	grid_.layout_use_vertical_scrollbar(maximum_height - border_space().y);
	tpoint size = grid_.get_best_size();

	// If the best size has a value of 0 it's means no limit so don't add the
	// border_size might set a very small best size.
	if(size.x) {
		size.x += border_size.x;
	}

	if(size.y) {
		size.y += border_size.y;
	}

	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " maximum_height " << maximum_height
		<< " border size " << border_size
		<< " returning " << size
		<< ".\n";

	set_layout_size(size);
}

void tcontainer_::layout_use_horizontal_scrollbar(const unsigned maximum_width)
{
	// Inherited.
	twidget::layout_use_horizontal_scrollbar(maximum_width);

	log_scope2(log_gui_layout, "tcontainer(" + get_control_type() + ") " + __func__);

	// We need a copy and adjust if for the borders, no use to ask the grid for
	// the best size if it won't fit in the end due to our borders.
	const tpoint border_size = border_space();

	// Calculate the best size
	grid_.layout_use_horizontal_scrollbar(maximum_width - border_space().y);
	tpoint size = grid_.get_best_size();

	// If the best size has a value of 0 it's means no limit so don't add the
	// border_size might set a very small best size.
	if(size.x) {
		size.x += border_size.x;
	}

	if(size.y) {
		size.y += border_size.y;
	}

	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " maximum_width " << maximum_width
		<< " border size " << border_size
		<< " returning " << size
		<< ".\n";

	set_layout_size(size);
}

void tcontainer_::layout_fit_width(const unsigned maximum_width,
		const tfit_flags flags)
{
	// Inherited.
	twidget::layout_fit_width(maximum_width, flags);

	log_scope2(log_gui_layout, "tcontainer(" + get_control_type() + ") " + __func__);
	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " maximum_width " << maximum_width
		<< " flags " << flags
		<< ".\n";

	// We need a copy and adjust if for the borders, no use to ask the grid for
	// the best size if it won't fit in the end due to our borders.
	const tpoint border_size = border_space();

	// Calculate the best size
	grid_.layout_fit_width(maximum_width - border_space().y, flags);
	tpoint size = grid_.get_best_size();

	// If the best size has a value of 0 it's means no limit so don't add the
	// border_size might set a very small best size.
	if(size.x) {
		size.x += border_size.x;
	}

	if(size.y) {
		size.y += border_size.y;
	}

	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " maximum_width " << maximum_width
		<< " flags " << flags
		<< " border size " << border_size
		<< " returning " << size
		<< ".\n";

	set_layout_size(size);
}

void tcontainer_::set_size(const tpoint& origin, const tpoint& size)
{
	tcontrol::set_size(origin, size);

	const SDL_Rect rect = get_client_rect();
	const tpoint client_size(rect.w, rect.h);
	const tpoint client_position(rect.x, rect.y);
	grid_.set_size(client_position, client_size);
}

tpoint tcontainer_::calculate_best_size() const
{
	log_scope2(log_gui_layout, "tcontainer(" +
		get_control_type() + ") " + __func__);

	tpoint result(grid_.get_best_size());
	const tpoint border_size = border_space();

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	if(result.x) {
		result.x += border_size.x;
	}

	if(result.y) {
		result.y += border_size.y;
	}

	DBG_G_L << "tcontainer(" + get_control_type() + "):"
		<< " border size " << border_size
		<< " returning " << result
		<< ".\n";

	return result;
}

void tcontainer_::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	const SDL_Rect rect = get_client_rect();
	const tpoint client_position(rect.x, rect.y);
	grid_.set_origin(client_position);
}

void tcontainer_::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	grid_.set_visible_area(area);
}

void tcontainer_::set_active(const bool active)
{
	// Not all our children might have the proper state so let them run
	// unconditionally.
	grid_.set_active(active);

	if(active == get_active()) {
		return;
	}

	set_dirty();

	set_self_active(active);
}

} // namespace gui2

