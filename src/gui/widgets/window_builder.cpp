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

#include "gui/widgets/window_builder.hpp"

#include "config.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "wml_exception.hpp"

#include <cassert>

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

twindow build(CVideo& video, const std::string& type)
{
	std::vector<twindow_builder::tresolution>::const_iterator 
		definition = get_window_builder(type);


	twindow window(video, 100, 100, definition->width, definition->height);

	const unsigned rows = definition->grid.rows;
	const unsigned cols = definition->grid.cols;

	window.set_rows(rows);
	window.set_cols(cols);

	for(unsigned x = 0; x < rows; ++x) {
		for(unsigned y = 0; y < cols; ++y) {

			tbutton *button = new tbutton();

			const std::string id = definition->grid.widgets[x * cols + y].id;
			const std::string def = definition->grid.widgets[x * cols + y].definition;
			const t_string label = definition->grid.widgets[x * cols + y].label;
			button->set_definition(id);
			button->set_definition(def);
			button->set_label(label);
			window.add_child(button, x, y);

			DBG_G << "Window builder: placed button '" << id << "' with defintion '" 
				<< def << "' at " << x << ", " << y << '\n';

		}
	}

	return window;
}

namespace {

} // namespace


const std::string& twindow_builder::read(const config& cfg)
{
/*WIKI
 * [window]
 *
 * A window defines how a window looks in the game.
 *
 *
 *     id = (string = "")            Unique id for this window.
 *     description = (t_string = "") Unique translatable name for this window.
 *
 *
 *
 * [/window]
 */


	id_ = cfg["id"];
	description_ = cfg["description"];

	VALIDATE(!id_.empty(), missing_mandatory_wml_key("window", "id"));
	VALIDATE(!description_.empty(), missing_mandatory_wml_key("window", "description"));

	DBG_G_P << "Window builder: reading data for window " << id_ << ".\n";

	const config::child_list& cfgs = cfg.get_children("resolution");
	VALIDATE(!cfgs.empty(), _("No resolution defined."));
	for(std::vector<config*>::const_iterator itor = cfgs.begin();
			itor != cfgs.end(); ++itor) {

		resolutions.push_back(tresolution(**itor));
	}


	return id_;
}

twindow_builder::tresolution::tresolution(const config& cfg) :
	window_width(lexical_cast_default<unsigned>(cfg["window_width"])),
	window_height(lexical_cast_default<unsigned>(cfg["window_height"])),
	width(lexical_cast_default<unsigned>(cfg["width"])),
	height(lexical_cast_default<unsigned>(cfg["height"])),
	definition(cfg["window_definition"]),
	grid(cfg.child("grid"))
{
/*WIKI
 * [resolution]
 *     window_width = (unsigned = 0) Width of the application window.
 *     window_height = (unsigned = 0) 
 *                                   Height of the application window.
 *     width = (unsigned)            Width of the window to show.
 *     height = (unsigned)           Height of the window to show.
 *
 *     window_definition = (string = "default")
 *                                   Definition of the window which we want to show.
 *
 *     [grid]                        The grid with the widgets to show.
 *
 * [/resolution]
 */

	DBG_G_P << "Window builder: parsing resolution " 
		<< window_width << ',' << window_height << '\n';

	if(definition.empty()) {
		definition = "default";
	}
	
}

twindow_builder::tresolution::tgrid::tgrid(const config* cfg) : 
	rows(0),
	cols(0),
	widgets()
{
	VALIDATE(cfg, _("No grid defined."));

	const config::child_list& row_cfgs = cfg->get_children("row");
	for(std::vector<config*>::const_iterator row_itor = row_cfgs.begin();
			row_itor != row_cfgs.end(); ++row_itor) {

		unsigned col = 0;

		const config::child_list& col_cfgs = (**row_itor).get_children("column");
		for(std::vector<config*>::const_iterator col_itor = col_cfgs.begin();
				col_itor != col_cfgs.end(); ++col_itor) {

			assert((**col_itor).child("button"));
			widgets.push_back(twidget(*((**col_itor).child("button"))));

			++col;
		}

		++rows;
		if(row_itor == row_cfgs.begin()) {
			cols = col;
		} else {
			VALIDATE(col, _("A row must have a column."));
			VALIDATE(col == cols, _("Number of columns differ."));
		}

	}

	DBG_G_P << "Window builder: grid has " 
		<< rows << " rows and " << cols << " columns.\n";
}

twindow_builder::tresolution::tgrid::twidget::twidget(const config& cfg) :
	id(cfg["id"]),
	definition(cfg["button_definition"]),
	label(cfg["label"])
{

	if(definition.empty()) {
		definition = "default";
	}

	DBG_G_P << "Window builder: found button with id '" 
		<< id << "' and definition '" << definition << "'.\n";
	
}

} // namespace gui2

