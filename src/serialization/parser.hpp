/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef SERIALIZATION_PARSER_HPP_INCLUDED
#define SERIALIZATION_PARSER_HPP_INCLUDED

#include "global.hpp"

#include <iosfwd>
#include <vector>

class config;
class t_string;

//read data in, clobbering existing data.
void read(config &cfg, std::istream &in, std::string* error_log = NULL); //throws config::error

void write(std::ostream &out, config const &cfg, unsigned int level=0);
void write_key_val(std::ostream &out, const std::string &key, const t_string &value, unsigned int level, std::string &textdomain);
void write_open_child(std::ostream &out, const std::string &child, unsigned int level);
void write_close_child(std::ostream &out, const std::string &child, unsigned int level);

#endif
