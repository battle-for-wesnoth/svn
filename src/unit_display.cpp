/* $Id$ */
/*
   Copyright (C) 2003 - 2007 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "actions.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "events.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "halo.hpp"
#include "image.hpp"
#include "log.hpp"
#include "scoped_resource.hpp"
#include "sound.hpp"
#include "unit_display.hpp"
#include "util.hpp"
#include "wassert.hpp"

#define LOG_DP LOG_STREAM(info, display)

static void teleport_unit_between( const gamemap::location& a, const gamemap::location& b, unit& temp_unit)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(a) && disp->fogged(b)) {
		return;
	}

	temp_unit.set_teleporting(*disp,a);
	if (!disp->fogged(a)) { // teleport
		disp->scroll_to_tile(a,game_display::ONSCREEN);
		while(!temp_unit.get_animation()->animation_finished()  && temp_unit.get_animation()->get_animation_time() < 0) {
			disp->invalidate(a);
			disp->place_temporary_unit(temp_unit, a);
			disp->draw();
			events::pump();
			disp->delay(10);
		}
	}
	if (!disp->fogged(b)) { // teleport
		temp_unit.restart_animation(*disp,0);
		disp->scroll_to_tile(b,game_display::ONSCREEN);
		while(!temp_unit.get_animation()->animation_finished()) {
			disp->invalidate(b);
			disp->place_temporary_unit(temp_unit, b);
			disp->draw();
			events::pump();
			disp->delay(10);
		}
	}
	temp_unit.set_standing(*disp,b);
	disp->update_display();
	events::pump();
}

static void move_unit_between( const gamemap& map, const gamemap::location& a, const gamemap::location& b, unit& temp_unit)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(a) && disp->fogged(b)) {
		return;
	}

	const t_translation::t_letter dst_terrain = map.get_terrain(b);

	const double acceleration = disp->turbo_speed();

	disp->scroll_to_tiles(a,b,game_display::ONSCREEN);

	const int total_mvt_time = static_cast<int>(150/acceleration * temp_unit.movement_cost(dst_terrain));
	const unsigned int start_time = SDL_GetTicks();
	int mvt_time = 1;

	while(mvt_time < total_mvt_time-1) { // one draw in each hex at least
		disp->delay(10);
		mvt_time = SDL_GetTicks() -start_time;
		if(mvt_time >=total_mvt_time) mvt_time = total_mvt_time -1;
		double pos =double(mvt_time)/total_mvt_time;
		const gamemap::location& ref_loc =pos<0.5?a:b;
		if(pos >= 0.5) pos = pos -1;
		temp_unit.set_walking(*disp,ref_loc);
		temp_unit.set_offset(pos);
		disp->place_temporary_unit(temp_unit,ref_loc);
		disp->draw();
		events::pump();

	}
}

namespace unit_display
{

bool unit_visible_on_path( const std::vector<gamemap::location>& path, const unit& u, const unit_map& units, const std::vector<team>& teams)
{
	game_display* disp = game_display::get_singleton();
	wassert(disp);
	for(size_t i = 0; i+1 < path.size(); ++i) {
		const bool invisible = teams[u.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
	             u.invisible(path[i],units,teams) &&
		         u.invisible(path[i+1],units,teams);
		if(!invisible) {
			return true;
		}
	}

	return false;
}

void move_unit( const gamemap& map, const std::vector<gamemap::location>& path, unit& u, const std::vector<team>& teams)
{
	game_display* disp = game_display::get_singleton();
	wassert(!path.empty());
	wassert(disp);
	// one hex path (strange), nothing to do
	if (path.size()==1) return;
	
	const unit_map& units = disp->get_units();

	// if the unit is visible scroll to there before hide it
	bool invisible =  teams[u.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
				u.invisible(path[0],units,teams);
	if (!invisible) {
		disp->scroll_to_tiles(path[0],path[1],game_display::ONSCREEN);
	}

	bool was_hidden = u.get_hidden();
	// Original unit is usually hidden (but still on map, so count is correct)
	unit temp_unit = u;
	u.set_hidden(true);
	temp_unit.set_hidden(false);

	for(size_t i = 0; i+1 < path.size(); ++i) {
		temp_unit.set_facing(path[i].get_relative_dir(path[i+1]));

		bool invisible = teams[temp_unit.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
				temp_unit.invisible(path[i],units,teams) &&
				temp_unit.invisible(path[i+1],units,teams);

		if(!invisible) {
			if( !tiles_adjacent(path[i], path[i+1])) {
				teleport_unit_between(path[i],path[i+1],temp_unit);
			} else {
				move_unit_between(map,path[i],path[i+1],temp_unit);
			}
		}
	}
	disp->remove_temporary_unit();
	u.set_facing(path[path.size()-2].get_relative_dir(path[path.size()-1]));
	u.set_standing(*disp,path[path.size()-1]);

	//clean the footsteps path, its hexes will be invalidated if needed
	disp->set_route(NULL);

	u.set_hidden(was_hidden);
	disp->invalidate_unit();
}

void unit_die(const gamemap::location& loc, unit& loser,
const attack_type* attack,const attack_type* secondary_attack, unit* winner)
{
	game_display* disp = game_display::get_singleton();
	if(!disp ||disp->video().update_locked() || disp->fogged(loc) || preferences::show_combat() == false) {
		return;
	}
		const std::string& die_sound = loser.die_sound();
		if(die_sound != "" && die_sound != "null") {
			sound::play_sound(die_sound);
		}

		loser.set_dying(*disp,loc,attack,secondary_attack);
	if(winner == NULL) { //test to see if there is no victor.

		while(!loser.get_animation()->animation_finished()) {

			disp->invalidate(loc);
			disp->draw();
			events::pump();
			disp->delay(10);
		}
	return;
	}
	winner->set_victorious(*disp,loc,attack,secondary_attack);
	int start_time = minimum<int>(loser.get_animation()->get_begin_time(),winner->get_animation()->get_begin_time());

	winner->restart_animation(*disp,start_time);
	loser.restart_animation(*disp,start_time);

	while((!loser.get_animation()->animation_would_finish()) || ((!winner->get_animation()->animation_would_finish()))) {

			disp->invalidate(loc);
			disp->draw();
			events::pump();
			disp->delay(10);
	}
}

static void unit_attack_ranged(
                        const gamemap::location& a, const gamemap::location& b,
			int damage, const attack_type& attack, const attack_type* secondary_attack, int swing,std::string hit_text)

{
	game_display* disp = game_display::get_singleton();
	if(!disp) return;
	const bool hide = disp->video().update_locked() || disp->fogged(a) && disp->fogged(b)
		|| preferences::show_combat() == false ;

	unit_map& units = disp->get_units();
	log_scope("unit_attack_range");

	const unit_map::iterator att = units.find(a);
	wassert(att != units.end());
	unit& attacker = att->second;

	const unit_map::iterator def = units.find(b);
	wassert(def != units.end());
	bool def_was_hidden = def->second.get_hidden();
	def->second.set_hidden(true);
	unit defender = def->second;
	disp->place_temporary_unit(defender,b);
	defender.set_hidden(false);

	const double acceleration = disp->turbo_speed();


	// more damage shown for longer, but 1s at most for this factor
	const double xsrc = disp->get_location_x(a);
	const double ysrc = disp->get_location_y(a) - (attacker.is_flying() ? 0 : disp->get_map().get_terrain_info(disp->get_map().get_terrain(a)).unit_height_adjust());
	const double xdst = disp->get_location_x(b);
	const double ydst = disp->get_location_y(b) -( defender.is_flying() ? 0 : disp->get_map().get_terrain_info(disp->get_map().get_terrain(b)).unit_height_adjust());

	gamemap::location update_tiles[6];
	get_adjacent_tiles(b,update_tiles);




	// start leader and attacker animation, wait for attacker animation to end
	unit_animation missile_animation = attacker.set_attacking(*disp,a,damage,attack,secondary_attack,swing);
	const gamemap::location leader_loc = under_leadership(units,a);
	unit_map::iterator leader = units.end();
	if(leader_loc.valid()){
		LOG_DP << "found leader at " << leader_loc << '\n';
		leader = units.find(leader_loc);
		wassert(leader != units.end());
		leader->second.set_facing(leader_loc.get_relative_dir(a));
		leader->second.set_leading(*disp,leader_loc);
	}
	int animation_time;


	halo::ORIENTATION orientation;
	const gamemap::location::DIRECTION attack_ori = a.get_relative_dir(b);
	switch(attack_ori)
	{
		case gamemap::location::NORTH:
		case gamemap::location::NORTH_EAST:
			orientation = halo::NORMAL;
			break;
		case gamemap::location::SOUTH_EAST:
		case gamemap::location::SOUTH:
			orientation = halo::VREVERSE;
			break;
		case gamemap::location::SOUTH_WEST:
			orientation = halo::HVREVERSE;
			break;
		case gamemap::location::NORTH_WEST:
			orientation = halo::HREVERSE;
			break;
		case gamemap::location::NDIRECTIONS:
		default:
			orientation = halo::NORMAL;
			break;
	}
	const bool vertical_dir = (a.x == b.x) ? true:false;

	defender.set_defending(*disp,b,damage,&attack,secondary_attack,swing);
	// min of attacker, defender, missile and -200
	int start_time = -200;
	start_time = minimum<int>(start_time,defender.get_animation()->get_begin_time());
	start_time = minimum<int>(start_time,missile_animation.get_begin_time());
	start_time = minimum<int>(start_time,attacker.get_animation()->get_begin_time());
	missile_animation.start_animation(start_time,false,acceleration);
	defender.restart_animation(*disp,start_time);
	attacker.restart_animation(*disp,start_time);
	animation_time = defender.get_animation()->get_animation_time();
	bool sound_played = false ;
	int missile_frame_halo = halo::NO_HALO;
	int missile_halo = halo::NO_HALO;
	while(!hide && (
		!attacker.get_animation()->animation_would_finish() ||
		!defender.get_animation()->animation_would_finish() ||
		!missile_animation.animation_finished()  ||
		(leader_loc.valid() && !leader->second.get_animation()->animation_finished()) ||
		damage >0)
	     ){
		const unit_frame& missile_frame = missile_animation.get_current_frame();
		double pos = missile_frame.offset(missile_animation.get_current_frame_time());
		if(pos == -20.0) {
			pos = double(animation_time -missile_animation.get_begin_time())/
				double(missile_animation.get_end_time()-missile_animation.get_begin_time());
		}
		disp->invalidate(b);
		disp->invalidate(a);
		if(leader_loc.valid()) disp->invalidate(leader_loc);
		halo::remove(missile_halo);
		halo::remove(missile_frame_halo);
		missile_halo = halo::NO_HALO;
		missile_frame_halo = halo::NO_HALO;
		if(animation_time > missile_animation.get_begin_time() &&
				animation_time < missile_animation.get_end_time() &&
				(!disp->fogged(b) || !disp->fogged(a))) {
			const int posx = int(pos*xdst + (1.0-pos)*xsrc);
			const int posy = int(pos*ydst + (1.0-pos)*ysrc);

			image::locator missile_image= missile_frame.image();
			const int d = disp->hex_size() / 2;
			if(vertical_dir) {
				missile_image = missile_frame.image();
			} else {
				missile_image = missile_frame.image_diagonal();
			}
			if(!missile_frame.halo(missile_animation.get_current_frame_time()).empty()) {
				if(attack_ori != gamemap::location::SOUTH_WEST && attack_ori != gamemap::location::NORTH_WEST) {
					missile_halo = halo::add(posx+d+missile_frame.halo_x(missile_animation.get_current_frame_time()),
							posy+d+missile_frame.halo_y(missile_animation.get_current_frame_time()),
							missile_frame.halo(missile_animation.get_current_frame_time()),
							gamemap::location(-1, -1),
							orientation);
				} else {
					missile_halo = halo::add(posx+d-missile_frame.halo_x(missile_animation.get_current_frame_time()),
							posy+d+missile_frame.halo_y(missile_animation.get_current_frame_time()),
							missile_frame.halo(missile_animation.get_current_frame_time()),
							gamemap::location(-1, -1),
							orientation);
				}
			}
			missile_frame_halo = halo::add(posx+d,
					posy+d,
					missile_image.get_filename(),
					gamemap::location(-1, -1),
					orientation);

		}
		if(!sound_played && animation_time > 0) {
			sound_played = true;
			std::string text ;
			if(damage) text = lexical_cast<std::string>(damage);
			if(!hit_text.empty()) {
				text.insert(text.begin(),hit_text.size()/2,' ');
				text = text + "\n" + hit_text;
			}
			sound::play_sound(defender.get_hit_sound());
			disp->float_label(b,text,255,0,0);
			disp->invalidate_unit();
		}
		if(damage > 0 && animation_time > 0) {
			defender.take_hit(1);
			damage--;
			disp->invalidate_unit();
		}
		disp->draw();
		events::pump();
		missile_animation.update_last_draw_time();
		disp->delay(10);
		// we use missile animation because it's the only one not reseted in the middle to go to standing
		animation_time = missile_animation.get_animation_time();
	}

	halo::remove(missile_halo);
	missile_halo = halo::NO_HALO;
	halo::remove(missile_frame_halo);
	missile_frame_halo = halo::NO_HALO;

	if(leader_loc.valid()) leader->second.set_standing(*disp,leader_loc);
	att->second.set_standing(*disp,a);
	def->second.set_standing(*disp,b);
	def->second.set_hidden(def_was_hidden);
	disp->remove_temporary_unit();

}

void unit_attack(
                 const gamemap::location& a, const gamemap::location& b, int damage,
                 const attack_type& attack, const attack_type* secondary_attack,
		  int swing,std::string hit_text)
{
	game_display* disp = game_display::get_singleton();
	if(!disp) return;
	unit_map& units = disp->get_units();
	const bool hide = disp->video().update_locked() || disp->fogged(a) && disp->fogged(b)
	                  || preferences::show_combat() == false;

	if(!hide) {
		disp->scroll_to_tiles(a,b,game_display::ONSCREEN);
	}

	log_scope("unit_attack");

	const unit_map::iterator att = units.find(a);
	wassert(att != units.end());
	unit& attacker = att->second;

	const unit_map::iterator def = units.find(b);
	wassert(def != units.end());

	att->second.set_facing(a.get_relative_dir(b));
	def->second.set_facing(b.get_relative_dir(a));
	if(attack.range_type() == attack_type::LONG_RANGE) {
		unit_attack_ranged(  a, b, damage, attack,secondary_attack, swing, hit_text);
		return;
	}

	int start_time = 500;
	int end_time = 0;

	bool def_was_hidden = def->second.get_hidden();
	def->second.set_hidden(true);
	unit defender = def->second;
	disp->place_temporary_unit(defender,b);
	defender.set_hidden(false);


	attacker.set_attacking(*disp,a,damage,attack,secondary_attack,swing);
	start_time=minimum<int>(start_time,attacker.get_animation()->get_begin_time());
	end_time=attacker.get_animation()->get_end_time();

	defender.set_defending(*disp,b,damage,&attack, secondary_attack, swing);
	start_time=minimum<int>(start_time,defender.get_animation()->get_begin_time());


	const gamemap::location leader_loc = under_leadership(units,a);
	unit_map::iterator leader = units.end();
	if(leader_loc.valid()){
		LOG_DP << "found leader at " << leader_loc << '\n';
		leader = units.find(leader_loc);
		wassert(leader != units.end());
		leader->second.set_facing(leader_loc.get_relative_dir(a));
		leader->second.set_leading(*disp,leader_loc);
		start_time=minimum<int>(start_time,leader->second.get_animation()->get_begin_time());
	}


	gamemap::location update_tiles[6];
	get_adjacent_tiles(b,update_tiles);


	attacker.restart_animation(*disp,start_time);
	defender.restart_animation(*disp,start_time);
	if(leader_loc.valid()) leader->second.restart_animation(*disp,start_time);

	int animation_time = start_time;
	bool sound_played = false;
	while(!hide && (
		!attacker.get_animation()->animation_would_finish()  ||
		!defender.get_animation()->animation_would_finish()  ||
		(leader_loc.valid() && !leader->second.get_animation()->animation_would_finish() ) ||
		damage > 0)
	     ){

		double pos = 0.0;
	        if(animation_time < attacker.get_animation()->get_begin_time()) {
			pos = 0.0;
		} else if( animation_time > 0 && end_time > 0) {
			pos = 1.0-double(animation_time)/double(end_time);
		} else {
			pos = 1.0 - double(animation_time)/double(minimum<int>(attacker.get_animation()->get_begin_time(),-150));
		}
		if(pos > 0.0) {
			attacker.set_offset(pos*0.6);
		}
		if(!sound_played && animation_time > 0) {
			sound_played = true;
			std::string text ;
			if(damage) text = lexical_cast<std::string>(damage);
			if(!hit_text.empty()) {
				text.insert(text.begin(),hit_text.size()/2,' ');
				text = text + "\n" + hit_text;
			}
			sound::play_sound(defender.get_hit_sound());
			disp->float_label(b,text,255,0,0);
			disp->invalidate_unit();
		}
		if(damage > 0 && animation_time > 0) {
			defender.take_hit(1);
			damage--;
			disp->invalidate_unit();
		}
		disp->invalidate(b);
		disp->invalidate(a);
		if(leader_loc.valid()) disp->invalidate(leader_loc);
		disp->draw();
		events::pump();
		if(attacker.get_animation()->animation_finished()) {
		   attacker.set_offset(0.0);
		}
		disp->delay(10);
		animation_time = attacker.get_animation()->get_animation_time();
	}

	if(leader_loc.valid()) leader->second.set_standing(*disp,leader_loc);
	att->second.set_standing(*disp,a);
	def->second.set_standing(*disp,b);
	def->second.set_hidden(def_was_hidden);
	disp->remove_temporary_unit();



}


void unit_recruited(gamemap::location& loc)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() ||disp->fogged(loc)) return;
	unit_map::iterator u = disp->get_units().find(loc);
	if(u == disp->get_units().end()) return;

	u->second.set_hidden(true);
	disp->scroll_to_tile(loc,game_display::ONSCREEN);
	disp->draw();
	u->second.set_recruited(*disp,loc);
	u->second.set_hidden(false);
	while(!u->second.get_animation()->animation_finished()) {

		disp->invalidate(loc);
		disp->draw();
		events::pump();
		disp->delay(10);
	}
	u->second.set_standing(*disp,loc);
	if (loc==disp->mouseover_hex()) disp->invalidate_unit();
}

void unit_healing(unit& healed_p,gamemap::location& healed_loc, std::vector<unit_map::iterator> healers, int healing)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(healed_loc)) return;
	if(healing==0) return;
	// This is all the pretty stuff.
	int start_time = INT_MAX;
	disp->scroll_to_tile(healed_loc, game_display::ONSCREEN);
	disp->select_hex(healed_loc);
	unit healed = healed_p;
	bool was_hidden = healed.get_hidden();
	healed_p.set_hidden(true);
	disp->place_temporary_unit(healed,healed_loc);
	healed.set_hidden(false);

	for(std::vector<unit_map::iterator>::iterator heal_anim_it = healers.begin(); heal_anim_it != healers.end(); ++heal_anim_it) {
		(*heal_anim_it)->second.set_facing((*heal_anim_it)->first.get_relative_dir(healed_loc));
		(*heal_anim_it)->second.set_healing(*disp,(*heal_anim_it)->first,healing);
		start_time = minimum<int>((*heal_anim_it)->second.get_animation()->get_begin_time(),start_time);
	}
	if (healing < 0) {
		healed.set_poisoned(*disp,healed_loc, -healing);
		start_time = minimum<int>(start_time, healed.get_animation()->get_begin_time());
		// FIXME
		sound::play_sound("poison.ogg");
		disp->float_label(healed_loc, lexical_cast<std::string>(-healing), 255,0,0);
	} else {
		healed.set_healed(*disp,healed_loc, healing);
		start_time = minimum<int>(start_time, healed.get_animation()->get_begin_time());
		sound::play_sound("heal.wav");
		disp->float_label(healed_loc, lexical_cast<std::string>(healing), 0,255,0);
	}
	disp->draw();
	events::pump();
	// restart all anims in a synchronized way
	healed.restart_animation(*disp, start_time);
	for(std::vector<unit_map::iterator>::iterator heal_reanim_it = healers.begin(); heal_reanim_it != healers.end(); ++heal_reanim_it) {
		(*heal_reanim_it)->second.restart_animation(*disp, start_time);
	}

	bool finished;
	do {
		finished = (healed.get_animation()->animation_finished() && healing==0);
		disp->invalidate(healed_loc);
		for(std::vector<unit_map::iterator>::iterator heal_fanim_it = healers.begin(); heal_fanim_it != healers.end(); ++heal_fanim_it) {
			finished &= (*heal_fanim_it)->second.get_animation()->animation_finished();
			disp->invalidate((*heal_fanim_it)->first);
		}
		// TODO : adapt HP change speed to turbo_speed
		if(healing > 0) {
			healed.heal(1);
			healing--;
		} else if (healing < 0) {
			healed.take_hit(1);
			healing++;
		}
		disp->draw();
		events::pump();
		disp->delay(10);
	} while (!finished);

	healed_p.set_standing(*disp,healed_loc);
	healed_p.set_hidden(was_hidden);
	disp->remove_temporary_unit();
	for(std::vector<unit_map::iterator>::iterator heal_sanim_it = healers.begin(); heal_sanim_it != healers.end(); ++heal_sanim_it) {
		(*heal_sanim_it)->second.set_standing(*disp,(*heal_sanim_it)->first);
	}

	disp->update_display();
	events::pump();
}
} // end unit display namespace
