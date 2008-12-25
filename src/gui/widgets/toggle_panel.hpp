/* $Id$ */
/*
   copyright (C) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef GUI_WIDGETS_TOGGLE_PANEL_HPP_INCLUDED
#define GUI_WIDGETS_TOGGLE_PANEL_HPP_INCLUDED

#include "gui/widgets/panel.hpp"
#include "gui/widgets/selectable.hpp"

namespace gui2 {

/**
 * Class for a toggle button.
 *
 * Quite some code looks like ttoggle_button maybe we should inherit from that but let's test first.
 * the problem is that the toggle_button has an icon we don't want, but maybe look at refactoring later.
 * but maybe we should also ditch the icon, not sure however since it's handy for checkboxes...
 */
class ttoggle_panel : public tpanel, public tselectable_
{
public:
	ttoggle_panel() : 
		tpanel(COUNT),
		state_(ENABLED),
		retval_(0),
		callback_state_change_(0)
	{
	}

	/**
	 * Sets the members of the child controls.
	 *
	 * Sets the members for all controls which have the proper member id. See
	 * tcontrol::set_members for more info.
	 *
	 * @param data                Map with the key value pairs to set the members.
	 */
	void set_child_members(const std::map<std::string /* widget id */, string_map>& data);

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherted from tevent_executor. */
	void mouse_enter(tevent_handler&);

	/** Inherted from tevent_executor. */
	void mouse_leave(tevent_handler&);

	/** Inherted from tevent_executor. */
	void mouse_left_button_click(tevent_handler&);

	/** Inherted from tevent_executor. */
	void mouse_left_button_double_click(tevent_handler&);

	/** Inherited from tcontainer_ */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active) 
	{
		/**
		 * @todo since there is no mouse event nesting (or event nesting at all)
		 * we need to capture all events. This means items on the panel will
		 * never receive an event, which gives problems with for example the
		 * intended button on the addon panel. So we need to chain mouse events
		 * as well and also add a handled flag for them.
		 */
//		twidget* result = tcontainer_::find_widget(coordinate, must_be_active);
		return /*result ? result :*/ tcontrol::find_widget(coordinate, must_be_active);
	}

	/** Inherited from tcontainer_ */
	const twidget* find_widget(const tpoint& coordinate, const bool must_be_active) const
	{
//		const twidget* result = tcontainer_::find_widget(coordinate, must_be_active);
		return /*result ? result :*/ tcontrol::find_widget(coordinate, must_be_active);
	}
#ifdef NEW_DRAW
	/** Inherited from tcontainer_ */
	twidget* find_widget2(const tpoint& coordinate, const bool must_be_active) 
	{
		/**
		 * @todo since there is no mouse event nesting (or event nesting at all)
		 * we need to capture all events. This means items on the panel will
		 * never receive an event, which gives problems with for example the
		 * intended button on the addon panel. So we need to chain mouse events
		 * as well and also add a handled flag for them.
		 */
//		twidget* result = tcontainer_::find_widget2(coordinate, must_be_active);
		return /*result ? result :*/ tcontrol::find_widget2(coordinate, must_be_active);
	}

	/** Inherited from tcontainer_ */
	const twidget* find_widget2(const tpoint& coordinate, const bool must_be_active) const
	{
//		const twidget* result = tcontainer_::find_widget2(coordinate, must_be_active);
		return /*result ? result :*/ tcontrol::find_widget2(coordinate, must_be_active);
	}
#endif
	// Needed to import the find_widget(const tpoint&, const bool) and it's const version
	// inheriting from panel eventhought they are the same as tcontainer_ but it might be
	// panel reimplements it.
	using tpanel::find_widget;

	/** Inherited from tpanel. */
	void set_active(const bool active);

	/** Inherited from tpanel. */
	bool get_active() const 
		{ return state_ != DISABLED && state_ != DISABLED_SELECTED; }

	/** Inherited from tpanel. */
	unsigned get_state() const { return state_; }

#ifndef NEW_DRAW	
	/** Inherited from tpanel. */
	void draw(surface& surface, const bool force = false, 
			const bool invalidate_background = false)
		{ tcontainer_::draw(surface, force, invalidate_background); }
#else
	/** Inherited from tpanel. */
	void draw_background(surface& frame_buffer)  
	{
		// We don't have a fore and background and need to draw depending on
		// our state, like a control. So we use the controls drawing method.
		tcontrol::draw_background(frame_buffer);
	}

	/** Inherited from tpanel. */
	void draw_foreground(surface& frame_buffer)
	{
		// We don't have a fore and background and need to draw depending on
		// our state, like a control. So we use the controls drawing method.
		tcontrol::draw_foreground(frame_buffer);
	}
#endif
	/** 
	 * Inherited from tpanel. 
	 *
	 * @todo only due to the fact our definition is slightly different from
	 * tpanel_defintion we need to override this function and do about the same,
	 * look at a way to 'fix' that.
	 */
	SDL_Rect get_client_rect() const;

	/** 
	 * Inherited from tpanel. 
	 *
	 * @todo only due to the fact our definition is slightly different from
	 * tpanel_defintion we need to override this function and do about the same,
	 * look at a way to 'fix' that.
	 */
	tpoint border_space() const;

	/** Inherited from tselectable_ */
	bool get_value() const { return state_ >= ENABLED_SELECTED; }

	/** Inherited from tselectable_ */
	void set_value(const bool selected);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_retval(const int retval);

	/** Inherited from tselectable_. */
	void set_callback_state_change(void (*callback) (twidget*)) 
		{ callback_state_change_ = callback; }

private:

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 * Also note the internals do assume the order for 'up' and 'down' to be the
	 * same and also that 'up' is before 'down'. 'up' has no suffix, 'down' has
	 * the SELECTED suffix.
	 */
	enum tstate { 
		ENABLED,          DISABLED,          FOCUSSED, 
		ENABLED_SELECTED, DISABLED_SELECTED, FOCUSSED_SELECTED, 
		COUNT};

	void set_state(const tstate state);

	/** 
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

 	/**
	 * The return value of the button.
	 *
	 * If this value is not 0 and the button is double clicked it sets the
	 * retval of the window and the window closes itself.
	 */
	int retval_;

 	/** See tselectable_::set_callback_state_change. */
	void (*callback_state_change_) (twidget*);

	/** Inherited from tpanel. */
	const std::string& get_control_type() const 
		{ static const std::string type = "toggle_panel"; return type; }

};

} // namespace gui2

#endif



