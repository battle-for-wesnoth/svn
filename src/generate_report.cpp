/* $Id$ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
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
#include "font.hpp"
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

static void add_status(report &r,
	char const *path, char const *desc1, char const *desc2)
{
	std::ostringstream s;
	s << gettext(desc1) << gettext(desc2);
	r.add_image(path, s.str());
}

static std::string flush(std::ostringstream &s)
{
	std::string r(s.str());
	s.str(std::string());
	return r;
}

static char const *naps = "</span>";

report generate_report(TYPE type,
                       std::map<reports::TYPE, std::string> report_contents,
                       const team &current_team, int current_side, int playing_side,
                       const map_location& loc, const map_location& mouseover, const map_location& displayed_unit_hex,
                       const std::set<std::string> &observers,
                       const config& level, bool show_everything)
{
	unit_map &units = *resources::units;
	gamemap &map = *resources::game_map;
	std::vector<team> &teams = *resources::teams;

	const unit *u = NULL;

	if((int(type) >= int(UNIT_REPORTS_BEGIN) && int(type) < int(UNIT_REPORTS_END)) || type == POSITION){
		u = get_visible_unit(displayed_unit_hex, current_team, show_everything);
		if (!u && type != POSITION) {
			return report();
		}
	}

	std::ostringstream str;
	std::ostringstream tooltip;
	using utils::signed_percent;
	using font::span_color;

	switch(type) {
	case UNIT_NAME:
		str << "<b>" << u->name() << "</b>";

		tooltip << _("Name: ")
			<< "<b>" << u->name() << "</b>";

		return report(str.str(), "", tooltip.str());
	case UNIT_TYPE: {
		str << span_color(font::unit_type_color) << u->type_name() << naps;

		tooltip << _("Type: ")
			<< "<b>" << u->type_name() << "</b>\n"
			<< u->unit_description();

		const std::string help_page = "unit_" + u->type_id();

		return report(str.str(), "", tooltip.str(), help_page);
	}
	case UNIT_RACE: {
		str << span_color(font::race_color)
			<< u->race()->name(u->gender()) << naps;

		tooltip << _("Race: ")
			<< "<b>" << u->race()->name(u->gender()) << "</b>";

		const std::string help_page = "..race_" + u->race()->id();

		return report(str.str(), "", tooltip.str(), help_page);
	}
	case UNIT_SIDE: {
		std::string flag_icon = teams[u->side() - 1].flag_icon();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = team::get_side_color_index(u->side());
		std::string mods = "~RC(" + old_rgb + ">" + new_rgb + ")";

		if(flag_icon.empty()) {
			flag_icon = game_config::flag_icon_image;
		}

		image::locator flag_icon_img(flag_icon, mods);
		return report("", flag_icon_img, teams[u->side() - 1].current_player());
	}
	case UNIT_LEVEL: {
		str << u->level();

		tooltip << _("Level: ")
			<< "<b>" << u->level() << "</b>\n"
			<< _("Advances to:") << "\n"
			<< "<b>";
		const std::vector<std::string>& adv_to = u->advances_to();
		foreach (const std::string& s, adv_to){
			tooltip << "\t" << s << "\n";
		}
		tooltip << "</b>";
		return report(str.str(), "", tooltip.str());
	}
	case UNIT_AMLA: {
	  report res;
		const std::vector<std::pair<std::string,std::string> > &amla_icons=u->amla_icons();
	  for(std::vector<std::pair<std::string,std::string> >::const_iterator i=amla_icons.begin();i!=amla_icons.end();++i){
	    res.add_image(i->first,i->second);
	  }
	  return(res);
	}
	case UNIT_TRAITS: {
		report res;
		const std::vector<t_string>& traits = u->trait_names();
		const std::vector<t_string>& descriptions = u->trait_descriptions();
		unsigned int nb = traits.size();
		for(unsigned int i = 0; i < nb; ++i) {
			str << traits[i];
			if(i != nb - 1 )
				str << ", ";
			tooltip << _("Trait: ")
				<< "<b>" << traits[i] << "</b>\n"
				<< descriptions[i];

			res.add_text(flush(str), flush(tooltip));
		}

		return res;
	}
	case UNIT_STATUS: {
		report res;
		if (map.on_board(displayed_unit_hex) &&
		    u->invisible(displayed_unit_hex))
		{
			add_status(res, "misc/invisible.png", N_("invisible: "),
				N_("This unit is invisible. It cannot be seen or attacked by enemy units."));
		}
		if (u->get_state(unit::STATE_SLOWED)) {
			add_status(res, "misc/slowed.png", N_("slowed: "),
				N_("This unit has been slowed. It will only deal half its normal damage when attacking and its movement cost is doubled."));
		}
		if (u->get_state(unit::STATE_POISONED)) {
			add_status(res, "misc/poisoned.png", N_("poisoned: "),
				N_("This unit is poisoned. It will lose 8 HP every turn until it can seek a cure to the poison in a village or from a friendly unit with the 'cures' ability.\n\nUnits cannot be killed by poison alone. The poison will not reduce it below 1 HP."));
		}
		if (u->get_state(unit::STATE_PETRIFIED)) {
			add_status(res, "misc/petrified.png", N_("petrified: "),
				N_("This unit has been petrified. It may not move or attack."));
		}
		return res;
	}

	case UNIT_ALIGNMENT: {
		const std::string &align = unit_type::alignment_description(u->alignment(), u->gender());
		const std::string &align_id = unit_type::alignment_id(u->alignment());
		int cm = combat_modifier(units, displayed_unit_hex, u->alignment(), u->is_fearless());

		str << align << " (" << signed_percent(cm) << ")";
		tooltip << _("Alignement: ")
			<< "<b>" << align << "</b>\n"
			<< string_table[align_id + "_description"];
		return report(str.str(), "", tooltip.str(), "time_of_day");
	}
	case UNIT_ABILITIES: {
		report res;
		const std::vector<std::string> &abilities = u->ability_tooltips();
		for(std::vector<std::string>::const_iterator i = abilities.begin(); i != abilities.end(); ++i) {
			const std::string& name = gettext(i->c_str());
			str << name;
			if(i+2 != abilities.end())
				str << ", ";
			++i;
			//FIXME pull out ability's name from description
			tooltip << _("Ability:") << "\n"
				<< *i;
			const std::string help_page = "ability_" + name;

			res.add_text(flush(str), flush(tooltip), help_page);
		}

		return res;
	}
	case UNIT_HP: {
		str << span_color(u->hp_color()) << u->hitpoints()
			<< '/' << u->max_hitpoints() << naps;

		std::set<std::string> resistances_table;

		string_map resistances = u->get_base_resistances();

		bool att_def_diff = false;
		for(string_map::iterator resist = resistances.begin();
				resist != resistances.end(); ++resist) {
			std::ostringstream line;
			line << gettext(resist->first.c_str()) << ": ";

			// Some units have different resistances when
			// attacking or defending.
			int res_att = 100 - u->resistance_against(resist->first, true, displayed_unit_hex);
			int res_def = 100 - u->resistance_against(resist->first, false, displayed_unit_hex);
			if (res_att == res_def) {
				line << signed_percent(res_def) << "\n";
			} else {
				line << signed_percent(res_att) << " / " << signed_percent(res_def) << "\n";
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

		return report(str.str(), "", tooltip.str());
	}
	case UNIT_XP: {
		str << span_color(u->xp_color()) << u->experience()
			<< '/' << u->max_experience() << naps;

		tooltip << _("Experience Modifier: ") << (!level["experience_modifier"].empty() ? level["experience_modifier"].str() : "100") << '%';
		return report(str.str(), "", tooltip.str());
	}
	case UNIT_ADVANCEMENT_OPTIONS: {
		report res;
		const std::map<std::string,std::string> &adv_icons = u->advancement_icons();
		for(std::map<std::string,std::string>::const_iterator i=adv_icons.begin();i!=adv_icons.end();++i){
			res.add_image(i->first,i->second);
		}
		return res;
	}
	case UNIT_DEFENSE: {
		const t_translation::t_terrain terrain = map[displayed_unit_hex];
		int def = 100 - u->defense_modifier(terrain);
		SDL_Color color = int_to_color(game_config::red_to_green(def));
		str << span_color(color) << def << "%</span>";


		tooltip << _("Terrain: ")
			<< "<b>" << map.get_terrain_info(terrain).description() << "</b>\n";

		const t_translation::t_list& underlyings = map.underlying_def_terrain(terrain);
		std::vector<int> t_defs;
		bool revert = false;
		if(underlyings.size() != 1 || underlyings.front() != terrain) {
			foreach(const t_translation::t_terrain& t, underlyings) {
				if(t == t_translation::MINUS) {
					revert = true;
				} else if(t == t_translation::PLUS) {
					revert = false;
				} else {
					int t_def = 100 - u->defense_modifier(t);
					SDL_Color color = int_to_color(game_config::red_to_green(t_def));
					tooltip << "\t" << map.get_terrain_info(t).description() << ": "
						<< span_color(color) << t_def << "%</span> "
						<< (revert ? _("maximum^max.") : _("minimum^min.")) << "\n";
				}
			}
		}

		tooltip << "<b>" << _("Defense: ")
			 << span_color(color)  << def << "%</span></b>";

		return report(str.str(), "", tooltip.str());
	}
	case UNIT_MOVES: {
		float movement_frac = 1.0;
		if (u->side() == playing_side) {
			movement_frac = static_cast<float>(u->movement_left()) / std::max(1.0f, static_cast<float>(u->total_movement()));
			if (movement_frac > 1.0)
				movement_frac = 1.0;
		}

		int grey = 128 + static_cast<int>((255-128) * movement_frac);
		SDL_Color c = create_color(grey, grey, grey);
		str << span_color(c) << u->movement_left()
			<< '/' << u->total_movement() << naps;
		break;
	}
	case UNIT_WEAPONS: {
		report res;

		size_t team_index = u->side() - 1;
		if(team_index >= teams.size()) {
			std::cerr << "illegal team index in reporting: " << team_index << "\n";
			return res;
		}

		foreach (const attack_type &at, u->attacks())
		{
			at.set_specials_context(displayed_unit_hex, map_location(), *u);

			int base_damage = at.damage();

			int damage_multiplier = 100;

			// Time of day bonus.
			int tod_bonus = combat_modifier(units, displayed_unit_hex, u->alignment(), u->is_fearless());
			damage_multiplier += tod_bonus;

			// Leadership bonus.
			int leader_bonus = 0;
			if (under_leadership(units, displayed_unit_hex, &leader_bonus).valid())
				damage_multiplier += leader_bonus;

			// assume no specific resistance
			damage_multiplier *= 100;

			bool slowed = u->get_state(unit::STATE_SLOWED);

			int damage_divisor = slowed ? 20000 : 10000;
			int damage = round_damage(base_damage, damage_multiplier, damage_divisor);

			int base_nattacks = at.num_attacks();
			int nattacks = base_nattacks;
			// Compute swarm attacks:
			unit_ability_list swarm = at.get_specials("swarm");
			if(!swarm.empty()) {
				int swarm_max_attacks = swarm.highest("swarm_attacks_max",nattacks).first;
				int swarm_min_attacks = swarm.highest("swarm_attacks_min").first;
				int hitp = u->hitpoints();
				int mhitp = u->max_hitpoints();

				nattacks = swarm_min_attacks + (swarm_max_attacks - swarm_min_attacks) * hitp / mhitp;
			}

			str << span_color(font::weapon_color)
				<< damage << font::weapon_numbers_sep << nattacks
				<< ' ' << at.name()
				<< "</span>\n";

			tooltip << _("Weapon: ") << "<b>" << at.name() << "</b>\n"
				<< _("Damage: ") << "<b>" << damage << "</b>\n";
			// Damage calculations details:
			if(tod_bonus || leader_bonus || slowed) {
				tooltip << "\t" << _("Base damage: ") << base_damage << "\n";
				if (tod_bonus) {
					tooltip << "\t" << _("Time of day: ")
						<< utils::signed_percent(tod_bonus) << "\n";
				}
				if (leader_bonus) {
					tooltip << "\t" << _("Leadership: ")
						<< utils::signed_percent(leader_bonus) << "\n";
				}
				if(slowed) {
					tooltip << "\t" << _("Slowed: ") << "/ 2" << "\n";
				}
			}

			tooltip << _("Attacks: ") << "<b>" << nattacks << "</b>\n";
			if(nattacks != base_nattacks){
				tooltip << "\t" << _("Base attacks: ") << base_nattacks << "\n";
				int hp_ratio = u->hitpoints() * 100 / u->max_hitpoints();
				tooltip << "\t" << _("Swarm: ") << "* "<< hp_ratio <<"%" << "\n";
			}

			res.add_text(flush(str), flush(tooltip));

			std::string range = gettext(at.range().c_str());
			std::string lang_type = gettext(at.type().c_str());

			str << span_color(font::weapon_details_color) << "  "
				<< range << font::weapon_details_sep
				<< lang_type << "</span>\n";

			tooltip << _("Weapon range: ") << "<b>" << range << "</b>\n"
				<< _("Damage type: ")  << "<b>" << lang_type << "</b>\n";

			// Find all the unit types on the map, and
			// show this weapon's bonus against all the different units.
			// Don't show invisible units, except if they are in our team or allied.
			std::set<std::string> seen_units;
			std::map<int,std::set<std::string> > resistances;
			for(unit_map::const_iterator u_it = units.begin(); u_it != units.end(); ++u_it) {
				const map_location& loc = u_it->get_location();
				if (teams[team_index].is_enemy(u_it->side()) &&
				    !current_team.fogged(loc) &&
				    seen_units.count(u_it->type_id()) == 0 &&
				    (!current_team.is_enemy(u_it->side()) ||
				     !u_it->invisible(loc)))
				{
					seen_units.insert(u_it->type_id());
					int resistance = u_it->resistance_against(at, false, loc) - 100;
					resistances[resistance].insert(u_it->type_name());
				}
			}

			// use reverse order to show higher damage bonus first
			for(std::map<int,std::set<std::string> >::reverse_iterator resist = resistances.rbegin(); resist != resistances.rend(); ++resist) {
				tooltip << signed_percent(resist->first) << " " << _("vs") << " ";
				for(std::set<std::string>::const_iterator i = resist->second.begin(); i != resist->second.end(); ++i) {
					if(i != resist->second.begin()) {
						tooltip << ", ";
					}

					tooltip << *i;
				}
				tooltip << "\n";
			}

			res.add_text(flush(str), flush(tooltip));

			const std::string& accuracy_parry = at.accuracy_parry_description();
			if(!accuracy_parry.empty()){
				str << span_color(font::weapon_details_color)
					<< "  " << accuracy_parry << "</span>\n";
				int accuracy = at.accuracy();
				if(accuracy) {
					tooltip << _("Accuracy :") << "<b>"
						<< signed_percent(accuracy) << "</b>\n";
				}
				int parry = at.parry();
				if(parry) {
					tooltip << _("Parry :") << "<b>"
						<< signed_percent(parry) << "</b>\n";
				}
				res.add_text(flush(str), flush(tooltip));
			}

			const std::vector<t_string> &specials = at.special_tooltips();

			if(! specials.empty()) {
				for(std::vector<t_string>::const_iterator sp_it = specials.begin(); sp_it != specials.end(); ++sp_it) {
					str << span_color(font::weapon_details_color)
						<< "  " << *sp_it << "</span>\n";
					const std::string help_page = "weaponspecial_" + sp_it->base_str();
					++sp_it;
					tooltip << *sp_it << '\n';

					res.add_text(flush(str), flush(tooltip), help_page);
				}
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
		time_of_day tod;

		if (current_team.shrouded(mouseover)) {
			// Don't show time on shrouded tiles.
			tod = resources::tod_manager->get_time_of_day();
		} else if (current_team.fogged(mouseover)) {
			// Don't show illuminated time on fogged tiles.
			tod = resources::tod_manager->get_time_of_day(0, mouseover);
		} else {
			tod = resources::tod_manager->time_of_day_at(units, mouseover, *resources::game_map);
		}

		int b = tod.lawful_bonus;
		tooltip << tod.name << '\n'
			<< _("Lawful units: ") << signed_percent(b) << "\n"
			<< _("Neutral units: ") << signed_percent(0) << "\n"
			<< _("Chaotic units: ") << signed_percent(-b);

		std::string tod_image = tod.image;
		if (tod.bonus_modified > 0) tod_image += "~BRIGHTEN()";
		else if (tod.bonus_modified < 0) tod_image += "~DARKEN()";
		if (preferences::flip_time()) tod_image += "~FL(horiz)";

		return report("",tod_image,tooltip.str(),"time_of_day");
	}
	case TURN: {
		str << resources::tod_manager->turn();
		int nb = resources::tod_manager->number_of_turns();
		if (nb != -1) str << '/' << nb;
		break;
	}
	// For the following status reports, show them in gray text
	// when it is not the active player's turn.
	case GOLD: {
		char const *end = naps;
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		else if (current_team.gold() < 0)
			str << span_color(font::BAD_COLOR);
		else
			end = "";
		str << current_team.gold() << end;
		break;
	}
	case VILLAGES: {
		const team_data data = calculate_team_data(current_team,current_side);
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		str << data.villages << '/';
		if (current_team.uses_shroud()) {
			int unshrouded_villages = 0;
			std::vector<map_location>::const_iterator i = map.villages().begin();
			for (; i != map.villages().end(); ++i) {
				if (!current_team.shrouded(*i))
					++unshrouded_villages;
			}
			str << unshrouded_villages;
		} else {
			str << map.villages().size();
		}
		if (current_side != playing_side)
			str << naps;
		break;
	}
	case NUM_UNITS: {
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		str << side_units(current_side);
		if (current_side != playing_side)
			str << naps;
		break;
	}
	case UPKEEP: {
		const team_data data = calculate_team_data(current_team,current_side);
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		str << data.expenses << " (" << data.upkeep << ")";
		if (current_side != playing_side)
			str << naps;
		break;
	}
	case EXPENSES: {
		const team_data data = calculate_team_data(current_team,current_side);
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		str << data.expenses;
		if (current_side != playing_side)
			str << naps;
		break;
	}
	case INCOME: {
		team_data data = calculate_team_data(current_team, current_side);
		char const *end = naps;
		if (current_side != playing_side)
			str << span_color(font::GRAY_COLOR);
		else if (data.net_income < 0)
			str << span_color(font::BAD_COLOR);
		else
			end = "";
		str << data.net_income << end;
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
		std::string new_rgb = team::get_side_color_index(playing_side);
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
			char const *end = naps;
			if (current_side != playing_side)
				str << span_color(font::GRAY_COLOR);
			else if (sec < 60)
				str << "<span foreground=\"#c80000\">";
			else if (sec < 120)
				str << "<span foreground=\"#c8c800\">";
			else
				end = "";

			min = sec / 60;
			str << min << ":";
			sec = sec % 60;
			if (sec < 10) {
				str << "0";
			}
			str << sec << end;
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

