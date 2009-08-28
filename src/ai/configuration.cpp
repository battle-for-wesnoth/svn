/* $Id$ */
/*
   Copyright (C) 2009 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


/**
 * Managing the AI configuration
 * @file ai/configuration.cpp
 */

#include "configuration.hpp"

#include "../filesystem.hpp"
#include "../foreach.hpp"
#include "../log.hpp"
#include "../serialization/parser.hpp"
#include "../serialization/preprocessor.hpp"
#include "../team.hpp"

#include <boost/lexical_cast.hpp>
#include <vector>

namespace ai {

static lg::log_domain log_ai_configuration("ai/config");
#define DBG_AI_CONFIGURATION LOG_STREAM(debug, log_ai_configuration)
#define LOG_AI_CONFIGURATION LOG_STREAM(info, log_ai_configuration)
#define WRN_AI_CONFIGURATION LOG_STREAM(warn, log_ai_configuration)
#define ERR_AI_CONFIGURATION LOG_STREAM(err, log_ai_configuration)


class well_known_aspect {
public:
	well_known_aspect(const std::string &name, bool attr = true)
		: name_(name),was_an_attribute_(attr)
	{
	}

	virtual ~well_known_aspect() {};

	std::string name_;
	bool was_an_attribute_;
};

std::vector<well_known_aspect> well_known_aspects;

void configuration::init(const config &game_config)
{
	ai_configurations_.clear();
	well_known_aspects.clear();
	well_known_aspects.push_back(well_known_aspect("aggression"));
	well_known_aspects.push_back(well_known_aspect("attack_depth"));
	well_known_aspects.push_back(well_known_aspect("attacks"));
	well_known_aspects.push_back(well_known_aspect("avoid",false));
	well_known_aspects.push_back(well_known_aspect("caution"));
	well_known_aspects.push_back(well_known_aspect("grouping"));
	well_known_aspects.push_back(well_known_aspect("leader_goal",false));
	well_known_aspects.push_back(well_known_aspect("leader_value"));
	well_known_aspects.push_back(well_known_aspect("number_of_possible_recruits_to_force_recruit"));
	well_known_aspects.push_back(well_known_aspect("passive_leader"));
	well_known_aspects.push_back(well_known_aspect("passive_leader_shares_keep"));
	//well_known_aspects.push_back(well_known_aspect("protect_leader"));
	//well_known_aspects.push_back(well_known_aspect("protect_leader_radius"));
	//well_known_aspects.push_back(well_known_aspect("protect_location",false));
	//well_known_aspects.push_back(well_known_aspect("protect_unit",false));
	well_known_aspects.push_back(well_known_aspect("recruitment"));
	well_known_aspects.push_back(well_known_aspect("recruitment_ignore_bad_combat"));
	well_known_aspects.push_back(well_known_aspect("recruitment_ignore_bad_movement"));
	well_known_aspects.push_back(well_known_aspect("recruitment_pattern"));
	well_known_aspects.push_back(well_known_aspect("scout_village_targeting"));
	well_known_aspects.push_back(well_known_aspect("simple_targeting"));
	well_known_aspects.push_back(well_known_aspect("support_villages"));
	well_known_aspects.push_back(well_known_aspect("village_value"));
	well_known_aspects.push_back(well_known_aspect("villages_per_scout"));

	const config &ais = game_config.child("ais");
	default_config_ = ais.child("default_config");
	if (!default_config_) {
		ERR_AI_CONFIGURATION << "Missing AI [default_config]. Therefore, default_config_ set to empty." << std::endl;
		default_config_ = config();
	}


	foreach (const config &ai_configuration, ais.child_range("ai")) {
		const std::string &id = ai_configuration["id"];
		if (id.empty()){

			ERR_AI_CONFIGURATION << "skipped AI config due to missing id" << ". Config contains:"<< std::endl << ai_configuration << std::endl;
			continue;
		}
		if (ai_configurations_.count(id)>0){
			ERR_AI_CONFIGURATION << "skipped AI config due to duplicate id [" << id << "]. Config contains:"<< std::endl << ai_configuration << std::endl;
			continue;
		}

		description desc;
		desc.id=id;
		desc.text=ai_configuration["description"];
		desc.cfg=ai_configuration;

		ai_configurations_.insert(std::make_pair(id,desc));
		LOG_AI_CONFIGURATION << "loaded AI config: " << ai_configuration["description"] << std::endl;
	}
}

std::vector<description*> configuration::get_available_ais(){
	std::vector<description*> ais_list;
	for(description_map::iterator desc = ai_configurations_.begin(); desc!=ai_configurations_.end(); ++desc) {
		ais_list.push_back(&desc->second);	
		DBG_AI_CONFIGURATION << "has ai with config: "<< std::endl << desc->second.cfg<< std::endl;
	}
	return ais_list;
}

const config& configuration::get_ai_config_for(const std::string &id)
{
	description_map::iterator cfg_it = ai_configurations_.find(id);
	if (cfg_it==ai_configurations_.end()){
		return default_config_;
	}
	return cfg_it->second.cfg;
}


configuration::description_map configuration::ai_configurations_ = configuration::description_map();
config configuration::default_config_ = config();

bool configuration::get_side_config_from_file(const std::string& file, config& cfg ){
	try {
		scoped_istream stream = preprocess_file(get_wml_location(file));
		read(cfg, *stream);
		LOG_AI_CONFIGURATION << "Reading AI configuration from file '" << file  << "'" << std::endl;
	} catch(config::error &) {
		ERR_AI_CONFIGURATION << "Error while reading AI configuration from file '" << file  << "'" << std::endl;
		return false;
	}
	LOG_AI_CONFIGURATION << "Successfully read AI configuration from file '" << file  << "'" << std::endl;
	return cfg;//in boolean context
}

const config& configuration::get_default_ai_parameters()
{
	return default_config_;
}


bool configuration::upgrade_aspect_config_from_1_07_02_to_1_07_03(side_number /*side*/, const config& cfg, config& parsed_cfg, const std::string &id, bool aspect_was_attribute)
{
	config aspect_config;
	aspect_config["id"] = id;

	foreach (const config &aiparam, cfg.child_range("ai")) {
		const config &_aspect = aiparam.find_child("aspect","id",id);
		if (_aspect) {
			aspect_config.append(_aspect);
		}

		if (aspect_was_attribute && !aiparam.has_attribute(id)) {
			continue;//we are looking for an attribute but it isn't present
		}

		if (!aspect_was_attribute && !aiparam.child(id)) {
			continue;//we are looking for a child but it isn't present
		}

		config facet_config;
		facet_config["engine"] = "cpp";
		facet_config["name"] = "standard_aspect";
		if (aspect_was_attribute) {
			facet_config["value"] = aiparam[id];
		} else {
			foreach (const config &value, aiparam.child_range(id)) {
				facet_config.add_child("value",value);
			}
		}

		if (aiparam.has_attribute("turns")) {
			facet_config["turns"] = aiparam["turns"];
		}
		if (aiparam.has_attribute("time_of_day")) {
			facet_config["time_of_day"] = aiparam["time_of_day"];
		}

		aspect_config.add_child("facet",facet_config);
	}

	parsed_cfg.add_child("aspect",aspect_config);
	return parsed_cfg;//in boolean context
}


bool configuration::parse_side_config(side_number side, const config& original_cfg, config &cfg )
{
	LOG_AI_CONFIGURATION << "side "<< side <<": parsing AI configuration from config" << std::endl;

	//leave only the [ai] children
	cfg = config();
	foreach (const config &aiparam, original_cfg.child_range("ai")) {
		cfg.add_child("ai",aiparam);
	}
	DBG_AI_CONFIGURATION << "side " << side << ": config contains:"<< std::endl << cfg << std::endl;

	//insert default config at the beginning
	if (default_config_) {
		DBG_AI_CONFIGURATION << "side "<< side <<": applying default configuration" << std::endl;
		cfg.add_child_at("ai",default_config_,0);
	} else {
		ERR_AI_CONFIGURATION << "side "<< side <<": default configuration is not available, do not applying it" << std::endl;
	}

	//find version
	int version = 10600;
	foreach (const config &aiparam, cfg.child_range("ai")) {
		if (aiparam.has_attribute("version")){
			int v = boost::lexical_cast<int>(aiparam["version"]);//@todo 1.7 parse version from more readable form, such as "1.7.3"
			if (version<v) {
				version = v;
			}
		}
	}

	if (version<10703) {
		if (!upgrade_side_config_from_1_07_02_to_1_07_03(side, cfg)) {
			return false;
		}
		version = 10703;
	}

	//construct new-style integrated config
	LOG_AI_CONFIGURATION << "side "<< side << ": doing final operations on AI config"<< std::endl;
	config parsed_cfg = config();

	LOG_AI_CONFIGURATION << "side "<< side <<": merging AI configurations"<< std::endl;
	foreach (const config &aiparam, cfg.child_range("ai")) {
		parsed_cfg.append(aiparam);
	}

	LOG_AI_CONFIGURATION << "side "<< side <<": setting config version to "<< version << std::endl;
	parsed_cfg["version"] = boost::lexical_cast<std::string>( version );


	LOG_AI_CONFIGURATION << "side "<< side <<": merging AI aspect with the same id"<< std::endl;
	parsed_cfg.merge_children_by_attribute("aspect","id");

	LOG_AI_CONFIGURATION << "side "<< side <<": removing duplicate [default] tags from aspects"<< std::endl;
	foreach (config &aspect_cfg, parsed_cfg.child_range("aspect")) {
		if (!aspect_cfg.child("default")) {
			WRN_AI_CONFIGURATION << "side "<< side <<": aspect with id=["<<aspect_cfg["id"]<<"] lacks default config facet!" <<std::endl;
			continue;
		}
		config c = aspect_cfg.child("default");
		aspect_cfg.clear_children("default");
		aspect_cfg.add_child("default",c);
	}

	DBG_AI_CONFIGURATION << "side "<< side <<": done parsing side config, it contains:"<< std::endl << parsed_cfg << std::endl;
	LOG_AI_CONFIGURATION << "side "<< side <<": done parsing side config"<< std::endl;

	cfg = parsed_cfg;
	return cfg;//in boolean context

}


bool configuration::upgrade_side_config_from_1_07_02_to_1_07_03(side_number side, config &cfg)
{
	LOG_AI_CONFIGURATION << "side "<< side <<": upgrading ai config version from version 1.7.2 to 1.7.3"<< std::endl;
	config parsed_cfg;

	//get values of all aspects
	upgrade_aspect_configs_from_1_07_02_to_1_07_03(side, cfg.child_range("ai"), parsed_cfg);

	//dump the rest of the config into fallback stage

	config fallback_stage_cfg;
	fallback_stage_cfg["engine"] = "cpp";
	fallback_stage_cfg["name"] = "testing_ai_default::fallback";
	fallback_stage_cfg["id"] = "fallback";
	config fallback_stage_cfg_ai;

	foreach (config &aiparam, cfg.child_range("ai")) {
		if (!aiparam.has_attribute("turns") && !aiparam.has_attribute("time_of_day")) {
			foreach (const well_known_aspect &wka, well_known_aspects) {
				if (wka.was_an_attribute_) {
					aiparam.remove_attribute(wka.name_);
				} else {
					aiparam.clear_children(wka.name_);
				}
			}
			fallback_stage_cfg_ai.append(aiparam);
		}

		foreach (const config &aitarget, aiparam.child_range("target")) {
			config aigoal;
			if (aiparam.has_attribute("turns")) {
				aigoal["turns"] = aiparam["turns"];
			}
			if (aiparam.has_attribute("time_of_day")) {
				aigoal["time_of_day"] = aiparam["time_of_day"];
			}
			if (aitarget.has_attribute("value")) {
				aigoal["value"] = aitarget["value"];
			} else {
				aigoal["value"] = "0";
			}

			config &aigoalcriteria = aigoal.add_child("criteria",aitarget);
			aigoalcriteria.remove_attribute("value");

			parsed_cfg.add_child("goal",aigoal);
		}
		aiparam.clear_children("target");
	}
	fallback_stage_cfg_ai.clear_children("aspect");//@todo 1.7: cleanup only configs of well-known ai aspects

	//move [stage]s to root of the config
	foreach (const config &aistage, fallback_stage_cfg_ai.child_range("stage")) {
		parsed_cfg.add_child("stage",aistage);
	}
	fallback_stage_cfg_ai.clear_children("stage");

	//move [goal] to root of the config
	foreach (const config &aigoal, fallback_stage_cfg_ai.child_range("goal")) {
		parsed_cfg.add_child("goal",aigoal);
	}
	fallback_stage_cfg_ai.clear_children("goal");

	//move [modify_ai]'s to root of the config
	foreach (const config &aimodifyai, fallback_stage_cfg_ai.child_range("modify_ai")) {
		parsed_cfg.add_child("modify_ai",aimodifyai);
	}
	fallback_stage_cfg_ai.clear_children("modify_ai");

	fallback_stage_cfg.add_child("ai",fallback_stage_cfg_ai);

	parsed_cfg.add_child("stage",fallback_stage_cfg);

	cfg = config();
	cfg.add_child("ai",parsed_cfg);
	DBG_AI_CONFIGURATION << "side "<< side <<": after upgrade to 1.7.3 syntax, config contains:"<< std::endl << cfg << std::endl;
	return cfg;//in boolean context
}


void configuration::upgrade_aspect_configs_from_1_07_02_to_1_07_03(side_number side, const config::const_child_itors &ai_parameters, config &parsed_cfg)
{
	parsed_cfg = config();
	config cfg;

	foreach (const config &aiparam, ai_parameters) {
		cfg.add_child("ai",aiparam);
	}

	DBG_AI_CONFIGURATION << "side "<< side <<": upgrading aspects from syntax of 1.7.2. to 1.7.3, old-style config is:" << std::endl << cfg << std::endl;		
	foreach (const well_known_aspect &wka, well_known_aspects) {
		upgrade_aspect_config_from_1_07_02_to_1_07_03(side, cfg,parsed_cfg,wka.name_,wka.was_an_attribute_);
	}
}

} //end of namespace ai
