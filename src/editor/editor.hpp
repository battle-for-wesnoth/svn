/*
  Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
  Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/
#ifndef EDITOR_H_INCLUDED
#define EDITOR_H_INCLUDED

#include "../display.hpp"
#include "../events.hpp"
#include "../hotkeys.hpp"

#include <map>

namespace map_editor {

/// A saved action that may be undone.
struct map_undo_action {
	map_undo_action(const gamemap::TERRAIN& old_tr,
					const gamemap::TERRAIN& new_tr,
					const gamemap::location& lc)
		: old_terrain(old_tr), new_terrain(new_tr), location(lc) {}
	gamemap::TERRAIN old_terrain;
	gamemap::TERRAIN new_terrain;
	gamemap::location location;
};
typedef std::deque<map_undo_action> map_undo_list;

/// Size specifications for the map editor.
struct size_specs {
	size_t nterrains;
	size_t terrain_size;
	size_t terrain_padding;
	size_t terrain_space;
	size_t palette_x;
	size_t button_x;
	size_t top_button_y;
	size_t palette_y;
	size_t bot_button_y;
};

/// How to abort the map editor.
/// DONT_ABORT is set during normal operation.
/// When ABORT_NORMALLY is set, the editor asks for confirmation and
/// if save is desired before it exits. 
/// When ABORT_HARD is set, the editor exists without asking any
/// questions or saving.
enum ABORT_MODE {DONT_ABORT, ABORT_NORMALLY, ABORT_HARD};

/// A palette where the terrain to be drawn can be selected.
class terrain_palette {
public:
	terrain_palette(display &gui, const size_specs &sizes,
					const gamemap &map);

	/// Scroll the terrain palette up one step if possible.
	void scroll_up();

	/// Scroll the terrain palette down one step if possible.
	void scroll_down();

	/// Return the currently selected terrain.
	gamemap::TERRAIN selected_terrain() const;
	
	/// Select a terrain.
	void select_terrain(gamemap::TERRAIN);

	/// To be called when a mouse click occurs. Check if the coordinates
	/// is a terrain that may be chosen, select the terrain if that is
	/// the case.
	void left_mouse_click(const int mousex, const int mousey);

	// Draw the palette. If force is true everything will be redrawn
	// even though it is not invalidated.
	void draw(bool force=false);

	/// Return the number of terrains in the palette.
	size_t num_terrains() const;


private:
	/// Return the number of the tile that is at coordinates (x, y) in the
	/// panel.
	int tile_selected(const int x, const int y) const;
					  
	const size_specs &size_specs_;
	scoped_sdl_surface surf_;
	display &gui_;
	unsigned int tstart_;
	std::vector<gamemap::TERRAIN> terrains_;
	gamemap::TERRAIN selected_terrain_;
	const gamemap &map_;
	// Set invalid_ to true if an operation that requires that the
	// palette is redrawn takes place.
	bool invalid_;
};

/// A map editor. Receives SDL events and can execute hotkey commands.
class map_editor : public events::handler,
				   public hotkey::command_executor {
public:
	map_editor(display &gui, gamemap &map, config &theme, config &game_config);
	
	/// Enter the main loop. The loop runs until set_abort() is called
	/// to set an abort mode which makes the loop exit.
	void main_loop();
	
	/// Set the filename that map should be saved as. 
	void set_file_to_save_as(const std::string);
	
	/// Set the abort flag, which indicates if the editor should exit in
	/// some way after the current iteration of the main loop.
	void set_abort(const ABORT_MODE abort=ABORT_NORMALLY);

	/// Save the current map. If filename is an empty string, use the
	/// filename that is set with set_file_to_save_as(). A message box
	/// that shows confirmation that the map was saved is shown if
	/// display_confirmation is true. Return false if the save failed.
	bool save_map(const std::string filename="",
		  const bool display_confirmation=true);
	
	/// Adjust the internal size specifications to fit the display.
	void adjust_sizes(const display &disp);
	
	virtual void handle_event(const SDL_Event &event);
	
	/// Handle a keyboard event. mousex and mousey is the current
	/// position of the mouse.
	void handle_keyboard_event(const SDL_KeyboardEvent &event,
				   const int mousex, const int mousey);
	
	/// Handle a mouse button event. mousex and mousey is the current
	/// position of the mouse.
	void handle_mouse_button_event(const SDL_MouseButtonEvent &event,
				   const int mousex, const int mousey);
	
	
	
	// Methods inherited from command_executor. Used to perform
	// operations on menu/hotkey commands.
	virtual void toggle_grid();
	virtual void undo();
	virtual void redo();
	virtual void edit_quit();
	virtual void edit_new_map();
	virtual void edit_load_map();
	virtual void edit_save_map();
	virtual void edit_save_as();
	/// Display a dialog asking for a player number and set the starting
	/// position of the given player to the currently selected hex.
	virtual void edit_set_start_pos();
	
	virtual bool can_execute_command(hotkey::HOTKEY_COMMAND command) const;
	
	// exception thrown when new map is to be loaded.
	struct new_map_exception {
		new_map_exception(const std::string &new_map) : new_map_(new_map) {}
		const std::string new_map_;
	};
	
private:
	void map_editor::rebuild_dirty_terrains();

	/// Called in every iteration when the left mouse button is held
	/// down. Note that this differs from a click.
	void left_button_down(const int mousex, const int mousey);
	
	/// Called in every iteration when the right mouse button is held
	/// down. Note that this differs from a click.
	void right_button_down(const int mousex, const int mousey);
	
	/// Called in every iteration when the middle mouse button is held
	/// down. Note that this differs from a click.
	void middle_button_down(const int mousex, const int mousey);
	
	/// Confirm that exiting is desired and ask for saving of the map.
	/// Return true if exit is confirmed and the save is successful or not
	/// wanted. Return false if exit is cancelled or the requested save
	/// failed.
	bool confirm_exit_and_save();
	
	/// Set the starting position for the given player to the location
	/// given.
	void set_starting_position(const int player, const gamemap::location loc);
	
	/// Display a menu with given items and at the given location.
	void show_menu(const std::vector<std::string>& items_arg, const int xloc,
		   const int yloc, const bool context_menu=false);
	
	/// Pass the command onto the hotkey handling system. Quit requests
	/// are intercepted because the editor do not want the default
	/// behavior of those.
	void execute_command(const hotkey::HOTKEY_COMMAND command);
	
	/// Draw terrain at a location. The operation is saved in the undo
	/// stack. Update the map to reflect the change.
	void draw_terrain(const gamemap::TERRAIN terrain,
			  const gamemap::location hex);
	
	// Invalidate the given hex and all the adjacent ones.
	void invalidate_adjacent(const gamemap::location hex);

	/// Shows dialog to create new map.
	std::string new_map_dialog(display& disp);

	/// Return true iff the map is not modified or user agreed to
	/// dispose the modification.
	bool confirm_modification_disposal(display& disp, const std::string message);

	void draw_terrain_name(display &disp, const gamemap::TERRAIN);

	display &gui_;
	gamemap &map_;
	gui::button tup_, tdown_;
	map_undo_list undo_stack_;
	map_undo_list redo_stack_;
	std::string filename_;
	ABORT_MODE abort_;
	// Keep track of the number of operations performed since the last
	// save. If this is zero when the editor is exited there is no need
	// to ask the user to save.
	int num_operations_since_save_;
	size_specs size_specs_;
	config &theme_;
	config &game_config_;
	bool draw_terrain_;
	CKey key_;
	gamemap::location selected_hex_;
	// When minimap_dirty_ is true a redraw of the minimap will be
	// scheduled.
	bool minimap_dirty_;
	terrain_palette palette_;
	std::vector<gamemap::location> dirty_positions_;
};




/// Display a dialog with map filenames and return the chosen
/// one. Create a temporary display to use.
std::string get_filename_from_dialog(CVideo &video, config &cfg);
void drawbar(display& disp);
bool drawterrainpalette(display& disp, int start, gamemap::TERRAIN selected,
			gamemap map, size_specs specs);


bool is_invalid_terrain(char c);

}

#endif // EDITOR_H_INCLUDED
