/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/widgets/grid.hpp"

#include "log.hpp"

#include <cassert>
#include <numeric>

#define DBG_G LOG_STREAM(debug, gui)
#define LOG_G LOG_STREAM(info, gui)
#define WRN_G LOG_STREAM(warn, gui)
#define ERR_G LOG_STREAM(err, gui)

#define DBG_G_D LOG_STREAM(debug, gui_draw)
#define LOG_G_D LOG_STREAM(info, gui_draw)
#define WRN_G_D LOG_STREAM(warn, gui_draw)
#define ERR_G_D LOG_STREAM(err, gui_draw)

#define DBG_G_E LOG_STREAM(debug, gui_event)
#define LOG_G_E LOG_STREAM(info, gui_event)
#define WRN_G_E LOG_STREAM(warn, gui_event)
#define ERR_G_E LOG_STREAM(err, gui_event)

#define DBG_G_P LOG_STREAM(debug, gui_parse)
#define LOG_G_P LOG_STREAM(info, gui_parse)
#define WRN_G_P LOG_STREAM(warn, gui_parse)
#define ERR_G_P LOG_STREAM(err, gui_parse)


namespace gui2 {

tgrid::tgrid(const unsigned rows, const unsigned cols, 
		const unsigned default_flags, const unsigned default_border_size) :
	rows_(rows),
	cols_(cols),
	default_flags_(default_flags),
	default_border_size_(default_border_size),
	children_(rows * cols)
{
}

tgrid::~tgrid()
{
	for(std::vector<tchild>::iterator itor = children_.begin();
			itor != children_.end(); ++itor) {

		if(itor->widget()) {
			delete itor->widget();
		}
	}
}

void tgrid::add_child(twidget* widget, const unsigned row, 
		const unsigned col, const unsigned flags, const unsigned border_size) 
{
	assert(row < rows_ && col < cols_);

	tchild& cell = child(row, col);

	// clear old child if any
	if(cell.widget()) {
		// free a child when overwriting it
		WRN_G << "Grid: child '" << cell.id() 
			<< "' at cell '" << row << ',' << col << "' will be replaced.\n";
		delete cell.widget();
	}

	// copy data
	cell.set_flags(flags);
	cell.set_border_size(border_size);
	cell.set_widget(widget);
	if(cell.widget()) {
		// make sure the new child is valid before deferring
		cell.set_id(cell.widget()->id());
		cell.widget()->set_parent(this);
	} else {
		cell.set_id("");
	}
}

void tgrid::set_rows(const unsigned rows)
{
	if(rows == rows_) {
		return;
	}

	if(!children_.empty()) {
		WRN_G << "Grid: resizing a non-empty grid may give unexpected problems.\n";
	}

	rows_ = rows;
	children_.resize(rows_ * cols_);
}

void tgrid::set_cols(const unsigned cols)
{
	if(cols == cols_) {
		return;
	}

	if(!children_.empty()) {
		WRN_G << "Grid: resizing a non-empty grid may give unexpected problems.\n";
	}

	cols_ = cols;
	children_.resize(cols_ * cols_);
}

void tgrid::remove_child(const unsigned row, const unsigned col)
{
	assert(row < rows_ && col < cols_);

	tchild& cell = child(row, col);

	cell.set_id("");
	cell.set_widget(0);
}

void tgrid::removed_child(const std::string& id, const bool find_all)
{
	for(std::vector<tchild>::iterator itor = children_.begin();
			itor != children_.end(); ++itor) {

		if(itor->id() == id) {
			itor->set_id("");
			itor->set_widget(0);

			if(!find_all) {
				break;
			}
		}
	}
}

tpoint tgrid::get_best_size()
{
	std::vector<unsigned> best_col_width(cols_, 0);
	std::vector<unsigned> best_row_height(rows_, 0);
	
	// First get the best sizes for all items.
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			const tpoint size = child(row, col).get_best_size();

			if(size.x > best_col_width[col]) {
				best_col_width[col] = size.x;
			}

			if(size.y > best_row_height[row]) {
				best_row_height[row] = size.y;
			}

		}
	}

	for(unsigned row = 0; row < rows_; ++row) {
		DBG_G << "Grid: the best height for row " << row 
			<< " will be " << best_row_height[row] << ".\n";
	}

	for(unsigned col = 0; col < cols_; ++col) {
		DBG_G << "Grid: the best width for col " << col 
			<< " will be " << best_col_width[col]  << ".\n";
	}

	return tpoint(
		std::accumulate(best_col_width.begin(), best_col_width.end(), 0),
		std::accumulate(best_row_height.begin(), best_row_height.end(), 0));

}

void tgrid::set_best_size(const tpoint& origin)
{
	std::vector<unsigned> best_col_width(cols_, 0);
	std::vector<unsigned> best_row_height(rows_, 0);
	
	// First get the best sizes for all items. (FIXME copy and paste of get best size)
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			const tpoint size = child(row, col).get_best_size();

			if(size.x > best_col_width[col]) {
				best_col_width[col] = size.x;
			}

			if(size.y > best_row_height[row]) {
				best_row_height[row] = size.y;
			}

		}
	}

	// Set the sizes
	tpoint orig = origin;
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			DBG_G << "Grid: set widget at " << row 
				<< ',' << col << " at origin " << orig << ".\n";

			if(child(row, col).widget()) {
				child(row, col).widget()->set_best_size(orig);
			}

			orig.x += best_col_width[col];
		}
		orig.y += best_row_height[row];
		orig.x = origin.x;
	}
}

twidget* tgrid::get_widget(const tpoint& coordinate)
{
	
	//! FIXME we need to store the sizes, since this is quite
	//! pathatic.
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			twidget* widget = child(row, col).widget();
			if(!widget) {
				continue;
			}
			
			widget = widget->get_widget(coordinate);
			if(widget) { 
				return widget;
			}
		}
	}
	
	return 0;
}

//! Gets a widget with the wanted id.
//! Override base class.
twidget* tgrid::get_widget_by_id(const std::string& id)
{

	//! FIXME we need to store the sizes, since this is quite
	//! pathatic.
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			twidget* widget = child(row, col).widget();
			if(!widget) {
				continue;
			}
			
			widget = widget->get_widget_by_id(id);
			if(widget) { 
				return widget;
			}
		}
	}
	
	return twidget::get_widget_by_id(id);
}

tpoint tgrid::tchild::get_best_size()
{
	if(!dirty_ && (!widget_ || !widget_->dirty())) {
		return best_size_;
	}

	best_size_ = widget_ ? widget_->get_best_size() : tpoint(0, 0);

	//FIXME take care of the border configuration.
	best_size_.x += 2 * border_size_;
	best_size_.y += 2 * border_size_;

	dirty_ = false;

	return best_size_;

}


} // namespace gui2


