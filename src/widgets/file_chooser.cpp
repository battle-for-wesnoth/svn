/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "../font.hpp"
#include "../events.hpp"
#include "../display.hpp"
#include "file_chooser.hpp"

namespace gui {

file_chooser::file_chooser(display &disp, std::string start_file) 
	: widget(disp), disp_(disp), path_delim_('/'), current_dir_(get_path(start_file)),
	  chosen_file_(start_file), file_list_(disp, files_in_current_dir_, false),
	  filename_textbox_(disp, 100, start_file, true), choice_made_(false),
	  last_selection_(-1) {
	// If the start file is not a file or directory, use the root.
	if (!file_exists(chosen_file_) || !is_directory(current_dir_)) {
		current_dir_ = path_delim_;
		chosen_file_ = current_dir_;
	}
	// Set sizes to some default values.
	set_location(1, 1);
	set_width(400);
	set_height(500);
	update_file_lists();
}

void file_chooser::adjust_layout() {
	const int current_path_y = location().y;
	current_path_rect_.y = current_path_y;
	current_path_rect_.w = width();
	current_path_rect_.x = location().x;
	current_path_rect_.h = 18;
	const int file_list_y = current_path_y + current_path_rect_.h + 10;
	const int filename_textbox_y = location().y + height() - filename_textbox_.height();
		
	const int file_list_height = filename_textbox_y  - file_list_y - 10;

	file_list_.set_width(width());
	filename_textbox_.set_width(width());

	file_list_.set_loc(location().x, file_list_y);
	filename_textbox_.set_location(location().x, filename_textbox_y);

	file_list_.set_max_height(file_list_height);
	// When the layout has changed we want to redisplay the files to
	// make them fit into the newly adjusted widget.
	set_dirty(true);
}

void file_chooser::display_current_files() {
	bg_restore();
	std::vector<std::string> to_show;
	if (!is_root(current_dir_)) {
		to_show.push_back("..");
	}
	std::copy(dirs_in_current_dir_.begin(), dirs_in_current_dir_.end(),
			  std::back_inserter(to_show));
	std::vector<std::string>::iterator it;
	for (it = to_show.begin(); it != to_show.end(); it++) {
		// Add a delimiter to show that these are directories.
		(*it) += path_delim_;
	}
	std::copy(files_in_current_dir_.begin(), files_in_current_dir_.end(),
			  std::back_inserter(to_show));
	const int menu_font_size = 14; // Known from menu.cpp.
	for (it = to_show.begin(); it != to_show.end(); it++) {
		// Make sure that all lines fit.
		// Guess the width of the scrollbar to be 30 since it is not accessible from here.
		while (font::line_width(*it, menu_font_size) > file_list_.width() - 30) {
			(*it).resize((*it).size() - 1);
		}
	}
	file_list_.set_items(to_show);

	// This will prevent the "box" with filenames from changing size on
	// every redisplay, it looks better when it's static.
	file_list_.set_width(width());  
}

void file_chooser::display_chosen_file() {
	// Clearing is not really necessary, but things end up nicer of we do.
	filename_textbox_.clear(); 
	if (is_directory(chosen_file_)) {
		filename_textbox_.set_text(strip_last_delim(chosen_file_) + path_delim_);
	}
	else {
		filename_textbox_.set_text(chosen_file_);
	}
}

void file_chooser::draw() {
	if (!dirty()) {
		return;
	}
	display_current_files();
	display_chosen_file();
	font::draw_text(&disp_, current_path_rect_, 14, font::NORMAL_COLOUR,
					current_dir_, current_path_rect_.x, current_path_rect_.y,
					disp_.video().getSurface());
	set_dirty(false);
}

void file_chooser::process() {
	CKey key;
	int mousex, mousey;
	const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);
	// The menu does not implement focus functionality, so we fake
	// it. We give the file list focus whenever the filename textbox
	// does not have focus. Inflexible but easy solution.
	if (!(mousex > location().x && (unsigned)mousex < location().x + width()
		&& mousey > location().y 
		&& (unsigned)mousey < location().y + height() - filename_textbox_.height())) {
		// Hmm, as I understand it this should happen automatically when
		// the mouse is in the textbox again. However this is not the
		// case so this is done explicitly here.
		filename_textbox_.set_focus(true);
	}
	else {
		filename_textbox_.set_focus(false);
	}
	if (!filename_textbox_.focus()) {
		const bool new_left_button = mouse_flags&SDL_BUTTON_LMASK;
		
		const bool new_up_arrow = key[SDLK_UP];
		const bool new_down_arrow = key[SDLK_DOWN];
		
		const bool new_page_up = key[SDLK_PAGEUP];
		const bool new_page_down = key[SDLK_PAGEDOWN];
		file_list_.process(mousex, mousey, new_left_button, new_up_arrow,
						   new_down_arrow, new_page_up, new_page_down, -1);
		const int new_selection = file_list_.selection();
		const bool double_click = file_list_.double_clicked();
		if (double_click && new_selection >= 0) {
			last_selection_ = new_selection;
			entry_chosen(new_selection);
		}
		if (new_selection >= 0 && last_selection_ != new_selection) {
			last_selection_ = new_selection;
			entry_selected(new_selection);
		}
	}
}

void file_chooser::entry_selected(const unsigned entry) {
	const int entry_index = entry - (is_root(current_dir_) ? 0 : 1);
	if (entry_index >= 0) {
		// Do not change the selection if the parent directory entry is selected.
		std::string selected;
		if ((unsigned)entry_index < dirs_in_current_dir_.size()) {
			const int dir_index = entry_index;
			selected = dirs_in_current_dir_[dir_index];
		}
		else {
			const int file_index = entry_index - dirs_in_current_dir_.size();
			selected = files_in_current_dir_[file_index];
		}
		chosen_file_ = add_path(current_dir_, selected);
		display_chosen_file();
	}
}

/// Enter the directory or choose the file.
void file_chooser::entry_chosen(const unsigned entry) {
	const int entry_index = entry - (is_root(current_dir_) ? 0 : 1);
	if (entry_index == -1) {
		// Parent dir wanted.
		if (!is_root(current_dir_)) {
			current_dir_ = get_path_up(current_dir_);
			update_file_lists();
			chosen_file_ = current_dir_;
		}
		else {
			return;
		}
	}
	else {
		if ((unsigned)entry_index < dirs_in_current_dir_.size()) {
			// Descend into a directory.
			const int dir_index = entry_index;
			const std::string selected_dir = dirs_in_current_dir_[dir_index];
			current_dir_ = add_path(current_dir_, selected_dir);
			chosen_file_ = current_dir_;
			update_file_lists();
		}
		else {
			// Choose a file.
			const int file_index = entry_index - dirs_in_current_dir_.size();
			const std::string selected_file = files_in_current_dir_[file_index];
			chosen_file_ = add_path(current_dir_, selected_file);
			choice_made_ = true;
		}
	}
	set_dirty(true);
}

bool file_chooser::choice_made() const {
	return choice_made_;
}

std::string file_chooser::get_choice() const {
	if (filename_textbox_.focus()) {
		return filename_textbox_.text();
	}
	return chosen_file_;
}

void file_chooser::set_dirty(bool dirty) {
	widget::set_dirty(dirty);
	filename_textbox_.set_dirty(dirty);
}

void file_chooser::set_location(const SDL_Rect& rect) {
	widget::set_location(rect);
	adjust_layout();
}

void file_chooser::set_location(int x, int y) {
	widget::set_location(x, y);
	adjust_layout();
}

void file_chooser::set_width(int w) {
	widget::set_width(w);
	adjust_layout();
}

void file_chooser::set_height(int h) {
	widget::set_height(h);
	adjust_layout();
}

std::string file_chooser::get_path(const std::string file_or_dir) const {
	std::string res_path = file_or_dir;
	if (!is_directory(file_or_dir)) {
		size_t index = file_or_dir.find_last_of(path_delim_);
		if (index != std::string::npos) {
			res_path = file_or_dir.substr(0, index);
		}
	}
	return res_path;
}

std::string file_chooser::get_path_up(const std::string path, const unsigned levels) const {
	std::string curr_path = get_path(path);
	for (unsigned i = 0; i < levels; i++) {
		if (is_root(curr_path)) {
			break;
		}
		curr_path = strip_last_delim(curr_path);
		size_t index = curr_path.find_last_of(path_delim_);
		if (index != std::string::npos) {
			curr_path = curr_path.substr(0, index);
		}
		else {
			break;
		}
	}
	if (curr_path.size() == 0) {
		// The root was reached, represent this as one delimiter only.
		curr_path = path_delim_;
	}
	return curr_path;
}

std::string file_chooser::strip_last_delim(const std::string path) const {
	std::string res_string = path;
	if (path[path.size() - 1] == path_delim_) {
		res_string = path.substr(0, path.size() - 1);
	}
	return res_string;
}

bool file_chooser::is_root(const std::string path) const {
	return path.size() == 0 || path.size() == 1 && path[0] == path_delim_;
}

std::string file_chooser::add_path(const std::string path, const std::string to_add) {
	std::string joined_path = strip_last_delim(path);
	if (to_add.size() > 0) {
		if (to_add[0] == path_delim_) {
			joined_path += to_add;
		}
		else {
			joined_path += "/" + to_add;
		}
	}
	return joined_path;
}

void file_chooser::update_file_lists() {
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();
	get_files_in_dir(current_dir_, &files_in_current_dir_,
					 &dirs_in_current_dir_, FILE_NAME_ONLY); 
}

void file_chooser::handle_event(const SDL_Event& event) {
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_RETURN) {
			if (filename_textbox_.focus()) {
				chosen_file_ = filename_textbox_.text();
				choice_made_ = true;
			}
			else {
				const int selected = file_list_.selection();
				if (selected >= 0) {
					entry_chosen(selected);
				}
			}
		}
	}
}

}
