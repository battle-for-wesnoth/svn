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

#ifndef EDITOR2_EDITOR_MAP_HPP_INCLUDED
#define EDITOR2_EDITOR_MAP_HPP_INCLUDED

#include "editor_common.hpp"

#include "../map.hpp"

#include <deque>

namespace editor2 {

struct editor_map_operation_exception : public editor_exception
{
};

/**
 * This class adds extra editor-specific functionality to a normal gamemap.
 */	
class editor_map : public gamemap 
{
public:

	editor_map(const config& terrain_cfg, const std::string& data);
	
	editor_map(const config& terrain_cfg, size_t width, size_t height, t_translation::t_terrain filler);
	
	~editor_map();
	
	/**
	 * Get a contigious set of tiles having the same terrain as the starting location.
	 * Useful for flood fill or magic wand selection
	 * @return a contigious set of locations that will always contain at least the starting element
	 */
	std::set<gamemap::location> get_contigious_terrain_tiles(const gamemap::location& start) const;
	
	std::set<gamemap::location> set_starting_position_labels(display& disp);
	
	/**
	 * @return true when the location is part of the selection, false otherwise
	 */
	bool in_selection(const gamemap::location& loc) const;
	
	/**
	 * Add a location to the selection. The location should be valid (i.e. on the map)
	 * @return true if the selected hexes set was modified
	 */
	bool add_to_selection(const gamemap::location& loc);
	
	/**
	 * Remove a location to the selection. The location does not actually have to be selected
	 * @return true if the selected hexes set was modified
	 */
	bool remove_from_selection(const gamemap::location& loc);
	
	/**
	 * Return the selection set.
	 */
	const std::set<gamemap::location> selection() const { return selection_; }
	
	/**
	 * Clear the selection
	 */
	void clear_selection();
	
	/**
	 * Invert the selection, i.e. select all the map hexes that were not selected.
	 */
	void invert_selection();
	
	/**
	 * Select all map hexes
	 */
	void select_all();
	
	bool everything_selected() const;
		
	/** 
	 * Resize the map. If the filler is NONE, the border terrain will be copied
	 * when expanding, otherwise the fill er terrain will be inserted there
	 */
	void resize(int width, int height, int x_offset, int y_offset,
		t_translation::t_terrain filler = t_translation::NONE_TERRAIN);

	/** 
	 * Flip the map along the X axis. Seems somewhat broken, was so in the old editor.
	 */
	void flip_x();

	/** 
	 * Flip the map along the Y axis. Seems somewhat broken, was so in the old editor.
	 */
	void flip_y();

protected:
	void set_starting_position(int pos, const location& loc);
	void swap_starting_position(int x1, int y1, int x2, int y2);
	t_translation::t_list clone_column(int x, t_translation::t_terrain filler);
	void expand_right(int count, t_translation::t_terrain filler);
	void expand_left(int count, t_translation::t_terrain filler);
	void expand_top(int count, t_translation::t_terrain filler);
	void expand_bottom(int count, t_translation::t_terrain filler);
	void shrink_right(int count);
	void shrink_left(int count);
	void shrink_top(int count);
	void shrink_bottom(int count);

	/**
	 * The selected hexes
	 */
	std::set<gamemap::location> selection_;
};


} //end namespace editor2

#endif
