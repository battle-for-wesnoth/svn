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
#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED

#include <map>
#include <string>
#include <vector>

std::string read_file(const std::string& fname);
void write_file(const std::string& fname, const std::string& data);
std::string preprocess_file(const std::string& fname,
                            const std::map<std::string,std::string>* defines=0);

typedef std::map<std::string,std::string> string_map;

struct config
{
	config() {}
	config(const std::string& data); //throws config::error
	config(const config& cfg);
	~config();

	config& operator=(const config& cfg);
	
	void read(const std::string& data); //throws config::error
	std::string write() const;
	
	std::map<std::string,std::string> values;
	std::map<std::string,std::vector<config*> > children;

	static std::vector<std::string> split(const std::string& val);
	static bool has_value(const std::string& values, const std::string& val);
	
	void clear();

	struct error {
		error(const std::string& msg) : message(msg) {}
		std::string message;
	};
};

#endif
