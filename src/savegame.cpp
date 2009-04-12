/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by J�rg Hinrichs, refactored from various
   places formerly created by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "savegame.hpp"

#include "dialogs.hpp" //FIXME: move illegal file character function here and get rid of this include
#include "foreach.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "unit_id.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "serialization/binary_or_text.hpp"
#include "sound.hpp"
#include "statistics.hpp"
#include "version.hpp"

#define LOG_SAVE LOG_STREAM(info, engine)

loadgame::loadgame(display& gui, const config& game_config, game_state& gamestate)
	: game_config_(game_config)
	, gui_(gui)
	, gamestate_(gamestate)
{
	gamestate_ = game_state();
}

void loadgame::show_dialog(bool show_replay, bool cancel_orders)
{
	bool show_replay_dialog = show_replay;
	bool cancel_orders_dialog = cancel_orders;

	if (filename_.empty())
	{
		//FIXME: Integrate the load_game dialog into this class
		filename_ = dialogs::load_game_dialog(gui_, game_config_, &show_replay_dialog, &cancel_orders_dialog);

		show_replay_ = show_replay;
		cancel_orders_ = cancel_orders;
	}
}

void loadgame::load_game()
{
	show_dialog(false, false);

	if(filename_ != "")
		throw game::load_game_exception(filename_, show_replay_, cancel_orders_);
}

void loadgame::load_game(std::string& filename, bool show_replay, bool cancel_orders)
{
	filename_ = filename;
	show_dialog(show_replay, cancel_orders);
	show_replay_ = show_replay;
	cancel_orders_ = cancel_orders;

	if (filename_.empty())
		show_dialog(show_replay, cancel_orders);

	if (filename_.empty())
		throw load_game_cancelled_exception();

	std::string error_log;
	::read_save_file(filename_, load_config_, &error_log);

	if(!error_log.empty()) {
        try {
		    gui::show_error_message(gui_,
				    _("Warning: The file you have tried to load is corrupt. Loading anyway.\n") +
				    error_log);
        } catch (utils::invalid_utf8_exception&) {
		    gui::show_error_message(gui_,
				    _("Warning: The file you have tried to load is corrupt. Loading anyway.\n") +
                    std::string("(UTF-8 ERROR)"));
        }
	}

	gamestate_.difficulty = load_config_["difficulty"];
	gamestate_.campaign_define = load_config_["campaign_define"];
	gamestate_.campaign_type = load_config_["campaign_type"];
	gamestate_.campaign_xtra_defines = utils::split(load_config_["campaign_extra_defines"]);
	gamestate_.version = load_config_["version"];

	if(gamestate_.version != game_config::version) {
		const version_info parsed_savegame_version(gamestate_.version);
		if(game_config::wesnoth_version.minor_version() % 2 != 0 ||
		   game_config::wesnoth_version.major_version() != parsed_savegame_version.major_version() ||
		   game_config::wesnoth_version.minor_version() != parsed_savegame_version.minor_version()) {
			// do not load if too old, if either the savegame or the current game
			// has the version 'test' allow loading
			if(!game_config::is_compatible_savegame_version(gamestate_.version)) {
				/* GCC-3.3 needs a temp var otherwise compilation fails */
				gui::message_dialog dlg(gui_, "", _("This save is from a version too old to be loaded."));
				dlg.show();
				throw load_game_cancelled_exception();
			}

			const int res = gui::dialog(gui_,"",
								_("This save is from a different version of the game. Do you want to try to load it?"),
								gui::YES_NO).show();
			if(res == 1) {
				throw load_game_cancelled_exception();
			}
		}
	}

}

void loadgame::set_gamestate()
{
	gamestate_ = game_state(load_config_, show_replay_);

	// Get the status of the random in the snapshot.
	// For a replay we need to restore the start only, the replaying gets at
	// proper location.
	// For normal loading also restore the call count.
	const int seed = lexical_cast_default<int>
		(load_config_["random_seed"], 42);
	const unsigned calls = show_replay_ ? 0 :
		lexical_cast_default<unsigned> (gamestate_.snapshot["random_calls"]);
	gamestate_.rng().seed_random(seed, calls);
}

savegame::savegame(game_state& gamestate, const std::string title)
	: gamestate_(gamestate)
	, snapshot_()
	, filename_()
	, title_(title)
	, error_message_(_("The game could not be saved"))
	, interactive_(false)
{}

void savegame::save_game_interactive(display& gui, const std::string& message, 
									 gui::DIALOG_TYPE dialog_type, const bool has_exit_button, 
									 const bool ask_for_filename)
{
	interactive_ = true;
	create_filename();
	const int res = dialogs::get_save_name(gui, message, _("Name: "), &filename_, dialog_type, title_, has_exit_button, ask_for_filename);

	if (res == 2)
		throw end_level_exception(QUIT);

	if (res != 0)
		return;

	save_game(&gui);
}

void savegame::before_save()
{
	gamestate_.replay_data = recorder.get_replay_data();
}

void savegame::save_game(const std::string& filename)
{
	filename_ = filename;
	if (!interactive_)
		create_filename();
	
	save_game();
}

void savegame::save_game(display* gui)
{
	try {
		before_save();
		save_game_internal(filename_);

		if (gui != NULL && interactive_)
			gui::message_dialog(*gui,_("Saved"),_("The game has been saved")).show();
	} catch(game::save_game_failed&) {
		if (gui != NULL){
			gui::message_dialog to_show(*gui,_("Error"), error_message_);
			to_show.show();
			//do not bother retrying, since the user can just try to save the game again
			//maybe show a yes-no dialog for "disable autosaves now"?
		}
	};
}

void savegame::save_game_internal(const std::string& filename)
{
	LOG_SAVE << "savegame::save_game";

	filename_ = filename;

	if(preferences::compress_saves()) {
		filename_ += ".gz";
	}

	std::stringstream ss;
	{
		config_writer out(ss, preferences::compress_saves());
		write_game(out);
		finish_save_game(out);
	}
	scoped_ostream os(open_save_game(filename_));
	(*os) << ss.str();

	if (!os->good()) {
		throw game::save_game_failed(_("Could not write to file"));
	}
}

void savegame::write_game(config_writer &out) const
{
	log_scope("write_game");

	out.write_key_val("label", gamestate_.label);
	out.write_key_val("history", gamestate_.history);
	out.write_key_val("abbrev", gamestate_.abbrev);
	out.write_key_val("version", game_config::version);
	out.write_key_val("scenario", gamestate_.scenario);
	out.write_key_val("next_scenario", gamestate_.next_scenario);
	out.write_key_val("completion", gamestate_.completion);
	out.write_key_val("campaign", gamestate_.campaign);
	out.write_key_val("campaign_type", gamestate_.campaign_type);
	out.write_key_val("difficulty", gamestate_.difficulty);
	out.write_key_val("campaign_define", gamestate_.campaign_define);
	out.write_key_val("campaign_extra_defines", utils::join(gamestate_.campaign_xtra_defines));
	out.write_key_val("random_seed", lexical_cast<std::string>(gamestate_.rng().get_random_seed()));
	out.write_key_val("random_calls", lexical_cast<std::string>(gamestate_.rng().get_random_calls()));
	out.write_key_val("next_underlying_unit_id", lexical_cast<std::string>(n_unit::id_manager::instance().get_save_id()));
	out.write_key_val("end_text", gamestate_.end_text);
	out.write_key_val("end_text_duration", str_cast<unsigned int>(gamestate_.end_text_duration));
	out.write_child("variables", gamestate_.get_variables());

	for(std::map<std::string, wml_menu_item *>::const_iterator j=gamestate_.wml_menu_items.begin();
	    j!=gamestate_.wml_menu_items.end(); ++j) {
		out.open_child("menu_item");
		out.write_key_val("id", j->first);
		out.write_key_val("image", j->second->image);
		out.write_key_val("description", j->second->description);
		out.write_key_val("needs_select", (j->second->needs_select) ? "yes" : "no");
		if(!j->second->show_if.empty())
			out.write_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			out.write_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			out.write_child("command", j->second->command);
		out.close_child("menu_item");
	}

	if (!gamestate_.replay_data.child("replay")) {
		out.write_child("replay", gamestate_.replay_data);
	}

	out.write_child("snapshot",snapshot_);
	out.write_child("replay_start",gamestate_.starting_pos);
	out.open_child("statistics");
	statistics::write_stats(out);
	out.close_child("statistics");
}

void savegame::finish_save_game(const config_writer &out)
{
	std::string name = gamestate_.label;
	std::replace(name.begin(),name.end(),' ','_');
	std::string fname(get_saves_dir() + "/" + name);

	try {
		if(!out.good()) {
			throw game::save_game_failed(_("Could not write to file"));
		}

		config& summary = save_summary(gamestate_.label);
		extract_summary_data_from_save(summary);
		const int mod_time = static_cast<int>(file_create_time(fname));
		summary["mod_time"] = str_cast(mod_time);
		write_save_index();
	} catch(io_exception& e) {
		throw game::save_game_failed(e.what());
	}
}

void savegame::extract_summary_data_from_save(config& out)
{
	const bool has_replay = gamestate_.replay_data.empty() == false;
	const bool has_snapshot = gamestate_.snapshot.child("side");

	out["replay"] = has_replay ? "yes" : "no";
	out["snapshot"] = has_snapshot ? "yes" : "no";

	out["label"] = gamestate_.label;
	out["campaign"] = gamestate_.campaign;
	out["campaign_type"] = gamestate_.campaign_type;
	out["scenario"] = gamestate_.scenario;
	out["difficulty"] = gamestate_.difficulty;
	out["version"] = gamestate_.version;
	out["corrupt"] = "";

	if(has_snapshot) {
		out["turn"] = gamestate_.snapshot["turn_at"];
		if(gamestate_.snapshot["turns"] != "-1") {
			out["turn"] = out["turn"].str() + "/" + gamestate_.snapshot["turns"].str();
		}
	}

	// Find the first human leader so we can display their icon in the load menu.

	/** @todo Ideally we should grab all leaders if there's more than 1 human player? */
	std::string leader;

	for(std::map<std::string, player_info>::const_iterator p = gamestate_.players.begin();
	    p!=gamestate_.players.end(); ++p) {
		for(std::vector<unit>::const_iterator u = p->second.available_units.begin(); u != p->second.available_units.end(); ++u) {
			if(u->can_recruit()) {
				leader = u->type_id();
			}
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config& snapshot = has_snapshot ? gamestate_.snapshot : gamestate_.starting_pos;
		foreach (const config &side, snapshot.child_range("side"))
		{
			if (side["controller"] != "human") {
				continue;
			}

			if (utils::string_bool(side["shroud"])) {
				shrouded = true;
			}

			foreach (const config &u, side.child_range("unit"))
			{
				if (utils::string_bool(u["canrecruit"], false)) {
					leader = u["id"];
					break;
				}
			}
		}
	}

	out["leader"] = leader;
	out["map_data"] = "";

	if(!shrouded) {
		if(has_snapshot) {
			if (!gamestate_.snapshot.find_child("side", "shroud", "yes")) {
				out["map_data"] = gamestate_.snapshot["map_data"];
			}
		} else if(has_replay) {
			if (!gamestate_.starting_pos.find_child("side", "shroud", "yes")) {
				out["map_data"] = gamestate_.starting_pos["map_data"];
			}
		}
	}
}

void savegame::set_filename(std::string filename)
{
	filename.erase(std::remove_if(filename.begin(), filename.end(),
	            dialogs::is_illegal_file_char), filename.end());
	filename_ = filename;
}

scenariostart_savegame::scenariostart_savegame(game_state &gamestate) 
	: savegame(gamestate) 
{
	set_filename(gamestate.label);
}

void scenariostart_savegame::before_save()
{
	//Add the player section to the starting position so we can get the correct recall list
	//when loading the replay later on
	write_players(gamestate(), gamestate().starting_pos);
}

replay_savegame::replay_savegame(game_state &gamestate) 
	: savegame(gamestate, _("Save Replay")) 
{}

void replay_savegame::create_filename()
{
	std::stringstream stream;

	const std::string ellipsed_name = font::make_text_ellipsis(gamestate().label,
			font::SIZE_NORMAL, 200);
	stream << ellipsed_name << " " << _("replay");

	set_filename(stream.str());
}

autosave_savegame::autosave_savegame(game_state &gamestate, const config& level_cfg, 
							 const game_display& gui, const std::vector<team>& teams, 
							 const unit_map& units, const gamestatus& gamestatus,
							 const gamemap& map)
	: game_savegame(gamestate, level_cfg, gui, teams, units, gamestatus, map)
{
	set_error_message(_("Could not auto save the game. Please save the game manually."));
	create_filename();
}

void autosave_savegame::create_filename()
{
	std::string filename;
	if (gamestate().label.empty())
		filename = _("Auto-Save");
	else
		filename = gamestate().label + "-" + _("Auto-Save") + lexical_cast<std::string>(gamestatus_.turn());

	set_filename(filename);
}

game_savegame::game_savegame(game_state &gamestate, const config& level_cfg, 
							 const game_display& gui, const std::vector<team>& teams, 
							 const unit_map& units, const gamestatus& gamestatus,
							 const gamemap& map) 
	: savegame(gamestate, _("Save Game")),
	level_cfg_(level_cfg), gui_(gui),
	teams_(teams), units_(units),
	gamestatus_(gamestatus), map_(map)
{}

void game_savegame::create_filename()
{
	std::stringstream stream;

	const std::string ellipsed_name = font::make_text_ellipsis(gamestate().label,
			font::SIZE_NORMAL, 200);
	stream << ellipsed_name << " " << _("Turn") << " " << gamestatus_.turn();
	set_filename(stream.str());
}

void game_savegame::before_save()
{
	savegame::before_save();
	write_game_snapshot();
}

void game_savegame::write_game_snapshot()
{
	snapshot().merge_attributes(level_cfg_);

	snapshot()["snapshot"] = "yes";

	std::stringstream buf;
	buf << gui_.playing_team();
	snapshot()["playing_team"] = buf.str();

	for(std::vector<team>::const_iterator t = teams_.begin(); t != teams_.end(); ++t) {
		const unsigned int side_num = t - teams_.begin() + 1;

		config& side = snapshot().add_child("side");
		t->write(side);
		side["no_leader"] = "yes";
		buf.str(std::string());
		buf << side_num;
		side["side"] = buf.str();

		//current visible units
		for(unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i) {
			if(i->second.side() == side_num) {
				config& u = side.add_child("unit");
				i->first.write(u);
				i->second.write(u);
			}
		}
		//recall list
		{
			for(std::map<std::string, player_info>::const_iterator i=gamestate().players.begin();
			i!=gamestate().players.end(); ++i) {
				for(std::vector<unit>::const_iterator j = i->second.available_units.begin();
					j != i->second.available_units.end(); ++j) {
					if (j->side() == side_num){
						config& u = side.add_child("unit");
						j->write(u);
					}
				}
			}
		}
	}

	gamestatus_.write(snapshot());
	game_events::write_events(snapshot());

	// Write terrain_graphics data in snapshot, too
	const config::child_list& terrains = level_cfg_.get_children("terrain_graphics");
	for(config::child_list::const_iterator tg = terrains.begin();
			tg != terrains.end(); ++tg) {

		snapshot().add_child("terrain_graphics", **tg);
	}

	sound::write_music_play_list(snapshot());

	gamestate().write_snapshot(snapshot());

	//write out the current state of the map
	snapshot()["map_data"] = map_.write();

	gui_.labels().write(snapshot());
}

