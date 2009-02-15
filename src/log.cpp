/* $Id$ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
                 2004 - 2009 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file log.cpp
 * Standard logging facilities (implementation).
 * See also the command line switches --logdomains and --log-<level>="domain".
 */

#include "global.hpp"

#include "SDL.h"

#include "log.hpp"

#include <sstream>
#include <ctime>

namespace {

class null_streambuf : public std::streambuf
{
	virtual int overflow(int c) { return std::char_traits< char >::not_eof(c); }
public:
	null_streambuf() {}
};

} // end anonymous namespace

static std::ostream null_ostream(new null_streambuf);
static int indent = 0;
static bool timestamp = true;

namespace lg {

std::vector< lg::logd > log_domains;
void timestamps(bool t) { timestamp = t; }

logger err("error", 0), warn("warning", 1), info("info", 2), debug("debug", 3);
log_domain general("general"), ai("ai"), formula_ai("formula_ai"), cache("cache"), config("config"), display("display"),
	engine("engine"), network("network"), mp_server("server"),
	filesystem("filesystem"), audio("audio"),
	replay("replay"), help("help"), gui("gui"), gui_parse("gui_parse"),
	gui_draw("gui_draw"), gui_layout("gui_layout"), gui_event("gui_event"), editor("editor"), wml("wml"),
	mp_user_handler("user_handler");

log_domain::log_domain(char const *name) : domain_(log_domains.size())
{
	logd d = { name, 0 };
	log_domains.push_back(d);
}

bool set_log_domain_severity(std::string const &name, int severity)
{
	std::vector< logd >::iterator
		it = log_domains.begin(),
		it_end = log_domains.end();
	if (name == "all") {
		for(; it != it_end; ++it)
			it->severity_ = severity;
	} else {
		for(; it != it_end; ++it)
			if (name == it->name_) break;
		if (it == it_end)
			return false;
		it->severity_ = severity;
	}
	return true;
}

std::string list_logdomains()
{
	std::vector< logd >::iterator
		it_begin = log_domains.begin(),
		it_end = log_domains.end(),
		it;
	std::string domainlist = "";
	for(it = it_begin; it != it_end; ++it) {
		if (it != it_begin)
			domainlist += ", ";
		domainlist += it->name_;
	}
	return domainlist;
}

std::string get_timestamp(const time_t& t, const std::string& format) {
	char buf[100] = {0};
	tm* lt = localtime(&t);
	if (lt) {
		strftime(buf, 100, format.c_str(), lt);
	}
	return buf;
}

std::ostream &logger::operator()(log_domain const &domain, bool show_names, bool do_indent) const
{
	logd const &d = log_domains[domain.domain_];
	if (severity_ > d.severity_)
		return null_ostream;
	else {
		if(do_indent) {
			for(int i = 0; i != indent; ++i)
				std::cerr << "  ";
			}
		if (timestamp) {
			std::cerr << get_timestamp(time(NULL));
		}
		if (show_names) {
			std::cerr << name_ << ' ' << d.name_ << ": ";
		}
		return std::cerr;
	}
}

void scope_logger::do_log_entry(log_domain const &domain, const std::string& str)
{
	output_ = &debug(domain, false, true);
	str_ = str;
	ticks_ = SDL_GetTicks();
	(*output_) << "{ BEGIN: " << str_ << "\n";
	++indent;
}

void scope_logger::do_log_exit()
{
	const int ticks = SDL_GetTicks() - ticks_;
	--indent;
	do_indent();
	if (timestamp) (*output_) << get_timestamp(time(NULL));
	(*output_) << "} END: " << str_ << " (took " << ticks << "ms)\n";
}

void scope_logger::do_indent() const
{
	for(int i = 0; i != indent; ++i)
		(*output_) << "  ";
}

std::stringstream wml_error;

} // end namespace lg

