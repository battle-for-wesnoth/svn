/* $Id$ */
/*
   Copyright (C) 2012 - 2013 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Define the common filters for the @ref gui2::tpane class.
 */

#ifndef GUI_AUXILIARY_FILTER_HPP_INCLUDED
#define GUI_AUXILIARY_FILTER_HPP_INCLUDED

#include "gui/widgets/text_box.hpp"
#include "map_utils.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"

namespace gui2 {

template <class T>
inline bool sort(
		  const tpane::titem& lhs
		, const tpane::titem& rhs
		, const std::string& tag
		, const bool ascending)
{
	if(ascending) {
		return
			  lexical_cast<T>(at(lhs.tags, tag))
			< lexical_cast<T>(at(rhs.tags, tag));
	} else {
		return
			  lexical_cast<T>(at(lhs.tags, tag))
			> lexical_cast<T>(at(rhs.tags, tag));
	}
}

/**
 * A filter testing whether a search string is used in a text.
 *
 * The comparison is a lower-case comparison.
 *
 * The function is expected to be used as a functor for
 * @ref gui2::tpane::filter().
 *
 * @param item                    The pane item to search in.
 * @param tag                     The tag of the field containing the text to
 *                                search in. @note This text field should
 *                                already contain lower-cased text only.
 * @param text_box                A text box object whose text is the search
 *                                string. The text in the text box will be
 *                                converted to lower-cased text before the
 *                                comparison is done.
 *
 * @returns                       Whether or not the comparison found a match.
 */
inline bool contains(
		  const tpane::titem& item
		, const std::string& tag
		, const ttext_box& text_box)
{
	return at(item.tags, tag)
			.find(utils::lowercase(text_box.text())) != std::string::npos;
}

} // namespace gui2

#endif
