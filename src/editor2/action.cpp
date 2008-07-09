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

//! @file action.cpp
//! Editor action classes

#include "action.hpp"
#include "editor_common.hpp"
#include "../foreach.hpp"

namespace editor2 {
	
editor_action_whole_map* editor_action_whole_map::perform(editor_map& m) const {
	editor_action_whole_map* undo = new editor_action_whole_map(m);
	perform_without_undo(m);
	return undo;
}
void editor_action_whole_map::perform_without_undo(editor_map& m) const {
	m = m_;
}

editor_action_chain::~editor_action_chain()
{
	foreach (editor_action* a, actions_) {
		delete a;
	}
}
editor_action_chain* editor_action_chain::perform(editor_map& m) const {
	std::vector<editor_action*> undo;
	foreach (editor_action* a, actions_) {
		undo.push_back(a->perform(m));
	}
	std::reverse(undo.begin(), undo.end());
	return new editor_action_chain(undo);
}
void editor_action_chain::perform_without_undo(editor_map& m) const
{
    foreach (editor_action* a, actions_) {
		a->perform_without_undo(m);
	}
}

editor_action_paste* editor_action_paste::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_paste::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_paint_hex* editor_action_paint_hex::perform(editor_map& map) const
{
	SCOPE_ED;
	LOG_ED << "Old terrain is " << map.get_terrain_string(loc_) << ", new will be " << map.get_terrain_string(t_) << "\n";
	editor_action_paint_hex* undo = new editor_action_paint_hex(loc_, map.get_terrain(loc_));
	perform_without_undo(map);
	LOG_ED << "Now the terrain is " << map.get_terrain_string(loc_) << "\n";
	return undo;
}
void editor_action_paint_hex::perform_without_undo(editor_map& map) const
{
	SCOPE_ED;
	map.set_terrain(loc_, t_);
}

editor_action_paste* editor_action_paint_brush::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_paint_brush::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_fill* editor_action_fill::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_fill::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_whole_map* editor_action_resize_map::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_resize_map::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_rotate_map* editor_action_rotate_map::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_rotate_map::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_mirror_map* editor_action_mirror_map::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_mirror_map::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

editor_action_paste* editor_action_plot_route::perform(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}
void editor_action_plot_route::perform_without_undo(editor_map& /*map*/) const
{
	throw editor_action_not_implemented();
}

} //end namespace editor2
