/* $Id$ */
/*
 Copyright (C) 2010 by Gabriel Morin <gabrielmorin (at) gmail (dot) com>
 Part of the Battle for Wesnoth Project http://www.wesnoth.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2
 or at your option any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY.

 See the COPYING file for more details.
 */

/**
 * @file manager.hpp
 */

#ifndef WB_MANAGER_HPP_
#define WB_MANAGER_HPP_

#include "map_location.hpp"

#include <deque>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

class unit;
class arrow;

namespace wb {

class action;
class move;

typedef boost::shared_ptr<action> action_ptr;
typedef std::deque<action_ptr> action_set;

/**
 * This class holds and manages all of the whiteboard's planned actions.
 */
class manager : private boost::noncopyable // Singleton -> Non-copyable
{
public:

	virtual ~manager();

	/**
	 * Get the singleton instance.
	 */
	static manager& instance();

	/**
	 * Determine whether the whiteboard is activated.
	 */
	bool active(){ return planning_mode_; }

	void set_active(bool active){ planning_mode_ = active; }

	const action_set& get_actions() const;

	/**
	 * Returns the index for the first (executed earlier) action within the actions set.
	 */
	size_t begin() {return 0; }

	/**
	 * Returns the index for the position *after* the last executed action within the actions set.
	 */
	size_t end() { return actions_.size(); }

	/**
	 * Inserts a move at the specified index. The begin() and end() functions might prove useful here.
	 */
	void insert_move(unit& subject, const map_location& target_hex, arrow& arrow, size_t index);

	/**
	 * Inserts a move to be executed last (i.e. at the back of the queue)
	 */
	void queue_move(unit& subject, const map_location& target_hex, arrow& arrow);

	/**
	 * Moves an action earlier in the execution order (i.e. at the front of the queue),
	 * by the specified increment.
	 * If the increment is too large, the action will be simply moved at the earliest position.
	 */
	void move_earlier(size_t index, size_t increment);

	/**
	 * Moves an action later in the execution order (i.e. at the back of the queue),
	 * by the specified increment.
	 * If the increment is too large, the action will be simply moved at the latest position.
	 */
	void move_later(size_t index, size_t increment);

	/**
	 * Deletes the action at the specified index. The begin() and end() functions might prove useful here.
	 * If the index doesn't exist, the function does nothing.
	 */
	void remove_action(size_t index);

private:
	/// Singleton -> private constructor
	manager();

	/**
	 * Utility function to move actions around the queue.
	 * Positive increment = move toward back of the queue and later execution.
	 * Negative increment = move toward front of the queue and earlier execution.
	 */
	void move_in_queue(size_t index, int increment);

	static manager* instance_;

	/**
	 * Tracks whether the whiteboard is active.
	 */
	bool planning_mode_;

	action_set actions_;
};

} // end namespace wb

#endif /* WB_MANAGER_HPP_ */
