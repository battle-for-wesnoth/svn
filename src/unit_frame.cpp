/* $Id: unit_frame.cpp 9735 2006-01-18 18:31:24Z boucman $ */
/*
   Copyright (C) 2006 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


#include "display.hpp"
#include "global.hpp"
#include "unit_frame.hpp"


progressive_string::progressive_string(const std::string & data,int duration)
{
		const std::vector<std::string> first_pass = utils::split(data);
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
const std::string grr;

const std::string& progressive_string::get_current_element(int current_time)const
{
	int time = 0;
	unsigned int sub_halo = 0;
	if(data_.empty()) return grr;
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
const T progressive_<T>::get_current_element(int current_time) const
{
	int time = 0;
	unsigned int sub_halo = 0;
	if(data_.empty()) return T();
	while(time < current_time&& sub_halo < data_.size()) {
		time += data_[sub_halo].second;
		sub_halo++;

	}
	if(sub_halo != 0) {
		sub_halo--;
		time -= data_[sub_halo].second;
	}
	if(sub_halo >= data_.size()) {
		sub_halo = data_.size();
		time = current_time; // never more than max allowed
	}

	const T first =  data_[sub_halo].first.first;
	const T second =  data_[sub_halo].first.second;

	return T(( double(current_time - time)/(double)(data_[sub_halo].second))*(second - first)+ first);
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

unit_frame::unit_frame() :
	 image_(), image_diagonal_(),halo_(), sound_(),
	halo_x_(), halo_y_(), duration_(0),
	blend_with_(0),blend_ratio_(),
	highlight_ratio_("1.0"),offset_("-20")
{
}

unit_frame::unit_frame(const image::locator& image, int duration,
		const std::string& highlight, const std::string& offset,
		Uint32 blend_color, const std::string& blend_rate,
		const std::string& in_halo, const std::string& halox, const std::string& haloy,
		const image::locator & diag) :
	 image_(image),image_diagonal_(diag),
	halo_(in_halo,duration),
	halo_x_(halox,duration),
	halo_y_(haloy,duration),
	duration_(duration),
	blend_with_(blend_color), blend_ratio_(blend_rate,duration),
	highlight_ratio_(highlight,duration)
{
	// let's decide of duration ourselves
	if(offset.empty()) offset_=progressive_double("-20",duration);
	else offset_=progressive_double(offset,duration);
	duration_ = maximum<int>(duration_, highlight_ratio_.duration());
	duration_ = maximum<int>(duration_, blend_ratio_.duration());
	duration_ = maximum<int>(duration_, halo_.duration());
	duration_ = maximum<int>(duration_, offset_.duration());
}

unit_frame::unit_frame(const config& cfg)
{
	image_ = image::locator(cfg["image"]);
	image_diagonal_ = image::locator(cfg["image_diagonal"]);
	sound_ = cfg["sound"];
	if(!cfg["duration"].empty()) {
		duration_ = atoi(cfg["duration"].c_str());
	} else {
		duration_ = atoi(cfg["end"].c_str()) - atoi(cfg["begin"].c_str());
	}
	halo_ = progressive_string(cfg["halo"],duration_);
	halo_x_ = progressive_int(cfg["halo_x"],duration_);
	halo_y_ = progressive_int(cfg["halo_y"],duration_);
	std::vector<std::string> tmp_blend=utils::split(cfg["blend_color"]);
	if(tmp_blend.size() ==3) blend_with_= display::rgb(atoi(tmp_blend[0].c_str()),atoi(tmp_blend[1].c_str()),atoi(tmp_blend[2].c_str()));
	blend_ratio_ = progressive_double(cfg["blend_ratio"],duration_);
	highlight_ratio_ = progressive_double(cfg["alpha"].empty()?"1.0":cfg["alpha"],duration_);
	offset_ = progressive_double(cfg["offset"].empty()?"-20":cfg["offset"],duration_);

}

bool unit_frame::does_not_change() const
{
	return halo_.does_not_change() &&
		halo_x_.does_not_change() &&
		halo_y_.does_not_change() &&
		blend_ratio_.does_not_change() &&
		highlight_ratio_.does_not_change() &&
		offset_.does_not_change();
}

