/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef LANGUAGE_HPP_INCLUDED
#define LANGUAGE_HPP_INCLUDED

#include "config.hpp"

#include <map>
#include <string>
#include <vector>

//the type we use to represent Unicode strings.
typedef std::vector<wchar_t> wide_string;

//this module controls internationalization.

struct symbol_table
{
	const std::string& operator[](const std::string& key) const;
	const std::string& operator[](const char* key) const;
};

//table of strings which are displayed to the user. Maps ids -> text.
//this table should be consulted whenever something is to be
//displayed on screen.
extern symbol_table string_table;

//function which translates a string if the string is available in
//the string table, and otherwise simply returns the string itself
const std::string& translate_string(const std::string& str);

//version of translate_string which will look up key in the string table,
//and otherwise returns default_val
const std::string& translate_string_default(const std::string& key, const std::string& default_val);

//function which, given the main configuration object, will return
//a list of the translations of the game available.
std::vector<std::string> get_languages();

//function which, given the main configuration object, and a locale,
//will set string_table to be populated with data from that locale.
//locale may be either the full name of the language, like 'English',
//or the 2-letter version, like 'en'.
bool set_language(const std::string& locale);

//function which returns the name of the language currently used
const std::string& get_language();

//function which attempts to query and return the locale on the system
std::string get_locale();

//functions for converting Unicode wide-char strings to UTF-8 encoded
//strings, back and forth
std::vector<std::string> split_utf8_string(const std::string &src);
std::string wstring_to_string(const wide_string &);
wide_string string_to_wstring(const std::string &);


#endif
