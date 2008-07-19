/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/dialogs/mp_method_selection.hpp"

#include "game_preferences.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/window_builder.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "log.hpp"
#include "video.hpp"
#include "wml_exception.hpp"

#define DBG_GUI LOG_STREAM_INDENT(debug, widget)
#define LOG_GUI LOG_STREAM_INDENT(info, widget)
#define WRN_GUI LOG_STREAM_INDENT(warn, widget)
#define ERR_GUI LOG_STREAM_INDENT(err, widget)

namespace gui2 {

/*WIKI
 * @page = GUIWindowWML
 * @order = 2_mp_method_selection
 *
 * == MP method selection ==
 *
 * This shows the dialog to select the kind of MP game the user wants to play.
 * 
 * @start_table = container
 *     user_name (text_box)            This text contains the name the user on 
 *                                     the MP server.
 *     method_list (listbox)           The list with possible game methods.
 * @end_table
 */
twindow tmp_method_selection::build_window(CVideo& video)
{
	return build(video, get_id(MP_METHOD_SELECTION));
}

void tmp_method_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	user_name_ = preferences::login();
	ttext_box* user_widget = dynamic_cast<ttext_box*>(window.find_widget("user_name", false));
	VALIDATE(user_widget, missing_widget("user_name"));

	user_widget->set_text(user_name_);
	window.keyboard_capture(user_widget);

	tlistbox* list = dynamic_cast<tlistbox*>(window.find_widget("method_list", false));
	VALIDATE(list, missing_widget("method_list"));

	window.add_to_keyboard_chain(list);
	window.add_to_keyboard_chain(user_widget);

	window.recalculate_size();
}

void tmp_method_selection::post_show(twindow& window)
{
	if(get_retval() == tbutton::OK) {

		ttext_box* user_widget = dynamic_cast<ttext_box*>(window.find_widget("user_name", false));
		assert(user_widget);

		tlistbox* list = dynamic_cast<tlistbox*>(window.find_widget("method_list", false));
		assert(list);

		choice_ = list->get_selected_row();
		user_widget->save_to_history();
		user_name_= user_widget->get_text();
		preferences::set_login(user_name_);
	}
}

} // namespace gui2
