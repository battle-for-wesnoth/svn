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

#ifndef GUI_WIDGETS_CONTAINER_HPP_INCLUDED
#define GUI_WIDGETS_CONTAINER_HPP_INCLUDED

#include "gui/widgets/grid.hpp"
#include "gui/widgets/control.hpp"

namespace gui2 {

/**
 * A generic container base class.
 *
 * A container is a class build with multiple items either acting as one 
 * widget.
 *
 */
class tcontainer_ : public tcontrol
{
	friend class tdebug_layout_graph;
public:
	tcontainer_(const unsigned canvas_count) :
		tcontrol(canvas_count),
		grid_(),
		background_changed_(true)
	{
		grid_.set_parent(this);
	}

	/** 
	 * Returns the size of the client area.
	 *
	 * The client area is the area available for widgets.
	 */
	virtual SDL_Rect get_client_rect() const { return get_rect(); }

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from twidget. */
	bool can_wrap() const { return grid_.can_wrap() || twidget::can_wrap(); }

	/** 
	 * Inherited from twidget. 
	 *
	 * @todo Adjust for our border.
	 */
	bool set_width_constrain(const unsigned width) {  return grid_.set_width_constrain(width); }

	/** Inherited from twidget. */
	void clear_width_constrain() { grid_.clear_width_constrain(); }

	/** 
	 * Inherited from twidget. 
	 * 
	 * Since we can't define a good default behaviour we force the inheriting
	 * classes to define this function. So inheriting classes act as one widget
	 * others as a collection of multiple objects.
	 */
	bool has_vertical_scrollbar() const = 0;

	/** Inherited from twidget.*/
	bool has_widget(const twidget* widget) const 
		{ return grid_.has_widget(widget); }

	/** Inherited from twidget. */
	bool is_dirty() const { return twidget::is_dirty() || grid_.is_dirty(); }

	/** Inherited from tcontrol. */
	tpoint get_minimum_size() const;

	/** Inherited from tcontrol. */
	tpoint get_best_size() const;

	/** Inherited from twidget. */
	tpoint get_best_size(const tpoint& maximum_size) const;

	/** Inherited from tcontrol. */
	void draw(surface& surface,  const bool force = false,
	        const bool invalidate_background = false);

	/** Inherited from tcontrol. */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active) 
		{ return grid_.find_widget(coordinate, must_be_active); }

	/** Inherited from tcontrol. */
	const twidget* find_widget(const tpoint& coordinate, 
			const bool must_be_active) const
		{ return grid_.find_widget(coordinate, must_be_active); }

	/** Inherited from tcontrol.*/
	twidget* find_widget(const std::string& id, const bool must_be_active)
	{ 
		twidget* result = tcontrol::find_widget(id, must_be_active);
		return result ? result : grid_.find_widget(id, must_be_active); 
	}

	/** Inherited from tcontrol.*/
	const twidget* find_widget(const std::string& id, const bool must_be_active) const
	{ 
		const twidget* result = tcontrol::find_widget(id, must_be_active);
		return result ? result : grid_.find_widget(id, must_be_active); 
	}

	/** Import overloaded versions. */
	using tcontrol::find_widget;
	
	/** Inherited from tcontrol. */
	void set_size(const SDL_Rect& rect) 
	{	
		tcontrol::set_size(rect);
		set_client_size(get_client_rect());
	}

	/** Inherited from tcontrol. */
	void set_active(const bool active);

	/** 
	 * Inherited from tcontrol.
	 *
	 * NOTE normally containers don't block, but their children may. But
	 * normally the state for the children is set as well so we don't need to
	 * delegate the request to our children.
	 */
	bool does_block_easy_close() const { return false; }

	/***** **** ***** ***** wrappers to the grid **** ********* *****/

	tgrid::iterator begin() { return grid_.begin(); }
	tgrid::iterator end() { return grid_.end(); }

	unsigned add_row(const unsigned count = 1) 
		{ return grid_.add_row(count); }

	void set_rows(const unsigned rows) { grid_.set_rows(rows); }
	unsigned int get_rows() const { return grid_.get_rows(); }

	void set_cols(const unsigned cols) { grid_.set_cols(cols); }
	unsigned int get_cols() const { return grid_.get_cols(); }

	void set_rows_cols(const unsigned rows, const unsigned cols)
		{ grid_.set_rows_cols(rows, cols); }

	void set_child(twidget* widget, const unsigned row, 
		const unsigned col, const unsigned flags, const unsigned border_size)
		{ grid_.set_child(widget, row, col, flags, border_size); }

	void set_row_grow_factor(const unsigned row, const unsigned factor) 
		{ grid_.set_row_grow_factor(row, factor); }

	void set_col_grow_factor(const unsigned col, const unsigned factor)
		{ grid_.set_col_grow_factor(col, factor); }

	/** FIXME see whether needed to be exported. */
	void set_client_size(const SDL_Rect& rect) { grid_.set_size(rect); }


	/** Inherited from twidget. */
	void set_dirty(const bool dirty = true);

protected:
	/***** ***** ***** setters / getters for members ***** ****** *****/

	const tgrid& grid() const { return grid_; }
	tgrid& grid() { return grid_; }

	void set_background_changed(const bool changed = true)
		{ background_changed_ = changed; }
	bool has_background_changed() const { return background_changed_; }
		
private:

	/** The grid which holds the child objects. */
	tgrid grid_;

	/** Returns the space used by the border. */
	virtual tpoint border_space() const { return tpoint(0, 0); }

	/**
	 * Helper for set_active.
	 *
	 * This function should set the control itself active. It's called by
	 * set_active if the state needs to change. The widget is set to dirty() by
	 * set_active so we only need to change the state.
	 */
	virtual void set_self_active(const bool active) = 0;

	/** 
	 * If the background has been changed the next draw cycle needs to do a full
	 * redraw and also tell the child items to invalidate their background. This
	 * overrides the 'invalidate_background' parameter send to draw().
	 */
	bool background_changed_;
};

} // namespace gui2

#endif

