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

#define NEW_DRAW

#include "gui/widgets/window_builder_private.hpp"

#include "foreach.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/horizontal_scrollbar.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/listbox.hpp"
#ifdef NEW_DRAW
#include "gui/widgets/generator.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"
#include "gui/widgets/window.hpp"


namespace gui2 {

namespace {

unsigned get_v_align(const std::string& v_align)
{
	if(v_align == "top") {
		return tgrid::VERTICAL_ALIGN_TOP;
	} else if(v_align == "bottom") {
		return tgrid::VERTICAL_ALIGN_BOTTOM;
	} else {
		if(!v_align.empty() && v_align != "center") {
			ERR_G_E << "Invalid vertical alignment '" 
				<< v_align << "' falling back to 'center'.\n";
		}
		return tgrid::VERTICAL_ALIGN_CENTER;
	}
}

unsigned get_h_align(const std::string& h_align)
{
	if(h_align == "left") {
		return tgrid::HORIZONTAL_ALIGN_LEFT;
	} else if(h_align == "right") {
		return tgrid::HORIZONTAL_ALIGN_RIGHT;
	} else {
		if(!h_align.empty() && h_align != "center") {
			ERR_G_E << "Invalid horizontal alignment '" 
				<< h_align << "' falling back to 'center'.\n";
		}
		return tgrid::HORIZONTAL_ALIGN_CENTER;
	}
}

unsigned get_border(const std::vector<std::string>& border)
{
	if(std::find(border.begin(), border.end(), "all") != border.end()) {
		return tgrid::BORDER_TOP 
			| tgrid::BORDER_BOTTOM | tgrid::BORDER_LEFT | tgrid::BORDER_RIGHT;
	} else {
		if(std::find(border.begin(), border.end(), "top") != border.end()) {
			return tgrid::BORDER_TOP;
		}
		if(std::find(border.begin(), border.end(), "bottom") != border.end()) {
			return tgrid::BORDER_BOTTOM;
		}
		if(std::find(border.begin(), border.end(), "left") != border.end()) {
			return tgrid::BORDER_LEFT;
		}
		if(std::find(border.begin(), border.end(), "right") != border.end()) {
			return tgrid::BORDER_RIGHT;
		}
	}

	return 0;
}

unsigned read_flags(const config& cfg)
{
	unsigned flags = 0;

	const unsigned v_flags = get_v_align(cfg["vertical_alignment"]);
	const unsigned h_flags = get_h_align(cfg["horizontal_alignment"]);
	flags |= get_border( utils::split(cfg["border"]));

	if(utils::string_bool(cfg["vertical_grow"])) {
		flags |= tgrid::VERTICAL_GROW_SEND_TO_CLIENT;

		if(! (cfg["vertical_alignment"]).empty()) {
			ERR_G_P << "vertical_grow and vertical_alignment "
				"can't be combined, alignment is ignored.\n";
		}
	} else {
		flags |= v_flags;
	}

	if(utils::string_bool(cfg["horizontal_grow"])) {
		flags |= tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT;

		if(! (cfg["horizontal_alignment"]).empty()) {
			ERR_G_P << "horizontal_grow and horizontal_alignment "
				"can't be combined, alignment is ignored.\n";
		}
	} else {
		flags |= h_flags;
	}

	return flags;
}

tmenubar::tdirection read_direction(const std::string& direction)
{
	if(direction == "vertical") {
		return tmenubar::VERTICAL;
	} else if(direction == "horizontal") {
		return tmenubar::HORIZONTAL;
	} else {
		ERR_G_E << "Invalid direction " 
				<< direction << "' falling back to 'horizontal'.\n";
		return tmenubar::HORIZONTAL;
	}
}

tvertical_scrollbar_container_::tscrollbar_mode  
		get_scrollbar_mode(const std::string& scrollbar_mode)
{
	if(scrollbar_mode == "always") {
		return tvertical_scrollbar_container_::SHOW;
	} else if(scrollbar_mode == "never") {
		return tvertical_scrollbar_container_::HIDE;
	} else {
		if(!scrollbar_mode.empty() && scrollbar_mode != "auto") {
			ERR_G_E << "Invalid scrollbar mode '" 
				<< scrollbar_mode << "' falling back to 'auto'.\n";
		}
		return tvertical_scrollbar_container_::SHOW_WHEN_NEEDED;
	}
}

tbuilder_widget_ptr create_builder_widget(const config& cfg)
{
	if(cfg.all_children().size() != 1) {
		ERR_G_P << "Grid cell has " << cfg.all_children().size()
			<< " children instead of 1, aborting. Config :\n"
			<< cfg;
		assert(false);
	}

	if(cfg.child("button")) {
		return new tbuilder_button(*(cfg.child("button")));
	} else if(cfg.child("horizontal_scrollbar")) {
		return new tbuilder_horizontal_scrollbar(
				*(cfg.child("horizontal_scrollbar")));

	} else if(cfg.child("image")) {
		return new tbuilder_image(*(cfg.child("image")));
	} else if(cfg.child("label")) {
		return new tbuilder_label(*(cfg.child("label")));
	} else if(cfg.child("listbox")) {
		return new tbuilder_listbox(*(cfg.child("listbox")));
	} else if(cfg.child("menubar")) {
		return new tbuilder_menubar(*(cfg.child("menubar")));
	} else if(cfg.child("minimap")) {
		return new tbuilder_minimap(*(cfg.child("minimap")));
	} else if(cfg.child("panel")) {
		return new tbuilder_panel(*(cfg.child("panel")));
	} else if(cfg.child("scroll_label")) {
		return new tbuilder_scroll_label(*(cfg.child("scroll_label")));
	} else if(cfg.child("slider")) {
		return new tbuilder_slider(*(cfg.child("slider")));
	} else if(cfg.child("spacer")) {
		return new tbuilder_spacer(*(cfg.child("spacer")));
	} else if(cfg.child("text_box")) {
		return new tbuilder_text_box(*(cfg.child("text_box")));
	} else if(cfg.child("toggle_button")) {
		return new tbuilder_toggle_button(*(cfg.child("toggle_button")));
	} else if(cfg.child("toggle_panel")) {
		return new tbuilder_toggle_panel(*(cfg.child("toggle_panel")));
	} else if(cfg.child("vertical_scrollbar")) {
		return new tbuilder_vertical_scrollbar(
				*(cfg.child("vertical_scrollbar")));

	} else if(cfg.child("grid")) {
		return new tbuilder_grid(*(cfg.child("grid")));
	} else {
		std::cerr << cfg;
		assert(false);
	}
}

} // namespace

twindow build(CVideo& video, const std::string& type)
{
	std::vector<twindow_builder::tresolution>::const_iterator 
		definition = get_window_builder(type);

	// We set the values from the definition since we can only determine the 
	// best size (if needed) after all widgets have been placed.
	twindow window(video, 
		definition->x, definition->y, definition->width, definition->height, 
		definition->automatic_placement, 
		definition->horizontal_placement, definition->vertical_placement,
		definition->definition);

	log_scope2(gui, "Window builder: building grid for window");

	window.set_easy_close(definition->easy_close);

	const unsigned rows = definition->grid->rows;
	const unsigned cols = definition->grid->cols;

	window.set_rows_cols(rows, cols);

	for(unsigned x = 0; x < rows; ++x) {
		window.set_row_grow_factor(x, definition->grid->row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				window.set_col_grow_factor(y, definition->grid->col_grow_factor[y]);
			}

			twidget* widget = definition->grid->widgets[x * cols + y]->build();
			window.set_child(widget, x, y, 
				definition->grid->flags[x * cols + y], 
				definition->grid->border_size[x * cols + y]);
		}
	}

	window.add_to_keyboard_chain(&window);

	return window;
}

const std::string& twindow_builder::read(const config& cfg)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1_window
 *
 * = Window definition =
 *
 * A window defines how a window looks in the game.
 *
 * @start_table = config
 *     id (string)                   Unique id for this window.
 *     description (t_string)        Unique translatable name for this window.
 *
 *     resolution (section)          The definitions of the window in various
 *                                   resolutions.
 * @end_table
 *
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
	automatic_placement(utils::string_bool(cfg["automatic_placement"], true)),
	x(cfg["x"]),
	y(cfg["y"]),
	width(cfg["width"]),
	height(cfg["height"]),
	vertical_placement(get_v_align(cfg["vertical_placement"])),
	horizontal_placement(get_h_align(cfg["horizontal_placement"])),
	easy_close(utils::string_bool(cfg["easy_close"])),
	definition(cfg["definition"]),
	grid(0)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1_window
 *
 * == Resolution ==
 *
 * @start_table = config
 *     window_width (unsigned = 0)   Width of the application window.
 *     window_height (unsigned = 0)  Height of the application window.
 *
 *     automatic_placement (bool = true)
 *                                   Automatically calculate the best size for
 *                                   the window and place it. If automatically
 *                                   placed ''vertical_placement'' and
 *                                   ''horizontal_placement'' can be used to
 *                                   modify the final placement. If not
 *                                   automatically placed the ''width'' and
 *                                   ''height'' are mandatory.
 *
 *     x (f_unsigned = 0)            X coordinate of the window to show.
 *     y (f_unsigned = 0)            Y coordinate of the window to show.
 *     width (f_unsigned = 0)        Width of the window to show.
 *     height (f_unsigned = 0)       Height of the window to show.
 *
 *     vertical_placement (v_align = "")
 *                                   The vertical placement of the window.
 *     horizontal_placement (h_align = "")
 *                                   The horizontal placement of the window.
 *
 *     easy_close (bool = false)     Does the window need easy close behaviour?
 *                                   Easy close behaviour means that any mouse
 *                                   click will close the dialog. Note certain
 *                                   widgets will automatically disable this
 *                                   behaviour since they need to process the
 *                                   clicks as well, for example buttons do need
 *                                   a click and a missclick on button shouldn't
 *                                   close the dialog. NOTE with some widgets
 *                                   this behaviour depends on their contents
 *                                   (like scrolling labels) so the behaviour
 *                                   might get changed depending on the data in
 *                                   the dialog. NOTE the default behaviour
 *                                   might be changed since it will be disabled
 *                                   when can't be used due to widgets which use
 *                                   the mouse, including buttons, so it might
 *                                   be wise to set the behaviour explicitly
 *                                   when not wanted and no mouse using widgets
 *                                   are available. This means enter, escape or
 *                                   an external source needs to be used to
 *                                   close the dialog (which is valid).
 *
 *     definition (string = "default")
 *                                   Definition of the window which we want to 
 *                                   show.
 *
 *     grid (section)                The grid with the widgets to show. FIXME 
 *                                   the grid needs its own documentation page.
 * @end_table
 *
 * The size variables are copied to the window and will be determined runtime.
 * This is needed since the main window can be resized and the dialog needs to
 * resize accordingly. The following variables are available:
 * @start_table = formula
 *     screen_width unsigned         The usable width of the wesnoth main window.
 *     screen_height unsigned        The usable height of the wesnoth main window.
 * @end_table
 */

	VALIDATE(cfg.child("grid"), _("No grid defined."));

	grid = new tbuilder_grid(*(cfg.child("grid")));

	if(!automatic_placement) {
		VALIDATE(width.has_formula() || width(), 
			missing_mandatory_wml_key("resolution", "width"));
		VALIDATE(height.has_formula() || height(), 
			missing_mandatory_wml_key("resolution", "height"));
	}

	DBG_G_P << "Window builder: parsing resolution " 
		<< window_width << ',' << window_height << '\n';

	if(definition.empty()) {
		definition = "default";
	}
	
}

tbuilder_grid::tbuilder_grid(const config& cfg) : 
	tbuilder_widget(cfg),
	id(cfg["id"]),
	rows(0),
	cols(0),
	row_grow_factor(),
	col_grow_factor(),
	flags(),
	border_size(),
	widgets()
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 2_cell
 *
 * = Cell =
 *
 * Every grid cell has some cell configuration values and one widget in the grid
 * cell. Here we describe the what is available more information about the usage
 * can be found here [[GUILayout]].
 *
 * == Row values ==
 *
 * For every row the following variables are available:
 *
 * @start_table = config
 *     grow_factor (unsigned = 0)      The grow factor for a row.
 * @end_table
 *
 * == Cell values ==
 *
 * For every column the following variables are available:
 * @start_table = config
 *     grow_factor (unsigned = 0)      The grow factor for a column, this value
 *                                     is only read for the first row.
 *
 *     border_size (unsigned = 0)      The border size for this grid cell.
 *     border (border = "")            Where to place the border in this grid
 *                                     cell.
 *
 *     vertical_alignment (v_align = "")
 *                                     The vertical alignment of the widget in
 *                                     the grid cell. (This value is ignored if
 *                                     vertical_grow is true.)
 *     horizontal_alignment (h_align = "")
 *                                     The horizontal alignment of the widget in
 *                                     the grid cell.(This value is ignored if
 *                                     horizontal_grow is true.)
 *    
 *     vertical_grow (bool = false)    Does the widget grow in vertical
 *                                     direction when the grid cell grows in the
 *                                     vertical directon. This is used if the
 *                                     grid cell is wider as the best width for
 *                                     the widget.
 *     horizontal_grow (bool = false)  Does the widget grow in horizontal
 *                                     direction when the grid cell grows in the
 *                                     horizontal directon. This is used if the
 *                                     grid cell is higher as the best width for
 *                                     the widget.
 * @end_table
 *
 * == Widget ==
 *
 * The widget is one of the following items:
 * * button a button.
 * * image an image.
 * * grid a grid, this is used to nest items.
 * * horizontal_scrollbar a horizontal scrollbar.
 * * label a label.
 * * listbox a listbox.
 * * panel a panel (a grid which can be drawn on).
 * * slider a slider.
 * * spacer a filler item. 
 * * text_box a text box.
 * * vertical_scrollbar a vertical scrollbar.
 *
 * More details about the widgets is in the next section.
 *
 */
	log_scope2(gui_parse, "Window builder: parsing a grid");

	const config::child_list& row_cfgs = cfg.get_children("row");
	for(std::vector<config*>::const_iterator row_itor = row_cfgs.begin();
			row_itor != row_cfgs.end(); ++row_itor) {

		unsigned col = 0;

		row_grow_factor.push_back(lexical_cast_default<unsigned>((**row_itor)["grow_factor"]));

		const config::child_list& col_cfgs = (**row_itor).get_children("column");
		for(std::vector<config*>::const_iterator col_itor = col_cfgs.begin();
				col_itor != col_cfgs.end(); ++col_itor) {

			flags.push_back(read_flags(**col_itor));
			border_size.push_back(lexical_cast_default<unsigned>((**col_itor)["border_size"]));
			if(rows == 0) {
				col_grow_factor.push_back(lexical_cast_default<unsigned>((**col_itor)["grow_factor"]));
			}

			widgets.push_back(create_builder_widget(**col_itor));

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

tbuilder_control::tbuilder_control(const config& cfg) :
	tbuilder_widget(cfg),
	id(cfg["id"]),
	definition(cfg["definition"]),
	label(cfg["label"]),
	tooltip(cfg["tooltip"]),
	help(cfg["help"]),
	use_tooltip_on_label_overflow(
		utils::string_bool("use_tooltip_on_label_overflow", true))
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget
 *
 * = Widget =
 *
 * All widgets placed in the cell have some values in common:
 * @start_table = config
 *     id (string = "")                This value is used for the engine to
 *                                     identify 'special' items. This means that
 *                                     for example a text_box can get the proper
 *                                     initial value. This value should be
 *                                     unique or empty. Those special values are
 *                                     documented at the window definition that
 *                                     uses them. NOTE items starting with an
 *                                     underscore are used for composed witdgets
 *                                     and these should be unique per composed
 *                                     widget.
 *
 *     definition (string = "default") The id of the widget definition to use.
 *                                     This way it's possible to select a
 *                                     specific version of the widget eg a title
 *                                     label when the label is used as title.
 *
 *     label (tstring = "")            Most widgets have some text accosiated
 *                                     with them, this field contain the value
 *                                     of that text. Some widgets use this value
 *                                     for other purposes, this is documented
 *                                     at the widget.
 *
 *     tooptip (tstring = "")          If you hover over a widget a while (the 
 *                                     time it takes can differ per widget) a
 *                                     short help can show up.This defines the
 *                                     text of that message.
 *
 *
 *     help (tstring = "")             If you hover over a widget and press F1 a
 *                                     help message can show up. This help
 *                                     message might be the same as the tooltip
 *                                     but in general (if used) this message
 *                                     should show more help. This defines the
 *                                     text of that message.
 *
 *    use_tooltip_on_label_overflow (bool = true)
 *                                     If the text on the label is truncated and
 *                                     the tooltip is empty the label can be
 *                                     used for the tooltip. If this variale is
 *                                     set to true this will happen.
 * @end_table
 *
 */

	if(definition.empty()) {
		definition = "default";
	}


	DBG_G_P << "Window builder: found control with id '" 
		<< id << "' and definition '" << definition << "'.\n";
}

void tbuilder_control::init_control(tcontrol* control) const
{
	assert(control);

	control->set_id(id);
	control->set_definition(definition);
	control->set_label(label);
	control->set_tooltip(tooltip);
	control->set_help_message(help);
	control->set_use_tooltip_on_label_overflow(use_tooltip_on_label_overflow);
}

tbuilder_button::tbuilder_button(const config& cfg) :
	tbuilder_control(cfg),
	retval_(lexical_cast_default<int>(cfg["return_value"]))
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_button
 *
 * == Button ==
 *
 * Definition of a button. When a button has a return value it sets the retour
 * value for the window. Normally this closes the window and returns this value
 * to the caller. The return value can either be defined by the user or
 * determined from the id of the button. The return value has a higher
 * precedence as the one defined by the id. (Of course it's weird to give a
 * button an id and then override it's return value.)
 *
 * List with the button specific variables:
 * @start_table = config
 *     return_value (int = 0)          The return value.
 *
 * @end_table
 *
 */
}

twidget* tbuilder_button::build() const
{
	tbutton* button = new tbutton();

	init_control(button);

	if(retval_) {
		button->set_retval(retval_);
	} else {
		button->set_retval(twindow::get_retval_by_id(id));
	}

	DBG_GUI << "Window builder: placed button '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return button;
}

twidget* tbuilder_horizontal_scrollbar::build() const
{
	thorizontal_scrollbar *horizontal_scrollbar = new thorizontal_scrollbar();

	init_control(horizontal_scrollbar);

	DBG_GUI << "Window builder:"
		<< " placed horizontal scrollbar '" << id
		<< "' with defintion '" << definition 
		<< "'.\n";

	return horizontal_scrollbar;
}

twidget* tbuilder_image::build() const
{
	timage* widget = new timage();

	init_control(widget);

	DBG_GUI << "Window builder: placed image '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return widget;
}

tbuilder_gridcell::tbuilder_gridcell(const config& cfg) :
	tbuilder_widget(cfg),
	flags(read_flags(cfg)),
	border_size(lexical_cast_default<unsigned>((cfg)["border_size"])),
	widget(create_builder_widget(cfg))
{
}

twidget* tbuilder_label::build() const
{
	tlabel* tmp_label = new tlabel();

	init_control(tmp_label);

	DBG_GUI << "Window builder: placed label '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return tmp_label;
}

tbuilder_listbox::tbuilder_listbox(const config& cfg) :
	tbuilder_control(cfg),
	scrollbar_mode(get_scrollbar_mode(cfg["vertical_scrollbar_mode"])),
	header(cfg.child("header") ? new tbuilder_grid(*(cfg.child("header"))) : 0),
	footer(cfg.child("footer") ? new tbuilder_grid(*(cfg.child("footer"))) : 0),
	list_builder(0),
	list_data(),
	assume_fixed_row_size(utils::string_bool(cfg["assume_fixed_row_size"]))
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_listbox
 *
 * == Listbox ==
 *
 * Definition of a listbox. 
 *
 * List with the listbox specific variables:
 * @start_table = config
 *     vertical_scrollbar_mode (scrollbar_mode = auto)
 *                                     Determines whether or not to show the 
 *                                     scrollbar.
 *
 *     header (section = [])           Defines the grid for the optional header.
 *     footer (section = [])           Defines the grid for the optional footer.
 *    
 *     list_definition (section)       This defines how a listboxs list data 
 *                                     looks. It must contain the grid
 *                                     definition for 1 row of the list.
 *
 *     list_data(section = [])         A grid alike section which stores the
 *                                     initial data for the listbox. Every row
 *                                     must have the same number of columns as
 *                                     the 'list_definition'.
 *
 *     assume_fixed_row_size (bool = true)    
 *                                     If not all rows can be shown this value
 *                                     becomes important. If fixed size we
 *                                     always show X rows and no half rows are
 *                                     shown. This doesn't mean the rows need to
 *                                     be fixed size eg the addon dialog might
 *                                     get the option to show verbose info in
 *                                     the same listbox in that case it's still
 *                                     allowed to set the value.
 *
 * @end_table
 *
 *
 * Inside the list section there are only the following widgets allowed
 * * grid (to nest)
 * * selectable widgets which are
 * ** toggle_button 
 * ** toggle_panel 
 *
 */

	VALIDATE(cfg.child("list_definition"), _("No list defined."));
	list_builder = new tbuilder_grid(*(cfg.child("list_definition")));
	assert(list_builder);
	VALIDATE(list_builder->rows == 1, _("A 'list_definition' should contain one row."));

	const config *data = cfg.child("list_data");
	if(data) {

		const config::child_list& row_cfgs = data->get_children("row");
		for(std::vector<config*>::const_iterator row_itor = row_cfgs.begin();
				row_itor != row_cfgs.end(); ++row_itor) {

			unsigned col = 0;

			const config::child_list& col_cfgs = (**row_itor).get_children("column");
			for(std::vector<config*>::const_iterator col_itor = col_cfgs.begin();
					col_itor != col_cfgs.end(); ++col_itor) {

				list_data.push_back((**col_itor).values);
				++col;
			}
			
			VALIDATE(col == list_builder->cols, _("'list_data' must have "
				"the same number of columns as the 'list_definition'."));
		}
	}
}

twidget* tbuilder_listbox::build() const
{
#ifndef NEW_DRAW
	tlistbox *listbox = new tlistbox();
#else
	tlistbox *listbox = new tlistbox(
			true, true, tgenerator_::vertical_list, true);
#endif
	init_control(listbox);

	listbox->set_list_builder(list_builder); // FIXME in finalize???
#ifndef NEW_DRAW
	listbox->set_assume_fixed_row_size(assume_fixed_row_size);
	listbox->set_scrollbar_mode(scrollbar_mode);
#else
	// scrollbar mode
#endif
	DBG_GUI << "Window builder: placed listbox '" << id << "' with defintion '" 
		<< definition << "'.\n";

	boost::intrusive_ptr<const tlistbox_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const tlistbox_definition::tresolution>(listbox->config());
	assert(conf);


#ifdef NEW_DRAW
	conf->grid->build(&listbox->grid());

	listbox->finalize(header, footer, list_data);
#else	
	/*
	 * We generate the following items to put in the listbox grid
	 * - _scrollbar_grid the grid containing the scrollbar.
	 * - _content_grid   the grid containing the content of the listbox.
	 * - _list           if the content has a header of footer they're an extra
	 *                   grid needed to find the scrolling content, the item 
	 *                   with the id _list holds this, so the listbox needs to
	 *                   test for this item as well.
	 */

	tgrid* scrollbar = dynamic_cast<tgrid*>(conf->scrollbar->build());
	assert(scrollbar);

	scrollbar->set_id("_scrollbar_grid");

	tgrid* content_grid = new tgrid();
	content_grid->set_definition("default");
	content_grid->set_id("_content_grid");
	assert(content_grid);

	if(header || footer) {

		content_grid->set_rows_cols(header && footer ? 3 : 2, 1);

		// Create and add the header.
		if(header) {
			twidget* widget = header->build();
			assert(widget);

			/**
			 * @todo
			 *
			 * We need sort indicators, which are tristat_buttons;
			 * none, acending, decending. Once we have them we can write them in.
			 */
			content_grid->set_child(widget, 0, 0, tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT 
				| tgrid::VERTICAL_ALIGN_TOP, 0);
		}

		// Create and add the footer.
		if(footer) {
			twidget* widget = footer->build();
			assert(widget);

			content_grid->set_child(widget, header && footer ? 2 : 1, 0, 
				tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT
				| tgrid::VERTICAL_ALIGN_BOTTOM, 0);
		}

		// Add the list itself which needs a new grid as described above.
		tgrid* list = new tgrid();
		assert(list);

		list->set_definition("default");
		list->set_id("_list");
		content_grid->set_child(list, header ? 1 : 0, 0, 
			tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT 
			| tgrid::VERTICAL_GROW_SEND_TO_CLIENT
			, 0);
		content_grid->set_row_grow_factor( header ? 1 : 0, 1);
	}

	listbox->grid().set_rows_cols(1, 2);
	listbox->grid().set_child(content_grid, 0, 0, 
		tgrid::VERTICAL_GROW_SEND_TO_CLIENT 
		| tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT 
		, 0);
	listbox->grid().set_col_grow_factor(0, 1);
	listbox->grid().set_child(scrollbar, 0, 1, 
		tgrid::VERTICAL_GROW_SEND_TO_CLIENT
		| tgrid::HORIZONTAL_ALIGN_CENTER
		, 0);

	if(!list_data.empty()) {
		listbox->add_rows(list_data);
	}

	listbox->finalize_setup();
#endif

	return listbox;
}

tbuilder_menubar::tbuilder_menubar(const config& cfg) :
	tbuilder_control(cfg),
	must_have_one_item_selected_(utils::string_bool(cfg["must_have_one_item_selected"])),
	direction_(read_direction(cfg["direction"])),
	selected_item_(lexical_cast_default<int>(
		cfg["selected_item"], must_have_one_item_selected_ ? 0 : -1)),
	cells_()
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_menubar
 *
 * == Menubar ==
 *
 * A menu bar used for menus and tab controls.
 *
 * List with the listbox specific variables:
 * @start_table = config
 *     must_have_one_item_selected (bool = false)
 *                                     Does the menu always have one item
 *                                     selected. This makes sense for tabsheets
 *                                     but not for menus.
 *     direction (direction = "")      The direction of the menubar.
 *     selected_item(int = -1)         The item to select upon creation, when
 *                                     'must_have_one_item_selected' is true the
 *                                     default value is 0 instead of -1. -1
 *                                     means no item selected.
 * @end_table
 */
	const config* data = cfg.child("data");

	if(data) {
		foreach(const config* cell, data->get_children("cell")) {
			cells_.push_back(tbuilder_gridcell(*cell));
		}
	}
}

twidget* tbuilder_menubar::build() const
{
	tmenubar* menubar = new tmenubar(direction_);

	init_control(menubar);


	DBG_GUI << "Window builder: placed menubar '" << id << "' with defintion '" 
		<< definition << "'.\n";

	if(direction_ == tmenubar::HORIZONTAL) {
		menubar->set_rows_cols(1, cells_.size());

		for(size_t i = 0; i < cells_.size(); ++i) {
			menubar->set_child(cells_[i].widget->build(), 
				0, i, cells_[i].flags, cells_[i].border_size);
		}
	} else {
		// vertical growth
		menubar->set_rows_cols(cells_.size(), 1);

		for(size_t i = 0; i < cells_.size(); ++i) {
			menubar->set_child(cells_[i].widget->build(), 
				i, 0, cells_[i].flags, cells_[i].border_size);
		}
	}

	menubar->set_selected_item(selected_item_);
	menubar->set_must_select(must_have_one_item_selected_);
	menubar->finalize_setup();

	return menubar;
}

twidget* tbuilder_minimap::build() const
{
	tminimap* minimap = new tminimap();

	init_control(minimap);

	DBG_GUI << "Window builder: placed minimap '" << id << "' with defintion '" 
		<< definition << "'.\n";
	
	return minimap;
}

tbuilder_panel::tbuilder_panel(const config& cfg) :
	tbuilder_control(cfg),
	grid(0)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_panel
 *
 * == Panel ==
 *
 * A panel is an item which can hold other items. The difference between a grid
 * and a panel is that it's possible to define how a panel looks. A grid in an
 * invisible container to just hold the items.
 *
 * @start_table = config
 *     grid (section)                  Defines the grid with the widgets to
 *                                     place on the panel.
 * @end_table                                   
 *
 */
	VALIDATE(cfg.child("grid"), _("No grid defined."));

	grid = new tbuilder_grid(*(cfg.child("grid")));
}

twidget* tbuilder_panel::build() const
{
	tpanel* panel = new tpanel();

	init_control(panel);

	DBG_GUI << "Window builder: placed panel '" << id << "' with defintion '" 
		<< definition << "'.\n";


	log_scope2(gui, "Window builder: building grid for panel.");

	const unsigned rows = grid->rows;
	const unsigned cols = grid->cols;

	panel->set_rows_cols(rows, cols);

	for(unsigned x = 0; x < rows; ++x) {
		panel->set_row_grow_factor(x, grid->row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				panel->set_col_grow_factor(y, grid->col_grow_factor[y]);
			}

			twidget* widget = grid->widgets[x * cols + y]->build();
			panel->set_child(widget, x, y, grid->flags[x * cols + y],  grid->border_size[x * cols + y]);
		}
	}

	return panel;
}

twidget* tbuilder_scroll_label::build() const
{
	tscroll_label* widget = new tscroll_label();

	init_control(widget);

	boost::intrusive_ptr<const tscroll_label_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const tscroll_label_definition::tresolution>(widget->config());
	assert(conf);

#ifdef NEW_DRAW
	conf->grid->build(&widget->grid());
#else	
	tgrid* grid = dynamic_cast<tgrid*>(conf->grid->build());
	assert(grid);

	widget->grid().set_rows_cols(1, 1);
	widget->grid().set_child(grid, 0, 0, 
		tgrid::VERTICAL_GROW_SEND_TO_CLIENT 
		| tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT 
		, 0);
#endif
	widget->finalize_setup();

	DBG_GUI << "Window builder: placed scroll label '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return widget;
}

tbuilder_slider::tbuilder_slider(const config& cfg) :
	tbuilder_control(cfg),
	best_slider_length_(lexical_cast_default<unsigned>(cfg["best_slider_length"])),
	minimum_value_(lexical_cast_default<int>(cfg["minimum_value"])),
	maximum_value_(lexical_cast_default<int>(cfg["maximum_value"])),
	step_size_(lexical_cast_default<unsigned>(cfg["step_size"])),
	value_(lexical_cast_default<unsigned>(cfg["value"])),
	minimum_value_label_(cfg["minimum_value_label"]),
	maximum_value_label_(cfg["maximum_value_label"]),
	value_labels_()
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_slider
 *
 * == Slider ==
 *
 * @start_table = config
 *     best_slider_length (unsigned = 0)
 *                                    The best length for the sliding part.
 *     minimum_value (int = 0)        The minimum value the slider can have.
 *     maximum_value (int = 0)        The maximum value the slider can have.
 *
 *     step_size (unsigned = 0)       The number of items the slider's value
 *                                    increases with one step.
 *     value (int = 0)                The value of the slider.
 *
 *     minimum_value_label (t_string = "")
 *                                    If the minimum value is choosen there
 *                                    might be the need for a special value (eg
 *                                    off). When this key has a value that value
 *                                    will be shown if the minimum is selected.
 *     maximum_value_label (t_string = "")
 *                                    If the maximum value is choosen there
 *                                    might be the need for a special value (eg
 *                                    unlimited)). When this key has a value
 *                                    that value will be shown if the maximum is
 *                                    selected.
 *     value_labels ([])              It might be the labels need to be shown
 *                                    are not a lineair number sequence eg (0.5,
 *                                    1, 2, 4) in that case for all items this
 *                                    section can be filled with the values,
 *                                    which should be the same number of items
 *                                    as the items in the slider. NOTE if this
 *                                    option is used, 'minimum_value_label' and
 *                                    'maximum_value_label' are ignored.
 * @end_table
 */
	const config* labels = cfg.child("value_labels");
	if(labels) {

		const config::child_list& value = labels->get_children("value");
		foreach(const config* label, value) {

			value_labels_.push_back((*label)["label"]);
		}
	}
}

twidget* tbuilder_slider::build() const
{
	tslider* slider = new tslider();

	init_control(slider);

	slider->set_best_slider_length(best_slider_length_);
	slider->set_maximum_value(maximum_value_);
	slider->set_minimum_value(minimum_value_);
	slider->set_step_size(step_size_);
	slider->set_value(value_);

	if(!value_labels_.empty()) {
		VALIDATE(value_labels_.size() == slider->get_item_count(),
			_("The number of value_labels and values don't match."));

		slider->set_value_labels(value_labels_);

	} else {
		slider->set_minimum_value_label(minimum_value_label_);
		slider->set_maximum_value_label(maximum_value_label_);
	}

	DBG_GUI << "Window builder: placed slider '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return slider;
}

twidget* tbuilder_spacer::build() const
{
	tspacer* spacer = new tspacer();

	init_control(spacer);

	const game_logic::map_formula_callable& size = get_screen_size_variables();
	const unsigned width = width_(size);
	const unsigned height = height_(size);

	if(width || height) {
		spacer->set_best_size(tpoint(width, height));
	}

	DBG_GUI << "Window builder: placed spacer '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return spacer;
}

twidget* tbuilder_toggle_button::build() const
{
	ttoggle_button *toggle_button = new ttoggle_button();

	init_control(toggle_button);

	toggle_button->set_icon_name(icon_name_);
	toggle_button->set_retval(retval_);

	DBG_GUI << "Window builder: placed toggle button '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return toggle_button;
}

tbuilder_toggle_panel::tbuilder_toggle_panel(const config& cfg) :
	tbuilder_control(cfg),
	grid(0),
	retval_(lexical_cast_default<int>(cfg["return_value"]))
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_panel
 *
 * == Panel ==
 *
 * A panel is an item which can hold other items. The difference between a grid
 * and a panel is that it's possible to define how a panel looks. A grid in an
 * invisible container to just hold the items.
 *
 * @start_table = config
 *     grid (section)                  Defines the grid with the widgets to
 *                                     place on the panel.
 *     return_value (int = 0)          The return value, see
 *                                     [[GUIToolkitWML#Button]] for more info.
 * @end_table                                   
 *
 */
	VALIDATE(cfg.child("grid"), _("No grid defined."));

	grid = new tbuilder_grid(*(cfg.child("grid")));
}

twidget* tbuilder_toggle_panel::build() const
{
	ttoggle_panel* toggle_panel = new ttoggle_panel();

	init_control(toggle_panel);

	toggle_panel->set_retval(retval_);

	DBG_GUI << "Window builder: placed toggle panel '" << id << "' with defintion '" 
		<< definition << "'.\n";


	log_scope2(gui, "Window builder: building grid for toggle panel.");

	const unsigned rows = grid->rows;
	const unsigned cols = grid->cols;

	toggle_panel->set_rows_cols(rows, cols);

	for(unsigned x = 0; x < rows; ++x) {
		toggle_panel->set_row_grow_factor(x, grid->row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				toggle_panel->set_col_grow_factor(y, grid->col_grow_factor[y]);
			}

			twidget* widget = grid->widgets[x * cols + y]->build();
			toggle_panel->set_child(widget, x, y, grid->flags[x * cols + y],  grid->border_size[x * cols + y]);
		}
	}

	return toggle_panel;
}

twidget* tbuilder_text_box::build() const
{
	ttext_box* text_box = new ttext_box();

	init_control(text_box);

	// A textbox doesn't have a label but a text
	text_box->set_value(label);

	if (!history_.empty()) {
		text_box->set_history(history_);		
	}

	DBG_GUI << "Window builder: placed text box '" << id << "' with defintion '" 
		<< definition << "'.\n";

	return text_box;
}

twidget* tbuilder_vertical_scrollbar::build() const
{
	tvertical_scrollbar *vertical_scrollbar = new tvertical_scrollbar();

	init_control(vertical_scrollbar);

	DBG_GUI << "Window builder:"
		<< " placed vertical scrollbar '" << id
		<< "' with defintion '" << definition 
		<< "'.\n";

	return vertical_scrollbar;
}

twidget* tbuilder_grid::build() const
{
	return build(new tgrid());
}

twidget* tbuilder_grid::build (tgrid* grid) const
{
	grid->set_id(id);
	grid->set_rows_cols(rows, cols);

	log_scope2(gui, "Window builder: building grid");

	DBG_GUI << "Window builder: grid '" << id
		<< "' has " << rows << " rows and "
		<< cols << " columns.\n";

	for(unsigned x = 0; x < rows; ++x) {
		grid->set_row_grow_factor(x, row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				grid->set_col_grow_factor(y, col_grow_factor[y]);
			}

			DBG_GUI << "Window builder: adding child at " << x << ',' << y << ".\n";

			twidget* widget = widgets[x * cols + y]->build();
			grid->set_child(widget, x, y, flags[x * cols + y],  border_size[x * cols + y]);
		}
	}

	return grid;
}	

} // namespace gui2
/*WIKI
 * @page = GUIToolkitWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 * [[Category: Generated]]
 *
 */

