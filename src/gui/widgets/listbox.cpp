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

#include "gui/widgets/listbox.hpp"

#include "gui/widgets/window.hpp"

namespace gui2 {

namespace {

void callback_list_item_clicked(twidget* caller)
{
	get_parent<tlistbox>(caller)->list_item_clicked(caller);
}

} // namespace

tlistbox::tlistbox(const bool has_minimum, const bool has_maximum,
		const tgenerator_::tplacement placement, const bool select)
	: tscrollbar_container(2) // FIXME magic number
	, generator_(NULL)
	, list_builder_(NULL)
	, callback_value_changed_(NULL)
{
	generator_ = tgenerator_::build(
			has_minimum, has_maximum, placement, select);
}

void tlistbox::add_row(const string_map& item)
{
	assert(generator_);
	generator_->create_item(
			-1, list_builder_, item, callback_list_item_clicked);
}

void tlistbox::add_row(
		const std::map<std::string /* widget id */, string_map>& data)
{
	assert(generator_);
	generator_->create_item(
			-1, list_builder_, data, callback_list_item_clicked);
}

void tlistbox::remove_row(const unsigned row, unsigned count)
{
	assert(generator_);

	if(row >= get_item_count()) {
		return;
	}

	if(!count || count > get_item_count()) {
		count = get_item_count();
	}

	for(; count; --count) {
		generator_->delete_item(row);
	}
}

void tlistbox::clear()
{
	// Due to the removing from the linked group, don't use
	// generator_->clear() directly.
	remove_row(0, 0);
}

unsigned tlistbox::get_item_count() const
{
	assert(generator_);
	return generator_->get_item_count();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	assert(generator_);
	generator_->get_item(row).set_active(active);
}

const tgrid* tlistbox::get_row_grid(const unsigned row) const
{
	assert(generator_);
	// rename this function and can we return a reference??
	return &generator_->get_item(row);
}

tgrid* tlistbox::get_row_grid(const unsigned row)
{
	assert(generator_);
	return &generator_->get_item(row);
}

bool tlistbox::select_row(const unsigned row, const bool select)
{
	assert(generator_);

	generator_->select_item(row, select);

	return true; // FIXME test what result should have been!!!
}

int tlistbox::get_selected_row() const
{
	assert(generator_);

	return generator_->get_selected_item();
}

void tlistbox::list_item_clicked(twidget* caller)
{
	assert(caller);
	assert(generator_);

	/** @todo Hack to capture the keyboard focus. */
	get_window()->keyboard_capture(this);

	for(size_t i = 0; i < generator_->get_item_count(); ++i) {

		if(generator_->get_item(i).has_widget(caller)) {
			generator_->toggle_item(i);
			if(callback_value_changed_) {
				callback_value_changed_(this);
			}
			return;
		}
	}
	assert(false);
}

void tlistbox::set_size(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tscrollbar_container::set_size(origin, size);

	/**
	 * @todo Work-around to set the selected item visible again.
	 *
	 * At the moment the listboxes and dialogs in general are resized a lot as
	 * work-around for sizing. So this function makes the selected item in view
	 * again. It doesn't work great in all cases but the proper fix is to avoid
	 * resizing dialogs a lot. Need more work later on.
	 */
	const int selected_item = generator_->get_selected_item();
	if(selected_item != -1) {
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->get_item(selected_item).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);
	}
}

void tlistbox::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tscrollbar_container::child_populate_dirty_list(caller, call_stack);

	assert(generator_);
	std::vector<twidget*> child_call_stack = call_stack;
	generator_->populate_dirty_list(caller, child_call_stack);
}

void tlistbox::handle_key_up_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_up_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->get_item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_down_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_down_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->get_item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_left_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_left_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->get_item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_right_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_right_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->get_item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

namespace {

/**
 * Swaps an item in a grid for another one.*/
void swap_grid(tgrid* grid,
		tgrid* content_grid, twidget* widget, const std::string& id)
{
	assert(content_grid);
	assert(widget);

	// Make sure the new child has same id.
	widget->set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = NULL;
	if(grid) {
		parent_grid = find_widget<tgrid>(grid, id, false, false);
	}
	if(!parent_grid) {
		parent_grid = find_widget<tgrid>(content_grid, id, true, false);
	}
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	assert(parent_grid);

	// Replace the child.
	widget = parent_grid->swap_child(id, widget, false);
	assert(widget);

	delete widget;
}

} // namespace

void tlistbox::finalize(
		tbuilder_grid_const_ptr header,
		tbuilder_grid_const_ptr footer,
		const std::vector<string_map>& list_data)
{
	// "Inherited."
	tscrollbar_container::finalize_setup();

	assert(generator_);

	if(header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if(footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	generator_->create_items(
			-1, list_builder_, list_data, callback_list_item_clicked);
	swap_grid(NULL, content_grid(), generator_, "_list_grid");

}

void tlistbox::set_content_size(const tpoint& origin, const tpoint& size)
{
	/** @todo This function needs more testing. */
	assert(content_grid());

	const int best_height = content_grid()->get_best_size().y;
	const tpoint s(size.x, size.y < best_height ? size.y : best_height);
	content_grid()->set_size(origin, s);
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}
} // namespace gui2

