/*
  Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
  Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/

#include "SDL.h"

#include "../display.hpp"
#include "../show_dialog.hpp"
#include "../config.hpp"
#include "../game_config.hpp"
#include "../mapgen.hpp"
#include "../filesystem.hpp"
#include "../font.hpp"
#include "../events.hpp"
#include "../util.hpp"
#include "../widgets/slider.hpp"
#include "../language.hpp"
#include "../map.hpp"
#include "../preferences.hpp"

#include "editor_dialogs.hpp"

namespace map_editor {

bool confirm_modification_disposal(display& disp) {
	const int res = gui::show_dialog(disp, NULL, "",
									 "Your modifications to the map will be lost. Continue?",
									 gui::OK_CANCEL);
	return res == 0;
}


std::string new_map_dialog(display& disp, gamemap::TERRAIN fill_terrain,
						   bool confirmation_needed, const config &game_config)
{
	const events::resize_lock prevent_resizing;
	const events::event_context dialog_events_context;

	int map_width(40), map_height(40);
	const int width = 600;
	const int height = 400;
	const int xpos = disp.x()/2 - width/2;
	const int ypos = disp.y()/2 - height/2;
	const int horz_margin = 5;
	const int vertical_margin = 20;

	SDL_Rect dialog_rect = {xpos-10,ypos-10,width+20,height+20};
	surface_restorer restorer(&disp.video(),dialog_rect);

	gui::draw_dialog_frame(xpos,ypos,width,height,disp);

	SDL_Rect title_rect = font::draw_text(NULL,disp.screen_area(),24,font::NORMAL_COLOUR,
					      "Create New Map",0,0);

	const std::string& width_label = string_table["map_width"] + ":";
	const std::string& height_label = string_table["map_height"] + ":";

	SDL_Rect width_rect = font::draw_text(NULL, disp.screen_area(), 14, font::NORMAL_COLOUR,
										  width_label, 0, 0);
	SDL_Rect height_rect = font::draw_text(NULL, disp.screen_area(), 14, font::NORMAL_COLOUR,
										   height_label, 0, 0);

	const int text_right = xpos + horz_margin +
	        maximum<int>(width_rect.w,height_rect.w);

	width_rect.x = text_right - width_rect.w;
	height_rect.x = text_right - height_rect.w;
	
	width_rect.y = ypos + title_rect.h + vertical_margin*2;
	height_rect.y = width_rect.y + width_rect.h + vertical_margin;

	gui::button new_map_button(disp,"Generate New Map With Selected Terrain");
	gui::button random_map_button(disp,"Generate Random Map");
	gui::button random_map_setting_button(disp,"Random Generator Setting");
	gui::button cancel_button(disp,"Cancel");

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

	const int min_width = 20;
	const int max_width = 200;
	const int max_height = 200;
	
	slider_rect.y = width_rect.y;
	gui::slider width_slider(disp,slider_rect);
	width_slider.set_min(min_width);
	width_slider.set_max(max_width);
	width_slider.set_value(map_width);

	slider_rect.y = height_rect.y;
	gui::slider height_slider(disp,slider_rect);
	height_slider.set_min(min_width);
	height_slider.set_max(max_height);
	height_slider.set_value(map_height);

 	const config* const cfg =
		game_config.find_child("multiplayer","id","ranmap")->child("generator");
 	util::scoped_ptr<map_generator> generator(NULL);
 	generator.assign(create_map_generator("", cfg));

	for(bool draw = true;; draw = false) {
		int mousex, mousey;
		const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

		const bool left_button = mouse_flags&SDL_BUTTON_LMASK;

		if(cancel_button.process(mousex,mousey,left_button)) {
			return "";
		}

		if(new_map_button.process(mousex,mousey,left_button)) {
			draw = true;
			if ((confirmation_needed &&
				 confirm_modification_disposal(disp))
				|| !confirmation_needed) {
				int i;
				std::stringstream str;
				std::stringstream map_str;
				for (i = 0; i < width_slider.value(); i++) {
					str << fill_terrain;
				}
				str << "\n";
				for (i = 0; i < height_slider.value(); i++) {
					map_str << str.str();
				}
				return map_str.str();
			}
		}
		if(random_map_setting_button.process(mousex,mousey,left_button)) {
			draw = true;
			if (generator.get()->allow_user_config()) {
				generator.get()->user_config(disp);
			}
		}

		if(random_map_button.process(mousex,mousey,left_button)) {
			draw = true;
			if ((confirmation_needed
				 && confirm_modification_disposal(disp))
				|| !confirmation_needed) {
				
				const std::string map = generator.get()->create_map(std::vector<std::string>());
				if (map == "") {
					gui::show_dialog(disp, NULL, "Creation Failed",
									 "Map creation failed.", gui::OK_ONLY);
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
			gui::draw_dialog_frame(xpos,ypos,width,height,disp);
			title_rect = font::draw_text(&disp,disp.screen_area(),24,font::NORMAL_COLOUR,
										 "Create New Map",xpos+(width-title_rect.w)/2,ypos+10);

			font::draw_text(&disp,disp.screen_area(),14,font::NORMAL_COLOUR,
							width_label,width_rect.x,width_rect.y);
			font::draw_text(&disp,disp.screen_area(),14,font::NORMAL_COLOUR,
							height_label,height_rect.x,height_rect.y);
			
			std::stringstream width_str;
			width_str << map_width;
			font::draw_text(&disp,disp.screen_area(),14,font::NORMAL_COLOUR,width_str.str(),
							slider_right+horz_margin,width_rect.y);
			
			std::stringstream height_str;
			height_str << map_height;
			font::draw_text(&disp,disp.screen_area(),14,font::NORMAL_COLOUR,height_str.str(),
							slider_right+horz_margin,height_rect.y);
			
		}

		new_map_button.set_dirty();
		random_map_button.set_dirty();
		random_map_setting_button.set_dirty();
		cancel_button.set_dirty();

		width_slider.set_min(min_width);
		height_slider.set_min(min_width);

		events::raise_process_event();
		events::raise_draw_event();

		if (draw) {
			update_rect(xpos,ypos,width,height);
		}
		disp.update_display();
		SDL_Delay(10);
		events::pump();
	}
}


std::string load_map_dialog(display &disp) {
	const std::string system_path = game_config::path + "/data/maps/";
	std::vector<std::string> files;
	get_files_in_dir(system_path,&files);
	files.push_back("Enter Path...");
	files.push_back("Local Map...");
  
	std::string filename;
	const int res = gui::show_dialog(disp, NULL, "",
					 "Choose map to edit:", gui::OK_CANCEL, &files);
	if(res == int(files.size()-1)) {
		std::vector<std::string> user_files;
		const std::string user_path = get_user_data_dir() + "/editor/maps/";
		get_files_in_dir(user_path,&user_files);
		const int res = gui::show_dialog(disp, NULL, "",
						 "Choose map to edit:", gui::OK_CANCEL, &user_files);
		if (res < 0 || user_files.empty()) {
			return "";
		}
		filename = user_path + user_files[res];
	}
	else if (res == int(files.size() - 2)) {
		filename = get_user_data_dir() + "/editor/maps/";
		const int res = gui::show_dialog(disp, NULL, "",
										 "Enter map to edit:", gui::OK_CANCEL,
										 NULL, NULL, "", &filename);
		if (res != 0) {
			return "";
		}
	}
	else if(res < 0 || files.empty()) {
		return "";
	}
	else {
		filename = system_path + files[res];
	}
	return filename;
}

void preferences_dialog(display &disp, config &prefs) {
	const events::resize_lock prevent_resizing;
	const events::event_context dialog_events_context;
	const gui::dialog_manager dialog_mgr;
	
	const int width = 600;
	const int height = 200;
	const int xpos = disp.x()/2 - width/2;
	const int ypos = disp.y()/2 - height/2;

	SDL_Rect clip_rect = {0,0,disp.x(),disp.y()};

	gui::button close_button(disp,string_table["close_window"]);

	std::vector<gui::button*> buttons;
	buttons.push_back(&close_button);

	surface_restorer restorer;
	gui::draw_dialog(xpos,ypos,width,height,disp,string_table["preferences"],NULL,&buttons,&restorer);

	const std::string& scroll_label = string_table["scroll_speed"];

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
	gui::slider scroll_slider(disp,slider_rect);
	scroll_slider.set_min(1);
	scroll_slider.set_max(100);
	scroll_slider.set_value(preferences::scroll_speed());

	gui::button fullscreen_button(disp,string_table["full_screen"],
	                              gui::button::TYPE_CHECK);

	fullscreen_button.set_check(preferences::fullscreen());

	fullscreen_button.set_location(slider_left,scroll_pos + 80);

	gui::button grid_button(disp,string_table["grid_button"],
	                        gui::button::TYPE_CHECK);
	grid_button.set_check(preferences::grid());

	grid_button.set_location(slider_left + fullscreen_button.width() + 100,
							 scroll_pos + 80);

	gui::button resolution_button(disp,string_table["video_mode"]);
	resolution_button.set_location(slider_left,scroll_pos + 80 + 50);

	gui::button hotkeys_button (disp,string_table["hotkeys_button"]);
	hotkeys_button.set_location(slider_left + fullscreen_button.width() + 100,
								scroll_pos + 80 + 50);

	bool redraw_all = true;

	for(;;) {
		int mousex, mousey;
		const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

		const bool left_button = mouse_flags&SDL_BUTTON_LMASK;

		if(close_button.process(mousex,mousey,left_button)) {
			break;
		}

		if(fullscreen_button.process(mousex,mousey,left_button)) {
			//the underlying frame buffer is changing, so cancel
			//the surface restorer restoring the frame buffer state
			restorer.cancel();
			preferences::set_fullscreen(fullscreen_button.checked());
			redraw_all = true;
		}

		if(redraw_all) {
			restorer.cancel();
			gui::draw_dialog(xpos,ypos,width,height,disp,string_table["preferences"],NULL,&buttons,&restorer);
			fullscreen_button.set_dirty();
			close_button.set_dirty();
			resolution_button.set_dirty();
			grid_button.set_dirty();
			hotkeys_button.set_dirty();
			scroll_slider.set_dirty();

			font::draw_text(&disp,clip_rect,14,font::NORMAL_COLOUR,scroll_label,
	    	                scroll_rect.x,scroll_rect.y);

			update_rect(disp.screen_area());

			redraw_all = false;
		}

		if(grid_button.process(mousex,mousey,left_button)) {
			preferences::set_grid(grid_button.checked());
		}

		if(resolution_button.process(mousex,mousey,left_button)) {
			const bool mode_changed = preferences::show_video_mode_dialog(disp);
			if(mode_changed) {
				//the underlying frame buffer is changing, so cancel
				//the surface restorer restoring the frame buffer state
				restorer.cancel();
			}
			break;
		}

		if(hotkeys_button.process(mousex,mousey,left_button)) {
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




}

