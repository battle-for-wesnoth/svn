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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/auxiliary/window_builder_private.hpp"

#include "asserts.hpp"
#include "foreach.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/window_builder/button.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/auxiliary/window_builder/horizontal_scrollbar.hpp"
#include "gui/auxiliary/window_builder/image.hpp"
#include "gui/auxiliary/window_builder/label.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/auxiliary/window_builder/minimap.hpp"
#include "gui/auxiliary/window_builder/menubar.hpp"
#include "gui/auxiliary/window_builder/slider.hpp"
#include "gui/auxiliary/window_builder/scroll_label.hpp"
#include "gui/auxiliary/window_builder/spacer.hpp"
#include "gui/auxiliary/window_builder/text_box.hpp"
#include "gui/auxiliary/window_builder/panel.hpp"
#include "gui/auxiliary/window_builder/password_box.hpp"
#include "gui/auxiliary/window_builder/toggle_panel.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/horizontal_scrollbar.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/generator.hpp"
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/multi_page.hpp"
#include "gui/widgets/password_box.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"
#include "gui/widgets/window.hpp"

// The widgets builders and helpers are partly in this namespace.
using namespace gui2::implementation;

namespace gui2 {

namespace {

tbuilder_widget_ptr create_builder_widget(const config& cfg)
{
	config::all_children_itors children = cfg.all_children_range();
	size_t nb_children = std::distance(children.first, children.second);
	if (nb_children != 1) {
		ERR_GUI_P << "Grid cell has " << nb_children
			<< " children instead of 1, aborting. Config :\n"
			<< cfg;
		assert(false);
	}

#define TRY(name) do { \
	if (const config &c = cfg.child(#name)) \
		return new tbuilder_##name(c); \
	} while (0)

	TRY(button);
	TRY(horizontal_scrollbar);
	TRY(image);
	TRY(label);
	TRY(listbox);
	TRY(menubar);
	TRY(minimap);
	TRY(multi_page);
	TRY(panel);
	TRY(scroll_label);
	TRY(scrollbar_panel);
	TRY(slider);
	TRY(spacer);
	TRY(text_box);
	TRY(password_box);
	TRY(toggle_button);
	TRY(toggle_panel);
	TRY(vertical_scrollbar);
	TRY(grid);

#undef TRY

	std::cerr << cfg;
	ERROR_LOG(false);
}

} // namespace

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 1
 *
 * THIS PAGE IS AUTOMATICALLY GENERATED, DO NOT MODIFY DIRECTLY !!!
 *
 * = Widget instance =
 *
 * Inside a grid (which is inside all container widgets) a widget is
 * instantiated. With this instantiation some more variables of a widget can
 * be tuned. This page will describe what can be tuned.
 *
 */
twindow* build(CVideo& video, const std::string& type)
{
	std::vector<twindow_builder::tresolution>::const_iterator
		definition = get_window_builder(type);

	// We set the values from the definition since we can only determine the
	// best size (if needed) after all widgets have been placed.
	twindow* window = new twindow(video
			, definition->x
			, definition->y
			, definition->width
			, definition->height
			, definition->automatic_placement
			, definition->horizontal_placement
			, definition->vertical_placement
			, definition->maximum_width
			, definition->maximum_height
			, definition->definition);
	assert(window);

	window->set_easy_close(definition->easy_close);

	window->init_grid(definition->grid);

	window->add_to_keyboard_chain(window);

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

	DBG_GUI_P << "Window builder: reading data for window " << id_ << ".\n";

	config::const_child_itors cfgs = cfg.child_range("resolution");
	VALIDATE(cfgs.first != cfgs.second, _("No resolution defined."));
	foreach (const config &i, cfgs) {
		resolutions.push_back(tresolution(i));
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
	maximum_width(lexical_cast_default<unsigned>(cfg["maximum_width"])),
	maximum_height(lexical_cast_default<unsigned>(cfg["maximum_height"])),
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
 *     maximum_width (unsigned = 0)  The maximum width of the window (only
 *                                   used for automatic placement).
 *     maximum_height (unsigned = 0) The maximum height of the window (only
 *                                   used for automatic placement).
 *
 *     easy_close (bool = false)     Does the window need easy close behaviour?
 *                                   Easy close behaviour means that any mouse
 *                                   click will close the dialog. Note certain
 *                                   widgets will automatically disable this
 *                                   behaviour since they need to process the
 *                                   clicks as well, for example buttons do need
 *                                   a click and a misclick on button shouldn't
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
 *     grid (grid)                   The grid with the widgets to show.
 * @end_table
 *
 * The size variables are copied to the window and will be determined runtime.
 * This is needed since the main window can be resized and the dialog needs to
 * resize accordingly. The following variables are available:
 * @start_table = formula
 *     screen_width unsigned         The usable width of the Wesnoth main window.
 *     screen_height unsigned        The usable height of the Wesnoth main window.
 *     gamemap_width unsigned        The usable width of the Wesnoth gamemap,
 *                                   if no gamemap shown it's the same value as
 *                                   screen_width.
 *     gamemap_height unsigned       The usable height of the Wesnoth gamemap,
 *                                   if no gamemap shown it's the same value as
 *                                   screen_height.
 * @end_table
 */

	const config &c = cfg.child("grid");

	VALIDATE(c, _("No grid defined."));

	grid = new tbuilder_grid(c);

	if(!automatic_placement) {
		VALIDATE(width.has_formula() || width(),
			missing_mandatory_wml_key("resolution", "width"));
		VALIDATE(height.has_formula() || height(),
			missing_mandatory_wml_key("resolution", "height"));
	}

	DBG_GUI_P << "Window builder: parsing resolution "
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
 *                                     vertical direction. This is used if the
 *                                     grid cell is wider as the best width for
 *                                     the widget.
 *     horizontal_grow (bool = false)  Does the widget grow in horizontal
 *                                     direction when the grid cell grows in the
 *                                     horizontal direction. This is used if the
 *                                     grid cell is higher as the best width for
 *                                     the widget.
 * @end_table
 *
 */
	log_scope2(log_gui_parse, "Window builder: parsing a grid");

	foreach (const config &row, cfg.child_range("row"))
	{
		unsigned col = 0;

		row_grow_factor.push_back(lexical_cast_default<unsigned>(row["grow_factor"]));

		foreach (const config &c, row.child_range("column"))
		{
			flags.push_back(read_flags(c));
			border_size.push_back(lexical_cast_default<unsigned>(c["border_size"]));
			if(rows == 0) {
				col_grow_factor.push_back(lexical_cast_default<unsigned>(c["grow_factor"]));
			}

			widgets.push_back(create_builder_widget(c));

			++col;
		}

		++rows;
		if (rows == 1) {
			cols = col;
		} else {
			VALIDATE(col, _("A row must have a column."));
			VALIDATE(col == cols, _("Number of columns differ."));
		}

	}

	DBG_GUI_P << "Window builder: grid has "
		<< rows << " rows and " << cols << " columns.\n";
}

tbuilder_gridcell::tbuilder_gridcell(const config& cfg) :
	tbuilder_widget(cfg),
	flags(read_flags(cfg)),
	border_size(lexical_cast_default<unsigned>((cfg)["border_size"])),
	widget(create_builder_widget(cfg))
{
}

tbuilder_multi_page::tbuilder_multi_page(const config& cfg) :
	implementation::tbuilder_control(cfg),
	builder(0),
	data()
{
/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_multi_page
 *
 * == Multi page ==
 *
 * Instance of a multi page.
 *
 * List with the multi page specific variables:
 * @start_table = config
 *     page_definition (section)       This defines how a listbox item
 *                                     looks. It must contain the grid
 *                                     definition for 1 row of the list.
 *
 *     page_data(section = [])         A grid alike section which stores the
 *                                     initial data for the listbox. Every row
 *                                     must have the same number of columns as
 *                                     the 'list_definition'.
 * @end_table
 */

	const config &page = cfg.child("page_definition");

	VALIDATE(page, _("No page defined."));
	builder = new tbuilder_grid(page);
	assert(builder);

	/** @todo This part is untested. */
	const config &d = cfg.child("page_data");
	if(!d){
		return;
	}

	foreach(const config &row, d.child_range("row"))
	{
		unsigned col = 0;

		foreach (const config &column, row.child_range("column"))
		{
			data.push_back(string_map());
			foreach (const config::attribute &i, column.attribute_range()) {
				data.back()[i.first] = i.second;
			}
			++col;
		}

		VALIDATE(col == builder->cols, _("'list_data' must have "
			"the same number of columns as the 'list_definition'."));
	}
}

twidget* tbuilder_multi_page::build() const
{
	tmulti_page *multi_page = new tmulti_page();

	init_control(multi_page);

	multi_page->set_page_builder(builder);

	DBG_GUI_G << "Window builder: placed multi_page '"
		<< id << "' with defintion '"
		<< definition << "'.\n";

	boost::intrusive_ptr<const tmulti_page_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const tmulti_page_definition::tresolution>(multi_page->config());
	assert(conf);

	multi_page->init_grid(conf->grid);

	multi_page->finalize(data);

	return multi_page;
}

tbuilder_scrollbar_panel::tbuilder_scrollbar_panel(const config& cfg)
	: implementation::tbuilder_control(cfg)
	, vertical_scrollbar_mode(
			get_scrollbar_mode(cfg["vertical_scrollbar_mode"]))
	, horizontal_scrollbar_mode(
			get_scrollbar_mode(cfg["horizontal_scrollbar_mode"]))
	, grid(0)
{
/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_scrollbar_panel
 *
 * == Scrollbar panel ==
 *
 * Instance of a scrollbar_panel.
 *
 * List with the scrollbar_panel specific variables:
 * @start_table = config
 *     vertical_scrollbar_mode (scrollbar_mode = auto | initial_auto)
 *                                     Determines whether or not to show the
 *                                     scrollbar. The default of initial_auto
 *                                     is used when --new-widgets is used.
 *                                     In the future the default will be
 *                                     auto.
 *     horizontal_scrollbar_mode (scrollbar_mode = auto | initial_auto)
 *                                     Determines whether or not to show the
 *                                     scrollbar. The default of initial_auto
 *                                     is used when --new-widgets is used.
 *                                     In the future the default will be
 *                                     initial_auto.
 *
 *     definition (section)            This defines how a scrollbar_panel item
 *                                     looks. It must contain the grid
 *                                     definition for 1 row of the list.
 *
 * @end_table
 *
 */

	const config &definition = cfg.child("definition");

	VALIDATE(definition, _("No list defined."));
	grid = new tbuilder_grid(definition);
	assert(grid);
}

twidget* tbuilder_scrollbar_panel::build() const
{
	tscrollbar_panel *scrollbar_panel = new tscrollbar_panel();

	init_control(scrollbar_panel);

	scrollbar_panel->set_vertical_scrollbar_mode(vertical_scrollbar_mode);
	scrollbar_panel->set_horizontal_scrollbar_mode(horizontal_scrollbar_mode);

	DBG_GUI_G << "Window builder: placed scrollbar_panel '" << id << "' with defintion '"
		<< definition << "'.\n";

	boost::intrusive_ptr<
			const tscrollbar_panel_definition::tresolution> conf =
				boost::dynamic_pointer_cast
					<const tscrollbar_panel_definition::tresolution>
						(scrollbar_panel->config());
	assert(conf);


	scrollbar_panel->init_grid(conf->grid);
	scrollbar_panel->finalize_setup();

	/*** Fill the content grid. ***/
	tgrid* content_grid = scrollbar_panel->content_grid();
	assert(content_grid);

	const unsigned rows = grid->rows;
	const unsigned cols = grid->cols;

	content_grid->set_rows_cols(rows, cols);

	for(unsigned x = 0; x < rows; ++x) {
		content_grid->set_row_grow_factor(x, grid->row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				content_grid->set_col_grow_factor(y
						, grid->col_grow_factor[y]);
			}

			twidget* widget = grid->widgets[x * cols + y]->build();
			content_grid->set_child(widget
					, x
					, y
					, grid->flags[x * cols + y]
					, grid->border_size[x * cols + y]);
		}
	}


	return scrollbar_panel;
}

twidget* tbuilder_toggle_button::build() const
{
	ttoggle_button *toggle_button = new ttoggle_button();

	init_control(toggle_button);

	toggle_button->set_icon_name(icon_name_);
	toggle_button->set_retval(get_retval(retval_id_, retval_, id));

	DBG_GUI_G << "Window builder: placed toggle button '"
			<< id << "' with defintion '" << definition << "'.\n";

	return toggle_button;
}

twidget* tbuilder_vertical_scrollbar::build() const
{
	tvertical_scrollbar *vertical_scrollbar = new tvertical_scrollbar();

	init_control(vertical_scrollbar);

	DBG_GUI_G << "Window builder:"
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

	log_scope2(log_gui_general, "Window builder: building grid");

	DBG_GUI_G << "Window builder: grid '" << id
		<< "' has " << rows << " rows and "
		<< cols << " columns.\n";

	for(unsigned x = 0; x < rows; ++x) {
		grid->set_row_grow_factor(x, row_grow_factor[x]);
		for(unsigned y = 0; y < cols; ++y) {

			if(x == 0) {
				grid->set_col_grow_factor(y, col_grow_factor[y]);
			}

			DBG_GUI_G << "Window builder: adding child at " << x << ',' << y << ".\n";

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

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 * [[Category: Generated]]
 *
 */

