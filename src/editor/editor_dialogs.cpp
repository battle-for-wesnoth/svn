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

#include "SDL.h"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "../config.hpp"
#include "../display.hpp"
#include "../events.hpp"
#include "../filesystem.hpp"
#include "../game_config.hpp"
#include "../gettext.hpp"
#include "../language.hpp"
#include "../map.hpp"
#include "../mapgen.hpp"
#include "../map_create.hpp"
#include "../marked-up_text.hpp"
#include "../show_dialog.hpp"
#include "../util.hpp"
#include "../preferences_display.hpp"
#include "../video.hpp"
#include "../widgets/slider.hpp"

#include "editor_dialogs.hpp"


namespace {
	const int map_min_height = 1;
	const int map_min_width = 1;
	const int map_max_height = 200;
	const int map_max_width = 200;
}

namespace map_editor {

bool confirm_modification_disposal(display& disp) {
	const int res = gui::show_dialog(disp, NULL, "",
					 _("Your modifications to the map will be lost. Continue?"),
					 gui::OK_CANCEL);
	return res == 0;
}


std::string new_map_dialog(display& disp, const t_translation::t_letter fill_terrain,
	const bool confirmation_needed, const config &game_config)
{
	const events::resize_lock prevent_resizing;
	const events::event_context dialog_events_context;
	const gui::dialog_manager dialog_mgr;

	int map_width(40), map_height(40);
	const int width = 600;
	const int height = 400;
	const int xpos = disp.w()/2 - width/2;
	const int ypos = disp.h()/2 - height/2;
	const int horz_margin = 5;
	const int vertical_margin = 20;

	SDL_Rect dialog_rect = {xpos-10,ypos-10,width+20,height+20};
	surface_restorer restorer(&disp.video(),dialog_rect);

	gui::dialog_frame frame(disp.video());
	frame.layout(xpos,ypos,width,height);
	frame.draw_background();
	frame.draw_border();

	SDL_Rect title_rect = font::draw_text(NULL,screen_area(),24,font::NORMAL_COLOUR,
					      _("Create New Map"),0,0);

	const std::string& width_label = _("Width:");
	const std::string& height_label = _("Height:");

	SDL_Rect width_rect = font::draw_text(NULL, screen_area(), 14, font::NORMAL_COLOUR,
										  width_label, 0, 0);
	SDL_Rect height_rect = font::draw_text(NULL, screen_area(), 14, font::NORMAL_COLOUR,
										   height_label, 0, 0);

	const int text_right = xpos + horz_margin +
	        maximum<int>(width_rect.w,height_rect.w);

	width_rect.x = text_right - width_rect.w;
	height_rect.x = text_right - height_rect.w;

	width_rect.y = ypos + title_rect.h + vertical_margin*2;
	height_rect.y = width_rect.y + width_rect.h + vertical_margin;

	gui::button new_map_button(disp.video(), _("Generate New Map"));
	gui::button random_map_button(disp.video(), _("Generate Random Map"));
	gui::button random_map_setting_button(disp.video(), _("Random Generator Settings"));
	gui::button cancel_button(disp.video(), _("Cancel"));

	new_map_button.set_location(xpos + horz_margin,height_rect.y + height_rect.h + vertical_margin);
	random_map_button.set_location(xpos + horz_margin,ypos + height - random_map_button.height()-14*2-vertical_margin);
	random_map_setting_button.set_location(random_map_button.location().x + random_map_button.width() + horz_margin,
	                                       ypos + height - random_map_setting_button.height()
									       - 14*2 - vertical_margin);
	cancel_button.set_location(xpos + width - cancel_button.width() - horz_margin,
	                           ypos + height - cancel_button.height()-14);

	const int right_space = 100;

	const int slider_left = text_right + 10;
	const int slider_right = xpos + width - horz_margin - right_space;
	SDL_Rect slider_rect = { slider_left,width_rect.y,slider_right-slider_left,width_rect.h};

	slider_rect.y = width_rect.y;
	gui::slider width_slider(disp.video());
	width_slider.set_location(slider_rect);
	width_slider.set_min(map_min_width);
	width_slider.set_max(map_max_width);
	width_slider.set_value(map_width);

	slider_rect.y = height_rect.y;
	gui::slider height_slider(disp.video());
	height_slider.set_location(slider_rect);
	height_slider.set_min(map_min_height);
	height_slider.set_max(map_max_height);
	height_slider.set_value(map_height);

	static util::scoped_ptr<map_generator> random_map_generator(NULL);
	if (random_map_generator == NULL) {
		// Initialize the map generator if this is the first call,
		// otherwise keep the settings and such.
		const config* const toplevel_cfg = game_config.find_child("multiplayer","id","ranmap");
		const config* const cfg = toplevel_cfg == NULL ? NULL : toplevel_cfg->child("generator");
		if (cfg == NULL) {
			config dummy_cfg;
			random_map_generator.assign(create_map_generator("", &dummy_cfg));
		}
		else {
			random_map_generator.assign(create_map_generator("", cfg));
		}
	}

	for(bool draw = true;; draw = false) {
		if(cancel_button.pressed()) {
			return "";
		}

		if(new_map_button.pressed()) {
			draw = true;
			if ((confirmation_needed &&
				 confirm_modification_disposal(disp))
				|| !confirmation_needed) {

				return map_editor::new_map(width_slider.value(), height_slider.value(), fill_terrain);
			}
		}
		if(random_map_setting_button.pressed()) {
			draw = true;
			if (random_map_generator.get()->allow_user_config()) {
				random_map_generator.get()->user_config(disp);
			}
		}

		if(random_map_button.pressed()) {
			draw = true;
			if ((confirmation_needed
				 && confirm_modification_disposal(disp))
				|| !confirmation_needed) {

				const std::string map =
					random_map_generator.get()->create_map(std::vector<std::string>());
				if (map == "") {
					gui::message_dialog(disp, "",
									 _("Map creation failed.")).show();
				}
				return map;
			}
		}
		if (width_slider.value() != map_width
			|| height_slider.value() != map_height) {
			draw = true;
		}
		if (draw) {
			map_width = width_slider.value();
			map_height = height_slider.value();
			frame.draw_background();
			frame.draw_border();
			title_rect = font::draw_text(&disp.video(),screen_area(),24,font::NORMAL_COLOUR,
										 _("Create New Map"),
										 xpos+(width-title_rect.w)/2,ypos+10);

			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,
							width_label,width_rect.x,width_rect.y);
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,
							height_label,height_rect.x,height_rect.y);

			std::stringstream width_str;
			width_str << map_width;
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,width_str.str(),
							slider_right+horz_margin,width_rect.y);

			std::stringstream height_str;
			height_str << map_height;
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,height_str.str(),
							slider_right+horz_margin,height_rect.y);

		}

		new_map_button.set_dirty();
		random_map_button.set_dirty();
		random_map_setting_button.set_dirty();
		cancel_button.set_dirty();

		width_slider.set_min(map_min_width);
		height_slider.set_min(map_min_height);

		events::raise_process_event();
		events::raise_draw_event();

		if (draw) {
			update_rect(xpos,ypos,width,height);
		}
		disp.update_display();
		SDL_Delay(20);
		events::pump();
	}
}


void preferences_dialog(display &disp, config &prefs) {
	const events::event_context dialog_events_context;
	const gui::dialog_manager dialog_mgr;

	const int width = 600;
	const int height = 200;
	const int xpos = disp.w()/2 - width/2;
	const int ypos = disp.h()/2 - height/2;

	SDL_Rect clip_rect = disp.screen_area();

	gui::button close_button(disp.video(),_("Close Window"));

	std::vector<gui::button*> buttons;
	buttons.push_back(&close_button);

	surface_restorer restorer;
	gui::dialog_frame frame(disp.video(),_("Preferences"),NULL,&buttons,&restorer);
	frame.layout(xpos,ypos,width,height);
	frame.draw();

	const std::string& scroll_label = _("Scroll Speed:");

	SDL_Rect scroll_rect = {0,0,0,0};
	scroll_rect = font::draw_text(NULL,clip_rect,14,font::NORMAL_COLOUR,
	                              scroll_label,0,0);

	const int text_right = xpos + scroll_rect.w + 5;

	const int scroll_pos = ypos + 20;

	scroll_rect.x = text_right - scroll_rect.w;
	scroll_rect.y = scroll_pos;

	const int slider_left = text_right + 10;
	const int slider_right = xpos + width - 5;
	if(slider_left >= slider_right)
		return;

	SDL_Rect slider_rect = { slider_left, scroll_pos, slider_right - slider_left, 10  };

	slider_rect.y = scroll_pos;
	gui::slider scroll_slider(disp.video());
	scroll_slider.set_location(slider_rect);
	scroll_slider.set_min(1);
	scroll_slider.set_max(100);
	scroll_slider.set_value(preferences::scroll_speed());
	
	gui::button fullscreen_button(disp.video(),_("Toggle Full Screen"),
	                              gui::button::TYPE_CHECK);

	fullscreen_button.set_check(preferences::fullscreen());

	fullscreen_button.set_location(slider_left,scroll_pos + 80);

	gui::button grid_button(disp.video(),_("Show Grid"),
	                        gui::button::TYPE_CHECK);
	grid_button.set_check(preferences::grid());

	grid_button.set_location(slider_left + fullscreen_button.width() + 100,
							 scroll_pos + 80);

	gui::button resolution_button(disp.video(),_("Video Mode"));
	resolution_button.set_location(slider_left,scroll_pos + 80 + 50);

	gui::button hotkeys_button (disp.video(),_("Hotkeys"));
	hotkeys_button.set_location(slider_left + fullscreen_button.width() + 100,
								scroll_pos + 80 + 50);

	bool redraw_all = true;
	
	for(;;) {
		if(close_button.pressed()) {
			break;
		}

		if(fullscreen_button.pressed()) {
			//the underlying frame buffer is changing, so cancel
			//the surface restorer restoring the frame buffer state
			restorer.cancel();
			preferences::set_fullscreen(fullscreen_button.checked());
			redraw_all = true;
		}

		if(redraw_all) {
			restorer.cancel();
			frame.draw();
			fullscreen_button.set_dirty();
			close_button.set_dirty();
			resolution_button.set_dirty();
			grid_button.set_dirty();
			hotkeys_button.set_dirty();
			scroll_slider.set_dirty();

			font::draw_text(&disp.video(),clip_rect,14,font::NORMAL_COLOUR,scroll_label,
		                scroll_rect.x,scroll_rect.y);

			update_rect(screen_area());

			redraw_all = false;
		}


		if(grid_button.pressed()) {
			preferences::set_grid(grid_button.checked());
		}

		if(resolution_button.pressed()) {
			const bool mode_changed = preferences::show_video_mode_dialog(disp);
			if(mode_changed) {
				//the underlying frame buffer is changing, so cancel
				//the surface restorer restoring the frame buffer state
				restorer.cancel();
			}
			break;
		}

		if(hotkeys_button.pressed()) {
			preferences::show_hotkeys_dialog(disp, &prefs);
			break;
		}

		events::pump();
		events::raise_process_event();
		events::raise_draw_event();

		preferences::set_scroll_speed(scroll_slider.value());

		disp.update_display();

		SDL_Delay(20);
	}
}

std::pair<unsigned, unsigned>
resize_dialog(display &disp, const unsigned curr_w, const unsigned curr_h) {
	const events::resize_lock prevent_resizing;
	const events::event_context dialog_events_context;
	const gui::dialog_manager dialog_mgr;

	int map_width(curr_w), map_height(curr_h);
	const int width = 600;
	const int height = 200;
	const int xpos = disp.w()/2 - width/2;
	const int ypos = disp.h()/2 - height/2;
	const int horz_margin = 5;
	const int vertical_margin = 20;
	const int button_padding = 20;

	SDL_Rect dialog_rect = {xpos-10,ypos-10,width+20,height+20};
	surface_restorer restorer(&disp.video(),dialog_rect);

	gui::dialog_frame frame(disp.video());
	frame.layout(xpos,ypos,width,height);
	frame.draw_background();
	frame.draw_border();

	SDL_Rect title_rect = font::draw_text(NULL,screen_area(),24,font::NORMAL_COLOUR,
					      _("Resize Map"),0,0);

	const std::string& width_label = _("Width:");
	const std::string& height_label = _("Height:");

	SDL_Rect width_rect = font::draw_text(NULL, screen_area(), 14, font::NORMAL_COLOUR,
										  width_label, 0, 0);
	SDL_Rect height_rect = font::draw_text(NULL, screen_area(), 14, font::NORMAL_COLOUR,
										   height_label, 0, 0);

	const int text_right = xpos + horz_margin +
	        maximum<int>(width_rect.w,height_rect.w);

	width_rect.x = text_right - width_rect.w;
	height_rect.x = text_right - height_rect.w;

	width_rect.y = ypos + title_rect.h + vertical_margin*2;
	height_rect.y = width_rect.y + width_rect.h + vertical_margin;

	gui::button cancel_button(disp.video(), _("Cancel"));
	gui::button ok_button(disp.video(), _("OK"));

	cancel_button.set_location(xpos + width - cancel_button.width() - horz_margin,
	                           ypos + height - cancel_button.height()-14);

	ok_button.set_location(xpos + width - cancel_button.width() - horz_margin
						   - ok_button.width() - button_padding,
						   ypos + height - ok_button.height()-14);

	const int right_space = 100;
	const int slider_left = text_right + 10;
	const int slider_right = xpos + width - horz_margin - right_space;
	SDL_Rect slider_rect = { slider_left,width_rect.y,slider_right-slider_left,width_rect.h};

	slider_rect.y = width_rect.y;
	gui::slider width_slider(disp.video());
	width_slider.set_location(slider_rect);
	width_slider.set_min(map_min_width);
	width_slider.set_max(map_max_width);
	width_slider.set_value(map_width);

	slider_rect.y = height_rect.y;
	gui::slider height_slider(disp.video());
	height_slider.set_location(slider_rect);
	height_slider.set_min(map_min_height);
	height_slider.set_max(map_max_height);
	height_slider.set_value(map_height);
	for(bool draw = true;; draw = false) {
		if(cancel_button.pressed()) {
			return std::make_pair((unsigned)0, (unsigned)0);
		}
		if (width_slider.value() != map_width
			|| height_slider.value() != map_height) {
			draw = true;
		}
		if (draw) {
			map_width = width_slider.value();
			map_height = height_slider.value();
			frame.draw_background();
			frame.draw_border();

			title_rect = font::draw_text(&disp.video(),screen_area(),24,font::NORMAL_COLOUR,
										 _("Resize Map"),
										 xpos+(width-title_rect.w)/2,ypos+10);

			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,
							width_label,width_rect.x,width_rect.y);
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,
							height_label,height_rect.x,height_rect.y);

			std::stringstream width_str;
			width_str << map_width;
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,width_str.str(),
							slider_right+horz_margin,width_rect.y);

			std::stringstream height_str;
			height_str << map_height;
			font::draw_text(&disp.video(),screen_area(),14,font::NORMAL_COLOUR,height_str.str(),
							slider_right+horz_margin,height_rect.y);

		}
		if (ok_button.pressed()) {
			return std::make_pair((unsigned)map_width, (unsigned)map_height);
		}
		cancel_button.set_dirty();
		ok_button.set_dirty();

		width_slider.set_min(map_min_width);
		height_slider.set_min(map_min_height);

		events::raise_process_event();
		events::raise_draw_event();

		if (draw) {
			update_rect(xpos,ypos,width,height);
		}
		disp.update_display();
		SDL_Delay(20);
		events::pump();
	}

}

FLIP_AXIS flip_dialog(display &disp) {
	std::vector<std::string> items;
	items.push_back(_("X-Axis"));
	items.push_back(_("Y-Axis"));
	const std::string msg = _("Flip around (this may change the dimensions of the map):");
	const int res =
		gui::show_dialog(disp, NULL, "",
						 font::word_wrap_text(msg, 12, 180),
						 gui::OK_CANCEL, &items);
	switch (res) {
	case 0:
		return FLIP_X;
	case 1:
		return FLIP_Y;
	default:
		return NO_FLIP;
	}
}

}



