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

#ifndef GUI_WIDGETS_SPACER_HPP_INCLUDED
#define GUI_WIDGETS_SPACER_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

/**
 * An empty widget.
 *
 * Since every grid cell needs a widget this is a blank widget. This widget can
 * also be used to 'force' sizes.
 *
 * Since we're a kind of dummy class we're always active, our drawing does
 * nothing.
 */
class tspacer : public tcontrol
{
public:
	tspacer() : 
		tcontrol(0),
		best_size_(0, 0)
	{
	}

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

private:
	/** Inherited from tcontrol. */
	tpoint calculate_best_size() const
	{ 
		return best_size_ != tpoint(0, 0) 
			? best_size_ : tcontrol::calculate_best_size(); 
	}
public:

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool) {}

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/** Inherited from tcontrol. */
	bool does_block_easy_close() const { return false; }

	/** 
	 * Inherited from tcontrol. 
	 *
	 * Since we're always empty the draw does nothing.
	 */
	void draw(surface& /*surface*/, const bool /*force*/ = false, 
		const bool /*invalidate_background*/ = false) {}
	
	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_best_size(const tpoint& best_size) { best_size_ = best_size; }
	
private:

	/** When we're used as a fixed size item, this holds the best size. */
	tpoint best_size_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const 
		{ static const std::string type = "spacer"; return type; }
};


} // namespace gui2

#endif


