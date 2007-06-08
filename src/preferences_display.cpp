/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "cursor.hpp"
#include "display.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "marked-up_text.hpp"
#include "preferences_display.hpp"
#include "show_dialog.hpp"
#include "video.hpp"
#include "wml_separators.hpp"
#include "widgets/button.hpp"
#include "widgets/label.hpp"
#include "widgets/menu.hpp"
#include "widgets/slider.hpp"
#include "widgets/textbox.hpp"
#include "theme.hpp"

#include <vector>
#include <string>

namespace preferences {

static void set_gamma(int gamma);
static void set_turbo_speed(double speed);


display* disp = NULL;

display_manager::display_manager(display* d)
{
	disp = d;

	load_hotkeys();

	set_grid(grid());
	set_turbo(turbo());
	set_turbo_speed(turbo_speed());
	set_fullscreen(fullscreen());
	set_gamma(gamma());
	set_colour_cursors(preferences::get("colour_cursors") == "yes");
	save_show_lobby_minimaps(preferences::get("lobby_minimaps") != "no");
}

display_manager::~display_manager()
{
	disp = NULL;
}

void set_fullscreen(bool ison)
{
	_set_fullscreen(ison);

	if(disp != NULL) {
		const std::pair<int,int>& res = resolution();
		CVideo& video = disp->video();
		if(video.isFullScreen() != ison) {
			const int flags = ison ? FULL_SCREEN : 0;
			const int bpp = video.modePossible(res.first,res.second,16,flags);
			if(bpp > 0) {
				video.setMode(res.first,res.second,bpp,flags);
				disp->redraw_everything();
			} else if(video.modePossible(1024,768,16,flags)) {
				set_resolution(std::pair<int,int>(1024,768));
			} else {
				gui::show_dialog(*disp,NULL,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."),
				                 gui::MESSAGE);
			}
		}
	}
}

void set_resolution(const std::pair<int,int>& resolution)
{
	std::pair<int,int> res = resolution;

	// - Ayin: disabled the following code. Why would one want to enforce that?
	// Some 16:9, or laptop screens, may have resolutions which do not
	// comply to this rule (see bug 10630). I'm commenting this until it
	// proves absolutely necessary.
	//
	//make sure resolutions are always divisible by 4
	//res.first &= ~3;
	//res.second &= ~3;

	bool write_resolution = true;

	if(disp != NULL) {
		CVideo& video = disp->video();
		const int flags = fullscreen() ? FULL_SCREEN : 0;
		const int bpp = video.modePossible(res.first,res.second,16,flags);
		if(bpp != 0) {
			video.setMode(res.first,res.second,bpp,flags);
			disp->redraw_everything();

		} else {
			write_resolution = false;
			gui::show_dialog(*disp,NULL,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."),gui::MESSAGE);
		}
	}

	if(write_resolution) {
		const std::string postfix = fullscreen() ? "resolution" : "windowsize";
		preferences::set('x' + postfix, lexical_cast<std::string>(res.first));
		preferences::set('y' + postfix, lexical_cast<std::string>(res.second));
	}
}

void set_turbo(bool ison)
{
	_set_turbo(ison);

	if(disp != NULL) {
		disp->set_turbo(ison);
	}
}

static void set_turbo_speed(double speed)
{
	save_turbo_speed(speed);

	if(disp != NULL) {
		disp->set_turbo_speed(speed);
	}
}

static void set_adjust_gamma(bool val)
{
	//if we are turning gamma adjustment off, then set it to '1.0'
	if(val == false && adjust_gamma()) {
		CVideo& video = disp->video();
		video.setGamma(1.0);
	}

	_set_adjust_gamma(val);
}

static void set_gamma(int gamma)
{
	_set_gamma(gamma);

	if(adjust_gamma()) {
		CVideo& video = disp->video();
		video.setGamma((float)gamma / 100);
	}
}

void set_grid(bool ison)
{
	_set_grid(ison);

	if(disp != NULL) {
		disp->set_grid(ison);
	}
}

static void set_lobby_joins(int ison)
{
	_set_lobby_joins(ison);
}

static void set_sort_list(bool ison)
{
	_set_sort_list(ison);
}

static void set_iconize_list(bool ison)
{
	_set_iconize_list(ison);
}

void set_colour_cursors(bool value)
{
	_set_colour_cursors(value);

	cursor::use_colour(value);
}

namespace {

class preferences_dialog : public gui::preview_pane
{
public:
	preferences_dialog(display& disp, const config& game_cfg);

	struct video_mode_change_exception
	{
		enum TYPE { CHANGE_RESOLUTION, MAKE_FULLSCREEN, MAKE_WINDOWED };

		video_mode_change_exception(TYPE type) : type(type)
		{}

		TYPE type;
	};

	virtual handler_vector handler_members();
private:

	void process_event();
	bool left_side() const { return false; }
	void set_selection(int index);
	void update_location(SDL_Rect const &rect);
	const config* get_advanced_pref() const;
	void set_advanced_menu();
	void set_friends_menu();
	std::vector<std::string> friends_names_;

//	
	// change
	gui::slider music_slider_, sound_slider_, UI_sound_slider_, bell_slider_, scroll_slider_,
				gamma_slider_, chat_lines_slider_, buffer_size_slider_;
	gui::list_slider<double> turbo_slider_;
	gui::button fullscreen_button_, turbo_button_, show_ai_moves_button_, 
	  		show_grid_button_, save_replays_button_, delete_autosaves_button_,
			lobby_minimaps_button_, show_lobby_joins_button1_, 
			show_lobby_joins_button2_, show_lobby_joins_button3_, 
			sort_list_by_group_button_, iconize_list_button_, 
			friends_list_button_, friends_back_button_, 
			friends_add_friend_button_, friends_add_ignore_button_,
			friends_remove_button_, show_floating_labels_button_, 
			turn_dialog_button_, turn_bell_button_, 
			show_team_colours_button_, show_colour_cursors_button_,
			show_haloing_button_, video_mode_button_, 
			theme_button_, hotkeys_button_, gamma_button_,
			flip_time_button_, advanced_button_, sound_button_, 
			music_button_, chat_timestamp_button_,
			advanced_sound_button_, normal_sound_button_, 
			UI_sound_button_, sample_rate_button1_, 
			sample_rate_button2_, sample_rate_button3_, 
			confirm_sound_button_;
	gui::label music_label_, sound_label_, UI_sound_label_, bell_label_, scroll_label_,
				gamma_label_, chat_lines_label_, turbo_slider_label_,
				sample_rate_label_, buffer_size_label_;
	gui::textbox sample_rate_input_, friends_input_;

	unsigned slider_label_width_;

	gui::menu advanced_, friends_;
	int advanced_selection_, friends_selection_;

	enum TAB {	GENERAL_TAB, DISPLAY_TAB, SOUND_TAB, MULTIPLAYER_TAB, ADVANCED_TAB,
				/*extra tab*/
				ADVANCED_SOUND_TAB, FRIENDS_TAB};
	TAB tab_;
	display &disp_;
	const config& game_cfg_;
};

//change
preferences_dialog::preferences_dialog(display& disp, const config& game_cfg)
	: gui::preview_pane(disp.video()),
	  music_slider_(disp.video()), sound_slider_(disp.video()), UI_sound_slider_(disp.video()),
	  bell_slider_(disp.video()),
	  scroll_slider_(disp.video()), gamma_slider_(disp.video()), chat_lines_slider_(disp.video()),
	  buffer_size_slider_(disp.video()), turbo_slider_(disp.video()), 

	  fullscreen_button_(disp.video(), _("Toggle Full Screen"), gui::button::TYPE_CHECK),
	  turbo_button_(disp.video(), _("Accelerated Speed"), gui::button::TYPE_CHECK),
	  show_ai_moves_button_(disp.video(), _("Skip AI Moves"), gui::button::TYPE_CHECK),
	  show_grid_button_(disp.video(), _("Show Grid"), gui::button::TYPE_CHECK),
	  save_replays_button_(disp.video(), _("Save Replays"), gui::button::TYPE_CHECK),
	  delete_autosaves_button_(disp.video(), _("Delete Autosaves"), gui::button::TYPE_CHECK),
	  lobby_minimaps_button_(disp.video(), _("Show Lobby Minimaps"), gui::button::TYPE_CHECK),
	  show_lobby_joins_button1_(disp.video(), _("Do Not Show Lobby Joins"), gui::button::TYPE_CHECK),
	  show_lobby_joins_button2_(disp.video(), _("Show Lobby Joins Of Friends Only"), gui::button::TYPE_CHECK),
	  show_lobby_joins_button3_(disp.video(), _("Show All Lobby Joins"), gui::button::TYPE_CHECK),
	  sort_list_by_group_button_(disp.video(), _("Sort Lobby List"), gui::button::TYPE_CHECK),
	  iconize_list_button_(disp.video(), _("Iconize Lobby List"), gui::button::TYPE_CHECK),
	  friends_list_button_(disp.video(), _("Friends List")),
	  friends_back_button_(disp.video(), _("Multiplayer Options")),
	  friends_add_friend_button_(disp.video(), _("Add As Friend")),
	  friends_add_ignore_button_(disp.video(), _("Add As Ignore")),
	  friends_remove_button_(disp.video(), _("Remove")),
	  show_floating_labels_button_(disp.video(), _("Show Floating Labels"), gui::button::TYPE_CHECK),
	  turn_dialog_button_(disp.video(), _("Turn Dialog"), gui::button::TYPE_CHECK),
	  turn_bell_button_(disp.video(), _("Turn Bell"), gui::button::TYPE_CHECK),
	  show_team_colours_button_(disp.video(), _("Show Team Colors"), gui::button::TYPE_CHECK),
	  show_colour_cursors_button_(disp.video(), _("Show Color Cursors"), gui::button::TYPE_CHECK),
	  show_haloing_button_(disp.video(), _("Show Haloing Effects"), gui::button::TYPE_CHECK),
	  video_mode_button_(disp.video(), _("Video Mode")),
	  theme_button_(disp.video(), _("Theme")),
	  hotkeys_button_(disp.video(), _("Hotkeys")),
	  gamma_button_(disp.video(), _("Adjust Gamma"), gui::button::TYPE_CHECK),
	  flip_time_button_(disp.video(), _("Reverse Time Graphics"), gui::button::TYPE_CHECK),
	  advanced_button_(disp.video(), "", gui::button::TYPE_CHECK),
	  sound_button_(disp.video(), _("Sound effects"), gui::button::TYPE_CHECK),
	  music_button_(disp.video(), _("Music"), gui::button::TYPE_CHECK),
	  chat_timestamp_button_(disp.video(), _("Chat Timestamping"), gui::button::TYPE_CHECK),
	  advanced_sound_button_(disp.video(), _("Advanced Mode")),
	  normal_sound_button_(disp.video(), _("Normal Mode")),
	  UI_sound_button_(disp.video(), _("User Interface Sounds"), gui::button::TYPE_CHECK),
	  sample_rate_button1_(disp.video(), "22050", gui::button::TYPE_CHECK),
	  sample_rate_button2_(disp.video(), "44100", gui::button::TYPE_CHECK),
	  sample_rate_button3_(disp.video(), _("Custom"), gui::button::TYPE_CHECK),
	  confirm_sound_button_(disp.video(), _("Apply")),

	  music_label_(disp.video(), _("Music Volume:")), sound_label_(disp.video(), _("SFX Volume:")),
	  UI_sound_label_(disp.video(), _("UI Sound Volume:")),
	  bell_label_(disp.video(), _("Bell Volume:")), scroll_label_(disp.video(), _("Scroll Speed:")),
	  gamma_label_(disp.video(), _("Gamma:")), chat_lines_label_(disp.video(), ""),
	  turbo_slider_label_(disp.video(), "", font::SIZE_SMALL ),
	  sample_rate_label_(disp.video(), _("Sample Rate (Hz):")), buffer_size_label_(disp.video(), ""),

	  sample_rate_input_(disp.video(), 70),
	  friends_input_(disp.video(), 170),

	  slider_label_width_(0),
	  advanced_(disp.video(),std::vector<std::string>(),false,-1,-1,NULL,&gui::menu::bluebg_style),
	  friends_(disp.video(),std::vector<std::string>(),false,-1,-1,NULL,&gui::menu::bluebg_style),
	  
	  advanced_selection_(-1),
	  friends_selection_(-1),

	  tab_(GENERAL_TAB), disp_(disp), game_cfg_(game_cfg)
{
	// FIXME: this box should be vertically centered on the screen, but is not
#ifdef USE_TINY_GUI
	set_measurements(180, 180);		  // FIXME: should compute this, but using what data ?
#else
	set_measurements(440, 440);
#endif


	sound_button_.set_check(sound_on());
	sound_button_.set_help_string(_("Sound effects on/off"));
	sound_slider_.set_min(0);
	sound_slider_.set_max(128);
	sound_slider_.set_value(sound_volume());
	sound_slider_.set_help_string(_("Change the sound effects volume"));

	music_button_.set_check(music_on());
	music_button_.set_help_string(_("Music on/off"));
	music_slider_.set_min(0);
	music_slider_.set_max(128);
	music_slider_.set_value(music_volume());
	music_slider_.set_help_string(_("Change the music volume"));

	// bell volume slider
	bell_slider_.set_min(0);
	bell_slider_.set_max(128);
	bell_slider_.set_value(bell_volume());
	bell_slider_.set_help_string(_("Change the bell volume"));

	UI_sound_button_.set_check(UI_sound_on());
	UI_sound_button_.set_help_string(_("Turn menu and button sounds on/off"));
	UI_sound_slider_.set_min(0);
	UI_sound_slider_.set_max(128);
	UI_sound_slider_.set_value(UI_volume());
	UI_sound_slider_.set_help_string(_("Change the sound volume for button clicks, etc."));

	sample_rate_label_.set_help_string(_("Change the sample rate"));
	std::string rate = lexical_cast<std::string>(sample_rate());
	if (rate == "22050")
		sample_rate_button1_.set_check(true);
	else if (rate == "44100")
		sample_rate_button2_.set_check(true);
	else
		sample_rate_button3_.set_check(true);
	sample_rate_input_.set_text(rate);
	sample_rate_input_.set_help_string(_("User defined sample rate"));
	confirm_sound_button_.enable(sample_rate_button3_.checked());

	buffer_size_slider_.set_min(0);
	buffer_size_slider_.set_max(3);
	int v = sound_buffer_size()/512 - 1;
	buffer_size_slider_.set_value(v);
	//avoid sound reset the first time we load advanced sound
	buffer_size_slider_.value_change();
	buffer_size_slider_.set_help_string(_("Change the buffer size"));
	std::stringstream buf;
	buf << _("Buffer Size: ") << sound_buffer_size();
	buffer_size_label_.set_text(buf.str());
	buffer_size_label_.set_help_string(_("Change the buffer size"));

	scroll_slider_.set_min(1);
	scroll_slider_.set_max(100);
	scroll_slider_.set_value(scroll_speed());
	scroll_slider_.set_help_string(_("Change the speed of scrolling around the map"));

	chat_lines_slider_.set_min(1);
	chat_lines_slider_.set_max(20);
	chat_lines_slider_.set_value(chat_lines());
	chat_lines_slider_.set_help_string(_("Set the amount of chat lines shown"));
	// Have the tooltip appear over the static "Chat lines" label, too.
	chat_lines_label_.set_help_string(_("Set the amount of chat lines shown"));

	chat_timestamp_button_.set_check(chat_timestamp());
	chat_timestamp_button_.set_help_string(_("Add a timestamp to chat messages"));

	gamma_button_.set_check(adjust_gamma());
	gamma_button_.set_help_string(_("Change the brightness of the display"));

	gamma_slider_.set_min(50);
	gamma_slider_.set_max(200);
	gamma_slider_.set_value(gamma());
	gamma_slider_.set_help_string(_("Change the brightness of the display"));

	fullscreen_button_.set_check(fullscreen());
	fullscreen_button_.set_help_string(_("Choose whether the game should run full screen or in a window"));

	turbo_button_.set_check(turbo());
	turbo_button_.set_help_string(_("Make units move and fight faster"));
	
	//0.25 0.5 1 2 4 8 16 
	std::vector< double > turbo_items;
	turbo_items.push_back(0.25);
	turbo_items.push_back(0.5);
	turbo_items.push_back(1);
	turbo_items.push_back(2);
	turbo_items.push_back(4);
	turbo_items.push_back(8);
	turbo_items.push_back(16);
	turbo_slider_.set_items(turbo_items);
	if(!turbo_slider_.select_item(turbo_speed())) {
		turbo_slider_.select_item(1);
	}
	turbo_slider_.set_help_string(_("Units move and fight speed"));

	show_ai_moves_button_.set_check(!show_ai_moves());
	show_ai_moves_button_.set_help_string(_("Do not animate AI units moving"));

	save_replays_button_.set_check(save_replays());
	save_replays_button_.set_help_string(_("Save replays at scenario end."));

	delete_autosaves_button_.set_check(delete_autosaves());
	delete_autosaves_button_.set_help_string(_("Do not animate AI units moving"));
	show_grid_button_.set_check(grid());
	show_grid_button_.set_help_string(_("Overlay a grid onto the map"));

	lobby_minimaps_button_.set_check(show_lobby_minimaps());
	lobby_minimaps_button_.set_help_string(_("Show minimaps in the multiplayer lobby"));

	sort_list_by_group_button_.set_check(sort_list());
	sort_list_by_group_button_.set_help_string(_("Sort the player list in the lobby by player groups"));

	iconize_list_button_.set_check(iconize_list());
	iconize_list_button_.set_help_string(_("Show icons in front of the player names in the lobby."));

	show_lobby_joins_button1_.set_check(lobby_joins() == SHOW_NON);
	show_lobby_joins_button1_.set_help_string(_("Do not show messages about players joining the multiplayer lobby"));
	show_lobby_joins_button2_.set_check(lobby_joins() == SHOW_FRIENDS);
	show_lobby_joins_button2_.set_help_string(_("Show messages about your friends joining the multiplayer lobby"));
	show_lobby_joins_button3_.set_check(lobby_joins() == SHOW_ALL);
	show_lobby_joins_button3_.set_help_string(_("Show messages about all players joining the multiplayer lobby"));
	
	friends_list_button_.set_help_string(_("View and edit your friends and ignores list"));
	friends_back_button_.set_help_string(_("Back to the multiplayer options"));
	friends_add_friend_button_.set_help_string(_("Add this username to your friends list"));
	friends_add_ignore_button_.set_help_string(_("Add this username to your ignores list"));
	friends_remove_button_.set_help_string(_("Remove this username from your list"));

	friends_input_.set_text("");
	friends_input_.set_help_string(_("Insert a username"));

	show_floating_labels_button_.set_check(show_floating_labels());
	show_floating_labels_button_.set_help_string(_("Show text above a unit when it is hit to display damage inflicted"));

	video_mode_button_.set_help_string(_("Change the resolution the game runs at"));
	theme_button_.set_help_string(_("Change the theme the game runs with"));

	turn_dialog_button_.set_check(turn_dialog());
	turn_dialog_button_.set_help_string(_("Display a dialog at the beginning of your turn"));

	turn_bell_button_.set_check(turn_bell());
	turn_bell_button_.set_help_string(_("Play a bell sound at the beginning of your turn"));

	show_team_colours_button_.set_check(show_side_colours());
	show_team_colours_button_.set_help_string(_("Show a colored circle around the base of each unit to show which side it is on"));

	flip_time_button_.set_check(flip_time());
	flip_time_button_.set_help_string(_("Choose whether the sun moves left-to-right or right-to-left"));

	show_colour_cursors_button_.set_check(use_colour_cursors());
	show_colour_cursors_button_.set_help_string(_("Use colored mouse cursors (may be slower)"));

	show_haloing_button_.set_check(show_haloes());
	show_haloing_button_.set_help_string(_("Use graphical special effects (may be slower)"));

	hotkeys_button_.set_help_string(_("View and configure keyboard shortcuts"));

	set_advanced_menu();
	set_friends_menu();
}

handler_vector preferences_dialog::handler_members()
{
	handler_vector h;
	h.push_back(&music_slider_);
	h.push_back(&sound_slider_);
	h.push_back(&bell_slider_);
	h.push_back(&UI_sound_slider_);
	h.push_back(&scroll_slider_);
	h.push_back(&gamma_slider_);
	h.push_back(&chat_lines_slider_);
	h.push_back(&turbo_slider_);
	h.push_back(&buffer_size_slider_);
	h.push_back(&fullscreen_button_);
	h.push_back(&turbo_button_);
	h.push_back(&show_ai_moves_button_);
	h.push_back(&save_replays_button_);
	h.push_back(&delete_autosaves_button_);
	h.push_back(&show_grid_button_);
	h.push_back(&lobby_minimaps_button_);
	h.push_back(&sort_list_by_group_button_);
	h.push_back(&iconize_list_button_);
	h.push_back(&show_lobby_joins_button1_);
	h.push_back(&show_lobby_joins_button2_);
	h.push_back(&show_lobby_joins_button3_);
	h.push_back(&friends_list_button_);
	h.push_back(&friends_back_button_);
	h.push_back(&friends_add_friend_button_);
	h.push_back(&friends_add_ignore_button_);
	h.push_back(&friends_remove_button_);
 	h.push_back(&friends_input_);		
	h.push_back(&show_floating_labels_button_);
	h.push_back(&turn_dialog_button_);
	h.push_back(&turn_bell_button_);
	h.push_back(&UI_sound_button_);
	h.push_back(&show_team_colours_button_);
	h.push_back(&show_colour_cursors_button_);
	h.push_back(&show_haloing_button_);
	h.push_back(&video_mode_button_);
	h.push_back(&theme_button_);
	h.push_back(&hotkeys_button_);
	h.push_back(&gamma_button_);
	h.push_back(&flip_time_button_);
	h.push_back(&advanced_button_);
	h.push_back(&sound_button_);
	h.push_back(&music_button_);
	h.push_back(&chat_timestamp_button_);
	h.push_back(&advanced_sound_button_);
	h.push_back(&normal_sound_button_);
	h.push_back(&sample_rate_button1_);
	h.push_back(&sample_rate_button2_);
	h.push_back(&sample_rate_button3_);
	h.push_back(&confirm_sound_button_);
	h.push_back(&music_label_);
	h.push_back(&sound_label_);
	h.push_back(&bell_label_);
	h.push_back(&UI_sound_label_);
	h.push_back(&scroll_label_);
	h.push_back(&gamma_label_);
	h.push_back(&turbo_slider_label_);
	h.push_back(&chat_lines_label_);
	h.push_back(&sample_rate_label_);
	h.push_back(&buffer_size_label_);
	h.push_back(&sample_rate_input_);
	h.push_back(&advanced_);
	h.push_back(&friends_);
	return h;
}

void preferences_dialog::update_location(SDL_Rect const &rect)
{
	bg_register(rect);


	const int right_border = font::relative_size(10);
	const int horizontal_padding = 25;
#if USE_TINY_GUI
	const int top_border = 14;
	const int bottom_border = 0;
	const int short_interline = 20;
	const int item_interline = 20;
#else
	const int top_border = 28;
	const int bottom_border = 40;
	const int short_interline = 20;
	const int item_interline = 50;
#endif
	const int bottom_row_y = rect.y + rect.h - bottom_border;

	// General tab
	int ypos = rect.y + top_border;
	scroll_label_.set_location(rect.x, ypos);
	SDL_Rect scroll_rect = { rect.x + scroll_label_.width(), ypos,
							rect.w - scroll_label_.width() - right_border, 0 };
	scroll_slider_.set_location(scroll_rect);
	ypos += item_interline; turbo_button_.set_location(rect.x, ypos);
	ypos += short_interline; turbo_slider_label_.set_location(rect.x + horizontal_padding, ypos);
	ypos += short_interline;
	SDL_Rect turbo_rect = { rect.x + horizontal_padding, ypos,
							rect.w - horizontal_padding - right_border, 0 };
	turbo_slider_.set_location(turbo_rect);
	ypos += item_interline; show_ai_moves_button_.set_location(rect.x, ypos);
	ypos += short_interline; turn_dialog_button_.set_location(rect.x, ypos);
	ypos += item_interline; show_team_colours_button_.set_location(rect.x, ypos);
	ypos += short_interline; show_grid_button_.set_location(rect.x, ypos);
	ypos += item_interline; save_replays_button_.set_location(rect.x, ypos);
	ypos += short_interline; delete_autosaves_button_.set_location(rect.x, ypos);
	hotkeys_button_.set_location(rect.x, bottom_row_y - hotkeys_button_.height());

	// Display tab
	ypos = rect.y + top_border;
	gamma_button_.set_location(rect.x, ypos);
	ypos += item_interline;
	gamma_label_.set_location(rect.x, ypos);
	SDL_Rect gamma_rect = { rect.x + gamma_label_.width(), ypos,
							rect.w - gamma_label_.width() - right_border, 0 };
	gamma_slider_.set_location(gamma_rect);
	ypos += item_interline; flip_time_button_.set_location(rect.x,ypos);
	ypos += item_interline; show_floating_labels_button_.set_location(rect.x, ypos);
	ypos += item_interline; show_colour_cursors_button_.set_location(rect.x, ypos);
	ypos += item_interline; show_haloing_button_.set_location(rect.x, ypos);
	ypos += item_interline; fullscreen_button_.set_location(rect.x, ypos);
	ypos += item_interline; video_mode_button_.set_location(rect.x, ypos);
	theme_button_.set_location(rect.x+video_mode_button_.width()+10, ypos);

	// Sound tab
	slider_label_width_ = maximum<unsigned>(music_label_.width(), sound_label_.width());
	slider_label_width_ = maximum<unsigned>(slider_label_width_, bell_label_.width());
	slider_label_width_ = maximum<unsigned>(slider_label_width_, UI_sound_label_.width());
	ypos = rect.y + top_border;
	sound_button_.set_location(rect.x, ypos);

	ypos += short_interline;
	sound_label_.set_location(rect.x, ypos);
	const SDL_Rect sound_rect = { rect.x + slider_label_width_, ypos,
								rect.w - slider_label_width_ - right_border, 0 };
	sound_slider_.set_location(sound_rect);

	ypos += item_interline;
	music_button_.set_location(rect.x, ypos);

	ypos += short_interline;
	music_label_.set_location(rect.x, ypos);
	const SDL_Rect music_rect = { rect.x + slider_label_width_, ypos,
								rect.w - slider_label_width_ - right_border, 0 };
	music_slider_.set_location(music_rect);

	ypos += item_interline; //Bell slider
	turn_bell_button_.set_location(rect.x, ypos);
	ypos += short_interline;
	bell_label_.set_location(rect.x, ypos);
	const SDL_Rect bell_rect = {rect.x + slider_label_width_, ypos,
								rect.w - slider_label_width_ - right_border, 0 };
	bell_slider_.set_location(bell_rect);

	ypos += item_interline; //UI sound slider
	UI_sound_button_.set_location(rect.x, ypos);
	ypos += short_interline;
	UI_sound_label_.set_location(rect.x, ypos);
	const SDL_Rect UI_sound_rect = {rect.x + slider_label_width_, ypos,
								rect.w - slider_label_width_ - right_border, 0 };
	UI_sound_slider_.set_location(UI_sound_rect);
	advanced_sound_button_.set_location(rect.x, bottom_row_y - advanced_sound_button_.height());


	//Advanced Sound tab
	ypos = rect.y + top_border;
	sample_rate_label_.set_location(rect.x, ypos);
	ypos += short_interline;
	int h_offset = rect.x + horizontal_padding;
	sample_rate_button1_.set_location(h_offset, ypos);
	ypos += short_interline;
	sample_rate_button2_.set_location(h_offset, ypos);
	ypos += short_interline;
	sample_rate_button3_.set_location(h_offset, ypos);
	h_offset += sample_rate_button3_.width() + 5;
	sample_rate_input_.set_location(h_offset, ypos);
	h_offset += sample_rate_input_.width() + 5;
	confirm_sound_button_.set_location(h_offset, ypos);

	ypos += item_interline;
	buffer_size_label_.set_location(rect.x, ypos);
	ypos += short_interline;
	SDL_Rect buffer_rect = {rect.x + horizontal_padding, ypos,
							rect.w - horizontal_padding - right_border, 0 };
	buffer_size_slider_.set_location(buffer_rect);
	ypos += item_interline;
	const int nsb_x = rect.x + rect.w - normal_sound_button_.width() - right_border;
	normal_sound_button_.set_location(nsb_x, ypos);


	// Multiplayer tab
	ypos = rect.y + top_border;
	chat_lines_label_.set_location(rect.x, ypos);
	ypos += short_interline;
	SDL_Rect chat_lines_rect = { rect.x + horizontal_padding, ypos,
								rect.w - horizontal_padding - right_border, 0 };
	chat_lines_slider_.set_location(chat_lines_rect);
	ypos += item_interline; chat_timestamp_button_.set_location(rect.x, ypos);
	ypos += item_interline; lobby_minimaps_button_.set_location(rect.x, ypos);
	ypos += item_interline; sort_list_by_group_button_.set_location(rect.x, ypos);
	ypos += item_interline; iconize_list_button_.set_location(rect.x, ypos);
	
	ypos += item_interline; show_lobby_joins_button1_.set_location(rect.x, ypos);
	ypos += short_interline; show_lobby_joins_button2_.set_location(rect.x, ypos);
	ypos += short_interline; show_lobby_joins_button3_.set_location(rect.x, ypos);
	
	friends_list_button_.set_location(rect.x, bottom_row_y - friends_list_button_.height());

	//Friends tab
	ypos = rect.y + top_border;
	friends_input_.set_location(rect.x,ypos);
	
	friends_.set_location(rect.x,ypos + item_interline);
	friends_.set_max_height(height()-100);
	
	int friends_xpos;
	
	if (friends_.width() > friends_input_.width()) {
		friends_xpos = rect.x+  friends_.width() + 20;
	} else {
		friends_xpos = rect.x+  friends_input_.width() + 20;
	}
	
	friends_add_friend_button_.set_location(friends_xpos,ypos);
	ypos += short_interline+3; friends_add_ignore_button_.set_location(friends_xpos,ypos);
	ypos += short_interline+3; friends_remove_button_.set_location(friends_xpos,ypos);
	ypos += item_interline*2; friends_back_button_.set_location(friends_xpos,ypos);
	
	//Advanced tab
	ypos = rect.y + top_border;
	advanced_.set_location(rect.x,ypos);
	advanced_.set_max_height(height()-100);

	ypos += advanced_.height() + font::relative_size(14);

	advanced_button_.set_location(rect.x,ypos);

	set_selection(tab_);
}

void preferences_dialog::process_event()
{
	if (tab_ == GENERAL_TAB) {
		if (turbo_button_.pressed()) {
			set_turbo(turbo_button_.checked());
			turbo_slider_.enable(turbo());
			turbo_slider_label_.enable(turbo());
		}
		if (show_ai_moves_button_.pressed())
			set_show_ai_moves(!show_ai_moves_button_.checked());
		if (show_grid_button_.pressed())
			set_grid(show_grid_button_.checked());
		if (turn_dialog_button_.pressed())
			set_turn_dialog(turn_dialog_button_.checked());
		if (show_team_colours_button_.pressed())
			set_show_side_colours(show_team_colours_button_.checked());
		if (hotkeys_button_.pressed())
			show_hotkeys_dialog(disp_);

		set_scroll_speed(scroll_slider_.value());
		set_turbo_speed(turbo_slider_.item_selected());

		std::stringstream buf;
		buf << _("Speed: ") << turbo_slider_.item_selected();
		turbo_slider_label_.set_text(buf.str());

		return;
	}

	if (tab_ == DISPLAY_TAB) {
		if (show_floating_labels_button_.pressed())
			set_show_floating_labels(show_floating_labels_button_.checked());
		if (video_mode_button_.pressed())
			throw video_mode_change_exception(video_mode_change_exception::CHANGE_RESOLUTION);
		if (theme_button_.pressed())
			show_theme_dialog(disp_);
		if (fullscreen_button_.pressed())
			throw video_mode_change_exception(fullscreen_button_.checked()
											? video_mode_change_exception::MAKE_FULLSCREEN
											: video_mode_change_exception::MAKE_WINDOWED);
		if (show_colour_cursors_button_.pressed())
			set_colour_cursors(show_colour_cursors_button_.checked());
		if (show_haloing_button_.pressed())
			set_show_haloes(show_haloing_button_.checked());
		if (gamma_button_.pressed()) {
			set_adjust_gamma(gamma_button_.checked());
			bool enable_gamma = adjust_gamma();
			gamma_slider_.enable(enable_gamma);
			gamma_label_.enable(enable_gamma);
		}
		if (flip_time_button_.pressed())
			set_flip_time(flip_time_button_.checked());

		set_gamma(gamma_slider_.value());

		return;
	}


	if (tab_ == SOUND_TAB) {
		if (turn_bell_button_.pressed()) {
			if(!set_turn_bell(turn_bell_button_.checked()))
				turn_bell_button_.set_check(false);
			bell_slider_.enable(turn_bell());
			bell_label_.enable(turn_bell());
		}
		if (sound_button_.pressed()) {
			if(!set_sound(sound_button_.checked()))
				sound_button_.set_check(false);
			sound_slider_.enable(sound_on());
			sound_label_.enable(sound_on());
		}
		if (UI_sound_button_.pressed()) {
			if(!set_UI_sound(UI_sound_button_.checked()))
				UI_sound_button_.set_check(false);
			UI_sound_slider_.enable(UI_sound_on());
			UI_sound_label_.enable(UI_sound_on());
		}
		set_sound_volume(sound_slider_.value());
		set_UI_volume(UI_sound_slider_.value());
		set_bell_volume(bell_slider_.value());

		if (music_button_.pressed()) {
			if(!set_music(music_button_.checked()))
				music_button_.set_check(false);
			music_slider_.enable(music_on());
			music_label_.enable(music_on());
		}
		set_music_volume(music_slider_.value());

		if (advanced_sound_button_.pressed())
			set_selection(ADVANCED_SOUND_TAB);

		return;
	}

	if (tab_ == ADVANCED_SOUND_TAB) {
		bool apply = false;
		std::string rate;

		if (sample_rate_button1_.pressed()) {
			if (sample_rate_button1_.checked()) {
				sample_rate_button2_.set_check(false);
				sample_rate_button3_.set_check(false);
				confirm_sound_button_.enable(false);
				apply = true;
				rate = "22050";
			} else
				sample_rate_button1_.set_check(true);
		}
		if (sample_rate_button2_.pressed()) {
			if (sample_rate_button2_.checked()) {
				sample_rate_button1_.set_check(false);
				sample_rate_button3_.set_check(false);
				confirm_sound_button_.enable(false);
				apply = true;
				rate = "44100";
			} else
				sample_rate_button2_.set_check(true);
		}
		if (sample_rate_button3_.pressed()) {
			if (sample_rate_button3_.checked()) {
				sample_rate_button1_.set_check(false);
				sample_rate_button2_.set_check(false);
				confirm_sound_button_.enable(true);
			} else
				sample_rate_button3_.set_check(true);
		}
		if (confirm_sound_button_.pressed()) {
			apply = true;
			rate = sample_rate_input_.text();
		}

		if (apply)
			try {
			save_sample_rate(lexical_cast<unsigned int>(rate));
			} catch (bad_lexical_cast&) {
			}

		if (buffer_size_slider_.value_change()) {
			const size_t buffer_size = 512 << buffer_size_slider_.value();
			save_sound_buffer_size(buffer_size);
			std::stringstream buf;
			buf << _("Buffer Size: ") << buffer_size;
			buffer_size_label_.set_text(buf.str());
		}

		if (normal_sound_button_.pressed())
			set_selection(SOUND_TAB);

		return;
	}

	if (tab_ == MULTIPLAYER_TAB) {
		if (lobby_minimaps_button_.pressed())
			save_show_lobby_minimaps(lobby_minimaps_button_.checked());
			
		if (show_lobby_joins_button1_.pressed()) {
			set_lobby_joins(SHOW_NON);
			show_lobby_joins_button1_.set_check(true);
			show_lobby_joins_button2_.set_check(false);
			show_lobby_joins_button3_.set_check(false);
        }
		if (show_lobby_joins_button2_.pressed()) {
			set_lobby_joins(SHOW_FRIENDS);
			show_lobby_joins_button1_.set_check(false);
			show_lobby_joins_button2_.set_check(true);
			show_lobby_joins_button3_.set_check(false);
        }
		if (show_lobby_joins_button3_.pressed()) {
			set_lobby_joins(SHOW_ALL);
			show_lobby_joins_button1_.set_check(false);
			show_lobby_joins_button2_.set_check(false);
			show_lobby_joins_button3_.set_check(true);
        }
		if (sort_list_by_group_button_.pressed())
			set_sort_list(sort_list_by_group_button_.checked());
		if (iconize_list_button_.pressed())
			set_iconize_list(iconize_list_button_.checked());
		if (chat_timestamp_button_.pressed())
			set_chat_timestamp(chat_timestamp_button_.checked());
		if (friends_list_button_.pressed())
			set_selection(FRIENDS_TAB);

		set_chat_lines(chat_lines_slider_.value());

		//display currently select amount of chat lines
		std::stringstream buf;
		buf << _("Chat Lines: ") << chat_lines_slider_.value();
		chat_lines_label_.set_text(buf.str());

		return;
	}

	if (tab_ == FRIENDS_TAB) {
 		if(friends_.selection() != friends_selection_) {
 			friends_selection_ = friends_.selection();
			std::stringstream ss;
			ss << friends_names_[friends_.selection()];
			if (ss.str() != "(empty list)") friends_input_.set_text(ss.str());
			else friends_input_.set_text("");
		}
		if (friends_back_button_.pressed())
			set_selection(MULTIPLAYER_TAB);
		if (friends_add_friend_button_.pressed()) {
			if (preferences::_set_relationship(friends_input_.text(), "friend")) {
				friends_input_.clear();
				set_friends_menu();
			} else {
          		friends_input_.set_text("Invalid username");
            }
        }
		if (friends_add_ignore_button_.pressed()) {
			if (preferences::_set_relationship(friends_input_.text(), "ignored")) {
				friends_input_.clear();
				set_friends_menu();
			} else {
          		friends_input_.set_text("Invalid username");
            }
        }
		if (friends_remove_button_.pressed()) {
			if (preferences::_set_relationship(friends_input_.text(), "no")) {
				friends_input_.clear();
				set_friends_menu();
			} else {
          		friends_input_.set_text("Invalid username");
            }
        }
		return;
	} 

	if (tab_ == ADVANCED_TAB) {
		if(advanced_.selection() != advanced_selection_) {
			advanced_selection_ = advanced_.selection();
			const config* const adv = get_advanced_pref();
			if(adv != NULL) {
				const config& pref = *adv;
				advanced_button_.set_width(0);
				advanced_button_.set_label(pref["name"]);
				std::string value = preferences::get(pref["field"]);
				if(value.empty()) {
					value = pref["default"];
				}

				advanced_button_.set_check(value == "yes");
			}
		}

		if(advanced_button_.pressed()) {
			const config* const adv = get_advanced_pref();
			if(adv != NULL) {
				const config& pref = *adv;
				preferences::set(pref["field"],
						advanced_button_.checked() ? "yes" : "no");
				set_advanced_menu();
			}
		}

		return;
	}
}

const config* preferences_dialog::get_advanced_pref() const
{
	const config::child_list& adv = game_cfg_.get_children("advanced_preference");
	if(advanced_selection_ >= 0 && advanced_selection_ < int(adv.size())) {
		return adv[advanced_selection_];
	} else {
		return NULL;
	}
}

void preferences_dialog::set_advanced_menu()
{
	std::vector<std::string> advanced_items;
	const config::child_list& adv = game_cfg_.get_children("advanced_preference");
	for(config::child_list::const_iterator i = adv.begin(); i != adv.end(); ++i) {
		std::ostringstream str;
		std::string field = preferences::get((**i)["field"]);
		if(field.empty()) {
			field = (**i)["default"];
		}

		if(field == "yes") {
			field = _("yes");
		} else if(field == "no") {
			field = _("no");
		}

		str << (**i)["name"] << COLUMN_SEPARATOR << field;
		advanced_items.push_back(str.str());
	}

	advanced_.set_items(advanced_items,true,true);
}

void preferences_dialog::set_friends_menu()
{
	std::vector<std::string> friends_items;
	std::vector<std::string> friends_names;
    if (!preferences::get_prefs()->child("relationship")){
		preferences::get_prefs()->add_child("relationship");
	}
	config* cignore;
	cignore = preferences::get_prefs()->child("relationship");

	std::string const imgpre = IMAGE_PREFIX + std::string("misc/status-");

	for(std::map<std::string,t_string>::const_iterator i = cignore->values.begin();
							i != cignore->values.end(); ++i){
		if (i->second == "friend"){
			friends_items.push_back(imgpre + "friend.png" + COLUMN_SEPARATOR + i->first + COLUMN_SEPARATOR + "friend");
			friends_names.push_back(i->first);
		}
		if (i->second == "ignored"){
			friends_items.push_back(imgpre + "ignore.png" + COLUMN_SEPARATOR + i->first + COLUMN_SEPARATOR + "ignored");
			friends_names.push_back(i->first);
		}
	}
	if (friends_items.empty()) {
		friends_items.push_back("(empty list)");
		friends_names.push_back("(empty list)");
	}
	friends_names_ = friends_names;
	friends_.set_items(friends_items,true,true);
}

void preferences_dialog::set_selection(int index)
{
	tab_ = TAB(index);
	set_dirty();
	bg_restore();

	const bool hide_general = tab_ != GENERAL_TAB;
	scroll_label_.hide(hide_general);
	scroll_slider_.hide(hide_general);
	turbo_button_.hide(hide_general);
	turbo_slider_label_.hide(hide_general);
	turbo_slider_.hide(hide_general);
	turbo_slider_label_.enable(turbo());
	turbo_slider_.enable(turbo());
	show_ai_moves_button_.hide(hide_general);
	turn_dialog_button_.hide(hide_general);
	hotkeys_button_.hide(hide_general);
	show_team_colours_button_.hide(hide_general);
	show_grid_button_.hide(hide_general);

	const bool hide_display = tab_ != DISPLAY_TAB;
	gamma_label_.hide(hide_display);
	gamma_slider_.hide(hide_display);
	gamma_label_.enable(adjust_gamma());
	gamma_slider_.enable(adjust_gamma());
	gamma_button_.hide(hide_display);
	show_floating_labels_button_.hide(hide_display);
	show_colour_cursors_button_.hide(hide_display);
	show_haloing_button_.hide(hide_display);
	fullscreen_button_.hide(hide_display);
	video_mode_button_.hide(hide_display);
	theme_button_.hide(hide_display);
	flip_time_button_.hide(hide_display);

	const bool hide_sound = tab_ != SOUND_TAB;
	music_button_.hide(hide_sound);
	music_label_.hide(hide_sound);
	music_slider_.hide(hide_sound);
	sound_button_.hide(hide_sound);
	sound_label_.hide(hide_sound);
	sound_slider_.hide(hide_sound);
	UI_sound_button_.hide(hide_sound);
	UI_sound_label_.hide(hide_sound);
	UI_sound_slider_.hide(hide_sound);
	turn_bell_button_.hide(hide_sound);
	bell_label_.hide(hide_sound);
	bell_slider_.hide(hide_sound);
	music_slider_.enable(music_on());
	bell_slider_.enable(turn_bell());
	sound_slider_.enable(sound_on());
	UI_sound_slider_.enable(UI_sound_on());
	music_label_.enable(music_on());
	bell_label_.enable(turn_bell());
	sound_label_.enable(sound_on());
	UI_sound_label_.enable(UI_sound_on());
	advanced_sound_button_.hide(hide_sound);

	const bool hide_advanced_sound = tab_ != ADVANCED_SOUND_TAB;
	sample_rate_label_.hide(hide_advanced_sound);
	sample_rate_button1_.hide(hide_advanced_sound);
	sample_rate_button2_.hide(hide_advanced_sound);
	sample_rate_button3_.hide(hide_advanced_sound);
	sample_rate_input_.hide(hide_advanced_sound);
	confirm_sound_button_.hide(hide_advanced_sound);
	buffer_size_label_.hide(hide_advanced_sound);
	buffer_size_slider_.hide(hide_advanced_sound);
	normal_sound_button_.hide(hide_advanced_sound);

	const bool hide_multiplayer = tab_ != MULTIPLAYER_TAB;
	chat_lines_label_.hide(hide_multiplayer);
	chat_lines_slider_.hide(hide_multiplayer);
	chat_timestamp_button_.hide(hide_multiplayer);
	lobby_minimaps_button_.hide(hide_multiplayer);
	sort_list_by_group_button_.hide(hide_multiplayer);
	iconize_list_button_.hide(hide_multiplayer);
	show_lobby_joins_button1_.hide(hide_multiplayer);
	show_lobby_joins_button2_.hide(hide_multiplayer);
	show_lobby_joins_button3_.hide(hide_multiplayer);
	friends_list_button_.hide(hide_multiplayer);
	
	const bool hide_friends = tab_ != FRIENDS_TAB;
	friends_.hide(hide_friends);
	friends_back_button_.hide(hide_friends);
	friends_add_friend_button_.hide(hide_friends);
	friends_add_ignore_button_.hide(hide_friends);
	friends_remove_button_.hide(hide_friends);
	friends_input_.hide(hide_friends);

	const bool hide_advanced = tab_ != ADVANCED_TAB;
	advanced_.hide(hide_advanced);
	advanced_button_.hide(hide_advanced);
}

}

void show_preferences_dialog(display& disp, const config& game_cfg)
{
	std::vector<std::string> items;

	std::string const pre = IMAGE_PREFIX + std::string("icons/icon-");
	char const sep = COLUMN_SEPARATOR;
	items.push_back(pre + "general.png" + sep + sgettext("Prefs section^General"));
	items.push_back(pre + "display.png" + sep + sgettext("Prefs section^Display"));
	items.push_back(pre + "music.png" + sep + sgettext("Prefs section^Sound"));
	items.push_back(pre + "multiplayer.png" + sep + sgettext("Prefs section^Multiplayer"));
	items.push_back(pre + "advanced.png" + sep + sgettext("Advanced section^Advanced"));

	for(;;) {
		try {
			preferences_dialog dialog(disp,game_cfg);
			std::vector<gui::preview_pane*> panes;
			panes.push_back(&dialog);

			gui::show_dialog(disp,NULL,_("Preferences"),"",gui::CLOSE_ONLY,&items,&panes);
			return;
		} catch(preferences_dialog::video_mode_change_exception& e) {
			switch(e.type) {
			case preferences_dialog::video_mode_change_exception::CHANGE_RESOLUTION:
				show_video_mode_dialog(disp);
				break;
			case preferences_dialog::video_mode_change_exception::MAKE_FULLSCREEN:
				set_fullscreen(true);
				break;
			case preferences_dialog::video_mode_change_exception::MAKE_WINDOWED:
				set_fullscreen(false);
				break;
			}

			if(items[1].empty() || items[1][0] != '*') {
				items[1] = "*" + items[1];
			}
		}
	}
}

bool show_video_mode_dialog(display& disp)
{
	const events::resize_lock prevent_resizing;
	const events::event_context dialog_events_context;

	CVideo& video = disp.video();

	SDL_PixelFormat format = *video.getSurface()->format;
	format.BitsPerPixel = video.getBpp();

	const SDL_Rect* const * modes = SDL_ListModes(&format,FULL_SCREEN);

	//the SDL documentation says that a return value of -1 if all dimensions
	//are supported/possible.
	if(modes == reinterpret_cast<SDL_Rect**>(-1)) {
		std::cerr << "Can support any video mode\n";
		// SDL says that all modes are possible so it's OK to use a
		// hardcoded list here.
		static const SDL_Rect scr_modes[] = {
			{ 0, 0, 800, 600 },
			{ 0, 0, 1024, 768 },
			{ 0, 0, 1280, 960 },
			{ 0, 0, 1280, 1024 },
		};
		static const SDL_Rect * const scr_modes_list[] = {
			&scr_modes[0],
			&scr_modes[1],
			&scr_modes[2],
			&scr_modes[3],
			NULL
		};

		modes = scr_modes_list;
	} else if(modes == NULL) {
		std::cerr << "No modes supported\n";
		gui::show_dialog(disp,NULL,"",_("There are no alternative video modes available"));
		return false;
	}

	std::vector<std::pair<int,int> > resolutions;

	for(int i = 0; modes[i] != NULL; ++i) {
		if(modes[i]->w >= min_allowed_width && modes[i]->h >= min_allowed_height) {
			resolutions.push_back(std::pair<int,int>(modes[i]->w,modes[i]->h));
		}
	}

	const std::pair<int,int> current_res(video.getSurface()->w,video.getSurface()->h);
	resolutions.push_back(current_res);

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<std::string> options;
	for(std::vector<std::pair<int,int> >::const_iterator j = resolutions.begin(); j != resolutions.end(); ++j) {
		std::ostringstream option;
		if (*j == current_res)
			option << DEFAULT_ITEM;

		option << j->first << "x" << j->second;
		options.push_back(option.str());
	}

	const int result = gui::show_dialog(disp,NULL,"",
	                                    _("Choose Resolution"),
	                                    gui::OK_CANCEL,&options);
	if(size_t(result) < resolutions.size() && resolutions[result] != current_res) {
		set_resolution(resolutions[result]);
		return true;
	} else {
		return false;
	}
}

void show_hotkeys_dialog (display & disp, config *save_config)
{
	log_scope ("show_hotkeys_dialog");

	const events::event_context dialog_events_context;

	const int centerx = disp.w()/2;
	const int centery = disp.h()/2;
#ifdef USE_TINY_GUI
	const int width = 300;			  // FIXME: should compute this, but using what data ?
	const int height = 220;
#else
	const int width = 700;
	const int height = 500;
#endif
	const int xpos = centerx  - width/2;
	const int ypos = centery  - height/2;

	gui::button close_button (disp.video(), _("Close Window"));
	std::vector<gui::button*> buttons;
	buttons.push_back(&close_button);

	surface_restorer restorer;
	gui::dialog_frame f(disp.video(),_("Hotkey Settings"),NULL,&buttons,&restorer);
	f.layout(xpos,ypos,width,height);
	f.draw();

	SDL_Rect clip_rect = { 0, 0, disp.w (), disp.h () };
	SDL_Rect text_size = font::draw_text(NULL, clip_rect, font::SIZE_PLUS,
					     font::NORMAL_COLOUR,_("Press desired Hotkey"),
					     0, 0);

	std::vector<std::string> menu_items;

	std::vector<hotkey::hotkey_item>& hotkeys = hotkey::get_hotkeys();
	for(std::vector<hotkey::hotkey_item>::iterator i = hotkeys.begin(); i != hotkeys.end(); ++i) {
		if(i->hidden())
			continue;
		std::stringstream str,name;
		name << i->get_description();
		str << name.str();
		str << COLUMN_SEPARATOR;
		str << i->get_name();
		menu_items.push_back(str.str());
	}

	std::ostringstream heading;
	heading << HEADING_PREFIX << _("Action") << COLUMN_SEPARATOR << _("Binding");
	menu_items.push_back(heading.str());

	gui::menu::basic_sorter sorter;
	sorter.set_alpha_sort(0).set_alpha_sort(1);

	gui::menu menu_(disp.video(), menu_items, false, height, -1, &sorter, &gui::menu::bluebg_style);
	menu_.sort_by(0);
	menu_.reset_selection();
	menu_.set_width(font::relative_size(400));
	menu_.set_location(xpos + font::relative_size(20), ypos);

	gui::button change_button (disp.video(), _("Change Hotkey"));
	change_button.set_location(xpos + width - change_button.width () - font::relative_size(30),ypos + font::relative_size(80));

	gui::button save_button (disp.video(), _("Save Hotkeys"));
	save_button.set_location(xpos + width - save_button.width () - font::relative_size(30),ypos + font::relative_size(130));

	for(;;) {

		if (close_button.pressed())
			break;

		if (change_button.pressed ()) {
			// Lets change this hotkey......
			SDL_Rect dlgr = {centerx-text_size.w/2 - 30,
								centery-text_size.h/2 - 16,
									text_size.w+60,
									text_size.h+32};
			surface_restorer restorer(&disp.video(),dlgr);
			gui::dialog_frame mini_frame(disp.video());
			mini_frame.layout(centerx-text_size.w/2 - 20,
									centery-text_size.h/2 - 6,
									text_size.w+40,
									text_size.h+12);
			mini_frame.draw_background();
			mini_frame.draw_border();
			font::draw_text (&disp.video(), clip_rect, font::SIZE_LARGE,font::NORMAL_COLOUR,
				 _("Press desired Hotkey"),centerx-text_size.w/2-10,
				 centery-text_size.h/2-3);
			disp.update_display();
			SDL_Event event;
			event.type = 0;
			int character=0,keycode=0; //just to avoid warning
			int mod=0;
			while (event.type!=SDL_KEYDOWN) SDL_PollEvent(&event);
			do {
				if (event.type==SDL_KEYDOWN)
				{
					keycode=event.key.keysym.sym;
					character=event.key.keysym.unicode;
					mod=event.key.keysym.mod;
				};
				SDL_PollEvent(&event);
				disp.flip();
				disp.delay(10);
			} while (event.type!=SDL_KEYUP);
			restorer.restore();
			disp.update_display();

			const hotkey::hotkey_item& oldhk = hotkey::get_hotkey(character, keycode, (mod & KMOD_SHIFT) != 0,
					(mod & KMOD_CTRL) != 0, (mod & KMOD_ALT) != 0, (mod & KMOD_LMETA) != 0);
			hotkey::hotkey_item& newhk = hotkey::get_visible_hotkey(menu_.selection());

			if(oldhk.get_id() != newhk.get_id() && !oldhk.null()) {
				gui::show_dialog(disp,NULL,"",_("This Hotkey is already in use."),gui::MESSAGE);
			} else {
				newhk.set_key(character, keycode, (mod & KMOD_SHIFT) != 0,
						(mod & KMOD_CTRL) != 0, (mod & KMOD_ALT) != 0, (mod & KMOD_LMETA) != 0);

				menu_.change_item(menu_.selection(), 1, newhk.get_name());
			};
		}
		if (save_button.pressed()) {
			if (save_config == NULL) {
				save_hotkeys();
			} else {
				hotkey::save_hotkeys(*save_config);
			}
		}

		menu_.process();

		events::pump();
		events::raise_process_event();
		events::raise_draw_event();

		disp.update_display();

		disp.delay(10);
	}
}

bool show_theme_dialog(display& disp)
{
	int action = 0;
	std::vector<std::string> options = disp.get_theme().get_known_themes();
	if(options.size()){
		std::string current_theme=_("Saved Theme Preference: ")+preferences::theme();
		action = gui::show_dialog(disp,NULL,"",current_theme,gui::OK_CANCEL,&options);
		if(action >= 0){
		preferences::set_theme(options[action]);
		//it would be preferable for the new theme to take effect
		//immediately, however, this will have to do for now.
		gui::show_dialog(disp,NULL,"",_("New theme will take effect on next new or loaded game."),gui::MESSAGE);
		return(1);
		}
	}else{
		gui::show_dialog(disp,NULL,"",_("No known themes.  Try changing from within an existing game."),gui::MESSAGE);
	}
	return(0);
}

}
