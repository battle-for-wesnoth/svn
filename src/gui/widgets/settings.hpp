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

//! @file setting.hpp
//! This file contains the settings handling of the widget library.

#ifndef GUI_WIDGETS_SETTING_HPP_INCLUDED
#define GUI_WIDGETS_SETTING_HPP_INCLUDED

#include "config.hpp"
#include "gui/widgets/canvas.hpp"
#include "gui/widgets/formula.hpp"
#include "gui/widgets/window_builder.hpp"
#include "tstring.hpp"

#include <map>
#include <string>
#include <vector>

namespace gui2 {

/** Do we wish to use the new library or not. */
extern bool new_widgets;

enum twindow_type {
	ADDON_CONNECT,           //<! The addon server connection dialog.
	LANGUAGE_SELECTION,      //<! The language selection dialog.
	MP_CONNECT,              //<! The mp server connection dialog.
	MP_METHOD_SELECTION,     //<! The dialog which allows you to choose the kind
	                         //!  mp game the user wants to play.
	MP_SERVER_LIST,          //<! The mp server list dialog.

	DUMMY                    //<! Dummy always the last one.
};

const std::string& get_id(const twindow_type window_type);

//! Contains the state info for a resolution.
//! Atm all states are the same so there is no need to use inheritance. If that
//! is needed at some point the containers should contain pointers and we should
//! inherit from reference_counted_object.
struct tstate_definition
{
private:
	tstate_definition();

public:
	tstate_definition(const config* cfg);

	bool full_redraw;

	tcanvas canvas;
};


//! Base class of a resolution, contains the common keys for a resolution.
struct tresolution_definition_ : public reference_counted_object
{
private:
	tresolution_definition_();
public:
	tresolution_definition_(const config& cfg);

	unsigned window_width;
	unsigned window_height;

	unsigned min_width;
	unsigned min_height;

	unsigned default_width;
	unsigned default_height;

	unsigned max_width;
	unsigned max_height;

	unsigned text_extra_width;
	unsigned text_extra_height;
	unsigned text_font_size;
	int text_font_style;

	std::vector<tstate_definition> state;
};

struct tcontrol_definition : public reference_counted_object
{
private:
	tcontrol_definition();
public:

	tcontrol_definition(const config& cfg);

	template<class T>
	void load_resolutions(const config::child_list& resolution_list);

	std::string id;
	t_string description;

	std::vector<tresolution_definition_*> resolutions;

};

struct tbutton_definition : public tcontrol_definition
{
	tbutton_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);
	};

};

struct tlabel_definition : public tcontrol_definition
{

	tlabel_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);
	};
};

struct tlistbox_definition : public tcontrol_definition
{

	tlistbox_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);

		// NOTE maybe we need the borders...

		tbuilder_grid* scrollbar;

	};
};

struct tpanel_definition : public tcontrol_definition
{

	tpanel_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);

		unsigned top_border;
		unsigned bottom_border;

		unsigned left_border;
		unsigned right_border;
	};
};

struct tspacer_definition : public tcontrol_definition
{

	tspacer_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);
	};
};

struct ttext_box_definition : public tcontrol_definition
{

	ttext_box_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);

		tformula<unsigned> text_x_offset;
		tformula<unsigned> text_y_offset;
	};

};

struct ttoggle_button_definition : public tcontrol_definition
{
	ttoggle_button_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);
	};
};

struct ttoggle_panel_definition : public tcontrol_definition
{
	ttoggle_panel_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);

		unsigned top_border;
		unsigned bottom_border;

		unsigned left_border;
		unsigned right_border;

		bool state_change_full_redraw;
	};
};

struct ttooltip_definition : public tcontrol_definition
{
	ttooltip_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);
	};
};

struct tvertical_scrollbar_definition : public tcontrol_definition
{
	tvertical_scrollbar_definition(const config& cfg);

	struct tresolution : public tresolution_definition_
	{
		tresolution(const config& cfg);

		unsigned minimum_positioner_length;

		unsigned top_offset;
		unsigned bottom_offset;
	};
};

struct twindow_definition : public tpanel_definition
{
	twindow_definition(const config& cfg);
};

struct tgui_definition
{
	tgui_definition() : 
		id(),
		description(),
		popup_show_delay_(0),
		popup_show_time_(0),
		help_show_time_(0),
		double_click_time_(0)
	{
	}

	std::string id;
	t_string description;

	const std::string& read(const config& cfg);

	//! Activates a gui.
	void activate() const;
	
	typedef std::map <std::string /*control type*/, 
		std::map<std::string /*id*/, tcontrol_definition*> > 
		tcontrol_definition_map;

	tcontrol_definition_map control_definition;

	std::map<std::string, twindow_definition> windows;

	std::map<std::string, twindow_builder> window_types;
private:
	template<class T>
	void load_definitions(const std::string& definition_type, 
		const config::child_list& definition_list);

	unsigned popup_show_delay_;
	unsigned popup_show_time_;
	unsigned help_show_time_;
	unsigned double_click_time_;
};

	tresolution_definition_* get_control(
		const std::string& control_type, const std::string& definition);

	std::vector<twindow_builder::tresolution>::const_iterator 
		get_window_builder(const std::string& type);

	//! Loads the setting for the theme.
	void load_settings();


	// This namespace contains the 'global' settings.
	namespace settings {
		//! The screen resolution should be available for all widgets since their
		//! drawing method will depend on it.
		extern unsigned screen_width;
		extern unsigned screen_height;

		//! These are copied from the active gui.
		extern unsigned popup_show_delay;
		extern unsigned popup_show_time;
		extern unsigned help_show_time;
		extern unsigned double_click_time;
	}

} // namespace gui2

#endif

