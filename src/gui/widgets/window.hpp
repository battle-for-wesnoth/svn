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
 *  @file window.hpp
 *  This file contains the window object, this object is a top level container 
 *  which has the event management as well.
 */

#ifndef GUI_WIDGETS_WINDOW_HPP_INCLUDED
#define GUI_WIDGETS_WINDOW_HPP_INCLUDED

#include "gui/widgets/event_handler.hpp"
#include "gui/widgets/formula.hpp"
#include "gui/widgets/helper.hpp" 
#include "gui/widgets/panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/tooltip.hpp"

#include "events.hpp"
#include "SDL.h"

#include <string>

class CVideo;

namespace gui2{

class tdialog;

/**
 * base class of top level items, the only item 
 * which needs to store the final canvase to draw on
 */
class twindow : public tpanel, public tevent_handler
{
public:
	twindow(CVideo& video,
		tformula<unsigned>x,
		tformula<unsigned>y,
		tformula<unsigned>w,
		tformula<unsigned>h,
		const bool automatic_placement, 
		const unsigned horizontal_placement,
		const unsigned vertical_placement,
		const std::string& definition);

	/**
	 * Default return values.
	 *
	 * These values are named return values and most are assigned to a widget
	 * automatically when using a certain id for that widget. The automatic
	 * return values are always a negative number.
	 *
	 * Note this might be moved somewhere else since it will force people to
	 * include the button, while it should be and implementation detail for most
	 * callers.
	 */
	enum tretval {
		NONE = 0,                      /**< 
										* Dialog is closed with no return
										* value, should be rare but eg a
										* message popup can do it.
										*/
		OK = -1,                       /**< Dialog is closed with ok button. */
		CANCEL = -2                    /**< 
										* Dialog is closed with the cancel
										* button.
										*/
		};

	/** Gets the retval for the default buttons. */
	static tretval get_retval_by_id(const std::string& id);

	/**
	 * Shows the window.
	 *
	 * @param restore             Restore the screenarea the window was on
	 *                            after closing it?
	 * @param flip_function       If not NULL this function is used to do the
	 *                            flipping of the video buffer, otherwise
	 *                            twindow::flip() is used. @todo Evaluate
	 *                            whether this is still needed and if so
	 *                            implement.
	 *
	 * @returns                   The close code of the window, predefined
	 *                            values are listed in tretval.
	 */
	int show(const bool restore = true, void* flip_function = NULL);

	/** 
	 * Layouts the window.
	 *
	 * @todo This function has no purpose at the moment, but also the resize
	 * code doesn't work so might be required later.
	 */
//	void layout(const SDL_Rect& position);

	/** The status of the window. */
	enum tstatus{ 
		NEW,                      /**< The window is new and not yet shown. */
		SHOWING,                  /**< The window is being shown. */
		REQUEST_CLOSE,            /**< The window has been requested to be
		                           *   closed but still needs to evalueate the
								   *   request.
								   */
		CLOSED                    /**< The window has been closed. */
		};   

	/** 
	 * Requests to close the window. 
	 *
	 * At the moment the request is always honoured but that might change in the
	 * future.
	 */
	void close() { status_ = REQUEST_CLOSE; }

	/**
	 * Converts a screen coordinate to a client coordinate.
	 *
	 * @param screen_position     The screen coordinate.
	 *
	 * @returns                   The client coordinate.
	 */
	tpoint client_position(const tpoint& screen_position) const
		{ return tpoint(screen_position.x - get_x(), screen_position.y - get_y()); }

	/**
	 * Converts a client coordinate to a screen coordinate.
	 *
	 * @param client_position     The client coordinate.
	 *
	 * @returns                   The screen coordinate.
	 */
	tpoint screen_position(const tpoint& client_position) const
		{ return tpoint(client_position.x + get_x(), client_position.y + get_y()); }

	/**
	 * Resize event for the window.
	 *
	 * @param event_handler       The handler sending the event.
	 * @param new_width           The new width for the window.
	 * @param new_height          The new height for the window.
	 */
	void window_resize(tevent_handler& event_handler, 
		const unsigned new_width, const unsigned new_height);

	/** 
	 * Updates the size of the window.
	 * 
	 * If the window has automatic placement set this function recacluates the
	 * window. To be used after creation and after modification or items which
	 * can have different sizes eg listboxes.
	 */
	void recalculate_size();

	/** Inherited from tevent_executor. */
	void key_press(tevent_handler& event_handler, bool& handled, 
		SDLKey key, SDLMod modifier, Uint16 unicode);

	/** Inherited from tevent_handler. */
	twindow& get_window() { return *this; }

	/** Inherited from tevent_handler. */
	const twindow& get_window() const { return *this; }

	/** Inherited from tevent_handler. */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active) 
		{ return tpanel::find_widget(coordinate, must_be_active); }

	/** Inherited from tevent_handler. */
	const twidget* find_widget(const tpoint& coordinate, 
			const bool must_be_active) const
		{ return tpanel::find_widget(coordinate, must_be_active); }

	/** Inherited from twidget. */
	tdialog* dialog() { return owner_; }

	/** 
	 * Inherited from tcontrol.
	 *
	 * @todo See whether we're required or simply can inherit. If needed we need
	 * to be implemented properly
	 */
	bool needs_full_redraw() const { return false; }

	/** Inherited from tcontainer_. */
	twidget* find_widget(const std::string& id, const bool must_be_active) 
		{ return tcontainer_::find_widget(id, must_be_active); }
	
	/** Inherited from tcontainer_. */
	const twidget* find_widget(const std::string& id, 
			const bool must_be_active) const 
		{ return tcontainer_::find_widget(id, must_be_active); }

	/** Inherited from tpanel. */
	SDL_Rect get_client_rect() const;

	/** Inherited from tpanel. */
	void draw(surface& surface, const bool force = false, 
			const bool invalidate_background = false);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	/**
	 * Sets there return value of the window.
	 *
	 * @param retval              The return value for the window.
	 * @param close               Close the window after setting the value.
	 */
	void set_retval(const int retval, const bool close_window = true)
		{ retval_ = retval; if(close_window) close(); }

	void set_owner(tdialog* owner) { owner_ = owner; }

private:

	/** Needed so we can change what's drawn on the screen. */
	CVideo& video_;

	/** The status of the window. */
	tstatus status_;

	// return value of the window, 0 default.
	int retval_;

	/** The dialog that owns the window. */
	tdialog* owner_;

	/** When set the form needs a full layout redraw cycle. */
	bool need_layout_;

	/** Widget for the tooltip. */
	ttooltip tooltip_;

	/** Widget for the help popup FIXME should be thelp_popup. */
	ttooltip help_popup_;

	/** Do we wish to place the widget automatically? */
	const bool automatic_placement_;

	/**
	 * Sets the horizontal placement.
	 *
	 * Only used if automatic_placement_ is true.
	 * The value should be a tgrid placement flag.
	 */
	const unsigned horizontal_placement_;

	/**
	 * Sets the vertical placement.
	 *
	 * Only used if automatic_placement_ is true.
	 * The value should be a tgrid placement flag.
	 */
	const unsigned vertical_placement_;

	/** The formula to calulate the x value of the dialog. */
	tformula<unsigned>x_;

	/** The formula to calulate the y value of the dialog. */
	tformula<unsigned>y_;

	/** The formula to calulate the width of the dialog. */
	tformula<unsigned>w_;

	/** The formula to calulate the height of the dialog. */
	tformula<unsigned>h_;

	/**
	 * Updates the size for the widget.
	 *
	 * Then the window isn't placed automatically the tformula for the sizes
	 * need to be recalculated and set_size() called with the result.
	 */
	void update_size();

	/** Does the real updating of the changes on the screen. */
	void flip();

	/** Inherited from tevent_handler. */
	void do_show_tooltip(const tpoint& location, const t_string& tooltip);

	/** Inherited from tevent_handler. */
	void do_remove_tooltip() { tooltip_.set_visible(false); }

	/** Inherited from tevent_handler. */
	void do_show_help_popup(const tpoint& location, const t_string& help_popup);

	/** Inherited from tevent_handler. */
	void do_remove_help_popup() { help_popup_.set_visible(false); }

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const 
		{ static const std::string type = "window"; return type; }
};

} // namespace gui2

#endif
