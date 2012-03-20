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

#include "addon/state.hpp"

#include "addon/manager.hpp"
#include "log.hpp"

static lg::log_domain log_addons_client("addons-client");
#define LOG_AC  LOG_STREAM(info, log_addons_client)

addon_tracking_info get_addon_tracking_info(const addon_info& addon)
{
	const std::string& id = addon.id;
	addon_tracking_info t;

	t.can_publish = have_addon_pbl_info(id);
	t.in_version_control = have_addon_in_vcs_tree(id);
	t.installed_version = version_info();

	if(is_addon_installed(id)) {
		try {
			t.installed_version = get_addon_version_info(id);
			const version_info& remote_version = addon.version;

			if(remote_version == t.installed_version) {
				t.state = ADDON_INSTALLED;
			} else if(remote_version > t.installed_version) {
				t.state = ADDON_INSTALLED_UPGRADABLE;
			} else /* if(remote_version < t.installed_version) */ {
				t.state = ADDON_INSTALLED_OUTDATED;
			}
		} catch(version_info::not_sane_exception const&) {
			LOG_AC << "local add-on " << id << " has invalid or missing version info, skipping from updates check...\n";
			t.state = ADDON_NOT_TRACKED;
		}
	} else {
		t.state = ADDON_NONE;
	}

	return t;
}

std::string get_addon_status_gui1_color_markup(ADDON_STATUS status)
{
	switch(status) {
	case ADDON_INSTALLED:
		return "@";
	case ADDON_INSTALLED_UPGRADABLE:
		return "<255,255,0>";
	case ADDON_INSTALLED_OUTDATED:
		return "<255,127,0>";
	case ADDON_INSTALLED_BROKEN:
		return "#";
	default:
		;
	}

	return "";
}
