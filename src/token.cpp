/* $Id: tstring.cpp 48153 2011-01-01 15:57:50Z mordante $ */
/*
   Copyright (C) 2004 by Philippe Plantier <ayin@anathas.org>
   Copyright (C) 2005 - 2011 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  t_token provides a repository of single word references to unqiue constant strings
 *  allowing for fast comparison operators.
 */

#include "token.hpp"

namespace n_token {

/// Do not inline this enforces static initialization order
t_token const & t_token::z_empty() {
	static t_token *z_empty = new t_token("", false);
	return *z_empty;
}

}//end namepace


