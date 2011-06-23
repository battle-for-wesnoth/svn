/* $Id$ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/dialog.hpp"

#include "foreach.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/widgets/integer_selector.hpp"
#include "video.hpp"

namespace gui2 {

tdialog::~tdialog()
{
	foreach(tfield_* field, fields_) {
		delete field;
	}
}

bool tdialog::show(CVideo& video, const unsigned auto_close_time)
{
	if(video.faked()) {
		return false;
	}

	std::auto_ptr<twindow> window(build_window(video));
	assert(window.get());

	post_build(video, *window);

	window->set_owner(this);

	init_fields(*window);

	pre_show(video, *window);

	retval_ = window->show(restore_, auto_close_time);

	/*
	 * It can happen that when two clicks follow eachother fast that the event
	 * handling code in events.cpp generates a DOUBLE_CLICK_EVENT. For some
	 * reason it can happen that this event gets pushed in the queue when the
	 * window is shown, but processed after the window is closed. This causes
	 * the next window to get this pending event.
	 *
	 * This caused a bug where double clicking in the campaign selection dialog
	 * directly selected a difficulty level and started the campaign. In order
	 * to avoid that problem, filter all pending DOUBLE_CLICK_EVENT events after
	 * the window is closed.
	 */
	events::discard(SDL_EVENTMASK(DOUBLE_CLICK_EVENT));

	finalize_fields(*window, (retval_ ==  twindow::OK || always_save_fields_));

	post_show(*window);

	return retval_ == twindow::OK;
}

tfield_bool* tdialog::register_bool(
		  const std::string& id
		, const bool mandatory
		, bool (*callback_load_value) ()
		, void (*callback_save_value) (const bool value)
		, void (*callback_change) (twidget* widget))
{
	tfield_bool* field =  new tfield_bool(
			  id
			, mandatory
			, callback_load_value
			, callback_save_value, callback_change);

	fields_.push_back(field);
	return field;
}

tfield_bool* tdialog::register_bool(const std::string& id
		, const bool mandatory
		, bool& linked_variable
		, void (*callback_change) (twidget* widget))
{
	tfield_bool* field =  new tfield_bool(
			  id
			, mandatory
			, linked_variable
			, callback_change);

	fields_.push_back(field);
	return field;
}

tfield_bool* tdialog::register_bool2(const std::string& id
		, const bool mandatory
		, bool& linked_variable
		, void (*callback_change) (twidget* widget))
{
	return register_bool(id, mandatory, linked_variable, callback_change);
}

tfield_integer* tdialog::register_integer(
		  const std::string& id
		, const bool mandatory
		, int (*callback_load_value) ()
		, void (*callback_save_value) (const int value))
{
	tfield_integer* field =  new tfield_integer(
			  id
			, tunused_parameter()
			, mandatory
			, callback_load_value
			, callback_save_value);

	fields_.push_back(field);
	return field;
}

tfield_integer* tdialog::register_integer(const std::string& id
		, const bool mandatory
		, int& linked_variable)
{
	tfield_integer* field =  new tfield_integer(
			  id
			, tunused_parameter()
			, mandatory
			, linked_variable);

	fields_.push_back(field);
	return field;
}

tfield_text* tdialog::register_text(
		  const std::string& id
		, const bool mandatory
		, std::string (*callback_load_value) ()
		, void (*callback_save_value) (const std::string& value))
{
	tfield_text* field =  new tfield_text(
			  id
			, mandatory
			, callback_load_value
			, callback_save_value);

	fields_.push_back(field);
	return field;
}

tfield_text* tdialog::register_text(const std::string& id
		, const bool mandatory
		, std::string& linked_variable)
{
	tfield_text* field =  new tfield_text(
			  id
			, mandatory
			, linked_variable);

	fields_.push_back(field);
	return field;
}

tfield_text* tdialog::register_text2(const std::string& id
			, const bool mandatory
			, std::string& linked_variable
			, const bool capture_focus)
{
	assert(!capture_focus || focus_.empty());

	if(capture_focus) {
		focus_ = id;
	}

	return tdialog::register_text(id, mandatory, linked_variable);
}

tfield_text* tdialog::register_text2(const std::string& id
		, const bool mandatory
		, std::string (*callback_load_value) ()
		, void (*callback_save_value) (const std::string& value)
		, const bool capture_focus)
{
	assert(!capture_focus || focus_.empty());

	if(capture_focus) {
		focus_ = id;
	}

	return tdialog::register_text(id
			, mandatory
			, callback_load_value
			, callback_save_value);
}

tfield_label* tdialog::register_label2(const std::string& id
		, const bool mandatory
		, const std::string& text
		, const bool use_markup)
{
	tfield_label* field =  new tfield_label(
			  id
			, mandatory
			, text
			, use_markup);

	fields_.push_back(field);
	return field;
}

twindow* tdialog::build_window(CVideo& video) const
{
	return build(video, window_id());
}

void tdialog::init_fields(twindow& window)
{
	foreach(tfield_* field, fields_) {
		field->attach_to_window(window);
		field->widget_init(window);
	}

	if(!focus_.empty()) {
		if(twidget* widget = window.find(focus_, false)) {
			window.keyboard_capture(widget);
		}
	}
}

void tdialog::finalize_fields(twindow& window, const bool save_fields)
{
	foreach(tfield_* field, fields_) {
		if(save_fields) {
			field->widget_finalize(window);
		}
		field->detach_from_window();
	}
}

} // namespace gui2


/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 1
 *
 * {{Autogenerated}}
 *
 * = Window definition =
 *
 * The window definition define how the windows shown in the dialog look.
 */

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 */

