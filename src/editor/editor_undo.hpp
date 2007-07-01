/* $Id$ */
/*
  Copyright (C) 2003 by David White <davidnwhite@verizon.net>
  Part of the Battle for Wesnoth Project http://www.wesnoth.org/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/

// This module is used to manage actions that may be undone in the map
// editor.

#ifndef EDITOR_UNDO_H_INCLUDED
#define EDITOR_UNDO_H_INCLUDED

#include "global.hpp"

#include "map.hpp"

#include <queue>
#include <vector>
#include <set>

namespace map_editor {

/// A saved action that may be undone.
class map_undo_action {
public:
	map_undo_action();

	const std::map<basemap::location, t_translation::t_letter>& undo_terrains() const;
	const std::map<basemap::location, t_translation::t_letter>& redo_terrains() const;

	const std::set<basemap::location> undo_selection() const;
	const std::set<basemap::location> redo_selection() const;

	std::string new_map_data() const;
	std::string old_map_data() const;

	const std::map<basemap::location, int>& undo_starting_locations() const;
	const std::map<basemap::location, int>& redo_starting_locations() const;

	void add_terrain(const t_translation::t_letter& old_tr,
					 const t_translation::t_letter& new_tr,
					 const basemap::location& lc);

	/// Return true if a terrain change has been saved in this undo
	/// action.
	bool terrain_set() const;

	void set_selection(const std::set<basemap::location> &old_selection,
					   const std::set<basemap::location> &new_selection);

	/// Return true if a selection change has been saved in this undo
	/// action.
	bool selection_set() const;

	void set_map_data(const std::string &old_data,
					  const std::string &new_data);

	/// Return true if a map data change has been saved in this undo
	/// action.
	bool map_data_set() const;

	void add_starting_location(const int old_side, const int new_side,
							   const basemap::location &old_loc,
							   const basemap::location &new_loc);

	/// Return true if starting locations have been saved in this undo
	/// action.
	bool starting_location_set() const;

private:
	std::map<basemap::location, t_translation::t_letter> old_terrain_;
	std::map<basemap::location, t_translation::t_letter> new_terrain_;
	bool terrain_set_;
	std::set<basemap::location> old_selection_;
	std::set<basemap::location> new_selection_;
	bool selection_set_;
	std::string old_map_data_;
	std::string new_map_data_;
	bool map_data_set_;
	std::map<basemap::location,int> old_starting_locations_;
	std::map<basemap::location,int> new_starting_locations_;
	bool starting_locations_set_;
};

typedef std::deque<map_undo_action> map_undo_list;

/// Add an undo action to the undo stack. Resize the stack if it gets
/// larger than the maximum size. Add an operation to the number done
/// since save. If keep_selection is true, it indicates that the
/// selection has not changed and the currently selected terrain should
/// be kept if this action is redone/undone. Also clear the redo stack.
void add_undo_action(const map_undo_action &action);

/// Return true if there exist any undo actions in the undo stack.
bool exist_undo_actions();
/// Return true if there exist any redo actions in the redo stack.
bool exist_redo_actions();


/// Remove, store in the redo stack and return the last undo action
/// stored.
map_undo_action pop_undo_action();

/// Remove, store in the undo stack and return the last redo action
/// stored.
map_undo_action pop_redo_action();

/// Clear all stored information about performed actions.
void clear_undo_actions();

}


#endif // EDITOR_UNDO_H_INCLUDED
