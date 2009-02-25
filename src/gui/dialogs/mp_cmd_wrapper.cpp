/* $Id$ */
/*
   Copyright (C) 2009 by Thomas Baumhauer <thomas.baumhauer@NOSPAMgmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/dialogs/mp_cmd_wrapper.hpp"

#include "gui/widgets/button.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/widgets/label.hpp"

#include "multiplayer_ui.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowWML
 * @order = 2_mp_cmd_wrapper
 *
 * == Multiplayer command wrapper ==
 *
 * This shows a dialog that provides a graphical front-end
 * to various commands in the multiplayer lobby
 *
 * @start_table = container
 *     message		(text_box)  Text to send as a private message
 *     reason		(text_box)  The reason for a ban
 *     time		(text_box)  The time the ban lasts
 *     [send_message]	(button)    Execute /msg
 *     [add_friend]	(button)    Execute /friend
 *     [add_ignore]	(button)    Execute /ignore
 *     [remove]		(button)    Execute /remove
 *     [status]		(button)    Execute /query status
 *     [kick]		(button)    Execute /query kick
 *     [ban]		(button)    Execute /query kban
 * @end_table
 */

tmp_cmd_wrapper::tmp_cmd_wrapper(const t_string& user) :
		message_(), reason_(), time_(), user_(user) { }

twindow* tmp_cmd_wrapper::build_window(CVideo& video)
{
	return build(video, get_id(MP_CMD_WRAPPER));
}

void tmp_cmd_wrapper::pre_show(CVideo& /*video*/, twindow& window)
{
	ttext_box* message =
		dynamic_cast<ttext_box*>(window.find_widget("message", false));
	if(message) window.keyboard_capture(message);

	message = dynamic_cast<ttext_box*>(window.find_widget("reason", false));
	if(message) message->set_active(mp::authenticated);

	message = dynamic_cast<ttext_box*>(window.find_widget("time", false));
	if(message) message->set_active(mp::authenticated);

	tlabel* label =
		dynamic_cast<tlabel*>(window.find_widget("user_label", false));
	if(label) label->set_label(user_);


	tbutton* b = dynamic_cast<tbutton*>(window.find_widget("add_friend", false));
	if(b) b->set_retval(1);

	b = dynamic_cast<tbutton*>(window.find_widget("add_ignore", false));
	if(b) b->set_retval(2);

	b = dynamic_cast<tbutton*>(window.find_widget("remove", false));
	if(b) b->set_retval(3);

	b = dynamic_cast<tbutton*>(window.find_widget("status", false));
	if(b) {
		b->set_retval(4);
		b->set_active(mp::authenticated);
	}

	b = dynamic_cast<tbutton*>(window.find_widget("kick", false));
	if(b) {
		b->set_retval(5);
		b->set_active(mp::authenticated);
	}

	b = dynamic_cast<tbutton*>(window.find_widget("ban", false));
	if(b) {
		b->set_retval(6);
		b->set_active(mp::authenticated);
	}

}

void tmp_cmd_wrapper::post_show(twindow& window)
{
	ttext_box* message =
		dynamic_cast<ttext_box*>(window.find_widget("message", false));
	message_ = message ? message_ = message->get_value() : "";

	ttext_box* reason =
		dynamic_cast<ttext_box*>(window.find_widget("reason", false));
	reason_ = reason ? reason_ = reason->get_value() : "";

	ttext_box* time =
		dynamic_cast<ttext_box*>(window.find_widget("time", false));
	time_ = time ? time_ = time->get_value() : "";

}

} // namespace gui2

