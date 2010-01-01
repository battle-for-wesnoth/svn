/* $Id$ */
/*
   Copyright (C) 2007 - 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/auxiliary/widget_definition.hpp"

#include "config.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/widgets/helper.hpp"

namespace gui2 {

tresolution_definition_::tresolution_definition_(const config& cfg) :
	window_width(lexical_cast_default<unsigned>(cfg["window_width"])),
	window_height(lexical_cast_default<unsigned>(cfg["window_height"])),
	min_width(lexical_cast_default<unsigned>(cfg["min_width"])),
	min_height(lexical_cast_default<unsigned>(cfg["min_height"])),
	default_width(lexical_cast_default<unsigned>(cfg["default_width"])),
	default_height(lexical_cast_default<unsigned>(cfg["default_height"])),
	max_width(lexical_cast_default<unsigned>(cfg["max_width"])),
	max_height(lexical_cast_default<unsigned>(cfg["max_height"])),
	text_extra_width(lexical_cast_default<unsigned>(cfg["text_extra_width"])),
	text_extra_height(lexical_cast_default<unsigned>(cfg["text_extra_height"])),
	text_font_size(lexical_cast_default<unsigned>(cfg["text_font_size"])),
	text_font_style(decode_font_style(cfg["text_font_style"])),
	state()
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1_widget
 *
 * == Resolution ==
 *
 * Depending on the resolution a widget can look different. Resolutions are
 * evaluated in order of appearance. The ''window_width'' and ''window_height''
 * are the upper limit this resolution is valid for. When one of the sizes
 * gets above the limit, the next resolution is selected. There's one special
 * case where both values are ''0''. This resolution always matches. (Resolution
 * definitions behind that one will never be picked.) This resolution can be
 * used as upper limit or if there's only one resolution.
 *
 * The default (and also minimum) size of a button is determined by two items,
 * the wanted default size and the size needed for the text. The size of the
 * text differs per used widget so needs to be determined per button.
 *
 * Container widgets like panels and windows have other rules for their sizes.
 * Their sizes are based on the size of their children (and the border they need
 * themselves). It's wise to set all sizes to 0 for these kind of widgets.
 *
 * @start_table = config
 *     window_width (unsigned = 0)   Width of the application window.
 *     window_height (unsigned = 0)
 *                                   Height of the application window.
 *     min_width (unsigned = 0)      The minimum width of the widget.
 *     min_height (unsigned = 0)     The minimum height of the widget.
 *
 *     default_width (unsigned = 0)  The default width of the widget.
 *     default_height (unsigned = 0) The default height of the widget.
 *
 *     max_width (unsigned = 0)      The maximum width of the widget.
 *     max_height (unsigned = 0)     The maximum height of the widget.
 *
 *     text_extra_width (unsigned = 0)
 *                                   The extra width needed to determine the
 *                                   minimal size for the text.
 *     text_extra_height (unsigned = 0)
 *                                   The extra height needed to determine the
 *                                   minimal size for the text.
 *     text_font_size (unsigned = 0) The font size, which needs to be used to
 *                                   determine the minimal size for the text.
 *     text_font_style (font_style = "")
 *                                   The font style, which needs to be used to
 *                                   determine the minimal size for the text.
 *
 *     state (section)               Every widget has one or more state
 *                                   sections.
 *                                   Note they aren't called state but state_xxx
 *                                   the exact names are listed per widget.
 * @end_table
 *
 */

	DBG_GUI_P << "Parsing resolution "
		<< window_width << ", " << window_height << '\n';
}

} // namespace gui2

