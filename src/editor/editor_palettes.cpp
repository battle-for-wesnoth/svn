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
 * Manage the terrain-palette in the editor.
 * Note: this is a near-straight rip from the old editor.
*/

#define GETTEXT_DOMAIN "wesnoth-editor"

#include "editor_common.hpp"
#include "editor_palettes.hpp"

#include "../foreach.hpp"
#include "../gettext.hpp"
#include "../serialization/string_utils.hpp"
#include "../sound.hpp"
#include "../tooltips.hpp"
#include "../marked-up_text.hpp"

namespace {
static std::string selected_terrain;
}

namespace editor {

std::string get_selected_terrain()
{
	return selected_terrain;
}

static bool is_valid_terrain(t_translation::t_terrain c) {
	return !(c == t_translation::VOID_TERRAIN || c == t_translation::FOGGED);
}

terrain_group::terrain_group(const config& cfg):
	id(cfg["id"]), name(cfg["name"].t_str()),
	icon(cfg["icon"]), core(cfg["core"].to_bool())
{
}

terrain_palette::terrain_palette(editor_display &gui, const size_specs &sizes,
								 const config& cfg,
								 t_translation::t_terrain& fore,
								 t_translation::t_terrain& back)
	: gui::widget(gui.video())
	, size_specs_(sizes)
	, gui_(gui)
	, tstart_(0)
	, terrain_map_()
	, terrains_()
	, terrain_groups_()
	, non_core_terrains_()
	, nterrains_()
	, nmax_terrains_()
	, terrain_start_()
	, selected_fg_terrain_(fore)
	, selected_bg_terrain_(back)
{
	// Get the available terrains temporary in terrains_
	terrains_ = map().get_terrain_list();

	//move "invalid" terrains to the end
	std::stable_partition(terrains_.begin(), terrains_.end(), is_valid_terrain);

	// Get the available groups and add them to the structure
	std::set<std::string> group_names;
	foreach (const config &g, cfg.child_range("editor_group"))
	{
		if (group_names.find(g["id"]) == group_names.end()) {
			terrain_groups_.push_back(terrain_group(g));
			group_names.insert(terrain_groups_.back().id);
		}
	}
	std::map<std::string, terrain_group*> id_to_group;
	foreach (terrain_group& tg, terrain_groups_) {
		id_to_group.insert(std::make_pair(tg.id, &tg));
	}

	// add the groups for all terrains to the map
	foreach (const t_translation::t_terrain& t, terrains_) {
		const terrain_type& t_info = map().get_terrain_info(t);
		DBG_ED << "Palette: processing terrain " << t_info.name()
			<< "(editor name: '" << t_info.editor_name() << "') "
			<< "(" << t_info.number() << ")"
			<< ": " << t_info.editor_group() << "\n";

		// don't display terrains that were automatically created from base+overlay
		if (t_info.is_combined()) continue;
		// nor display terrains that have hide_in_editor=true
		if (t_info.hide_in_editor()) continue;

		// add the terrain to the requested groups
		const std::vector<std::string>& keys = utils::split(t_info.editor_group());
		bool core = false;
		foreach (const std::string& k, keys) {
			terrain_map_[k].push_back(t);
			nmax_terrains_ = std::max(nmax_terrains_, terrain_map_[k].size());
			std::map<std::string, terrain_group*>::iterator i = id_to_group.find(k);
			if (i != id_to_group.end()) {
				if (i->second->core) {
					core = true;
				}
			}
		}
		// A terrain is considered core iff it appears in at least
		// one core terrain group
		if (core) {
			// Add the terrain to the default group
			terrain_map_["all"].push_back(t);
			nmax_terrains_ = std::max(nmax_terrains_, terrain_map_["all"].size());
		} else {
			non_core_terrains_.insert(t);
		}

	}
	typedef std::pair<std::string, t_translation::t_list> map_pair;

	// Set the default group
	set_group("all");

	if(terrains_.empty()) {
		ERR_ED << "No terrain found.\n";
	}
	update_report();
	//adjust_size();
}

void terrain_palette::set_group(const std::string& id)
{
	terrains_ = terrain_map_[id];
	active_group_ = id;
	if(terrains_.empty()) {
		ERR_ED << "No terrain found.\n";
	}
	scroll_top();
}

void terrain_palette::set_group(size_t index)
{
	set_group(terrain_groups_[index].id);
}

size_t terrain_palette::active_group_index()
{
	for (size_t i = 0 ; i < terrain_groups_.size(); i++) {
		if (terrain_groups_[i].id == active_group_)
			return i;
	}
	return 0;
}

const config terrain_palette::active_terrain_report()
{
	config cfg;
	config& report = cfg.add_child("element");
	for (size_t i = 0 ; i < terrain_groups_.size(); i++) {
		if (terrain_groups_[i].id == active_group_) {
			std::string groupname = terrain_groups_[i].name;
			report["image"] = "buttons/" + terrain_groups_[i].icon + ".png";
			report["tooltip"] = groupname;
		}
	}
	return cfg;
}

void terrain_palette::adjust_size() {
	scroll_top();

	SDL_Rect rect = create_rect(size_specs_.palette_x
			, size_specs_.palette_y
			, size_specs_.palette_w
			, size_specs_.palette_h);

	set_location(rect);
	terrain_start_ = size_specs_.palette_y;
	const size_t space_for_terrains = size_specs_.palette_h;
	rect.y = terrain_start_;
	rect.h = space_for_terrains;
	bg_register(rect);
	const unsigned terrains_fitting =
		static_cast<unsigned> (space_for_terrains / size_specs_.terrain_space) *
		size_specs_.terrain_width;
	nterrains_ = std::min<int>(terrains_fitting, nmax_terrains_);

	set_dirty();
}

void terrain_palette::scroll_right() {

	unsigned int rows = (size_specs_.palette_h / size_specs_.terrain_space);

	if(tstart_ + nterrains_ + rows <= num_terrains()) {
		tstart_ += rows;
		bg_restore();
		set_dirty();
	}
	else if (tstart_ + nterrains_ + (num_terrains() % rows) <= num_terrains()) {
		tstart_ += num_terrains() % rows;
		bg_restore();
		set_dirty();
	}
}

void terrain_palette::scroll_left() {
	unsigned int rows = (size_specs_.palette_h / size_specs_.terrain_space);
	unsigned int decrement = rows;
	if (tstart_ + nterrains_ == num_terrains() && num_terrains() % rows != 0) {
		decrement = num_terrains() % rows;
	}
	if(tstart_ >= decrement) {
		bg_restore();
		set_dirty();
		tstart_ -= decrement;
	}
}

void terrain_palette::scroll_top() {
	tstart_ = 0;
	bg_restore();
	set_dirty();
}

void terrain_palette::scroll_bottom() {
	unsigned int old_start = num_terrains();
	while (old_start != tstart_) {
		old_start = tstart_;
		scroll_right();
	}
}

t_translation::t_terrain terrain_palette::selected_fg_terrain() const
{
	return selected_fg_terrain_;
}

t_translation::t_terrain terrain_palette::selected_bg_terrain() const
{
	return selected_bg_terrain_;
}

void terrain_palette::select_fg_terrain(t_translation::t_terrain terrain)
{
	if (selected_fg_terrain_ != terrain) {
		set_dirty();
		selected_fg_terrain_ = terrain;
		update_report();
	}
}

void terrain_palette::select_bg_terrain(t_translation::t_terrain terrain)
{
	if (selected_bg_terrain_ != terrain) {
		set_dirty();
		selected_bg_terrain_ = terrain;
		update_report();
	}
}

void terrain_palette::swap()
{
	std::swap(selected_fg_terrain_, selected_bg_terrain_);
	set_dirty();
	update_report();
}

std::string terrain_palette::get_terrain_string(const t_translation::t_terrain t)
{
	return map().get_terrain_editor_string(t);
}

void terrain_palette::left_mouse_click(const int mousex, const int mousey) {
	int tselect = tile_selected(mousex, mousey);
	if(tselect >= 0 && (tstart_+tselect) < terrains_.size()) {
		select_fg_terrain(terrains_[tstart_+tselect]);
		gui_.invalidate_game_status();
	}
}

void terrain_palette::right_mouse_click(const int mousex, const int mousey) {
	int tselect = tile_selected(mousex, mousey);
	if(tselect >= 0 && (tstart_+tselect) < terrains_.size()) {
		select_bg_terrain(terrains_[tstart_+tselect]);
		gui_.invalidate_game_status();
	}
}

size_t terrain_palette::num_terrains() const {
	return terrains_.size();
}

void terrain_palette::draw() {
	draw(false);
}

void terrain_palette::handle_event(const SDL_Event& event) {
	if (event.type == SDL_MOUSEMOTION) {
		// If the mouse is inside the palette, give it focus.
		if (point_in_rect(event.button.x, event.button.y, location())) {
			if (!focus(&event)) {
				set_focus(true);
			}
		}
		// If the mouse is outside, remove focus.
		else {
			if (focus(&event)) {
				set_focus(false);
			}
		}
	}
	if (!focus(&event)) {
		return;
	}
	int mousex, mousey;
	SDL_GetMouseState(&mousex,&mousey);
	const SDL_MouseButtonEvent mouse_button_event = event.button;
	if (mouse_button_event.type == SDL_MOUSEBUTTONDOWN) {
		if (mouse_button_event.button == SDL_BUTTON_LEFT) {
			left_mouse_click(mousex, mousey);
		}
		if (mouse_button_event.button == SDL_BUTTON_RIGHT) {
			right_mouse_click(mousex, mousey);
		}
		if (mouse_button_event.button == SDL_BUTTON_WHEELUP) {
			scroll_left();
		}
		if (mouse_button_event.button == SDL_BUTTON_WHEELDOWN) {
			scroll_right();
		}
		if (mouse_button_event.button == SDL_BUTTON_WHEELLEFT) {
			set_group( (active_group_index() -1) % (terrain_groups_.size() -1));
			gui_.set_terrain_report(active_terrain_report());
		//	gui_.redraw_everything();
		}
		if (mouse_button_event.button == SDL_BUTTON_WHEELRIGHT) {
			set_group( (active_group_index() +1) % (terrain_groups_.size() -1));
			gui_.set_terrain_report(active_terrain_report());
		//	gui_.redraw_everything();
		}
	}
	if (mouse_button_event.type == SDL_MOUSEBUTTONUP) {
		if (mouse_button_event.button == SDL_BUTTON_LEFT) {
		}
	}
}

void terrain_palette::draw(bool force) {
	if (!dirty() && !force) {
		return;
	}
	unsigned int starting = tstart_;
	unsigned int ending = starting + nterrains_;
	surface screen = gui_.video().getSurface();
	if(ending > num_terrains()){
		ending = num_terrains();
	}
	const SDL_Rect &loc = location();
	int x = loc.x;
	SDL_Rect palrect;
	palrect.x = loc.x;
	palrect.y = terrain_start_;
	palrect.w = size_specs_.palette_w;
	palrect.h = size_specs_.palette_h;
	tooltips::clear_tooltips(palrect);
	for(unsigned int counter = starting; counter < ending; counter++){
		const t_translation::t_terrain terrain = terrains_[counter];
		const t_translation::t_terrain base_terrain = map().get_terrain_info(terrain).default_base();

		const int counter_from_zero = counter - starting;
		SDL_Rect dstrect;
		dstrect.x = x;
//		dstrect.y = terrain_start_ + (counter_from_zero % size_specs_.terrain_width) * size_specs_.terrain_space;
	//	(counter_from_zero % size_specs_.terrain_width == size_specs_.terrain_width - 1)
		dstrect.y = terrain_start_ + (counter_from_zero % (palrect.h /size_specs_.terrain_space) ) * size_specs_.terrain_space;
		dstrect.w = size_specs_.terrain_size;
		dstrect.h = size_specs_.terrain_size;

		// Reset the tile background
		bg_restore(dstrect);

		std::stringstream tooltip_text;

		//Draw default base for overlay terrains
		if(base_terrain != t_translation::NONE_TERRAIN) {
			const std::string base_filename = "terrain/" + map().get_terrain_info(base_terrain).editor_image() + ".png";
			surface base_image(image::get_image(base_filename));

			if(base_image == NULL) {
				tooltip_text << "BASE IMAGE NOT FOUND\n";
				ERR_ED << "image for terrain " << counter << ": '" << base_filename << "' not found\n";
				base_image = image::get_image(game_config::images::missing);
				if (base_image == NULL) {
					ERR_ED << "Placeholder image not found\n";
					return;
				}
			}

			if(static_cast<unsigned>(base_image->w) != size_specs_.terrain_size ||
			   static_cast<unsigned>(base_image->h) != size_specs_.terrain_size) {

				base_image.assign(scale_surface(base_image,
				   size_specs_.terrain_size, size_specs_.terrain_size));
			}

			sdl_blit(base_image, NULL, screen, &dstrect);
		}

		const std::string filename = "terrain/" + map().get_terrain_info(terrain).editor_image() + ".png";
		surface image(image::get_image(filename));
		if(image == NULL) {
			tooltip_text << "IMAGE NOT FOUND\n";
			ERR_ED << "image for terrain " << counter << ": '" << filename << "' not found\n";
			image = image::get_image(game_config::images::missing);
			if (image == NULL) {
				ERR_ED << "Placeholder image not found\n";
				return;
			}
		}

		if(static_cast<unsigned>(image->w) != size_specs_.terrain_size ||
			static_cast<unsigned>(image->h) != size_specs_.terrain_size) {

			image.assign(scale_surface(image,
				size_specs_.terrain_size, size_specs_.terrain_size));
		}

		sdl_blit(image, NULL, screen, &dstrect);

		surface screen = gui_.video().getSurface();
		Uint32 color;
		if (terrain == selected_bg_terrain() && terrain == selected_fg_terrain()) {
			color = SDL_MapRGB(screen->format,0xFF,0x00,0xFF);
		}
		else if (terrain == selected_bg_terrain()) {
			color = SDL_MapRGB(screen->format,0x00,0x00,0xFF);
		}
		else if (terrain == selected_fg_terrain()) {
			color = SDL_MapRGB(screen->format,0xFF,0x00,0x00);
		}
		else {
			color = SDL_MapRGB(screen->format,0x00,0x00,0x00);
		}

		draw_rectangle(dstrect.x, dstrect.y, image->w, image->h, color, screen);
		/* TODO The call above overdraws the border of the terrain image.
		   The following call is doing better but causing other drawing glitches.
		   draw_rectangle(dstrect.x -1, dstrect.y -1, image->w +2, image->h +2, color, screen);
		*/

		bool is_core = non_core_terrains_.find(terrain) == non_core_terrains_.end();
		tooltip_text << map().get_terrain_editor_string(terrain);
		if (gui_.get_draw_terrain_codes()) {
			tooltip_text << " " + utils::unicode_em_dash + " " << terrain;
		}
		if (!is_core) {
			tooltip_text << " "
					<< font::span_color(font::BAD_COLOR)
					<< _("(non-core)") << "\n"
					<< _("Will not work in game without extra care.")
					<< "</span>";
		}
		tooltips::add_tooltip(dstrect, tooltip_text.str());
		if ( (counter_from_zero +1) % ( palrect.h / size_specs_.terrain_space ) == 0)
			x += size_specs_.terrain_space;
	}
	update_rect(loc);
	set_dirty(false);
}

int terrain_palette::tile_selected(const int x, const int y) const {
	for(unsigned int i = 0; i != nterrains_; i++) {
		const int px = size_specs_.palette_x + (i % size_specs_.terrain_width) * size_specs_.terrain_space;
		const int py = terrain_start_ + (i / size_specs_.terrain_width) * size_specs_.terrain_space;
		const int pw = size_specs_.terrain_space;
		const int ph = size_specs_.terrain_space;

		if(x > px && x < px + pw && y > py && y < py + ph) {
			return i;
		}
	}
	return -1;
}

void terrain_palette::update_report()
{
	std::ostringstream msg;
	msg << _("FG: ") << get_terrain_string(selected_fg_terrain())
		<< '\n' << _("BG: ") << get_terrain_string(selected_bg_terrain());
	selected_terrain = msg.str();
}

brush_bar::brush_bar(display &gui, const size_specs &sizes,
	std::vector<brush>& brushes, brush** the_brush)
: gui::widget(gui.video()), size_specs_(sizes), gui_(gui),
selected_(0), brushes_(brushes), the_brush_(the_brush),
size_(30) {
	adjust_size();
}

void brush_bar::adjust_size() {
	set_location(size_specs_.brush_x -1, size_specs_.brush_y -1);
	set_measurements(size_ * brushes_.size() + (brushes_.size() -1) * (size_specs_.brush_padding) + 2, (size_ +2));
	set_dirty();
}

unsigned int brush_bar::selected_brush_size() {
	return selected_;
}

void brush_bar::left_mouse_click(const int mousex, const int mousey) {
	sound::play_UI_sound(game_config::sounds::button_press);
	int index = selected_index(mousex, mousey);
	if(index >= 0) {
		if (static_cast<size_t>(index) != selected_) {
			set_dirty();
			selected_ = index;
			*the_brush_ = &brushes_[index];
		}
	}
}

void brush_bar::handle_event(const SDL_Event& event) {
	if (event.type == SDL_MOUSEMOTION) {
		// If the mouse is inside the palette, give it focus.
		if (point_in_rect(event.button.x, event.button.y, location())) {
			if (!focus(&event)) {
				set_focus(true);
			}
		}
		// If the mouse is outside, remove focus.
		else {
			if (focus(&event)) {
				set_focus(false);
			}
		}
	}
	if (!focus(&event)) {
		return;
	}
	int mousex, mousey;
	SDL_GetMouseState(&mousex,&mousey);
	const SDL_MouseButtonEvent mouse_button_event = event.button;
	if (mouse_button_event.type == SDL_MOUSEBUTTONDOWN) {
		if (mouse_button_event.button == SDL_BUTTON_LEFT) {
			left_mouse_click(mousex, mousey);
		}
	}
}

void brush_bar::draw() {
	draw(false);
}

void brush_bar::draw(bool force) {
	if (!dirty() && !force) {
		return;
	}
	const SDL_Rect loc = location();
	int x = loc.x;
	// Everything will be redrawn even though only one little part may
	// have changed, but that happens so seldom so we'll settle with this.
	surface screen = gui_.video().getSurface();
	for (size_t i = 0; i < brushes_.size(); i++) {
		std::string filename = brushes_[i].image();
		surface image(image::get_image(filename));
		if (image == NULL) {
			ERR_ED << "Image " << filename << " not found." << std::endl;
			continue;
		}
		if (static_cast<unsigned>(image->w) != size_
		|| static_cast<unsigned>(image->h) != size_) {
			image.assign(scale_surface(image, size_, size_));
		}
		SDL_Rect dstrect = create_rect(x, size_specs_.brush_y, image->w, image->h);
		sdl_blit(image, NULL, screen, &dstrect);
		const Uint32 color = i == selected_brush_size() ?
			SDL_MapRGB(screen->format,0xFF,0x00,0x00) :
			SDL_MapRGB(screen->format,0x00,0x00,0x00);
		//TODO look at the todo above
		//draw_rectangle(dstrect.x -1, dstrect.y -1, image->w +2, image->h+2, color, screen);
		draw_rectangle(dstrect.x, dstrect.y, image->w, image->h, color, screen);
		x += image->w + size_specs_.brush_padding;
	}
	update_rect(loc);
	set_dirty(false);
}

int brush_bar::selected_index(int x, int y) const {
	const int bar_x = size_specs_.brush_x;
	const int bar_y = size_specs_.brush_y;

	if ((x < bar_x || static_cast<size_t>(x) > bar_x + size_ * brushes_.size() +
	                  brushes_.size() * size_specs_.brush_padding) ||
	    (y < bar_y || static_cast<size_t>(y) > bar_y + size_)) {

		return -1;
	}

	for(size_t i = 0; i <  brushes_.size(); i++) {
		int px = bar_x + size_ * i + i * size_specs_.brush_padding;
		if (x >= px && x <= px + static_cast<int>(size_) && y >= bar_y && y <= bar_y + static_cast<int>(size_)) {
			return i;
		}
	}
	return -1;
}

} // end namespace editor

