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

#include "gui/widgets/scrollbar_container_private.hpp"

#include "foreach.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/layout_exception.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

namespace {

static const std::string button_up_names[] = {
	"_begin", "_line_up", "_half_page_up", "_page_up" };

static const std::string button_down_names[] = {
	"_end", "_line_down", "_half_page_down", "_page_down" };

/**
 * Returns a map with the names of all buttons and the scrollbar jump they're
 * supposed to execute.
 */
const std::map<std::string, tscrollbar_::tscroll>& scroll_lookup()
{
	static std::map<std::string, tscrollbar_::tscroll> lookup;
	if(lookup.empty()) {
		lookup["_begin"] = tscrollbar_::BEGIN;
		lookup["_line_up"] = tscrollbar_::ITEM_BACKWARDS;
		lookup["_half_page_up"] = tscrollbar_::HALF_JUMP_BACKWARDS;
		lookup["_page_up"] = tscrollbar_::JUMP_BACKWARDS;

		lookup["_end"] = tscrollbar_::END;
		lookup["_line_down"] = tscrollbar_::ITEM_FORWARD;
		lookup["_half_page_down"] = tscrollbar_::HALF_JUMP_FORWARD;
		lookup["_page_down"] = tscrollbar_::JUMP_FORWARD;
	}

	return lookup;
}

void callback_vertical_scrollbar_button(twidget* caller)
{
	gui2::get_parent<gui2::tscrollbar_container>
		(caller)->vertical_scrollbar_click(caller);
}

void callback_horizontal_scrollbar_button(twidget* caller)
{
	gui2::get_parent<gui2::tscrollbar_container>
		(caller)->horizontal_scrollbar_click(caller);
}

void callback_vertical_scrollbar(twidget* caller)
{
	gui2::get_parent<gui2::tscrollbar_container>
		(caller)->vertical_scrollbar_moved(caller);
}

void callback_horizontal_scrollbar(twidget* caller)
{
	gui2::get_parent<gui2::tscrollbar_container>
		(caller)->horizontal_scrollbar_moved(caller);
}

} // namespace

tscrollbar_container::tscrollbar_container(const unsigned canvas_count)
	: tcontainer_(canvas_count)
	, state_(ENABLED)
	, vertical_scrollbar_mode_(auto_visible_first_run)
	, horizontal_scrollbar_mode_(auto_visible_first_run)
	, vertical_scrollbar_grid_(NULL)
	, horizontal_scrollbar_grid_(NULL)
	, vertical_scrollbar_(NULL)
	, horizontal_scrollbar_(NULL)
	, content_grid_(NULL)
	, content_(NULL)
	, content_visible_area_()
{
}

void tscrollbar_container::NEW_layout_init(const bool full_initialization)
{
	// Inherited.
	tcontainer_::NEW_layout_init(full_initialization);

	if(full_initialization) {

		assert(vertical_scrollbar_grid_);
		switch(vertical_scrollbar_mode_) {
			case always_visible :
				vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				vertical_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}

		assert(horizontal_scrollbar_grid_);
		switch(horizontal_scrollbar_mode_) {
			case always_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
	}

	assert(content_grid_);
	content_grid_->NEW_layout_init(full_initialization);
}

void tscrollbar_container::NEW_request_reduce_height(
		const unsigned maximum_height)
{
	if(vertical_scrollbar_mode_ == always_invisible) {
		return;
	}

	assert(vertical_scrollbar_grid_);

	tpoint size = get_best_size();
	if(static_cast<unsigned>(size.y) < maximum_height) {
		return;
	}

	const bool resized =
		vertical_scrollbar_grid_->get_visible() == twidget::INVISIBLE;

	// Always set the bar visible, is a nop is already visible.
	vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);

	const tpoint scrollbar_size = vertical_scrollbar_grid_->get_best_size();

	// If showing the scrollbar increased the height, hide and abort.
	if(resized && scrollbar_size.y > size.y) {
		vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		return;
	}

	if(maximum_height > static_cast<unsigned>(scrollbar_size.y)) {
		size.y = maximum_height;
	} else {
		size.y = scrollbar_size.y;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);

	if(resized) {
		throw tlayout_exception_width_modified();
	}
}

void tscrollbar_container::NEW_request_reduce_width(
		const unsigned maximum_width)
{
	// First ask our content, it might be able to wrap which looks better as
	// a scrollbar.
	assert(content_grid_);
	const unsigned offset = vertical_scrollbar_grid_
			&& vertical_scrollbar_grid_->get_visible() != twidget::INVISIBLE
				?  vertical_scrollbar_grid_->get_best_size().x
				: 0;
	content_grid_->NEW_request_reduce_width(maximum_width - offset);

	// Did we manage to achieve the wanted size?
	assert(horizontal_scrollbar_grid_);

	tpoint size = get_best_size();
	if(static_cast<unsigned>(size.x) < maximum_width) {
		return;
	}

	if(horizontal_scrollbar_mode_ == always_invisible) {
		return;
	}

	// Always set the bar visible, is a nop when it's already visible.
	horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
	size = get_best_size();

	const tpoint scrollbar_size = horizontal_scrollbar_grid_->get_best_size();

	// If showing the scrollbar increased the width, hide and abort.
	if(horizontal_scrollbar_mode_ == auto_visible_first_run
			&& scrollbar_size.x > size.x) {

		horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		return;
	}

	if(maximum_width > static_cast<unsigned>(scrollbar_size.x)) {
		size.x = maximum_width;
	} else {
		size.x = scrollbar_size.x;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);
}

tpoint tscrollbar_container::calculate_best_size() const
{
	log_scope2(log_gui_layout,
		std::string("tscrollbar_container ") + __func__);

	/***** get vertical scrollbar size *****/
	const tpoint vertical_scrollbar =
			vertical_scrollbar_grid_->get_visible() == twidget::INVISIBLE
			? tpoint(0, 0)
			: vertical_scrollbar_grid_->get_best_size();

	/***** get horizontal scrollbar size *****/
	const tpoint horizontal_scrollbar =
			horizontal_scrollbar_grid_->get_visible() == twidget::INVISIBLE
			? tpoint(0, 0)
			: horizontal_scrollbar_grid_->get_best_size();

	/***** get content size *****/
	assert(content_grid_);
	const tpoint content = content_grid_->get_best_size();

	const tpoint result(
			vertical_scrollbar.x +
				std::max(horizontal_scrollbar.x, content.x),
			horizontal_scrollbar.y +
				std::max(vertical_scrollbar.y,  content.y));

	DBG_GUI_L << "tscrollbar_container"
		<< " vertical_scrollbar " << vertical_scrollbar
		<< " horizontal_scrollbar " << horizontal_scrollbar
		<< " content " << content
		<< " result " << result
		<< ".\n";

	return result;
}

static void set_scrollbar_mode(tgrid* scrollbar_grid, tscrollbar_* scrollbar,
		tscrollbar_container::tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items)
{

	assert(scrollbar_grid && scrollbar);

	if(scrollbar_mode == tscrollbar_container::always_invisible) {
		scrollbar_grid->set_visible(twidget::INVISIBLE);
		return;
	}


	scrollbar->set_item_count(items);
	scrollbar->set_visible_items(visible_items);

	if(scrollbar_mode == tscrollbar_container::auto_visible) {

		const bool scrollbar_needed = items > visible_items;

		scrollbar_grid->set_visible(scrollbar_needed
				? twidget::VISIBLE
				: twidget::HIDDEN);
	}
}

void tscrollbar_container::
		set_size(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tcontainer_::set_size(origin, size);

	// Set content size
	assert(content_ && content_grid_);

	const tpoint content_origin = content_->get_origin();

	const tpoint best_size = content_grid_->get_best_size();
	const tpoint content_size(content_->get_width(), content_->get_height());

	const tpoint content_grid_size(
			std::max(best_size.x, content_size.x),
			std::max(best_size.y, content_size.y));

	content_grid_->set_size(content_origin, content_grid_size);

	// Set vertical scrollbar
	set_scrollbar_mode(vertical_scrollbar_grid_, vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_->get_height(),
			content_->get_height());

	// Set horizontal scrollbar
	set_scrollbar_mode(horizontal_scrollbar_grid_, horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_->get_width(),
			content_->get_width());

	// Update the buttons.
	set_scrollbar_button_status();

	// Set the easy close status.
	set_block_easy_close(is_visible()
			&& get_active() && does_block_easy_close());

	// Now set the visible part of the content.
	content_visible_area_ = content_->get_rect();
	content_grid_->set_visible_area(content_visible_area_);
}

void tscrollbar_container::set_origin(const tpoint& origin)
{
	// Inherited.
	tcontainer_::set_origin(origin);

	// Set content size
	assert(content_ && content_grid_);

	const tpoint content_origin = content_->get_origin();

	content_grid_->set_origin(content_origin);

	// Changing the origin also invalidates the visible area.
	content_grid_->set_visible_area(content_visible_area_);
}

void tscrollbar_container::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	tcontainer_::set_visible_area(area);

	// Now get the visible part of the content.
	content_visible_area_ =
			get_rect_union(area, content_->get_rect());

	content_grid_->set_visible_area(content_visible_area_);
}

void tscrollbar_container::key_press(tevent_handler& /*event*/,
		bool& handled, SDLKey key,
		SDLMod modifier, Uint16 /*unicode*/)
{
	DBG_GUI_E << "Scrollbar container: key press.\n";

	switch(key) {
		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;


		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;


		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;
		default:
			/* ignore */
			break;
		}
}

void tscrollbar_container::focus(tevent_handler&)
{
	twindow* window = get_window();
	assert(window);
	window->keyboard_capture(this);
}

twidget* tscrollbar_container::find_widget(
		const tpoint& coordinate, const bool must_be_active)
{
	return tscrollbar_container_implementation
			::find_widget<twidget>(*this, coordinate, must_be_active);
}

const twidget* tscrollbar_container::find_widget(const tpoint& coordinate,
		const bool must_be_active) const
{
	return tscrollbar_container_implementation
			::find_widget<const twidget>(*this, coordinate, must_be_active);
}

bool tscrollbar_container::does_block_easy_close() const
{
	assert(vertical_scrollbar_grid_
			&& vertical_scrollbar_
			&& horizontal_scrollbar_grid_
			&& horizontal_scrollbar_);

	const bool vertical_block = vertical_scrollbar_grid_->is_visible()
			&& !(vertical_scrollbar_->at_begin()
					&& vertical_scrollbar_->at_end());

	const bool horizontal_block = horizontal_scrollbar_grid_->is_visible()
			&& !(horizontal_scrollbar_->at_begin()
					&& horizontal_scrollbar_->at_end());

	return vertical_block || horizontal_block;
}

void tscrollbar_container::vertical_scrollbar_click(twidget* caller)
{
	const std::map<std::string, tscrollbar_::tscroll>::const_iterator
		itor = scroll_lookup().find(caller->id());

	assert(itor != scroll_lookup().end());
	vertical_scrollbar_->scroll(itor->second);

	scrollbar_moved();
}

void tscrollbar_container::horizontal_scrollbar_click(twidget* caller)
{
	const std::map<std::string, tscrollbar_::tscroll>::const_iterator
		itor = scroll_lookup().find(caller->id());

	assert(itor != scroll_lookup().end());
	horizontal_scrollbar_->scroll(itor->second);

	scrollbar_moved();
}

void tscrollbar_container::finalize_setup()
{
	/***** Setup vertical scrollbar *****/

	vertical_scrollbar_grid_ =
		find_widget<tgrid>("_vertical_scrollbar_grid", false, true);

	vertical_scrollbar_ = vertical_scrollbar_grid_->
		find_widget<tscrollbar_>("_vertical_scrollbar", false, true);

	vertical_scrollbar_->
		set_callback_positioner_move(callback_vertical_scrollbar);

	/***** Setup horizontal scrollbar *****/
	horizontal_scrollbar_grid_ =
		find_widget<tgrid>("_horizontal_scrollbar_grid", false, true);

	horizontal_scrollbar_ = horizontal_scrollbar_grid_->
		find_widget<tscrollbar_>("_horizontal_scrollbar", false, true);

	horizontal_scrollbar_->
		set_callback_positioner_move(callback_horizontal_scrollbar);

	/***** Setup the scrollbar buttons *****/
	typedef std::pair<std::string, tscrollbar_::tscroll> hack;
	foreach(const hack& item, scroll_lookup()) {

		// Vertical.
		tbutton* button = vertical_scrollbar_grid_->
				find_widget<tbutton>(item.first, false, false);

		if(button) {
			button->set_callback_mouse_left_click(
					callback_vertical_scrollbar_button);
		}

		// Horizontal.
		button = horizontal_scrollbar_grid_->
				find_widget<tbutton>(item.first, false, false);

		if(button) {
			button->set_callback_mouse_left_click(
					callback_horizontal_scrollbar_button);
		}
	}

	/***** Setup the content *****/
	content_ = new tspacer();
	content_->set_definition("default");

	content_grid_ = dynamic_cast<tgrid*>(
			grid().swap_child("_content_grid", content_, true));
	assert(content_grid_);

	content_grid_->set_parent(this);

	/***** Set the easy close status. *****/
	set_block_easy_close(is_visible()
			&& get_active() && does_block_easy_close());

	/***** Let our subclasses initialize themselves. *****/
	finalize_subclass();
}

void tscrollbar_container::
		set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(vertical_scrollbar_mode_ != scrollbar_mode) {
		vertical_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscrollbar_container::
		set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(horizontal_scrollbar_mode_ != scrollbar_mode) {
		horizontal_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscrollbar_container::impl_draw_children(surface& frame_buffer)
{
	// Inherited.
	tcontainer_::impl_draw_background(frame_buffer);

	content_grid_->draw_children(frame_buffer);
}

void tscrollbar_container::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tcontainer_::child_populate_dirty_list(caller, call_stack);

	assert(content_grid_);
	std::vector<twidget*> child_call_stack(call_stack);
	content_grid_->populate_dirty_list(caller, child_call_stack);
}

void tscrollbar_container::show_content_rect(const SDL_Rect& rect)
{
	assert(content_);
	assert(horizontal_scrollbar_ && vertical_scrollbar_);

	// Set the bottom right location first if it doesn't fit the top left
	// will look good. First calculate the left and top position depending on
	// the current position.

	const int left_position = horizontal_scrollbar_->get_item_position()
			+ (rect.x - content_->get_x());
	const int top_position = vertical_scrollbar_->get_item_position()
			+ (rect.y - content_->get_y());

	// bottom.
	const int wanted_bottom = rect.y + rect.h;
	const int current_bottom = content_->get_y() + content_->get_height();
	int distance = wanted_bottom - current_bottom;
	if(distance > 0) {
		vertical_scrollbar_->set_item_position(
				vertical_scrollbar_->get_item_position() + distance);
	}

	// right.
	const int wanted_right = rect.x + rect.w;
	const int current_right = content_->get_x() + content_->get_width();
	distance = wanted_right - current_right;
	if(distance > 0) {
		horizontal_scrollbar_->set_item_position(
				horizontal_scrollbar_->get_item_position() + distance);
	}

	// top.
	if(top_position < static_cast<int>(
				vertical_scrollbar_->get_item_position())) {

		vertical_scrollbar_->set_item_position(top_position);
	}

	// left.
	if(left_position < static_cast<int>(
				horizontal_scrollbar_->get_item_position())) {

		horizontal_scrollbar_->set_item_position(left_position);
	}

	// Update.
	scrollbar_moved();
}

void tscrollbar_container::set_scrollbar_button_status()
{
	if(true) { /** @todo scrollbar visibility. */
		/***** set scroll up button status *****/
		foreach(const std::string& name, button_up_names) {
			tbutton* button = vertical_scrollbar_grid_->
					find_widget<tbutton>(name, false, false);

			if(button) {
				button->set_active(!vertical_scrollbar_->at_begin());
			}
		}

		/***** set scroll down status *****/
		foreach(const std::string& name, button_down_names) {
			tbutton* button = vertical_scrollbar_grid_->
					find_widget<tbutton>(name, false, false);

			if(button) {
				button->set_active(!vertical_scrollbar_->at_end());
			}
		}

		/***** Set the status if the scrollbars *****/
		vertical_scrollbar_->set_active( !(vertical_scrollbar_->at_begin()
				&& vertical_scrollbar_->at_end()));
	}

	if(true) { /** @todo scrollbar visibility. */
		/***** Set scroll left button status *****/
		foreach(const std::string& name, button_up_names) {
			tbutton* button = horizontal_scrollbar_grid_->
					find_widget<tbutton>(name, false, false);

			if(button) {
				button->set_active(!horizontal_scrollbar_->at_begin());
			}
		}

		/***** Set scroll right button status *****/
		foreach(const std::string& name, button_down_names) {
			tbutton* button = horizontal_scrollbar_grid_->
					find_widget<tbutton>(name, false, false);

			if(button) {
				button->set_active(!horizontal_scrollbar_->at_end());
			}
		}

		/***** Set the status if the scrollbars *****/
		horizontal_scrollbar_->set_active( !(horizontal_scrollbar_->at_begin()
				&& horizontal_scrollbar_->at_end()));
	}
}

void tscrollbar_container::
		mouse_wheel_up(tevent_handler& /*event_handler*/, bool& handled)
{
	assert(vertical_scrollbar_grid_ && vertical_scrollbar_);

	if(vertical_scrollbar_grid_->is_visible()) {
		vertical_scrollbar_->scroll(tscrollbar_::HALF_JUMP_BACKWARDS);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::
		mouse_wheel_down(tevent_handler& /*event_handler*/, bool& handled)
{
	assert(vertical_scrollbar_grid_ && vertical_scrollbar_);

	if(vertical_scrollbar_grid_->is_visible()) {
		vertical_scrollbar_->scroll(tscrollbar_::HALF_JUMP_FORWARD);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::
		mouse_wheel_left(tevent_handler& /*event_handler*/, bool& handled)
{
	assert(horizontal_scrollbar_grid_ && horizontal_scrollbar_);

	if(horizontal_scrollbar_grid_->is_visible()) {
		horizontal_scrollbar_->scroll(tscrollbar_::HALF_JUMP_BACKWARDS);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::
		mouse_wheel_right(tevent_handler& /*event_handler*/, bool& handled)
{
	assert(horizontal_scrollbar_grid_ && horizontal_scrollbar_);

	if(horizontal_scrollbar_grid_->is_visible()) {
		horizontal_scrollbar_->scroll(tscrollbar_::HALF_JUMP_FORWARD);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::handle_key_home(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::BEGIN);
	horizontal_scrollbar_->scroll(tscrollbar_::BEGIN);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::handle_key_end(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::END);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_page_up(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_page_down(SDLMod /*modifier*/, bool& handled)

{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_up_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_down_arrow( SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container
		::handle_key_left_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container
		::handle_key_right_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::scrollbar_moved()
{
	// Init.
	assert(content_ && content_grid_);
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	/*** Update the content location. ***/
	const int x_offset = horizontal_scrollbar_mode_ == always_invisible
			? 0
			: horizontal_scrollbar_->get_item_position() *
			  horizontal_scrollbar_->get_step_size();

	const int y_offset = vertical_scrollbar_mode_ == always_invisible
			? 0
			: vertical_scrollbar_->get_item_position() *
			  vertical_scrollbar_->get_step_size();

	const tpoint content_size = content_grid_->get_best_size();

	const tpoint content_origin = tpoint(
			content_->get_x() - x_offset,
			content_->get_y() - y_offset);

	content_grid_->set_origin(content_origin);
	content_grid_->set_visible_area(content_visible_area_);
	content_grid_->set_dirty();

	// Update scrollbar.
	set_scrollbar_button_status();
}

} // namespace gui2

