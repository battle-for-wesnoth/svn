/* $Id$ */
/*
   Copyright (C) 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_AUXILIARY_TIPS_HPP_INCLUDED
#define GUI_AUXILIARY_TIPS_HPP_INCLUDED

#include "tstring.hpp"

#include <vector>

class config;

namespace gui2 {

class ttip;

namespace tips {

/**
 * Loads the tips from a config.
 *
 * @param cfg                     A config with the tips.
 *
 * @returns                       The loaded tips.
 */
std::vector<ttip> load(const config& cfg);

/**
 * Shuffles the tips.
 *
 * This routine shuffles the tips and filters out the unwanted ones.
 *
 * @param tips                    The tips.
 *
 * @returns                       The filtered tips in random order.
 */
std::vector<ttip> shuffle(const std::vector<ttip>& tips);

} // namespace tips {

/** The tips of day structure. */
class ttip
{
public:

	const t_string& text() const { return text_; }
	const t_string& source() const { return source_; }

private:
	friend std::vector<ttip> tips::load(const config&);
	ttip(const t_string& text, const t_string& source);

	t_string text_;
	t_string source_;
};

} // namespace gui2

#endif

