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
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "wml_exception.hpp"

#include <cassert>

#define DBG_G LOG_STREAM_INDENT(debug, gui)
#define LOG_G LOG_STREAM_INDENT(info, gui)
#define WRN_G LOG_STREAM_INDENT(warn, gui)
#define ERR_G LOG_STREAM_INDENT(err, gui)

#define DBG_G_D LOG_STREAM_INDENT(debug, gui_draw)
#define LOG_G_D LOG_STREAM_INDENT(info, gui_draw)
#define WRN_G_D LOG_STREAM_INDENT(warn, gui_draw)
#define ERR_G_D LOG_STREAM_INDENT(err, gui_draw)

#define DBG_G_E LOG_STREAM_INDENT(debug, gui_event)
#define LOG_G_E LOG_STREAM_INDENT(info, gui_event)
#define WRN_G_E LOG_STREAM_INDENT(warn, gui_event)
#define ERR_G_E LOG_STREAM_INDENT(err, gui_event)

#define DBG_G_P LOG_STREAM_INDENT(debug, gui_parse)
#define LOG_G_P LOG_STREAM_INDENT(info, gui_parse)
#define WRN_G_P LOG_STREAM_INDENT(warn, gui_parse)
#define ERR_G_P LOG_STREAM_INDENT(err, gui_parse)

namespace gui2 {

struct tbuilder_button : public tbuilder_widget
{

private:
	tbuilder_button();
public:
	tbuilder_button(const config& cfg) : 
		tbuilder_widget(cfg),
		retval_(0)
	{ read_extra(cfg); }

	twidget* build () const;

private:
	int retval_;

	//! After reading the general part in the constructor read extra data.
	void read_extra(const config& cfg);
};

struct tbuilder_label : public tbuilder_widget
{

private:
	tbuilder_label();
public:
	tbuilder_label(const config& cfg) :
		tbuilder_widget(cfg)
	{}

	twidget* build () const;

};

struct tbuilder_text_box : public tbuilder_widget
{

private:
	tbuilder_text_box();
public:
	tbuilder_text_box(const config& cfg) :
		tbuilder_widget(cfg)
	{}

	twidget* build () const;

};


twindow build(CVideo& video, const std::string& type)
{
	std::vector<twindow_builder::tresolution>::const_iterator 
		definition = get_window_builder(type);


	twindow window(video, 100, 100, definition->width, definition->height); // FIXME use proper origin
//	twindow window(video, 0, 0, definition->width, definition->height); // FIXME use proper origin

	const unsigned rows = definition->grid.rows;
	const unsigned cols = definition->grid.cols;

	window.set_rows_cols(rows, cols);

	for(unsigned x = 0; x < rows; ++x) {
		window.set_row_scaling(x, definition->grid.row_scale[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				window.set_col_scaling(y, definition->grid.col_scale[y]);
			}

			twidget* widget = definition->grid.widgets[x * cols + y]->build();
			window.add_child(widget, x, y, definition->grid.flags[x * cols + y],  definition->grid.border_size[x * cols + y]);
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

static unsigned read_flags(const config& cfg)
{
	unsigned flags = 0;

	// Read the flags. FIXME document.
	std::string v_align = cfg["vertical_alignment"];
	if(v_align == "top") {
		flags |= tgrid::VERTICAL_ALIGN_TOP;
	} else if(v_align == "bottom") {
		flags |= tgrid::VERTICAL_ALIGN_BOTTOM;
	} else {
		flags |= tgrid::VERTICAL_ALIGN_CENTER;
	}

	std::string h_align = cfg["horizontal_alignment"];
	if(h_align == "left") {
		flags |= tgrid::HORIZONTAL_ALIGN_LEFT;
	} else if(h_align == "right") {
		flags |= tgrid::HORIZONTAL_ALIGN_RIGHT;
	} else {
		flags |= tgrid::HORIZONTAL_ALIGN_CENTER;
	}

	std::vector<std::string> border = utils::split(cfg["border"]);
	if(std::find(border.begin(), border.end(), "all") != border.end()) {
		flags |= tgrid::BORDER_TOP 
			| tgrid::BORDER_BOTTOM | tgrid::BORDER_LEFT | tgrid::BORDER_RIGHT;
	} else {
		if(std::find(border.begin(), border.end(), "top") != border.end()) {
			flags |= tgrid::BORDER_TOP;
		}
		if(std::find(border.begin(), border.end(), "bottom") != border.end()) {
			flags |= tgrid::BORDER_BOTTOM;
		}
		if(std::find(border.begin(), border.end(), "left") != border.end()) {
			flags |= tgrid::BORDER_LEFT;
		}
		if(std::find(border.begin(), border.end(), "right") != border.end()) {
			flags |= tgrid::BORDER_RIGHT;
		}
	}

	if(utils::string_bool(cfg["vertical_grow"])) {
		flags |= tgrid::VERTICAL_GROW_SEND_TO_CLIENT;
	}

	if(utils::string_bool(cfg["horizontal_grow"])) {
		flags |= tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT;
	}

	return flags;
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

		row_scale.push_back(lexical_cast_default<unsigned>((**row_itor)["scale"]));

		const config::child_list& col_cfgs = (**row_itor).get_children("column");
		for(std::vector<config*>::const_iterator col_itor = col_cfgs.begin();
				col_itor != col_cfgs.end(); ++col_itor) {

			flags.push_back(read_flags(**col_itor));
			border_size.push_back(lexical_cast_default<unsigned>((**col_itor)["border_size"]));
			if(rows == 0) {
				col_scale.push_back(lexical_cast_default<unsigned>((**col_itor)["scale"]));
			}

			if((**col_itor).child("button")) {
				widgets.push_back(new tbuilder_button(*((**col_itor).child("button"))));
			} else if((**col_itor).child("label")) {
				widgets.push_back(new tbuilder_label(*((**col_itor).child("label"))));
			} else if ((**col_itor).child("text_box")) {
				widgets.push_back(new tbuilder_text_box(*((**col_itor).child("text_box"))));
			} else {
				assert(false);
			}

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

tbuilder_widget::tbuilder_widget(const config& cfg) :
	id(cfg["id"]),
	definition(cfg["button_definition"]),
	label(cfg["label"])
{

	if(definition.empty()) {
		definition = "default";
	}


	DBG_G_P << "Window builder: found widget with id '" 
		<< id << "' and definition '" << definition << "'.\n";
}

twidget* tbuilder_button::build() const
{
	tbutton *button = new tbutton();

	button->set_id(id);
	button->set_definition(definition);
	button->set_label(label);

	if(retval_) {
		button->set_retval(retval_);
	} else {
		button->set_retval(tbutton::get_retval_by_id(id));
	}

	DBG_G << "Window builder: placed button '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return button;
}

void tbuilder_button::read_extra(const config& cfg)
{
	retval_ = lexical_cast_default<int>(cfg["return_value"]);
}

twidget* tbuilder_label::build() const
{
	tlabel *tmp_label = new tlabel();

	tmp_label->set_id(id);
	tmp_label->set_definition(definition);
	tmp_label->set_label(label);

	DBG_G << "Window builder: placed label '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return tmp_label;
}

twidget* tbuilder_text_box::build() const
{
	ttext_box *text_box = new ttext_box();

	text_box->set_id(id);
	text_box->set_definition(definition);
	// A textbox doesn't have a label but a text
	text_box->set_text(label);

	DBG_G << "Window builder: placed text box '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return text_box;
}

} // namespace gui2

