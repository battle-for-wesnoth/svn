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
#include "preferences_display.hpp"
#include "replay.hpp"
#include "serialization/binary_or_text.hpp"
#include "sound.hpp"

#define LOG_SAVE LOG_STREAM(info, engine)

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
		::write_game(out, snapshot_, gamestate_);
		finish_save_game(out);
	}
	scoped_ostream os(open_save_game(filename_));
	(*os) << ss.str();

	if (!os->good()) {
		throw game::save_game_failed(_("Could not write to file"));
	}
}

void savegame::finish_save_game(config_writer &out)
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

