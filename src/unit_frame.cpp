/* $Id$ */
/*
   Copyright (C) 2006 - 2008 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file unit_frame.cpp */

#include "global.hpp"
#include "sound.hpp"
#include "halo.hpp"
#include <cassert>
// NOTE: global.hpp must be first!

#include "display.hpp"
#define UNIT_FRAME_H_PART2
#include "unit_frame.hpp"


progressive_string::progressive_string(const std::string & data,int duration)
{
		const std::vector<std::string> first_pass = utils::split(data);
		input_ = data;
		const int time_chunk = maximum<int>(duration / (first_pass.size()?first_pass.size():1),1);

		std::vector<std::string>::const_iterator tmp;
		for(tmp=first_pass.begin();tmp != first_pass.end() ; tmp++) {
			std::vector<std::string> second_pass = utils::split(*tmp,':');
			if(second_pass.size() > 1) {
				data_.push_back(std::pair<std::string,int>(second_pass[0],atoi(second_pass[1].c_str())));
			} else {
				data_.push_back(std::pair<std::string,int>(second_pass[0],time_chunk));
			}
		}
}
int progressive_string::duration() const
{
	int total =0;
	std::vector<std::pair<std::string,int> >::const_iterator cur_halo;
	for(cur_halo = data_.begin() ; cur_halo != data_.end() ; cur_halo++) {
		total += cur_halo->second;
	}
	return total;

}
static const std::string empty_string ="";
const std::string& progressive_string::get_current_element(int current_time)const
{
	int time = 0;
	unsigned int sub_halo = 0;
	if(data_.empty()) return empty_string;
	while(time < current_time&& sub_halo < data_.size()) {
		time += data_[sub_halo].second;
		sub_halo++;

	}
	if(sub_halo) sub_halo--;
	if(sub_halo >= data_.size()) sub_halo = data_.size();
	return data_[sub_halo].first;
}

template <class T>
progressive_<T>::progressive_(const std::string &data, int duration)
{
	input_ = data;
	const std::vector<std::string> first_split = utils::split(data);
	const int time_chunk = maximum<int>(duration / (first_split.size()?first_split.size():1),1);

	std::vector<std::string>::const_iterator tmp;
	std::vector<std::pair<std::string,int> > first_pass;
	for(tmp=first_split.begin();tmp != first_split.end() ; tmp++) {
		std::vector<std::string> second_pass = utils::split(*tmp,':');
		if(second_pass.size() > 1) {
			first_pass.push_back(std::pair<std::string,int>(second_pass[0],atoi(second_pass[1].c_str())));
		} else {
			first_pass.push_back(std::pair<std::string,int>(second_pass[0],time_chunk));
		}
	}
	std::vector<std::pair<std::string,int> >::const_iterator tmp2;
	for(tmp2=first_pass.begin();tmp2 != first_pass.end() ; tmp2++) {
		std::vector<std::string> range = utils::split(tmp2->first,'~');
		data_.push_back(std::pair<std::pair<T, T>,int> (
			std::pair<T, T>(
				lexical_cast<T>(range[0].c_str()),
				lexical_cast<T>(range.size() > 1 ? range[1].c_str() : range[0].c_str())),
				tmp2->second));
	}

}

template <class T>
const T progressive_<T>::get_current_element(int current_time, T default_val) const
{
	int time = 0;
	unsigned int sub_halo = 0;
	int searched_time = current_time;
	if(searched_time < 0) searched_time = 0;
	if(searched_time > duration()) searched_time = duration();
	if(data_.empty()) return default_val;
	while(time < searched_time&& sub_halo < data_.size()) {
		time += data_[sub_halo].second;
		sub_halo++;

	}
	if(sub_halo != 0) {
		sub_halo--;
		time -= data_[sub_halo].second;
	}
	if(sub_halo >= data_.size()) {
		sub_halo = data_.size();
		time = searched_time; // Never more than max allowed
	}

	const T first =  data_[sub_halo].first.first;
	const T second =  data_[sub_halo].first.second;

	return T((static_cast<double>(searched_time - time) /
		static_cast<double>(data_[sub_halo].second)) *
		(second - first) + first);
}

template<class T>
int progressive_<T>::duration() const
{
	int total = 0;
	typename std::vector<std::pair<std::pair<T, T>, int> >::const_iterator cur_halo;
	for(cur_halo = data_.begin() ; cur_halo != data_.end() ; cur_halo++) {
		total += cur_halo->second;
	}
	return total;

}

template <class T>
bool progressive_<T>::does_not_change() const
{
return data_.empty() ||
	( data_.size() == 1 && data_[0].first.first == data_[0].first.second);
}

// Force compilation of the following template instantiations
template class progressive_<int>;
template class progressive_<double>;

#undef UNIT_FRAME_H_PART2
#include "unit_frame.hpp"



frame_builder::frame_builder(const config& cfg,const std::string& frame_string)
{
	image(image::locator(cfg[frame_string+"image"]),cfg[frame_string+"image_mod"]);
	image_diagonal(image::locator(cfg[frame_string+"image_diagonal"]),cfg[frame_string+"image_mod"]);
	sound(cfg[frame_string+"sound"]);
	std::vector<std::string> tmp_string_vect=utils::split(cfg[frame_string+"text_color"]);
	if(tmp_string_vect.size() ==3) {
		text(cfg[frame_string+"text"],
		 display::rgb(atoi(tmp_string_vect[0].c_str()),atoi(tmp_string_vect[1].c_str()),atoi(tmp_string_vect[2].c_str())));
	} else {
		text(cfg[frame_string+"text"],0);
	}

	if(!cfg[frame_string+"duration"].empty()) {
		duration(atoi(cfg[frame_string+"duration"].c_str()));
	} else {
		duration(atoi(cfg[frame_string+"end"].c_str()) - atoi(cfg[frame_string+"begin"].c_str()));
	}
	halo(cfg[frame_string+"halo"],cfg[frame_string+"halo_x"],cfg[frame_string+"halo_y"]);
	 tmp_string_vect=utils::split(cfg[frame_string+"blend_color"]);
	if(tmp_string_vect.size() ==3) {
		blend(cfg[frame_string+"blend_ratio"],display::rgb(atoi(tmp_string_vect[0].c_str()),atoi(tmp_string_vect[1].c_str()),atoi(tmp_string_vect[2].c_str())));
	} else {
		blend(cfg[frame_string+"blend_ratio"],0);
	}
	highlight(cfg[frame_string+"alpha"]);
	offset(cfg[frame_string+"offset"]);
	submerge(cfg[frame_string+"submerge"]);
	x(cfg[frame_string+"x"]);
	y(cfg[frame_string+"y"]);

}

const frame_parameters frame_builder::parameters(int current_time) const
{
	frame_parameters result;
	result.duration = duration_;
	result.image = image_;
	result.image_diagonal = image_diagonal_;
	result.image_mod = image_mod_;
	result.halo = halo_.get_current_element(current_time);
	result.halo_x = halo_x_.get_current_element(current_time);
	result.halo_y = halo_y_.get_current_element(current_time);
	result.sound = sound_;
	result.text = text_;
	result.text_color = text_color_;
	result.blend_with = blend_with_;
	result.blend_ratio = blend_ratio_.get_current_element(current_time);
	result.highlight_ratio = highlight_ratio_.get_current_element(current_time,1.0);
	result.offset = offset_.get_current_element(current_time);
	result.submerge = submerge_.get_current_element(current_time);
	result.x = x_.get_current_element(current_time);
	result.y = y_.get_current_element(current_time);
	return result;
}
frame_builder & frame_builder::image(const image::locator image ,const std::string & image_mod)
{
	image_ = image;
	image_mod_ = image_mod;
	return *this;
}
frame_builder & frame_builder::image_diagonal(const image::locator image_diagonal,const std::string& image_mod)
{
	image_diagonal_ = image_diagonal;
	image_mod_ = image_mod;
	return *this;
}
frame_builder & frame_builder::sound(const std::string& sound)
{
	sound_=sound;
	return *this;
}
frame_builder & frame_builder::text(const std::string& text,const  Uint32 text_color)
{
	text_=text;
	text_color_=text_color;
	return *this;
}
frame_builder & frame_builder::halo(const std::string &halo, const std::string &halo_x, const std::string& halo_y)
{
	halo_ = progressive_string(halo,duration_);
	halo_x_ = progressive_int(halo_x,duration_);
	halo_y_ = progressive_int(halo_y,duration_);
	return *this;
}
frame_builder & frame_builder::duration(const int duration)
{
	duration_= duration;
	recalculate_duration();
	return *this;
}
void frame_builder::recalculate_duration()
{
	halo_ = progressive_string(halo_.get_original(),duration_);
	halo_x_ = progressive_int(halo_x_.get_original(),duration_);
	halo_y_ = progressive_int(halo_y_.get_original(),duration_);
	blend_ratio_=progressive_double(blend_ratio_.get_original(),duration_);
	highlight_ratio_=progressive_double(highlight_ratio_.get_original(),duration_);
	offset_=progressive_double(offset_.get_original(),duration_);
	submerge_=progressive_double(submerge_.get_original(),duration_);
	x_=progressive_int(x_.get_original(),duration_);
	y_=progressive_int(y_.get_original(),duration_);
}
frame_builder & frame_builder::blend(const std::string& blend_ratio,const Uint32 blend_color)
{
	blend_with_=blend_color;
	blend_ratio_=progressive_double(blend_ratio,duration_);
	return *this;
}
frame_builder & frame_builder::highlight(const std::string& highlight)
{
	highlight_ratio_=progressive_double(highlight,duration_);
	return *this;
}
frame_builder & frame_builder::offset(const std::string& offset)
{
	offset_=progressive_double(offset,duration_);
	return *this;
}
frame_builder & frame_builder::submerge(const std::string& submerge)
{
	submerge_=progressive_double(submerge,duration_);
	return *this;
}
frame_builder & frame_builder::x(const std::string& x)
{
	x_=progressive_int(x,duration_);
	return *this;
}
frame_builder & frame_builder::y(const std::string& y)
{
	y_=progressive_int(y,duration_);
	return *this;
}
bool frame_builder::does_not_change() const
{
	return halo_.does_not_change() &&
		halo_x_.does_not_change() &&
		halo_y_.does_not_change() &&
		blend_ratio_.does_not_change() &&
		highlight_ratio_.does_not_change() &&
		offset_.does_not_change() &&
		submerge_.does_not_change() &&
		x_.does_not_change() &&
		y_.does_not_change();
}
bool frame_builder::need_update() const
{
	if(!halo_.does_not_change() ||
			!halo_x_.does_not_change() ||
			!halo_y_.does_not_change() ||
			!blend_ratio_.does_not_change() ||
			!highlight_ratio_.does_not_change() ||
			!offset_.does_not_change() ||
			!submerge_.does_not_change() ||
			!x_.does_not_change() ||
			!y_.does_not_change() ) {
			return true;
	}
	return false;
}

void unit_frame::redraw(const int frame_time,bool first_time,const gamemap::location & src,const gamemap::location & dst,int*halo_id,const frame_parameters & animation_val,const frame_parameters & engine_val)const
{
	const int xsrc = game_display::get_singleton()->get_location_x(src);
	const int ysrc = game_display::get_singleton()->get_location_y(src);
	const int xdst = game_display::get_singleton()->get_location_x(dst);
	const int ydst = game_display::get_singleton()->get_location_y(dst);
	const gamemap::location::DIRECTION direction = src.get_relative_dir(dst);

	const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	double tmp_offset = current_data.offset;
	int d2 = game_display::get_singleton()->hex_size() / 2;

	if(first_time ) {
		// stuff sthat should be done only once per frame
		if(!current_data.sound.empty()  ) {
			sound::play_sound(current_data.sound);
		}
		if(!current_data.text.empty()  ) {
			game_display::get_singleton()->float_label(src,current_data.text,
			(current_data.text_color & 0x00FF0000) >> 16,
			(current_data.text_color & 0x0000FF00) >> 8,
			(current_data.text_color & 0x000000FF) >> 0);
		}
	}
	image::locator image_loc;
	if(direction != gamemap::location::NORTH && direction != gamemap::location::SOUTH) {
		image_loc = image::locator(current_data.image_diagonal,current_data.image_mod);
	} 
	if(image_loc.is_void() || image_loc.get_filename() == "") { // invalid diag image, or not diagonal
		image_loc = image::locator(current_data.image,current_data.image_mod);
	}

	surface image;
	if(!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
		image=image::get_image(image_loc,
				image::SCALED_TO_ZOOM,
				false
				);
	}
	const int x = static_cast<int>(tmp_offset * xdst + (1.0-tmp_offset) * xsrc) + d2;
	const int y = static_cast<int>(tmp_offset * ydst + (1.0-tmp_offset) * ysrc) + d2;
	if (image != NULL) {
		bool facing_west = direction == gamemap::location::NORTH_WEST || direction == gamemap::location::SOUTH_WEST;
		bool facing_north = direction == gamemap::location::NORTH_WEST || direction == gamemap::location::NORTH || direction == gamemap::location::NORTH_EAST;
		game_display::get_singleton()->render_unit_image(x + current_data.x- image->w/2,
			       	y  + current_data.y- image->h/2, false,
			       	gamemap::get_drawing_order(src), image, facing_west, false,
				ftofxp(current_data.highlight_ratio), current_data.blend_with,
			       	current_data.blend_ratio,current_data.submerge,!facing_north);
	}
	halo::remove(*halo_id);
	*halo_id = halo::NO_HALO;
	if(!current_data.halo.empty()) {
		halo::ORIENTATION orientation;
		switch(direction)
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
		if(direction != gamemap::location::SOUTH_WEST && direction != gamemap::location::NORTH_WEST) {
			*halo_id = halo::add(static_cast<int>(x+current_data.halo_x* game_display::get_singleton()->get_zoom_factor()),
					static_cast<int>(y+current_data.halo_y* game_display::get_singleton()->get_zoom_factor()),
					current_data.halo,
					gamemap::location(-1, -1),
					orientation);
		} else {
			*halo_id = halo::add(static_cast<int>(x-current_data.halo_x* game_display::get_singleton()->get_zoom_factor()),
					static_cast<int>(y+current_data.halo_y* game_display::get_singleton()->get_zoom_factor()),
					current_data.halo,
					gamemap::location(-1, -1),
					orientation);
		}
	}
}
void unit_frame::invalidate(const int frame_time,const gamemap::location & src,const gamemap::location & dst,const frame_parameters & animation_val,const frame_parameters & engine_val,const bool primary) const
{
	const int xsrc = game_display::get_singleton()->get_location_x(src);
	const int ysrc = game_display::get_singleton()->get_location_y(src);
	const int xdst = game_display::get_singleton()->get_location_x(dst);
	const int ydst = game_display::get_singleton()->get_location_y(dst);
	const gamemap::location::DIRECTION direction = src.get_relative_dir(dst);

	const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val,primary);
	double tmp_offset = current_data.offset;
	//unused var - int d2 = game_display::get_singleton()->hex_size() / 2;

	image::locator image_loc;
	if(direction != gamemap::location::NORTH && direction != gamemap::location::SOUTH) {
		image_loc = current_data.image_diagonal;
	} 
	if(image_loc.is_void() || image_loc.get_filename() == "") { // invalid diag image, or not diagonal
		image_loc = current_data.image;
	}

	surface image;
	if(!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
		/**
		 * @todo cache handling: here we will use the image again soon we
		 * should cache it here and release it (if needed) after redrawn
		 */
		image=image::get_image(image_loc,
				image::SCALED_TO_ZOOM,
				false
				);
	}
	const int x = static_cast<int>(tmp_offset * xdst + (1.0-tmp_offset) * xsrc)+current_data.x;
	const int y = static_cast<int>(tmp_offset * ydst + (1.0-tmp_offset) * ysrc)+current_data.y;
	if (image != NULL) {
		// optimization for most common case
		if(x==xsrc && y == ysrc && 
				image->w  == game_display::get_singleton()->hex_size() &&
				image->h  == game_display::get_singleton()->hex_size()) {

			game_display::get_singleton()->invalidate(src);
		} else {
			game_display::get_singleton()->invalidate_zone(x,y,x+image->w,y+image->h);
		}

	}
}



const frame_parameters unit_frame::merge_parameters(int current_time,const frame_parameters & animation_val,const frame_parameters & engine_val, bool primary) const
{
	/** 
	 * this function merges the value provided by
	 *  * the frame
	 *  * the engine (poison, flying unit...)
	 *  * the animation as a whole
	 *  there is no absolute rule for merging, so creativity is the rule
	 *  if a value is never provided by the engine, assert. (this way if it becomes used, people will easily find the right place to look)
	 *  
	 */
	frame_parameters result;
	const frame_parameters & current_val = builder_.parameters(current_time);
	assert(engine_val.image.is_void() || engine_val.image.get_filename() == "");
	result.image = current_val.image.is_void() || current_val.image.get_filename() == ""?animation_val.image:current_val.image;

	assert(engine_val.image_diagonal.is_void() || engine_val.image_diagonal.get_filename() == "");
	result.image_diagonal = current_val.image_diagonal.is_void()|| current_val.image_diagonal.get_filename() == ""?animation_val.image_diagonal:current_val.image_diagonal;

	result.image_mod = current_val.image_mod +animation_val.image_mod;
	if(primary)result.image_mod += engine_val.image_mod;

	assert(engine_val.halo.empty());
	result.halo = current_val.halo.empty()?animation_val.halo:current_val.halo;

	assert(engine_val.halo_x == 0);
	result.halo_x =  current_val.halo_x?current_val.halo_x:animation_val.halo_x;

	result.halo_y = current_val.halo_y?current_val.halo_y:animation_val.halo_y;
	result.halo_y += engine_val.halo_y;

	assert(engine_val.duration == 0);
	result.duration = current_val.duration;

	assert(engine_val.sound.empty());
	result.sound = current_val.sound.empty()?animation_val.sound:current_val.sound;

	assert(engine_val.text.empty());
	result.text = current_val.text.empty()?animation_val.text:current_val.text;

	assert(engine_val.text_color == 0);
	result.text_color = current_val.text_color?current_val.text_color:animation_val.text_color;

	result.blend_with = current_val.blend_with?current_val.blend_with:animation_val.blend_with;
	if(primary&& engine_val.blend_with) result.blend_with = engine_val.blend_with;

	result.blend_ratio = current_val.blend_ratio?current_val.blend_ratio:animation_val.blend_ratio;
	if(primary && engine_val.blend_ratio) result.blend_ratio += engine_val.blend_ratio;

	result.highlight_ratio = current_val.highlight_ratio!=1.0?current_val.highlight_ratio:animation_val.highlight_ratio;
	if(primary && engine_val.highlight_ratio != 1.0) result.highlight_ratio = result.highlight_ratio +engine_val.highlight_ratio - 1.0; // selected unit

	assert(engine_val.offset == 0);
	result.offset = current_val.offset?current_val.offset:animation_val.offset;

	result.submerge = current_val.submerge?current_val.submerge:animation_val.submerge;
	if(primary && engine_val.submerge) result.submerge = engine_val.submerge;

	assert(engine_val.x == 0);
	result.x = current_val.x?current_val.x:animation_val.x;

	result.y = current_val.y?current_val.y:animation_val.y; 
	result.y += engine_val.y;

	return result;
}
