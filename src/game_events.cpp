/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "game_events.hpp"
#include "image.hpp"
#include "language.hpp"
#include "log.hpp"
#include "playlevel.hpp"
#include "replay.hpp"
#include "show_dialog.hpp"
#include "sound.hpp"
#include "unit_display.hpp"
#include "util.hpp"

#include <cstdlib>
#include <deque>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

namespace game_events {

bool conditional_passed(const std::map<gamemap::location,unit>* units,
                        const config& cond)
{
	//an 'or' statement means that if the contained statements are true,
	//then it automatically passes
	const config::child_list& or_statements = cond.get_children("or");
	for(config::child_list::const_iterator or_it = or_statements.begin();
	    or_it != or_statements.end(); ++or_it) {
		if(conditional_passed(units,**or_it)) {
			return true;
		}
	}

	//if the if statement requires we have a certain unit, then
	//check for that.
	const config::child_list& have_unit = cond.get_children("have_unit");

	for(config::child_list::const_iterator u = have_unit.begin();
	    u != have_unit.end(); ++u) {

		if(units == NULL)
			return false;

		std::map<gamemap::location,unit>::const_iterator itor;
		for(itor = units->begin(); itor != units->end(); ++itor) {
			if(itor->second.hitpoints() > 0 && game_events::unit_matches_filter(itor, **u)) {
				break;
			}
		}

		if(itor == units->end()) {
			return false;
		}
	}

	//check against each variable statement to see if the variable
	//matches the conditions or not
	const config::child_list& variables = cond.get_children("variable");
	for(config::child_list::const_iterator var = variables.begin();
	    var != variables.end(); ++var) {
		const config& values = **var;

		const std::string& name = values["name"];
		const std::string& value = game_events::get_variable(name);

		const double num_value = atof(value.c_str());

		const std::string& equals = values["equals"];
		if(equals != "" && value != equals) {
			return false;
		}

		const std::string& numerical_equals = values["numerical_equals"];
		if(numerical_equals != "" && atof(numerical_equals.c_str()) != num_value){
			return false;
		}

		const std::string& not_equals = values["not_equals"];
		if(not_equals != "" && not_equals == value) {
			return false;
		}

		const std::string& numerical_not_equals = values["numerical_not_equals"];
		if(numerical_not_equals != "" && atof(numerical_not_equals.c_str()) == num_value){
			return false;
		}

		const std::string& greater_than = values["greater_than"];
		if(greater_than != "" && atof(greater_than.c_str()) >= num_value){
			return false;
		}

		const std::string& less_than = values["less_than"];
		if(less_than != "" && atof(less_than.c_str()) <= num_value){
			return false;
		}

		const std::string& greater_than_equal_to = values["greater_than_equal_to"];
		if(greater_than_equal_to != "" && atof(greater_than_equal_to.c_str()) > num_value){
			return false;
		}

		const std::string& less_than_equal_to = values["less_than_equal_to"];
		if(less_than_equal_to != "" && atof(less_than_equal_to.c_str()) < num_value) {
			return false;
		}
	}

	return !have_unit.empty() || !variables.empty();
}

} //end namespace game_events

namespace {

display* screen = NULL;
gamemap* game_map = NULL;
std::map<gamemap::location,unit>* units = NULL;
std::vector<team>* teams = NULL;
game_state* state_of_game = NULL;
game_data* game_data_ptr = NULL;
gamestatus* status_ptr = NULL;
std::set<std::string> used_items;

const size_t MaxLoop = 1024;

bool events_init() { return screen != NULL; }

struct queued_event {
	queued_event(const std::string& name, const gamemap::location& loc1,
	                                      const gamemap::location& loc2)
			: name(name), loc1(loc1), loc2(loc2) {}

	std::string name;
	gamemap::location loc1;
	gamemap::location loc2;
};

std::deque<queued_event> events_queue;

class event_handler
{
public:
	event_handler(const config* cfg) : name_((*cfg)["name"]),
	                 first_time_only_((*cfg)["first_time_only"] != "no"),
					 disabled_(false),
	                 cfg_(cfg)
	{}

	void write(config& cfg) const {
		if(cfg_ == NULL || disabled_)
			return;

		cfg = *cfg_;
	}

	const std::string& name() const { return name_; }

	bool first_time_only() const { return first_time_only_; }

	void disable() { disabled_ = true; }
	bool disabled() const { return disabled_; }

	const config::child_list& first_arg_filters() {
		return cfg_->get_children("filter");
	}

	const config::child_list& second_arg_filters() {
		return cfg_->get_children("filter_second");
	}

	void handle_event(const queued_event& event_info, const config* cfg=NULL);

private:
	bool handle_event_command(const queued_event& event_info, const std::string& cmd, const config& cfg);

	std::string name_;
	bool first_time_only_;
	bool disabled_;
	const config* cfg_;
};

std::vector<gamemap::location> multiple_locs(const config& cfg)
{
	return parse_location_range(cfg["x"],cfg["y"]);
}

std::multimap<std::string,event_handler> events_map;

//this function handles all the different types of actions that can be triggered
//by an event.
bool event_handler::handle_event_command(const queued_event& event_info, const std::string& cmd, const config& cfg)
{
	log_scope("handle_event_command");
	std::cerr << "handling command: '" << cmd << "'\n";

	bool rval = true;
	//sub commands that need to be handled in a guaranteed ordering
	if(cmd == "command") {
		handle_event(event_info,&cfg);
	}

	//change shroud settings for portions of the map
	else if(cmd == "remove_shroud" || cmd == "place_shroud") {
		const bool remove = cmd == "remove_shroud";

		const size_t index = maximum<int>(1,atoi(cfg["side"].c_str())) - 1;
		if(index < teams->size()) {
			const std::vector<gamemap::location>& locs = multiple_locs(cfg);
			for(std::vector<gamemap::location>::const_iterator j = locs.begin(); j != locs.end(); ++j) {
				if(remove) {
					(*teams)[index].clear_shroud(j->x,j->y);
				} else {
					(*teams)[index].place_shroud(j->x,j->y);
				}
			}
		}
		
		screen->invalidate_all();
	}


	//teleport a unit from one location to another
	else if(cmd == "teleport") {
		
		unit_map::iterator u = units->find(event_info.loc1);

		//search for a valid unit filter, and if we have one, look for the matching unit
		const config* const filter = cfg.child("filter");
		if(filter != NULL) {	
			for(u = units->begin(); u != units->end(); ++u){
				if(game_events::unit_matches_filter(u,*filter))
					break;
			}
		}

		//we have found a unit that matches the filter
		if(u != units->end()) {
			const gamemap::location dst(cfg);
			if(game_map->on_board(dst)) {
				const gamemap::location vacant_dst = find_vacant_tile(*game_map,*units,dst);
				if(game_map->on_board(vacant_dst)) {
					const int side = u->second.side();

					//note that inserting into a map does not invalidate iterators
					//into the map, so this sequence is fine.
					units->insert(std::pair<gamemap::location,unit>(vacant_dst,u->second));
					units->erase(u);

					if(game_map->is_village(vacant_dst)) {
						get_village(vacant_dst,*teams,side,*units);
					}

					if(cfg["clear_shroud"] != "no") {
						clear_shroud(*screen,*status_ptr,*game_map,*game_data_ptr,*units,*teams,side-1);
					}
				}
			}
		}
	}

	//remove units from being turned to stone
	else if(cmd == "unstone") {
		const config* const filter = cfg.child("filter");
		for(unit_map::iterator i = units->begin(); i != units->end(); ++i) {
			if(i->second.stone() && (filter == NULL || i->second.matches_filter(*filter))) {
				i->second.remove_flag("stone");
			}
		}
	}

	//allow a side to recruit a new type of unit
	else if(cmd == "allow_recruit") {
		const int side = maximum<int>(1,atoi(cfg["side"].c_str()));
		const size_t index = side-1;
		if(index >= teams->size())
			return rval;

		const std::string& type = cfg["type"];

		const std::vector<std::string>& types = config::split(type);
		for(std::vector<std::string>::const_iterator i = types.begin(); i != types.end(); ++i) {
			(*teams)[index].recruits().insert(*i);
			if(index == 0) {
				state_of_game->can_recruit.insert(*i);
			}
		}
	}

	//remove the ability to recruit a unit from a certain side
	else if(cmd == "disallow_recruit") {
		const int side = maximum<int>(1,atoi(cfg["side"].c_str()));
		const size_t index = side-1;
		if(index >= teams->size())
			return rval;

		const std::string& type = cfg["type"];
		const std::vector<std::string>& types = config::split(type);
		for(std::vector<std::string>::const_iterator i = types.begin(); i != types.end(); ++i) {
			(*teams)[index].recruits().erase(*i);
			if(index == 0) {
				state_of_game->can_recruit.erase(*i);
			}
		}
	}

	else if(cmd == "set_recruit") {
		const int side = maximum<int>(1,atoi(cfg["side"].c_str()));
		const size_t index = side-1;
		if(index >= teams->size())
			return rval;

		std::vector<std::string> recruit = config::split(cfg["recruit"]);
		if(recruit.size() == 1 && recruit.back() == "")
			recruit.clear();

		std::set<std::string>& can_recruit = (*teams)[index].recruits();
		can_recruit.clear();
		std::copy(recruit.begin(),recruit.end(),std::inserter(can_recruit,can_recruit.end()));
		if(index == 0) {
			state_of_game->can_recruit = can_recruit;
		}
	}

	else if(cmd == "music") {
		sound::play_music(cfg["name"]);
	}

	else if(cmd == "sound") {
		sound::play_sound(cfg["name"]);
	}

	else if(cmd == "colour_adjust") {
		const int r = atoi(cfg["red"].c_str());
		const int g = atoi(cfg["green"].c_str());
		const int b = atoi(cfg["blue"].c_str());
		screen->adjust_colours(r,g,b);
		screen->invalidate_all();
		screen->draw(true,true);
	}

	else if(cmd == "delay") {
		const int delay_time = atoi(cfg["time"].c_str());
		::SDL_Delay(delay_time);
	}

	else if(cmd == "scroll") {
		const int xoff = atoi(cfg["x"].c_str());
		const int yoff = atoi(cfg["y"].c_str());
		screen->scroll(xoff,yoff);
		screen->draw(true,true);
	}

	else if(cmd == "scroll_to_unit") {
		unit_map::const_iterator u;
		for(u = units->begin(); u != units->end(); ++u){
			if(game_events::unit_matches_filter(u,cfg))
				break;
		}

		if(u != units->end()) {
			screen->scroll_to_tile(u->first.x,u->first.y);
		}
	}

	//an award of gold to a particular side
	else if(cmd == "gold") {
		const std::string& side = cfg["side"];
		const std::string& amount = cfg["amount"];
		const int side_num = side.empty() ? 1 : atoi(side.c_str());
		const int amount_num = atoi(amount.c_str());
		const size_t team_index = side_num-1;
		if(team_index < teams->size()) {
			(*teams)[team_index].spend_gold(-amount_num);
		}
	}

	//modifications of some attributes of a side: gold, income, team name
	else if(cmd == "modify_side") {
		const std::string& side = cfg["side"];
		const std::string& income = cfg["income"];
		const std::string& team_name = cfg["team_name"];
		const std::string& gold = cfg["gold"];
		const int side_num = lexical_cast_default<int>(side.c_str(),1);
		const size_t team_index = side_num-1;

		if(team_index < teams->size()) {
			if(!team_name.empty()) {
				(*teams)[team_index].change_team(team_name);
			}

			if(!income.empty()) {
				(*teams)[team_index].set_income(lexical_cast_default<int>(income));
			}

			if(!gold.empty()) {
				(*teams)[team_index].spend_gold((*teams)[team_index].gold()-lexical_cast_default<int>(gold));
			}
		}
	}

	//command to store gold into a variable
	else if(cmd == "store_gold") {
		const std::string& side = cfg["side"];
		std::string var_name = cfg["variable"];
		if(var_name.empty()) {
			var_name = "gold";
		}

		const int side_num = side.empty() ? 1 : atoi(side.c_str());
		const size_t team_index = side_num-1;
		if(team_index < teams->size()) {
			char value[50];
			sprintf(value,"%d",(*teams)[team_index].gold());
			game_events::set_variable(var_name,value);
		}
	}

	//moving a 'unit' - i.e. a dummy unit that is just moving for
	//the visual effect
	else if(cmd == "move_unit_fake") {
		const std::string& type = cfg["type"];
		const game_data::unit_type_map::const_iterator itor = game_data_ptr->unit_types.find(type);
		if(itor != game_data_ptr->unit_types.end()) {
			unit dummy_unit(&itor->second,0,false,true);
			const std::vector<std::string> xvals = config::split(cfg["x"]);
			const std::vector<std::string> yvals = config::split(cfg["y"]);
			std::vector<gamemap::location> path;
			for(size_t i = 0; i != minimum(xvals.size(),yvals.size()); ++i) {
				path.push_back(gamemap::location(atoi(xvals[i].c_str())-1,
				                                 atoi(yvals[i].c_str())-1));
			}

			unit_display::move_unit(*screen,*game_map,path,dummy_unit);
		}
	}

	//setting a variable
	else if(cmd == "set_variable") {
		const std::string& name = config::interpolate_variables_into_string(cfg.get_attribute("name"));
		std::string& var = game_events::get_variable(name);
		const std::string& value = cfg["value"];
		if(value.empty() == false) {
			var = value;
		}

		const std::string& format = config::interpolate_variables_into_string(cfg.get_attribute("format"));
		if(format.empty() == false) {
			var = format;
		}

		const std::string& to_variable = config::interpolate_variables_into_string(cfg.get_attribute("to_variable"));
		if(to_variable.empty() == false) {
			var = game_events::get_variable(to_variable);
		}

		const std::string& add = cfg["add"];
		if(add.empty() == false) {
			int value = int(atof(var.c_str()));
			value += atoi(add.c_str());
			char buf[50];
			sprintf(buf,"%d",value);
			var = buf;
		}

		const std::string& multiply = cfg["multiply"];
		if(multiply.empty() == false) {
			int value = int(atof(var.c_str()));
			value = int(double(value) * atof(multiply.c_str()));
			char buf[50];
			sprintf(buf,"%d",value);
			var = buf;
		}

		// random generation works as follows:
		// random=[comma delimited list]
		// Each element in the list will be considered a separate choice, 
		// unless it contains "..". In this case, it must be a numerical
		// range. (i.e. -1..-10, 0..100, -10..10, etc)
		const std::string& random = cfg["random"];
		if(random.empty() == false) {
			std::string random_value, word;
			std::vector<std::string> words;
			std::vector<std::pair<int,int> > ranges;
			int num_choices = 0;
			int pos = 0, pos2 = -1, tmp; 
			std::stringstream ss(std::stringstream::in|std::stringstream::out);
			while (pos2 != (int)random.length()) {
				pos = pos2+1;
				pos2 = random.find(",", pos);

				if (pos2 == std::string::npos) 
					pos2 = random.length();

				word = random.substr(pos, pos2-pos);
				words.push_back(word);
				tmp = word.find("..");
				
				
				if (tmp == std::string::npos) {
					// treat this element as a string
					ranges.push_back(std::pair<int, int>(0,0));
					num_choices += 1;
				}
				else {
					// treat as a numerical range
					const std::string first = word.substr(0, tmp);
					const std::string second = word.substr(tmp+2,
														random.length());

					int low, high;
					ss << first + " " + second;
					ss >> low;
					ss >> high;
					ss.clear();

					if (low > high) {
						tmp = low;
						low = high;
						high = tmp;
					}
					ranges.push_back(std::pair<int, int>(low,high));
					num_choices += (high - low) + 1;
				}
			}

			int choice = get_random() % num_choices;
			tmp = 0;	
			for (int i = 0; i < ranges.size(); i++) {
				tmp += (ranges[i].second - ranges[i].first) + 1;
				if (tmp > choice) {
					if (ranges[i].first == 0 && ranges[i].second == 0) {
						random_value = words[i];
					}
					else {
						tmp = (ranges[i].second - (tmp - choice)) + 1;
						ss << tmp;
						ss >> random_value;
					}
					break;
				}
			}

			var = random_value;
		}
	}

	//conditional statements
	else if(cmd == "if" || cmd == "while") {
		log_scope(cmd);
		const size_t max_iterations = (cmd == "if" ? 1 : MaxLoop);
		const std::string pass = (cmd == "if" ? "then" : "do");
		const std::string fail = (cmd == "if" ? "else" : "");
		for(size_t i = 0; i != max_iterations; ++i) {
			const std::string type = game_events::conditional_passed(
			                              units,cfg) ? pass : fail;

			if(type == "") {
				break;
			}

			//if the if statement passed, then execute all 'then' statements,
			//otherwise execute 'else' statements
			const config::child_list& commands = cfg.get_children(type);
			for(config::child_list::const_iterator cmd = commands.begin();
			    cmd != commands.end(); ++cmd) {
				handle_event(event_info,*cmd);
			}
		}
	}

	else if(cmd == "role") {

		//get a list of the types this unit can be
		std::vector<std::string> types = config::split(cfg["type"]);

		//iterate over all the types, and for each type, try to find
		//a unit that matches
		std::vector<std::string>::iterator ti;
		for(ti = types.begin(); ti != types.end(); ++ti) {
			config item = cfg;
			item["type"] = *ti;
			item["role"] = "";

			std::map<gamemap::location,unit>::iterator itor;
			for(itor = units->begin(); itor != units->end(); ++itor) {
				if(game_events::unit_matches_filter(itor,item)) {
					itor->second.assign_role(cfg["role"]);
					break;
				}
			}

			if(itor != units->end())
				break;

			std::vector<unit>::iterator ui;
			//iterate over the units, and try to find one that matches
			for(ui = state_of_game->available_units.begin();
			    ui != state_of_game->available_units.end(); ++ui) {
				if(ui->matches_filter(item)) {
					ui->assign_role(cfg["role"]);
					break;
				}
			}

			//if we found a unit, we don't have to keep going.
			if(ui != state_of_game->available_units.end())
				break;
		}
	}

	else if(cmd == "removeitem") {
		gamemap::location loc(cfg);
		if(!loc.valid()) {
			loc = event_info.loc1;
		}

		screen->remove_overlay(loc);
	}

	else if(cmd == "unit_overlay") {
		for(std::map<gamemap::location,unit>::iterator itor = units->begin(); itor != units->end(); ++itor) {
			if(game_events::unit_matches_filter(itor,cfg)) {
				itor->second.add_overlay(cfg["image"]);
				break;
			}
		}
	}

	else if(cmd == "remove_unit_overlay") {
		for(std::map<gamemap::location,unit>::iterator itor = units->begin(); itor != units->end(); ++itor) {
			if(game_events::unit_matches_filter(itor,cfg)) {
				itor->second.remove_overlay(cfg["image"]);
				break;
			}
		}
	}

	//hiding units
	else if(cmd == "hide_unit") {
		const gamemap::location loc(cfg);
		screen->hide_unit(loc,true);
		screen->draw_tile(loc.x,loc.y);
	}

	else if(cmd == "unhide_unit") {
		const gamemap::location loc = screen->hide_unit(gamemap::location());
		screen->draw_tile(loc.x,loc.y);
	}

	//adding new items
	else if(cmd == "item") {
		gamemap::location loc(cfg);
		const std::string& img = cfg["image"];
		const std::string& halo = cfg["halo"];
		if(!img.empty() || !halo.empty()) {
			screen->add_overlay(loc,img,cfg["halo"]);
			screen->draw_tile(loc.x,loc.y);
		}
	}

	//changing the terrain
	else if(cmd == "terrain") {
		const std::vector<gamemap::location> locs = multiple_locs(cfg);

		for(std::vector<gamemap::location>::const_iterator loc = locs.begin(); loc != locs.end(); ++loc) {
			const std::string& terrain_type = cfg["letter"];
			if(terrain_type.size() > 0) {
				game_map->set_terrain(*loc,terrain_type[0]);
			}
		}
		screen->recalculate_minimap();
		screen->invalidate_all();
		screen->rebuild_all();
	}

	//if we should spawn a new unit on the map somewhere
	else if(cmd == "unit") {
		unit new_unit(*game_data_ptr,cfg);
		gamemap::location loc(cfg);

		if(game_map->on_board(loc)) {
			loc = find_vacant_tile(*game_map,*units,loc);
			units->insert(std::pair<gamemap::location,unit>(loc,new_unit));
			if(game_map->is_village(loc)) {
				get_village(loc,*teams,new_unit.side()-1,*units);
			}

			screen->invalidate(loc);
		} else {
			state_of_game->available_units.push_back(new_unit);
		}
	}

	//if we should recall units that match a certain description
	else if(cmd == "recall") {
		std::vector<unit>& avail = state_of_game->available_units;
		for(std::vector<unit>::iterator u = avail.begin(); u != avail.end(); ++u) {
			if(u->matches_filter(cfg)) {
				gamemap::location loc(cfg);
				recruit_unit(*game_map,1,*units,*u,loc,cfg["show"] == "no" ? NULL : screen,false,true);
				avail.erase(u);
				break;
			}
		}
	}

	else if(cmd == "object") {
		const config* filter = cfg.child("filter");

		const std::string& id = cfg["id"];

		//if this item has already been used
		if(id != "" && used_items.count(id))
			return rval;

		const std::string image = cfg["image"];
		std::string caption = cfg["name"];

		const std::string& caption_lang = string_table[id + "_name"];
		if(caption_lang.empty() == false)
			caption = caption_lang;

		std::string text;

		gamemap::location loc;
		if(filter != NULL) {
			for(unit_map::const_iterator u = units->begin(); u != units->end(); ++u) {
				if(game_events::unit_matches_filter(u,*filter)) {
					loc = u->first;
					break;
				}
			}
		}

		if(loc.valid() == false) {
			loc = event_info.loc1;
		}

		const unit_map::iterator u = units->find(loc);

		std::string command_type = "then";

		if(u != units->end() && (filter == NULL || u->second.matches_filter(*filter))) {
			const std::string& lang = string_table[id];
			if(!lang.empty())
				text = lang;
			else
				text = cfg["description"];

			u->second.add_modification("object",cfg);

			screen->select_hex(event_info.loc1);
			screen->invalidate_unit();

			//mark that this item won't be used again
			used_items.insert(id);
		} else {
			const std::string& lang = string_table[id + "_cannot_use"];
			if(!lang.empty())
				text = lang;
			else
				text = cfg["cannot_use_message"];

			command_type = "else";
		}

		if(cfg["silent"] != "yes") {
			surface surface(NULL);

			if(image.empty() == false) {
				surface.assign(image::get_image(image,image::UNSCALED));
			}

			//this will redraw the unit, with its new stats
			screen->draw();

			gui::show_dialog(*screen,surface,caption,text);
		}

		const config::child_list& commands = cfg.get_children(command_type);
		for(config::child_list::const_iterator cmd = commands.begin();
		    cmd != commands.end(); ++cmd) {
			handle_event(event_info,*cmd);
		}
	}

	//displaying a message on-screen
	else if(cmd == "print") {
		const std::string& text = cfg["text"];
		const std::string& id = cfg["id"];
		const int size = lexical_cast_default<int>(cfg["size"],12);
		const int lifetime = lexical_cast_default<int>(cfg["duration"],20);
		const int red = lexical_cast_default<int>(cfg["red"],0);
		const int green = lexical_cast_default<int>(cfg["green"],0);
		const int blue = lexical_cast_default<int>(cfg["blue"],0);

		SDL_Color colour = {red,green,blue,255};

		const std::string& msg = translate_string_default(id,text);
		if(msg != "") {
			const SDL_Rect rect = screen->map_area();
			font::add_floating_label(msg,size,colour,rect.w/2,rect.h/2,
			                         0.0,0.0,lifetime,rect,font::CENTER_ALIGN);
		}
	}

	//displaying a message dialog
	else if(cmd == "message") {
		std::map<gamemap::location,unit>::iterator speaker = units->end();
		if(cfg["speaker"] == "unit") {
			speaker = units->find(event_info.loc1);
		} else if(cfg["speaker"] == "second_unit") {
			speaker = units->find(event_info.loc2);
		} else if(cfg["speaker"] != "narrator") {
			for(speaker = units->begin(); speaker != units->end(); ++speaker){
				if(game_events::unit_matches_filter(speaker,cfg))
					break;
			}
		}
	
		if(speaker == units->end()) {
			//no matching unit found, so the dialog can't come up
			//continue onto the next message
			std::cerr << "cannot show message\n";
			return rval;
		}

		std::cerr << "set speaker to '" << speaker->second.description() << "'\n";

		const std::string& sfx = cfg["sound"];
		if(sfx != "") {
			sound::play_sound(sfx);
		}

		const std::string& id = cfg["id"];

		std::string image = cfg["image"];
		std::string caption;

		const std::string& lang_caption = string_table[id + "_caption"];
		if(!lang_caption.empty())
			caption = lang_caption;
		else
			caption = cfg["caption"];

		if(speaker != units->end()) {
			std::cerr << "scrolling to speaker..\n";
			screen->highlight_hex(speaker->first);
			screen->scroll_to_tile(speaker->first.x,speaker->first.y);

			if(image.empty()) {
				image = speaker->second.type().image_profile();
			}

			if(caption.empty()) {
				caption = speaker->second.description();
				if(caption.empty()) {
					caption = speaker->second.type().language_name();
				}
			}
		}

		std::cerr << "done scrolling to speaker...\n";

		std::vector<std::string> options;
		std::vector<config::const_child_itors> option_events;

		const config::child_list& menu_items = cfg.get_children("option");
		for(config::child_list::const_iterator mi = menu_items.begin();
		    mi != menu_items.end(); ++mi) {
			const std::string& msg = translate_string_default((**mi)["id"],(**mi)["message"]);
			options.push_back(msg);
			option_events.push_back((*mi)->child_range("command"));
		}

		surface surface(NULL);
		if(image.empty() == false) {
			surface.assign(image::get_image(image,image::UNSCALED));
		}

		const std::string& lang_message = string_table[id];
		int option_chosen = -1;

		std::cerr << "showing dialog...\n";
		
		//if we're not replaying, or if we are replaying and there is no choice
		//to be made, show the dialog.
		if(get_replay_source().at_end() || options.empty()) {
			const std::string msg = config::interpolate_variables_into_string(lang_message.empty() ? cfg["message"] : lang_message);
			option_chosen = gui::show_dialog(*screen,surface,caption,msg,
		                        options.empty() ? gui::MESSAGE : gui::OK_ONLY,
		                        options.empty() ? NULL : &options);

			std::cerr << "showed dialog...\n";

			if (option_chosen == gui::ESCAPE_DIALOG){
				rval = false;
			}

			if(options.empty() == false) {
				recorder.choose_option(option_chosen);
			}
		}

		//otherwise if a choice has to be made, get it from the replay data
		else {
			const config* const action = get_replay_source().get_next_action();
			if(action == NULL || action->get_children("choose").empty()) {
				std::cerr << "choice expected but none found\n";
				throw replay::error();
			}

			if(size_t(option_chosen) >= options.size()) {
				option_chosen = 0;
			}

			const std::string& val = (*(action->get_children("choose").front()))["value"];
			option_chosen = atol(val.c_str());
		}

		//implement the consequences of the choice
		if(options.empty() == false) {
			assert(size_t(option_chosen) < menu_items.size());
			
			for(config::const_child_itors events = option_events[option_chosen];
			    events.first != events.second; ++events.first) {
				handle_event(event_info,*events.first);
			}
		}
	}

	else if(cmd == "kill") {

		for(unit_map::iterator un = units->begin(); un != units->end();) {
			if(game_events::unit_matches_filter(un,cfg)) {
				if(cfg["animate"] == "yes") {
					screen->scroll_to_tile(un->first.x,un->first.y,display::WARP);
					unit_display::unit_die(*screen,un->first,un->second);
				}

				if(cfg["fire_event"] == "yes") {
					gamemap::location loc = un->first;
					game_events::fire("die",loc,un->first);
				}
				units->erase(un++);
			} else {
				++un;
			}
		}

		//if the filter doesn't contain positional information, then it may match
		//units on the recall list.
		if(cfg["x"].empty() && cfg["y"].empty()) {
			std::vector<unit>& avail_units = state_of_game->available_units;
			for(std::vector<unit>::iterator j = avail_units.begin(); j != avail_units.end();) {
				if(j->matches_filter(cfg)) {
					j = avail_units.erase(j);
				} else {
					++j;
				}
			}
		}
	}

	//adding of new events
	else if(cmd == "event") {
		event_handler new_handler(&cfg);
		events_map.insert(std::pair<std::string,event_handler>(new_handler.name(),new_handler));
	}

	//unit serialization to and from variables
	else if(cmd == "store_unit") {
		const config empty_filter;
		const config* filter_ptr = cfg.child("filter");
		const config& filter = filter_ptr != NULL ? *filter_ptr : empty_filter;

		const std::string& variable = cfg["variable"];

		config& vars = state_of_game->variables;
		vars.clear_children(variable);

		const bool kill_units = cfg["kill"] == "yes";

		for(unit_map::iterator i = units->begin(); i != units->end();) {
			if(game_events::unit_matches_filter(i,filter) == false) {
				++i;
				continue;
			}

			config& data = vars.add_child(variable);
			i->first.write(data);
			i->second.write(data);

			if(kill_units) {
				units->erase(i++);
			} else {
				++i;
			}
		}

		if(filter["x"].empty() && filter["y"].empty()) {
			std::vector<unit>& avail_units = state_of_game->available_units;
			for(std::vector<unit>::iterator j = avail_units.begin(); j != avail_units.end();) {
				if(j->matches_filter(filter) == false) {
					++j;
					continue;
				}
	
				config& data = vars.add_child(variable);
				j->write(data);
				data["x"] = "recall";
				data["y"] = "recall";
	
				if(kill_units) {
					j = avail_units.erase(j);
				} else {
					++j;
				}
			}
		}
	}

	else if(cmd == "unstore_unit") {
		const config& var = game_events::get_variable_cfg(config::interpolate_variables_into_string(cfg.get_attribute("variable")));

		try {
			const unit u(*game_data_ptr,var);
			gamemap::location loc(var);
			if(loc.valid()) {
				if(cfg["find_vacant"] == "yes") {
					loc = find_vacant_tile(*game_map,*units,loc);
				}

				units->erase(loc);
				units->insert(std::pair<gamemap::location,unit>(loc,u));
			} else {
				state_of_game->available_units.push_back(u);
			}
		} catch(gamestatus::load_game_failed& e) {
			std::cerr << "could not de-serialize unit: '" << e.message << "'\n";
		}
	}

	else if(cmd == "store_starting_location") {
		const int side = lexical_cast_default<int>(cfg["side"]);
		const gamemap::location& loc = game_map->starting_position(side);
		static const std::string default_store = "location";
		const std::string& store = cfg["variable"].empty() ? default_store : cfg["variable"];
		loc.write(game_events::get_variable_cfg(store));
	}

	else if(cmd == "store_locations") {
		log_scope("store_locations");
		const std::string& variable = cfg["variable"];
		const std::string& terrain = cfg["terrain"];
		const config* const unit_filter = cfg.child("filter");

		state_of_game->variables.clear_children(variable);

		std::vector<gamemap::location> locs = parse_location_range(cfg["x"],cfg["y"]);
		if(locs.size() > MaxLoop) {
			locs.resize(MaxLoop);
		}

		const size_t radius = minimum<size_t>(MaxLoop,lexical_cast_default<size_t>(cfg["radius"]));
		std::set<gamemap::location> res;
		for(std::vector<gamemap::location>::const_iterator i = locs.begin(); i != locs.end(); ++i) {
			get_tiles_radius(*i,radius,res);
		}

		size_t added = 0;
		for(std::set<gamemap::location>::const_iterator j = res.begin(); j != res.end() && added != MaxLoop; ++j) {
			if(game_map->on_board(*j)) {
				if(terrain.empty() == false) {
					const gamemap::TERRAIN c = game_map->get_terrain(*j);
					if(std::find(terrain.begin(),terrain.end(),c) == terrain.end()) {
						continue;
					}
				}

				if(unit_filter != NULL) {
					const unit_map::const_iterator u = units->find(*j);
					if(u == units->end() || game_events::unit_matches_filter(u,*unit_filter) == false) {
						continue;
					}
				}

				j->write(state_of_game->variables.add_child(variable));
				++added;
			}
		}
	}

	//command to take control of a village for a certain side
	else if(cmd == "capture_village") {
		const int side = lexical_cast_default<int>(cfg["side"]);

		//if 'side' is 0, then it will become an invalid index, and so
		//the village will become neutral.
		const size_t team_num = size_t(side-1);

		const gamemap::location loc(cfg);

		if(game_map->is_village(loc)) {
			get_village(loc,*teams,team_num,*units);
		}
	}

	//command to remove a variable
	else if(cmd == "clear_variable") {
		state_of_game->variables.values.erase(cfg["name"]);
		state_of_game->variables.clear_children(cfg["name"]);
	}

	else if(cmd == "endlevel") {
		const std::string& next_scenario = cfg["next_scenario"];
		if(next_scenario.empty() == false) {
			state_of_game->scenario = next_scenario;
		}

		const std::string& result = cfg["result"];
		if(result.empty() || result == "victory") {
			const bool bonus = cfg["bonus"] == "yes";
			throw end_level_exception(VICTORY,bonus);
		} else if(result == "continue") {
			throw end_level_exception(LEVEL_CONTINUE);
		} else if(result == "continue_no_save") {
			throw end_level_exception(LEVEL_CONTINUE_NO_SAVE);
		} else {
			std::cerr << "throwing event defeat...\n";
			throw end_level_exception(DEFEAT);
		}
	}

	else if(cmd == "redraw") {
		screen->invalidate_all();
		screen->draw(true);
	}

	std::cerr << "done handling command...\n";

	return rval;
}

void event_handler::handle_event(const queued_event& event_info, const config* cfg)
{
	if(cfg == NULL)
		cfg = cfg_;

	bool skip_messages = false;
	for(config::all_children_iterator i = cfg->ordered_begin();
		i != cfg->ordered_end(); ++i) {
		const std::pair<const std::string*,const config*> item = *i;

		// If the user pressed escape, we skip any message that doesn't 
		// require them to make a choice.
		if ((skip_messages) && (*item.first == "message")) {
			if ((item.second)->get_children("option").size() == 0) {
				continue;
			}
		}

		if (!handle_event_command(event_info,*item.first,*item.second))  {
			skip_messages = true;
		}
		else { 
			skip_messages = false;
		}
	}
}

bool filter_loc_impl(const gamemap::location& loc, const std::string& xloc,
                                                   const std::string& yloc)
{
	if(std::find(xloc.begin(),xloc.end(),',') != xloc.end()) {
		std::vector<std::string> xlocs = config::split(xloc);
		std::vector<std::string> ylocs = config::split(yloc);

		const int size = xlocs.size() < ylocs.size()?xlocs.size():ylocs.size();
		for(int i = 0; i != size; ++i) {
			if(filter_loc_impl(loc,xlocs[i],ylocs[i]))
				return true;
		}

		return false;
	}

	if(!xloc.empty()) {
		const std::string::const_iterator dash =
		             std::find(xloc.begin(),xloc.end(),'-');

		if(dash != xloc.end()) {
			const std::string beg(xloc.begin(),dash);
			const std::string end(dash+1,xloc.end());

			const int bot = atoi(beg.c_str()) - 1;
			const int top = atoi(end.c_str()) - 1;

			if(loc.x < bot || loc.x > top)
				return false;
		} else {
			const int xval = atoi(xloc.c_str()) - 1;
			if(xval != loc.x)
				return false;
		}
	}

	if(!yloc.empty()) {
		const std::string::const_iterator dash =
		             std::find(yloc.begin(),yloc.end(),'-');

		if(dash != yloc.end()) {
			const std::string beg(yloc.begin(),dash);
			const std::string end(dash+1,yloc.end());

			const int bot = atoi(beg.c_str()) - 1;
			const int top = atoi(end.c_str()) - 1;

			if(loc.y < bot || loc.y > top)
				return false;
		} else {
			const int yval = atoi(yloc.c_str()) - 1;
			if(yval != loc.y)
				return false;
		}
	}

	return true;
}

bool filter_loc(const gamemap::location& loc, const config& cfg)
{
	//iterate over any [not] tags, and if any match, then the filter does not match
	const config::child_list& negatives = cfg.get_children("not");
	for(config::child_list::const_iterator i = negatives.begin(); i != negatives.end(); ++i) {
		if(filter_loc(loc,**i)) {
			return false;
		}
	}

	const std::string& xloc = cfg["x"];
	const std::string& yloc = cfg["y"];

	return filter_loc_impl(loc,xloc,yloc);
}

bool process_event(event_handler& handler, const queued_event& ev)
{
	if(handler.disabled())
		return false;

	unit_map::iterator unit1 = units->find(ev.loc1);
	unit_map::iterator unit2 = units->find(ev.loc2);

	const config::child_list& first_filters = handler.first_arg_filters();
	for(config::child_list::const_iterator ffi = first_filters.begin();
	    ffi != first_filters.end(); ++ffi) {

		if(unit1 == units->end() || !game_events::unit_matches_filter(unit1,**ffi)) {
			return false;
		}
	}

	const config::child_list& second_filters = handler.second_arg_filters();
	for(config::child_list::const_iterator sfi = second_filters.begin();
	    sfi != second_filters.end(); ++sfi) {
		if(unit2 == units->end() || !game_events::unit_matches_filter(unit2,**sfi)) {
			return false;
		}
	}

	//the event hasn't been filtered out, so execute the handler
	handler.handle_event(ev);

	if(handler.first_time_only()) {
		handler.disable();
	}

	return true;
}

void get_variable_internal(const std::string& key, config& cfg,
						   std::string** varout, config** cfgout)
{
	//we get the variable from the [variables] section of the game state. Variables may
	//be in the format 
	const std::string::const_iterator itor = std::find(key.begin(),key.end(),'.');
	if(itor != key.end()) {
		std::string element(key.begin(),itor);
		const std::string sub_key(itor+1,key.end());

		size_t index = 0;
		const std::string::iterator index_start = std::find(element.begin(),element.end(),'[');
		const bool explicit_index = index_start != element.end();

		if(explicit_index) {
			const std::string::iterator index_end = std::find(index_start,element.end(),']');
			const std::string index_str(index_start+1,index_end);
			index = size_t(atoi(index_str.c_str()));
			if(index > MaxLoop) {
				std::cerr << "ERROR: index greater than 1024: truncated\n";
				index = MaxLoop;
			}

			element = std::string(element.begin(),index_start);
		}

		const config::child_list& items = cfg.get_children(element);

		//special case -- '.length' on an array returns the size of the array
		if(explicit_index == false && sub_key == "length") {
			if(items.empty()) {
				if(varout != NULL) {
					static std::string zero_str;
					zero_str = "0";
					*varout = &zero_str;
				}
			} else {
				char buf[50];
				sprintf(buf,"%d",minimum<int>(MaxLoop,int(items.size())));
				((*items.back())["__length"] = buf);
				if(varout != NULL) {
					*varout = &(*items.back())["__length"];
				}
			}

			return;
		}
		
		while(cfg.get_children(element).size() <= index) {
			cfg.add_child(element);
		}

		if(cfgout != NULL) {
			*cfgout = cfg.get_children(element)[index];
		}
		
		get_variable_internal(sub_key,*cfg.get_children(element)[index],varout,cfgout);
	} else {
		if(varout != NULL) {
			*varout = &cfg[key];
		}
	}
}

} //end anonymous namespace

namespace game_events {

bool unit_matches_filter(unit_map::const_iterator itor, const config& filter)
{
	return filter_loc(itor->first,filter) && itor->second.matches_filter(filter);
}

std::string& get_variable(const std::string& key)
{
	if(state_of_game != NULL) {
		std::string* res = NULL;
		get_variable_internal(key,state_of_game->variables,&res,NULL);
		if(res != NULL) {
			return *res;
		}
	}
	
	static std::string empty_string;
	return empty_string;
}

const std::string& get_variable_const(const std::string& key)
{
	return get_variable(key);
}

config& get_variable_cfg(const std::string& key)
{
	if(state_of_game != NULL) {
		config* res = NULL;
		get_variable_internal(key + ".",state_of_game->variables,NULL,&res);
		if(res != NULL) {
			return *res;
		}
	}

	static config empty_cfg;
	return empty_cfg;
}

void set_variable(const std::string& key, const std::string& value)
{
	state_of_game->variables[key] = value;
}

manager::manager(config& cfg, display& gui_, gamemap& map_,
                 std::map<gamemap::location,unit>& units_,
                 std::vector<team>& teams_,
                 game_state& state_of_game_, gamestatus& status, game_data& game_data_)
{
	const config::child_list& events_list = cfg.get_children("event");
	for(config::child_list::const_iterator i = events_list.begin();
	    i != events_list.end(); ++i) {
		event_handler new_handler(*i);
		events_map.insert(std::pair<std::string,event_handler>(
		                                   new_handler.name(), new_handler));
	}

	teams = &teams_;
	screen = &gui_;
	game_map = &map_;
	units = &units_;
	state_of_game = &state_of_game_;
	game_data_ptr = &game_data_;
	status_ptr = &status;

	used_items.clear();
	const std::string& used = cfg["used_items"];
	if(!used.empty()) {
		const std::vector<std::string>& v = config::split(used);
		for(std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i) {
			used_items.insert(*i);
		}
	}
}

void write_events(config& cfg)
{
	for(std::multimap<std::string,event_handler>::const_iterator i = events_map.begin(); i != events_map.end(); ++i) {
		if(!i->second.disabled()) {
			i->second.write(cfg.add_child("event"));
		}
	}

	std::stringstream used;
	for(std::set<std::string>::const_iterator u = used_items.begin(); u != used_items.end(); ++u) {
		if(u != used_items.begin())
			used << ",";

		used << *u;
	}

	cfg["used_items"] = used.str();

	if(screen != NULL)
		screen->write_overlays(cfg);
}

manager::~manager() {
	events_queue.clear();
	events_map.clear();
	screen = NULL;
	game_map = NULL;
	units = NULL;
	state_of_game = NULL;
	game_data_ptr = NULL;
	status_ptr = NULL;
}

void raise(const std::string& event,
           const gamemap::location& loc1,
           const gamemap::location& loc2)
{
	if(!events_init())
		return;

	events_queue.push_back(queued_event(event,loc1,loc2));
}

bool fire(const std::string& event,
          const gamemap::location& loc1,
          const gamemap::location& loc2)
{
	raise(event,loc1,loc2);
	return pump();
}

bool pump()
{
	if(!events_init())
		return false;

	bool result = false;

	while(events_queue.empty() == false) {
		queued_event ev = events_queue.front();
		events_queue.pop_front(); //pop now for exception safety
		const std::string& event_name = ev.name;
		typedef std::multimap<std::string,event_handler>::iterator itor;

		//find all handlers for this event in the map
		std::pair<itor,itor> i = events_map.equal_range(event_name);

		//set the variables for the event
		if(i.first != i.second && state_of_game != NULL) {
			char buf[50];
			sprintf(buf,"%d",ev.loc1.x+1);
			state_of_game->variables["x1"] = buf;

			sprintf(buf,"%d",ev.loc1.y+1);
			state_of_game->variables["y1"] = buf;

			sprintf(buf,"%d",ev.loc2.x+1);
			state_of_game->variables["x2"] = buf;

			sprintf(buf,"%d",ev.loc2.y+1);
			state_of_game->variables["y2"] = buf;
		}

		while(i.first != i.second) {
			std::cerr << "processing event '" << event_name << "'\n";
			event_handler& handler = i.first->second;
			if(process_event(handler, ev))
				result = true;
			++i.first;
		}
	}

	return result;
}

} //end namespace game_events
