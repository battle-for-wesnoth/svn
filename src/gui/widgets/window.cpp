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
#include "log.hpp"
#include "tstring.hpp"
#include "video.hpp"

#include <cassert>

#define DBG_G LOG_STREAM_INDENT(debug, gui)
#define LOG_G LOG_STREAM_INDENT(info, gui)
#define WRN_G LOG_STREAM_INDENT(warn, gui)
#define ERR_G LOG_STREAM_INDENT(err, gui)

#define DBG_G_D LOG_STREAM_INDENT(debug, gui_draw)
#define LOG_G_D LOG_STREAM_INDENT(info, gui_draw)
#define WRN_G_D LOG_STREAM_INDENT(warn, gui_draw)
#define ERR_G_D LOG_STREAM_INDENT(err, gui_draw)

#define DBG_G_E LOG_STREAM_INDENT(debug, gui_event)
#define LOG_G_E LOG_STREAM_INDENT(info, gui_event)
#define WRN_G_E LOG_STREAM_INDENT(warn, gui_event)
#define ERR_G_E LOG_STREAM_INDENT(err, gui_event)

#define DBG_G_P LOG_STREAM_INDENT(debug, gui_parse)
#define LOG_G_P LOG_STREAM_INDENT(info, gui_parse)
#define WRN_G_P LOG_STREAM_INDENT(warn, gui_parse)
#define ERR_G_P LOG_STREAM_INDENT(err, gui_parse)


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
		const std::string& definition) :
	tpanel(),
	tevent_handler(),
	video_(video),
	status_(NEW),
	retval_(0),
	owner_(0),
	need_layout_(true),
	resized_(true),
	suspend_drawing_(true),
	top_level_(false),
	window_(),
	restorer_(),
	tooltip_(),
	help_popup_(),
	automatic_placement_(automatic_placement),
	horizontal_placement_(horizontal_placement),
	vertical_placement_(vertical_placement),
	x_(x),
	y_(y),
	w_(w),
	h_(h)
{
	// We load the config in here as exception.
	set_definition(definition);
	load_config();

	tooltip_.set_definition("default");
	tooltip_.set_visible(false);

	help_popup_.set_definition("default");
	help_popup_.set_visible(false);
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
	} else {
		return NONE;
	}
}

int twindow::show(const bool restore, void* /*flip_function*/)
{
	log_scope2(gui_draw, "Window: show.");	

	assert(status_ == NEW);

	top_level_ = (draw_interval == 0);
	if(top_level_) {
		draw_interval = 30;
		SDL_AddTimer(draw_interval, draw_timer, NULL);
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
		return;
	}

	surface frame_buffer = get_video_surface();

	const bool full_redraw = resized_ || need_layout_;
	
	if(resized_) {
		// Restore old surface.
		if(restorer_) {
			SDL_Rect rect = get_rect();
			SDL_BlitSurface(restorer_, 0, frame_buffer, &rect);
		}

		layout();
		
		// Get new surface
		SDL_Rect rect = get_rect();
		restorer_ = get_surface_portion(video_.getSurface(), rect);
		window_ = make_neutral_surface(restorer_); // should be copy surface...

		resized_ = false;
	}
	assert(window_ && restorer_);

	if(need_layout_) {
		layout();
	}

	if(full_redraw) {
		canvas(0).draw();
		blit_surface(canvas(0).surf(), 0, window_, 0);
	}		

	for(tgrid::iterator itor = begin(); itor != end(); ++itor) {
		if(! *itor || !itor->is_dirty()) {
			continue;
		}

		log_scope2(gui_draw, "Window: draw child.");

		itor->draw(window_, false, false);
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

void twindow::layout()
{
	if(automatic_placement_) {
		
		log_scope2(gui, "Window: Recalculate size");	

		tpoint size = get_best_size(); 
		DBG_G << "Window best size : " << size << " screen size " 
			<< settings::screen_width << ',' << settings::screen_height << ".\n";

		// If too big try it gracefully.
		if(size.x > settings::screen_width 
				|| size.y > settings::screen_height) {

			size = get_best_size(
				tpoint(settings::screen_width, settings::screen_height));
			DBG_G << "Window best size : After resize request : " << size << ".\n";
		}
		// If still to big, just resize.
		size.x = size.x < static_cast<int>(settings::screen_width) 
			? size.x : static_cast<int>(settings::screen_width);

		size.y = size.y < static_cast<int>(settings::screen_height) 
			? size.y : static_cast<int>(settings::screen_height);

		DBG_G << "Window final size " << size << ".\n";

		tpoint position(0, 0);
		switch(horizontal_placement_) {
			case tgrid::HORIZONTAL_ALIGN_LEFT :
				// Do nothing
				break;
			case tgrid::HORIZONTAL_ALIGN_CENTER :
				position.x = (settings::screen_width - size.x) / 2;
				break;
			case tgrid::HORIZONTAL_ALIGN_RIGHT :
				position.x = settings::screen_width - size.x;
				break;
			default :
				assert(false);
		}
		switch(vertical_placement_) {
			case tgrid::VERTICAL_ALIGN_TOP :
				// Do nothing
				break;
			case tgrid::VERTICAL_ALIGN_CENTER :
				position.y = (settings::screen_height - size.y) / 2;
				break;
			case tgrid::VERTICAL_ALIGN_BOTTOM :
				position.y = settings::screen_height - size.y;
				break;
			default :
				assert(false);
		}

		set_size(create_rect(position, size));
	} else {
		game_logic::map_formula_callable variables;
		variables.add("screen_width", variant(settings::screen_width));
		variables.add("screen_height", variant(settings::screen_height));

		set_size(::create_rect(
			x_(variables), y_(variables), w_(variables), h_(variables)));
	}

	need_layout_ = false;
}

void twindow::do_show_tooltip(const tpoint& location, const t_string& tooltip)
{
	DBG_G << "Showing tooltip message: '" << tooltip << "'.\n";

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

	tooltip_.set_size(tooltip_rect);
	tooltip_.set_visible();
}

void twindow::do_show_help_popup(const tpoint& location, const t_string& help_popup)
{
	// Note copy past of twindow::do_show_tooltip except that the help may be empty.
	DBG_G << "Showing help message: '" << help_popup << "'.\n";

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

	help_popup_.set_size(help_popup_rect);
	help_popup_.set_visible();
}

void twindow::draw(surface& /*surf*/, const bool /*force*/, 
		const bool /*invalidate_background*/)
{
	assert(false);
}

} // namespace gui2

