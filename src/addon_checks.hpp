/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
                 2008 by Ignacio R. Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef ADDON_CHECKS_HPP_INCLUDED
#define ADDON_CHECKS_HPP_INCLUDED

#include <vector>
#include <string>

class config;

/** Values used for the directory scans */
enum ADDON_GROUP {
	ADDON_SINGLEPLAYER,	/**< (userdir)/data/campaigns */
	ADDON_MULTIPLAYER,	/**< (userdir)/data/multiplayer */
	ADDON_ALL			/**< all of the above. */
};

/**
 * Values used for add-on classification; UI-only
 * at the moment, in the future it could be used for
 * directory allocation too, removing the need for
 * the ADDON_GROUP constants (TODO).
 */
enum ADDON_TYPE {
	ADDON_UNKNOWN,		/**< a.k.a. anything. */
	ADDON_SP_CAMPAIGN,	/**< Single-player campaign. */
	ADDON_SP_SCENARIO,	/**< Single-player scenario. */
	ADDON_MP_CAMPAIGN,	/**< Multiplayer campaign. */
	ADDON_MP_SCENARIO,	/**< Multiplayer scenario. */
	ADDON_MP_MAPS,		/**< Multiplayer plain (no WML) map pack. */
	ADDON_MP_ERA,		/**< Multiplayer era. */
	ADDON_MP_FACTION,	/**< Multiplayer faction. */
	// NOTE: following two still require proper engine support
	//ADDON_MOD,			// Modification of the game for SP and/or MP.
	//ADDON_GUI,			// GUI add-ons/themes.
	ADDON_MEDIA			/**< Miscellaneous content/media (unit packs, terrain packs, music packs, etc.). */
};

ADDON_TYPE get_addon_type(const std::string& str);

inline ADDON_GROUP is_addon_sp_or_mp(ADDON_TYPE t)
{
	return (t == ADDON_MP_CAMPAIGN ||
	        t == ADDON_MP_SCENARIO ||
	        t == ADDON_MP_ERA ||
	        t == ADDON_MP_MAPS ||
	        t == ADDON_MP_FACTION) ? ADDON_MULTIPLAYER : ADDON_SINGLEPLAYER;
}

/** Checks whether an add-on name is legal or not. */
bool addon_name_legal(const std::string& name);
/** Probes an add-on archive for illegal names. */
bool check_names_legal(const config& dir);
/** Return a vector of detected scripts. */
std::vector<config *> find_scripts(const config &cfg, std::string extension);

std::string encode_binary(const std::string& str);
std::string unencode_binary(const std::string& str);
bool needs_escaping(char c);

#endif /* !ADDON_CHECKS_HPP_INCLUDED */
