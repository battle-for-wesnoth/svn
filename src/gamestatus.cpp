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

#include "global.hpp"

#include "filesystem.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "language.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "statistics.hpp"
#include "util.hpp"
#include "serialization/binary_wml.hpp"
#include "serialization/parser.hpp"
#include "serialization/string_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <sstream>

time_of_day::time_of_day(const config& cfg)
                 : lawful_bonus(atoi(cfg["lawful_bonus"].c_str())),
                   image(cfg["image"]), name(cfg["name"]), id(cfg["id"]),
				   image_mask(cfg["mask"]),
                   red(atoi(cfg["red"].c_str())),
                   green(atoi(cfg["green"].c_str())),
                   blue(atoi(cfg["blue"].c_str()))
{
}

void time_of_day::write(config& cfg) const
{
	char buf[50];
	sprintf(buf,"%d",lawful_bonus);
	cfg["lawful_bonus"] = buf;

	sprintf(buf,"%d",red);
	cfg["red"] = buf;

	sprintf(buf,"%d",green);
	cfg["green"] = buf;

	sprintf(buf,"%d",blue);
	cfg["blue"] = buf;

	cfg["image"] = image;
	cfg["name"] = name;
	cfg["id"] = id;
	cfg["mask"] = image_mask;
}

namespace {

void parse_times(const config& cfg, std::vector<time_of_day>& normal_times, std::vector<time_of_day>& illuminated_times)
{
	const config::child_list& times = cfg.get_children("time");
	config::child_list::const_iterator t;
	for(t = times.begin(); t != times.end(); ++t) {
		normal_times.push_back(time_of_day(**t));
	}

	const config::child_list& times_illum = cfg.get_children("illuminated_time");
	const config::child_list& illum = times_illum.empty() ? times : times_illum;

	for(t = illum.begin(); t != illum.end(); ++t) {
		illuminated_times.push_back(time_of_day(**t));
	}
}

}

gamestatus::gamestatus(config& time_cfg, int num_turns) :
                 turn_(1),numTurns_(num_turns)
{
	const std::string& turn_at = time_cfg["turn_at"];
	if(turn_at.empty() == false) {
		turn_ = atoi(turn_at.c_str());
	}

	parse_times(time_cfg,times_,illuminatedTimes_);

	const config::child_list& times_range = time_cfg.get_children("time_area");
	for(config::child_list::const_iterator t = times_range.begin(); t != times_range.end(); ++t) {
		const std::vector<gamemap::location> locs = parse_location_range((**t)["x"],(**t)["y"]);
		area_time_of_day area;
		area.xsrc = (**t)["x"];
		area.ysrc = (**t)["y"];
		std::copy(locs.begin(),locs.end(),std::inserter(area.hexes,area.hexes.end()));
		parse_times(**t,area.times,area.illuminated_times);

		areas_.push_back(area);
	}
}

void gamestatus::write(config& cfg) const
{
	std::stringstream buf;
	buf << turn_;
	cfg["turn_at"] = buf.str();
	buf.str(std::string());
	buf << numTurns_;
	cfg["turns"] = buf.str();

	std::vector<time_of_day>::const_iterator t;
	for(t = times_.begin(); t != times_.end(); ++t) {
		t->write(cfg.add_child("time"));
	}

	for(t = illuminatedTimes_.begin(); t != illuminatedTimes_.end(); ++t) {
		t->write(cfg.add_child("illuminated_time"));
	}

	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		config& area = cfg.add_child("time_area");
		area["x"] = i->xsrc;
		area["y"] = i->ysrc;
		for(t = i->times.begin(); t != i->times.end(); ++t) {
			t->write(area.add_child("time"));
		}

		for(t = i->illuminated_times.begin(); t != i->illuminated_times.end(); ++t) {
			t->write(area.add_child("illuminated_time"));
		}
	}
}

const time_of_day& gamestatus::get_time_of_day_turn(int nturn) const
{
	if(times_.empty() == false) {
		return times_[(nturn-1)%times_.size()];
	} else {
		config dummy_cfg;
		const static time_of_day default_time(dummy_cfg);
		return default_time;
	}
}

const time_of_day& gamestatus::get_time_of_day() const
{
	return get_time_of_day_turn(turn());
}

const time_of_day& gamestatus::get_previous_time_of_day() const
{
	return get_time_of_day_turn(turn()-1);
}

const time_of_day& gamestatus::get_time_of_day(bool illuminated, const gamemap::location& loc) const
{
	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		if(i->hexes.count(loc) == 1) {
			if(illuminated && i->illuminated_times.empty() == false) {
				return i->illuminated_times[(turn()-1)%i->illuminated_times.size()];
			} else if(i->times.empty() == false) {
				return i->times[(turn()-1)%i->times.size()];
			}
		}
	}

	if(illuminated && illuminatedTimes_.empty() == false) {
		return illuminatedTimes_[(turn()-1)%illuminatedTimes_.size()];
	} else if(times_.empty() == false) {
		return times_[(turn()-1)%times_.size()];
	}

	config dummy_cfg;
	const static time_of_day default_time(dummy_cfg);
	return default_time;
}

size_t gamestatus::turn() const
{
	return turn_;
}

int gamestatus::number_of_turns() const
{
	return numTurns_;
}

bool gamestatus::next_turn()
{
	++turn_;
	return numTurns_ == -1 || turn_ <= size_t(numTurns_);
}

player_info read_player(const game_data& data, const config* cfg)
{
	player_info res;

	res.gold = atoi((*cfg)["gold"].c_str());

	const config::child_list& units = cfg->get_children("unit");
	for(config::child_list::const_iterator i = units.begin(); i != units.end(); ++i) {
		res.available_units.push_back(unit(data,**i));
	}

	res.can_recruit.clear();

	const std::string& can_recruit_str = (*cfg)["can_recruit"];
	if(can_recruit_str != "") {
		const std::vector<std::string> can_recruit = utils::split(can_recruit_str);
		std::copy(can_recruit.begin(),can_recruit.end(),std::inserter(res.can_recruit,res.can_recruit.end()));
	}

	return res;
}

game_state read_game(const game_data& data, const config* cfg)
{
	log_scope("read_game");
	game_state res;
	res.label = (*cfg)["label"];
	res.version = (*cfg)["version"];
	res.scenario = (*cfg)["scenario"];

	const config::child_list& players = cfg->get_children("player");

	if(players.empty()) {
		//backwards compatibility code: assume that there is player data
		//in the file itself, which corresponds to the leader of side 1
		const config::child_list& units = cfg->get_children("unit");
		config::child_list::const_iterator i;
		for(i = units.begin(); i != units.end(); ++i) {
			if((**i)["side"] == "1" && (**i)["canrecruit"] == "1") {
				break;
			}
		}

		if(i != units.end()) {
			std::cerr << "backwards compatibility: loading player '" << (**i)["description"] << "'\n";
			player_info player = read_player(data,cfg);
			res.players.insert(std::pair<std::string,player_info>((**i)["description"],player));
		}
	} else {
		for(config::child_list::const_iterator i = players.begin(); i != players.end(); ++i) {
			std::string save_id = (**i)["save_id"];

			if(save_id.empty()) {
				std::cerr << "Corrupted player entry: NULL save_id" << std::endl;
			} else {
				player_info player = read_player(data, *i);
				res.players.insert(std::pair<std::string, player_info>(save_id,player));
			}
		}
	}

	std::cerr << "scenario: '" << res.scenario << "'\n";

	res.difficulty = (*cfg)["difficulty"];
	if(res.difficulty.empty())
		res.difficulty = "NORMAL";

	res.campaign_define = (*cfg)["campaign_define"];

	res.campaign_type = (*cfg)["campaign_type"];
	if(res.campaign_type.empty())
		res.campaign_type = "scenario";

	const config* const vars = cfg->child("variables");
	if(vars != NULL) {
		res.variables = *vars;
	}

	const config* const replay = cfg->child("replay");
	if(replay != NULL) {
		res.replay_data = *replay;
	}

	const config* snapshot = cfg->child("snapshot");

	//older save files used to use 'start', so still support that for now
	if(snapshot == NULL) {
		snapshot = cfg->child("start");
	}

	if(snapshot != NULL) {
		res.snapshot = *snapshot;
	}

	const config* replay_start = cfg->child("replay_start");
	if(replay_start != NULL) {
		res.starting_pos = *replay_start;
	}

	if(cfg->child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(*cfg->child("statistics"));
	}

	return res;
}

void write_player(const player_info& player, config& cfg)
{
	char buf[50];
	sprintf(buf,"%d",player.gold);

	cfg["gold"] = buf;

	for(std::vector<unit>::const_iterator i = player.available_units.begin();
	    i != player.available_units.end(); ++i) {
		config new_cfg;
		i->write(new_cfg);
		cfg.add_child("unit",new_cfg);
	}

	std::stringstream can_recruit;
	std::copy(player.can_recruit.begin(),player.can_recruit.end(),std::ostream_iterator<std::string>(can_recruit,","));
	std::string can_recruit_str = can_recruit.str();

	//remove the trailing comma
	if(can_recruit_str.empty() == false) {
		can_recruit_str.resize(can_recruit_str.size()-1);
	}

	cfg["can_recruit"] = can_recruit_str;
}

void write_game(const game_state& game, config& cfg, WRITE_GAME_MODE mode)
{
	log_scope("write_game");
	cfg["label"] = game.label;
	cfg["version"] = game_config::version;

	cfg["scenario"] = game.scenario;

	cfg["campaign_type"] = game.campaign_type;

	cfg["difficulty"] = game.difficulty;

	cfg["campaign_define"] = game.campaign_define;

	cfg.add_child("variables",game.variables);

	for(std::map<std::string, player_info>::const_iterator i=game.players.begin();
	    i!=game.players.end(); ++i) {
		config new_cfg;
		write_player(i->second, new_cfg);
		new_cfg["save_id"]=i->first;
		cfg.add_child("player", new_cfg);
	}

	if(mode == WRITE_FULL_GAME) {
		if(game.replay_data.child("replay") == NULL) {
			cfg.add_child("replay",game.replay_data);
		}
		
		cfg.add_child("snapshot",game.snapshot);
		cfg.add_child("replay_start",game.starting_pos);
		cfg.add_child("statistics",statistics::write_stats());
	}
}

//a structure for comparing to save_info objects based on their modified time.
//if the times are equal, will order based on the name
struct save_info_less_time {
	bool operator()(const save_info& a, const save_info& b) const {
		return a.time_modified > b.time_modified ||
		       a.time_modified == b.time_modified && a.name > b.name;
	}
};

std::vector<save_info> get_saves_list()
{
	const std::string& saves_dir = get_saves_dir();

	std::vector<std::string> saves;
	get_files_in_dir(saves_dir,&saves);

	std::vector<save_info> res;
	for(std::vector<std::string>::iterator i = saves.begin(); i != saves.end(); ++i) {
		const time_t modified = file_create_time(saves_dir + "/" + *i);

		std::replace(i->begin(),i->end(),'_',' ');
		res.push_back(save_info(*i,modified));
	}

	std::sort(res.begin(),res.end(),save_info_less_time());

	return res;
}

bool save_game_exists(const std::string& name)
{
	std::string fname = name;
	std::replace(fname.begin(),fname.end(),' ','_');
	
	return file_exists(get_saves_dir() + "/" + fname);
}

void delete_game(const std::string& name)
{
	std::string modified_name = name;
	std::replace(modified_name.begin(),modified_name.end(),' ','_');

	remove((get_saves_dir() + "/" + name).c_str());
	remove((get_saves_dir() + "/" + modified_name).c_str());
}

void read_save_file(const std::string& name, config& cfg)
{
	std::string modified_name = name;
	std::replace(modified_name.begin(),modified_name.end(),' ','_');

	//try reading the file both with and without underscores
	std::string file_data = read_file(get_saves_dir() + "/" + modified_name);
	if(file_data.empty()) {
		file_data = read_file(get_saves_dir() + "/" + name);
	}

	cfg.clear();
	detect_format_and_read(cfg, file_data);

	if(cfg.empty()) {
		std::cerr << "Could not parse file data into config\n";
		throw gamestatus::load_game_failed();
	}
}

void load_game(const game_data& data, const std::string& name, game_state& state)
{
	log_scope("load_game");

	config cfg;
	read_save_file(name,cfg);
	
	state = read_game(data,&cfg);
}

//throws gamestatus::save_game_failed
void save_game(const game_state& state)
{
	log_scope("save_game");

	std::string name = state.label;
	std::replace(name.begin(),name.end(),' ','_');

	config cfg;
	try {
		write_game(state,cfg);

		const std::string fname = get_saves_dir() + "/" + name;

		write_file(fname, preferences::compress_saves() ? write_compressed(cfg) : write(cfg));

		config& summary = save_summary(state.label);
		extract_summary_data_from_save(state,summary);
		const int mod_time = static_cast<int>(file_create_time(fname));
		summary["mod_time"] = str_cast(mod_time);

		write_save_index();

	} catch(io_exception& e) {
		throw gamestatus::save_game_failed(e.what());
	}
}

namespace {
bool save_index_loaded = false;
config save_index_cfg;
}

config& save_index()
{
	if(save_index_loaded == false) {
		try {
			detect_format_and_read(save_index_cfg, read_file(get_save_index_file()));
		} catch(io_exception& e) {
			std::cerr << "error reading save index: '" << e.what() << "'\n";
		} catch(config::error&) {
			std::cerr << "error parsing save index config file\n";
			save_index_cfg.clear();
		}

		save_index_loaded = true;
	}

	return save_index_cfg;
}

config& save_summary(const std::string& save)
{
	config& cfg = save_index();
	config* res = cfg.find_child("save","save",save);
	if(res == NULL) {
		res = &cfg.add_child("save");
		(*res)["save"] = save;
	}

	return *res;
}

void delete_save_summary(const std::string& save)
{
	config& cfg = save_index();
	const config* const res = cfg.find_child("save","save",save);
	if(res != NULL) {
		const config::child_list& children = cfg.get_children("save");
		const size_t index = std::find(children.begin(),children.end(),res) - children.begin();
		cfg.remove_child("save",index);
	}
}

void write_save_index()
{
	log_scope("write_save_index()");
	try {
		write_file(get_save_index_file(), write_compressed(save_index()));
	} catch(io_exception& e) {
		std::cerr << "error writing to save index file: '" << e.what() << "'\n";
	}
}

void extract_summary_data_from_save(const game_state& state, config& out)
{
	const bool has_replay = state.replay_data.empty() == false;
	const bool has_snapshot = state.snapshot.child("side") != NULL;

	out["replay"] = has_replay ? "yes" : "no";
	out["snapshot"] = has_snapshot ? "yes" : "no";
	
	out["campaign_type"] = state.campaign_type;
	out["scenario"] = state.scenario;
	out["difficulty"] = state.difficulty;
	out["version"] = state.version;
	out["corrupt"] = "";

	if(has_snapshot) {
		out["turn"] = state.snapshot["turn_at"];
		if(state.snapshot["turns"] != "-1") {
			out["turn"] += "/" + state.snapshot["turns"];
		}
	}

	//find the first human leader so we can display their icon in the load menu

	//ideally we should grab all leaders if there's more than 1
	//human player?
	std::string leader;

	for(std::map<std::string, player_info>::const_iterator p = state.players.begin();
	    p!=state.players.end(); ++p) {
		for(std::vector<unit>::const_iterator u = p->second.available_units.begin(); u != p->second.available_units.end(); ++u) {
			if(u->can_recruit()) {
				leader = u->type().name();
			}
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config& snapshot = has_snapshot ? state.snapshot : state.starting_pos;
		const config::child_list& sides = snapshot.get_children("side");
		for(config::child_list::const_iterator s = sides.begin(); s != sides.end() && leader.empty(); ++s) {

			if((**s)["controller"] != "human") {
				continue;
			}

			if((**s)["shroud"] == "yes") {
				shrouded = true;
			}

			const config::child_list& units = (**s).get_children("unit");
			for(config::child_list::const_iterator u = units.begin(); u != units.end(); ++u) {
				if((**u)["canrecruit"] == "1") {
					leader = (**u)["type"];
					break;
				}
			}
		}
	}

	out["leader"] = leader;
	out["map_data"] = "";

	if(!shrouded) {
		if(has_snapshot) {
			if(state.snapshot.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = state.snapshot["map_data"];
			}
		} else if(has_replay) {
			if(state.starting_pos.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = state.starting_pos["map_data"];
			}
		}
	}
}
