/*
   Copyright (C) 2006 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "dialogs.hpp"
#include "display.hpp"
#include "game_config.hpp"
#include "game_errors.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "help.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "sound.hpp"
#include "unit_display.hpp"
#include "unit_types.hpp"
#include "wassert.hpp"
#include "wml_separators.hpp"
#include "wesconfig.h"

#include <sstream>

namespace events{

	class delete_recall_unit : public gui::dialog_button_action
	{
	public:
		delete_recall_unit(display& disp, std::vector<unit>& units) : disp_(disp), units_(units) {}
	private:
		gui::dialog_button_action::RESULT button_pressed(int menu_selection);

		display& disp_;
		std::vector<unit>& units_;
	};

	gui::dialog_button_action::RESULT delete_recall_unit::button_pressed(int menu_selection)
	{
		const size_t index = size_t(menu_selection);
		if(index < units_.size()) {
			const unit& u = units_[index];

			//if the unit is of level > 1, or is close to advancing, we warn the player
			//about it
			std::string message = "";
			if(u.level() > 1) {
				message = _("My lord, this unit is an experienced one, having advanced levels! Do you really want to dismiss $noun?");
			} else if(u.experience() > u.max_experience()/2) {
				message = _("My lord, this unit is close to advancing a level! Do you really want to dismiss $noun?");
			}

			if(message != "") {
				utils::string_map symbols;
				symbols["noun"] = (u.gender() == unit_race::MALE ? _("him") : _("her"));
				message = utils::interpolate_variables_into_string(message, &symbols);

				const int res = gui::show_dialog(disp_,NULL,"",message,gui::YES_NO);
				if(res != 0) {
					return gui::dialog_button_action::NO_EFFECT;
				}
			}

			units_.erase(units_.begin() + index);
			recorder.add_disband(index);
			return gui::dialog_button_action::DELETE_ITEM;
		} else {
			return gui::dialog_button_action::NO_EFFECT;
		}
	}

	bool is_illegal_file_char(char c)
	{
		return c == '/' || c == '\\' || c == ':';
	}

	menu_handler::menu_handler(display* gui, unit_map& units, std::vector<team>& teams,
		const config& level, const game_data& gameinfo, const gamemap& map,
		const config& game_config, const gamestatus& status, game_state& gamestate,
		undo_list& undo_stack, undo_list& redo_stack) : 
	gui_(gui), units_(units), teams_(teams), level_(level), gameinfo_(gameinfo), map_(map),
		game_config_(game_config), status_(status), gamestate_(gamestate), undo_stack_(undo_stack),
		redo_stack_(redo_stack)
	{
	}

	menu_handler::~menu_handler()
	{
	}
	const undo_list& menu_handler::get_undo_list() const{
		 return undo_stack_;
	}

	gui::floating_textbox& menu_handler::get_textbox(){
		return textbox_info_;
	}

	void menu_handler::objectives(const unsigned int team_num)
	{
		dialogs::show_objectives(*gui_, level_, teams_[team_num - 1].objectives());
		teams_[team_num - 1].reset_objectives_changed();
	}

	void menu_handler::show_statistics()
	{
		const statistics::stats& stats = statistics::calculate_stats(0,gui_->viewing_team()+1);
		std::vector<std::string> items;

		{
			std::stringstream str;
			str << _("Recruits") << COLUMN_SEPARATOR << statistics::sum_str_int_map(stats.recruits);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Recalls") << COLUMN_SEPARATOR << statistics::sum_str_int_map(stats.recalls);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Advancements") << COLUMN_SEPARATOR << statistics::sum_str_int_map(stats.advanced_to);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Losses") << COLUMN_SEPARATOR << statistics::sum_str_int_map(stats.deaths);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Kills") << COLUMN_SEPARATOR << statistics::sum_str_int_map(stats.killed);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Damage Inflicted") << COLUMN_SEPARATOR << stats.damage_inflicted;
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Damage Taken") << COLUMN_SEPARATOR << stats.damage_taken;
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Damage Inflicted (EV)") << COLUMN_SEPARATOR
				<< (stats.expected_damage_inflicted / 100.0);
			items.push_back(str.str());
		}

		{
			std::stringstream str;
			str << _("Damage Taken (EV)") <<  COLUMN_SEPARATOR
				<< (stats.expected_damage_taken / 100.0);
			items.push_back(str.str());
		}

		for(;;) {
			const int res = gui::show_dialog2(*gui_, NULL, _("Statistics"), "", gui::OK_CANCEL, &items);
			std::string title;
			std::vector<std::string> items_sub;

			switch(res) {
			case 0:
				items_sub = create_unit_table(stats.recruits);
				title = _("Recruits");
				break;
			case 1:
				items_sub = create_unit_table(stats.recalls);
				title = _("Recalls");
				break;
			case 2:
				items_sub = create_unit_table(stats.advanced_to);
				title = _("Advancements");
				break;
			case 3:
				items_sub = create_unit_table(stats.deaths);
				title = _("Losses");
				break;
			case 4:
				items_sub = create_unit_table(stats.killed);
				title = _("Kills");
				break;
			default:
				return;
			}

			if (items_sub.empty() == false)
				gui::show_dialog2(*gui_, NULL, "", title, gui::OK_ONLY, &items_sub);
		}
	}

	std::vector<std::string> menu_handler::create_unit_table(const statistics::stats::str_int_map& m)
	{
		std::vector<std::string> table;
		for(statistics::stats::str_int_map::const_iterator i = m.begin(); i != m.end(); ++i) {
			const game_data::unit_type_map::const_iterator type = gameinfo_.unit_types.find(i->first);
			if(type == gameinfo_.unit_types.end()) {
				continue;
			}

			std::stringstream str;
			str << IMAGE_PREFIX << type->second.image() << COLUMN_SEPARATOR
				<< type->second.language_name() << COLUMN_SEPARATOR << i->second << "\n";
			table.push_back(str.str());
		}

		return table;
	}

	void menu_handler::unit_list()
	{
		const std::string heading = std::string(1,HEADING_PREFIX) +
									_("Type") + COLUMN_SEPARATOR +
									_("Name") + COLUMN_SEPARATOR +
									_("HP") + COLUMN_SEPARATOR +
									_("XP") + COLUMN_SEPARATOR +
									_("Traits") + COLUMN_SEPARATOR +
									_("Moves") + COLUMN_SEPARATOR +
									_("Location");

		gui::menu::basic_sorter sorter;
		sorter.set_alpha_sort(0).set_alpha_sort(1).set_numeric_sort(2).set_numeric_sort(3)
			  .set_alpha_sort(4).set_numeric_sort(5).set_numeric_sort(6);

		std::vector<std::string> items;
		items.push_back(heading);

		std::vector<gamemap::location> locations_list;
		std::vector<unit> units_list;
		for(unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i) {
			if(i->second.side() != (gui_->viewing_team()+1))
				continue;

			std::stringstream row;
			row << i->second.language_name() << COLUMN_SEPARATOR
				<< i->second.description() << COLUMN_SEPARATOR
				<< i->second.hitpoints() << "/" << i->second.max_hitpoints() << COLUMN_SEPARATOR
				<< i->second.experience() << "/";

			if(i->second.can_advance() == false)
				row << "-";
			else
				row << i->second.max_experience();

			row << COLUMN_SEPARATOR
				<< i->second.traits_description() << COLUMN_SEPARATOR
				<< i->second.movement_left() << "/"
				<< i->second.total_movement() << COLUMN_SEPARATOR
				<< i->first;

			items.push_back(row.str());

			locations_list.push_back(i->first);
			units_list.push_back(i->second);
		}

		int selected = 0;

		{
			dialogs::unit_preview_pane unit_preview(*gui_, &map_, units_list);
			std::vector<gui::preview_pane*> preview_panes;
			preview_panes.push_back(&unit_preview);

			selected = gui::show_dialog2(*gui_,NULL,_("Unit List"),"",
										gui::OK_ONLY,&items,&preview_panes,
										"",NULL,0,NULL,NULL,-1,-1,NULL,NULL,"",&sorter);
		}

		if(selected > 0 && selected < int(locations_list.size())) {
			const gamemap::location& loc = locations_list[selected];
			gui_->scroll_to_tile(loc.x,loc.y,display::WARP);
		}
	}

	void menu_handler::status_table()
	{
		std::stringstream heading;
		heading << HEADING_PREFIX << _("Leader") << COLUMN_SEPARATOR << ' ' << COLUMN_SEPARATOR
				<< _("Gold") << COLUMN_SEPARATOR
				<< _("Villages") << COLUMN_SEPARATOR
				<< _("Units") << COLUMN_SEPARATOR
				<< _("Upkeep") << COLUMN_SEPARATOR
				<< _("Income");

		gui::menu::basic_sorter sorter;
		sorter.set_redirect_sort(0,1).set_alpha_sort(1).set_numeric_sort(2).set_numeric_sort(3)
			  .set_numeric_sort(4).set_numeric_sort(5).set_numeric_sort(6).set_numeric_sort(7);

		if(game_config::debug)
			heading << COLUMN_SEPARATOR << _("Gold");

		std::vector<std::string> items;
		items.push_back(heading.str());

		const team& viewing_team = teams_[gui_->viewing_team()];

		//if the player is under shroud or fog, they don't get to see
		//details about the other sides, only their own side, allied sides and a ??? is
		//shown to demonstrate lack of information about the other sides
		bool fog = false;
		for(size_t n = 0; n != teams_.size(); ++n) {
			if(teams_[n].is_empty()) {
				continue;
			}

			const bool known = viewing_team.knows_about_team(n);
			const bool enemy = viewing_team.is_enemy(n+1);
			if(!known) {
				fog = true;
				continue;
			}

			const team_data data = calculate_team_data(teams_[n],n+1,units_);

			std::stringstream str;

			const unit_map::const_iterator leader = team_leader(n+1,units_);
			//output the number of the side first, and this will
			//cause it to be displayed in the correct colour
			if(leader != units_.end()) {
				str << IMAGE_PREFIX << leader->second.absolute_image() << COLUMN_SEPARATOR
					<< "\033[3" << lexical_cast<char, size_t>(n+1) << 'm' << leader->second.description() << COLUMN_SEPARATOR;
			} else {
				str << ' ' << COLUMN_SEPARATOR << "\033[3" << lexical_cast<char, size_t>(n+1) << "m-" << COLUMN_SEPARATOR;
			}

			if(enemy && viewing_team.uses_fog()) {
				str << ' ' << COLUMN_SEPARATOR;
			} else {
				str << data.gold << COLUMN_SEPARATOR;
			}
			str << data.villages << COLUMN_SEPARATOR
				<< data.units << COLUMN_SEPARATOR << data.upkeep << COLUMN_SEPARATOR
				<< (data.net_income < 0 ? font::BAD_TEXT : font::NULL_MARKUP) << data.net_income;

			if(game_config::debug)
				str << COLUMN_SEPARATOR << teams_[n].gold();

			items.push_back(str.str());
		}

		if(fog)
			items.push_back(IMAGE_PREFIX + std::string("random-enemy.png") + COLUMN_SEPARATOR +
							IMAGE_PREFIX + "random-enemy.png");

		gui::show_dialog2(*gui_,NULL,"","",gui::CLOSE_ONLY,&items,
						 NULL,"",NULL,0,NULL,NULL,-1,-1,NULL,NULL,"",&sorter);
	}

	void menu_handler::save_game(const std::string& message, gui::DIALOG_TYPE dialog_type)
	{
		std::stringstream stream;

		const std::string ellipsed_name = font::make_text_ellipsis(gamestate_.label,
				font::SIZE_NORMAL, 200);
		stream << ellipsed_name << " " << _("Turn")
			   << " " << status_.turn();
		std::string label = stream.str();
		if(dialog_type == gui::NULL_DIALOG && message != "") {
			label = message;
		}

		label.erase(std::remove_if(label.begin(),label.end(),is_illegal_file_char),label.end());

		const int res = dialog_type == gui::NULL_DIALOG ? 0 : dialogs::get_save_name(*gui_,message,_("Name:"),&label,dialog_type);

		if(res == 0) {

			if(std::count_if(label.begin(),label.end(),is_illegal_file_char)) {
				gui::show_dialog(*gui_,NULL,_("Error"),_("Save names may not contain colons, slashes, or backslashes. Please choose a different name."),gui::OK_ONLY);
				save_game(message,dialog_type);
				return;
			}

			config snapshot;
			write_game_snapshot(snapshot);
			try {
				recorder.save_game(label, snapshot, gamestate_.starting_pos);
				if(dialog_type != gui::NULL_DIALOG) {
					gui::show_dialog(*gui_,NULL,_("Saved"),_("The game has been saved"), gui::OK_ONLY);
				}
			} catch(game::save_game_failed&) {
				gui::show_dialog(*gui_,NULL,_("Error"),_("The game could not be saved"),gui::MESSAGE);
				//do not bother retrying, since the user can just try to save the game again
			};
		}
	}

	void menu_handler::write_game_snapshot(config& start) const
	{
		start.values = level_.values;

		start["snapshot"] = "yes";

		std::stringstream buf;
		buf << gui_->playing_team();
		start["playing_team"] = buf.str();

		for(std::vector<team>::const_iterator t = teams_.begin(); t != teams_.end(); ++t) {
			const unsigned int side_num = t - teams_.begin() + 1;

			config& side = start.add_child("side");
			t->write(side);
			side["no_leader"] = "yes";
			buf.str(std::string());
			buf << side_num;
			side["side"] = buf.str();

			for(unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i) {
				if(i->second.side() == side_num) {
					config& u = side.add_child("unit");
					i->first.write(u);
					i->second.write(u);
				}
			}
		}

		status_.write(start);
		game_events::write_events(start);

		// Write terrain_graphics data in snapshot, too
		const config::child_list& terrains = level_.get_children("terrain_graphics");
		for(config::child_list::const_iterator tg = terrains.begin();
				tg != terrains.end(); ++tg) {

			start.add_child("terrain_graphics", **tg);
		}

		sound::write_music_play_list(start);

		write_game(gamestate_, start /*,WRITE_SNAPSHOT_ONLY*/);

		// Clobber gold values to make sure the snapshot uses the values
		// in [side] instead.
		const config::child_list& players=start.get_children("player");
		for(config::child_list::const_iterator pi=players.begin();
			pi!=players.end(); ++pi) {
			(**pi)["gold"] = "-1000000";
		}

		//write out the current state of the map
		start["map_data"] = map_.write();

		gui_->labels().write(start);
	}

	void menu_handler::autosave(unsigned turn, const config &starting_pos) const
	{
		// We save at the start of every third turn.
		if (turn % 3 == 1) {
			config snapshot;
			write_game_snapshot(snapshot);
			try {
				recorder.save_game(_("Auto-Save") + lexical_cast<std::string>(turn), snapshot, starting_pos);
			} catch(game::save_game_failed&) {
				gui::show_dialog(*gui_,NULL,"",_("Could not auto save the game. Please save the game manually."),gui::MESSAGE);
				//do not bother retrying, since the user can just save the game
			}
		}
	}

	void menu_handler::load_game(){
		bool show_replay = false;
		const std::string game = dialogs::load_game_dialog(*gui_, game_config_, gameinfo_, &show_replay);
		if(game != "") {
			throw game::load_game_exception(game,show_replay);
		}
	}

	void menu_handler::preferences()
	{
		preferences::show_preferences_dialog(*gui_, game_config_);
		gui_->redraw_everything();
	}

	void menu_handler::show_chat_log()
	{
		std::string text = recorder.build_chat_log(teams_[gui_->viewing_team()].team_name());
		gui::show_dialog(*gui_,NULL,_("Chat Log"),"",gui::CLOSE_ONLY,NULL,NULL,"",&text);
	}

	void menu_handler::show_help()
	{
		help::show_help(*gui_);
	}

	void menu_handler::speak()
	{
		textbox_info_.show(gui::TEXTBOX_MESSAGE,_("Message:"), 
			has_friends() ? _("Send to allies only") : "", preferences::message_private(), *gui_);
	}

	bool menu_handler::has_friends() const
	{
		if(is_observer()) {
			return false;
		}

		for(size_t n = 0; n != teams_.size(); ++n) {
			if(n != gui_->viewing_team() && teams_[gui_->viewing_team()].team_name() == teams_[n].team_name() && teams_[n].is_network()) {
				return true;
			}
		}

		return false;
	}

	void menu_handler::recruit(const bool browse, const unsigned int team_num, const gamemap::location& last_hex)
	{
		if(browse)
			return;

		team& current_team = teams_[team_num-1];

		std::vector<unit> sample_units;

		gui_->draw(); //clear the old menu
		std::vector<std::string> item_keys;
		std::vector<std::string> items;
		const std::set<std::string>& recruits = current_team.recruits();
		for(std::set<std::string>::const_iterator it = recruits.begin(); it != recruits.end(); ++it) {
			const std::map<std::string,unit_type>::const_iterator
					u_type = gameinfo_.unit_types.find(*it);
			if(u_type == gameinfo_.unit_types.end()) {
				LOG_STREAM(err, engine) << "could not find unit '" << *it << "'\n";
				return;
			}

			item_keys.push_back(*it);

			const unit_type& type = u_type->second;

			//display units that we can't afford to recruit in red
			const char prefix = (type.cost() > current_team.gold() ? font::BAD_TEXT : font::NULL_MARKUP);

			std::stringstream description;

			description << font::IMAGE << type.image() << COLUMN_SEPARATOR << font::LARGE_TEXT
						<< prefix << type.language_name() << "\n"
						<< prefix << type.cost() << " " << sgettext("unit^Gold");
			items.push_back(description.str());
			sample_units.push_back(unit(&gameinfo_,&units_,&map_,&status_,&teams_,&type,team_num));
		}

		if(sample_units.empty()) {
			gui::show_dialog(*gui_,NULL,"",_("You have no units available to recruit."),gui::MESSAGE);
			return;
		}

		int recruit_res = 0;

		{
			dialogs::unit_preview_pane unit_preview(*gui_,&map_,sample_units);
			std::vector<gui::preview_pane*> preview_panes;
			preview_panes.push_back(&unit_preview);

			recruit_res = gui::show_dialog2(*gui_,NULL,_("Recruit"),
					_("Select unit:") + std::string("\n"),
					gui::OK_CANCEL,&items,&preview_panes,"",NULL,-1,NULL,NULL,-1,-1,
					NULL,NULL,"recruit_and_recall");
		}

		if(recruit_res != -1) {
			do_recruit(item_keys[recruit_res], team_num, last_hex);
		}
	}

	void menu_handler::repeat_recruit(const unsigned int team_num, const gamemap::location& last_hex)
	{
		if(last_recruit_.empty() == false)
			do_recruit(last_recruit_, team_num, last_hex);
	}

	void menu_handler::do_recruit(const std::string& name, const int unsigned team_num, const gamemap::location& last_hex)
	{
		team& current_team = teams_[team_num-1];

		//search for the unit to be recruited in recruits
		int recruit_num = 0;
		const std::set<std::string>& recruits = current_team.recruits();
		for(std::set<std::string>::const_iterator r = recruits.begin(); ; ++r) {
			if (r == recruits.end()) {
				return;
			}

			if (name == *r) {
				break;
			}
			++recruit_num;
		}

		const std::map<std::string,unit_type>::const_iterator
				u_type = gameinfo_.unit_types.find(name);
		wassert(u_type != gameinfo_.unit_types.end());

		if(u_type->second.cost() > current_team.gold()) {
			gui::show_dialog(*gui_,NULL,"",
				 _("You don't have enough gold to recruit that unit"),gui::OK_ONLY);
		} else {
			last_recruit_ = name;

			//create a unit with traits
			recorder.add_recruit(recruit_num, last_hex);
			unit new_unit(&gameinfo_,&units_,&map_,&status_,&teams_,&(u_type->second),team_num,true);
			gamemap::location loc = last_hex;
			const std::string& msg = recruit_unit(map_,team_num,units_,new_unit,loc,gui_);
			if(msg.empty()) {
				current_team.spend_gold(u_type->second.cost());
				statistics::recruit_unit(new_unit);

				//MP_COUNTDOWN grant time bonus for recruiting
				current_team.set_action_bonus_count(1 + current_team.action_bonus_count());

				redo_stack_.clear();
				if(new_unit.type().genders().size() > 1 || new_unit.type().has_random_traits()) {
					clear_undo_stack(team_num);
				} else {
					undo_stack_.push_back(undo_action(new_unit,loc,RECRUIT_POS));
				}

				clear_shroud(team_num);

				gui_->recalculate_minimap();
				gui_->invalidate_game_status();
				gui_->invalidate_all();
			} else {
				recorder.undo();
				gui::show_dialog(*gui_,NULL,"",msg,gui::OK_ONLY);
			}
		}
	}

	void menu_handler::recall(const unsigned int team_num, const gamemap::location& last_hex)
	{
		player_info *player = gamestate_.get_player(teams_[team_num-1].save_id());
		if(!player) {
			LOG_STREAM(err, engine) << "cannot recall a unit for side " << team_num
				<< ", which has no recall list!\n";
			return;
		}

		team& current_team = teams_[team_num-1];
		std::vector<unit>& recall_list = player->available_units;

		//sort the available units into order by value
		//so that the most valuable units are shown first
		sort_units(recall_list);

		gui_->draw(); //clear the old menu

		if(utils::string_bool(level_["disallow_recall"])) {
			gui::show_dialog(*gui_,NULL,"",_("You are separated from your soldiers and may not recall them"));
		} else if(recall_list.empty()) {
			gui::show_dialog(*gui_,NULL,"",_("There are no troops available to recall\n(You must have veteran survivors from a previous scenario)"));
		} else {
			std::vector<std::string> options;

			std::ostringstream heading;
			heading << HEADING_PREFIX << COLUMN_SEPARATOR << _("Type")
					<< COLUMN_SEPARATOR << _("Name")
					<< COLUMN_SEPARATOR << _("Level")
					<< COLUMN_SEPARATOR << _("XP");

			gui::menu::basic_sorter sorter;
			sorter.set_alpha_sort(1).set_alpha_sort(2).set_id_sort(3).set_numeric_sort(4);

			options.push_back(heading.str());

			for(std::vector<unit>::const_iterator u = recall_list.begin(); u != recall_list.end(); ++u) {
				std::stringstream option;
				const std::string& description = u->description().empty() ? "-" : u->description();
				option << IMAGE_PREFIX << u->absolute_image() << COLUMN_SEPARATOR
						<< u->language_name() << COLUMN_SEPARATOR
						<< description << COLUMN_SEPARATOR
						<< u->level() << COLUMN_SEPARATOR
						<< u->experience() << "/";

				if(u->can_advance() == false) {
					option << "-";
				} else {
					option << u->max_experience();
				}

				options.push_back(option.str());
			}

			delete_recall_unit recall_deleter(*gui_,recall_list);
			gui::dialog_button delete_button(&recall_deleter,_("Dismiss Unit"));
			std::vector<gui::dialog_button> buttons;
			buttons.push_back(delete_button);

			int res = 0;

			{
				dialogs::unit_preview_pane unit_preview(*gui_,&map_,recall_list);
				std::vector<gui::preview_pane*> preview_panes;
				preview_panes.push_back(&unit_preview);

				res = gui::show_dialog2(*gui_,NULL,_("Recall"),
						_("Select unit:") + std::string("\n"),
						gui::OK_CANCEL,&options,
						&preview_panes,"",NULL,-1,
						NULL,NULL,-1,-1,NULL,&buttons,"",&sorter);
			}

			if(res >= 0) {
				if(current_team.gold() < game_config::recall_cost) {
					std::stringstream msg;
					utils::string_map i18n_symbols;
					i18n_symbols["cost"] = lexical_cast<std::string>(game_config::recall_cost);
					msg << vgettext("You must have at least $cost gold pieces to recall a unit", i18n_symbols);
					gui::show_dialog(*gui_,NULL,"",msg.str());
				} else {
					std::cerr << "recall index: " << res << "\n";
					unit& un = recall_list[res];
					gamemap::location loc = last_hex;
					recorder.add_recall(res,loc);
					un.set_game_context(&gameinfo_,&units_,&map_,&status_,&teams_);
					const std::string err = recruit_unit(map_,team_num,units_,un,loc,gui_);
					if(!err.empty()) {
						recorder.undo();
						gui::show_dialog(*gui_,NULL,"",err,gui::OK_ONLY);
					} else {
						statistics::recall_unit(un);
						current_team.spend_gold(game_config::recall_cost);

						undo_stack_.push_back(undo_action(un,loc,res));
						redo_stack_.clear();

						clear_shroud(team_num);

						recall_list.erase(recall_list.begin()+res);
						gui_->invalidate_game_status();
						gui_->invalidate_all();
					}
				}
			}
		}
	}
	void menu_handler::undo(const unsigned int team_num, mouse_handler& mousehandler)
	{
		if(undo_stack_.empty())
			return;

		const events::command_disabler disable_commands;

		undo_action& action = undo_stack_.back();
		if(action.is_recall()) {
			player_info* const player = gamestate_.get_player(teams_[team_num - 1].save_id());

			if(player == NULL) {
				LOG_STREAM(err, engine) << "trying to undo a recall for side " << team_num
					<< ", which has no recall list!\n";
			} else {
				// Undo a recall action
				if(units_.count(action.recall_loc) == 0) {
					return;
				}

				const unit& un = units_.find(action.recall_loc)->second;
				statistics::un_recall_unit(un);
				teams_[team_num - 1].spend_gold(-game_config::recall_cost);

				std::vector<unit>& recall_list = player->available_units;
				recall_list.insert(recall_list.begin()+action.recall_pos,un);
				units_.erase(action.recall_loc);
				gui_->invalidate(action.recall_loc);
				gui_->draw();
			}
		} else if(action.is_recruit()) {
			// Undo a recruit action
			team& current_team = teams_[team_num-1];
			if(units_.count(action.recall_loc) == 0) {
				return;
			}

			const unit& un = units_.find(action.recall_loc)->second;
			statistics::un_recruit_unit(un);
			current_team.spend_gold(-un.type().cost());

			//MP_COUNTDOWN take away recruit bonus
			if(action.countdown_time_bonus)
			{
				teams_[team_num-1].set_action_bonus_count(teams_[team_num-1].action_bonus_count() - 1);
			}

			units_.erase(action.recall_loc);
			gui_->invalidate(action.recall_loc);
			gui_->draw();
		} else {
			// Undo a move action
			const int starting_moves = action.starting_moves;
			std::vector<gamemap::location> route = action.route;
			std::reverse(route.begin(),route.end());
			const unit_map::iterator u = units_.find(route.front());
			if(u == units_.end()) {
				//this can actually happen if the scenario designer has abused the [allow_undo] command
				LOG_STREAM(err, engine) << "Illegal 'undo' found. Possible abuse of [allow_undo]?\n";
				return;
			}

			if(map_.is_village(route.front())) {
				get_village(route.front(),teams_,action.original_village_owner,units_);
				//MP_COUNTDOWN take away capture bonus
				if(action.countdown_time_bonus)
				{
					teams_[team_num-1].set_action_bonus_count(teams_[team_num-1].action_bonus_count() - 1);
				}
			}

			action.starting_moves = u->second.movement_left();

			std::pair<gamemap::location,unit> *up = units_.extract(u->first);
			unit_display::move_unit(*gui_,map_,route,up->second,units_,teams_);
			up->second.set_goto(gamemap::location());
			up->second.set_movement(starting_moves);
			up->first = route.back();
			units_.add(up);
			gui_->invalidate(route.back());
			gui_->draw();
		}

		gui_->invalidate_unit();
		gui_->invalidate_game_status();

		redo_stack_.push_back(action);
		undo_stack_.pop_back();

		mousehandler.set_selected_hex(gamemap::location());
		mousehandler.set_current_paths(paths());

		recorder.undo();

		const bool shroud_cleared = clear_shroud(team_num);

		if(shroud_cleared) {
			gui_->recalculate_minimap();
		} else {
			gui_->redraw_minimap();
		}
	}

	void menu_handler::redo(const unsigned int team_num, mouse_handler& mousehandler)
	{
		if(redo_stack_.empty())
			return;

		const events::command_disabler disable_commands;

		//clear routes, selected hex, etc
		mousehandler.set_selected_hex(gamemap::location());
		mousehandler.set_current_paths(paths());

		undo_action& action = redo_stack_.back();
		if(action.is_recall()) {
			player_info *player=gamestate_.get_player(teams_[team_num - 1].save_id());
			if(!player) {
				LOG_STREAM(err, engine) << "trying to redo a recall for side " << team_num
					<< ", which has no recall list!\n";
			} else {
				// Redo recall
				std::vector<unit>& recall_list = player->available_units;
				unit un = recall_list[action.recall_pos];

				recorder.add_recall(action.recall_pos,action.recall_loc);
				un.set_game_context(&gameinfo_,&units_,&map_,&status_,&teams_);
				const std::string& msg = recruit_unit(map_,team_num,units_,un,action.recall_loc,gui_);
				if(msg.empty()) {
					statistics::recall_unit(un);
					teams_[team_num - 1].spend_gold(game_config::recall_cost);
					recall_list.erase(recall_list.begin()+action.recall_pos);

					gui_->invalidate(action.recall_loc);
					gui_->draw();
				} else {
					recorder.undo();
					gui::show_dialog(*gui_,NULL,"",msg,gui::OK_ONLY);
				}
			}
		} else if(action.is_recruit()) {
			// Redo recruit action
			team& current_team = teams_[team_num-1];
			gamemap::location loc = action.recall_loc;
			const std::string name = action.affected_unit.id();

			//search for the unit to be recruited in recruits
			int recruit_num = 0;
			const std::set<std::string>& recruits = current_team.recruits();
			for(std::set<std::string>::const_iterator r = recruits.begin(); ; ++r) {
				if (r == recruits.end()) {
					LOG_STREAM(err, engine) << "trying to redo a recruit for side " << team_num
						<< ", which does not recruit type \"" << name << "\"\n";
					wassert(0);
				}
				if (name == *r) {
					break;
				}
				++recruit_num;
			}
			last_recruit_ = name;
			recorder.add_recruit(recruit_num,loc);
			unit new_unit = action.affected_unit;
			//unit new_unit(action.affected_unit.type(),team_num_,true);
			const std::string& msg = recruit_unit(map_,team_num,units_,new_unit,loc,gui_);
			if(msg.empty()) {
				current_team.spend_gold(new_unit.type().cost());
				statistics::recruit_unit(new_unit);

				//MP_COUNTDOWN: restore recruitment bonus
				current_team.set_action_bonus_count(1 + current_team.action_bonus_count());

				gui_->invalidate(action.recall_loc);
				gui_->draw();
				//gui_.invalidate_game_status();
				//gui_.invalidate_all();
			} else {
				recorder.undo();
				gui::show_dialog(*gui_,NULL,"",msg,gui::OK_ONLY);
			}
		} else {
			// Redo movement action
			const int starting_moves = action.starting_moves;
			std::vector<gamemap::location> route = action.route;
			const unit_map::iterator u = units_.find(route.front());
			if(u == units_.end()) {
				wassert(false);
				return;
			}

			action.starting_moves = u->second.movement_left();

			std::pair<gamemap::location,unit> *up = units_.extract(u->first);
			unit_display::move_unit(*gui_,map_,route,up->second,units_,teams_);
			up->second.set_goto(gamemap::location());
			up->second.set_movement(starting_moves);
			up->first = route.back();
			units_.add(up);

			if(map_.is_village(route.back())) {
				get_village(route.back(),teams_,up->second.side()-1,units_);
				//MP_COUNTDOWN restore capture bonus
				if(action.countdown_time_bonus)
				{
					teams_[team_num-1].set_action_bonus_count(1 + teams_[team_num-1].action_bonus_count());
				}
			}

			gui_->invalidate(route.back());
			gui_->draw();

			recorder.add_movement(route.front(),route.back());
		}
		gui_->invalidate_unit();
		gui_->invalidate_game_status();

		undo_stack_.push_back(action);
		redo_stack_.pop_back();
	}

	bool menu_handler::clear_shroud(const unsigned int team_num)
	{
		bool cleared = teams_[team_num - 1].auto_shroud_updates() &&
			::clear_shroud(*gui_,status_,map_,gameinfo_,units_,teams_,team_num-1);
		return cleared;
	}

	void menu_handler::clear_undo_stack(const unsigned int team_num)
	{
		if(teams_[team_num - 1].auto_shroud_updates() == false)
			apply_shroud_changes(undo_stack_,gui_,status_,map_,gameinfo_,units_,teams_,team_num-1);
		undo_stack_.clear();
	}

	// Highlights squares that an enemy could move to on their turn, showing how many can reach each square.
	void menu_handler::show_enemy_moves(bool ignore_units, const unsigned int team_num)
	{
		gui_->unhighlight_reach();

		// Compute enemy movement positions
		for(unit_map::iterator u = units_.begin(); u != units_.end(); ++u) {
			bool invisible = u->second.invisible(u->first, units_, teams_);

			if(teams_[team_num - 1].is_enemy(u->second.side()) && !gui_->fogged(u->first.x,u->first.y) && !u->second.incapacitated() && !invisible) {
				const unit_movement_resetter move_reset(u->second);
				const bool is_skirmisher = u->second.get_ability_bool("skirmisher",u->first);
				const bool teleports = u->second.get_ability_bool("teleport",u->first);
				unit_map units(u->first, u->second);
				const paths& path = paths(map_,status_,gameinfo_,ignore_units?units:units_,
										  u->first,teams_,is_skirmisher,teleports,teams_[gui_->viewing_team()]);

				gui_->highlight_another_reach(path);
			}
		}
	}

	void menu_handler::toggle_shroud_updates(const unsigned int team_num) {
		bool auto_shroud = teams_[team_num - 1].auto_shroud_updates();
		// If we're turning automatic shroud updates on, then commit all moves
		if(auto_shroud == false) update_shroud_now(team_num);
		teams_[team_num - 1].set_auto_shroud_updates(!auto_shroud);
	}

	void menu_handler::update_shroud_now(const unsigned int team_num)
	{
		clear_undo_stack(team_num);
	}

	bool menu_handler::end_turn(const unsigned int team_num)
	{
		bool unmoved_units = false, partmoved_units = false, some_units_have_moved = false;
		for(unit_map::const_iterator un = units_.begin(); un != units_.end(); ++un) {
			if(un->second.side() == team_num) {
				if(unit_can_move(un->first,units_,map_,teams_)) {
					if(!un->second.has_moved()) {
						unmoved_units = true;
					}

					partmoved_units = true;
				}
				if(un->second.has_moved()) {
					some_units_have_moved = true;
				}
			}
		}

		//Ask for confirmation if the player hasn't made any moves (other than gotos).
		if(preferences::confirm_no_moves() && ! some_units_have_moved) {
			const int res = gui::show_dialog(*gui_,NULL,"",_("You have not started your turn yet. Do you really want to end your turn?"), gui::YES_NO);
			if(res != 0) {
				return false;
			}
		}

		// Ask for confirmation if units still have movement left
		if(preferences::yellow_confirm() && partmoved_units) {
			const int res = gui::show_dialog(*gui_,NULL,"",_("Some units have movement left. Do you really want to end your turn?"),gui::YES_NO);
			if (res != 0) {
				return false;
			}
		} else if (preferences::green_confirm() && unmoved_units) {
			const int res = gui::show_dialog(*gui_,NULL,"",_("Some units have movement left. Do you really want to end your turn?"),gui::YES_NO);
			if (res != 0) {
				return false;
			}
		}

		//force any pending fog updates
		clear_undo_stack(team_num);
		gui_->set_route(NULL);

		recorder.end_turn();

		return true;
	}

	void menu_handler::goto_leader(const unsigned int team_num)
	{
		const unit_map::const_iterator i = team_leader(team_num,units_);
		if(i != units_.end()) {
			clear_shroud(team_num);
			gui_->scroll_to_tile(i->first.x,i->first.y,display::WARP);
		}
	}

	void menu_handler::unit_description(mouse_handler& mousehandler)
	{
		const unit_map::const_iterator un = current_unit(mousehandler);
		if(un != units_.end()) {
			dialogs::show_unit_description(*gui_, un->second);
		}
	}

	void menu_handler::rename_unit(mouse_handler& mousehandler)
	{
		const unit_map::iterator un = current_unit(mousehandler);
		if(un == units_.end() || gui_->viewing_team()+1 != un->second.side())
			return;
		if(un->second.unrenamable())
			return;

		std::string name = un->second.description();
		const int res = gui::show_dialog(*gui_,NULL,_("Rename Unit"),"", gui::OK_CANCEL,NULL,NULL,"",&name);
		if(res == 0) {
			recorder.add_rename(name, un->first);
			un->second.rename(name);
			gui_->invalidate_unit();
		}
	}

	unit_map::iterator menu_handler::current_unit(mouse_handler& mousehandler)
	{
		unit_map::iterator res = find_visible_unit(units_, mousehandler.get_last_hex(),
			map_, teams_, teams_[gui_->viewing_team()]);
		if(res != units_.end()) {
			return res;
		} else {
			return find_visible_unit(units_, mousehandler.get_selected_hex(),
			map_, teams_, teams_[gui_->viewing_team()]);
		}
	}

	unit_map::const_iterator menu_handler::current_unit(const mouse_handler& mousehandler) const
	{
		unit_map::const_iterator res = find_visible_unit(units_, mousehandler.get_last_hex(),
			map_, teams_, teams_[gui_->viewing_team()]);
		if(res != units_.end()) {
			return res;
		} else {
			return find_visible_unit(units_, mousehandler.get_selected_hex(),
			map_, teams_, teams_[gui_->viewing_team()]);
		}
	}

	void menu_handler::create_unit(mouse_handler& mousehandler)
	{
		std::vector<std::string> options;
		std::vector<unit> unit_choices;
		for(game_data::unit_type_map::const_iterator i = gameinfo_.unit_types.begin(); i != gameinfo_.unit_types.end(); ++i) {
			options.push_back(i->second.language_name());
			unit_choices.push_back(unit(&gameinfo_,&units_,&map_,&status_,&teams_,&i->second,1,false));
			unit_choices.back().new_turn();
		}

		int choice = 0;

		{
			dialogs::unit_preview_pane unit_preview(*gui_,&map_,unit_choices);
			std::vector<gui::preview_pane*> preview_panes;
			preview_panes.push_back(&unit_preview);

			choice = gui::show_dialog2(*gui_,NULL,"",dsgettext(PACKAGE "-lib","Create Unit (Debug!)"),
									  gui::OK_CANCEL,&options,&preview_panes);
		}

		if (size_t(choice) < unit_choices.size()) {
			units_.erase(mousehandler.get_last_hex());
			units_.add(new std::pair<gamemap::location,unit>(mousehandler.get_last_hex(),unit_choices[choice]));
			gui_->invalidate(mousehandler.get_last_hex());
			gui_->invalidate_unit();
		}
	}

	void menu_handler::change_unit_side(mouse_handler& mousehandler)
	{
		const unit_map::iterator i = units_.find(mousehandler.get_last_hex());
		if(i == units_.end()) {
			return;
		}

		int side = i->second.side();
		++side;
		if(side > team::nteams()) {
			side = 1;
		}

		i->second.set_side(side);
	}

	void menu_handler::label_terrain(mouse_handler& mousehandler)
	{
		if(map_.on_board(mousehandler.get_last_hex()) == false) {
			return;
		}

		std::string label = gui_->labels().get_label(mousehandler.get_last_hex());
		const int res = gui::show_dialog(*gui_,NULL,_("Place Label"),"",gui::OK_CANCEL,
										 NULL,NULL,_("Label:"),&label,
						 map_labels::get_max_chars());
		if(res == 0) {
			gui_->labels().set_label(mousehandler.get_last_hex(),label);
			recorder.add_label(label,mousehandler.get_last_hex());
		}
	}

	void menu_handler::clear_labels()
	{
		gui_->labels().clear();
		recorder.clear_labels();
	}

	void menu_handler::continue_move(mouse_handler& mousehandler, const unsigned int team_num)
	{
		unit_map::iterator i = current_unit(mousehandler);
		if(i == units_.end() || i->second.move_interrupted() == false) {
			i = units_.find(mousehandler.get_selected_hex());
			if (i == units_.end() || i->second.move_interrupted() == false) return;
		}
		move_unit_to_loc(i,i->second.get_interrupted_move(),true, team_num, mousehandler);
	}

	void menu_handler::move_unit_to_loc(const unit_map::const_iterator& ui, const gamemap::location& target, bool continue_move, const unsigned int team_num, mouse_handler& mousehandler)
	{
		wassert(ui != units_.end());

		unit u = ui->second;
		const shortest_path_calculator calc(u,teams_[team_num - 1],mousehandler.visible_units(),teams_,map_);

		const std::set<gamemap::location>* teleports = NULL;

		std::set<gamemap::location> allowed_teleports;
		if(u.get_ability_bool("teleport",ui->first)) {
			allowed_teleports = vacant_villages(teams_[team_num - 1].villages(),units_);
			teleports = &allowed_teleports;
			if(teams_[team_num - 1].villages().count(ui->first))
				allowed_teleports.insert(ui->first);
		}

		paths::route route = a_star_search(ui->first, target, 10000.0, &calc, map_.x(), map_.y(), teleports);
		if(route.steps.empty())
			return;

		wassert(route.steps.front() == ui->first);

		route.move_left = route_turns_to_complete(ui->second,map_,route);
		gui_->set_route(&route);
		move_unit(gui_,gameinfo_,status_,map_,units_,teams_,route.steps,&recorder,&undo_stack_,NULL,continue_move);
		gui_->invalidate_game_status();
	}

	void menu_handler::toggle_grid()
	{
		preferences::set_grid(!preferences::grid());
		gui_->invalidate_all();
	}

	void menu_handler::unit_hold_position(mouse_handler& mousehandler, const unsigned int team_num)
	{
		const unit_map::iterator un = units_.find(mousehandler.get_selected_hex());
		if(un != units_.end() && un->second.side() == team_num && un->second.movement_left() >= 0) {
			un->second.set_hold_position(!un->second.hold_position());
			gui_->invalidate(mousehandler.get_selected_hex());

			mousehandler.set_current_paths(paths());
			gui_->draw();

			if(un->second.hold_position()) {
				un->second.set_user_end_turn(true);
				mousehandler.cycle_units();
			}
		}
	}

	void menu_handler::end_unit_turn(mouse_handler& mousehandler, const unsigned int team_num)
	{
		const unit_map::iterator un = units_.find(mousehandler.get_selected_hex());
		if(un != units_.end() && un->second.side() == team_num && un->second.movement_left() >= 0) {
			un->second.set_user_end_turn(!un->second.user_end_turn());
			if(un->second.hold_position() && !un->second.user_end_turn()){
			  un->second.set_hold_position(false);
			}
			gui_->invalidate(mousehandler.get_selected_hex());

			mousehandler.set_current_paths(paths());
			gui_->draw();

			if(un->second.user_end_turn()) {
				mousehandler.cycle_units();
			}
		}
	}

	void menu_handler::search()
	{
		std::ostringstream msg;
		msg << _("Search");
		if(last_search_hit_.valid()) {
			msg << " [" << last_search_ << "]";
		}
		msg << ':';
		textbox_info_.show(gui::TEXTBOX_SEARCH,msg.str(), "", false, *gui_);
	}

	void menu_handler::do_speak(){
		//None of the two parameters really needs to be passed since the informations belong to members of the class.
		//But since it makes the called method more generic, it is done anyway.
		chat_handler::do_speak(textbox_info_.box()->text(),textbox_info_.check() != NULL ? textbox_info_.check()->checked() : false);
	}

	void menu_handler::add_chat_message(const std::string& speaker, int side, const std::string& message, display::MESSAGE_TYPE type)
	{
		gui_->add_chat_message(speaker,side,message,type);
	}

	chat_handler::~chat_handler()
	{
	}

	void chat_handler::do_speak(const std::string& message, bool allies_only)
	{
		if(message == "") {
			return;
		}

		static const std::string query = "/query";
		static const std::string whisper = "/whisper";
		static const std::string whisper2 = "/msg";
		static const std::string ignore = "/ignore";
		static const std::string help = "/help";
		static const std::string emote = "/emote";
		static const std::string emote2 = "/me";

		static const std::string add = "add";
		static const std::string remove = "remove";
		static const std::string list = "list";
		static const std::string clear = "clear";

		static const std::string help_chat_help = _("Commands: whisper ignore emote. Type /help [command] for more help.");

		bool is_command = (message.at(0) == '/');
		unsigned int argc = 0;
		std::string cmd, arg1, arg2;

		if(is_command){
			std::string::size_type sp1 = message.find_first_of(' ');
			cmd = message.substr(0,sp1);
			if(sp1 != std::string::npos) {
				std::string::size_type arg1_start = message.find_first_not_of(' ',sp1);
				if(arg1_start != std::string::npos) {
					++argc;
					std::string::size_type substr_len, sp2;
					sp2 = message.find(' ',arg1_start);
					substr_len = (sp2 == std::string::npos) ? sp2 : sp2 - arg1_start;
					arg1 = message.substr(arg1_start,substr_len);
					if(sp2 != std::string::npos) {
						std::string::size_type arg2_end = message.find_last_not_of(' ');
						if(arg2_end > sp2) {
							++argc;
							arg2 = message.substr(sp2+1, arg2_end - sp2);
						}
					}
				}
			}
		}

		
		if(cmd == query && argc > 0) {
			const std::string args = (argc < 2) ? arg1 : arg1 + " " + arg2;
			send_chat_query(args);
		} else if ((cmd == whisper || cmd == whisper2) && argc > 1 /*&& is_observer()*/) {
			config cwhisper,data;
			cwhisper["message"] = arg2;
			cwhisper["sender"] = preferences::login();
			cwhisper["receiver"] = arg1;
			data.add_child("whisper", cwhisper);
			add_chat_message("whisper to "+cwhisper["receiver"],0,cwhisper["message"], display::MESSAGE_PRIVATE);
			network::send_data(data);

		} else if (cmd == help) {
															
			bool have_command = (argc > 0);
			bool have_subcommand = (argc > 1);
			
			const std::string command = arg1;
			const std::string subcommand = arg2;

			if (have_command) {
				if (command == "whisper" || command == "msg") {
					add_chat_message("help",0,_("Sends private message. You can't send messages to players that control any side in game. Usage: /whisper [nick] [message]"));
				} else if (command == "ignore") {
					if (have_subcommand) {
						if (subcommand == "add"){
							add_chat_message("help",0,_("Add player to your ignore list. Usage: /ignore add [argument]"));
						} else if (subcommand == "remove") {
							add_chat_message("help",0,_("Remove player from your ignore list. Usage: /ignore remove [argument]"));
						} else if (subcommand == "clear") {
							add_chat_message("help",0,_("Clear your ignore list. Usage: /ignore clear"));
						} else if (subcommand == "list") {
							add_chat_message("help",0,_("Show your ignore list. Usage: /ignore list"));
						} else {
							add_chat_message("help",0,_("Unknown subcommand"));
						}
					} else {
						add_chat_message("help",0,_("Ignore messages from players on this list. Usage: /ignore [subcommand] [argument](optional) Subcommands: add remove list clear. Type /help ignore [subcommand] for more info."));
					}
				} else if (command == "emote" || command == "me") {
					add_chat_message("help",0,_("Send an emotion or personal action in chat. Usage: /emote [message]"));
				} else {
					add_chat_message("help",0,_("Unknown command"));
				}
			} else {
				add_chat_message("help",0,help_chat_help);
			}
		} else if (message.size() > ignore.size() && std::equal(ignore.begin(),ignore.end(), message.begin())) {

			config* cignore;

			if (arg1 == add){
				if (!preferences::get_prefs()->child("ignore")){
					preferences::get_prefs()->add_child("ignore");
				}
				cignore = preferences::get_prefs()->child("ignore");
				if(utils::isvalid_username(arg2))
				{
					(*cignore)[arg2] = "yes";
					add_chat_message("ignores list",0, _("Added to ignore list: ")+arg2,display::MESSAGE_PRIVATE);
				} else {
					add_chat_message("ignores list",0, _("Invalid username: ")+arg2,display::MESSAGE_PRIVATE);
				}

			} else if (arg1 == remove){
				if ((cignore = preferences::get_prefs()->child("ignore"))){
					if(utils::isvalid_username(arg2))
					{
						(*cignore)[arg2] = "no";
						add_chat_message("ignores list",0, _("Removed from ignore list: ")+arg2,display::MESSAGE_PRIVATE);
					} else {
						add_chat_message("ignores list",0, _("Invalid username: ")+arg2,display::MESSAGE_PRIVATE);
					}
				}	
			} else if (arg1 == list){
				std::string text;
				if ((cignore = preferences::get_prefs()->child("ignore"))){
					for(std::map<std::string,t_string>::const_iterator i = cignore->values.begin();
							i != cignore->values.end(); ++i){
						if (i->second == "yes"){
							text+=i->first+",";
						}
					}
					if(!text.empty()){
						text.erase(text.length()-1,1);
					}
				}	
				add_chat_message("ignores list",0, text,display::MESSAGE_PRIVATE);
			} else if (arg1 == clear){

				if ((cignore = preferences::get_prefs()->child("ignore"))){
					string_map::iterator nick;
					for(nick= cignore->values.begin() ; nick!= cignore->values.end(); nick++) {
						if((*cignore)[nick->first] != "no") {
							(*cignore)[nick->first] = "no";
							add_chat_message("ignores list",0, _("Removed from ignore list: ")+nick->first,display::MESSAGE_PRIVATE);
						}
					}
				}	
			} else {			
				add_chat_message("ignores list",0,_("Unknown command: ")+arg1,display::MESSAGE_PRIVATE);	
			}
		} else if ((cmd == emote || cmd == emote2) && argc > 0) {
			//emote message
			send_chat_message("/me" + message.substr(cmd.size()), allies_only);
		} else if (is_command) {
			//command not accepted, show help chat help
			add_chat_message("help",0,help_chat_help);
		} else {
			//not a command, send as normal
			send_chat_message(message, allies_only);
		}
	}


	void menu_handler::send_chat_message(const std::string& message, bool allies_only)
	{
		config cfg;
		cfg["description"] = preferences::login();
		cfg["message"] = message;

		const int side = is_observer() ? 0 : gui_->viewing_team()+1;
		if(!is_observer()) {
			cfg["side"] = lexical_cast<std::string>(side);
		}

		bool private_message = has_friends() && allies_only;

		if(private_message) {
			cfg["team_name"] = teams_[gui_->viewing_team()].team_name();
		}

		recorder.speak(cfg);
		add_chat_message(cfg["description"],side,message,
							  private_message ? display::MESSAGE_PRIVATE : display::MESSAGE_PUBLIC);

	}


	void menu_handler::do_search(const std::string& new_search)
	{
		if(new_search.empty() == false && new_search != last_search_)
			last_search_ = new_search;

		if(last_search_.empty()) return;

		bool found = false;
		gamemap::location loc = last_search_hit_;
		//If this is a location search, just center on that location.
		std::vector<std::string> args = utils::split(last_search_, ',');
		if(args.size() == 2) {
			int x, y;
			x = lexical_cast_default<int>(args[0], 0)-1;
			y = lexical_cast_default<int>(args[1], 0)-1;
			if(x >= 0 && x < map_.x() && y >= 0 && y < map_.y()) {
				loc = gamemap::location(x,y);
				found = true;
			}
		}
		//Start scanning the game map
		if(loc.valid() == false)
			loc = gamemap::location(map_.x()-1,map_.y()-1);
		gamemap::location start = loc;
		while (!found) {
			//Move to the next location
			loc.x = (loc.x + 1) % map_.x();
			if(loc.x == 0)
				loc.y = (loc.y + 1) % map_.y();

			//Search label
			const std::string label = gui_->labels().get_label(loc);
			if(label.empty() == false) {
				if(std::search(label.begin(), label.end(),
						last_search_.begin(), last_search_.end(),
						chars_equal_insensitive) != label.end()) {
					found = !gui_->shrouded(loc.x, loc.y);
				}
			}
			//Search unit name
			unit_map::const_iterator ui = units_.find(loc);
			if(ui != units_.end()) {
				const std::string name = ui->second.description();
				if(std::search(name.begin(), name.end(),
						last_search_.begin(), last_search_.end(),
						chars_equal_insensitive) != name.end()) {
					found = !gui_->fogged(loc.x, loc.y);
				}
			}
			if(loc == start)
				break;
		}
		if(found) {
			last_search_hit_ = loc;
			gui_->scroll_to_tile(loc.x,loc.y,display::ONSCREEN,false);
			gui_->highlight_hex(loc);
		} else {
			last_search_hit_ = gamemap::location();
			//Not found, inform the player
			utils::string_map symbols;
			symbols["search"] = last_search_;
			const std::string msg = utils::interpolate_variables_into_string(
				_("Couldn't find label or unit containing the string '$search'."),&symbols);
			gui::show_dialog(*gui_,NULL,"",msg);
		}
	}

	void menu_handler::do_command(const std::string& str, const unsigned int team_num, mouse_handler& mousehandler)
	{
		const std::string::const_iterator i = std::find(str.begin(),str.end(),' ');
		const std::string cmd(str.begin(),i);
		const std::string data(i == str.end() ? str.end() : i+1,str.end());

		if(cmd == "refresh") {
			image::flush_cache();
			gui_->redraw_everything();
		} else if (cmd == "droid") {
			const unsigned int side = lexical_cast_default<unsigned int>(data, 1);
			const size_t index = static_cast<size_t>(side - 1);
			if (index >= teams_.size() || teams_[index].is_network()) {
				//do nothing
			} else if (teams_[index].is_human()) {
				teams_[index].make_ai();
				textbox_info_.close(*gui_);
				if(team_num == side) {
					throw end_turn_exception(side);
				}
			} else if (teams_[index].is_ai()) {
				teams_[index].make_human();
			}
		} else if (cmd == "theme") {
		  preferences::show_theme_dialog(*gui_);
		} else if(cmd == "ban" || cmd == "kick") {
			config cfg;
			config& ban = cfg.add_child(cmd);
			ban["username"] = data;

			network::send_data(cfg);
		} else if (cmd == "mute") {
			config cfg;
			config& mute = cfg.add_child(cmd);
			if (!data.empty()) {
				mute["username"] = data;
			}

			network::send_data(cfg);
		} else if (cmd == "muteall") {
			config cfg;
			cfg.add_child(cmd);

			network::send_data(cfg);
		} else if(cmd == "control") {
			const std::string::const_iterator j = std::find(data.begin(),data.end(),' ');
			if(j != data.end()) {
				const std::string side(data.begin(),j);
				const std::string player(j+1,data.end());
				unsigned int side_num;
				try {
					side_num = lexical_cast<unsigned int, std::string>(side);
				} catch(bad_lexical_cast&) {
					return;
				}
				if(side_num > 0 && side_num <= teams_.size()) {
					if(teams_[static_cast<size_t>(side_num - 1)].is_human()) {
						change_side_controller(side,player,true);
						if(team_num == side_num) {
							teams_[static_cast<size_t>(side_num - 1)].make_network();
							textbox_info_.close(*gui_);
							throw end_turn_exception(side_num);
						}
					} else {
						change_side_controller(side,player);
					}
				}
			}
		} else if(cmd == "clear") {
			gui_->clear_chat_messages();
		} else if(cmd == "w") {
   			save_game(data,gui::NULL_DIALOG);
		} else if(cmd == "wq") {
			save_game(data,gui::NULL_DIALOG);
			throw end_level_exception(QUIT);
		} else if(cmd == "q!" || cmd == "q") {
			throw end_level_exception(QUIT);
		} else if(cmd == "ignore_replay_errors") {
			game_config::ignore_replay_errors = (data != "off") ? true : false;
		} else if(cmd == "n" && game_config::debug) {
			throw end_level_exception(VICTORY);
		} else if(cmd == "debug" && network::nconnections() == 0) {
			game_config::debug = true;
		} else if(game_config::debug && cmd == "unit") {
			const unit_map::iterator i = current_unit(mousehandler);
			if(i != units_.end()) {
				const std::string::const_iterator j = std::find(data.begin(),data.end(),'=');
				if(j != data.end()) {
					const std::string name(data.begin(),j);
					const std::string value(j+1,data.end());
					config cfg;
					i->second.write(cfg);
					cfg[name] = value;
					i->second = unit(&gameinfo_,&units_,&map_,&status_,&teams_,cfg);

					gui_->invalidate(i->first);
					gui_->invalidate_unit();
				}
			}
		} else if(game_config::debug && cmd == "create" && map_.on_board(mousehandler.get_last_hex())) {
			const game_data::unit_type_map::const_iterator i = gameinfo_.unit_types.find(data);
			if(i == gameinfo_.unit_types.end()) {
				return;
			}

			units_.erase(mousehandler.get_last_hex());
			units_.add(new std::pair<gamemap::location,unit>(mousehandler.get_last_hex(),unit(&gameinfo_,&units_,&map_,&status_,&teams_,&i->second,1,false)));
			gui_->invalidate(mousehandler.get_last_hex());
			gui_->invalidate_unit();
		} else if(game_config::debug && cmd == "gold") {
			teams_[team_num - 1].spend_gold(-lexical_cast_default<int>(data,1000));
			gui_->redraw_everything();
		}
	}

	void menu_handler::user_command()
	{
		textbox_info_.show(gui::TEXTBOX_COMMAND,sgettext("prompt^Command:"), "", false, *gui_);
	}

	void menu_handler::change_side_controller(const std::string& side, const std::string& player, bool orphan_side)
	{
		config cfg;
		config& change = cfg.add_child("change_controller");
		change["side"] = side;
		change["player"] = player;

		if(orphan_side) {
			change["orphan_side"] = "yes";
		}

		network::send_data(cfg);
	}
}
