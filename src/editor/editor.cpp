/*
  Copyright (C) 2003 by David Whire <davidnwhite@optusnet.com.au>
  Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/
#include "SDL.h"
#include "SDL_keysym.h"

#include "../actions.hpp"
#include "../ai.hpp"
#include "../config.hpp"
#include "../cursor.hpp"
#include "../dialogs.hpp"
#include "../filesystem.hpp"
#include "../font.hpp"
#include "../game_config.hpp"
#include "../gamestatus.hpp"
#include "../image.hpp"
#include "../key.hpp"
#include "../language.hpp"
#include "../widgets/menu.hpp"
#include "../pathfind.hpp"
#include "../playlevel.hpp"
#include "../preferences.hpp"
#include "../sdl_utils.hpp"
#include "../team.hpp"
#include "../unit_types.hpp"
#include "../unit.hpp"
#include "../util.hpp"
#include "../video.hpp"

#include "editor.hpp"

#include <cctype>
#include <iostream>
#include <map>
#include <string>

namespace {
  const unsigned int undo_limit = 100;
  // Milliseconds to sleep in every iteration of the main loop.
  const unsigned int sdl_delay = 20;
  const size_t default_terrain_size = 35;
}

namespace map_editor {

map_editor::map_editor(display &gui, gamemap &map, config &theme)
  : gui_(gui), map_(map), tup_(gui, "", gui::button::TYPE_PRESS, "uparrow-button"),
    tdown_(gui, "", gui::button::TYPE_PRESS, "downarrow-button"), abort_(DONT_ABORT),
    tstart_(0), num_operations_since_save_(0), theme_(theme), draw_terrain_(false) {

  terrains_ = map_.get_terrain_precedence();
  terrains_.erase(std::remove_if(terrains_.begin(), terrains_.end(), is_invalid_terrain),
		  terrains_.end());
  if(terrains_.empty()) {
    std::cerr << "No terrain found.\n";
    abort_ = ABORT_HARD;
  }
  selected_terrain_ = terrains_[1];

  // Set size specs.
  adjust_sizes(gui_);

  tup_.set_xy(gui.mapx() + size_specs_.button_x, size_specs_.top_button_y);
  tdown_.set_xy(gui.mapx() + size_specs_.button_x, size_specs_.bot_button_y);

  hotkey::add_hotkeys(theme_, true);
}

void map_editor::handle_event(const SDL_Event &event) {
  int mousex, mousey;
  SDL_GetMouseState(&mousex,&mousey);
  const SDL_KeyboardEvent keyboard_event = event.key;
  handle_keyboard_event(keyboard_event, mousex, mousey);

  const SDL_MouseButtonEvent mouse_button_event = event.button;
  handle_mouse_button_event(mouse_button_event, mousex, mousey);
}

void map_editor::handle_keyboard_event(const SDL_KeyboardEvent &event,
				       const int mousex, const int mousey) {
  if (event.type == SDL_KEYDOWN) {
    const SDLKey sym = event.keysym.sym;
    // We must intercept escape-presses here because we don't want the
    // default shutdown behavior, we want to ask for saving.
    if(sym == SDLK_ESCAPE) {
      set_abort();
    }
    else {
      hotkey::key_event(gui_, event, this);
    }
  }
}

void map_editor::handle_mouse_button_event(const SDL_MouseButtonEvent &event,
					   const int mousex, const int mousey) {
  if (event.type == SDL_MOUSEBUTTONDOWN) {
    const Uint8 button = event.button;
    if (button == SDL_BUTTON_WHEELUP) {
      scroll_palette_up();
    }
    if (button == SDL_BUTTON_WHEELDOWN) {
      scroll_palette_down();
    }
    if (button == SDL_BUTTON_RIGHT) {
      selected_hex_ = gui_.hex_clicked_on(mousex, mousey);
      const theme::menu* const m = gui_.get_theme().context_menu();
      show_menu(m->items(), mousex, mousey + 1, true);
    }
    if (button == SDL_BUTTON_LEFT) {
      draw_terrain_ = true;
      int tselect = tile_selected(mousex, mousey, gui_, size_specs_);
      if(tselect >= 0) {
	// Was the click on a terrain item in the palette? If so, set
	// the selected terrain.
	selected_terrain_ = terrains_[tstart_+tselect];
      }
    }
  }
  if (event.type == SDL_MOUSEBUTTONUP) {
    draw_terrain_ = false;
  }
}

void map_editor::edit_save_map() {
  save_map("", true);
}

void map_editor::edit_quit() {
  set_abort();
}

void map_editor::edit_save_as() {
  // TODO
  gui::show_dialog(gui_, NULL, "",
		   "Not implemented.", gui::OK_ONLY);
}

void map_editor::edit_set_start_pos() {
  std::string player = "1";
  const int res = gui::show_dialog(gui_, NULL, "", "Which player should start here?",
				   gui::OK_CANCEL, NULL, NULL, "", &player);
  if (player != "" && res == 0) {
    int int_player;
    bool invalid_number = player.size() > 1;
    std::stringstream str(player);
    str >> int_player;
    invalid_number = invalid_number ? true : int_player < 1 || int_player > 9;
    if (invalid_number) { 
      gui::show_dialog(gui_, NULL, "",
		       "You must enter a number between 1 and 9.", gui::OK_ONLY);
    }
    else {
      set_starting_position(int_player, selected_hex_);
    }
  }
}


bool map_editor::can_execute_command(hotkey::HOTKEY_COMMAND command) const {
  switch (command) {
  case hotkey::HOTKEY_UNDO:
  case hotkey::HOTKEY_REDO:
  case hotkey::HOTKEY_ZOOM_IN:
  case hotkey::HOTKEY_ZOOM_OUT:
  case hotkey::HOTKEY_ZOOM_DEFAULT:
  case hotkey::HOTKEY_FULLSCREEN:
  case hotkey::HOTKEY_TOGGLE_GRID:
  case hotkey::HOTKEY_EDIT_SAVE_MAP:
  case hotkey::HOTKEY_EDIT_SAVE_AS:
  case hotkey::HOTKEY_EDIT_QUIT:
  case hotkey::HOTKEY_EDIT_SET_START_POS:
    return true;
  default:
    return false;
  }
}

void map_editor::adjust_sizes(const display &disp) {
  const size_t button_height = 24;
  const size_t button_palette_padding = 8;
  size_specs_.terrain_size = default_terrain_size;
  size_specs_.terrain_padding = 2;
  size_specs_.terrain_space = size_specs_.terrain_size + size_specs_.terrain_padding;
  size_specs_.palette_x = 40;
  size_specs_.button_x = size_specs_.palette_x + size_specs_.terrain_space - 12;
  size_specs_.top_button_y = 190;
  size_specs_.palette_y = size_specs_.top_button_y + button_height +
    button_palette_padding;
  const size_t max_bot_button_y = disp.y() - 60 - button_height;
  size_t space_for_terrains = max_bot_button_y - button_palette_padding -
    size_specs_.palette_y;
  space_for_terrains = space_for_terrains / size_specs_.terrain_space % 2 == 0 ? 
    space_for_terrains : space_for_terrains - size_specs_.terrain_space;
  size_specs_.nterrains = minimum((space_for_terrains / size_specs_.terrain_space) * 2,
				  terrains_.size());
  size_specs_.bot_button_y = size_specs_.palette_y +
    (size_specs_.nterrains / 2) * size_specs_.terrain_space + button_palette_padding;
}

void map_editor::toggle_grid() {
  preferences::set_grid(!preferences::grid());
  gui_.set_grid(preferences::grid());
  gui_.invalidate_all();
}

void map_editor::undo() {
  if(!undo_stack_.empty()) {
    --num_operations_since_save_;
    map_undo_action action = undo_stack_.back();
    map_.set_terrain(action.location,action.old_terrain);
    undo_stack_.pop_back();
    redo_stack_.push_back(action);
    if(redo_stack_.size() > undo_limit)
      redo_stack_.pop_front();
    gamemap::location locs[7];
    locs[0] = action.location;
    get_adjacent_tiles(action.location,locs+1);
    for(int i = 0; i != 7; ++i) {
      gui_.draw_tile(locs[i].x,locs[i].y);
    }
  }
}

void map_editor::redo() {
  if(!redo_stack_.empty()) {
    ++num_operations_since_save_;
    map_undo_action action = redo_stack_.back();
    map_.set_terrain(action.location,action.new_terrain);
    redo_stack_.pop_back();
    undo_stack_.push_back(action);
    if(undo_stack_.size() > undo_limit)
      undo_stack_.pop_front();
    gamemap::location locs[7];
    locs[0] = action.location;
    get_adjacent_tiles(action.location,locs+1);
    for(int i = 0; i != 7; ++i) {
      gui_.draw_tile(locs[i].x,locs[i].y);
    }
  }
}

void map_editor::set_starting_position(const int player, const gamemap::location loc) {
  if(map_.on_board(loc)) {
    map_.set_terrain(loc, gamemap::CASTLE);
    // This operation is currently not undoable, so we need to make sure
    // that save is always asked for after it is performed.
    num_operations_since_save_ = undo_limit + 1;
    map_.set_starting_position(player, loc);
    gui_.invalidate_all();
  }
  else {
    gui::show_dialog(gui_, NULL, "",
		     "You must have a hex selected on the board.", gui::OK_ONLY);
  }
}

void map_editor::set_abort(const ABORT_MODE abort) {
  abort_ = abort;
}

void map_editor::set_file_to_save_as(const std::string filename) {
  filename_ = filename;
}

void map_editor::left_button_down(const int mousex, const int mousey) {
  const gamemap::location& loc = gui_.minimap_location_on(mousex,mousey);
  if (loc.valid()) {
    gui_.scroll_to_tile(loc.x,loc.y,display::WARP,false);
  }
  // If the left mouse button is down and we beforhand have registred
  // a mouse down event, draw terrain at the current location.
  else if (draw_terrain_) {
    const gamemap::location hex = gui_.hex_clicked_on(mousex, mousey);
    if(map_.on_board(hex)) {
      const gamemap::TERRAIN terrain = map_[hex.x][hex.y];
      if(selected_terrain_ != terrain) {
	if(key_[SDLK_RCTRL] || key_[SDLK_LCTRL]) {
	  selected_terrain_ = terrain;
	}
	else {
	  draw_terrain(selected_terrain_, hex);
	}
      }
    }
  }
}

void map_editor::draw_terrain(const gamemap::TERRAIN terrain,
			      const gamemap::location hex) {
  ++num_operations_since_save_;
  redo_stack_.clear();
  const gamemap::TERRAIN current_terrain = map_[hex.x][hex.y];
  undo_stack_.push_back(map_undo_action(current_terrain, terrain, hex));
  if(undo_stack_.size() > undo_limit)
    undo_stack_.pop_front();
  map_.set_terrain(hex, terrain);
  gamemap::location locs[7];
  locs[0] = hex;
  get_adjacent_tiles(hex,locs+1);
  for(int i = 0; i != 7; ++i) {
    gui_.draw_tile(locs[i].x,locs[i].y);
  }
  gui_.draw();
  gui_.recalculate_minimap();
}

void map_editor::right_button_down(const int mousex, const int mousey) {
}
void map_editor::middle_button_down(const int mousex, const int mousey) {
}

void map_editor::scroll_palette_down() {
  if(tstart_ + size_specs_.nterrains + 2 <= terrains_.size()) {
    tstart_ += 2;
  }
}

void map_editor::scroll_palette_up() {
  if(tstart_ >= 2) {
    tstart_ -= 2;
  }
}

bool map_editor::confirm_exit_and_save() {
  int exit_res = gui::show_dialog(gui_, NULL, "",
				 "Do you want to exit the map editor?", gui::YES_NO);
  if (exit_res != 0) {
    return false;
  }
  if (num_operations_since_save_ != 0) {
    const int save_res = gui::show_dialog(gui_, NULL, "",
					  "Do you want to save before exiting?",
					  gui::YES_NO);
    if(save_res == 0) {
      if (!save_map("", false)) {
	return false;
      }
    }
  }
  return true;
}

bool map_editor::save_map(const std::string fn, const bool display_confirmation) {
  std::string filename = fn;
  if (filename == "") {
    filename = filename_;
  }
  try {
    write_file(filename, map_.write());
    num_operations_since_save_ = 0;
    if (display_confirmation) {
      gui::show_dialog(gui_, NULL, "", "Map saved.", gui::OK_ONLY);
    }
  }
  catch (io_exception e) {
    std::string msg = "Could not save the map: "; 
    msg += e.what();
    gui::show_dialog(gui_, NULL, "", msg, gui::OK_ONLY);
    return false;
  }
  return true;
}

void map_editor::show_menu(const std::vector<std::string>& items_arg, const int xloc,
			   const int yloc, const bool context_menu) {
  std::vector<std::string> items = items_arg;
  // menu is what to display in the menu.
  std::vector<std::string> menu;
  if(items.size() == 1) {
    execute_command(hotkey::string_to_command(items.front()));
    return;
  }
  for(std::vector<std::string>::const_iterator i = items.begin();
      i != items.end(); ++i) {
    std::stringstream str;
    // Try to translate it to nicer format.
    str << translate_string("action_" + *i);
    // See if this menu item has an associated hotkey.
    const hotkey::HOTKEY_COMMAND cmd = hotkey::string_to_command(*i);
    const std::vector<hotkey::hotkey_item>& hotkeys = hotkey::get_hotkeys();
    std::vector<hotkey::hotkey_item>::const_iterator hk;
    for(hk = hotkeys.begin(); hk != hotkeys.end(); ++hk) {
      if(hk->action == cmd) {
	break;
      }
    }
    if(hk != hotkeys.end()) {
      // Hotkey was found for this item, add the hotkey description to
      // the menu item.
      str << "," << hotkey::get_hotkey_name(*hk);
    }
    menu.push_back(str.str());
  }
  static const std::string style = "menu2";
  const int res = gui::show_dialog(gui_, NULL, "", "", gui::MESSAGE, &menu, NULL, "",
				   NULL, NULL, NULL, xloc, yloc, &style);
  if(res < 0 || res >= items.size())
    return;
  const hotkey::HOTKEY_COMMAND cmd = hotkey::string_to_command(items[res]);
  execute_command(cmd);
}

void map_editor::execute_command(const hotkey::HOTKEY_COMMAND command) {
  if (command == hotkey::HOTKEY_QUIT_GAME) {
    set_abort();
  }
  else {
    hotkey::execute_command(gui_, command, this);
  }
}
  
void map_editor::main_loop() {
  const double scroll_speed = preferences::scroll_speed();

  while (abort_ == DONT_ABORT) {
    int mousex, mousey;
    const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);
    const bool l_button_down = mouse_flags & SDL_BUTTON_LMASK;
    const bool r_button_down = mouse_flags & SDL_BUTTON_RMASK;
    const bool m_button_down = mouse_flags & SDL_BUTTON_MMASK;

    const gamemap::location cur_hex = gui_.hex_clicked_on(mousex,mousey);
    gui_.highlight_hex(cur_hex);
    selected_hex_ = cur_hex;

    const theme::menu* const m = gui_.menu_pressed(mousex, mousey,
						   mouse_flags & SDL_BUTTON_LMASK);
    if (m != NULL) {
      const SDL_Rect& menu_loc = m->location(gui_.screen_area());
      const int x = menu_loc.x + 1;
      const int y = menu_loc.y + menu_loc.h + 1;
      show_menu(m->items(), x, y, false);
    }
    
    if(key_[SDLK_UP] || mousey == 0) {
      gui_.scroll(0.0,-scroll_speed);
    }
    if(key_[SDLK_DOWN] || mousey == gui_.y()-1) {
      gui_.scroll(0.0,scroll_speed);
    }
    if(key_[SDLK_LEFT] || mousex == 0) {
      gui_.scroll(-scroll_speed,0.0);
    }
    if(key_[SDLK_RIGHT] || mousex == gui_.x()-1) {
      gui_.scroll(scroll_speed,0.0);
    }
    
    if (l_button_down) {
      left_button_down(mousex, mousey);
    }
    if (r_button_down) {
      right_button_down(mousex, mousey);
    }
    if (m_button_down) {
      middle_button_down(mousex, mousey);
    }

    gui_.draw(false);
    if(drawterrainpalette(gui_, tstart_, selected_terrain_, map_, size_specs_) == false)
      scroll_palette_down();

    if(tup_.process(mousex,mousey,l_button_down)) {
      scroll_palette_up();
    }

    if(tdown_.process(mousex,mousey,l_button_down)) {
      scroll_palette_down();
    }

    gui_.update_display();
    SDL_Delay(sdl_delay);
    events::pump();
    if (abort_ == ABORT_NORMALLY) {
      if (!confirm_exit_and_save()) {
	set_abort(DONT_ABORT);
      }
    }
  }
}


std::string get_filename_from_dialog(CVideo &video, config &cfg) {
  std::string path = game_config::path;
  path += "/data/maps/";
  
  display::unit_map u_map;
  config dummy_cfg("");
  
  config dummy_theme;
  display disp(u_map, video, gamemap(dummy_cfg,"1"), gamestatus(dummy_cfg,0),
	       std::vector<team>(), dummy_theme, cfg);
  
  std::vector<std::string> files;
  get_files_in_dir(path,&files);
  
  files.push_back("New Map...");
  
  const int res = gui::show_dialog(disp, NULL, "",
				   "Choose map to edit:", gui::OK_CANCEL, &files);
  if(res < 0) {
    return "";
  }
  std::string filename;
  if(res == int(files.size()-1)) {
    filename = "new-map";
    gui::show_dialog(disp, NULL, "", "Create new map",
		     gui::OK_ONLY, NULL, NULL, "", &filename);
    if (filename == "") {
      // If no name was given, return the empty filename; don't add
      // the path.
      return filename;
    }
  } else {
    filename = files[res];
  }
  filename = path + filename;
  return filename;
}

void drawbar(display& disp)
{
  SDL_Surface* const screen = disp.video().getSurface();
  SDL_Rect dst = {disp.mapx(), 0, disp.x() - disp.mapx(), disp.y()};
  SDL_FillRect(screen,&dst,0);
  update_rect(dst);
}

bool drawterrainpalette(display& disp, int start, gamemap::TERRAIN selected, gamemap map,
			size_specs specs)
{
  size_t x = disp.mapx() + specs.palette_x;
  size_t y = specs.palette_y;

  unsigned int starting = start;
  unsigned int ending = starting+specs.nterrains;

  bool status = true;

  SDL_Rect invalid_rect;
  invalid_rect.x = x;
  invalid_rect.y = y;
  invalid_rect.w = 0;

  SDL_Surface* const screen = disp.video().getSurface();

  std::vector<gamemap::TERRAIN> terrains = map.get_terrain_precedence();
  terrains.erase(std::remove_if(terrains.begin(), terrains.end(), is_invalid_terrain),
		 terrains.end());
  if(ending > terrains.size()){
    ending = terrains.size();
    starting = ending - specs.nterrains;
    status = false;
  }

  for(unsigned int counter = starting; counter < ending; counter++){
    const gamemap::TERRAIN terrain = terrains[counter];
    const std::string filename = "terrain/" +
      map.get_terrain_info(terrain).default_image() + ".png";
    scoped_sdl_surface image(image::get_image(filename, image::UNSCALED));
					       
    if(image->w != specs.terrain_size || image->h != specs.terrain_size) {
      image.assign(scale_surface(image, specs.terrain_size, specs.terrain_size));
    }

    if(image == NULL) {
      std::cerr << "image for terrain '" << counter << "' not found\n";
      return status;
    }

    SDL_Rect dstrect;
    dstrect.x = x + (counter % 2 != 0 ?  0 : specs.terrain_space);
    dstrect.y = y;
    dstrect.w = image->w;
    dstrect.h = image->h;

    SDL_BlitSurface(image, NULL, screen, &dstrect);
    gui::draw_rectangle(dstrect.x, dstrect.y, image->w, image->h,
			terrain == selected ? 0xF000:0 , screen);
    
    if (counter % 2 != 0)
      y += specs.terrain_space;
  }
  invalid_rect.w = specs.terrain_space * 2;
  
  invalid_rect.h = y - invalid_rect.y;
  update_rect(invalid_rect);
  return status;
}

int tile_selected(const unsigned int x, const unsigned int y,
		  const display& disp, const size_specs specs)
{
  for(unsigned int i = 0; i != specs.nterrains; i++) {
    const unsigned int px = disp.mapx() + specs.palette_x +
      (i % 2 != 0 ? 0 : specs.terrain_space);
    const unsigned int py = specs.palette_y + (i / 2) * specs.terrain_space;
    const unsigned int pw = specs.terrain_space;
    const unsigned int ph = specs.terrain_space;

    if(x > px && x < px + pw && y > py && y < py + ph) {
      return i;
    }
  }
  return -1;
}

bool is_invalid_terrain(char c) {
  return c == ' ' || c == '~';
}

}

int main(int argc, char** argv)
{
  game_config::editor = true;

  if(argc > 2) {
    std::cout << "usage: " << argv[0] << " [map-name]" << std::endl;
    return 0;
  }

  CVideo video;

  const font::manager font_manager;
  const preferences::manager prefs_manager;
  const image::manager image_manager;
  std::pair<int, int> desired_resolution = preferences::resolution();
  int video_flags = preferences::fullscreen() ? FULL_SCREEN : 0;
  video.setMode(desired_resolution.first, desired_resolution.second,
		16, video_flags);

  preproc_map defines_map;
  defines_map["MEDIUM"] = preproc_define();
  defines_map["NORMAL"] = preproc_define();
  config cfg(preprocess_file("data/game.cfg",&defines_map));

  set_language("English");

  std::string filename;

  if(argc == 1) {
    filename = map_editor::get_filename_from_dialog(video, cfg);
    if (filename == "") {
      return 0;
    }
  } else if(argc == 2) {
    filename = argv[1];
  }
  
  std::string mapdata;
  try {
    mapdata = read_file(filename);
  }
  catch (io_exception) {
    std::cerr << "Could not read the map file, sorry." << std::endl;
    return 1;
  }
  if(mapdata.empty()) {
    for(int i = 0; i != 30; ++i) {
      mapdata = mapdata + "gggggggggggggggggggggggggggggggggggggg\n";
    }
  }
  try {
    gamemap map(cfg, mapdata);
    gamestatus status(cfg, 0);
    std::vector<team> teams;

    config* const theme_cfg = cfg.find_child("theme", "name", "editor");
    config dummy_theme;
    if (!theme_cfg) {
      std::cerr << "Editor theme could not be loaded." << std::endl;
      *theme_cfg = dummy_theme;
    }
    std::map<gamemap::location,unit> units;
    display gui(units, video, map, status, teams,
		*theme_cfg, cfg);
    gui.set_grid(preferences::grid());
    
    //Draw the nice background bar
    map_editor::drawbar(gui);

    events::event_context ec;
    map_editor::map_editor editor(gui, map, *theme_cfg);
    editor.set_file_to_save_as(filename);
    editor.main_loop();
  }
  catch (gamemap::incorrect_format_exception) {
    std::cerr << "The map is not in a correct format, sorry." << std::endl;
    return 1;
  }
  return 0;
}
