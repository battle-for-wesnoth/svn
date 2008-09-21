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

#include "gui/dialogs/message.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/window_builder.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "log.hpp"

#define DBG_GUI LOG_STREAM_INDENT(debug, gui)
#define LOG_GUI LOG_STREAM_INDENT(info, gui)
#define WRN_GUI LOG_STREAM_INDENT(warn, gui)
#define ERR_GUI LOG_STREAM_INDENT(err, gui)

namespace gui2 {

twindow tmessage::build_window(CVideo& video)
{
	return build(video, get_id(MESSAGE));
}

void tmessage::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* title = 
		dynamic_cast<tlabel*>(window.find_widget("title", false));
	VALIDATE(title, missing_widget("title"));

	title->set_label(title_);

	tcontrol* label = 
		dynamic_cast<tcontrol*>(window.find_widget("label", false));
	VALIDATE(label, missing_widget("label"));

	label->set_label(message_);
}

/** @todo the caption is ignored. */
void show_message(CVideo& video, const std::string& title, 
	const std::string& message, const std::string& /*button_caption*/)
{
	tmessage(title, message).show(video);
}

} // namespace gui2

