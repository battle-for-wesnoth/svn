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

/** 
 * @file window_builder_private.hpp 
 * This file contains all classes used privately in window_builder.cpp and
 * should only be included by window_builder.cpp.
 */

#ifndef GUI_WIDGETS_WINDOW_BUILDER_PRIVATE_HPP_INCLUDED
#define GUI_WIDGETS_WINDOW_BUILDER_PRIVATE_HPP_INCLUDED

#include "gui/widgets/window_builder.hpp"

#include "config.hpp"
#include "gui/widgets/menubar.hpp"

namespace gui2 {

struct tbuilder_control : public tbuilder_widget
{
private:
	tbuilder_control();
public:

	tbuilder_control(const config& cfg);

	void init_control(tcontrol* control) const;

	//! Parameters for the control.
	std::string id;
	std::string definition;
	t_string label;
	t_string tooltip;
	t_string help;
};

struct tbuilder_button : public tbuilder_control
{

private:
	tbuilder_button();
public:
	tbuilder_button(const config& cfg);

	twidget* build () const;

private:
	int retval_;
};

/**
 * A temporary helper class.
 *
 * @todo refactore with the grid builder.
 */
struct tbuilder_gridcell : public tbuilder_widget
{
	tbuilder_gridcell(const config& cfg);

	//! The flags for the cell.
	unsigned flags;

	//! The bordersize for the cell.
	unsigned border_size;

	//! The widgets for the cell.
	tbuilder_widget_ptr widget;

	// We're a dummy the building is done on construction.
	twidget* build () const { return NULL; }
};

struct tbuilder_menubar : public tbuilder_control
{
	tbuilder_menubar(const config& cfg);

	twidget* build () const;

private:
	bool must_have_one_item_selected_;

	tmenubar::tdirection direction_;

	int selected_item_;

	std::vector<tbuilder_gridcell> cells_;

};

struct tbuilder_label : public tbuilder_control
{

private:
	tbuilder_label();
public:
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_label
 *
 * == Label ==
 *
 * A label has no special fields.
 *
 */
	tbuilder_label(const config& cfg) :
		tbuilder_control(cfg)
	{}

	twidget* build () const;

};

struct tbuilder_listbox : public tbuilder_control
{

private:
	tbuilder_listbox();
public:
	tbuilder_listbox(const config& cfg);

	twidget* build () const;

	tbuilder_grid* header;
	tbuilder_grid* footer;

	tbuilder_grid* list_builder;

	/**
	 * Listbox data.
	 *
	 * Contains a vector with the data to set in every cell, it's used to 
	 * serialize the data in the config, so the config is no longer required.
	 */
	std::vector<std::map<std::string /*key*/, t_string/*value*/> >list_data;

	const bool assume_fixed_row_size;
};

struct tbuilder_panel : public tbuilder_control
{

private:
	tbuilder_panel();
public:
	tbuilder_panel(const config& cfg);

	twidget* build () const;

	tbuilder_grid* grid;
};

struct tbuilder_slider : public tbuilder_control
{

private:
	tbuilder_slider();
public:
	tbuilder_slider(const config& cfg);

	twidget* build () const;

private:
	int minimum_value_;
	int maximum_value_;
	int step_size_;
	int value_;

	t_string minimum_value_label_;
	t_string maximum_value_label_;

	std::vector<t_string> value_labels_;
};

struct tbuilder_spacer : public tbuilder_control
{

private:
	tbuilder_spacer();
public:
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_spacer
 *
 * == Spacer ==
 *
 * A spacer is a dummy item to either fill in a widget since no empty items are
 * allowed or to reserve a fixed space. If either the width or the height is not
 * zero the spacer functions as a fixed size spacer.
 *
 * @start_table = config
 *     width (unsigned = 0)            The width of the spacer.
 *     height (unsigned = 0)           The height of the spacer.
 * @end_table
 */
	tbuilder_spacer(const config& cfg) :
		tbuilder_control(cfg),
		width_(lexical_cast_default<unsigned>(cfg["width"])),
		height_(lexical_cast_default<unsigned>(cfg["height"]))
	{}

	twidget* build () const;

private:
	unsigned width_;
	unsigned height_;
};

struct tbuilder_text_box : public tbuilder_control
{
private:
	tbuilder_text_box();
	std::string history_;

public:
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_text_box
 *
 * == Text box ==
 *
 * @start_table = config
 *     label (tstring = "")            The initial text of the text box.
 *     history (string = "")           The name of the history for the text box.
 *                                     A history saves the data entered in a
 *                                     text box between the games. With the up
 *                                     and down arrow it can be accessed. To
 *                                     create a new history item just add a new
 *                                     unique name for this field and the engine
 *                                     will handle the rest.
 * @end_table
 *
 */
	tbuilder_text_box(const config& cfg) :
		tbuilder_control(cfg),
		history_(cfg["history"])
	{}

	twidget* build () const;
};

struct tbuilder_toggle_button : public tbuilder_control
{
private:
	tbuilder_toggle_button();

public:
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_toggle_button
 *
 * == Toggle button ==
 *
 * @start_table = config
 *     icon (f_string = "")            The name of the icon file to show.
 *     return_value (int = 0)          The return value, see
 *                                     [[GUIToolkitWML#Button]] for more info.
 * @end_table
 */
	tbuilder_toggle_button(const config& cfg) :
		tbuilder_control(cfg),
		icon_name_(cfg["icon"]),
		retval_(lexical_cast_default<int>(cfg["return_value"]))
	{}

	twidget* build () const;

private:	
	std::string icon_name_;
	int retval_;
};

struct tbuilder_toggle_panel : public tbuilder_control
{

private:
	tbuilder_toggle_panel();
public:
	tbuilder_toggle_panel(const config& cfg);

	twidget* build () const;

	tbuilder_grid* grid;

private:
	int retval_;
};

struct tbuilder_vertical_scrollbar : public tbuilder_control
{
private:
	tbuilder_vertical_scrollbar();

public:
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_vertical_scrollbar
 *
 * == Vertical scrollbar ==
 *
 * A vertical scrollbar has no special fields.
 *
 */
	tbuilder_vertical_scrollbar(const config& cfg) :
		tbuilder_control(cfg)
	{}

	twidget* build () const;
};

} // namespace gui2

#endif

