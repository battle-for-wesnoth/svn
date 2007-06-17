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

#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "display.hpp"
#include "events.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "key.hpp"
#include "marked-up_text.hpp"
#include "show_dialog.hpp"
#include "wml_separators.hpp"
#include "widgets/file_menu.hpp"

#include <sstream>
#include <cstdio>

namespace {
	std::vector<std::string> empty_string_vector;
}

namespace gui {

static const std::string dir_picture("misc/folder-icon.png");
static const std::string path_up("..");
const char file_menu::path_delim('/');

file_menu::file_menu(CVideo &disp, std::string start_file)
	: menu(disp, empty_string_vector, false),
	  current_dir_(get_path(start_file)),
	  chosen_file_(start_file), last_selection_(-1)
{
	// If the start file is not a file or directory, use the root.
	if(!file_exists(chosen_file_) && !::is_directory(chosen_file_)
		|| !::is_directory(current_dir_)) {
		current_dir_ = path_delim;
		chosen_file_ = current_dir_;
	}
	// FIXME: quick hack
	// on a high-res screen the initial max_items_onscreen is based
	// on .66 of y dimension, eg. 17 menu items, exceeding the
	// starting box which can only take 13 or so: force it to be smaller
//	set_measurements(400, 384);
	update_file_lists();
}

void file_menu::display_current_files() {
	std::vector<std::string> to_show;
	if (!is_root(current_dir_)) {
		to_show.push_back(path_up);
	}
	std::vector<std::string>::iterator it;
	for (it = dirs_in_current_dir_.begin(); it != dirs_in_current_dir_.end(); it++) {
		// Add an image to show that these are directories.
		std::stringstream ss;
		ss << font::IMAGE << dir_picture << COLUMN_SEPARATOR << *it;
		to_show.push_back(ss.str());
	}
	for (it = files_in_current_dir_.begin(); it != files_in_current_dir_.end(); it++) {
		const std::string display_string = COLUMN_SEPARATOR + *it;
		to_show.push_back(display_string);
	}
	const int menu_font_size = font::SIZE_NORMAL; // Known from menu.cpp.
	for (it = to_show.begin(); it != to_show.end(); it++) {
		// Make sure that all lines fit.
		// Guess the width of the scrollbar to be 30 since it is not accessible from here.
		// -25 to compensate for the picture column.
		while ((unsigned int)font::line_width(*it, menu_font_size) > width() - 30 - 25) {
			(*it).resize((*it).size() - 1);
		}
	}
	set_items(to_show);
}

int file_menu::delete_chosen_file() {
	const int ret = remove(chosen_file_.c_str());
	if (ret == -1) {
	//	gui::message_dialog(disp_, "", _("Deletion of the file failed.")).show();
	}
	else {
		last_selection_ = -1;
		update_file_lists();
		chosen_file_ = current_dir_;
	}
	return ret;
}

bool file_menu::make_directory(const std::string& subdir_name) {
	bool ret = ::make_directory(add_path(current_dir_, subdir_name));
	if (ret == false) {
	//	gui::message_dialog(disp_, "", _("Creation of the directory failed.")).show();
	}
	else {
		last_selection_ = -1;
		update_file_lists();
		chosen_file_ = current_dir_;
	}
	return ret;
}

void file_menu::handle_event(const SDL_Event& event) {
	menu::handle_event(event);
	if(selection() != last_selection_) {
		entry_selected(selection());
		last_selection_ = selection();
	}
}

void file_menu::entry_selected(const unsigned entry) {
	const int entry_index = entry - (is_root(current_dir_) ? 0 : 1);
	if (entry_index >= 0) {
		std::string selected;
		if ((unsigned)entry_index < dirs_in_current_dir_.size()) {
			const int dir_index = entry_index;
			selected = dirs_in_current_dir_[dir_index];
		}
		else {
			const int file_index = entry_index - dirs_in_current_dir_.size();
			if(file_index >= 0 && size_t(file_index) < files_in_current_dir_.size()) {
				selected = files_in_current_dir_[file_index];
			} else {
				return;
			}
		}
		chosen_file_ = add_path(current_dir_, selected);
	} else {
		chosen_file_ = path_up;
	}
}

bool file_menu::is_directory(const std::string& fname) const {
	if(fname == path_up)
		return true;
	return ::is_directory(fname);
}

void file_menu::change_directory(const std::string path) {
	if(path == path_up)
	{
		// Parent dir wanted.
		if (!is_root(current_dir_)) {
			current_dir_ = get_path_up(current_dir_);
			last_selection_ = -1;
			update_file_lists();
			chosen_file_ = current_dir_;
		}
		else {
			return;
		}

	} else {
		current_dir_ = path;
		chosen_file_ = current_dir_;
		last_selection_ = -1;
		update_file_lists();
	}
}

std::string file_menu::get_choice() const {
	return chosen_file_;
}


std::string file_menu::get_path(const std::string file_or_dir) const {
	std::string res_path = file_or_dir;
	if (!::is_directory(file_or_dir)) {
		size_t index = file_or_dir.find_last_of(path_delim);
		if (index != std::string::npos) {
			res_path = file_or_dir.substr(0, index);
		}
	}
	return res_path;
}

std::string file_menu::get_path_up(const std::string path, const unsigned levels) const {
	std::string curr_path = get_path(path);
	for (unsigned i = 0; i < levels; i++) {
		if (is_root(curr_path)) {
			break;
		}
		curr_path = strip_last_delim(curr_path);
		size_t index = curr_path.find_last_of(path_delim);
		if (index != std::string::npos) {
			curr_path = curr_path.substr(0, index);
		}
		else {
#ifdef __AMIGAOS4__
			index = curr_path.find_last_of(':');
			if (index != std::string::npos) index++;
#endif
			break;
		}
	}
	if (curr_path.size() == 0) {
		// The root was reached, represent this as one delimiter only.
		curr_path = path_delim;
	}
	return curr_path;
}

std::string file_menu::strip_last_delim(const std::string path) const {
	std::string res_string = path;
	if (path[path.size() - 1] == path_delim) {
		res_string = path.substr(0, path.size() - 1);
	}
	return res_string;
}

bool file_menu::is_root(const std::string path) const {
#ifdef __AMIGAOS4__
	return path.size() == 0 || path[path.size()-1] == ':';
#else
	return path.size() == 0 || path.size() == 1 && path[0] == path_delim;
#endif
}

std::string file_menu::add_path(const std::string path, const std::string to_add) const
{
	std::string joined_path = strip_last_delim(path);
	if (to_add.size() > 0) {
		if (to_add == path_up) {
			return get_path_up(path);
		}
#ifdef __AMIGAOS4__
		else if (joined_path.empty() || joined_path[joined_path.size()-1] == ':') {
			if (to_add[0] == path_delim)
				joined_path += to_add.substr(1);
			else
				joined_path += to_add;
		}
#endif
		else if (to_add[0] == path_delim) {
			joined_path += to_add;
		}
		else {
			joined_path += "/" + to_add;
		}
	}
	return joined_path;
}

void file_menu::update_file_lists() {
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();
	get_files_in_dir(current_dir_, &files_in_current_dir_,
	                 &dirs_in_current_dir_, FILE_NAME_ONLY);
	display_current_files();
}

}
