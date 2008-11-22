/* $Id$ */
/*
   Copyright (C) 2007 - 2008 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file window.cpp
 *  Implementation of window.hpp.
 */

#include "gui/widgets/window.hpp"

#include "cursor.hpp"
#include "font.hpp"
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
#include "gui/widgets/debug.hpp"
#endif
#include "preferences.hpp"
#include "titlescreen.hpp"
#include "video.hpp"


namespace gui2{

namespace {

/**
 * The interval between draw events.
 *
 * When the window is shown this value is set, the callback function always
 * uses this value instead of the parameter send, that way the window can stop
 * drawing when it wants.
 */
static int draw_interval = 0;

/**
 * SDL_AddTimer() callback for the draw event.
 *
 * When this callback is called it pushes a new draw event in the event queue.
 *
 * @returns                       The new timer interval, 0 to stop.
 */
static Uint32 draw_timer(Uint32, void*)
{
	DBG_G_E << "Pushing draw event in queue.\n";

	SDL_Event event;
	SDL_UserEvent data;

	data.type = DRAW_EVENT;
	data.code = 0;
	data.data1 = NULL;
	data.data2 = NULL;

	event.type = DRAW_EVENT;
	event.user = data;
	
	SDL_PushEvent(&event);
	return draw_interval;
}

} // namespace

twindow::twindow(CVideo& video, 
		tformula<unsigned>x,
		tformula<unsigned>y,
		tformula<unsigned>w,
		tformula<unsigned>h,
		const bool automatic_placement, 
		const unsigned horizontal_placement,
		const unsigned vertical_placement,
		const std::string& definition)
	: tpanel()
	, tevent_handler()
	, video_(video)
	, status_(NEW)
	, retval_(0)
	, owner_(0)
	, need_layout_(true)
	, resized_(true)
	, suspend_drawing_(true)
	, top_level_(false)
	, window_()
	, restorer_()
	, tooltip_()
	, help_popup_()
	, automatic_placement_(automatic_placement)
	, horizontal_placement_(horizontal_placement)
	, vertical_placement_(vertical_placement)
	, x_(x)
	, y_(y)
	, w_(w)
	, h_(h)
	, easy_close_(false)
	, easy_close_blocker_()
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
	, debug_layout_(new tdebug_layout_graph(this))
#endif
{
	// We load the config in here as exception.
	// Our caller did update the screen size so no need for us to do that again.
	set_definition(definition);
	load_config();

	tooltip_.set_definition("default");
	tooltip_.set_parent(this);
	tooltip_.set_visible(false);

	help_popup_.set_definition("default");
	help_popup_.set_parent(this);
	help_popup_.set_visible(false);
}

void twindow::update_screen_size()
{
	// Only if we're the toplevel window we need to update the size, otherwise
	// it's done in the resize event.
	if(draw_interval == 0) {
		const SDL_Rect rect = screen_area();
		settings::screen_width = rect.w;
		settings::screen_height = rect.h;
	}
}

twindow::tretval twindow::get_retval_by_id(const std::string& id)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_window_2
 *
 * List if the id's that have generate a return value:
 * * ok confirms the dialog.
 * * cancel cancels the dialog.
 *
 */
	// Note it might change to a map later depending on the number
	// of items.
	if(id == "ok") {
		return OK;
	} else if(id == "cancel") {
		return CANCEL;
	
	/**
	 * The ones for the title screen.
	 *
	 * This is a kind of hack, but the values are hardcoded in the titlescreen
	 * and don't want to change them at the moment. It would be a good idea to
	 * add some namespaces to avoid names clashing.
	 */
	} else if(id == "tutorial") {
		return static_cast<tretval>(gui::TUTORIAL);
#ifndef DISABLE_EDITOR2		
	} else if(id == "editor") {
		return static_cast<tretval>(gui::START_MAP_EDITOR);
#endif		
	} else if(id == "credits") {
		return static_cast<tretval>(gui::SHOW_ABOUT);
	} else if(id == "quit") {
		return static_cast<tretval>(gui::QUIT_GAME);

	/**
	 * The hacks which are here so the old engine can handle the event. The new
	 * engine can't handle all dialogs yet, so it needs to fall back to the old
	 * engine to make certain things happen.
	 */
	} else if(id == "campaign") { 
		return static_cast<tretval>(gui::NEW_CAMPAIGN);
	} else if(id == "multiplayer") { 
		return static_cast<tretval>(gui::MULTIPLAYER);
	} else if(id == "load") { 
		return static_cast<tretval>(gui::LOAD_GAME);
	} else if(id == "addons") { 
		return static_cast<tretval>(gui::GET_ADDONS);
	} else if(id == "preferences") { 
		return static_cast<tretval>(gui::EDIT_PREFERENCES);

	// default if nothing matched
	} else {
		return NONE;
	}
}

int twindow::show(const bool restore, void* /*flip_function*/)
{
	log_scope2(gui_draw, "Window: show.");

	generate_dot_file("show");

	assert(status_ == NEW);

	top_level_ = (draw_interval == 0);
	if(top_level_) {
		draw_interval = 30;
		SDL_AddTimer(draw_interval, draw_timer, NULL);

		// There might be some time between creation and showing so reupdate
		// the sizes.
		update_screen_size();
	}

	suspend_drawing_ = false;

	// Start our loop drawing will happen here as well.
	for(status_ = SHOWING; status_ != REQUEST_CLOSE; ) {
		process_events();
		// Add a delay so we don't keep spinning if there's no event.
		SDL_Delay(10);
	}

	suspend_drawing_ = true;

	if(top_level_) {
		draw_interval = 0;
	}

	// restore area
	if(restore) {
		SDL_Rect rect = get_rect();
		SDL_BlitSurface(restorer_, 0, video_.getSurface(), &rect);
		update_rect(get_rect());
	}

	return retval_;
}

void twindow::draw()
{
	// NOTE since we're single threaded there's no need to create a critical
	// section in this drawing routine. 

	// Prohibited from drawing?
	if(suspend_drawing_) {
		return;
	}

	// Drawing not required?
	if(!resized_ && !need_layout_ && !is_dirty()) {
		// If we have colour cursor we're responsible for the updates so do
		// them otherwise the mouse move won't be visible until something else
		// dirties the screen.
		if(preferences::use_colour_cursors()) {
			surface frame_buffer = get_video_surface();

			cursor::draw(frame_buffer);
			video_.flip();
			cursor::undraw(frame_buffer);
		}
		return;
	}

	surface frame_buffer = get_video_surface();

	/**
	 * @todo It seems resized_ and need_layout_ need to do exactly the same so
	 * maybe remove the resized_ flag. Wait for some testing.
	 */

	const bool full_redraw = resized_ || need_layout_;

	if(resized_ || need_layout_) {
		// Restore old surface.
		if(restorer_) {
			SDL_Rect rect = get_rect();
			SDL_BlitSurface(restorer_, 0, frame_buffer, &rect);
			// Since the old area might be bigger as the new one, invalidate it.
			update_rect(rect);
		}

		layout();
		
		// Get new surface
		SDL_Rect rect = get_rect();
		restorer_ = get_surface_portion(video_.getSurface(), rect);
		window_ = make_neutral_surface(restorer_); // should be copy surface...

		resized_ = false;
	}
	assert(window_ && restorer_);

	if(full_redraw) {
		canvas(0).draw();
		blit_surface(canvas(0).surf(), 0, window_, 0);
	}		

	for(tgrid::iterator itor = begin(); itor != end(); ++itor) {
		if(! *itor || !itor->is_dirty()) {
			continue;
		}

		log_scope2(gui_draw, "Window: draw child.");

		itor->draw(window_, full_redraw, full_redraw);
	}

	if(full_redraw) {
		canvas(1).draw();
		blit_surface(canvas(1).surf(), 0, window_, 0);
	}		

	if(tooltip_.is_dirty()) {
		tooltip_.draw(window_);
	}

	if(help_popup_.is_dirty()) {
		help_popup_.draw(window_);
	}

	// Floating label hack
	font::draw_floating_labels(frame_buffer);

	SDL_Rect rect = get_rect();
	SDL_BlitSurface(window_, 0, frame_buffer, &rect);
	update_rect(get_rect());
	set_dirty(false);

	cursor::draw(frame_buffer);
	video_.flip();
	cursor::undraw(frame_buffer);
	// Floating hack part 2.
	font::undraw_floating_labels(frame_buffer);
}

void twindow::window_resize(tevent_handler&, 
		const unsigned new_width, const unsigned new_height)
{
	settings::screen_width = new_width;
	settings::screen_height = new_height;
	resized_ = true;
}

void twindow::key_press(tevent_handler& /*event_handler*/, bool& handled, 
		SDLKey key, SDLMod /*modifier*/, Uint16 /*unicode*/)
{
	if(key == SDLK_KP_ENTER || key == SDLK_RETURN) {
		set_retval(OK);
		handled = true;
	} else if(key == SDLK_ESCAPE) {
		set_retval(CANCEL);
		handled = true;
	}
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
	if(key == SDLK_F12) {
		debug_layout_->generate_dot_file("manual");
		handled = true;
	}
#endif	
}

SDL_Rect twindow::get_client_rect() const
{
	boost::intrusive_ptr<const twindow_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const twindow_definition::tresolution>(config());
	assert(conf);

	SDL_Rect result = get_rect();
	result.x = conf->left_border;
	result.y = conf->top_border;
	result.w -= conf->left_border + conf->right_border;
	result.h -= conf->top_border + conf->bottom_border;

	// FIXME validate for an available client area.
	
	return result;

}
void twindow::add_easy_close_blocker(const std::string& id)
{
	// avoid duplicates.
	remove_easy_close_blocker(id);

	easy_close_blocker_.push_back(id);
}

void twindow::remove_easy_close_blocker(const std::string& id)
{
	easy_close_blocker_.erase(
		std::remove(easy_close_blocker_.begin(), easy_close_blocker_.end(), id), 
		easy_close_blocker_.end());
}

void twindow::layout()
{
	/**** Initialize and get initial size. *****/

	boost::intrusive_ptr<const twindow_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const twindow_definition::tresolution>
		(config());
	assert(conf);

	log_scope2(gui_layout, "Window: Recalculate size");	

	layout_init();
	generate_dot_file("layout_init");

	const game_logic::map_formula_callable variables = 
		get_screen_size_variables(); 

	const int maximum_width = automatic_placement_ ?
			settings::screen_width :  w_(variables);

	const int maximum_height = automatic_placement_ ?
			settings::screen_height :  h_(variables);

	tpoint size = get_best_size();
	generate_dot_file("get_initial_best_size");

	DBG_G_L << "twindow " << __func__ << ": " << size << " maximum size " 
			<< maximum_width << ',' << maximum_height << ".\n";

	/***** Does the width fit in the available width? *****/
	
	// *** wrap (can change height)
	if(size.x > maximum_width && can_wrap()) {
		layout_wrap(maximum_width);
		size = get_best_size();
		generate_dot_file("wrapped");
	}

	// *** scrollbar (leaves height untouched)
	if(size.x > maximum_width && has_horizontal_scrollbar()) {
		layout_use_horizontal_scrollbar(maximum_width);
		size = get_best_size();
		generate_dot_file("horizontal_scrollbar");
	}

	// *** shrink (can change height)
	if(size.x > maximum_width) {
		layout_shrink_width(maximum_width);
		size = get_best_size();
		generate_dot_file("shrink_width");
	}

	// *** failed?
	if(size.x > maximum_width) {
		ERR_G_L << "Failed to resize window, wanted width "
			<< size.x << " available width "
			<< maximum_width << ".\n";
		assert(false);
	}

	/***** Does the height fit in the available height? ******/

	// *** scrollbar (leaves width untouched)
	if(size.y > maximum_height && has_vertical_scrollbar()) {
		layout_use_vertical_scrollbar(maximum_height);
		size = get_best_size();
		generate_dot_file("vertical_scrollbar");
	}

	// *** shrink (can change width)
	if(size.y > maximum_height) {
		layout_shrink_height(maximum_height);
		size = get_best_size();
		generate_dot_file("shrink_height");
	}

	// *** failed?
	if(size.y > maximum_height) {
		ERR_G_L << "Failed to resize window, wanted height "
			<< size.y << " available height "
			<< maximum_height << ".\n";
		assert(false);
	}

	/***** Get the best location for the window *****/

	tpoint origin(0, 0);

	if(automatic_placement_) {
		
		switch(horizontal_placement_) {
			case tgrid::HORIZONTAL_ALIGN_LEFT :
				// Do nothing
				break;
			case tgrid::HORIZONTAL_ALIGN_CENTER :
				origin.x = (settings::screen_width - size.x) / 2;
				break;
			case tgrid::HORIZONTAL_ALIGN_RIGHT :
				origin.x = settings::screen_width - size.x;
				break;
			default :
				assert(false);
		}
		switch(vertical_placement_) {
			case tgrid::VERTICAL_ALIGN_TOP :
				// Do nothing
				break;
			case tgrid::VERTICAL_ALIGN_CENTER :
				origin.y = (settings::screen_height - size.y) / 2;
				break;
			case tgrid::VERTICAL_ALIGN_BOTTOM :
				origin.y = settings::screen_height - size.y;
				break;
			default :
				assert(false);
		}
	} else {
		origin.x = x_(variables);
		origin.y = y_(variables);

		size.x = w_(variables);
		size.y = h_(variables);
	}

	/***** Set the window size *****/
	set_size(origin, size);

	generate_dot_file("layout_finished");
	need_layout_ = false;
}

void twindow::do_show_tooltip(const tpoint& location, const t_string& tooltip)
{
	DBG_GUI << "Showing tooltip message: '" << tooltip << "'.\n";

	assert(!tooltip.empty());

	twidget* widget = find_widget(location, true);
	assert(widget);
	
	const SDL_Rect widget_rect = widget->get_rect();
	const SDL_Rect client_rect = get_client_rect();

	tooltip_.set_label(tooltip);
	const tpoint size = tooltip_.get_best_size();

	SDL_Rect tooltip_rect = {0, 0, size.x, size.y};

	// Find the best position to place the widget
	if(widget_rect.y - size.y > 0) {
		// put above
		tooltip_rect.y = widget_rect.y - size.y;
	} else {
		//put below no test
		tooltip_rect.y = widget_rect.y + widget_rect.h;
	}

	if(widget_rect.x + size.x < client_rect.w) {
		// Directly above the mouse
		tooltip_rect.x = widget_rect.x;
	} else {
		// shift left, no test
		tooltip_rect.x = client_rect.w - size.x;
	}

	tooltip_.set_size(
			tpoint(tooltip_rect.x, tooltip_rect.y), 
			tpoint(tooltip_rect.w, tooltip_rect.h));
	tooltip_.set_visible();
}

void twindow::do_show_help_popup(const tpoint& location, const t_string& help_popup)
{
	// Note copy past of twindow::do_show_tooltip except that the help may be empty.
	DBG_GUI << "Showing help message: '" << help_popup << "'.\n";

	if(help_popup.empty()) {
		return;
	}
	twidget* widget = find_widget(location, true);
	assert(widget);
	
	const SDL_Rect widget_rect = widget->get_rect();
	const SDL_Rect client_rect = get_client_rect();

	help_popup_.set_label(help_popup);
	const tpoint size = help_popup_.get_best_size();

	SDL_Rect help_popup_rect = {0, 0, size.x, size.y};

	// Find the best position to place the widget
	if(widget_rect.y - size.y > 0) {
		// put above
		help_popup_rect.y = widget_rect.y - size.y;
	} else {
		//put below no test
		help_popup_rect.y = widget_rect.y + widget_rect.h;
	}

	if(widget_rect.x + size.x < client_rect.w) {
		// Directly above the mouse
		help_popup_rect.x = widget_rect.x;
	} else {
		// shift left, no test
		help_popup_rect.x = client_rect.w - size.x;
	}

	help_popup_.set_size(
			tpoint(help_popup_rect.w, help_popup_rect.h), 
			tpoint(help_popup_rect.x, help_popup_rect.y));
	help_popup_.set_visible();
}

void twindow::easy_close()
{
	if(easy_close_ && easy_close_blocker_.empty()) {
		set_retval(OK);
	}
}

void twindow::draw(surface& /*surf*/, const bool /*force*/, 
		const bool /*invalidate_background*/)
{
	assert(false);
}

#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
twindow::~twindow() 
{
	delete debug_layout_; 
}

void twindow::generate_dot_file(const std::string& generator)
{
	debug_layout_->generate_dot_file(generator);
}
#endif	
} // namespace gui2

