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

#include "gui/dialogs/editor_generate_map.hpp"

#include "gui/dialogs/helper.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/window_builder.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "log.hpp"
#include "mapgen.hpp"
#include "wml_exception.hpp"
#define DBG_GUI LOG_STREAM_INDENT(debug, gui)
#define LOG_GUI LOG_STREAM_INDENT(info, gui)
#define WRN_GUI LOG_STREAM_INDENT(warn, gui)
#define ERR_GUI LOG_STREAM_INDENT(err, gui)
#define ERR_ED LOG_STREAM_INDENT(err, editor)

namespace gui2 {

teditor_generate_map::teditor_generate_map()
: map_generator_(NULL), gui_(NULL)
{
}

void teditor_generate_map::do_settings(twindow& window)
{
	if (map_generator_->allow_user_config()) {
		map_generator_->user_config(*gui_);
	}
}

twindow teditor_generate_map::build_window(CVideo& video)
{
	return build(video, get_id(EDITOR_GENERATE_MAP));
}

void teditor_generate_map::pre_show(CVideo& /*video*/, twindow& window)
{
	assert(map_generator_);
	assert(gui_);
	tbutton& settings_button = window.get_widget<tbutton>("settings", false);
	settings_button.set_callback_mouse_left_click(
		dialog_callback<teditor_generate_map, &teditor_generate_map::do_settings>);
}

void teditor_generate_map::post_show(twindow& /*window*/)
{
}

} // namespace gui2
