/* $Id$ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_ADDON_LIST_HPP_INCLUDED
#define GUI_DIALOGS_ADDON_LIST_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "gui/widgets/pane.hpp"

class config;

namespace gui2 {

/** Shows the list of addons on the server. */
class taddon_list
	: public tdialog
{
public:
	explicit taddon_list(const config& cfg)
		: cfg_(cfg)
	{
	}

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Config which contains the list with the campaigns. */
	const config& cfg_;
};

} // namespace gui2

#endif

