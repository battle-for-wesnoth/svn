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

#ifndef ADDON_STATE_HPP_INCLUDED
#define ADDON_STATE_HPP_INCLUDED

#include "addon/info.hpp"

/** Defines various add-on installation statuses. */
enum ADDON_STATUS {
	/** Add-on is not installed. */
	ADDON_NONE,
	/** Version in the server matches local installation. */
	ADDON_INSTALLED,
	/** Version in the server is newer than local installation. */
	ADDON_INSTALLED_UPGRADABLE,
	/** Version in the server is older than local installation. */
	ADDON_INSTALLED_OUTDATED,
	/** Dependencies not satisfied.
	 *  @todo This option isn't currently implemented! */
	ADDON_INSTALLED_BROKEN,
	/** No tracking information available. */
	ADDON_NOT_TRACKED
};

/** Stores additional status information about add-ons. */
struct addon_tracking_info
{
	ADDON_STATUS state;
	bool can_publish;
	bool in_version_control;
	version_info installed_version;
};

/**
 * Get information about an add-on comparing its local state with the add-ons server entry.
 *
 * The add-on doesn't need to be locally installed; this is part of
 * the retrieved information.
 *
 * @param addon The add-ons server entry information.
 * @return      The local tracking status information.
 */
addon_tracking_info get_addon_tracking_info(const addon_info& addon);

std::string get_addon_status_gui1_color_markup(const addon_tracking_info& info);

#endif
