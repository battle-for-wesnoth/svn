/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "help.hpp"

#include "events.hpp"
#include "font.hpp"
#include "language.hpp"
#include "preferences.hpp"
#include "show_dialog.hpp"
#include "util.hpp"

#include "SDL_ttf.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <locale>
#include <queue>
#include <stack>
#include <sstream>

namespace {
	const config *help_config = NULL;
	config dummy_cfg;
	std::vector<std::string> empty_string_vector;
	const int max_section_level = 15;
	const int menu_font_size = 14;
	const int title_size = 18;
	const int title2_size = 15;
	const int normal_font_size = 12;
	const std::string topic_img = "help/topic.png";
	const std::string closed_section_img = "help/closed_section.png";
	const std::string open_section_img = "help/open_section.png";
	
	/// Return true if the id is valid for user defined topics and
	/// sections. Some IDs are special, such as toplevel and may not be
	/// be defined in the config.
	bool is_valid_id(const std::string &id) {
		if (id == "toplevel") {
			return false;
		}
		return true;
	}

	void parse_config_internal(const config *help_cfg, const config *section_cfg,
							   help::section &sec, int level=0) {
		if (level > max_section_level) {
			std::cerr << "Maximum section depth has been reached. Maybe circular dependency?"
					  << std::endl;
		}
		else if (section_cfg != NULL) {
			const std::vector<std::string> sections = config::quoted_split((*section_cfg)["sections"]);
			const std::string id = level == 0 ? "toplevel" : (*section_cfg)["id"];
			if (level != 0) {
				if (!is_valid_id(id)) {
					std::stringstream ss;
					ss << "Invalid ID, used for internal purpose: '" << id << "'";
					throw help::parse_error(ss.str());
				}
			}
			const std::string title = level == 0 ? "" : (*section_cfg)["title"];
			sec.id = id;
			sec.title = title;
			std::vector<std::string>::const_iterator it;
			// Find all child sections.
			for (it = sections.begin(); it != sections.end(); it++) {
				config const *child_cfg = help_cfg->find_child("section", "id", *it);
				if (child_cfg != NULL) {
					help::section child_section;
					parse_config_internal(help_cfg, child_cfg, child_section, level + 1);
					sec.add_section(child_section);
				}
				else {
					std::stringstream ss;
					ss << "Help-section '" << *it << "' referenced from '"
					   << id << "' but could not be found.";
					throw help::parse_error(ss.str());
				}
			}
			const std::vector<std::string> topics  = config::quoted_split((*section_cfg)["topics"]);
			// Find all topics in this section.
			for (it = topics.begin(); it != topics.end(); it++) {
				config const *topic_cfg = help_cfg->find_child("topic", "id", *it);
				if (topic_cfg != NULL) {
					help::topic child_topic((*topic_cfg)["title"], (*topic_cfg)["id"],
											(*topic_cfg)["text"]);
					if (!is_valid_id(child_topic.id)) {
						std::stringstream ss;
						ss << "Invalid ID, used for internal purpose: '" << id << "'";
						throw help::parse_error(ss.str());
					}
					sec.topics.push_back(child_topic);
				}
				else {
					std::stringstream ss;
					ss << "Help-topic '" << *it << "' referenced from '" << id
					   << "' but could not be found." << std::endl;
					throw help::parse_error(ss.str());
				}
			}
		}
	}	
}

namespace help {

section parse_config(const config *cfg) {
	section sec;
	if (cfg != NULL) {
		config const *toplevel_cfg = cfg->child("toplevel");
		parse_config_internal(cfg, toplevel_cfg, sec);
	}
	return sec;
}

help_manager::help_manager(const config *hlp_config) {
	help_config = hlp_config == NULL ? &dummy_cfg : hlp_config;
}

help_manager::~help_manager() {
	help_config = NULL;
}

bool topic::operator==(const topic &t) const {
	return t.id == id;
}

bool topic::operator<(const topic &t) const {
	return id < t.id;
}


section::section(const std::string _title, const std::string _id, const topic_list &_topics,
		const std::vector<section> &_sections)
	: title(_title), id(_id), topics(_topics) {
	std::transform(_sections.begin(), _sections.end(), std::back_inserter(sections),
				   create_section());
}

section::~section() {
	std::for_each(sections.begin(), sections.end(), delete_section());
}

section::section(const section &sec) 
	: title(sec.title), id(sec.id), topics(sec.topics) {
	std::transform(sec.sections.begin(), sec.sections.end(),
				   std::back_inserter(sections), create_section());
}

section& section::operator=(const section &sec) {
	std::transform(sec.sections.begin(), sec.sections.end(),
				   std::back_inserter(sections), create_section());
	return *this;
}
	

bool section::operator==(const section &sec) const {
	return sec.id == id;
}

bool section::operator<(const section &sec) const {
	return id < sec.id;
}

void section::add_section(const section &s) {
	sections.push_back(new section(s));
}

void section::clear() {
	topics.clear();
	std::for_each(sections.begin(), sections.end(), delete_section());
	sections.clear();
}

help_menu::help_menu(display& disp, const section &toplevel, int max_height)
	: menu(disp, empty_string_vector, false, max_height), disp_(disp),
	  toplevel_(toplevel), chosen_topic_(NULL), internal_width_(0), selected_item_(&toplevel, "") {
	bg_backup();
	update_visible_items(toplevel_);
	display_visible_items();
	if (!visible_items_.empty()) {
		selected_item_ = visible_items_.front();
	}
}

void help_menu::bg_backup() {
	restorer_ = surface_restorer(&disp_.video(), get_rect());
}

void help_menu::bg_restore() {
	restorer_.restore();
}


bool help_menu::expanded(const section &sec) {
	return expanded_.find(&sec) != expanded_.end();
}

void help_menu::expand(const section &sec) {
	if (sec.id != "toplevel") {
		expanded_.insert(&sec);
	}
}

void help_menu::contract(const section &sec) {
	expanded_.erase(&sec);
}

void help_menu::update_visible_items(const section &sec, unsigned level) {
	if (level == 0) {
		// Clear if this is the top level, otherwise append items.
		visible_items_.clear();
	}
	section_list::const_iterator sec_it;
	for (sec_it = sec.sections.begin(); sec_it != sec.sections.end(); sec_it++) {
		const std::string vis_string = get_string_to_show(*(*sec_it), level);
		visible_items_.push_back(visible_item(*sec_it, vis_string));
		if (expanded(*(*sec_it))) {
			update_visible_items(*(*sec_it), level + 1);
		}
	}
	topic_list::const_iterator topic_it;
	for (topic_it = sec.topics.begin(); topic_it != sec.topics.end(); topic_it++) {
		const std::string vis_string = get_string_to_show(*topic_it, level);
		visible_items_.push_back(visible_item(&(*topic_it), vis_string));
	}
}


std::string help_menu::get_string_to_show(const section &sec, const unsigned level) {
	std::stringstream to_show;
	std::string pad_string;
	// Indentation is represented as three spaces per level.
	pad_string.resize(level * 3, ' ');
	to_show << pad_string << char(menu::IMG_TEXT_SEPERATOR) << "&" ;
	if (expanded(sec)) {
		to_show << open_section_img;
	}
	else {
		to_show << closed_section_img;
	}
	to_show << char(menu::IMG_TEXT_SEPERATOR) << sec.title;
	return to_show.str();
}

std::string help_menu::get_string_to_show(const topic &topic, const unsigned level) {
	std::string pad_string;
	pad_string.resize(level * 3, ' ');
	std::stringstream to_show;
	to_show << pad_string << char(menu::IMG_TEXT_SEPERATOR) << "&" << topic_img
			<< char(menu::IMG_TEXT_SEPERATOR) << topic.title;
	return to_show.str();
}

void help_menu::handle_event(const SDL_Event &event) {
	int mousex, mousey;
	SDL_GetMouseState(&mousex,&mousey);
	// Only handle events if we have focus, that is, the mouse is within
	// the menu.
	if (point_in_rect(mousex, mousey, get_rect())) {
		menu::handle_event(event);
	}
}

void help_menu::set_loc(int x, int y) {
	menu::set_loc(x, y);
	bg_backup();
	display_visible_items();
}

void help_menu::set_width(int w) {
	menu::set_width(w);
	set_max_width(w);
	bg_backup();
	internal_width_ = w;
	display_visible_items();
}

void help_menu::set_max_height(const int new_height) {
	menu::set_max_height(new_height);
	bg_backup();
	display_visible_items();
}

bool help_menu::select_topic_internal(const topic &t, const section &sec) {
	topic_list::const_iterator tit =
		std::find(sec.topics.begin(), sec.topics.end(), t);
	if (tit != sec.topics.end()) {
			expand(sec);
			return true;
	}
	section_list::const_iterator sit;
	for (sit = sec.sections.begin(); sit != sec.sections.end(); sit++) {
		if (select_topic_internal(t, *(*sit))) {
			expand(sec);
			return true;
		}
	}
	return false;
}

void help_menu::select_topic(const topic &t) {
	if (select_topic_internal(t, toplevel_)) {
		update_visible_items(toplevel_);
		for (std::vector<visible_item>::const_iterator it = visible_items_.begin();
			 it != visible_items_.end(); it++) {
			if (*it == t) {
				selected_item_ = *it;
				break;
			}
		}
		display_visible_items();
	}
}
	
int help_menu::process(int x, int y, bool button,bool up_arrow,bool down_arrow,
					   bool page_up, bool page_down, int select_item) {
	int res = menu::process(x, y, button, up_arrow, down_arrow, page_up, page_down, select_item);
	if (!visible_items_.empty() && selection() >= 0
		&& (unsigned)selection() < visible_items_.size()) {
		selected_item_ = visible_items_[selection()];
		if (double_clicked()) {
			if (selected_item_.sec != NULL) {
				// Open or close a section if it is double clicked.
				expanded(*selected_item_.sec) ? contract(*selected_item_.sec)
					: expand(*selected_item_.sec);
				update_visible_items(toplevel_);
				display_visible_items();
			}
			else if (selected_item_.t != NULL) {
				/// Choose a topic if it is double clicked.
				chosen_topic_ = selected_item_.t;
			}
		}
	}
	return res;
}

const topic *help_menu::chosen_topic() {
	const topic *ret = chosen_topic_;
	chosen_topic_ = NULL;
	return ret;
}
	
void help_menu::display_visible_items() {
	bg_restore();
	std::vector<std::string> menu_items;
	std::vector<visible_item>::const_iterator items_it;
	for (items_it = visible_items_.begin(); items_it != visible_items_.end(); items_it++) {
		std::string to_show = (*items_it).visible_string;
		if (selected_item_ == *items_it) {
			to_show = std::string("*") + to_show;
		}
		menu_items.push_back(to_show);
	}
	set_items(menu_items, false);
	menu::set_width(internal_width_);
}

help_menu::visible_item::visible_item(const section *_sec, const std::string &vis_string) :
	t(NULL), sec(_sec), visible_string(vis_string) {}

help_menu::visible_item::visible_item(const topic *_t, const std::string &vis_string) :
	t(_t), sec(NULL), visible_string(vis_string) {}

bool help_menu::visible_item::operator==(const section &_sec) const {
	return sec != NULL && *sec == _sec;
}

bool help_menu::visible_item::operator==(const topic &_t) const {
	return t != NULL && *t == _t;
}

bool help_menu::visible_item::operator==(const visible_item &vis_item) const {
	return t == vis_item.t && sec == vis_item.sec;
}
	
help_text_area::help_text_area(display &disp, const section &toplevel)
	: gui::widget(disp), disp_(disp), toplevel_(toplevel), shown_topic_(NULL),
	  title_spacing_(16), curr_loc_(0, 0),
	  min_row_height_(font::get_max_height(normal_font_size)), curr_row_height_(min_row_height_),
	  scrollbar_(disp, this), use_scrollbar_(false),
	  uparrow_(disp,"",gui::button::TYPE_PRESS,"uparrow-button"),
	  downarrow_(disp,"",gui::button::TYPE_PRESS,"downarrow-button"),
	  contents_height_(0)
{}

void help_text_area::show_topic(const topic &t) {
	bg_restore();
	shown_topic_ = &t;
	scrollbar_.set_grip_position(0);
	// A new topic will be shown, we do not know yet if a new scrollbar
	// is needed.
	use_scrollbar_ = false;
	std::vector<std::string> parsed_text;
	parsed_text = parse_text(t.text);
	set_items(parsed_text, t.title);
	set_dirty(true);
}

	
help_text_area::item::item(shared_sdl_surface surface, int x, int y, const std::string _text,
						   const std::string reference_to, bool _floating,
						   ALIGNMENT alignment)
	: surf(surface), text(_text), ref_to(reference_to), floating(_floating), align(alignment) {
	rect.x = x;
	rect.y = y;
	rect.w = surface->w;
	rect.h = surface->h;
}

help_text_area::item::item(shared_sdl_surface surface, int x, int y, bool _floating,
						   ALIGNMENT alignment)
	: surf(surface), text(""), ref_to(""), floating(_floating), align(alignment) {
	rect.x = x;
	rect.y = y;
	rect.w = surface->w;
	rect.h = surface->h;
}

void help_text_area::set_items(const std::vector<std::string> &parsed_items,
							   const std::string &title) {
	last_row_.clear();
	items_.clear();
	curr_loc_.first = 0;
	curr_loc_.second = 0;
	curr_row_height_ = min_row_height_;
	// Add the title item.
	const std::string show_title =
		font::make_text_ellipsis(shown_topic_->title, title_size, text_width());
	shared_sdl_surface surf(font::get_rendered_text(show_title, title_size,
													font::NORMAL_COLOUR, TTF_STYLE_BOLD));
	if (surf != NULL) {
		add_item(item(surf, 0, 0, show_title));
		curr_loc_.second = title_spacing_;
		contents_height_ = title_spacing_;
		down_one_line();
	}
	bool retry = false;
	// Parse and add the text.
	std::vector<std::string>::const_iterator it;
	for (it = parsed_items.begin(); it != parsed_items.end(); it++) {
		if (*it != "" && (*it)[0] == '[') {
			if (contents_height_ > (int)height() && !use_scrollbar_) {
				// The items did not fit, we need a
				// scrollbar. Everything has to be redone since the
				// width is less now and items added before may not fit.
				retry = true;
				use_scrollbar_ = true;
				break;
			}
			// Should be parsed as WML.
			try {
				config cfg(*it);
				config *child = cfg.child("ref");
				if (child != NULL) {
					handle_ref_cfg(*child);
				}
				child = cfg.child("img");
				if (child != NULL) {
					handle_img_cfg(*child);
				}
				child = cfg.child("bold");
				if (child != NULL) {
					handle_bold_cfg(*child);
				}
				child = cfg.child("italic");
				if (child != NULL) {
					handle_italic_cfg(*child);
				}
				child = cfg.child("header");
				if (child != NULL) {
					handle_header_cfg(*child);
				}
				child = cfg.child("jump");
				if (child != NULL) {
					handle_jump_cfg(*child);
				}
			}
			catch (config::error e) {
				std::stringstream msg;
				msg << "Error when parsing help markup as WML: '" << e.message << "'";
				throw parse_error(msg.str());
			}
		}
		else {
			add_text_item(*it);
		}
	}
	down_one_line(); // End the last line.
	if (contents_height_ > (int)height() && !use_scrollbar_) {
		// Last line caused us to have to use a scrollbar.
		use_scrollbar_ = true;
		retry = true;
	}
	if (retry) {
		set_items(parsed_items, title);
	}
	update_scrollbar();
}

void help_text_area::handle_ref_cfg(const config &cfg) {
	const std::string dst = cfg["dst"];
	const std::string text = cfg["text"];
	if (dst == "" || text == "") {
		throw parse_error("Ref markup must have both dst and text attributes.");
	}
	add_text_item(text, dst);
}

void help_text_area::handle_img_cfg(const config &cfg) {
	const std::string src = cfg["src"];
	const std::string align = cfg["align"];
	const bool floating = get_bool(cfg["float"]);
	if (src == "") {
		throw parse_error("Img markup must have src attribute.");
	}
	add_img_item(src, align, floating);
}

void help_text_area::handle_bold_cfg(const config &cfg) {
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Bold markup must have text attribute.");
	}
	add_text_item(text, "", -1, true);
}

void help_text_area::handle_italic_cfg(const config &cfg) {
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Italic markup must have text attribute.");
	}
	add_text_item(text, "", -1, false, true);
}

void help_text_area::handle_header_cfg(const config &cfg) {
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Header markup must have text attribute.");
	}
	add_text_item(text, "", title2_size, true);
}

void help_text_area::handle_jump_cfg(const config &cfg) {
	const std::string amount_str = cfg["amount"];
	const std::string to_str = cfg["to"];
	if (amount_str == "" && to_str == "") {
		throw parse_error("Jump markup must have either a to or an amount attribute.");
	}
	unsigned jump_to = curr_loc_.first;
	if (amount_str != "") {
		unsigned amount;
		try {
			amount = lexical_cast<unsigned, std::string>(amount_str);
		}
		catch (bad_lexical_cast) {
			throw parse_error("Invalid amount the amount attribute in jump markup.");
		}
		jump_to += amount;
	}
	if (to_str != "") {
		unsigned to;
		try {
			to = lexical_cast<unsigned, std::string>(to_str);
		}
		catch (bad_lexical_cast) {
			throw parse_error("Invalid amount in the to attribute in jump markup.");
		}
		if (to < (unsigned)jump_to) {
			down_one_line();
		}
		jump_to = to;
	}
	if (jump_to > 0 && (int)jump_to < get_max_x(curr_loc_.first, curr_row_height_)) {
		curr_loc_.first = jump_to;
	}
}

void help_text_area::update_scrollbar() {
	if (!use_scrollbar_) {
		scrollbar_.enable(false);
		downarrow_.hide(true);
		uparrow_.hide(true);
		return;
	}
	uparrow_.set_location(location().x + width() - uparrow_.width(), location().y);
	downarrow_.set_location(location().x + width() - downarrow_.width(),
							location().y + location().h - downarrow_.height());
	const int scrollbar_height = height() - uparrow_.height() - downarrow_.height();
	scrollbar_.enable(true);
	scrollbar_.set_location(location().x + width() - scrollbar_.get_width(),
							location().y + uparrow_.height());
	scrollbar_.set_height(scrollbar_height);
	scrollbar_.set_width(scrollbar_.get_max_width()); // Needed to update the screen.
	float pos_percent = (float)scrollbar_height / (float)contents_height_;
	int grip_height = (int)(pos_percent * scrollbar_height);
	const int min_height = scrollbar_.get_minimum_grip_height();
	if (grip_height < min_height) {
		grip_height = min_height;
	}
	if (grip_height > (int)height()) {
		std::cerr << "Strange. For some reason I want my scrollbar"
				  << " to be larger than me!\n\n";
		grip_height = height();
	}
	scrollbar_.set_grip_height(grip_height);
	scrollbar_.set_grip_position(0);
}

void help_text_area::set_dirty(bool dirty) {
	widget::set_dirty(dirty);
	if (dirty) {
		scrollbar_.set_dirty(true);
		uparrow_.set_dirty(true);
		downarrow_.set_dirty(true);
	}
}

int help_text_area::text_width() const {
	if (use_scrollbar_) {
		return width() - scrollbar_.get_max_width();
	}
	return width();
}

void help_text_area::add_text_item(const std::string text, const std::string ref_dst,
								   int _font_size, bool bold, bool italic) {
	const int font_size = _font_size < 0 ? normal_font_size : _font_size;
	if (text == "") {
		return;
	}
	const int remaining_width = get_remaining_width();
	size_t first_word_start = text.find_first_not_of(" ");
	if (first_word_start == std::string::npos) {
		first_word_start = 0;
	}
	if (text[first_word_start] == '\n') {
		down_one_line();
		std::string rest_text = text;
		rest_text.erase(0, first_word_start + 1);
		add_text_item(rest_text, ref_dst, _font_size, bold, italic);
		return;
	}
	const std::string first_word = get_first_word(text);
	if (curr_loc_.first != get_min_x(curr_loc_.second, curr_row_height_)
		&& remaining_width < font::line_width(first_word, font_size)) {
		// The first word does not fit, and we are not at the start of
		// the line. Move down.
		down_one_line();
		add_text_item(text, ref_dst, _font_size, bold, italic);
	}
	else {
		std::vector<std::string> parts = split_in_width(text, font_size, remaining_width);
		std::string first_part = parts.front();
		int state = ref_dst == "" ? 0 : TTF_STYLE_UNDERLINE;
		state |= bold ? TTF_STYLE_BOLD : 0;
		state |= italic ? TTF_STYLE_ITALIC : 0;
		shared_sdl_surface surf(font::get_rendered_text(first_part, font_size,
														font::NORMAL_COLOUR, state));
		add_item(item(surf, curr_loc_.first, curr_loc_.second, first_part, ref_dst));
		if (parts.size() > 1) {
			// Parts remain, remove the first part from the string and
			// add the remaining parts.
			std::string s = text;
			s.erase(0, first_part.size());
			const std::string first_word_before = get_first_word(s) + " ";
			s = remove_first_space(s);
			const std::string first_word_after = get_first_word(s);

			if (get_remaining_width() >= font::line_width(first_word_after, font_size)
				&& get_remaining_width() < font::line_width(first_word_before, font_size)) {
				// If the removal of the space made this word fit, we
				// must move down a line, otherwise it will be drawn
				// without a space at the end of the line.
				down_one_line();
			}
			add_text_item(s, ref_dst, _font_size, bold, italic);
		}
	}
}

void help_text_area::add_img_item(const std::string path, const std::string alignment,
								  const bool floating) {
	shared_sdl_surface surf(image::get_image(path, image::UNSCALED));
	if (surf == NULL) {
		std::stringstream msg;
		msg << "Image " << path << " could not be loaded.";
		std::cerr << msg.str();
		return;
	}
	ALIGNMENT align = str_to_align(alignment);
	if (align == HERE && floating) {
		std::cerr << "Floating image with align HERE, aligning left." << std::endl;
		align = LEFT;
	}
	int xpos;
	int ypos = curr_loc_.second;
	switch (align) {
	case HERE:
		xpos = curr_loc_.first;
		break;
	case LEFT:
		xpos = 0;
		break;
	case MIDDLE:
		xpos = text_width() / 2 - surf->w / 2;
		break;
	case RIGHT:
		xpos = text_width() - surf->w;
		break;
	}
	if (curr_loc_.first != get_min_x(curr_loc_.second, curr_row_height_)
		&& (xpos < curr_loc_.first || xpos + surf->w > text_width())) {
		down_one_line();
		add_img_item(path, alignment, floating);
	}
	else {
		if (!floating) {
			curr_loc_.first = xpos;
		}
		else {
			ypos = get_y_for_floating_img(surf->w, xpos, ypos);
		}
		add_item(item(surf, xpos, ypos, floating, align));
	}
}

int help_text_area::get_y_for_floating_img(const int width, const int x, const int desired_y) {
	int min_y = desired_y;
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); it++) {
		const item& itm = *it;
		if (itm.floating) {
			if ((itm.rect.x + itm.surf->w > x && itm.rect.x < x + width)
				|| (itm.rect.x > x && itm.rect.x < x + width)) {
				min_y = maximum<int>(min_y, itm.rect.y + itm.surf->h);
			}
		}
	}
	return min_y;
}

int help_text_area::get_min_x(const int y, const int height) {
	int min_x = 0;
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); it++) {
		const item& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.surf->h > y && itm.align == LEFT) {
				min_x = maximum<int>(min_x, itm.surf->w);
			}
		}
	}
	return min_x;
}

int help_text_area::get_max_x(const int y, const int height) {
	int max_x = text_width();
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); it++) {
		const item& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.surf->h > y) {
				if (itm.align == RIGHT) {
					max_x = minimum<int>(max_x, text_width() - itm.surf->w);
				}
				if (itm.align == MIDDLE) {
					max_x = minimum<int>(max_x, text_width() / 2 - itm.surf->w / 2);
				}
			}
		}
	}
	return max_x;
}

void help_text_area::add_item(const item &itm) {
	items_.push_back(itm);
	if (!itm.floating) {
		curr_loc_.first += itm.surf->w;
		curr_row_height_ = maximum<int>(itm.surf->h, curr_row_height_);
		contents_height_ = maximum<int>(contents_height_, curr_loc_.second + curr_row_height_);
		last_row_.push_back(&items_.back());
	}
	else {
		if (itm.align == LEFT) {
			curr_loc_.first = itm.surf->w;
		}
		contents_height_ = maximum<int>(contents_height_, itm.rect.y + itm.surf->h);
	}
}
	

help_text_area::ALIGNMENT help_text_area::str_to_align(const std::string &s) {
	const std::string cmp_str = to_lower(s);
	if (cmp_str == "left") {
		return LEFT;
	} else if (cmp_str == "middle") {
		return MIDDLE;
	} else if (cmp_str == "right") {
		return RIGHT;
	} else if (cmp_str == "here" || cmp_str == "") { // Make the empty string be "here" alignment.
		return HERE;
	}
	std::stringstream msg;
	msg << "Invalid alignment string: '" << s << "'";
	throw parse_error(msg.str());
}
	
void help_text_area::down_one_line() {
	adjust_last_row();
	last_row_.clear();
	curr_loc_.second += curr_row_height_ + (curr_row_height_ == min_row_height_ ? 0 : 2);
	curr_row_height_ = min_row_height_;
	contents_height_ = maximum<int>(curr_loc_.second + curr_row_height_, contents_height_);
	curr_loc_.first = get_min_x(curr_loc_.second, curr_row_height_);
}

void help_text_area::adjust_last_row() {
	for (std::list<item *>::iterator it = last_row_.begin(); it != last_row_.end(); it++) {
		item &itm = *(*it);
		const int gap = curr_row_height_ - itm.surf->h;
		itm.rect.y += gap / 2;
	}
}

int help_text_area::get_remaining_width() {
	const int total_w = (int)get_max_x(curr_loc_.second, curr_row_height_);
	return total_w - curr_loc_.first;
}

unsigned help_text_area::get_scroll_offset() const {
	const int scrollbar_pos = scrollbar_.get_grip_position();
	float pos_percent = scrollbar_.height() / (float)contents_height_;
	unsigned to_sub = 0;
	if (pos_percent > 0) {
		to_sub = (unsigned)(scrollbar_pos / pos_percent);
	}
	return to_sub;
}

void help_text_area::draw() {
	if (dirty()) {
		bg_restore();
		SDL_Rect clip_rect = { location().x, location().y, text_width(), height() };
		const int scrollbar_pos = scrollbar_.get_grip_position();
		uparrow_.hide(scrollbar_pos == 0 || !scrollbar_.enabled());
		downarrow_.hide(scrollbar_pos + scrollbar_.get_grip_height() == (int)scrollbar_.height()
						|| !scrollbar_.enabled());
		SDL_Surface* const screen = disp_.video().getSurface();
		clip_rect_setter clip_rect_set(screen, clip_rect);
		std::list<item>::const_iterator it;
		for (it = items_.begin(); it != items_.end(); it++) {
			SDL_Rect dst = (*it).rect;
			dst.y -= get_scroll_offset();
			if (dst.y < (int)height() && dst.y + (*it).surf->h > 0) {
				dst.x += location().x;
				dst.y += location().y;
				sdl_safe_blit((*it).surf, NULL, screen, &dst);
			}
		}
		update_rect(location());
		scrollbar_.set_dirty(true);
		set_dirty(false);
	}
}

void help_text_area::process() {
	if (uparrow_.pressed()) {
		scroll_up();
	}
	if (downarrow_.pressed()) {
		scroll_down();
	}
}

void help_text_area::scroll_up(int how_much) {
	if (use_scrollbar_) {
		// Only allow scrolling when we have a scrollbar.
		const int amount = how_much < 0 ? height() / 50 : how_much;
		scrollbar_.set_grip_position(scrollbar_.get_grip_position() - amount);
		set_dirty(true);
	}
}

void help_text_area::scroll_down(int how_much) {
	if (use_scrollbar_) {
		// Only allow scrolling when we have a scrollbar.
		const int amount = how_much < 0 ? height() / 50 : how_much;
		scrollbar_.set_grip_position(scrollbar_.get_grip_position() + amount);
		set_dirty(true);
	}
}

void help_text_area::handle_event(const SDL_Event &event) {
	if(event.type == SDL_MOUSEMOTION) {
		// If the mouse has moved, check if this widget still has focus or not.
		int mousex, mousey;
		SDL_GetMouseState(&mousex,&mousey);
		if (point_in_rect(mousex, mousey, location())) {
			if (!focus()) {
				set_focus(true);
				// wiget::set_focus will set everything dirty, so we
				// need to redraw the contents.
				set_dirty(true);
			}
		}
		else {
			if (focus()) {
				set_focus(false);
				// wiget::set_focus will set everything dirty, so we
				// need to redraw the contents.
				set_dirty(true);
			}
		}
		return;
	}
	if (focus()) {
		switch (event.type) {
		case SDL_MOUSEBUTTONDOWN:
			// Scroll up/down when the mouse wheel is used.
			SDL_MouseButtonEvent mouse_event = event.button;
			if (mouse_event.button == SDL_BUTTON_WHEELUP) {
				scroll_up();
			}
			if (mouse_event.button == SDL_BUTTON_WHEELDOWN) {
				scroll_down();
			}
			break;
		default:
			;
		}
	}
}

void help_text_area::scroll(int pos) {
	// Nothing will be done on the actual scroll event. The scroll
	// position is checked when drawing instead and things drawn
	// accordingly.
	set_dirty(true);
}

bool help_text_area::item_at::operator()(const item& item) const {
	return point_in_rect(x_, y_, item.rect);
}

std::string help_text_area::ref_at(const int x, const int y) {
	const int local_x = x - location().x;
	const int local_y = y - location().y;
	if (local_y < (int)height() && local_y > 0) {
		const int cmp_y = local_y + get_scroll_offset();
		const std::list<item>::const_iterator it =
			std::find_if(items_.begin(), items_.end(), item_at(local_x, cmp_y));
		if (it != items_.end()) {
			if ((*it).ref_to != "") {
				return ((*it).ref_to);
			}
		}
	}
	return "";
}



help_browser::help_browser(display &disp, section &toplevel)
	: gui::widget(disp), disp_(disp), menu_(disp, toplevel),
	  text_area_(disp, toplevel), toplevel_(toplevel) {
	// Set sizes to some default values.
	set_location(1, 1);
	set_width(400);
	set_height(500);
}

void help_browser::adjust_layout() {
	const int menu_y = location().y;
	const int menu_x = location().x;
	const int menu_w = 250;
	const int menu_h = height();
	
	const int menu_text_area_padding = 10;
	const int text_area_y = location().y;
	const int text_area_x = menu_x + menu_w + menu_text_area_padding;
	const int text_area_w = width() - menu_w - menu_text_area_padding;
	const int text_area_h = height();

	menu_.set_width(menu_w);
	menu_.set_loc(menu_x, menu_y);
	menu_.set_max_height(menu_h);
	menu_.set_max_width(menu_w);

	text_area_.set_location(text_area_x, text_area_y);
	text_area_.set_width(text_area_w);
	text_area_.set_height(text_area_h);

	set_dirty(true);
}

void help_browser::set_dirty(bool dirty) {
	widget::set_dirty(dirty);
	if (dirty) {
		menu_.set_dirty();
		text_area_.set_dirty(true);
	}
}

void help_browser::set_location(const SDL_Rect& rect) {
	widget::set_location(rect);
	adjust_layout();
}

void help_browser::set_location(int x, int y) {
	widget::set_location(x, y);
	adjust_layout();
}

void help_browser::set_width(int w) {
	widget::set_width(w);
	adjust_layout();
}

void help_browser::set_height(int h) {
	widget::set_height(h);
	adjust_layout();
}

void help_browser::process() {
	CKey key;
	int mousex, mousey;
	const int mouse_flags = SDL_GetMouseState(&mousex,&mousey);

	const bool new_left_button = mouse_flags&SDL_BUTTON_LMASK;
	
	const bool new_up_arrow = key[SDLK_UP];
	const bool new_down_arrow = key[SDLK_DOWN];
	
	const bool new_page_up = key[SDLK_PAGEUP];
	const bool new_page_down = key[SDLK_PAGEDOWN];
	/// Fake focus functionality for the menu, only process it if it has focus.
	if (point_in_rect(mousex, mousey, menu_.get_rect())) {
		menu_.process(mousex, mousey, new_left_button, new_up_arrow,
					  new_down_arrow, new_page_up, new_page_down, -1);
		const topic *chosen_topic = menu_.chosen_topic();
		if (chosen_topic != NULL) {
			/// A topic has been chosen in the menu, display it.
			text_area_.show_topic(*chosen_topic);
		}
	}
}

void help_browser::handle_event(const SDL_Event &event) {
	switch (event.type) {
	case SDL_MOUSEBUTTONDOWN:
		SDL_MouseButtonEvent mouse_event = event.button;
		if (mouse_event.button == SDL_BUTTON_LEFT) {
			const int x = mouse_event.x;
			const int y = mouse_event.y;
			// Did the user click a cross-reference?
			const std::string ref = text_area_.ref_at(x, y);
			if (ref != "") {
				const topic *t = find_topic(toplevel_, ref);
				if (t == NULL) {
					std::stringstream msg;
					msg << "Reference to unknown topic: '" << ref << "'.";
					gui::show_dialog(disp_, NULL, "", msg.str(), gui::OK_ONLY);
				}
				else {
					menu_.select_topic(*t);
					text_area_.show_topic(*t);
				}
			}
		}
	}	
}


const topic *find_topic(const section &sec, const std::string &id) {
	topic_list::const_iterator tit =
		std::find_if(sec.topics.begin(), sec.topics.end(), has_id(id));
	if (tit != sec.topics.end()) {
		return &(*tit);
	}
	section_list::const_iterator sit;
	for (sit = sec.sections.begin(); sit != sec.sections.end(); sit++) {
		const topic *t = find_topic(*(*sit), id);
		if (t != NULL) {
			return t;
		}
	}
	return NULL;
}

void help_browser::show_topic(const std::string &topic_id) {
	const topic *t = find_topic(toplevel_, topic_id);
	if (t != NULL) {
		text_area_.show_topic(*t);
		menu_.select_topic(*t);
	}
	else {
		std::cerr << "Help browser tried to show topic with id '" << topic_id
				  << "' but that topic could not be found." << std::endl;
	}
}

std::vector<std::string> parse_text(const std::string &text) {
	std::vector<std::string> res;
	bool last_char_escape = false;
	const char escape_char = '\\';
	std::stringstream ss;
	size_t pos;
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (pos = 0; pos < text.size(); pos++) {
		const char c = text[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		}
		else {
			if (state == OTHER) {
				if (c == '<') {
					if (last_char_escape) {
						ss << c;
					}
					else {
						res.push_back(ss.str());
						ss.str("");
						state = ELEMENT_NAME;
					}
				}
				else {
					ss << c;
				}
			}
			else if (state == ELEMENT_NAME) {
				if (c == '/') {
					std::string msg = "Errornous / in element name.";
					throw parse_error(msg);
				}
				else if (c == '>') {
					// End of this name.
					std::stringstream s;
					const std::string element_name = ss.str();
					ss.str("");
					s << "</" << element_name << ">";
					const std::string end_element_name = s.str();
					size_t end_pos = text.find(end_element_name, pos);
					if (end_pos == std::string::npos) {
						std::stringstream msg;
						msg << "Unterminated element: " << element_name;
						throw parse_error(msg.str());
					}
					s.str("");
					const std::string contents = text.substr(pos + 1, end_pos - pos - 1);
					const std::string element = convert_to_wml(element_name, contents);
					res.push_back(element);
					pos = end_pos + end_element_name.size() - 1;
					state = OTHER;
				}
				else {
					ss << c;
				}
			}
			last_char_escape = false;
		}
	}
	if (state == ELEMENT_NAME) {
		std::stringstream msg;
		msg << "Element '" << ss.str() << "' continues through end of string.";
		throw parse_error(msg.str());
	}
	if (ss.str() != "") {
		// Add the last string.
		res.push_back(ss.str());
	}
	return res;
}

std::string convert_to_wml(const std::string &element_name, const std::string &contents) {
	std::stringstream ss;
	bool in_quotes = false;
	bool last_char_escape = false;
	const char escape_char = '\\';
	std::vector<std::string> attributes;
	// Find the different attributes. No checks are made for the equal
	// sign or something like that. Attributes are just separated by
	// spaces or newlines. Attributes that contain spaces must be in
	// single quotes.
	for (size_t pos = 0; pos < contents.size(); pos++) {
		const char c = contents[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		}
		else {
			if (c == '\'' && !last_char_escape) {
				in_quotes = !in_quotes;
			}
			else if ((c == ' ' || c == '\n') && !last_char_escape && !in_quotes) {
				// Space or newline, end of attribute.
				attributes.push_back(ss.str());
				ss.str("");
			}
			else {
				ss << c;
			}
			last_char_escape = false;
		}
	}
	if (in_quotes) {
		std::stringstream msg;
		msg << "Unterminated single quote after: '" << ss.str() << "'";
		throw parse_error(msg.str());
	}
	if (ss.str() != "") {
		attributes.push_back(ss.str());
	}
	ss.str("");
	// Create the WML.
	ss << "[" << element_name << "]\n";
	for (std::vector<std::string>::const_iterator it = attributes.begin();
		 it != attributes.end(); it++) {
		ss << *it << "\n";
	}
	ss << "[/" << element_name << "]\n";
	return ss.str();
}

bool get_bool(const std::string &s) {
	if (s == "yes" || s == "true" || s == "1" || s == "on") {
		return true;
	}
	return false;
}

std::vector<std::string> split_in_width(const std::string &s, const int font_size,
										const unsigned width) {
	std::string wrapped = font::word_wrap_text(s, font_size, width);
	std::vector<std::string> parts = config::split(wrapped, '\n', 0);
	return parts;
}

std::string remove_first_space(const std::string& text) {
  if (text.length() > 0 && text[0] == ' ') {
    return text.substr(1);
  }
  return text;
}

std::string to_lower(const std::string &s) {
	std::string lower_string;
	lower_string.resize(s.size());
	std::transform(s.begin(), s.end(), lower_string.begin(), tolower);
	return lower_string;
}
		
std::string get_first_word(const std::string &s) {
	size_t first_word_start = s.find_first_not_of(" ");
	if (first_word_start == std::string::npos) {
		first_word_start = 0;
	}
	size_t first_word_end = s.find_first_of(" \n", first_word_start);
	if (first_word_end == std::string::npos) {
		first_word_end = s.size();
	}
	const std::string first_word = s.substr(0, first_word_end);
	return first_word;
}

void show_help(display &disp, std::string show_topic, int xloc, int yloc) {
	const events::event_context dialog_events_context;
	const gui::dialog_manager manager;
	const events::resize_lock prevent_resizing;

	CVideo& screen = disp.video();
	SDL_Surface* const scr = screen.getSurface();

	const int width = minimum<int>(900, scr->w - 20);
	const int height = minimum<int>(800, scr->h - 150);
	const int left_padding = 10;
	const int right_padding = 10;
	const int top_padding = 10;
	const int bot_padding = 10;

	// If not both locations were supplied, put the dialog in the middle
	// of the screen.
	if (yloc <= -1 || xloc <= -1) {
		xloc = scr->w / 2 - width / 2;
		yloc = scr->h / 2 - height / 2; 
	}
	std::vector<gui::button*> buttons_ptr;
	gui::button close_button_(disp, string_table["close_button"]);
	buttons_ptr.push_back(&close_button_);
	surface_restorer restorer;
	gui::draw_dialog(xloc, yloc, width, height, disp, translate_string("help_title"),
					 NULL, &buttons_ptr, &restorer);

	try {
		section toplevel = parse_config(help_config);
		help_browser hb(disp, toplevel);
		hb.set_location(xloc + left_padding, yloc + top_padding);
		hb.set_width(width - left_padding - right_padding);
		hb.set_height(height - top_padding - bot_padding);
		if (show_topic != "") {
			hb.show_topic(show_topic);
		}
		hb.set_dirty(true);
		events::raise_draw_event();
		screen.flip();
		disp.invalidate_all();
		CKey key;
		for (;;) {
			events::pump();
			events::raise_process_event();
			events::raise_draw_event();
			if (key[SDLK_ESCAPE]) {
				// Escape quits from the dialog.
				return;
			}
			for (std::vector<gui::button*>::iterator button_it = buttons_ptr.begin();
				 button_it != buttons_ptr.end(); button_it++) {
				if ((*button_it)->pressed()) {
					// There is only one button, close.
					return;
				}
			}
			screen.flip();
			SDL_Delay(10);
		}
	}
	catch (parse_error e) {
		std::stringstream msg;
		msg << "Parse error when parsing help text: '" << e.message << "'";
		gui::show_dialog(disp, NULL, "", msg.str(), gui::OK_ONLY);
	}
}

} // End namespace help.
