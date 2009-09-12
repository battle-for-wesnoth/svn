/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file generate_report.cpp
 * Formatted output of various stats about units and the game.
 * Used for the right sidebar and the top line of the main game-display.
 */

#include "global.hpp"

#include "actions.hpp"
#include "foreach.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "language.hpp"
#include "map.hpp"
#include "marked-up_text.hpp"
#include "reports.hpp"
#include "resources.hpp"
#include "team.hpp"
#include "tod_manager.hpp"

#include <iostream>
#include <ctime>

namespace reports {

report generate_report(TYPE type,
                       std::map<reports::TYPE, std::string> report_contents,
                       const team &current_team, int current_side, int playing_side,
                       const map_location& loc, const map_location& mouseover, const map_location& displayed_unit_hex,
                       const std::set<std::string> &observers,
                       const config& level, bool show_everything)
{
	unit_map &units = *resources::units;
	gamemap map = *resources::game_map;
	std::vector<team> &teams = *resources::teams;

	const unit *u = NULL;

	if((int(type) >= int(UNIT_REPORTS_BEGIN) && int(type) < int(UNIT_REPORTS_END)) || type == POSITION){
		u = get_visible_unit(units, displayed_unit_hex, map, teams, current_team, show_everything);
		if (!u && type != POSITION) {
			return report();
		}
	}

	std::stringstream str;

	switch(type) {
	case UNIT_NAME:
		return report(std::string(1,font::BOLD_TEXT) + u->name(), "", u->name());
	case UNIT_TYPE:
		return report(font::unit_type + u->type_name(), "", u->unit_description());
	case UNIT_RACE:
		return report(font::race + u->race()->name(u->gender()));
	case UNIT_SIDE: {
		std::string flag_icon = teams[u->side() - 1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_colour_index(u->side());
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if(flag_icon.empty()) {
			flag_icon = game_config::flag_icon_image;
		}

		image::locator flag_icon_img(flag_icon, mods);
		return report("", flag_icon_img, teams[u->side() - 1].current_player());
	}
	case UNIT_LEVEL:
		str << u->level();
		break;
	case UNIT_AMLA: {
	  report res;
		const std::vector<std::pair<std::string,std::string> > &amla_icons=u->amla_icons();
	  for(std::vector<std::pair<std::string,std::string> >::const_iterator i=amla_icons.begin();i!=amla_icons.end();i++){
	    res.add_image(i->first,i->second);
	  }
	  return(res);
	}
	case UNIT_TRAITS:
		return report(u->traits_description(), "", u->modification_description("trait"));
	case UNIT_STATUS: {
		std::stringstream unit_status;
		std::stringstream tooltip;
		report res;

		if (map.on_board(displayed_unit_hex) &&
		    u->invisible(displayed_unit_hex, units, teams))
		{
			unit_status << "misc/invisible.png";
			tooltip << _("invisible: ") << _("This unit is invisible. It cannot be seen or attacked by enemy units.");
			res.add_image(unit_status,tooltip);
		}
		if (u->get_state(unit::STATE_SLOWED)) {
			unit_status << "misc/slowed.png";
			tooltip << _("slowed: ") << _("This unit has been slowed. It will only deal half its normal damage when attacking and its movement cost is doubled.");
			res.add_image(unit_status,tooltip);
		}
		if (u->get_state(unit::STATE_POISONED)) {
			unit_status << "misc/poisoned.png";
			tooltip << _("poisoned: ") << _("This unit is poisoned. It will lose 8 HP every turn until it can seek a cure to the poison in a village or from a friendly unit with the 'cures' ability.\n\
\n\
Units cannot be killed by poison alone. The poison will not reduce it below 1 HP.");
			res.add_image(unit_status,tooltip);
		}
		if (u->get_state(unit::STATE_PETRIFIED)) {
			unit_status << "misc/petrified.png";
			tooltip << _("petrified: ") << _("This unit has been petrified. It may not move or attack.");
			res.add_image(unit_status,tooltip);
		}

		return res;
	}
	case UNIT_ALIGNMENT: {
		const std::string &align = unit_type::alignment_description(u->alignment(), u->gender());
		const std::string &align_id = unit_type::alignment_id(u->alignment());
		std::stringstream ss;
		int cm = combat_modifier(units, displayed_unit_hex, u->alignment(), u->is_fearless());
		ss << align << " (" << (cm >= 0 ? "+" : "") << cm << "%)";
		return report(ss.str(), "", string_table[align_id + "_description"]);
	}
	case UNIT_ABILITIES: {
		report res;
		std::stringstream tooltip;
		const std::vector<std::string> &abilities = u->ability_tooltips();
		for(std::vector<std::string>::const_iterator i = abilities.begin(); i != abilities.end(); ++i) {
			str << gettext(i->c_str());
			if(i+2 != abilities.end())
				str << ",";
			++i;
			tooltip << i->c_str(); //string_table[*i + "_description"];
			res.add_text(str,tooltip);
		}

		return res;
	}
	case UNIT_HP: {
		report res;
		std::stringstream tooltip;
		str << font::color2markup(u->hp_color());
		str << u->hitpoints() << "/" << u->max_hitpoints();

		std::set<std::string> resistances_table;

		string_map resistances = u->get_base_resistances();

		bool att_def_diff = false;
		for(string_map::iterator resist = resistances.begin();
				resist != resistances.end(); ++resist) {
			std::stringstream line;
			line << gettext(resist->first.c_str()) << ": ";

			// Some units have different resistances when
			// attacking or defending.
			int res_att = 100 - u->resistance_against(resist->first, true, displayed_unit_hex);
			int res_def = 100 - u->resistance_against(resist->first, false, displayed_unit_hex);
			if (res_att == res_def) {
				line << res_def << "%\n";
			} else {
				line << res_att << "% / " << res_def << "%\n";
				att_def_diff = true;
			}
			resistances_table.insert(line.str());
		}

		tooltip << _("Resistances: ");
		if (att_def_diff)
			tooltip << _("(Att / Def)");
		tooltip << "\n";

		// the STL set will give alphabetical sorting
		for(std::set<std::string>::iterator line = resistances_table.begin();
				line != resistances_table.end(); ++line) {
			tooltip << (*line);
		}

		res.add_text(str,tooltip);
		return res ;
	}
	case UNIT_XP: {
		report res;
		std::stringstream tooltip;

		str << font::color2markup(u->xp_color());
		str << u->experience() << "/" << u->max_experience();

		tooltip << _("Experience Modifier: ") << ((level["experience_modifier"] != "") ? level["experience_modifier"] : "100") << "%";
		res.add_text(str,tooltip);

		return res;
	}
	case UNIT_ADVANCEMENT_OPTIONS: {
		report res;
		const std::map<std::string,std::string> &adv_icons = u->advancement_icons();
		for(std::map<std::string,std::string>::const_iterator i=adv_icons.begin();i!=adv_icons.end();i++){
			res.add_image(i->first,i->second);
		}
		return(res);
	}
	case UNIT_DEFENSE: {
		const t_translation::t_terrain terrain = map[displayed_unit_hex];
		int def = 100 - u->defense_modifier(terrain);
		SDL_Color color = int_to_color(game_config::red_to_green(def));
		str << font::color2markup(color);
		str << def << "%";
		break;
	}
	case UNIT_MOVES: {
		float movement_frac = 1.0;
		if (u->side() == playing_side) {
			movement_frac = static_cast<float>(u->movement_left()) / std::max(1.0f, static_cast<float>(u->total_movement()));
			if (movement_frac > 1.0)
				movement_frac = 1.0;
		}

		int grey = 128 + static_cast<int>((255-128) * movement_frac);
		str << "<" << grey << "," << grey << "," << grey <<">";
		str << u->movement_left() << "/" << u->total_movement();
		break;
	}
	case UNIT_WEAPONS: {
		report res;
		std::stringstream tooltip;

		size_t team_index = u->side() - 1;
		if(team_index >= teams.size()) {
			std::cerr << "illegal team index in reporting: " << team_index << "\n";
			return res;
		}

		foreach (const attack_type &at, u->attacks())
		{
			at.set_specials_context(displayed_unit_hex, map_location(), *u);
			std::string lang_type = gettext(at.type().c_str());
			str.str("");
			str << font::weapon;
			if (u->get_state(unit::STATE_SLOWED)) {
				str << round_damage(at.damage(), 1, 2) << '-';
			} else {
				str << at.damage() << '-';
			}
			int nattacks = at.num_attacks();
			// Compute swarm attacks:
			unit_ability_list swarm = at.get_specials("swarm");
			if(!swarm.empty()) {
				int swarm_max_attacks = swarm.highest("swarm_attacks_max",nattacks).first;
				int swarm_min_attacks = swarm.highest("swarm_attacks_min").first;
				int hitp = u->hitpoints();
				int mhitp = u->max_hitpoints();

				nattacks = swarm_min_attacks + (swarm_max_attacks - swarm_min_attacks) * hitp / mhitp;

			}
			str << nattacks;
			str << ' ' << at.name() << ' ' << at.accuracy_parry_description();
			tooltip << at.name() << "\n";
			int effdmg;
			if (u->get_state(unit::STATE_SLOWED)) {
				effdmg = round_damage(at.damage(),1,2);
			} else {
				effdmg = at.damage();
			}
			tooltip << effdmg   << ' ' << _n("tooltip^damage", "damage",  effdmg) << ", ";
			tooltip << nattacks << ' ' << _n("tooltip^attack", "attacks", nattacks);

			int accuracy = at.accuracy();
			if(accuracy) {
				// Help xgettext with a directive to recognise the string as a non C printf-like string
				// xgettext:no-c-format
				tooltip << " " << (accuracy > 0 ? "+" : "") << accuracy << _("tooltip^% accuracy");
			}

			int parry = at.parry();
			if(parry) {
				// xgettext:no-c-format
				tooltip << " " << (parry > 0 ? "+" : "") << parry << _("tooltip^% parry");
			}

			str<<"\n";
			res.add_text(str,tooltip);

			str << font::weapon_details << "  ";
			std::string range = gettext(at.range().c_str());
			str << range << "--" << lang_type << "\n";
			str<<"\n";

			tooltip << _("weapon range: ") << range <<"\n";
			tooltip << _("damage type: ")  << lang_type << "\n";
			// Find all the unit types on the map, and
			// show this weapon's bonus against all the different units.
			// Don't show invisible units, except if they are in our team or allied.
			std::set<std::string> seen_units;
			std::map<int,std::vector<std::string> > resistances;
			for(unit_map::const_iterator u_it = units.begin(); u_it != units.end(); ++u_it) {
				if(teams[team_index].is_enemy(u_it->second.side()) &&
				   !current_team.fogged(u_it->first) &&
				   seen_units.count(u_it->second.type_id()) == 0 &&
				   ( !current_team.is_enemy(u_it->second.side()) ||
				     !u_it->second.invisible(u_it->first,units,teams)))
				{
					seen_units.insert(u_it->second.type_id());
					int resistance = u_it->second.resistance_against(at, false, u_it->first) - 100;
					resistances[resistance].push_back(u_it->second.type_name());
				}
			}

			for(std::map<int,std::vector<std::string> >::reverse_iterator resist = resistances.rbegin(); resist != resistances.rend(); ++resist) {
				std::sort(resist->second.begin(),resist->second.end());
				tooltip << (resist->first >= 0 ? "+" : "") << resist->first << "% " << _("vs") << " ";
				for(std::vector<std::string>::const_iterator i = resist->second.begin(); i != resist->second.end(); ++i) {
					if(i != resist->second.begin()) {
						tooltip << ",";
					}

					tooltip << *i;
				}
				tooltip << "\n";
			}

			res.add_text(str,tooltip);


			const std::vector<t_string> &specials = at.special_tooltips();

			if(! specials.empty()) {
				for(std::vector<t_string>::const_iterator sp_it = specials.begin(); sp_it != specials.end(); ++sp_it) {
					str << font::weapon_details << "  ";
					str << (*sp_it);
					str<<"\n";
					++sp_it;
					tooltip << (*sp_it) << "\n";
				}
				res.add_text(str,tooltip);
			}
		}

		return res;
	}
	case UNIT_IMAGE:
	{
//		const std::vector<Uint32>& old_rgb = u->second.team_rgb_range();
//		color_range new_rgb = team::get_side_color_range(u->second.side());
		return report("", image::locator(u->absolute_image(), u->image_mods()), "");
	}
	case UNIT_PROFILE:
		return report("", u->profile(), "");
	case TIME_OF_DAY: {
		time_of_day tod = resources::tod_manager->time_of_day_at(units, mouseover, *resources::game_map);
		const std::string tod_image = tod.image + (preferences::flip_time() ? "~FL(horiz)" : "");

		// Don't show illuminated time on fogged/shrouded tiles
		if (current_team.fogged(mouseover) || current_team.shrouded(mouseover)) {
			tod = resources::tod_manager->get_time_of_day(false, mouseover);
		}
		std::stringstream tooltip;

		tooltip << tod.name << "\n"
				<< _("Lawful units: ")
				<< (tod.lawful_bonus > 0 ? "+" : "") << tod.lawful_bonus << "%\n"
				<< _("Neutral units: ") << "0%\n"
				<< _("Chaotic units: ")
				<< (tod.lawful_bonus < 0 ? "+" : "") << (tod.lawful_bonus*-1) << "%";

		return report("",tod_image,tooltip.str());
	}
	case TURN: {
		str << resources::tod_manager->turn();

		int nb = resources::tod_manager->number_of_turns();
		if (nb != -1) str << '/' << nb;

		str << "\n";
		break;
	}
	// For the following status reports, show them in gray text
	// when it is not the active player's turn.
	case GOLD:
		str << (current_side != playing_side ? font::GRAY_TEXT : (current_team.gold() < 0 ? font::BAD_TEXT : font::NULL_MARKUP)) << current_team.gold();
		break;
	case VILLAGES: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << (current_side != playing_side ? font::GRAY_TEXT : font::NULL_MARKUP) << data.villages << "/";
		if (current_team.uses_shroud()) {
			int unshrouded_villages = 0;
			std::vector<map_location>::const_iterator i = map.villages().begin();
			for (; i != map.villages().end(); i++) {
				if (!current_team.shrouded(*i))
					unshrouded_villages++;
			}
			str << unshrouded_villages;
		} else {
			str << map.villages().size();
		}
		break;
	}
	case NUM_UNITS: {
		str << (current_side != playing_side ? font::GRAY_TEXT : font::NULL_MARKUP) << side_units(units, current_side);
		break;
	}
	case UPKEEP: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << (current_side != playing_side ? font::GRAY_TEXT : font::NULL_MARKUP) << data.expenses << " (" << data.upkeep << ")";
		break;
	}
	case EXPENSES: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << (current_side != playing_side ? font::GRAY_TEXT : font::NULL_MARKUP) << data.expenses;
		break;
	}
	case INCOME: {
		const team_data data = calculate_team_data(current_team,current_side,units);
		str << (current_side != playing_side ? font::GRAY_TEXT : (data.net_income < 0 ? font::BAD_TEXT : font::NULL_MARKUP)) << data.net_income;
		break;
	}
	case TERRAIN: {
		if(!map.on_board(mouseover) || current_team.shrouded(mouseover))
			break;

		const t_translation::t_terrain terrain = map.get_terrain(mouseover);
		if (terrain == t_translation::OFF_MAP_USER)
			break;

		const t_translation::t_list& underlying = map.underlying_union_terrain(terrain);

		if(map.is_village(mouseover)) {
			int owner = village_owner(mouseover, teams) + 1;
			if(owner == 0 || current_team.fogged(mouseover)) {
				str << map.get_terrain_info(terrain).income_description();
			} else if(owner == current_side) {
				str << map.get_terrain_info(terrain).income_description_own();
			} else if(current_team.is_enemy(owner)) {
				str << map.get_terrain_info(terrain).income_description_enemy();
			} else {
				str << map.get_terrain_info(terrain).income_description_ally();
			}
			str << " ";
		} else {
		        str << map.get_terrain_info(terrain).description();
		}

		if(underlying.size() != 1 || underlying.front() != terrain) {
			str << " (";

			for(t_translation::t_list::const_iterator i =
					underlying.begin(); i != underlying.end(); ++i) {

			str << map.get_terrain_info(*i).name();
				if(i+1 != underlying.end()) {
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

		const t_translation::t_terrain terrain = map[mouseover];

		if (terrain == t_translation::OFF_MAP_USER)
			break;

		str << mouseover;

		if (!u)
			break;
		if(displayed_unit_hex != mouseover && displayed_unit_hex != loc)
			break;
		if(current_team.shrouded(mouseover))
			break;

		int move_cost = u->movement_cost(terrain);
		int defense = 100 - u->defense_modifier(terrain);

		if(move_cost < unit_movement_type::UNREACHABLE) {
			str << " (" << defense << "%," << move_cost << ")";
		} else if (mouseover == displayed_unit_hex) {
			str << " (" << defense << "%,-)";
		} else {
			str << " (-)";
		}

		break;
	}

	case SIDE_PLAYING: {
		std::string flag_icon = teams[playing_side-1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_colour_index(playing_side);
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if(flag_icon.empty()) {
			flag_icon = game_config::flag_icon_image;
		}

		image::locator flag_icon_img(flag_icon, mods);
		return report("",flag_icon_img,teams[playing_side-1].current_player());
	}

	case OBSERVERS: {
		if(observers.empty()) {
			return report();
		}

		str << _("Observers:") << "\n";

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
	case REPORT_COUNTDOWN: {
		int min;
		int sec;
		if (current_team.countdown_time() > 0){
			sec = current_team.countdown_time() / 1000;

			str << (current_side != playing_side ? font::GRAY_TEXT : font::NORMAL_TEXT);

			if(sec < 60)
				str << "<200,0,0>";
			else if(sec < 120)
				str << "<200,200,0>";

			min = sec / 60;
			str << min << ":";
			sec = sec % 60;
			if (sec < 10) {
				str << "0";
			}
			str << sec;
			break;
		} // Intentional fall-through to REPORT_CLOCK
		  // if the time countdown isn't valid.
		  // If there is no turn time limit,
		  // then we display the clock instead.
		}
	case REPORT_CLOCK: {
		time_t t = std::time(NULL);
		struct tm *lt = std::localtime(&t);
		if (lt) {
			char temp[10];
			size_t s = std::strftime(temp, 10, preferences::clock_format().c_str(), lt);
			if(s>0) {
				return report(temp);
			} else {
				return report();
			}
		} else {
			return report();
		}
	}
	default:
		assert(false);
		break;
	}
	return report(str.str());
}

} // end namespace reports

