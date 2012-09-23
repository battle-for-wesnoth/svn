/* $Id$ */
/*
   Copyright (C) 2003 - 2012 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

/**
 *  @file
 *  Manage display-related preferences, e.g. screen-size, etc.
 */

#include "global.hpp"
#include "preferences_display.hpp"

#include "construct_dialog.hpp"
#include "display.hpp"
#include "formatter.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "hotkeys.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"

#include <boost/foreach.hpp>

namespace preferences {

display* disp = NULL;

display_manager::display_manager(display* d)
{
	disp = d;

	load_hotkeys();

	set_grid(grid());
	set_turbo(turbo());
	set_turbo_speed(turbo_speed());
	set_fullscreen(fullscreen());
	set_scroll_to_action(scroll_to_action());
	set_color_cursors(preferences::get("color_cursors", false));
}

display_manager::~display_manager()
{
	disp = NULL;
}

bool detect_video_settings(CVideo& video, std::pair<int,int>& resolution, int& bpp, int& video_flags)
{
	video_flags = fullscreen() ? FULL_SCREEN : 0;
	resolution = preferences::resolution();

	int DefaultBPP = 24;
	const SDL_VideoInfo* const video_info = SDL_GetVideoInfo();
	if(video_info != NULL && video_info->vfmt != NULL) {
		DefaultBPP = video_info->vfmt->BitsPerPixel;
	}

	std::cerr << "Checking video mode: " << resolution.first << 'x'
		<< resolution.second << 'x' << DefaultBPP << "...\n";

	typedef std::pair<int, int> res_t;
	std::vector<res_t> res_list;
	res_list.push_back(res_t(1024, 768));
	res_list.push_back(res_t(1024, 600));
	res_list.push_back(res_t(800, 600));
	res_list.push_back(res_t(800, 480));

	bpp = video.modePossible(resolution.first, resolution.second,
		DefaultBPP, video_flags, true);

	BOOST_FOREACH(const res_t &res, res_list)
	{
		if (bpp != 0) break;
		std::cerr << "Video mode " << resolution.first << 'x'
			<< resolution.second << 'x' << DefaultBPP
			<< " is not supported; attempting " << res.first
			<< 'x' << res.second << 'x' << DefaultBPP << "...\n";
		resolution = res;
		bpp = video.modePossible(resolution.first, resolution.second,
			DefaultBPP, video_flags);
	}

	return bpp != 0;
}

void set_fullscreen(CVideo& video, const bool ison)
{
	_set_fullscreen(ison);

	const std::pair<int,int>& res = resolution();
	if(video.isFullScreen() != ison) {
		const int flags = ison ? FULL_SCREEN : 0;
		int bpp = video.bppForMode(res.first, res.second, flags);

		if(bpp > 0) {
			video.setMode(res.first,res.second,bpp,flags);
			if(disp) {
				disp->redraw_everything();
			}
		} else {
			int tmp_flags = flags;
			std::pair<int,int> tmp_res;
			if(detect_video_settings(video, tmp_res, bpp, tmp_flags)) {
				set_resolution(video, tmp_res.first, tmp_res.second);
			// TODO: see if below line is actually needed, possibly for displays that only support 16 bbp
			} else if(video.modePossible(1024,768,16,flags)) {
				set_resolution(video, 1024, 768);
			} else {
				gui2::show_transient_message(video,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."));
			}
			// We reinit color cursors, because SDL on Mac seems to forget the SDL_Cursor
			set_color_cursors(preferences::get("color_cursors", false));
		}
	}
}

void set_fullscreen(bool ison)
{
	_set_fullscreen(ison);

	if(disp != NULL) {
		set_fullscreen(disp->video(), ison);
	}
}

void set_scroll_to_action(bool ison)
{
	_set_scroll_to_action(ison);
}
void set_resolution(const std::pair<int,int>& resolution)
{
	if(disp) {
		set_resolution(disp->video(), resolution.first, resolution.second);
	} else {
		/* This part is needed when wesnoth is started with the -r parameter. */
		const std::string postfix = fullscreen() ? "resolution" : "windowsize";
		preferences::set(
				'x' + postfix, lexical_cast<std::string>(resolution.first));
		preferences::set(
				'y' + postfix, lexical_cast<std::string>(resolution.second));
	}
}

bool set_resolution(CVideo& video
		, const unsigned width, const unsigned height)
{
	SDL_Rect rect;
	SDL_GetClipRect(video.getSurface(), &rect);
	if(rect.w == width && rect.h == height) {
		return true;
	}

	const int flags = fullscreen() ? FULL_SCREEN : 0;
	int bpp = video.bppForMode(width, height, flags);

	if(bpp != 0) {
		video.setMode(width, height, bpp, flags);

		if(disp) {
			disp->redraw_everything();
		}

	} else {
        // grzywacz: is this even true?
		gui2::show_transient_message(video,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."));
		return false;
	}

	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	preferences::set('x' + postfix, lexical_cast<std::string>(width));
	preferences::set('y' + postfix, lexical_cast<std::string>(height));

	return true;
}

void set_turbo(bool ison)
{
	_set_turbo(ison);

	if(disp != NULL) {
		disp->set_turbo(ison);
	}
}

void set_turbo_speed(double speed)
{
	save_turbo_speed(speed);

	if(disp != NULL) {
		disp->set_turbo_speed(speed);
	}
}

void set_ellipses(bool ison)
{
	_set_ellipses(ison);
}

void set_grid(bool ison)
{
	_set_grid(ison);

	if(disp != NULL) {
		disp->set_grid(ison);
	}
}

void set_color_cursors(bool value)
{
	_set_color_cursors(value);

	cursor::set();
}

void set_idle_anim(bool ison) {
	_set_idle_anim(ison);
	if(disp != NULL) {
		disp->set_idle_anim(ison);
	}
}

void set_idle_anim_rate(int rate) {
	_set_idle_anim_rate(rate);
	if(disp != NULL) {
		disp->set_idle_anim_rate(rate);
	}
}

namespace {
class escape_handler : public events::handler {
public:
	escape_handler() : escape_pressed_(false) {}
	bool escape_pressed() const { return escape_pressed_; }
	void handle_event(const SDL_Event &event) { escape_pressed_ |= (event.type == SDL_KEYDOWN)
		&& (reinterpret_cast<const SDL_KeyboardEvent&>(event).keysym.sym == SDLK_ESCAPE); }
private:
	bool escape_pressed_;
};

void repopulate_hotkeys_menu(std::vector<std::string>& menu_items,
		std::vector<std::string>& item_commands)
{
	menu_items.clear();
	item_commands.clear();

	const hotkey::hotkey_command* list = hotkey::get_hotkey_commands();

	for (size_t i = 0; list[i].id != hotkey::HOTKEY_NULL; ++i) {

		const hotkey::scope& scope = list[i].scope;
		if (list[i].hidden || !(hotkey::is_scope_active(scope)) )
				continue;

		const std::string& description = hotkey::get_description(list[i].command);
		const std::string& name = hotkey::get_names(list[i].id);

		menu_items.push_back((formatter() << description << COLUMN_SEPARATOR << font::NULL_MARKUP << name).str());
		item_commands.push_back(list[i].command);
	}
	menu_items.push_back((formatter() << HEADING_PREFIX << _("Action") << COLUMN_SEPARATOR << _("Binding")).str());
}

} // end anonymous namespace


bool compare_resolutions(const std::pair<int,int>& lhs, const std::pair<int,int>& rhs)
{
	return lhs.first*lhs.second < rhs.first*rhs.second;
}

bool show_video_mode_dialog(display& disp)
{
	const resize_lock prevent_resizing;
	const events::event_context dialog_events_context;

	CVideo& video = disp.video();

	SDL_PixelFormat format = *video.getSurface()->format;
	format.BitsPerPixel = video.getBpp();

	const SDL_Rect* const * modes = SDL_ListModes(&format,FULL_SCREEN);

	// The SDL documentation says that a return value of -1
	// means that all dimensions are supported/possible.
	if(modes == reinterpret_cast<SDL_Rect**>(-1)) {
		std::cerr << "Can support any video mode\n";
		// SDL says that all modes are possible, so it's OK to use a
		// hardcoded list here. Include tiny and small gui since they
		// will be filtered out later if not needed.
		static const SDL_Rect scr_modes[] = {
			{ 0, 0,  320, 240  },
			{ 0, 0,  640, 480  },
			{ 0, 0,  800, 480  },	// small-gui (EeePC resolution)
			{ 0, 0,  800, 600  },
			{ 0, 0, 1024, 600  },	// used on many netbooks
			{ 0, 0, 1024, 768  },
			{ 0, 0, 1280, 960  },
			{ 0, 0, 1280, 1024 },
			{ 0, 0, 1366, 768  },	// 16:9 notebooks
			{ 0, 0, 1440, 900  },
			{ 0, 0, 1440, 1200 },
			{ 0, 0, 1600, 1200 },
			{ 0, 0, 1680, 1050 },
			{ 0, 0, 1920, 1080 },
			{ 0, 0, 1920, 1200 },
			{ 0, 0, 2560, 1600 }
		};
		static const SDL_Rect * const scr_modes_list[] = {
			&scr_modes[0],
			&scr_modes[1],
			&scr_modes[2],
			&scr_modes[3],
			&scr_modes[4],
			&scr_modes[5],
			&scr_modes[6],
			&scr_modes[7],
			&scr_modes[8],
			&scr_modes[9],
			&scr_modes[10],
			&scr_modes[11],
			&scr_modes[12],
			&scr_modes[13],
			&scr_modes[14],
			&scr_modes[15],
			NULL
		};

		modes = scr_modes_list;
	} else if(modes == NULL) {
		std::cerr << "No modes supported\n";
		gui2::show_transient_message(disp.video(),"",_("There are no alternative video modes available"));
		return false;
	}

	std::vector<std::pair<int,int> > resolutions;

	for(int i = 0; modes[i] != NULL; ++i) {
		if(modes[i]->w >= min_allowed_width() && modes[i]->h >= min_allowed_height()) {
			resolutions.push_back(std::pair<int,int>(modes[i]->w,modes[i]->h));
		}
	}

	const std::pair<int,int> current_res(video.getSurface()->w,video.getSurface()->h);
	resolutions.push_back(current_res);

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<std::string> options;
	unsigned current_choice = 0;

	for(size_t k = 0; k < resolutions.size(); ++k) {
		std::pair<int, int> const& res = resolutions[k];
		std::ostringstream option;

		if (res == current_res)
			current_choice = static_cast<unsigned>(k);

		option << res.first << utils::unicode_multiplication_sign << res.second;
		/*widescreen threshold is 16:10*/
		if (static_cast<double>(res.first)/res.second >= 16.0/10.0)
		  option << _(" (widescreen)");
		options.push_back(option.str());
	}

	gui2::tsimple_item_selector dlg(_("Choose Resolution"), "", options);
	dlg.set_selected_index(current_choice);
	dlg.show(disp.video());

	int choice = dlg.selected_index();

	if(choice == -1 || resolutions[static_cast<size_t>(choice)] == current_res) {
		return false;
	}

	set_resolution(resolutions[static_cast<size_t>(choice)]);
	return true;
}

} // end namespace preferences

