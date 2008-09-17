/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/dialogs/editor_settings.hpp"

#include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/window_builder.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gettext.hpp"
#include "image.hpp"
#include "log.hpp"
#include "wml_exception.hpp"
#define DBG_GUI LOG_STREAM_INDENT(debug, gui)
#define LOG_GUI LOG_STREAM_INDENT(info, gui)
#define WRN_GUI LOG_STREAM_INDENT(warn, gui)
#define ERR_GUI LOG_STREAM_INDENT(err, gui)
#define ERR_ED LOG_STREAM_INDENT(err, editor)

namespace gui2 {

teditor_settings::teditor_settings()
: redraw_callback_(),
tods_(), current_tod_(0),
current_tod_label_(NULL), current_tod_image_(NULL),
custom_tod_toggle_(NULL), 
custom_tod_toggle_field_(register_bool("custom_tod_toggle", false)),
custom_tod_red_(NULL), custom_tod_green_(NULL), custom_tod_blue_(NULL),
custom_tod_red_field_(register_integer("custom_tod_red", false)),
custom_tod_green_field_(register_integer("custom_tod_green", false)),
custom_tod_blue_field_(register_integer("custom_tod_blue", false))
{
}

void teditor_settings::do_next_tod(twindow& window)
{
	current_tod_++;
	current_tod_ %= tods_.size();
	custom_tod_toggle_->set_value(false);
	update_selected_tod_info(window);
}

const time_of_day& teditor_settings::get_selected_tod() const
{
	assert(static_cast<size_t>(current_tod_) < tods_.size());
	return tods_[current_tod_];
}

int teditor_settings::get_red() const
{
	std::cerr << "get red " << custom_tod_red_field_->get_cache_value() << "\n";
	return custom_tod_red_field_->get_cache_value();
}
int teditor_settings::get_green() const
{
	std::cerr << "get green " << custom_tod_green_field_->get_cache_value() << "\n";
	return custom_tod_green_field_->get_cache_value();
}
int teditor_settings::get_blue() const
{
	std::cerr << "get blue" << custom_tod_blue_field_->get_cache_value() << "\n";
	return custom_tod_blue_field_->get_cache_value();
}

void teditor_settings::update_tod_display(twindow& window)
{
	redraw_callback_(custom_tod_red_->get_value(),
		custom_tod_green_->get_value(),
		custom_tod_blue_->get_value());
}

void teditor_settings::set_current_adjustment(int r, int g, int b)
{
	for (size_t i = 0; i < tods_.size(); ++i) {
		time_of_day& tod = tods_[i];
		if (tod.red == r && tod.green == g && tod.blue == b) {
			current_tod_ = i;
			custom_tod_toggle_field_->set_cache_value(false);
			return;
		}
	}
	/* custom tod */
	std::cerr << "adj set b\n";
	custom_tod_red_field_->set_cache_value(r);
	custom_tod_green_field_->set_cache_value(g);
	custom_tod_blue_field_->set_cache_value(b);
	custom_tod_toggle_field_->set_cache_value(true);
	std::cerr << "adj set " << r << " " << g << " " << b << "\n";
}

void teditor_settings::update_selected_tod_info(twindow& window)
{
	std::cerr << "update_selected_tod_info begin\n";
	bool custom = custom_tod_toggle_->get_value();
	if (custom) {
		current_tod_label_->set_label(_("Custom setting"));
	} else {
		std::stringstream ss; 
		ss << (current_tod_ + 1);
		ss << "/" << tods_.size();
		ss << ": " << get_selected_tod().name;
		current_tod_label_->set_label(ss.str());
		//current_tod_image_->set_icon_name(get_selected_tod().image);
		custom_tod_red_->set_value(get_selected_tod().red);
		custom_tod_green_->set_value(get_selected_tod().green);
		custom_tod_blue_->set_value(get_selected_tod().blue);
		custom_tod_red_field_->set_cache_value(get_selected_tod().red);
		custom_tod_green_field_->set_cache_value(get_selected_tod().green);
		custom_tod_blue_field_->set_cache_value(get_selected_tod().blue);
	}
	custom_tod_red_->set_active(custom);
	custom_tod_green_->set_active(custom);
	custom_tod_blue_->set_active(custom);	
	current_tod_label_->set_active(!custom);
	update_tod_display(window);
	window.invalidate_layout();
	std::cerr << "update_selected_tod_info end\n";
}

twindow teditor_settings::build_window(CVideo& video)
{
	return build(video, get_id(EDITOR_SETTINGS));
}

void teditor_settings::pre_show(CVideo& /*video*/, twindow& window)
{
	assert(!tods_.empty());
	current_tod_label_ = &window.get_widget<tlabel>("current_tod", false);
	current_tod_image_ = &window.get_widget<tlabel>("current_tod_image", false);
	custom_tod_toggle_ = &window.get_widget<ttoggle_button>("custom_tod_toggle", false);
	custom_tod_red_ = &window.get_widget<tslider>("custom_tod_red", false);
	custom_tod_green_ = &window.get_widget<tslider>("custom_tod_green", false);
	custom_tod_blue_ = &window.get_widget<tslider>("custom_tod_blue", false);
	tbutton& next_tod_button = window.get_widget<tbutton>("next_tod", false);
	next_tod_button.set_callback_mouse_left_click(
		dialog_callback<teditor_settings, &teditor_settings::do_next_tod>);
	custom_tod_toggle_->set_callback_state_change(
		dialog_callback<teditor_settings, &teditor_settings::update_selected_tod_info>);
	custom_tod_red_->set_callback_positioner_move(
		dialog_callback<teditor_settings, &teditor_settings::update_tod_display>);
	custom_tod_green_->set_callback_positioner_move(
		dialog_callback<teditor_settings, &teditor_settings::update_tod_display>);
	custom_tod_blue_->set_callback_positioner_move(
		dialog_callback<teditor_settings, &teditor_settings::update_tod_display>);
	update_selected_tod_info(window);
}

void teditor_settings::post_show(twindow& /*window*/)
{
}

} // namespace gui2
