#include "statistics.hpp"

namespace {

//this variable is true whenever the statistics are mid-scenario. This means
//a new scenario shouldn't be added to the master stats record.
bool mid_scenario = false;

typedef statistics::stats stats;

int stats_disabled = 0;

struct scenario_stats
{
	explicit scenario_stats(const std::string& name) : scenario_name(name)
	{}

	explicit scenario_stats(const config& cfg);

	config write() const;

	std::vector<stats> team_stats;
	std::string scenario_name;
};

scenario_stats::scenario_stats(const config& cfg)
{
	scenario_name = cfg["scenario"];
	const config::child_list& teams = cfg.get_children("team");
	for(config::child_list::const_iterator i = teams.begin(); i != teams.end(); ++i) {
		team_stats.push_back(stats(**i));
	}
}

config scenario_stats::write() const
{
	config res;
	res["scenario"] = scenario_name;
	for(std::vector<stats>::const_iterator i = team_stats.begin(); i != team_stats.end(); ++i) {
		res.add_child("team",i->write());
	}

	return res;
}

std::vector<scenario_stats> master_stats;

stats& get_stats(int team)
{
	if(master_stats.empty()) {
		master_stats.push_back(scenario_stats(""));
	}

	std::vector<stats>& team_stats = master_stats.back().team_stats;
	const size_t index = size_t(team-1);
	if(index >= team_stats.size()) {
		team_stats.resize(index+1);
	}

	return team_stats[index];
}

config write_str_int_map(const stats::str_int_map& m)
{
	config res;
	for(stats::str_int_map::const_iterator i = m.begin(); i != m.end(); ++i) {
		char buf[50];
		sprintf(buf,"%d",i->second);
		res[i->first] = buf;
	}

	return res;
}

stats::str_int_map read_str_int_map(const config& cfg)
{
	stats::str_int_map m;
	for(string_map::const_iterator i = cfg.values.begin(); i != cfg.values.end(); ++i) {
		m[i->first] = atoi(i->second.c_str());
	}

	return m;
}

config write_battle_result_map(const stats::battle_result_map& m)
{
	config res;
	for(stats::battle_result_map::const_iterator i = m.begin(); i != m.end(); ++i) {
		config& new_cfg = res.add_child("sequence");
		new_cfg = write_str_int_map(i->second);

		char buf[50];
		sprintf(buf,"%d",i->first);
		new_cfg["_num"] = buf;
	}

	return res;
}

stats::battle_result_map read_battle_result_map(const config& cfg)
{
	stats::battle_result_map m;
	const config::child_list c = cfg.get_children("sequence");
	for(config::child_list::const_iterator i = c.begin(); i != c.end(); ++i) {
		config item = **i;
		const int key = atoi(item["_num"].c_str());
		item.values.erase("_num");
		m[key] = read_str_int_map(item);
	}

	return m;
}

void merge_str_int_map(stats::str_int_map& a, const stats::str_int_map& b)
{
	for(stats::str_int_map::const_iterator i = b.begin(); i != b.end(); ++i) {
		a[i->first] += i->second;
	}
}

void merge_battle_result_maps(stats::battle_result_map& a, const stats::battle_result_map& b)
{
	for(stats::battle_result_map::const_iterator i = b.begin(); i != b.end(); ++i) {
		merge_str_int_map(a[i->first],i->second);
	}
}

void merge_stats(stats& a, const stats& b)
{
	merge_str_int_map(a.recruits,b.recruits);
	merge_str_int_map(a.recalls,b.recalls);
	merge_str_int_map(a.advanced_to,b.advanced_to);
	merge_str_int_map(a.deaths,b.deaths);
	merge_str_int_map(a.killed,b.killed);

	merge_battle_result_maps(a.attacks,b.attacks);
	merge_battle_result_maps(a.defends,b.defends);

	a.recruit_cost += b.recruit_cost;
	a.recall_cost += b.recall_cost;
	a.damage_inflicted += b.damage_inflicted;
	a.damage_taken += b.damage_taken;
}

}

namespace statistics
{

stats::stats() : recruit_cost(0), recall_cost(0), damage_inflicted(0), damage_taken(0)
{}

stats::stats(const config& cfg)
{
	read(cfg);
}

config stats::write() const
{
	config res;
	res.add_child("recruits",write_str_int_map(recruits));
	res.add_child("recalls",write_str_int_map(recalls));
	res.add_child("advances",write_str_int_map(advanced_to));
	res.add_child("deaths",write_str_int_map(deaths));
	res.add_child("killed",write_str_int_map(killed));
	res.add_child("attacks",write_battle_result_map(attacks));
	res.add_child("defends",write_battle_result_map(defends));

	char buf[50];
	sprintf(buf,"%d",recruit_cost);
	res["recruit_cost"] = buf;

	sprintf(buf,"%d",recall_cost);
	res["recall_cost"] = buf;

	sprintf(buf,"%d",damage_inflicted);
	res["damage_inflicted"] = buf;

	sprintf(buf,"%d",damage_taken);
	res["damage_taken"] = buf;

	return res;
}

void stats::read(const config& cfg)
{
	if(cfg.child("recruits")) {
		recruits = read_str_int_map(*cfg.child("recruits"));
	}

	if(cfg.child("recalls")) {
		recalls = read_str_int_map(*cfg.child("recalls"));
	}

	if(cfg.child("advances")) {
		advanced_to = read_str_int_map(*cfg.child("advances"));
	}

	if(cfg.child("deaths")) {
		deaths = read_str_int_map(*cfg.child("deaths"));
	}

	if(cfg.child("killed")) {
		killed = read_str_int_map(*cfg.child("killed"));
	}

	if(cfg.child("recalls")) {
		recalls = read_str_int_map(*cfg.child("recalls"));
	}

	if(cfg.child("attacks")) {
		attacks = read_battle_result_map(*cfg.child("attacks"));
	}

	if(cfg.child("defends")) {
		attacks = read_battle_result_map(*cfg.child("attacks"));
	}

	recruit_cost = atoi(cfg["recruit_cost"].c_str());
	recall_cost = atoi(cfg["recall_cost"].c_str());
	damage_inflicted = atoi(cfg["damage_inflicted"].c_str());
	damage_taken = atoi(cfg["damage_taken"].c_str());
}

disabler::disabler() { stats_disabled++; }
disabler::~disabler() { stats_disabled--; }

scenario_context::scenario_context(const std::string& name)
{
	if(!mid_scenario || master_stats.empty()) {
		master_stats.push_back(scenario_stats(name));
	}

	mid_scenario = true;
}

scenario_context::~scenario_context()
{
	mid_scenario = false;
}

attack_context::attack_context(const unit& a, const unit& d, const battle_stats& stats)
   : attacker_type(a.type().name()), defender_type(d.type().name()),
     bat_stats(stats), attacker_side(a.side()), defender_side(d.side())
{
}

attack_context::~attack_context()
{
	if(stats_disabled > 0)
		return;

	attacker_stats().attacks[bat_stats.chance_to_hit_defender][attacker_res]++;
	defender_stats().defends[bat_stats.chance_to_hit_attacker][defender_res]++;
}

stats& attack_context::attacker_stats()
{
	return get_stats(attacker_side);
}

stats& attack_context::defender_stats()
{
	return get_stats(defender_side);
}

void attack_context::attack_result(attack_context::ATTACK_RESULT res)
{
	if(stats_disabled > 0)
		return;

	attacker_res.resize(attacker_res.size()+1);
	attacker_res[attacker_res.size()-1] = (res == MISSES ? '0' : '1');

	attacker_stats().damage_inflicted += bat_stats.damage_defender_takes;
	defender_stats().damage_taken += bat_stats.damage_defender_takes;

	if(res == KILLS) {
		attacker_stats().killed[defender_type]++;
		defender_stats().deaths[attacker_type]++;
	}
}

void attack_context::defend_result(attack_context::ATTACK_RESULT res)
{
	if(stats_disabled > 0)
		return;

	defender_res.resize(defender_res.size()+1);
	defender_res[defender_res.size()-1] = (res == MISSES ? '0' : '1');

	attacker_stats().damage_taken += bat_stats.damage_attacker_takes;
	defender_stats().damage_inflicted += bat_stats.damage_defender_takes;

	if(res == KILLS) {
		attacker_stats().deaths[defender_type]++;
		defender_stats().killed[attacker_type]++;
	}
}

void recruit_unit(const unit& u)
{
	if(stats_disabled > 0)
		return;

	stats& s = get_stats(u.side());
	s.recruits[u.type().name()]++;
	s.recruit_cost += u.type().cost();
}

void recall_unit(const unit& u)
{
	if(stats_disabled > 0)
		return;

	stats& s = get_stats(u.side());
	s.recalls[u.type().name()]++;
	s.recall_cost += u.type().cost();
}

void advance_unit(const unit& u)
{
	if(stats_disabled > 0)
		return;

	stats& s = get_stats(u.side());
	s.advanced_to[u.type().name()]++;
}

std::vector<std::string> get_categories()
{
	std::vector<std::string> res;
	res.push_back("all_statistics");
	for(std::vector<scenario_stats>::const_iterator i = master_stats.begin(); i != master_stats.end(); ++i) {
		res.push_back(i->scenario_name);
	}

	return res;
}

stats calculate_stats(int category, int side)
{
	if(category == 0) {
		stats res;
		for(int i = 1; i <= int(master_stats.size()); ++i) {
			merge_stats(res,calculate_stats(i,side));
		}

		return res;
	} else {
		const size_t index = master_stats.size() - size_t(side);
		const size_t side_index = size_t(side) - 1;
		if(index < master_stats.size() && side_index < master_stats[index].team_stats.size()) {
			return master_stats[index].team_stats[side_index];
		} else {
			return stats();
		}
	}
}

config write_stats()
{
	config res;
	res["mid_scenario"] = (mid_scenario ? "true" : "false");

	for(std::vector<scenario_stats>::const_iterator i = master_stats.begin(); i != master_stats.end(); ++i) {
		res.add_child("scenario",i->write());
	}

	return res;
}

void read_stats(const config& cfg)
{
	fresh_stats();
	mid_scenario = (cfg["mid_scenario"] == "true");

	const config::child_list& scenarios = cfg.get_children("scenario");
	for(config::child_list::const_iterator i = scenarios.begin(); i != scenarios.end(); ++i) {
		master_stats.push_back(scenario_stats(**i));
	}
}

void fresh_stats()
{
	master_stats.clear();
	mid_scenario = false;
}

int sum_str_int_map(const stats::str_int_map& m)
{
	int res = 0;
	for(stats::str_int_map::const_iterator i = m.begin(); i != m.end(); ++i) {
		res += i->second;
	}

	return res;
}

}