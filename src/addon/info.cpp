/* $Id$ */
/*
   Copyright (C) 2012 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "addon/info.hpp"

#include "foreach.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "image.hpp"
#include "log.hpp"
#include "serialization/string_utils.hpp"

static lg::log_domain log_addons_client("addons-client");
#define ERR_AC LOG_STREAM(err ,  log_addons_client)
#define LOG_AC LOG_STREAM(info,  log_addons_client)

namespace {
	const std::string fallback_addon_icon = "misc/blank-hex.png";

	void resolve_deps_recursive(const addons_list& addons, const std::string& base_id, std::set<std::string>& dest)
	{
		addons_list::const_iterator it = addons.find(base_id);
		if(it == addons.end()) {
			LOG_AC << "resolve_deps_recursive(): " << base_id << " not in add-ons list\n";
			return;
		}
		
		const std::vector<std::string>& base_deps = it->second.depends;

		if(base_deps.empty()) {
			return;
		}

		foreach(const std::string& dep, base_deps) {
			if(base_id == dep) {
				// TODO: make it possible to report this to the UI so it can be fixed by the add-on maintainer
				ERR_AC << dep << " depends upon itself; breaking circular dependency\n";
				continue;
			} else if(dest.find(dep) != dest.end()) {
				// TODO: make it possible to report this to the UI so it can be fixed by the add-on maintainer
				ERR_AC << dep << " already in dependency tree; breaking circular dependency\n";
				continue;
			}

			resolve_deps_recursive(addons, dep, dest);

			dest.insert(dep);
		}
	}
}

void addon_info::read(const config& cfg)
{
	this->id = cfg["name"].str();
	this->title = cfg["title"].str();
	this->description = cfg["description"].str();
	this->icon = cfg["icon"].str();
	this->version = cfg["version"].str();
	this->author = cfg["author"].str();
	this->size = cfg["size"];
	this->downloads = cfg["downloads"];
	this->uploads = cfg["uploads"];
	this->type = get_addon_type(cfg["type"].str());

	const config::const_child_itors& locales = cfg.child_range("translation");

	foreach(const config& locale, locales) {
		this->locales.push_back(locale["language"].str());
	}

	this->depends = utils::split(cfg["dependencies"].str());
}

std::string addon_info::display_icon() const
{
	std::string ret = icon;

	if(ret.empty()) {
		ERR_AC << "add-on '" << id << "' doesn't have an icon path set\n";
		ret = fallback_addon_icon;
	}
	else if(!image::exists(ret)) {
		ERR_AC << "add-on '" << id << "' has an icon which cannot be found: '" << ret << "'\n";
		ret = game_config::debug ? game_config::images::missing : fallback_addon_icon;
	}
	else if(ret.find("units/") != std::string::npos && ret.find_first_of('~') == std::string::npos) {
		// HACK: prevent magenta icons, because they look awful
		LOG_AC << "add-on '" << id << "' uses a unit baseframe as icon without TC/RC specifications\n";
		ret += "~RC(magenta>red)";
	}

	return ret;
}

std::set<std::string> addon_info::resolve_dependencies(const addons_list& addons) const
{
	std::set<std::string> deps;
	resolve_deps_recursive(addons, this->id, deps);

	if(deps.find(this->id) != deps.end()) {
		// TODO: make it possible to report this to the UI so it can be fixed by the add-on maintainer
		ERR_AC << this->id << " depends upon itself; breaking circular dependency\n";
		deps.erase(this->id);
	}

	return deps;
}

std::string size_display_string(double size)
{
	if(size > 0.0) {
		return utils::si_string(size, true, _("unit_byte^B"));
	} else {
		return "";
	}
}
