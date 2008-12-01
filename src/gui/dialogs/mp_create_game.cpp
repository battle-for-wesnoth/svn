/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/dialogs/mp_create_game.hpp"

#include "foreach.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/integer_selector.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/minimap.hpp"
#include "../../settings.hpp"

namespace gui2 {

tmp_create_game::tmp_create_game(const config& cfg) :
	cfg_(cfg),
	scenario_(NULL),
	use_map_settings_(register_bool("use_map_settings", false, 
		preferences::use_map_settings, 
		preferences::set_use_map_settings,
		dialog_callback<tmp_create_game, &tmp_create_game::update_map_settings>)),
	fog_(register_bool("fog", false, 
			preferences::fog, 
			preferences::set_fog)),
	shroud_(register_bool("shroud", false, 
			preferences::shroud, 
			preferences::set_shroud)),
	start_time_(register_bool("random_start_time", false, 
			preferences::random_start_time, 
			preferences::set_random_start_time)),

	turns_(register_integer("turn_count", false, 
		preferences::turns , 
		preferences::set_turns)),
	gold_(register_integer("village_gold", false, 
		preferences::village_gold , 
		preferences::set_village_gold)),
	experience_(register_integer("experience_modifier", false, 
		preferences::xp_modifier , 
		preferences::set_xp_modifier))
{
}

twindow tmp_create_game::build_window(CVideo& video)
{
	return build(video, get_id(MP_CREATE_GAME));
}

void tmp_create_game::pre_show(CVideo& /*video*/, twindow& window)
{
	tminimap* minimap = dynamic_cast<tminimap*>(window.find_widget("minimap", false));
	VALIDATE(minimap, missing_widget("minimap"));
	minimap->set_config(&cfg_);

	tlistbox* list = dynamic_cast<tlistbox*>(window.find_widget("map_list", false));
	VALIDATE(list, missing_widget("map_list"));

	list->set_callback_value_change(dialog_callback<tmp_create_game, &tmp_create_game::update_map>);

	// Load option (might turn it into a button later).
	string_map item;
	item.insert(std::make_pair("label", _("Load Game")));
	item.insert(std::make_pair("tooltip", _("Load Game...")));
	list->add_row(item);

	// User maps
/*	FIXME implement user maps
	std::vector<std::string> maps;
	get_files_in_dir(get_user_data_dir() + "/editor/maps", &maps, NULL, FILE_NAME_ONLY);

	foreach(const std::string& map, maps) {
		std::map<std::string, t_string> item;
		item.insert(std::make_pair("label", map));
		list->add_row(item);
	}
*/	

	// Standard maps
	int i = 0;
	foreach(const config* map, cfg_.get_children("multiplayer")) {

		if(utils::string_bool((*map)["allow_new_game"], true)) {
			string_map item;
			item.insert(std::make_pair("label", (*map)["name"]));
			item.insert(std::make_pair("tooltip", (*map)["name"]));
			list->add_row(item);

			// This hack is needed since the next item is too wide to fit.
			// and the scrollbar can't truncate text yet.
			if(++i == 46) {
				break;
			}
		}
	}

	update_map_settings(window);
}

void tmp_create_game::post_show(twindow& window)
{
	if(get_retval() == twindow::OK) {
		tlistbox* list = dynamic_cast<tlistbox*>(window.find_widget("map_list", false));
		assert(list);

	}
}

void tmp_create_game::update_map(twindow& window)
{
	tlistbox* list = dynamic_cast<tlistbox*>(window.find_widget("map_list", false));
	VALIDATE(list, missing_widget("map_list"));

	tminimap* minimap = dynamic_cast<tminimap*>(window.find_widget("minimap", false));
	VALIDATE(list, missing_widget("minimap"));

	const int index = list->get_selected_row() - 1;

	if(index >= 0) {
		scenario_ = cfg_.get_children("multiplayer")[index];
		minimap->set_map_data((*scenario_)["map_data"]);
	} else {
		minimap->set_map_data("");
		scenario_ = NULL;
	}

	update_map_settings(window);
}

void tmp_create_game::update_map_settings(twindow& window)
{
	const bool use_map_settings = use_map_settings_->get_widget_value(window);

	fog_->widget_set_enabled(window, !use_map_settings, false);
	shroud_->widget_set_enabled(window, !use_map_settings, false);
	start_time_->widget_set_enabled(window, !use_map_settings, false);

	turns_->widget_set_enabled(window, !use_map_settings, false);
	gold_->widget_set_enabled(window, !use_map_settings, false);
	experience_->widget_set_enabled(window, !use_map_settings, false);
	
	if(use_map_settings) {
		if(scenario_) {

			fog_->set_widget_value(window, ::settings::use_fog((*(*scenario_).get_children("side").front())["fog"]));
			shroud_->set_widget_value(window, ::settings::use_shroud((*(*scenario_).get_children("side").front())["shroud"]));
			start_time_->set_widget_value(window, ::settings::use_random_start_time((*scenario_)["random_start_time"]));

			turns_->set_widget_value(window, ::settings::get_turns((*scenario_)["turns"]));
			gold_->set_widget_value(window, ::settings::get_village_gold((*(*scenario_).get_children("side").front())["village_gold"]));
			experience_->set_widget_value(window, ::settings::get_xp_modifier((*scenario_)["experience_modifier"]));
		}
		// No scenario selected just leave the state unchanged for now.

	} else {

		// Fixme we should store the value and reuse it later...
		fog_->set_widget_value(window, preferences::fog());
		shroud_->set_widget_value(window, preferences::shroud());
		start_time_->set_widget_value(window, preferences::random_start_time());

		turns_->set_widget_value(window, preferences::turns());
		gold_->set_widget_value(window, preferences::village_gold());
		experience_->set_widget_value(window, preferences::xp_modifier());
	}
}

} // namespace gui2

