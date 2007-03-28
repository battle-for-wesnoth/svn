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

#include "global.hpp"
#include "wassert.hpp"

#include "filesystem.hpp"
#include "game_config.hpp"
#include "game_errors.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "language.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "statistics.hpp"
#include "util.hpp"
#include "variable.hpp"
#include "wesconfig.h"
#include "serialization/binary_or_text.hpp"
#include "serialization/binary_wml.hpp"
#include "serialization/parser.hpp"
#include "serialization/string_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <sstream>

#define LOG_NG lg::info(lg::engine)
#define WRN_NG lg::warn(lg::engine)
#define ERR_NG lg::err(lg::engine)

#ifdef _WIN32
#include <windows.h>

// conv_ansi_utf8()
//   - Convert a string between ANSI encoding (for Windows filename) and UTF-8
//  string &name
//     - filename to be converted
//  bool a2u
//     - if true, convert the string from ANSI to UTF-8.
//     - if false, reverse. (convert it from UTF-8 to ANSI)
void conv_ansi_utf8(std::string &name, bool a2u) {
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
                                   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return;
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return;
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
                            wc, wlen) == 0) {
		delete wc;
		return;
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
                                   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete wc;
		return;
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete wc;
		return;
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
                        ac, alen, NULL, NULL);
	delete wc;
	if (ac == NULL) {
		return;
	}
	name = ac;
	delete ac;

	return;
}

void replace_underbar2space(std::string &name) {
    LOG_NG << "conv(A2U)-from:[" << name << "]" << std::endl;
    conv_ansi_utf8(name, true);
    LOG_NG << "conv(A2U)-to:[" << name << "]" << std::endl;
    LOG_NG << "replace_underbar2space-from:[" << name << "]" << std::endl;
    std::replace(name.begin(), name.end(), '_', ' ');
    LOG_NG << "replace_underbar2space-to:[" << name << "]" << std::endl;
}

void replace_space2underbar(std::string &name) {
    LOG_NG << "conv(U2A)-from:[" << name << "]" << std::endl;
    conv_ansi_utf8(name, false);
    LOG_NG << "conv(U2A)-to:[" << name << "]" << std::endl;
    LOG_NG << "replace_underbar2space-from:[" << name << "]" << std::endl;
    std::replace(name.begin(), name.end(), ' ', '_');
    LOG_NG << "replace_underbar2space-to:[" << name << "]" << std::endl;
}
#else /* ! _WIN32 */
void replace_underbar2space(std::string &name) {
    std::replace(name.begin(),name.end(),'_',' ');
}
void replace_space2underbar(std::string &name) {
    std::replace(name.begin(),name.end(),' ','_');
}
#endif /* _WIN32 */

player_info* game_state::get_player(const std::string& id) {
	std::map< std::string, player_info >::iterator found = players.find(id);
	if (found == players.end()) {
		LOG_STREAM(warn, engine) << "player " << id << " does not exist.\n";
		return NULL;
	} else
		return &found->second;
}

time_of_day::time_of_day(const config& cfg)
                 : lawful_bonus(atoi(cfg["lawful_bonus"].c_str())),
                   bonus_modified(0),
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
	snprintf(buf,sizeof(buf),"%d",lawful_bonus);
	cfg["lawful_bonus"] = buf;

	snprintf(buf,sizeof(buf),"%d",red);
	cfg["red"] = buf;

	snprintf(buf,sizeof(buf),"%d",green);
	cfg["green"] = buf;

	snprintf(buf,sizeof(buf),"%d",blue);
	cfg["blue"] = buf;

	cfg["image"] = image;
	cfg["name"] = name;
	cfg["id"] = id;
	cfg["mask"] = image_mask;
}

namespace {

void parse_times(const config& cfg, std::vector<time_of_day>& normal_times)
{
	const config::child_list& times = cfg.get_children("time");
	config::child_list::const_iterator t;
	for(t = times.begin(); t != times.end(); ++t) {
		normal_times.push_back(time_of_day(**t));
	}


	if ( normal_times.empty() )
	{
		// Makeing sure we have at least default time

		config dummy_cfg;
		normal_times.push_back(time_of_day(dummy_cfg));
	}
}

}
/// Reads turns and time information from parameters
/// It sets random starting ToD and current_tod to config
///
gamestatus::gamestatus(const config& time_cfg, int num_turns, game_state* s_o_g) :
                 turn_(1),numTurns_(num_turns),currentTime_(0)
{
    std::string turn_at = time_cfg["turn_at"];
	std::string current_tod = time_cfg["current_tod"];
	std::string random_start_time = time_cfg["random_start_time"];
	if (s_o_g) 
	{
	    turn_at = utils::interpolate_variables_into_string(turn_at, *s_o_g);
		current_tod = utils::interpolate_variables_into_string(current_tod, *s_o_g);

	}
	
	if(turn_at.empty() == false) 
	{
		turn_ = atoi(turn_at.c_str());
	}
	
	
	parse_times(time_cfg,times_);

	set_start_ToD(const_cast<config&>(time_cfg),s_o_g);

	const config::child_list& times_range = time_cfg.get_children("time_area");
	for(config::child_list::const_iterator t = times_range.begin(); t != times_range.end(); ++t) {
		const std::vector<gamemap::location> locs = parse_location_range((**t)["x"],(**t)["y"]);
		area_time_of_day area;
		area.xsrc = (**t)["x"];
		area.ysrc = (**t)["y"];
		std::copy(locs.begin(),locs.end(),std::inserter(area.hexes,area.hexes.end()));
		parse_times(**t,area.times);
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
	buf.str(std::string());
	buf << currentTime_;
	cfg["current_tod"] = buf.str();

	std::vector<time_of_day>::const_iterator t;
	for(t = times_.begin(); t != times_.end(); ++t) {
		t->write(cfg.add_child("time"));
	}


	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		config& area = cfg.add_child("time_area");
		area["x"] = i->xsrc;
		area["y"] = i->ysrc;
		for(t = i->times.begin(); t != i->times.end(); ++t) {
			t->write(area.add_child("time"));
		}

	}
}

/// returns time of day object in the turn 
/// Correct time is calculated from current time.

time_of_day gamestatus::get_time_of_day_turn(int nturn) const
{
	wassert(!times_.empty());
	
	int time = (currentTime_ + nturn  - turn())% times_.size();
	
	if (time < 0)
	{
		time += times_.size();
	}

	return times_[time];
}

/// returns time of day object for current turn

time_of_day gamestatus::get_time_of_day() const
{
	return times_[currentTime_];
}

time_of_day gamestatus::get_previous_time_of_day() const
{
	return get_time_of_day_turn(turn()-1);
}

/// Returns time of day object in the turn. 
/// It first tryes to look for specified. If no area time specified in location it returns global time.

time_of_day gamestatus::get_time_of_day(int illuminated, const gamemap::location& loc, int n_turn) const
{
	time_of_day res = get_time_of_day_turn(n_turn);
	
	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		if(i->hexes.count(loc) == 1) {
			
			wassert(!i->times.empty());

			res = i->times[(n_turn-1)%i->times.size()];
			break;
		}
	}


	if(illuminated) {
		res.bonus_modified=illuminated;
		res.lawful_bonus += illuminated;
	}
	return res;
}

time_of_day gamestatus::get_time_of_day(int illuminated, const gamemap::location& loc) const
{
	return get_time_of_day(illuminated,loc,turn());
}

/// Sets global time of day in this turn.
/// Time is number between 0 and n-1 where n is number of ToDs

bool gamestatus::set_time_of_day(int newTime)
{
	// newTime can come from network so have to take run time test
	if( newTime >= static_cast<int>(times_.size()) 
	  || newTime < 0)
	{
		return false;
	}
	
	currentTime_ = newTime;

	return true;
}

bool gamestatus::is_start_ToD(const std::string& random_start_time)
{
	return !random_start_time.empty()
			&& utils::string_bool(random_start_time, true);
}

void gamestatus::set_start_ToD(config &level, game_state* s_o_g)
{
	if (!level["current_tod"].empty())
	{
		set_time_of_day(atoi(level["current_tod"].c_str()));
		return;
	}
	std::string random_start_time = level["random_start_time"];
	if (s_o_g)
	{
		random_start_time = utils::interpolate_variables_into_string(random_start_time, *s_o_g);
	}			 
	if (gamestatus::is_start_ToD(random_start_time))
	{
		std::vector<std::string> start_strings = 
			utils::split(random_start_time, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);

		if (utils::string_bool(random_start_time,false))
		{
			// We had boolean value
			set_time_of_day(rand()%times_.size());
		}
		else
		{
			set_time_of_day(atoi(start_strings[rand()%start_strings.size()].c_str()) - 1);
		}
	}
	else
	{
		// We have to set right ToD for oldsaves 
		
		set_time_of_day((turn() - 1) % times_.size());
	}
	// Setting tod to level data

	std::stringstream buf;
	buf << currentTime_;
	level["current_tod"] = buf.str();

}

void gamestatus::next_time_of_day()
{
	wassert(times_.size() > 0);

	currentTime_ = (currentTime_ + 1)%times_.size();
}
	
size_t gamestatus::turn() const
{
	return turn_;
}

int gamestatus::number_of_turns() const
{
	return numTurns_;
}
void gamestatus::modify_turns(const std::string& mod)
{
	numTurns_ = maximum<int>(utils::apply_modifier(numTurns_,mod,0),-1);
}
void gamestatus::add_turns(int num)
{
	numTurns_ = maximum<int>(numTurns_ + num,-1);
}


bool gamestatus::next_turn()
{
	next_time_of_day();
	++turn_;
	return numTurns_ == -1 || turn_ <= size_t(numTurns_);
}

player_info read_player(const game_data& data, const config* cfg)
{
	player_info res;

	res.name = (*cfg)["name"];

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
	res.campaign = (*cfg)["campaign"];

	const config* snapshot = cfg->child("snapshot");

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

			//backwards compatibility for 1.2 and 1.2.1,
			//------------------------------------------
			//add recall list units to the snapshot so they don't get lost
			if (!snapshot->empty() && (res.version < "1.2.2") )
			{
				//find the side of this player in the snapshot
				config* current_side = NULL;
				const std::vector<config*>& side_cfg = snapshot->get_children("side");
				for (std::vector<config*>::const_iterator side = side_cfg.begin(); side != side_cfg.end(); side++){
					if ((**side)["save_id"] == save_id){
						current_side = *side;
						break;
					}
				}
				if (!current_side){
					std::cerr << "Could not find side " << save_id << " in the snapshot!";
					throw game::load_game_failed();
				}
				//add available units of this player to the snapshot
				const config::child_list& unit_cfg = (*i)->get_children("unit");
				for (config::child_list::const_iterator u = unit_cfg.begin(); u != unit_cfg.end(); u++){
					//we don't want the leader to show up in the recall list
					if ((**u)["canrecruit"].empty()){
						//we also don't want units to show up in the recall list if they have been recalled
						//already by WML events. However, since available units might be in the snapshot already
						//we better check that before adding them
						bool found_unit_in_snapshot = false;
						const config::child_list& snapshot_units = current_side->get_children("unit");
						for (config::child_list::const_iterator su = snapshot_units.begin(); 
						su != snapshot_units.end(); su++){
							//experience / hitpoints and such are not sufficient to identify a unit since
							//they are subject to change if fighting or leveling happens. Therefore identification
							//is achieved by the user_description. If the "x"-attribute is filled it means this is
							//one of the units originally contained in the snapshot (and not added here).
							if ( ((**su)["user_description"] == (**u)["user_description"]) && (!(**su)["x"].empty()) ){
								found_unit_in_snapshot = true;
							}
						}
						if (!found_unit_in_snapshot){
							//Add this unit to the snapshot. Missing "x"- and "y"-attributes will later cause it to go
							//into the available_units vector (recall list)
							current_side->add_child("unit", **u);
						}
					}
				}
			}

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
		res.set_variables(*vars);
	}

	const config* const replay = cfg->child("replay");
	if(replay != NULL) {
		res.replay_data = *replay;
	}

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
	cfg["name"] = player.name;

	char buf[50];
	snprintf(buf,sizeof(buf),"%d",player.gold);

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

void write_player(config_writer &out, const player_info& player)
{
	out.write_key_val("name", player.name);

	char buf[50];
	snprintf(buf,sizeof(buf),"%d",player.gold);

	out.write_key_val("gold", buf);

	for(std::vector<unit>::const_iterator i = player.available_units.begin();
	    i != player.available_units.end(); ++i) {
		config new_cfg;
		i->write(new_cfg);
		out.write_child("unit",new_cfg);
	}

	std::stringstream can_recruit;
	std::copy(player.can_recruit.begin(),player.can_recruit.end(),std::ostream_iterator<std::string>(can_recruit,","));
	std::string can_recruit_str = can_recruit.str();

	//remove the trailing comma
	if(can_recruit_str.empty() == false) {
		can_recruit_str.resize(can_recruit_str.size()-1);
	}

	out.write_key_val("can_recruit", can_recruit_str);
}


// Deprecated, use other write_game below.
void write_game(const game_state& gamestate, config& cfg, WRITE_GAME_MODE mode)
{
	log_scope("write_game");
	cfg["label"] = gamestate.label;
	cfg["version"] = game_config::version;

	cfg["scenario"] = gamestate.scenario;

	cfg["campaign"] = gamestate.campaign;

	cfg["campaign_type"] = gamestate.campaign_type;

	cfg["difficulty"] = gamestate.difficulty;

	cfg["campaign_define"] = gamestate.campaign_define;

	cfg.add_child("variables",gamestate.get_variables());

	for(std::map<std::string, player_info>::const_iterator i=gamestate.players.begin();
	    i!=gamestate.players.end(); ++i) {
		config new_cfg;
		write_player(i->second, new_cfg);
		new_cfg["save_id"]=i->first;
		cfg.add_child("player", new_cfg);
	}

	if(mode == WRITE_FULL_GAME) {
		if(gamestate.replay_data.child("replay") == NULL) {
			cfg.add_child("replay",gamestate.replay_data);
		}

		cfg.add_child("snapshot",gamestate.snapshot);
		cfg.add_child("replay_start",gamestate.starting_pos);
		cfg.add_child("statistics",statistics::write_stats());
	}
}

void write_game(config_writer &out, const game_state& gamestate, WRITE_GAME_MODE mode)
{
	log_scope("write_game");

	out.write_key_val("label", gamestate.label);
	out.write_key_val("version", game_config::version);
	out.write_key_val("scenario", gamestate.scenario);
	out.write_key_val("campaign", gamestate.campaign);
	out.write_key_val("campaign_type", gamestate.campaign_type);
	out.write_key_val("difficulty", gamestate.difficulty);
	out.write_key_val("campaign_define", gamestate.campaign_define);
	out.write_child("variables", gamestate.get_variables());

	for(std::map<std::string, player_info>::const_iterator i=gamestate.players.begin();
	    i!=gamestate.players.end(); ++i) {
		out.open_child("player");
		out.write_key_val("save_id", i->first);
		write_player(out, i->second);
		out.close_child("player");
	}

	if(mode == WRITE_FULL_GAME) {
		if(gamestate.replay_data.child("replay") == NULL) {
			out.write_child("replay", gamestate.replay_data);
		}

		out.write_child("snapshot",gamestate.snapshot);
		out.write_child("replay_start",gamestate.starting_pos);
		out.open_child("statistics");
		statistics::write_stats(out);
		out.close_child("statistics");
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

std::vector<save_info> get_saves_list(const std::string *dir)
{
	const std::string& saves_dir = (dir) ? *dir : get_saves_dir();

	std::vector<std::string> saves;
	get_files_in_dir(saves_dir,&saves);

	std::vector<save_info> res;
	for(std::vector<std::string>::iterator i = saves.begin(); i != saves.end(); ++i) {
		const time_t modified = file_create_time(saves_dir + "/" + *i);

		replace_underbar2space(*i);
		res.push_back(save_info(*i,modified));
	}

	std::sort(res.begin(),res.end(),save_info_less_time());

	return res;
}

bool save_game_exists(const std::string& name)
{
	std::string fname = name;
	replace_space2underbar(fname);

	return file_exists(get_saves_dir() + "/" + fname);
}

void delete_game(const std::string& name)
{
	std::string modified_name = name;
	replace_space2underbar(modified_name);

	remove((get_saves_dir() + "/" + name).c_str());
	remove((get_saves_dir() + "/" + modified_name).c_str());
}

void read_save_file(const std::string& name, config& cfg, std::string* error_log)
{
	std::string modified_name = name;
	replace_space2underbar(modified_name);

	//try reading the file both with and without underscores
	scoped_istream file_stream = istream_file(get_saves_dir() + "/" + modified_name);
	if (file_stream->fail())
		file_stream = istream_file(get_saves_dir() + "/" + name);

	cfg.clear();
	try{
	detect_format_and_read(cfg, *file_stream, error_log);
	} catch (config::error &err)
	{
		std::cerr << err.message;
		throw game::load_game_failed();
	}

	if(cfg.empty()) {
		std::cerr << "Could not parse file data into config\n";
		throw game::load_game_failed();
	}
}

void load_game(const game_data& data, const std::string& name, game_state& gamestate, std::string* error_log)
{
	log_scope("load_game");

	config cfg;
	read_save_file(name,cfg,error_log);

	gamestate = read_game(data,&cfg);
}

void load_game_summary(const std::string& name, config& cfg_summary, std::string* error_log){
	log_scope("load_game_summary");

	config cfg;
	read_save_file(name,cfg,error_log);

	extract_summary_from_config(cfg, cfg_summary);
}

//throws game::save_game_failed
scoped_ostream open_save_game(const std::string &label)
{
	std::string name = label;
 	replace_space2underbar(name);

	try {
		return scoped_ostream(ostream_file(get_saves_dir() + "/" + name));
	} catch(io_exception& e) {
		throw game::save_game_failed(e.what());
	}
}

void finish_save_game(config_writer &out, const game_state& gamestate, const std::string &label)
{
	std::string name = label;
	std::replace(name.begin(),name.end(),' ','_');
	std::string fname(get_saves_dir() + "/" + name);

	try {
		if(!out.good()) {
			throw game::save_game_failed(_("Could not write to file"));
		}

		config& summary = save_summary(label);
		extract_summary_data_from_save(gamestate,summary);
		const int mod_time = static_cast<int>(file_create_time(fname));
		summary["mod_time"] = str_cast(mod_time);
		write_save_index();
	} catch(io_exception& e) {
		throw game::save_game_failed(e.what());
	}
}

//throws game::save_game_failed
void save_game(const game_state& gamestate)
{
	scoped_ostream os(open_save_game(gamestate.label));
	config_writer out(*os, preferences::compress_saves(), PACKAGE);
	write_game(out, gamestate);
	finish_save_game(out, gamestate, gamestate.label);
}

namespace {
bool save_index_loaded = false;
config save_index_cfg;
}

config& save_index()
{
	if(save_index_loaded == false) {
		try {
			scoped_istream stream = istream_file(get_save_index_file());
			detect_format_and_read(save_index_cfg, *stream);
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
		scoped_ostream stream = ostream_file(get_save_index_file());
		write_compressed(*stream, save_index());
	} catch(io_exception& e) {
		std::cerr << "error writing to save index file: '" << e.what() << "'\n";
	}
}

void extract_summary_data_from_save(const game_state& gamestate, config& out)
{
	const bool has_replay = gamestate.replay_data.empty() == false;
	const bool has_snapshot = gamestate.snapshot.child("side") != NULL;

	out["replay"] = has_replay ? "yes" : "no";
	out["snapshot"] = has_snapshot ? "yes" : "no";

	out["label"] = gamestate.label;
	out["campaign_type"] = gamestate.campaign_type;
	out["scenario"] = gamestate.scenario;
	out["difficulty"] = gamestate.difficulty;
	out["version"] = gamestate.version;
	out["corrupt"] = "";

	if(has_snapshot) {
		out["turn"] = gamestate.snapshot["turn_at"];
		if(gamestate.snapshot["turns"] != "-1") {
			out["turn"] = out["turn"].str() + "/" + gamestate.snapshot["turns"].str();
		}
	}

	//find the first human leader so we can display their icon in the load menu

	//ideally we should grab all leaders if there's more than 1
	//human player?
	std::string leader;

	for(std::map<std::string, player_info>::const_iterator p = gamestate.players.begin();
	    p!=gamestate.players.end(); ++p) {
		for(std::vector<unit>::const_iterator u = p->second.available_units.begin(); u != p->second.available_units.end(); ++u) {
			if(u->can_recruit()) {
				leader = u->id();
			}
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config& snapshot = has_snapshot ? gamestate.snapshot : gamestate.starting_pos;
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
					leader = (**u)["id"];
					break;
				}
			}
		}
	}

	out["leader"] = leader;
	out["map_data"] = "";

	if(!shrouded) {
		if(has_snapshot) {
			if(gamestate.snapshot.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = gamestate.snapshot["map_data"];
			}
		} else if(has_replay) {
			if(gamestate.starting_pos.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = gamestate.starting_pos["map_data"];
			}
		}
	}
}

void extract_summary_from_config(config& cfg_save, config& cfg_summary)
{
	const config* cfg_snapshot = cfg_save.child("snapshot");
	const config* cfg_replay_start = cfg_save.child("replay_start");

	const bool has_replay = cfg_save.child("replay") != NULL;
	const bool has_snapshot = (cfg_snapshot != NULL) && (cfg_snapshot->child("side") != NULL);

	cfg_summary["replay"] = has_replay ? "yes" : "no";
	cfg_summary["snapshot"] = has_snapshot ? "yes" : "no";

	cfg_summary["label"] = cfg_save["label"];
	cfg_summary["campaign_type"] = cfg_save["campaign_type"];
	cfg_summary["scenario"] = cfg_save["scenario"];
	cfg_summary["difficulty"] = cfg_save["difficulty"];
	cfg_summary["version"] = cfg_save["version"];
	cfg_summary["corrupt"] = "";

	if(has_snapshot) {
		cfg_summary["turn"] = (*cfg_snapshot)["turn_at"];
		if((*cfg_snapshot)["turns"] != "-1") {
			cfg_summary["turn"] = cfg_summary["turn"].str() + "/" + (*cfg_snapshot)["turns"].str();
		}
	}

	//find the first human leader so we can display their icon in the load menu

	//ideally we should grab all leaders if there's more than 1
	//human player?
	std::string leader;

	const config::child_list& players = cfg_save.get_children("player");

	for(config::child_list::const_iterator i = players.begin(); i != players.end(); ++i) {
		if ((**i)["canrecruit"] == "1"){
			leader = (**i)["save_id"];
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config* snapshot = has_snapshot ? cfg_snapshot : cfg_replay_start;
		if (snapshot != NULL){
			const config::child_list& sides = snapshot->get_children("side");
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
						leader = (**u)["id"];
						break;
					}
				}
			}
		}
	}

	cfg_summary["leader"] = leader;
	cfg_summary["map_data"] = "";

	if(!shrouded) {
		if(has_snapshot) {
			if(cfg_snapshot->find_child("side","shroud","yes") == NULL) {
				cfg_summary["map_data"] = (*cfg_snapshot)["map_data"];
			}
		} else if(has_replay) {
			if(cfg_replay_start->find_child("side","shroud","yes") == NULL) {
				cfg_summary["map_data"] = (*cfg_replay_start)["map_data"];
			}
		}
	}
}

namespace {
const size_t MaxLoop = 1024;
}

void game_state::activate_scope_variable(std::string var_name) const
{
	if(recursive_)
		return;
	const std::string::iterator itor = std::find(var_name.begin(),var_name.end(),'.');
	if(itor != var_name.end()) {
		var_name.erase(itor, var_name.end());
	}
	std::vector<scoped_wml_variable*>::const_reverse_iterator rit;
	for(rit = scoped_variables.rbegin(); rit != scoped_variables.rend(); ++rit) {
		if((**rit).name() == var_name) {
			recursive_ = true;
			if(!(**rit).activated()) {
				(**rit).activate();
			}
			recursive_ = false;
			break;
		}
	}
}

void game_state::get_variable_internal_const(const std::string& key, const config& cfg,
		const t_string** varout) const
{
	//we get the variable from the [variables] section of the game state. Variables may
	//be in the format
	const std::string::const_iterator itor = std::find(key.begin(),key.end(),'.');
	if(itor != key.end()) {
		std::string element(key.begin(),itor);
		std::string sub_key(itor+1,key.end());

		size_t index = 0;
		const std::string::iterator index_start = std::find(element.begin(),element.end(),'[');
		const bool explicit_index = index_start != element.end();

		if(explicit_index) {
			const std::string::iterator index_end = std::find(index_start,element.end(),']');
			const std::string index_str(index_start+1,index_end);
			index = size_t(atoi(index_str.c_str()));
			if(index > MaxLoop) {
				LOG_NG << "get_variable_internal: index greater than " << MaxLoop
				       << ", truncated\n";
				index = MaxLoop;
			}

			element = std::string(element.begin(),index_start);
		}

		const config::child_list& items = cfg.get_children(element);

		//special case -- '.length' on an array returns the size of the array
		if(explicit_index == false && sub_key == "length") {
			if(items.empty()) {
				if(varout != NULL) {
					static const t_string zero_str = "";
					*varout = &zero_str;
				}
			} else {
				int size = minimum<int>(MaxLoop,int(items.size()));
				(*items.back())["__length"] = lexical_cast<std::string>(size);

				if(varout != NULL) {
					*varout = &(*items.back())["__length"];
				}
			}

			return;
		}

		if(cfg.get_children(element).size() <= index) {
			static const t_string empty_str = "";
			*varout = &empty_str;
			return;
		}


		get_variable_internal_const(sub_key,*cfg.get_children(element)[index],varout);
	} else {
		if(varout != NULL) {
			*varout = &cfg[key];
		}
	}
}
void game_state::get_variable_internal(const std::string& key, config& cfg,
		t_string** varout, config** cfgout)
{
	//we get the variable from the [variables] section of the game state. Variables may
	//be in the format
	const std::string::const_iterator itor = std::find(key.begin(),key.end(),'.');
	if(itor != key.end()) {
		std::string element(key.begin(),itor);
		std::string sub_key(itor+1,key.end());

		size_t index = 0;
		const std::string::iterator index_start = std::find(element.begin(),element.end(),'[');
		const bool explicit_index = index_start != element.end();

		if(explicit_index) {
			const std::string::iterator index_end = std::find(index_start,element.end(),']');
			const std::string index_str(index_start+1,index_end);
			index = size_t(atoi(index_str.c_str()));
			if(index > MaxLoop) {
				LOG_NG << "get_variable_internal: index greater than " << MaxLoop
				       << ", truncated\n";
				index = MaxLoop;
			}

			element = std::string(element.begin(),index_start);
		}

		const config::child_list& items = cfg.get_children(element);

		//special case -- '.length' on an array returns the size of the array
		if(explicit_index == false && sub_key == "length") {
			if(items.empty()) {
				if(varout != NULL) {
					static t_string zero_str;
					zero_str = "";
					*varout = &zero_str;
				}
			} else {
				int size = minimum<int>(MaxLoop,int(items.size()));
				(*items.back())["__length"] = lexical_cast<std::string>(size);

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


t_string& game_state::get_variable(const std::string& key)
{
	t_string* res = NULL;
	activate_scope_variable(key);
	get_variable_internal(key, variables, &res, NULL);
	return *res;
}

const t_string& game_state::get_variable_const(const std::string& key) const
{
	const t_string* res = NULL;
	activate_scope_variable(key);
	get_variable_internal_const(key, variables, &res);
	if(res != NULL) {
		return *res;
	}

	static t_string empty_string;
	return empty_string;
}

config& game_state::get_variable_cfg(const std::string& key)
{
	config* res = NULL;
	activate_scope_variable(key);
	get_variable_internal(key + ".", variables, NULL, &res);
	if(res != NULL) {
		return *res;
	}
	static config illegal_key_cfg;
	LOG_NG << "get_variable_cfg: illegal name \"" << key << "\" used for WML array.";
	return illegal_key_cfg;
}

void game_state::set_variable(const std::string& key, const t_string& value)
{
	t_string* res = NULL;
	activate_scope_variable(key);
	get_variable_internal(key, variables, &res, NULL);
	*res = value;
}

config& game_state::add_variable_cfg(const std::string& key, const config& value)
{
	activate_scope_variable(key);
	return variables.add_child(key, value);
}

void game_state::clear_variable_cfg(const std::string& varname)
{
	variables.clear_children(varname);
}

void game_state::clear_variable(const std::string& varname)
{
	config* vars = &variables;
	std::string key(varname);
	std::string::const_iterator itor = std::find(key.begin(),key.end(),'.');
	int dot_index = key.find('.');
	// "mover.modifications.trait[0]"
	while(itor != key.end()) { // subvar access
		std::string element=key.substr(0,dot_index);
		key = key.substr(dot_index+1);

		size_t index = 0;
		const std::string::iterator index_start = std::find(element.begin(),element.end(),'[');
		const bool explicit_index = index_start != element.end();
		if(explicit_index) {
			const std::string::iterator index_end = std::find(index_start,element.end(),']');
			const std::string index_str(index_start+1,index_end);
			index = static_cast<size_t>(lexical_cast_default<int>(index_str));
			if(index > MaxLoop) {
				LOG_NG << "clear_variable: index greater than " << MaxLoop
				       << ", truncated\n";
				index = MaxLoop;
			}
			element = std::string(element.begin(),index_start);
		}

		if(vars->get_children(element).size() <= index) {
			return;
		}
		//std::cerr << "Entering " << element << "[" << index << "] of [" << vars->get_children(element).size() << "]\n";
		vars = vars->get_children(element)[index];
		if(!vars) {
			return;
		}
		itor = std::find(key.begin(),key.end(),'.');
		dot_index = key.find('.');
	}
	size_t index = 0;
	const std::string::iterator index_start = std::find(key.begin(),key.end(),'[');
	const bool explicit_index = index_start != key.end();
	if(explicit_index) {
		const std::string::iterator index_end = std::find(index_start,key.end(),']');
		const std::string index_str(index_start+1,index_end);
		index = static_cast<size_t>(lexical_cast_default<int>(index_str));
		if(index > MaxLoop) {
			LOG_NG << "clear_variable: index greater than " << MaxLoop
				   << ", truncated\n";
			index = MaxLoop;
		}
		key = std::string(key.begin(),index_start);
	}

	if(explicit_index) {
		if(vars->get_children(key).size() <= index) {
			return;
		}
		//std::cerr << "Removing " << key << "\n";
		vars->remove_child(key,index);
	} else {
		vars->values.erase(key);
		vars->clear_children(key);
	}
}

namespace {
void clear_wmi(std::map<std::string, wml_menu_item*>& gs_wmi) {
	std::map<std::string, wml_menu_item*>::iterator itor = gs_wmi.begin();
	while(itor != gs_wmi.end()) {
		delete itor->second;
		itor = gs_wmi.erase(itor);
	}
}
} //end anon namespace

game_state::game_state(const game_state& state)
{
	*this = state;
}

game_state& game_state::operator=(const game_state& state)
{
	if(this == &state) {
		return *this;
	}

	label = state.label;
	version = state.version;
	campaign_type = state.campaign_type;
	campaign_define = state.campaign_define;
	campaign = state.campaign;
	scenario = state.scenario;
	players = state.players;
	scoped_variables = state.scoped_variables;
	
	clear_wmi(wml_menu_items);
	std::map<std::string, wml_menu_item*>::const_iterator itor;
	for (itor = state.wml_menu_items.begin(); itor != state.wml_menu_items.end(); ++itor) {
		wml_menu_item*& mref = wml_menu_items[itor->first];
		mref = new wml_menu_item(*(itor->second));
	}

	difficulty = state.difficulty;
	replay_data = state.replay_data;
	starting_pos = state.starting_pos;
	snapshot = state.snapshot;
	recursive_ = state.recursive_;

	return *this;
}

game_state::~game_state() {
	clear_wmi(wml_menu_items);
}
