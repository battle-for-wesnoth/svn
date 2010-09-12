/* $Id$ */
/*
   Copyright (C) 2008 - 2010 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/title_screen.hpp"

#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/addon_connect.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/multi_page.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "titlescreen.hpp"

#include <boost/bind.hpp>

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

namespace gui2 {

namespace {

template<class D>
void show_dialog(CVideo& video)
{
	D dlg;
	dlg.show(video);
}

void show_language_dialog(CVideo& video, twindow& window)
{
	window.set_retval(static_cast<twindow::tretval>(gui::CHANGE_LANGUAGE));
	return;
 	//NOTE following code doesn't redraw the titlescreen, reload tips and images
	tlanguage_selection dlg;
	dlg.show(video);
	if(dlg.get_retval() == twindow::OK) {
		/*
		 * This call both reloads all texts in the new translation for the
		 * widgets and then finds the new best layout in the new language.
		 */
		window.invalidate_layout();
	}
}

} // namespace

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen. This dialog is still under construction and
 * is only shown when --new-widgets are used.
 *
 * @begin{table}[dialog_widgets]
 * addons & & button & m &
 *         The button to get the addons. $
 *
 * language & & button & m &
 *         The button to change the language. $
 *
 * tips & & multi_page & o &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         The tip of the day. $
 *
 * -source & & label & o &
 *         The source for the tip of the day. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the image. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number. $
 *
 * @end{table}
 */

REGISTER_WINDOW(title_screen)

static bool hotkey(twindow& window, const gui::TITLE_RESULT title_result)
{
	window.set_retval(static_cast<twindow::tretval>(title_result));

	return true;
}

ttitle_screen::ttitle_screen()
	: video_(NULL)
	, tips_()
	, logo_timer_id_(0)
{
	read_tips_of_day(tips_);
}

ttitle_screen::~ttitle_screen()
{
	if(logo_timer_id_) {
		remove_timer(logo_timer_id_);
	}
}
static void animate_logo(
		  unsigned long& timer_id
		, unsigned& percentage
		, tprogress_bar& progress_bar
		, twindow& window)
{
	assert(percentage <= 100);
	++percentage;
	progress_bar.set_percentage(percentage);

	/*
	 * The progress bar may overlap (actually underlap) other widgets, which
	 * the update invalidates, so make sure the whole window is redrawn to fix
	 * this possible problem. Of course this is expensive but the logo is
	 * animated once so the cost is only once.
	 */
	window.set_dirty();


	if(percentage == 100) {
		remove_timer(timer_id);
		timer_id = 0;
	}
}

void ttitle_screen::post_build(CVideo&, twindow& window)
{
	/** @todo Should become a title screen hotkey. */
	window.register_hotkey(hotkey::TITLE_SCREEN__RELOAD_WML
				, boost::bind(
					  &hotkey
					, boost::ref(window)
					, gui::RELOAD_GAME_DATA));
}

void ttitle_screen::pre_show(CVideo& video, twindow& window)
{
	assert(!video_);
	video_ = &video;

	set_restore(false);
	window.set_click_dismiss(false);
	window.set_escape_disabled(true);

	/**** Set the version number ****/
	if(tcontrol* control
			= find_widget<tcontrol>(&window, "revision_number", false, false)) {

		control->set_label(_("Version ") + game_config::revision);
	}
	window.canvas()[0].set_variable("revision_number",
		variant(_("Version") + std::string(" ") + game_config::revision));

	/**** Set the buttons ****/
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "addons", false)
			, boost::bind(
				  show_dialog<gui2::taddon_connect>
				, boost::ref(video)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "language", false)
			, boost::bind(
				  show_language_dialog
				, boost::ref(video)
				, boost::ref(window)));

	/**** Set the tip of the day ****/
	/*
	 * NOTE: although the tips are set in the multi_page only the first page
	 * will be used and the widget content is set there. This is a kind of
	 * hack, but will be fixed when this dialog will be moved out of
	 * --new-widgets. Then the tips part of the code no longer needs to be
	 *  shared with the other title screen and can be moved here.
	 */
	tmulti_page* tip_pages =
			find_widget<tmulti_page>(&window, "tips", false, false);
	if(tip_pages) {
		foreach(const config& tip, tips_.child_range("tip")) {

			string_map widget;
			std::map<std::string, string_map> page;

			widget["label"] = tip["text"];
			page["tip"] = widget;

			widget["label"] = tip["source"];
			widget["use_markup"] = "true";
			page["source"] = widget;

			tip_pages->add_page(page);

		}
	}
	update_tip(window, true);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "next_tip", false)
			, boost::bind(
				  &ttitle_screen::update_tip
				, this
				, boost::ref(window)
				, true));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "previous_tip", false)
			, boost::bind(
				  &ttitle_screen::update_tip
				, this
				, boost::ref(window)
				, false));

	if(game_config::images::game_title.empty()) {
		ERR_CF << "No title image defined\n";
	} else {
		window.canvas()[0].set_variable("background_image",
			variant(game_config::images::game_title));
	}

	/***** Set the logo *****/
	tprogress_bar* logo =
			find_widget<tprogress_bar>(&window, "logo", false, false);
	if(logo) {
		static unsigned percentage = preferences::startup_effect() ? 0 : 100;
		logo->set_percentage(percentage);

		if(percentage < 100) {
			/*
			 * The interval is empirically determined  so that the speed "felt"
			 * good.
			 */
			logo_timer_id_ = add_timer(30
					, boost::bind(animate_logo
						, boost::ref(logo_timer_id_)
						, boost::ref(percentage)
						, boost::ref(*logo)
						, boost::ref(window))
					, true);
		}
	}
}

void ttitle_screen::post_show(twindow& /*window*/)
{
	video_ = NULL;
}

void ttitle_screen::update_tip(twindow& window, const bool previous)
{
	next_tip_of_day(tips_, previous);
	const config *tip = get_tip_of_day(tips_);
	assert(tip);

	find_widget<tlabel>(&window, "tip", false).set_label((*tip)["text"]);
	find_widget<tlabel>(&window, "source", false).set_label((*tip)["source"]);

	/**
	 * @todo Need to move the real tips "calculation" to this file so we can
	 * move through the pages.
	 *
	 */
	if(!find_widget<tmulti_page>(&window, "tips", false, false)) {
		window.invalidate_layout();
	}
}

void ttitle_screen::next_tip(twidget* caller)
{
	ttitle_screen *dialog = dynamic_cast<ttitle_screen*>(caller->dialog());
	assert(dialog);

	twindow *window = caller->get_window();
	assert(window);

	dialog->update_tip(*window, true);
}

void ttitle_screen::previous_tip(twidget* caller)
{
	ttitle_screen *dialog = dynamic_cast<ttitle_screen*>(caller->dialog());
	assert(dialog);

	twindow *window = caller->get_window();
	assert(window);

	dialog->update_tip(*window, false);
}

} // namespace gui2

