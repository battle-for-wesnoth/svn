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
#include "config.hpp"
#include "font.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "preferences.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
	CHARSET charset_used = CHARSET_LATIN1;
	std::string current_language;
}

CHARSET charset() { return charset_used; }

string_map string_table;

const std::string& translate_string(const std::string& str)
{
	return translate_string_default(str,str);
}

const std::string& translate_string_default(const std::string& str, const std::string& default_val)
{
	const string_map::const_iterator i = string_table.find(str);
	if(i != string_table.end() && i->second != "")
		return i->second;
	else
		return default_val;
}

std::vector<std::string> get_languages()
{
	std::vector<std::string> res;

	config cfg;

	try {
		cfg.read(preprocess_file("data/translations/",NULL,NULL));
	} catch(config::error& e) {
		std::cerr << "could not open translations: '" << e.message << "' -- defaulting to English only\n";
		res.push_back("English");
		return res;
	}

	const config::child_list& lang = cfg.get_children("language");
	for(config::child_list::const_iterator i = lang.begin(); i != lang.end(); ++i) {
		res.push_back((**i)["language"]);
	}

	return res;
}

namespace {
bool internal_set_language(const std::string& locale, config& cfg)
{
	const config::child_list& lang = cfg.get_children("language");
	for(config::child_list::const_iterator i = lang.begin(); i != lang.end(); ++i) {
		if((**i)["id"] == locale || (**i)["language"] == locale) {

			current_language = (**i)["language"];

			const std::string& enc = (**i)["encoding"];
			if(enc == "UTF-8") {
				charset_used = CHARSET_UTF8;
			} else if(enc == "LATIN1" || enc == "") {
				charset_used = CHARSET_LATIN1;
			} else {
				std::cerr << "Unrecognized character set: '" << enc
				          << "' (defaulting to LATIN1)\n";
				charset_used = CHARSET_LATIN1;
			}

			for(string_map::const_iterator j = (*i)->values.begin(); j != (*i)->values.end(); ++j) {
				string_table[j->first] = j->second;
			}

			hotkey::add_hotkeys(**i,false);

			font::set_font((**i)["font"]);

			return true;
		}
	}

	return false;
}
}

bool set_language(const std::string& locale)
{
	string_table.clear();

	std::string locale_lc;
	locale_lc.resize(locale.size());
	std::transform(locale.begin(),locale.end(),locale_lc.begin(),tolower);

	config cfg;
	if(locale_lc == "en" || locale_lc == "english") {
		try {
			cfg.read(preprocess_file("data/translations/english.cfg"));
		} catch(config::error& e) {
			std::cerr << "Could not read english.cfg\n";
			throw e;
		}
	} else {
		try {
			cfg.read(preprocess_file("data/translations/"));
			
			//default to English locale first, then set desired locale
			internal_set_language("en",cfg);
		} catch(config::error& e) {
			std::cerr << "error opening translations: '" << e.message << "' Defaulting to English\n";
			return set_language("english");
		}
	}

	return internal_set_language(locale,cfg);
}

const std::string& get_language() { return current_language; }

std::string get_locale()
{
	//TODO: Add in support for querying the locale on Windows

	const std::string& prefs_locale = preferences::locale();
	if(prefs_locale.empty() == false) {
		return prefs_locale;
	}

	const char* const locale = getenv("LANG");
	if(locale != NULL && strlen(locale) >= 2) {
		//we can't pass pointers into the string to the std::string
		//constructor because some STL implementations don't support
		//it (*cough* MSVC++6)
		std::string res(2,'z');
		res[0] = tolower(locale[0]);
		res[1] = tolower(locale[1]);
		return res;
	}

	std::cerr << "locale could not be determined; defaulting to locale 'en'\n";
	return "en";
}
