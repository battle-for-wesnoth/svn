/* $Id$ */
/*
   Copyright (C) 2007
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"
#include "filesystem.hpp"
#include "marked-up_text.hpp"
#include "minimap.hpp"
#include "multiplayer_lobby.hpp"
#include "replay.hpp"
#include "wassert.hpp"
#include "wml_separators.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "playmp_controller.hpp"
#include "show_dialog.hpp"
#include "sound.hpp"

namespace {
std::vector<std::string> empty_string_vector;
}

namespace mp {
	gamebrowser::gamebrowser(CVideo& video, const config* map_hashes) :
	menu(video, empty_string_vector, false, -1, -1, NULL, &menu::bluebg_style),
	gold_icon_locator_("misc/gold.png"),
	xp_icon_locator_("misc/units.png"),
	vision_icon_locator_("misc/invisible.png"),
	time_limit_icon_locator_("misc/sand-clock.png"),
	observer_icon_locator_("misc/eye.png"), map_hashes_(map_hashes),
	item_height_(100), margin_(5), h_padding_(5),
	header_height_(20), selected_(0), visible_range_(std::pair<size_t,size_t>(0,0)),
	double_clicked_(false), ignore_next_doubleclick_(false), last_was_doubleclick_(false)
{
	set_numeric_keypress_selection(false);
}

void gamebrowser::set_inner_location(const SDL_Rect& rect)
{
	set_full_size(games_.size());
	set_shown_size(rect.h / row_height());
	bg_register(rect);
	scroll(get_position());
}

void gamebrowser::scroll(unsigned int pos)
{
	if(pos < games_.size()) {
		visible_range_.first = pos;
		visible_range_.second = minimum<size_t>(pos + inner_location().h / row_height(), games_.size());
		set_dirty();
	}
}

SDL_Rect gamebrowser::get_item_rect(size_t index) const {
	if(index < visible_range_.first || index > visible_range_.second) {
		const SDL_Rect res = { 0, 0, 0, 0 };
		return res;
	}
	const SDL_Rect& loc = inner_location();
	const SDL_Rect res = { loc.x, loc.y + (index - visible_range_.first) * row_height(), loc.w, row_height()};
	return res;
}

void gamebrowser::draw()
{
	if(hidden())
		return;
	if(dirty()) {
		bg_restore();
		util::scoped_ptr<clip_rect_setter> clipper(NULL);
		if(clip_rect())
			clipper.assign(new clip_rect_setter(video().getSurface(), *clip_rect()));
		draw_contents();
		update_rect(location());
		set_dirty(false);
	}
}

void gamebrowser::draw_contents()
{
	if(!games_.empty()) {
		for(size_t i = visible_range_.first; i != visible_range_.second; ++i) {
			style_->draw_row(*this,i,get_item_rect(i),(i==selected_)? SELECTED_ROW : NORMAL_ROW);
		}
	} else {
		const SDL_Rect rect = inner_location();
		font::draw_text(&video(), rect, font::SIZE_NORMAL, font::NORMAL_COLOUR, _("--no games open--"), rect.x + margin_, rect.y + margin_);
	}
}

void gamebrowser::draw_row(const size_t index, const SDL_Rect& item_rect, ROW_TYPE /*type*/) {
	const game_item& game = games_[index];
	int xpos = item_rect.x + margin_;
	int ypos = item_rect.y + margin_;

	//bg_restore(item_rect);
	//draw_solid_tinted_rectangle(item_rect.x, item_rect.y, item_rect.w, item_rect.h, 0, 0, 0, 0.2, video().getSurface());

	//draw minimaps
	video().blit_surface(xpos, ypos, game.mini_map);
	xpos += item_height_ + margin_;

	//set font color
	SDL_Color font_color;
	if (game.vacant_slots > 0) {
		if (!game.started) {
			font_color = font::GOOD_COLOUR;
		} else {
			font_color = font::YELLOW_COLOUR;
		}
	} else {
		if (game.observers) {
			font_color = font::NORMAL_COLOUR;
		} else {
			font_color = font::BAD_COLOUR;
		}
	}

	//draw game name
	const surface name_surf(font::get_rendered_text(font::make_text_ellipsis(game.name, font::SIZE_PLUS, (item_rect.x + item_rect.w) - xpos - margin_), font::SIZE_PLUS, font_color));
	video().blit_surface(xpos, ypos, name_surf);

	ypos = item_rect.y + item_rect.h/2;

	// draw map info
	const surface map_info_surf(font::get_rendered_text(font::make_text_ellipsis(game.map_info, font::SIZE_NORMAL, (item_rect.x + item_rect.w) - xpos - margin_), font::SIZE_NORMAL, font::NORMAL_COLOUR));
	video().blit_surface(xpos, ypos - map_info_surf->h/2, map_info_surf);

	// draw gold icon
	const surface gold_icon(image::get_image(gold_icon_locator_));
	ypos = item_rect.y + item_rect.h  - margin_ - gold_icon->h;
	video().blit_surface(xpos, ypos, gold_icon);

	xpos += gold_icon->w + h_padding_;

	// draw gold text
	const surface gold_text(font::get_rendered_text(game.gold, font::SIZE_NORMAL, game.use_map_settings ? font::GRAY_COLOUR : font::NORMAL_COLOUR));
	ypos -= abs(gold_icon->h - gold_text->h) / 2;
	video().blit_surface(xpos, ypos, gold_text);

	xpos += gold_text->w + 2 * h_padding_;

	// draw xp icon
	const surface xp_icon(image::get_image(xp_icon_locator_));
	ypos = item_rect.y + item_rect.h  - margin_ - xp_icon->h;
	video().blit_surface(xpos, ypos, xp_icon);

	xpos += xp_icon->w + h_padding_;

	// draw xp text
	const surface xp_text(font::get_rendered_text(game.xp, font::SIZE_NORMAL, font::NORMAL_COLOUR));
	ypos -= abs(xp_icon->h - xp_text->h) / 2;
	video().blit_surface(xpos, ypos, xp_text);

	xpos += xp_text->w + 2 * h_padding_;

	if(!game.time_limit.empty()) {
		// draw time icon
		const surface time_icon(image::get_image(time_limit_icon_locator_));
		ypos = item_rect.y + item_rect.h  - margin_ - time_icon->h;
		video().blit_surface(xpos, ypos, time_icon);

		xpos += time_icon->w + h_padding_;

		// draw time text
		const surface time_text(font::get_rendered_text(game.time_limit, font::SIZE_NORMAL, font::NORMAL_COLOUR));
		ypos -= abs(time_icon->h - time_text->h) / 2;
		video().blit_surface(xpos, ypos, time_text);

		xpos += time_text->w + 2 * h_padding_;
	}

	// draw vision icon
	const surface vision_icon(image::get_image(vision_icon_locator_));
	video().blit_surface(xpos, ypos, vision_icon);

	xpos += vision_icon->w + h_padding_;

	const surface status_text(font::get_rendered_text(game.status, font::SIZE_NORMAL, font_color));

	const int status_text_width = status_text ? status_text->w : 0;
	const surface vision_text(font::get_rendered_text(font::make_text_ellipsis(game.vision, font::SIZE_NORMAL, maximum<int>((item_rect.x + item_rect.w - margin_ - status_text_width - 2 * h_padding_) - xpos, 0)),font::SIZE_NORMAL, game.use_map_settings ? font::GRAY_COLOUR : font::NORMAL_COLOUR));
	// draw vision text
	video().blit_surface(xpos, ypos, vision_text);

	// draw map settings text
	if (game.use_map_settings) {
		const surface map_settings_text(font::get_rendered_text(_("Use map settings"), font::SIZE_NORMAL, font::NORMAL_COLOUR));
		xpos += vision_text->w + 4 * h_padding_;
		video().blit_surface(xpos, ypos, map_settings_text);
	}

	// draw status text
	xpos = item_rect.x + item_rect.w - margin_ - status_text_width;
	if(status_text) {
		video().blit_surface(xpos, ypos, status_text);
	}

	//if(selected_ == index)
	//	draw_solid_tinted_rectangle(item_rect.x, item_rect.y, item_rect.w, item_rect.h, 153, 0, 0, 0.3, video().getSurface());
}

void gamebrowser::handle_event(const SDL_Event& event)
{
	scrollarea::handle_event(event);
	if(event.type == SDL_KEYDOWN) {
		if(focus(&event) && !games_.empty()) {
			switch(event.key.keysym.sym) {
				case SDLK_UP:
					if(selected_ > 0) {
						--selected_;
						adjust_position(selected_);
						set_dirty();
					}
					break;
				case SDLK_DOWN:
					if(selected_ < games_.size() - 1) {
						++selected_;
						adjust_position(selected_);
						set_dirty();
					}
					break;
				case SDLK_PAGEUP:
				{
					const long items_on_screen = visible_range_.second - visible_range_.first;
					selected_ = static_cast<size_t>(maximum<long>(static_cast<long>(selected_) - items_on_screen, 0));
					adjust_position(selected_);
					set_dirty();
				}
					break;
				case SDLK_PAGEDOWN:
				{
					const size_t items_on_screen = visible_range_.second - visible_range_.first;
					selected_ = minimum<size_t>(selected_ + items_on_screen, games_.size() - 1);
					adjust_position(selected_);
					set_dirty();
				}
					break;
				case SDLK_HOME:
					selected_ = 0;
					adjust_position(selected_);
					set_dirty();
					break;
				case SDLK_END:
					selected_ = games_.size() - 1;
					adjust_position(selected_);
					set_dirty();
					break;
				default:
					break;
			}
		}
	} else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT || event.type == DOUBLE_CLICK_EVENT) {
		int x = 0;
		int y = 0;
		if(event.type == SDL_MOUSEBUTTONDOWN) {
			x = event.button.x;
			y = event.button.y;
		} else {
			x = (long)event.user.data1;
			y = (long)event.user.data2;
		}
		const SDL_Rect& loc = inner_location();

		if(!games_.empty() && point_in_rect(x, y, loc)) {
			for(size_t i = visible_range_.first; i != visible_range_.second; ++i) {
				const SDL_Rect& item_rect = get_item_rect(i);

				if(point_in_rect(x, y, item_rect)) {
					set_focus(true);
					selected_ = i;
					break;
				}
			}
			if(event.type == DOUBLE_CLICK_EVENT) {
				if (ignore_next_doubleclick_) {
					ignore_next_doubleclick_ = false;
				} else if(selection_is_joinable() || selection_is_observable()) {
					double_clicked_ = true;
					last_was_doubleclick_ = true;
				}
			} else if (last_was_doubleclick_) {
				// If we have a double click as the next event, it means
				// this double click was generated from a click that
				// already has helped in generating a double click.
				SDL_Event ev;
				SDL_PeepEvents(&ev, 1, SDL_PEEKEVENT,
							   SDL_EVENTMASK(DOUBLE_CLICK_EVENT));
				if (ev.type == DOUBLE_CLICK_EVENT) {
					ignore_next_doubleclick_ = true;
				}
				last_was_doubleclick_ = false;
			}
		}
	}
}

struct minimap_cache_item {
	std::string map_id;
	surface mini_map;
	std::string map_info_size;
};

void gamebrowser::set_game_items(const config& cfg, const config& game_config)
{
	const bool scrolled_to_max = (has_scrollbar() && get_position() == get_max_position());

	item_height_ = 100;

	// don't throw the rendered minimaps away
	std::vector<minimap_cache_item> minimap_cache;
	for(std::vector<game_item>::iterator oldgame = games_.begin(); oldgame != games_.end(); ++oldgame) {
		minimap_cache_item item;
		item.map_id = oldgame->id;
		item.mini_map = oldgame->mini_map;
		item.map_info_size = oldgame->map_info_size;
		minimap_cache.push_back(item);
	}

	games_.clear();
	config::child_list games = cfg.get_children("game");
	config::child_iterator game;

	for(game = games.begin(); game != games.end(); ++game) {
		games_.push_back(game_item());
		if((**game)["mp_era"] != "") {
			const config* const era_cfg = game_config.find_child("era", "id", (**game)["mp_era"]);
			games_.back().map_info = era_cfg != NULL ? era_cfg->get_attribute("name") : _("Unknown era");

		} else {
			games_.back().map_info = _("Unknown era");
		}
		games_.back().map_data = (**game)["map_data"];
		if(games_.back().map_data.empty()) {
			games_.back().map_data = read_map((**game)["map"]);
		}

		if(! games_.back().map_data.empty()) {
			try {
				std::vector<minimap_cache_item>::iterator i;
				bool found = false;
				for(i = minimap_cache.begin(); i != minimap_cache.end() && !found; ++i) {
					if (i->map_id == games_.back().id) {
						found = true;
						games_.back().map_info_size = i->map_info_size;
						games_.back().mini_map = i->mini_map;
					}
				}
				if (!found) {
					// parsing the map and generating the minimap are both cpu expensive
					gamemap map(game_config, games_.back().map_data);
					games_.back().mini_map = image::getMinimap(item_height_ - margin_, item_height_ - 2 * margin_, map, 0);
					games_.back().map_info_size = lexical_cast_default<std::string, int>(map.w(), "??") + std::string("x") + lexical_cast_default<std::string, int>(map.h(), "??");
				}

				games_.back().map_info += " - " + games_.back().map_info_size;
			} catch(gamemap::incorrect_format_exception &e) {
				std::cerr << "illegal map: " << e.msg_ << "\n";
			}
		} else {
			games_.back().map_info += " - ??x??";
		}
		games_.back().map_info += " ";
		if((**game)["mp_scenario"] != "" && map_hashes_) {
			const config* level_cfg = game_config.find_child("generic_multiplayer", "id", (**game)["mp_scenario"]);
			if(level_cfg == NULL)
				level_cfg = game_config.find_child("multiplayer", "id", (**game)["mp_scenario"]);
			games_.back().map_info += level_cfg != NULL ? level_cfg->get_attribute("name") : _("Unknown scenario");
			if(level_cfg) {
				const std::string& hash = (**game)["hash"];
				bool hash_found = false;
				for(string_map::const_iterator i = map_hashes_->values.begin(); i != map_hashes_->values.end(); ++i) {
					if(i->first == (**game)["mp_scenario"] && i->second == hash) {
						hash_found = true;
						break;
					}
				}
				if(!hash_found) {
					games_.back().map_info += " - ";
					games_.back().map_info += _("Remote scenario");
				}
			}
		} else {
			games_.back().map_info += _("Unknown scenario");
		}
		games_.back().id = (**game)["id"];
		games_.back().name = (**game)["name"];
		const std::string& turn = (**game)["turn"];
		const std::string& slots = (**game)["slots"];
		games_.back().vacant_slots = lexical_cast_default<size_t>(slots, 0);
		games_.back().current_turn = 0;
		if(turn != "") {
			games_.back().started = true;
			int index = turn.find_first_of('/');
			if (index > -1){
				const std::string current_turn = turn.substr(0, index);
				games_.back().current_turn = lexical_cast<unsigned int>(current_turn);
			}
			games_.back().status = _("Turn") + (" " + turn);
		} else {
			games_.back().started = false;
			if(slots != "")
				games_.back().status = std::string(ngettext(_("Vacant Slot:"), _("Vacant Slots:"), games_.back().vacant_slots)) + " " + slots;
		}

		if((**game)["mp_use_map_settings"] == "yes") {
			games_.back().use_map_settings = true;
		} else {
			games_.back().use_map_settings = false;
		}
		games_.back().gold = (**game)["mp_village_gold"];
		if((**game)["mp_fog"] == "yes") {
			games_.back().vision = _("Fog");
			games_.back().fog = true;
			if((**game)["mp_shroud"] == "yes") {
				games_.back().vision += "/";
				games_.back().vision += _("Shroud");
				games_.back().shroud = true;
			} else {
				games_.back().shroud = false;
			}
		} else if((**game)["mp_shroud"] == "yes") {
			games_.back().vision = _("Shroud");
			games_.back().fog = false;
			games_.back().shroud = true;
		} else {
			games_.back().vision = _("none");
			games_.back().fog = false;
			games_.back().shroud = false;
		}
		if((**game)["mp_countdown"] == "yes" ) {
			games_.back().time_limit = (**game)["mp_countdown_init_time"] + " / +" + (**game)["mp_countdown_turn_bonus"] + " " + (**game)["mp_countdown_action_bonus"];
		} else {
			games_.back().time_limit = "";
		}

		games_.back().xp = (**game)["experience_modifier"] + "%";
		games_.back().observers = (**game)["observer"] != "no" ? true : false;
	}
	set_full_size(games_.size());
	set_shown_size(inner_location().h / row_height());
	if(scrolled_to_max) {
		set_position(get_max_position());
	}
	scroll(get_position());
	if(selected_ >= games_.size())
		selected_ = maximum<long>(static_cast<long>(games_.size()) - 1, 0);
	set_dirty();
}

lobby::lobby_sorter::lobby_sorter(const config& cfg) : cfg_(cfg)
{
	set_alpha_sort(1);
}

bool lobby::lobby_sorter::column_sortable(int column) const
{
	switch(column)
	{
	case MAP_COLUMN:
	case STATUS_COLUMN:
		return true;
	default:
		return basic_sorter::column_sortable(column);
	}
}

bool lobby::lobby_sorter::less(int column, const gui::menu::item& row1, const gui::menu::item& row2) const
{
	const config* const list = cfg_.child("gamelist");
	if(list == NULL) {
		return false;
	}

	const config::child_list& games = list->get_children("game");
	if(row1.id >= games.size() || row2.id >= games.size()) {
		return false;
	}

	const config& game1 = *games[row1.id];
	const config& game2 = *games[row2.id];

	if(column == MAP_COLUMN) {
		size_t mapsize1 = game1["map_data"].size();
		if(mapsize1 == 0) {
			mapsize1 = game1["map"].size();
		}

		size_t mapsize2 = game2["map_data"].size();
		if(mapsize2 == 0) {
			mapsize2 = game2["map"].size();
		}

		return mapsize1 < mapsize2;

	} else if(column == STATUS_COLUMN) {
		const int nslots1 = atoi(game1["slots"].c_str());
		const int nslots2 = atoi(game2["slots"].c_str());

		const int turn1 = atoi(game1["turn"].c_str());
		const int turn2 = atoi(game2["turn"].c_str());

		if(nslots1 > nslots2) {
			return true;
		} else if(nslots1 < nslots2) {
			return false;
		} else {
			return turn1 < turn2;
		}
	} else {
		return basic_sorter::less(column,row1,row2);
	}

	return false;
}

lobby::lobby(game_display& disp, const config& cfg, chat& c, config& gamelist) :
	mp::ui(disp, _("Game Lobby"), cfg, c, gamelist),

	observe_game_(disp.video(), _("Observe Game")),
	join_game_(disp.video(), _("Join Game")),
	create_game_(disp.video(), _("Create Game")),
	skip_replay_(disp.video(), _("Quick Replays"), gui::button::TYPE_CHECK),
#ifndef USE_TINY_GUI
	game_preferences_(disp.video(), _("Preferences")),
#endif
	quit_game_(disp.video(), _("Quit")),
	last_selected_game_(-1), sorter_(gamelist),
	games_menu_(disp.video(),cfg.child("multiplayer_hashes"))
{
	skip_replay_.set_check(preferences::skip_mp_replay());
	skip_replay_.set_help_string(_("Skip quickly to the active turn when observing"));
	game_config::debug = false;
	gamelist_updated();
	sound::play_music_repeatedly(game_config::title_music);
}

void lobby::hide_children(bool hide)
{
	ui::hide_children(hide);

	games_menu_.hide(hide);
	observe_game_.hide(hide);
	join_game_.hide(hide);
	create_game_.hide(hide);
	skip_replay_.hide(hide);
#ifndef USE_TINY_GUI
	game_preferences_.hide(hide);
#endif
	quit_game_.hide(hide);
}

void lobby::layout_children(const SDL_Rect& rect)
{
	ui::layout_children(rect);

#ifdef USE_TINY_GUI
	unsigned int btn_space = 3;
	unsigned int xborder   = 0;
	unsigned int yborder   = 0;
#else
	unsigned int btn_space = 5;
	unsigned int xborder   = 10;
	unsigned int yborder   = 7;
#endif

	// align to the left border
	join_game_.set_location(xscale(xborder), yscale(yborder));
	observe_game_.set_location(join_game_.location().x + join_game_.location().w + btn_space, yscale(yborder));
	create_game_.set_location(observe_game_.location().x + observe_game_.location().w + btn_space, yscale(yborder));

#ifndef USE_TINY_GUI
	// align 'Quit' to the right border
	quit_game_.set_location(xscale(xscale_base - xborder) - quit_game_.location().w, yscale(yborder));

	// align in the middle between the right and left buttons
	int space = (quit_game_.location().x - create_game_.location().x - create_game_.location().w
	             - skip_replay_.location().w - game_preferences_.location().w - btn_space) / 2;
	if (space < btn_space) space = btn_space;
	skip_replay_.set_location(create_game_.location().x + create_game_.location().w + space, yscale(yborder));
	game_preferences_.set_location(quit_game_.location().x - game_preferences_.location().w - space, yscale(yborder));
#else
	skip_replay_.set_location(create_game_.location().x + create_game_.location().w, yscale(yborder));
	quit_game_.set_location(skip_replay_.location().x + skip_replay_.location().w + btn_space, yscale(yborder));
#endif

	games_menu_.set_location(client_area().x, client_area().y + title().height());
	games_menu_.set_measurements(client_area().w, client_area().h
			- title().height() - gui::ButtonVPadding);
}

void lobby::gamelist_updated(bool silent)
{
	ui::gamelist_updated(silent);

	const config* list = gamelist().child("gamelist");
	if(list == NULL) {
		// No gamelist yet. Do not update anything.
		return;
	}
	games_menu_.set_game_items(*list, game_config());
	join_game_.enable(games_menu_.selection_is_joinable());
	observe_game_.enable(games_menu_.selection_is_observable());
}

void lobby::process_event()
{
	join_game_.enable(games_menu_.selection_is_joinable());
	observe_game_.enable(games_menu_.selection_is_observable());

	const bool observe = (observe_game_.pressed() || (games_menu_.selected() && !games_menu_.selection_is_joinable())) && games_menu_.selection_is_observable();
	const bool join = (join_game_.pressed() || games_menu_.selected()) && games_menu_.selection_is_joinable();
	preferences::set_skip_mp_replay(skip_replay_.checked());
	playmp_controller::set_replay_last_turn(0);

	int selected_game = games_menu_.selection();
	if (selected_game != last_selected_game_) {
		if (games_menu_.empty()) {
			set_selected_game("");
		} else {
			set_selected_game(games_menu_.selected_game().name);
		}
		ui::gamelist_updated();
		last_selected_game_ = selected_game;
	}

	if(join || observe) {
		const int selected = games_menu_.selection();
		if(!games_menu_.empty() && selected >= 0) {
			gamebrowser::game_item game = games_menu_.selected_game();

			config response;
			config& join = response.add_child("join");
			join["id"] = game.id;
			if (observe){
				join["observe"] = "yes";
			}
			else{
				join["observe"] = "no";
			}
			network::send_data(response);

			if(observe) {
				if (game.started){
					playmp_controller::set_replay_last_turn(game.current_turn);
				}
				set_result(OBSERVE);
			} else {
				set_result(JOIN);
			}
		}
		return;
	}

	if(create_game_.pressed()) {
		set_result(CREATE);
		return;
	}

#ifndef USE_TINY_GUI
	if(game_preferences_.pressed()) {
		set_result(PREFERENCES);
		return;
	}
#endif

	if(quit_game_.pressed()) {
		recorder.set_skip(false);
		set_result(QUIT);
		return;
	}
}


void lobby::process_network_data(const config& data, const network::connection sock)
{
	ui::process_network_data(data, sock);

	// invalidate game selection for the player list
	last_selected_game_ = -1;
}

}
