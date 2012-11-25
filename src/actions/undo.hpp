/* $Id$ */
/*
   Copyright (C) 2003 - 2012 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various functions that implement the undoing (and redoing) of in-game commands.
 */

#ifndef ACTIONS_UNDO_H_INCLUDED
#define ACTIONS_UNDO_H_INCLUDED

#include "../map_location.hpp"
#include "../unit.hpp"

#include <vector>


/** Records information to be able to undo a movement. */
struct undo_action {
	enum ACTION_TYPE { NONE, RECRUIT, RECALL, DISMISS };

	undo_action(const unit& u,
	            const std::vector<map_location>::const_iterator & begin,
	            const std::vector<map_location>::const_iterator & end,
	            int sm, int timebonus=0, int orig=-1,
	            const map_location::DIRECTION dir=map_location::NDIRECTIONS) :
			route(begin, end),
			starting_moves(sm),
			original_village_owner(orig),
			recall_loc(),
			recall_from(),
			type(NONE),
			affected_unit(u),
			countdown_time_bonus(timebonus),
			starting_dir(dir == map_location::NDIRECTIONS ? u.facing() : dir)
		{
		}

	undo_action(const unit& u, const map_location& loc, const map_location& from,
	            const ACTION_TYPE action_type=NONE) :
			route(),
			starting_moves(),
			original_village_owner(),
			recall_loc(loc),
			recall_from(from),
			type(action_type),
			affected_unit(u),
			countdown_time_bonus(1),
			starting_dir(u.facing())
		{}

	std::vector<map_location> route;
	int starting_moves;
	int original_village_owner;
	map_location recall_loc;
	map_location recall_from;
	ACTION_TYPE type;
	unit affected_unit;
	int countdown_time_bonus;
	map_location::DIRECTION starting_dir;

	bool is_dismiss() const { return type == DISMISS; }
	bool is_recall() const { return type == RECALL; }
	bool is_recruit() const { return type == RECRUIT; }
};


/// Class to store the actions that a player can undo and redo.
class undo_list {
public:
	undo_list() :
		undos_(), redos_()
	{}
	~undo_list() {}

	/// Clears the stack of undoable actions.
	void clear();

	bool empty() const                         { return undos_.empty(); }
	const undo_action & back() const           { return undos_.back(); }
	void push_back(const undo_action & action) { undos_.push_back(action); }
	void pop_back()                            { undos_.pop_back(); }

	void clear_redo()                               { redos_.clear(); }
	bool empty_redo() const                         { return redos_.empty(); }
	const undo_action & back_redo() const           { return redos_.back(); }
	void push_back_redo(const undo_action & action) { redos_.push_back(action); }
	void pop_back_redo()                            { redos_.pop_back(); }

private: // functions
	/// Copying the undo list is probably an error, so it is not implemented.
	undo_list(const undo_list &);
	/// Assigning the undo list is probably an error, so it is not implemented.
	undo_list & operator=(const undo_list &);

	/// Applies the pending fog/shroud changes from the undo stack.
	void apply_shroud_changes() const;

private: // data
	typedef std::vector<undo_action> action_list;

	action_list undos_;
	action_list redos_;
};

#endif
