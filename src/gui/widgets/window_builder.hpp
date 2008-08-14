/* $Id$ */
/*
   Copyright (C) 2008 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_WINDOW_BUILDER_HPP_INCLUDED
#define GUI_WIDGETS_WINDOW_BUILDER_HPP_INCLUDED

#include "reference_counted_object.hpp"
#include "tstring.hpp"

#include <string>
#include <vector>

class config;
class CVideo;

namespace gui2 {

class twidget;
class twindow;

twindow build(CVideo& video, const std::string& type);


//! Contains the info needed to instantiate a widget.
struct tbuilder_widget : public reference_counted_object
{
private:
	tbuilder_widget();

public:
	tbuilder_widget(const config& /*cfg*/) {}


	virtual twidget* build() const = 0;
	virtual ~tbuilder_widget() {}
};

typedef boost::intrusive_ptr<tbuilder_widget> tbuilder_widget_ptr;
typedef boost::intrusive_ptr<const tbuilder_widget> const_tbuilder_widget_ptr;

//!
struct tbuilder_grid : public tbuilder_widget
{
private:
	tbuilder_grid();

public:
	tbuilder_grid(const config& cfg);
	unsigned rows;
	unsigned cols;

	//! The grow factor for the rows / columns.
	std::vector<unsigned> row_grow_factor;
	std::vector<unsigned> col_grow_factor;

	//! The flags per grid cell.
	std::vector<unsigned> flags;

	//! The border size per grid cell.
	std::vector<unsigned> border_size;

	//! The widgets per grid cell.
	std::vector<tbuilder_widget_ptr> widgets;

	twidget* build () const;

private:
	//! After reading the general part in the constructor read extra data.
	void read_extra(const config& cfg);
};

typedef boost::intrusive_ptr<tbuilder_grid> tbuilder_grid_ptr;
typedef boost::intrusive_ptr<const tbuilder_grid> tbuilder_grid_const_ptr;

class twindow_builder
{
public:
	const std::string& read(const config& cfg);

	struct tresolution
	{
	private:
		tresolution();

	public:
		tresolution(const config& cfg);

		unsigned window_width;
		unsigned window_height;

		bool automatic_placement;

		unsigned x;
		unsigned y;
		unsigned width;
		unsigned height;

		unsigned vertical_placement;
		unsigned horizontal_placement;
		
		std::string definition;
	
		tbuilder_grid_ptr grid;
	};

	std::vector<tresolution> resolutions;
	
private:
	std::string id_;
	std::string description_;

};

} // namespace gui2


#endif



