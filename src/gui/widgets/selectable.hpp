/* $Id$ */
/*
   copyright (C) 2007 - 2009 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef GUI_WIDGETS_SELECTABLE_HPP_INCLUDED
#define GUI_WIDGETS_SELECTABLE_HPP_INCLUDED

namespace gui2 {

/**
 * Small abstract helper class.
 *
 * Parts of the engine inherit this class so we can have generic
 * selectable items.
 */
class tselectable_
{
public:
	virtual ~tselectable_() {}

	/** Is the control selected? */
	virtual bool get_value() const = 0;

	/** Select the control. */
	virtual void set_value(const bool) = 0;

	/**
	 * When the user does something to change the widget state this event is
	 * fired. Most of the time it will be a left click on the widget.
	 */
	virtual void set_callback_state_change(void (*callback) (twidget*)) = 0;
};

} // namespace gui2

#endif
