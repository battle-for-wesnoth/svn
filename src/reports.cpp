#include "actions.hpp"
#include "font.hpp"
#include "game_config.hpp"
#include "language.hpp"
#include "reports.hpp"

#include <cassert>
#include <map>
#include <set>
#include <sstream>

namespace {
	const std::string report_names[] = { "unit_description", "unit_type", "unit_level",
		"unit_traits","unit_status","unit_alignment","unit_abilities","unit_hp","unit_xp",
		"unit_moves","unit_weapons","unit_image","unit_profile","time_of_day",
		"turn","gold","villages","num_units","upkeep", "expenses",
		 "income", "terrain", "position", "side_playing", "observers", "selected_terrain",
		 "edit_left_button_function"};
	std::map<reports::TYPE, std::string> report_contents;
}

namespace reports {

const std::string& report_name(TYPE type)
{
	assert(sizeof(report_names)/sizeof(*report_names) == NUM_REPORTS);
	assert(type < NUM_REPORTS);

	return report_names[type];
}

void report::add_text(std::stringstream& text, std::stringstream& tooltip) {
	add_text(text.str(), tooltip.str());
	// Clear the streams
	text.str("");
	tooltip.str("");
}

void report::add_text(const std::string& text, const std::string& tooltip) {
	this->push_back(element(text,"",tooltip));
}

void report::add_image(std::stringstream& image, std::stringstream& tooltip) {
	add_image(image.str(), tooltip.str());
	// Clear the streams
	image.str("");
	tooltip.str("");
}

void report::add_image(const std::string& image, const std::string& tooltip) {
	this->push_back(element("",image,tooltip));
}

report generate_report(TYPE type, const gamemap& map, const unit_map& units,
						const std::vector<team>& teams,
                  const team& current_team, int current_side, int playing_side,
					   const gamemap::location& loc, const gamemap::location& mouseover,
					   const gamestatus& status, const std::set<std::string>& observers,
					   const std::string* format_string)
{
	unit_map::const_iterator u = units.end();
	
	if(int(type) >= int(UNIT_REPORTS_BEGIN) && int(type) < int(UNIT_REPORTS_END) || type == POSITION) {

		if(!current_team.fogged(mouseover.x,mouseover.y)) {
			u = find_visible_unit(units,mouseover,
					map,
					status.get_time_of_day().lawful_bonus,
					teams,current_team);
		}

		if(u == units.end()) {
			if(!current_team.fogged(loc.x,loc.y)) {
				u = find_visible_unit(units,loc,
						map,
						status.get_time_of_day().lawful_bonus,
						teams,current_team);
			}

			if(u == units.end() && type != POSITION) {
				return report();
			}
		}
	}

	std::stringstream str;

	switch(type) {
	case UNIT_DESCRIPTION:
		return report(u->second.description());
	case UNIT_TYPE:
		return report(u->second.type().language_name(),"",u->second.unit_description());
	case UNIT_LEVEL:
		str << u->second.type().level();
		break;
	case UNIT_TRAITS:
		return report(u->second.traits_description(),"",u->second.modification_description("trait"));
	case UNIT_STATUS: {
		std::stringstream unit_status;
		std::stringstream tooltip;
		report res;

		if(map.on_board(loc) && u->second.invisible(map.underlying_terrain(map[loc.x][loc.y]),status.get_time_of_day().lawful_bonus,loc,units,teams)) {
			unit_status << "misc/invisible.png";
			tooltip << string_table["invisible"] << ": " << string_table["invisible_description"];
			res.add_image(unit_status,tooltip);
		}
		if(u->second.has_flag("slowed")) {
			unit_status << "misc/slowed.png";
			tooltip << string_table["slowed"] << ": " << string_table["slowed_description"];
			res.add_image(unit_status,tooltip);
		}
		if(u->second.has_flag("poisoned")) {
			unit_status << "misc/poisoned.png";
			tooltip << string_table["poisoned"] << ": " << string_table["poisoned_description"];
			res.add_image(unit_status,tooltip);
		}
		if(u->second.has_flag("stone")) {
			unit_status << "misc/stone.png";
			tooltip << string_table["stone"] << ": " << string_table["stone_description"];
			res.add_image(unit_status,tooltip);
		}

		return res;
	}
	case UNIT_ALIGNMENT: {
		const std::string& align = unit_type::alignment_description(u->second.type().alignment());
		return report(translate_string(align),"",string_table[align + "_description"]);
	}
	case UNIT_ABILITIES: {
		report res;
		std::stringstream tooltip;
		const std::vector<std::string>& abilities = u->second.type().abilities();
		for(std::vector<std::string>::const_iterator i = abilities.begin(); i != abilities.end(); ++i) {
			str << translate_string(*i);
			if(i+1 != abilities.end())
				str << ",";

			tooltip << string_table[*i + "_description"];
			res.add_text(str,tooltip);
		}

		return res;
	}
	case UNIT_HP:
		if(u->second.hitpoints() <= u->second.max_hitpoints()/3)
			str << font::BAD_TEXT;
		else if(u->second.hitpoints() > 2*(u->second.max_hitpoints()/3))
			str << font::GOOD_TEXT;

		str << u->second.hitpoints()
		    << "/" << u->second.max_hitpoints() << "\n";

		break;
	case UNIT_XP:
		if(u->second.type().advances_to().empty()) {
			str << u->second.experience() << "/-";
		} else {
			//if killing a unit of the same level as us lets us advance, display in 'good' colour
			if(u->second.max_experience() - u->second.experience() <= game_config::kill_experience*u->second.type().level()) {
				str << font::GOOD_TEXT;
			}

			str << u->second.experience() << "/" << u->second.max_experience();
		}

		break;
	case UNIT_MOVES:
		str << u->second.movement_left() << "/" << u->second.total_movement();
		break;
	case UNIT_WEAPONS: {
		report res;
		std::stringstream tooltip;
		
		const size_t team_index = u->second.side()-1;
		if(team_index >= teams.size()) {
			std::cerr << "illegal team index in reporting: " << team_index << "\n";
			return res;
		}

		const std::vector<attack_type>& attacks = u->second.attacks();
		for(std::vector<attack_type>::const_iterator at_it = attacks.begin();
		    at_it != attacks.end(); ++at_it) {
			const std::string& lang_weapon = string_table["weapon_name_" + at_it->name()];
			const std::string& lang_type = string_table["weapon_type_" + at_it->type()];
			const std::string& lang_special = string_table["weapon_special_" + at_it->special()];
			
			str << (lang_weapon.empty() ? at_it->name():lang_weapon) << " ("
				<< (lang_type.empty() ? at_it->type():lang_type) << ")\n";

			tooltip << (lang_weapon.empty() ? at_it->name():lang_weapon) << "\n";

			//find all the unit types on the map, and show this weapon's bonus against all the different units
			std::set<const unit_type*> seen_units;
			std::map<int,std::vector<std::string> > resistances;
			for(unit_map::const_iterator u_it = units.begin(); u_it != units.end(); ++u_it) {
				if(teams[team_index].is_enemy(u_it->second.side()) && !current_team.fogged(u_it->first.x,u_it->first.y) &&
				   seen_units.count(&u_it->second.type()) == 0) {
					seen_units.insert(&u_it->second.type());
					const int resistance = u_it->second.type().movement_type().resistance_against(*at_it) - 100;
					resistances[resistance].push_back(translate_string(u_it->second.type().name()));
				}
			}

			for(std::map<int,std::vector<std::string> >::reverse_iterator resist = resistances.rbegin(); resist != resistances.rend(); ++resist) {
				std::sort(resist->second.begin(),resist->second.end());
				tooltip << (resist->first >= 0 ? "+" : "") << resist->first << "% " << string_table["versus"] << " ";
				for(std::vector<std::string>::const_iterator i = resist->second.begin(); i != resist->second.end(); ++i) {
					if(i != resist->second.begin()) {
						tooltip << ",";
					}

					tooltip << *i;
				}

				tooltip << "\n";
			}

			res.add_text(str,tooltip);
			
			str << (lang_special.empty() ? at_it->special():lang_special) << "\n";
			tooltip << string_table["weapon_special_" + at_it->special() + "_description"];
			res.add_text(str,tooltip);
			
			str << at_it->damage() << "-" << at_it->num_attacks() << " -- "
		        << (at_it->range() == attack_type::SHORT_RANGE ?
		            string_table["short_range"] :
					string_table["long_range"]);
			tooltip << at_it->damage() << " " << string_table["damage"] << ", "
					<< at_it->num_attacks() << " " << string_table["attacks"];
			
			if(at_it->hexes() > 1) {
				str << " (" << at_it->hexes() << ")";
				tooltip << ", " << at_it->hexes() << " " << string_table["hexes"];
			}
			str << "\n";
			res.add_text(str,tooltip);
		}

		return res;
	}
	case UNIT_IMAGE:
		return report("",u->second.type().image(),"");
	case UNIT_PROFILE:
		return report("",u->second.type().image_profile(),"");
	case TIME_OF_DAY: {
		const time_of_day& tod = timeofday_at(status,units,mouseover);
		std::stringstream tooltip;
		
		tooltip << font::LARGE_TEXT << translate_string_default(tod.id,tod.name) << "\n"
		        << string_table["lawful_units"] << ": "
				<< (tod.lawful_bonus > 0 ? "+" : "") << tod.lawful_bonus << "%\n"
				<< string_table["neutral_units"] << ": " << "0%\n"
				<< string_table["chaotic_units"] << ": "
				<< (tod.lawful_bonus < 0 ? "+" : "") << (tod.lawful_bonus*-1) << "%";
		
		return report("",tod.image,tooltip.str());
	}
	case TURN:
		str << status.turn();

		if(status.number_of_turns() != -1) {
			str << "/" << status.number_of_turns();
		}

		str << "\n";
		break;
	case GOLD:
		str << (current_team.gold() < 0 ? font::BAD_TEXT : font::NULL_MARKUP) << current_team.gold();
		break;
	case VILLAGES: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << data.villages;

		break;
	}
	case NUM_UNITS: {
		str << team_units(units,current_side);
		break;
	}
	case UPKEEP: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << data.expenses << " (" << data.upkeep << ")";
		break;
	}
	case EXPENSES: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << data.expenses;
		break;
	}
	case INCOME: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << (data.net_income < 0 ? font::BAD_TEXT : font::NULL_MARKUP) << data.net_income;
		break;
	}
	case TERRAIN: {
		if(!map.on_board(mouseover) || current_team.shrouded(mouseover.x,mouseover.y))
			break;

		const gamemap::TERRAIN terrain = map.get_terrain(mouseover);
		const std::string& name = map.terrain_name(terrain);
		const std::vector<std::string>& underlying_names = map.underlying_terrain_name(terrain);

		if(map.is_village(mouseover)) {
			const int owner = village_owner(mouseover,teams)+1;
			if(owner == 0) {
			} else if(owner == current_side) {
				str << translate_string("owned");
			} else if(current_team.is_enemy(owner)) {
				str << translate_string("enemy");
			} else {
				str << translate_string("ally");
			}

			str << " ";
		}

		const std::string& translated_name = translate_string(name);

		str << translated_name;

		if(underlying_names.size() != 1 || translate_string(underlying_names.front()) != translated_name) {
			str << " (";
			
			for(std::vector<std::string>::const_iterator i = underlying_names.begin(); i != underlying_names.end(); ++i) {
				str << translate_string(*i);
				if(i+1 != underlying_names.end()) {
					str << ",";
				}
			}

			str << ")";
		}

		break;
	}
	case POSITION: {
		if(!map.on_board(mouseover)) {
			break;
		}

		str << (mouseover.x+1) << ", " << (mouseover.y+1);

		if(u == units.end() || current_team.shrouded(mouseover.x,mouseover.y))
			break;

		const gamemap::TERRAIN terrain = map[mouseover.x][mouseover.y];

		const int move_cost = u->second.movement_cost(map,terrain);
		const int defense = 100 - u->second.defense_modifier(map,terrain);

		if(move_cost > 10) {
			str << " (-)";
		} else {
			str << " (" << defense << "%," << move_cost << ")";
		}

		break;	
	}

	case SIDE_PLAYING: {
		char buf[50];
		sprintf(buf,"terrain/flag-team%d.png",playing_side);

		u = find_leader(units,playing_side);
		return report("",buf,u != units.end() ? u->second.description() : "");
	}

	case OBSERVERS: {
		if(observers.empty()) {
			return report();
		}

		str << translate_string("observers") << ":\n";

		for(std::set<std::string>::const_iterator i = observers.begin(); i != observers.end(); ++i) {
			str << *i << "\n";
		}

		return report("",game_config::observer_image,str.str());
	}
	case SELECTED_TERRAIN: {
		std::map<TYPE, std::string>::const_iterator it =
			report_contents.find(SELECTED_TERRAIN);
		if (it != report_contents.end()) {
			return report(it->second);
		}
		else {
			return report();
		}
	}
	case EDIT_LEFT_BUTTON_FUNCTION: {
		std::map<TYPE, std::string>::const_iterator it =
			report_contents.find(EDIT_LEFT_BUTTON_FUNCTION);
		if (it != report_contents.end()) {
			return report(it->second);
		}
		else {
			return report();
		}
	}
		
	}
	return report(str.str());
}
	
	
void set_report_content(const TYPE which_report, const std::string &content) {
	report_contents[which_report] = content;
}
	

}
