#include "actions.hpp"
#include "game_config.hpp"
#include "language.hpp"
#include "reports.hpp"

#include <cassert>
#include <sstream>

namespace {
	const std::string report_names[] = { "unit_description", "unit_type", "unit_level",
		"unit_traits","unit_status","unit_alignment","unit_hp","unit_xp","unit_moves",
		"unit_weapons","unit_image","unit_profile","time_of_day",
		"turn","gold","villages","num_units","upkeep", "expenses",
		"income", "terrain", "position" };
}

namespace reports {

const std::string& report_name(TYPE type)
{
	assert(sizeof(report_names)/sizeof(*report_names) == NUM_REPORTS);
	assert(type < NUM_REPORTS);

	return report_names[type];
}

report generate_report(TYPE type, const gamemap& map, const unit_map& units,
                       const team& current_team, int current_side,
					   const gamemap::location& loc, const gamemap::location& mouseover,
					   const gamestatus& status, const std::string* format_string)
{
	unit_map::const_iterator u = units.end();
	
	if(type >= UNIT_REPORTS_BEGIN && type < UNIT_REPORTS_END) {
		u = units.find(mouseover);
		if(u == units.end()) {
			u = units.find(loc);
			if(u == units.end()) {
				return report();
			}
		}
	}

	std::stringstream str;

	switch(type) {
	case UNIT_DESCRIPTION:
		return report(u->second.description());
	case UNIT_TYPE:
		return report(u->second.type().language_name());
	case UNIT_LEVEL:
		str << u->second.type().level();
		break;
	case UNIT_TRAITS:
		return report(u->second.traits_description());
	case UNIT_STATUS:
		if(map.on_board(loc) && u->second.invisible(map.underlying_terrain(map[loc.x][loc.y])))
			return report("@" + string_table["invisible"]);
		else if(u->second.has_flag("slowed"))
			return report("#" + string_table["slowed"]);
		else if(u->second.has_flag("poisoned"))
			return report("#" + string_table["poisoned"]);
		else
			return report(string_table["healthy"]);
	case UNIT_ALIGNMENT:
		return report(unit_type::alignment_description(u->second.type().alignment()));
	case UNIT_HP:
		if(u->second.hitpoints() <= u->second.max_hitpoints()/3)
			str << "#";
		else if(u->second.hitpoints() > 2*(u->second.max_hitpoints()/3))
			str << "@";

		str << u->second.hitpoints()
		    << "/" << u->second.max_hitpoints() << "\n";

		break;
	case UNIT_XP:
		if(u->second.type().advances_to().empty()) {
			str << u->second.experience() << "/-";
		} else {
			//if killing a unit of the same level as us lets us advance, display in 'good' colour
			if(u->second.max_experience() - u->second.experience() <= game_config::kill_experience*u->second.type().level()) {
				str << "@";
			}

			str << u->second.experience() << "/" << u->second.max_experience();
		}

		break;
	case UNIT_MOVES:
		str << u->second.movement_left() << "/" << u->second.total_movement();
		break;
	case UNIT_WEAPONS: {
		const std::vector<attack_type>& attacks = u->second.attacks();
		for(std::vector<attack_type>::const_iterator at_it = attacks.begin();
		    at_it != attacks.end(); ++at_it) {
			const std::string& lang_weapon = string_table["weapon_name_" + at_it->name()];
			const std::string& lang_type = string_table["weapon_type_" + at_it->type()];
			const std::string& lang_special = string_table["weapon_special_" + at_it->special()];
			str << (lang_weapon.empty() ? at_it->name():lang_weapon) << " ("
				<< (lang_type.empty() ? at_it->type():lang_type) << ")\n"
				<< (lang_special.empty() ? at_it->special():lang_special) << "\n"
				<< at_it->damage() << "-" << at_it->num_attacks() << " -- "
		        << (at_it->range() == attack_type::SHORT_RANGE ?
		            string_table["short_range"] :
					string_table["long_range"]);
			
			if(at_it->hexes() > 1) {
				str << " (" << at_it->hexes() << ")";
			}

			str << "\n";
		}

		break;
	}
	case UNIT_IMAGE:
		return report("",u->second.type().image());
	case UNIT_PROFILE:
		return report("",u->second.type().image_profile());
	case TIME_OF_DAY:
		return report("",timeofday_at(status,units,mouseover).image);
	case TURN:
		str << status.turn() << "/" << status.number_of_turns() << "\n";
		break;
	case GOLD:
		str << current_team.gold();
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
		str << team_upkeep(units,current_side);
		break;
	}
	case EXPENSES: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << data.expenses;
		break;
	}
	case INCOME: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << data.net_income;
		break;
	}
	case TERRAIN: {
		if(!map.on_board(mouseover) || current_team.shrouded(mouseover.x,mouseover.y))
			break;

		const gamemap::TERRAIN terrain = map.get_terrain(mouseover);
		const std::string& name = map.terrain_name(terrain);
		const std::string& underlying_name = map.underlying_terrain_name(terrain);

		str << translate_string(name);

		if(underlying_name != name) {
			str << " (" << translate_string(underlying_name) << ")";
		}

		break;
	}
	case POSITION: {
		if(!map.on_board(mouseover))
			break;

		str << (mouseover.x+1) << ", " << (mouseover.y+1);

		u = units.find(mouseover);
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

	}

	return report(str.str());
}

}