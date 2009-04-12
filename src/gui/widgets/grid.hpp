/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_GRID_HPP_INCLUDED
#define GUI_WIDGETS_GRID_HPP_INCLUDED

#include "gui/widgets/widget.hpp"
#include "gui/widgets/control.hpp"

#include <cassert>

namespace gui2 {

/**
 * Base container class.
 *
 * This class holds a number of widgets and their wanted layout parameters. It
 * also layouts the items in the grid and hanldes their drawing.
 */
class tgrid : public virtual twidget
{
	friend class tdebug_layout_graph;

public:

	tgrid(const unsigned rows = 0, const unsigned cols = 0);

	virtual ~tgrid();

	/***** ***** ***** ***** LAYOUT FLAGS ***** ***** ***** *****/
	static const unsigned VERTICAL_SHIFT                 = 0;
	static const unsigned VERTICAL_GROW_SEND_TO_CLIENT   = 1 << VERTICAL_SHIFT;
	static const unsigned VERTICAL_ALIGN_TOP             = 2 << VERTICAL_SHIFT;
	static const unsigned VERTICAL_ALIGN_CENTER          = 3 << VERTICAL_SHIFT;
	static const unsigned VERTICAL_ALIGN_BOTTOM          = 4 << VERTICAL_SHIFT;
	static const unsigned VERTICAL_MASK                  = 7 << VERTICAL_SHIFT;

	static const unsigned HORIZONTAL_SHIFT               = 3;
	static const unsigned HORIZONTAL_GROW_SEND_TO_CLIENT = 1 << HORIZONTAL_SHIFT;
	static const unsigned HORIZONTAL_ALIGN_LEFT          = 2 << HORIZONTAL_SHIFT;
	static const unsigned HORIZONTAL_ALIGN_CENTER        = 3 << HORIZONTAL_SHIFT;
	static const unsigned HORIZONTAL_ALIGN_RIGHT         = 4 << HORIZONTAL_SHIFT;
	static const unsigned HORIZONTAL_MASK                = 7 << HORIZONTAL_SHIFT;

	static const unsigned BORDER_TOP                     = 1 << 6;
	static const unsigned BORDER_BOTTOM                  = 1 << 7;
	static const unsigned BORDER_LEFT                    = 1 << 8;
	static const unsigned BORDER_RIGHT                   = 1 << 9;
	static const unsigned BORDER_ALL                     =
		BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT;

	/***** ***** ***** ***** ROW COLUMN MANIPULATION ***** ***** ***** *****/

	/**
	 * Addes a row to end of the grid.
	 *
	 * @param count               Number of rows to add, should be > 0.
	 *
	 * @returns                   The row number of the first row added.
	 */
	unsigned add_row(const unsigned count = 1);

	/**
	 * Sets the grow factor for a row.
	 *
	 * @todo refer to a page with the layout manipulation info.
	 *
	 * @param row                 The row to modify.
	 * @param factor              The grow factor.
	 */
	void set_row_grow_factor(const unsigned row, const unsigned factor)
	{
		assert(row < row_grow_factor_.size());
		row_grow_factor_[row] = factor;
		set_dirty();
	}

	/**
	 * Sets the grow factor for a column.
	 *
	 * @todo refer to a page with the layout manipulation info.
	 *
	 * @param column              The column to modify.
	 * @param factor              The grow factor.
	 */
	void set_col_grow_factor(const unsigned col, const unsigned factor)
	{
		assert(col< col_grow_factor_.size());
		col_grow_factor_[col] = factor;
		set_dirty();
	}

	/***** ***** ***** ***** CHILD MANIPULATION ***** ***** ***** *****/

	/**
	 * Sets a child in the grid.
	 *
	 * When the child is added to the grid the grid 'owns' the widget.
	 * The widget is put in a cell, and every cell can only contain 1 widget if
	 * the wanted cell already contains a widget, that widget is freed and
	 * removed.
	 *
	 * @param widget              The widget to put in the grid.
	 * @param row                 The row of the cell.
	 * @param col                 The columnof the cell.
	 * @param flags               The flags for the widget in the cell.
	 * @param border_size         The size of the border for the cell, how the
	 *                            border is applied depends on the flags.
	 */
	void set_child(twidget* widget, const unsigned row,
		const unsigned col, const unsigned flags, const unsigned border_size);

	/**
	 * Exchangs a child in the grid.
	 *
	 * It replaced the child with a certain id with the new widget but doesn't
	 * touch the other settings of the child.
	 *
	 * @param id                  The id of the widget to free.
	 * @param widget              The widget to put in the grid.
	 * @parem recurse             Do we want to decent into the child grids.
	 * @parem new_parent          The new parent for the swapped out widget.
	 *
	 * returns                    The widget which got removed (the parent of
	 *                            the widget is cleared). If no widget found
	 *                            and thus not replace NULL will returned.
	 */
	twidget* swap_child(
		const std::string& id, twidget* widget, const bool recurse,
		twidget* new_parent = NULL);

	/**
	 * Removes and frees a widget in a cell.
	 *
	 * @param row                 The row of the cell.
	 * @param col                 The columnof the cell.
	 */
	void remove_child(const unsigned row, const unsigned col);

	/**
	 * Removes and frees a widget in a cell by it's id.
	 *
	 * @param id                  The id of the widget to free.
	 * @param find_all            If true if removes all items with the id,
	 *                            otherwise it stops after removing the first
	 *                            item (or once all children have been tested).
	 */
	void remove_child(const std::string& id, const bool find_all = false);

	/**
	 * Activates all children.
	 *
	 * If a child inherits from tcontrol or is a tgrid it will call
	 * set_active() for the child otherwise it ignores the widget.
	 *
	 * @param active              Parameter for set_active.
	 */
	void set_active(const bool active);


	/** Returns the widget in the selected cell. */
	const twidget* widget(const unsigned row, const unsigned col) const
		{ return child(row, col).widget(); }

	/** Returns the widget in the selected cell. */
	twidget* widget(const unsigned row, const unsigned col)
		{ return child(row, col).widget(); }

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/** Inherited from twidget. */
	void layout_init();
	void layout_init2(const bool full_initialization);

private:

	/** Inherited from twidget. */
	tpoint calculate_best_size() const;
public:

	/** Inherited from twidget. */
	bool can_wrap() const;

	/** Inherited from twidget. */
	void layout_wrap(const unsigned maximum_width);

	/** Inherited from twidget. */
	bool has_vertical_scrollbar() const;

	/** Inherited from twidget. */
	void layout_use_vertical_scrollbar(const unsigned maximum_height);

	/** Inherited from twidget. */
	bool has_horizontal_scrollbar() const;

	/** Inherited from twidget. */
	void layout_use_horizontal_scrollbar(const unsigned maximum_width);

	/** Inherited from twidget. */
	void layout_fit_width(const unsigned maximum_width,
			const tfit_flags flags);
private:
	/** Internal helper for layout_fit_width. */
	void impl_layout_fit_width(const unsigned maximum_width,
			const tfit_flags flags);
public:
	/** Inherited from twidget. */
	void set_size(const tpoint& origin, const tpoint& size);

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from twidget. */
	void set_origin(const tpoint& origin);

	/** Inherited from twidget. */
	void set_visible_area(const SDL_Rect& area);

	/** Inherited from twidget. */
	void child_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack);

	/** Inherited from twidget. */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active);

	/** Inherited from twidget. */
	const twidget* find_widget(const tpoint& coordinate,
			const bool must_be_active) const;

	/** Inherited from twidget.*/
	twidget* find_widget(const std::string& id, const bool must_be_active);

	/** Inherited from twidget.*/
	const twidget* find_widget(const std::string& id,
			const bool must_be_active) const;

	/** Import overloaded versions. */
	using twidget::find_widget;

	/** Inherited from twidget.*/
	bool has_widget(const twidget* widget) const;

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_rows(const unsigned rows);
	unsigned int get_rows() const { return rows_; }

	void set_cols(const unsigned cols);
	unsigned int get_cols() const { return cols_; }

	/**
	 * Wrapper to set_rows and set_cols.
	 *
	 * @param rows                Parameter to call set_rows with.
	 * @param cols                Parameter to call set_cols with.
	 */
	void set_rows_cols(const unsigned rows, const unsigned cols);

private:
	/** Child item of the grid. */
	class tchild
	{
	public:
		tchild() :
			flags_(0),
			border_size_(0),
			widget_(0)

			// Fixme make a class wo we can store some properties in the cache
			// regarding size etc.
			{}

		/** Returns the best size for the cell. */
		tpoint get_best_size() const;

		/**
		 * Sets the size of the widget in the cell.
		 *
		 * @param origin          The origin (x, y) for the widget.
		 * @param size            The size for the widget.
		 */
		void set_size(tpoint origin, tpoint size);

		/** Returns the can_wrap for the cell. */
		bool can_wrap() const
			{ return widget_ ? widget_->can_wrap() : false; }

		/** Forwards layout_wrap() to the cell. */
		void layout_wrap(const unsigned maximum_width);

		/** Forwards layout_use_vertical_scrollbar() to the cell. */
		void layout_use_vertical_scrollbar(const unsigned maximum_height);

		/** Forwards layout_use_horizontal_scrollbar() to the cell. */
		void layout_use_horizontal_scrollbar(const unsigned maximum_width);

		/** Forwards layout_fit_width() to the cell. */
		void layout_fit_width(const unsigned maximum_width,
				const tfit_flags flags);

		/** Returns the id of the widget/ */
		const std::string& id() const;

		unsigned get_flags() const { return flags_; }
		void set_flags(const unsigned flags) { flags_ = flags; }

		unsigned get_border_size() const { return border_size_; }
		void set_border_size(const unsigned border_size)
			{  border_size_ = border_size; }

		const twidget* widget() const { return widget_; }
		twidget* widget() { return widget_; }

		void set_widget(twidget* widget) { widget_ = widget; }

	private:
		/** The flags for the border and cell setup. */
		unsigned flags_;

		/**
		 * The size of the border, the actual configuration of the border
		 * is determined by the flags.
		 */
		unsigned border_size_;

		/**
		 * Pointer to the widget.
		 *
		 * Once the widget is assigned to the grid we own the widget and are
		 * responsible for it's destruction.
		 */
		twidget* widget_;

		/** Returns the space needed for the border. */
		tpoint border_space() const;

	}; // class tchild

public:
	/** Iterator for the tchild items. */
	class iterator
	{

	public:

		iterator(std::vector<tchild>::iterator itor) :
			itor_(itor)
			{}

		iterator operator++() { return iterator(++itor_); }
		iterator operator--() { return iterator(--itor_); }
		twidget* operator->() { return itor_->widget(); }
		twidget* operator*() { return itor_->widget(); }

		bool operator!=(const iterator& i) const
			{ return i.itor_ != this->itor_; }

	private:
		std::vector<tchild>::iterator itor_;

	};

	iterator begin() { return iterator(children_.begin()); }
	iterator end() { return iterator(children_.end()); }

private:
	/** The number of grid rows. */
	unsigned rows_;

	/** The number of grid columns. */
	unsigned cols_;

	/***** ***** ***** ***** size caching ***** ***** ***** *****/

	/** The row heights in the grid. */
	mutable std::vector<unsigned> row_height_;

	/** The column widths in the grid. */
	mutable std::vector<unsigned> col_width_;

	/** The grow factor for all rows. */
	std::vector<unsigned> row_grow_factor_;

	/** The grow factor for all columns. */
	std::vector<unsigned> col_grow_factor_;

	/**
	 * The child items.
	 *
	 * All children are stored in a 1D vector and the formula to access a cell
	 * is: rows_ * col + row. All other vectors use the same access formula.
	 */
	std::vector<tchild> children_;
	const tchild& child(const unsigned row, const unsigned col) const
		{ return children_[rows_ * col + row]; }
	tchild& child(const unsigned row, const unsigned col)
		{ return children_[rows_ * col + row]; }

	/** Layouts the children in the grid. */
	void layout(const tpoint& origin);

	/** Inherited from twidget. */
	void impl_draw_children(surface& frame_buffer);

	/**
	 * Gets the best height for a row.
	 *
	 * @param row                 The row to get the best height for.
	 * @param maximum_height      The wanted maximum height.
	 *
	 * @returns                   The best height for a row, if possible
	 *                            smaller as the maximum.
	 */
	unsigned row_use_vertical_scrollbar(
			const unsigned row, const unsigned maximum_height);

	/**
	 * Gets the best width for a column.
	 *
	 * @param column              The column to get the best width for.
	 * @param maximum_width       The wanted maximum width.
	 *
	 * @returns                   The best width for a column, if possible
	 *                            smaller as the maximum.
	 */
	unsigned column_use_horizontal_scrollbar(
			const unsigned column, const unsigned maximum_width);

};

} // namespace gui2

#endif

