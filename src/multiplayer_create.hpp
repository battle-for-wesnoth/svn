/* $Id$ */
/*
   Copyright (C) 
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef MULTIPLAYER_CREATE_HPP_INCLUDED
#define MULTIPLAYER_CREATE_HPP_INCLUDED

#include "multiplayer_ui.hpp"
#include "widgets/slider.hpp"
#include "widgets/label.hpp"
#include "widgets/combo.hpp"
#include "mapgen.hpp"
#include "tooltips.hpp"

namespace mp {

class create : public mp::ui, public font::floating_label_context
{
public:
	struct parameters
	{
		parameters() { reset(); };

		void reset() {
			name = "";
			era = "";
			num_turns = 0;
			village_gold = 0;
			xp_modifier = 0;
			fog_game = shroud_game = allow_observers = share_view = share_maps = false;

			scenario_data.clear();
		}

		// The items returned while configuring the game

		std::string name;
		std::string era;

		int num_turns;
		int village_gold;
		int xp_modifier;
		bool fog_game;
		bool shroud_game;
		bool allow_observers;
		bool share_view;
		bool share_maps;

		bool saved_game;

		// If the game is to be randomly generated, the map generator
		// will create the scenario data in this variable:
		config scenario_data;
	};

	create(display& dist, const config& game_config, chat& c, config& gamelist);
	~create();

	parameters& get_parameters();

protected:
	virtual void layout_children(const SDL_Rect& rect);
	virtual void process_event();
	virtual void hide_children(bool hide=true);

private:
	void update_minimap(void);

	tooltips::manager tooltip_manager_;
	int map_selection_;

	std::vector<std::string> user_maps_;
	std::vector<std::string> map_options_;

	gui::menu maps_menu_;
	gui::slider turns_slider_;
	gui::label turns_label_;
	gui::slider village_gold_slider_;
	gui::label village_gold_label_;
	gui::slider xp_modifier_slider_;
	gui::label xp_modifier_label_;

	gui::label name_entry_label_;
	gui::label num_players_label_;
	gui::label era_label_;
	gui::label map_label_;

	gui::button fog_game_;
	gui::button shroud_game_;
	gui::button observers_game_;
	gui::button cancel_game_;
	gui::button launch_game_;
	gui::button regenerate_map_;
	gui::button generator_settings_;

	gui::combo era_combo_;
	gui::combo vision_combo_;
	gui::textbox name_entry_;

	util::scoped_ptr<surface_restorer> minimap_restorer_;
	SDL_Rect minimap_rect_;

	util::scoped_ptr<map_generator> generator_;

	parameters parameters_;
};

}

#endif

