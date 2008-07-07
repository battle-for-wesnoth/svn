/* $Id$ */
/*
   Copyright (C) 2008 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file action_base.hpp
//! Base class for editor actions

//! An action is constructed in response to a user command, then executed on 
//! the map. An undo action is returned by default via pointer, caller-owned).
//! It is possible to call an action without creatung the undo action.
//! Actions report failure via exceptions.
//! Code that only deals with actions polymorphically should only need to 
//! include this header file.

#ifndef EDITOR2_ACTION_BASE_HPP_INCLUDED
#define EDITOR2_ACTION_BASE_HPP_INCLUDED

#include "editor_common.hpp"

namespace editor2 {

//base class (interface) for editor actions.
class editor_action
{
    public:
        editor_action()
        {
        }
        virtual ~editor_action()
        {
        }        
        virtual editor_action* perform(editor_map&) const = 0;
        virtual void perform_without_undo(editor_map&) const = 0;
};


//TODO: add messages etc
struct editor_action_exception : public editor_exception
{
};

//thrown instead of a "todo" debug message
struct editor_action_not_implemented : public editor_action_exception
{
};

//used when e.g. passed parameters are invalid
struct editor_action_creation_fail : public editor_action_exception
{
};

} //end namespace editor2

#endif
