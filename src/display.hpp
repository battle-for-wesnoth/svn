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
#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

class config;
class gamestatus;
class team;
class unit;
class unit_map;

#include "builder.hpp"
#include "generic_event.hpp"
#include "image.hpp"
#include "key.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "pathfind.hpp"
#include "reports.hpp"
#include "theme.hpp"
#include "video.hpp"
#include "widgets/button.hpp"

#include "SDL.h"

#include <map>
#include <set>
#include <string>

//display: class which takes care of displaying the map and
//game-data on the screen.
//
//the display is divided into two main sections: the game area,
//which displays the tiles of the game board, and units on them,
//and the side bar, which appears on the right hand side. The side bar
//display is divided into three sections:
// - the minimap, which is displayed at the top right
// - the game status, which includes the day/night image, the turn number,
//   information about the current side, and information about the hex
//   currently moused over (highlighted)
// - the unit status, which displays an image for, and stats for, the
//   current unit.
class display
{
public:
	display(unit_map& units, CVideo& video,
			const gamemap& map, const gamestatus& status,
			const std::vector<team>& t, const config& theme_cfg,
			const config& cfg, const config& level);
	~display();
	static display* get_singleton() { return singleton_ ;}

	static Uint32 rgb(Uint8 red, Uint8 green, Uint8 blue)
		{ return 0xFF000000 | (red << 16) | (green << 8) | blue; }

	//new_turn should be called on every new turn, to update
	//lighting settings.
	void new_turn();

	//this will add r,g,b to the colours for all images displayed on
	//the map. Used for special effects like flashes.
	void adjust_colours(int r, int g, int b);

	//function which scrolls the display by xmov,ymov pixels. Invalidation and
	//redrawing will be scheduled.
	void scroll(int xmov, int ymov);

	// Zooms the display by the specified amount. Negative values zoom out.
	// Note the amount should be a multiple of four otherwise the images might
	// start to look odd. (hex_width() gets rounding errors)
	void set_zoom(int amount);

	// sets the zoom amount to the default.
	void set_default_zoom();

	// Returns the current zoom factor.
	double get_zoom_factor() { return double(zoom_)/double(image::tile_size); }

	//function to make a screenshot and save it in a default location
	void screenshot();

	//function which returns the size of a hex in pixels
	//(from top tip to bottom tip or left edge to right edge)
	int hex_size() const { return zoom_; }

	//function which returns the width of a pixel, up to where the next hex starts
	//(i.e. not entirely from tip to tip -- use hex_size() to get the distance from tip to tip)
	int hex_width() const { return (zoom_*3)/4; }

	enum SCROLL_TYPE { SCROLL, WARP, ONSCREEN };

	//function which will scroll such that location loc is on-screen.
	// WARP jumps to loc; SCROLL uses scroll speed;
	// ONSCREEN only scrolls if x,y is offscreen
	void scroll_to_tile(const gamemap::location& loc, SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true);

	//function which will scroll such that location loc is on-screen.
	//it will also try to make it such that loc is on-screen but this
	//is not guaranteed.
	void scroll_to_tiles(const gamemap::location& loc1, const gamemap::location& loc2,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true);
	//scrolls to the leader of a certain side. This will normally be the playing team.
	void scroll_to_leader(unit_map& units, int side);

	//invalidates entire screen, including all tiles and sidebar.
	void redraw_everything();

	void flip();

	//draws invalidated items. If update is true, will also copy the
	//display to the frame buffer. If force is true, will not skip frames,
	//even if running behind.
	void draw(bool update=true,bool force=false);

	//the dimensions of the display. x and y are width/height. mapx is the
	//width of the portion of the display which shows the game area. Between
	//mapx and x is the sidebar region.
	int x() const { return screen_.getx(); }
	int mapx() const { return x() - 140; }
	int y() const { return screen_.gety(); }

	const SDL_Rect& map_area() const 
		{ return theme_.main_map_location(screen_area()); }
	const SDL_Rect& minimap_area() const 
		{ return theme_.mini_map_location(screen_area()); }
	const SDL_Rect& unit_image_area() const 
		{ return theme_.unit_image_location(screen_area()); }

	SDL_Rect screen_area() const;

	//check if pixel x,y is outside specified area
	bool outside_area(const SDL_Rect& area, const int x, const int y) const;

	//function to display a location as selected. If a unit is in the location,
	//and there is no unit in the currently highlighted hex, the unit will be
	//displayed in the sidebar.
	void select_hex(gamemap::location hex);
	const gamemap::location& selected_hex() { return selectedHex_; }
	const gamemap::location& mouseover_hex() { return mouseoverHex_; }

	//function to highlight a location. If a unit is in the location, it will
	//be displayed in the sidebar. Selection is used when a unit has been
	//clicked on, while highlighting is used when a location has been moused
	//over
	void highlight_hex(gamemap::location hex);

	//given x,y co-ordinates of an onscreen pixel, will return the
	//location of the hex that this pixel corresponds to. Returns an
	//invalid location is the mouse isn't over any valid location.
	const gamemap::location hex_clicked_on(int x, int y, 
		gamemap::location::DIRECTION* nearest_hex=NULL, 
		gamemap::location::DIRECTION* second_nearest_hex=NULL) const;

	//given x,y co-ordinates of a pixel on the map, will return the
	//location of the hex that this pixel corresponds to. Returns an
	//invalid location if the mouse isn't over any valid location.
	const gamemap::location pixel_position_to_hex(int x, int y, 
		gamemap::location::DIRECTION* nearest_hex=NULL, 
		gamemap::location::DIRECTION* second_nearest_hex=NULL) const;

	//given x,y co-ordinates of the mouse, will return the location of the
	//hex in the minimap that the mouse is currently over, or an invalid
	//location if the mouse isn't over the minimap.
	gamemap::location minimap_location_on(int x, int y);

	//sets the paths that are currently displayed as available for the unit
	//to move along.  All other paths will be greyed out.
	void highlight_reach(const paths &paths_list);

	//add more paths to highlight.  Print numbers where they overlap.
	//Used only by Show Enemy Moves.
	void highlight_another_reach(const paths &paths_list);

	//reset highlighting of paths.
	void unhighlight_reach();

	//sets the route along which footsteps are drawn to show movement of a
	//unit. If NULL, no route is displayed.
	//route does not have to remain valid after being set
	void set_route(const paths::route* route);

	//functions to get the on-screen positions of hexes.
	int get_location_x(const gamemap::location& loc) const
		{ return map_area().x + loc.x*hex_width() - xpos_; }
	int get_location_y(const gamemap::location& loc) const
		{ return map_area().y + loc.y*zoom_ - ypos_ + (is_odd(loc.x) ? zoom_/2 : 0); }

	//returns the locations of 2 hexes that bind the visible area of the map.
	void get_visible_hex_bounds(gamemap::location &topleft, gamemap::location &bottomright) const;

	//function to remove a footstep from a specific location
	void remove_footstep(const gamemap::location& loc);

	//function to float a label above a tile
	void float_label(const gamemap::location& loc, const std::string& text,
	                 int red, int green, int blue);

	const gamemap& get_map()const { return map_;}

	//expose the event so that observers can be notified about map scrolling
	events::generic_event &scroll_event() const { return _scroll_event; }

	// The last action in drawing a tile is adding the overlays
	// these overlays are drawn in the following order
	// hex_overlay_ 			if the drawn location is in the map
	// selected_hex_overlay_	if the drawn location is selected
	// mouseover_hex_overlay_	if the drawn location is underneath the mouse
	//
	// These functions require a prerendered surface since they are
	// drawn at the top, they are not influenced by TOD, shroud etc
	void set_hex_overlay(const gamemap::location& loc, surface image) { hex_overlay_[loc] = image; }
	void clear_hex_overlay(const gamemap::location& loc);
	
	void set_selected_hex_overlay(const surface& image) { selected_hex_overlay_ = image; }
	void clear_selected_hex_overlay() { selected_hex_overlay_ = NULL; }

	void set_mouseover_hex_overlay(const surface& image) { mouseover_hex_overlay_ = image; }
	void clear_mouseover_hex_overlay() { mouseover_hex_overlay_ = NULL; }
	
private:
	enum ADJACENT_TERRAIN_TYPE { ADJACENT_BACKGROUND, ADJACENT_FOREGROUND, ADJACENT_FOGSHROUD };

	//composes and draws the terrains on a tile
	void draw_terrain_on_tile(const gamemap::location& loc, image::TYPE image_type, ADJACENT_TERRAIN_TYPE type);


	gui::button::TYPE string_to_button_type(std::string type);

	/**
	 * Used to draw the number of enemies which can reach a location.
	 *
	 * @param num 		number of enemies which can reach the location
	 * 					If num == 1 nothing is drawn
	 * @paream xloc		the on screen x position in pixels
	 * @param yloc		the on screen y position in pixels
	 */
	void draw_enemies_reach(const unsigned int num, const int xloc, const int yloc);

	// event raised when the map is being scrolled
	mutable events::generic_event _scroll_event;

public:
	//function to draw a footstep for the given location, on screen at
	//pixel co-ordinates (xloc,yloc). A footstep will only be drawn if
	//loc is on the current route set by set_route. Otherwise it will
	//return with no effect.
	void draw_footstep(const gamemap::location& loc, int xloc, int yloc);

	//draws the movement info (turns available) for a given location
	void draw_movement_info(const gamemap::location& loc, int xloc, int yloc);

	//gets the underlying screen object.
	CVideo& video() { return screen_; }

	//function to invalidate all tiles.
	void invalidate_all();

	//function to invalidate a specific tile for redrawing
	void invalidate(const gamemap::location& loc);

	//function to invalidate the game status displayed on the sidebar.
	void invalidate_game_status() { invalidateGameStatus_ = true; }
	const gamestatus &get_game_status() { return status_; }

	//function to invalidate that unit status displayed on the sidebar.
	void invalidate_unit() { invalidateUnit_ = true; }

	//function to invalidate animated terrains which may have changed.
	void invalidate_animations();

	//function to invalidate controls and panels when changed after
	//they have been drawn initially. Useful for dynamic theme modification.
	void invalidate_theme() { panelsDrawn_ = false; }

	//function to schedule the minimap for recalculation. Useful if any
	//terrain in the map has changed.
	void recalculate_minimap();

	//function to schedule the minimap to be redrawn. Useful if units
	//have moved about on the map
	void redraw_minimap() { redrawMinimap_ = true; }

	//temporarily place a unit on map (moving: can overlap others)
	void place_temporary_unit(unit &u, const gamemap::location& loc);
	void remove_temporary_unit();

	//functions to add and remove overlays from locations. An overlay is an
	//image that is displayed on top of the tile. One tile may have multiple
	//overlays. remove_overlay will remove all overlays on a tile.
	void add_overlay(const gamemap::location& loc, const std::string& image, const std::string& halo="");
	void remove_overlay(const gamemap::location& loc);

	//function to serialize overlay data
	void write_overlays(config& cfg) const;


	//function which copies the backbuffer to the framebuffer.
	void update_display();

	//functions used in the editor.
	//void draw_terrain_palette(int x, int y, terrain_type::TERRAIN selected);
	t_translation::t_letter get_terrain_on(int palx, int paly, int x, int y);

	//set_team sets the team controlled by the player using the computer,
	//and it is this team whose data is displayed in the game status.
	//set_playing_team sets the team whose turn it currently is
	void set_team(size_t team);
	void set_playing_team(size_t team);
	const std::vector<team>& get_teams() {return teams_;};

	unit_map& get_units() {return units_;};
	const unit_map& get_const_units() const {return units_;};

	//compat methods to be dropped after full migration
	void lock_updates(bool value) {screen_.lock_updates(value); };
	bool update_locked() const {return screen_.update_locked(); };

	//functions to set/get whether 'turbo' mode is on. When turbo mode is on,
	//everything moves much faster.
	void set_turbo(const bool turbo) { turbo_ = turbo; }

	double turbo_speed() const;
	void set_turbo_speed(const double speed);

	//Delay routines: use these not SDL_Delay (for --nogui).
	void delay(unsigned int milliseconds) const;

	//function which determines whether a grid should be overlayed on the
	//game board to more clearly show where hexes are.
	void set_grid(const bool grid) { grid_ = grid; }

	//a debug highlight draws a cross on a tile to emphasize something there.
	//it is used in debug mode, typically to show AI plans.
	static void debug_highlight(const gamemap::location& loc, fixed_t amount);
	static void clear_debug_highlights() { debugHighlights_.clear(); }

	//function which returns true if location (x,y) is covered in shroud.
	bool shrouded(const gamemap::location& loc) const;

	bool fogged(const gamemap::location& loc) const
		{ return team_valid() ? teams_[currentTeam_].fogged(loc.x, loc.y) : false; }

	//the viewing team is the team currently viewing the game. The playing team
	//is the team whose turn it is
	size_t viewing_team() const { return currentTeam_; }
	size_t playing_team() const { return activeTeam_; }
	bool team_valid() const { return currentTeam_ < teams_.size(); }
	const std::string current_team_name() const;
			
	theme& get_theme() { return theme_; }
	gui::button* find_button(const std::string& id);

	const theme::menu* menu_pressed();

	//finds the menu which has a given item in it, and enables or disables it.
	void enable_menu(const std::string& item, bool enable);

	void add_observer(const std::string& name) { observers_.insert(name); }
	void remove_observer(const std::string& name) { observers_.erase(name); }
	const std::set<std::string>& observers() const { return observers_; }

	map_labels& labels() { return map_labels_; }
	const map_labels& labels() const { return map_labels_; }

	void set_diagnostic(const std::string& msg);

	enum MESSAGE_TYPE { MESSAGE_PUBLIC, MESSAGE_PRIVATE };
	void add_chat_message(const std::string& speaker, int side, const std::string& msg, MESSAGE_TYPE type, bool bell);
	void clear_chat_messages() { prune_chat_messages(true); }

	//function to draw the image of a unit at a certain location
	//x,y: pixel location on screen to draw the unit
	//image: the image of the unit
	//reverse: if the unit should be flipped across the y axis
	//upside_down: if the unit should be flipped across the x axis
	//alpha: the merging to use with the background
	//blendto: if blendto is not 0, then the alpha parameter will be used
	//         to blend to this colour, instead of the background
	//submerged: the amount of the unit out of 1.0 that is submerged
	//           (presumably under water) and thus shouldn't be drawn
	void draw_unit(int x, int y, surface image,
		        bool upside_down=false,fixed_t alpha=ftofxp(1.0),
			Uint32 blendto=0, double blend_ratio=0,
			double submerged=0.0);

	//rebuild the dynamic terrain at the given location.
	void rebuild_terrain(const gamemap::location &loc) 
		{ builder_.rebuild_terrain(loc); }
	//rebuild all dynamic terrain.
	void rebuild_all() { builder_.rebuild_all(); }

	//Add a location to highlight. Note that this has nothing to do with
	//selecting hexes, it is pure highlighting. These hexes will be
	//highlighted slightly darker than the currently selected hex.
	void add_highlighted_loc(const gamemap::location &hex);

	void clear_highlighted_locs();

	void remove_highlighted_loc(const gamemap::location &hex);

	void begin_game();

	bool in_game() const { return in_game_; }
	void draw_bar(const std::string& image, int xpos, int ypos, size_t height, double filled, const SDL_Color& col, fixed_t alpha);

	void get_rect_hex_bounds(SDL_Rect rect, gamemap::location &topleft, gamemap::location &bottomright) const;

private:
	display(const display&);
	void operator=(const display&);

	void draw_tile(const gamemap::location &loc, const SDL_Rect &clip_rect);
	void draw_sidebar();
	void draw_minimap(int x, int y, int w, int h);
	void draw_game_status();

	void draw_image_for_report(surface& img, SDL_Rect& rect);
	void draw_report(reports::TYPE report_num);
	SDL_Rect reportRects_[reports::NUM_REPORTS];
	surface reportSurfaces_[reports::NUM_REPORTS];
	reports::report reports_[reports::NUM_REPORTS];

	void bounds_check_position();
	void bounds_check_position(int& xpos, int& ypos);

	std::vector<surface> get_terrain_images(const gamemap::location &loc, 
		image::TYPE type, ADJACENT_TERRAIN_TYPE terrain_type);

	std::vector<std::string> get_fog_shroud_graphics(const gamemap::location& loc);

	//this surface must be freed by the caller
	surface get_flag(const t_translation::t_letter& terrain, const gamemap::location& loc);

	//this surface must be freed by the caller
	surface get_minimap(int w, int h);

	CVideo& screen_;
	CKey keys_;
	int xpos_, ypos_, zoom_;
	const gamemap& map_;

	std::map<gamemap::location, surface> hex_overlay_;
	surface selected_hex_overlay_;
	surface mouseover_hex_overlay_;

	gamemap::location selectedHex_;
	gamemap::location mouseoverHex_;

	unit_map& units_;

	unit *temp_unit_;
	gamemap::location temp_unit_loc_;

	//function which finds the start and end rows on the energy bar image
	//where white pixels are substituted for the colour of the energy
	const SDL_Rect& calculate_energy_bar(surface surf);
	std::map<surface,SDL_Rect> energy_bar_rects_;

	surface minimap_;
	bool redrawMinimap_;

	paths::route route_;

	const gamestatus& status_;


	const std::vector<team>& teams_;

	// holds the tick count for when the next drawing event is scheduled
	// drawing shouldn't occur before this time
	int nextDraw_;

	void invalidate_route();

	std::set<gamemap::location> invalidated_;
	bool invalidateAll_;
	bool invalidateUnit_;
	bool invalidateGameStatus_;

	void invalidate_locations_in_rect(SDL_Rect r);

	struct overlay {
		overlay(const std::string& img, const std::string& halo_img,
		        int handle) : image(img), halo(halo_img),
				halo_handle(handle) {}
		std::string image;
		std::string halo;
		int halo_handle;
	};

	typedef std::multimap<gamemap::location,overlay> overlay_map;

	overlay_map overlays_;

	bool panelsDrawn_;

	size_t currentTeam_, activeTeam_;



	double turbo_speed_;
	bool turbo_, grid_;
	double sidebarScaling_;

	theme theme_;
	terrain_builder builder_;

	void create_buttons();
	std::vector<gui::button> buttons_;

	bool first_turn_, in_game_;

	std::set<std::string> observers_;

	map_labels map_labels_;

	struct chat_message
	{
		chat_message(int speaker, int h) : speaker_handle(speaker), handle(h), created_at(SDL_GetTicks())
		{}

		int speaker_handle;
		int handle;
		Uint32 created_at;
	};

	void prune_chat_messages(bool remove_all=false);

	std::vector<chat_message> chat_messages_;

	//if we're transitioning from one time of day to the next,
	//then we will use these two masks on top of all hexes when we blit
	surface tod_hex_mask1, tod_hex_mask2;

	//tiles lit for showing where unit(s) can reach
	typedef std::map<gamemap::location,unsigned int> reach_map;
	reach_map reach_map_;
	reach_map reach_map_old_;
	bool reach_map_changed_;
	void process_reachmap_changes();

	//for debug mode
	static std::map<gamemap::location,fixed_t> debugHighlights_;

	std::set<gamemap::location> highlighted_locations_;

	int diagnostic_label_;

	//animated flags for each team
	//
	std::vector<animated<image::locator> > flags_;

	//the handle for the label which displays fps
	int fps_handle_;
	static display * singleton_;
};

#endif
