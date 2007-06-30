/* $Id: animated_game.cpp 18467 2007-06-27 18:18:16Z boucman $ */
/*
   Copyright (C) 2007 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
   */

#include "animated.i"
// Force compilation of the following template instantiations

#include "unit_frame.hpp"
#include "image.hpp"

template class animated< image::locator >;
template class animated< std::string >;
template class animated< unit_frame >;
