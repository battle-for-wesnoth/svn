/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file image.cpp
//! Routines for images: load, scale, re-color, etc.

#include "global.hpp"

#include "color_range.hpp"
#include "config.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "image.hpp"
#include "log.hpp"
#include "sdl_utils.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"

#include "SDL_image.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>

#define ERR_DP LOG_STREAM(err, display)
#define INFO_DP LOG_STREAM(info, display)

namespace {

typedef std::map<image::locator::value, int> locator_finder_t;
typedef std::pair<image::locator::value, int> locator_finder_pair;
locator_finder_t locator_finder;

//! Definition of all image maps
image::image_cache images_,hexed_images_,scaled_to_hex_images_,scaled_to_zoom_,unmasked_images_;
image::image_cache brightened_images_,semi_brightened_images_;

// const int cache_version_ = 0;

std::map<image::locator,bool> image_existence_map;

// directories where we already cached file existence
std::set<std::string> precached_dirs;

std::map<surface, surface> reversed_images_;

int red_adjust = 0, green_adjust = 0, blue_adjust = 0;

//! List of colors used by the TC image modification
std::vector<std::string> team_colors;

std::string image_mask;

int zoom = image::tile_size;
int cached_zoom = 0;

} // end anon namespace

namespace image {

template<typename T>
void cache_type<T>::flush()
{
	typename std::vector<cache_item<T> >::iterator beg = content.begin();
	typename std::vector<cache_item<T> >::iterator end = content.end();

	for(; beg != end; ++beg) {
		beg->loaded = false;
		beg->item = T();
	}
}
mini_terrain_cache_map mini_terrain_cache;
mini_terrain_cache_map mini_fogged_terrain_cache;

void flush_cache()
{
	images_.flush();
	hexed_images_.flush();
	scaled_to_hex_images_.flush();
	scaled_to_zoom_.flush();
	unmasked_images_.flush();
	brightened_images_.flush();
	semi_brightened_images_.flush();
	mini_terrain_cache.clear();
	mini_fogged_terrain_cache.clear();
	reversed_images_.clear();
	image_existence_map.clear();
	precached_dirs.clear();
}

int locator::last_index_ = 0;

void locator::init_index()
{
	locator_finder_t::iterator i = locator_finder.find(val_);

	if(i == locator_finder.end()) {
		index_ = last_index_++;
		locator_finder.insert(locator_finder_pair(val_, index_));


	} else {
		index_ = i->second;
	}
}

void locator::parse_arguments()
{
	std::string& fn = val_.filename_;
	if(fn.empty()) {
		return;
	}
	size_t markup_field = fn.find('~');

	if(markup_field != std::string::npos) {
		val_.type_ = SUB_FILE;
		val_.modifications_ = fn.substr(markup_field, fn.size() - markup_field);
		fn = fn.substr(0,markup_field);
	}
}

locator::locator() :
	index_(-1)
{
}

locator::locator(const locator &a, const std::string& mods):
	 val_(a.val_)
{
	if(mods.size()){
			val_.modifications_ += mods;
			val_.type_=SUB_FILE;
			init_index();
	}
	else index_=a.index_;
}

locator::locator(const char *filename) :
	val_(filename)
{
	parse_arguments();
	init_index();
}

locator::locator(const std::string &filename) :
	val_(filename)
{
	parse_arguments();
	init_index();
}

locator::locator(const char *filename, const std::string& modifications) :
	val_(filename, modifications)
{
	init_index();
}

locator::locator(const std::string &filename, const std::string& modifications) :
	val_(filename, modifications)
{
	init_index();
}

locator::locator(const std::string &filename, const gamemap::location &loc, const std::string& modifications) :
	val_(filename, loc, modifications)
{
	init_index();
}

    locator::locator(const std::string &filename, const gamemap::location &loc, int center_x, int center_y, const std::string& modifications) :
        val_(filename, loc, center_x, center_y, modifications)
{
	init_index();
}

locator& locator::operator=(const locator &a)
{
	index_ = a.index_;
	val_ = a.val_;

	return *this;
}

locator::value::value(const locator::value& a) :
  type_(a.type_), filename_(a.filename_), loc_(a.loc_),
  modifications_(a.modifications_), 
  center_x_(a.center_x_), center_y_(a.center_y_)
{
}

locator::value::value() :
	type_(NONE), filename_(), loc_(), modifications_(),
  center_x_(0), center_y_(0)

{}

locator::value::value(const char *filename) :
  type_(FILE), filename_(filename), loc_(), modifications_(),
  center_x_(0), center_y_(0)

{
}


locator::value::value(const char *filename, const std::string& modifications) :
  type_(SUB_FILE), filename_(filename), loc_(), modifications_(modifications), 
  center_x_(0), center_y_(0)

{
}

locator::value::value(const std::string& filename) :
  type_(FILE), filename_(filename),  loc_(), modifications_(),
  center_x_(0), center_y_(0)

{
}

locator::value::value(const std::string& filename, const std::string& modifications) :
  type_(SUB_FILE), filename_(filename), loc_(), modifications_(modifications), 
  center_x_(0), center_y_(0)

{
}

locator::value::value(const std::string& filename, const gamemap::location& loc, const std::string& modifications) :
  type_(SUB_FILE), filename_(filename), loc_(loc), modifications_(modifications), 
  center_x_(0), center_y_(0)

{
}

locator::value::value(const std::string& filename, const gamemap::location& loc, int center_x, int center_y, const std::string& modifications) :
  type_(SUB_FILE), filename_(filename), loc_(loc), modifications_(modifications), center_x_(center_x), center_y_(center_y)
{	
}

bool locator::value::operator==(const value& a) const
{
	if(a.type_ != type_) {
		return false;
	} else if(type_ == FILE) {
		return filename_ == a.filename_;
	} else if(type_ == SUB_FILE) {
	  return filename_ == a.filename_ && loc_ == a.loc_ && modifications_ == a.modifications_ 
          && center_x_ == a.center_x_ && center_y_ == a.center_y_;
	} else {
		return false;
	}
}

bool locator::value::operator<(const value& a) const
{
	if(type_ != a.type_) {
		return type_ < a.type_;
	} else if(type_ == FILE) {
		return filename_ < a.filename_;
	} else if(type_ == SUB_FILE) {
		if(filename_ != a.filename_)
			return filename_ < a.filename_;
		if(loc_ != a.loc_)
		        return loc_ < a.loc_;
        if(center_x_ != a.center_x_)
            return center_x_ < a.center_x_;
        if(center_y_ != a.center_y_)
            return center_y_ < a.center_y_;

		return(modifications_ < a.modifications_);
	} else {
		return false;
	}
}

surface locator::load_image_file() const
{
	surface res;

	std::string location = get_binary_file_location("images", val_.filename_);

	bool try_units = false;

	do {
		if (!location.empty()) {
			res = IMG_Load(location.c_str());
		}
		if (res.null() && (!try_units)) {
			try_units = true;
			location = get_binary_file_location("images", "units/" + val_.filename_);
			if (!location.empty() && !val_.filename_.empty()) {
				LOG_STREAM(err, filesystem) << "warning: implict prefix 'units/' for '" <<
					val_.filename_ << "'. Support of this will be removed in 1.5.3\n";
			}

		} else {
			try_units = false;
		}
	} while (try_units);

	if (res.null() && !val_.filename_.empty()) {
		ERR_DP << "could not open image '" << val_.filename_ << "'\n";
		static const std::string missing = "misc/missing-image.png";
		if (game_config::debug && val_.filename_ != missing)
			return get_image(missing, UNSCALED);
	}

	return res;
}

surface locator::load_image_sub_file() const
{
	const surface mother_surface(get_image(val_.filename_, UNSCALED));
	const surface mask(get_image(game_config::terrain_mask_image, UNSCALED));

	if(mother_surface == NULL)
		return surface(NULL);
	if(mask == NULL)
		return surface(NULL);

	surface surf=mother_surface;
	if(val_.loc_.x>-1 && val_.loc_.y>-1 && val_.center_x_>-1 && val_.center_y_>-1){
		int offset_x = mother_surface->w/2 - val_.center_x_;
		int offset_y = mother_surface->h/2 - val_.center_y_;
		SDL_Rect srcrect = {
			((tile_size*3) / 4) * val_.loc_.x + offset_x,
			tile_size * val_.loc_.y + (tile_size/2) * (val_.loc_.x % 2) + offset_y,
			tile_size, tile_size
		};

		surface tmp(cut_surface(mother_surface, srcrect));
		surf=mask_surface(tmp, mask);
	}
	else if(val_.loc_.x>-1 && val_.loc_.y>-1 ){
	  SDL_Rect srcrect = {
	    ((tile_size*3) / 4) * val_.loc_.x,
	    tile_size * val_.loc_.y + (tile_size/2) * (val_.loc_.x % 2),
	    tile_size, tile_size
	  };

	  surface tmp(cut_surface(mother_surface, srcrect));
	  surf=mask_surface(tmp, mask);
	}


	if(val_.modifications_.size()){
		bool xflip = false;
		bool yflip = false;
		bool greyscale = false;
		std::map<Uint32, Uint32> recolor_map;
		std::vector<std::string> modlist = utils::paranthetical_split(val_.modifications_,'~');
		for(std::vector<std::string>::const_iterator i=modlist.begin();
			i!= modlist.end();i++){
			std::vector<std::string> tmpmod = utils::paranthetical_split(*i);
			std::vector<std::string>::const_iterator j=tmpmod.begin();
			while(j!= tmpmod.end()){
				std::string function=*j++;
				if(j==tmpmod.end()){
					if(function.size()){
						ERR_DP << "error parsing image modifications: "
							<< val_.modifications_<< "\n";
					}
					break;
				}
				std::string field = *j++;

				if("TC" == function){
					std::vector<std::string> param = utils::split(field,',');
					if(param.size() < 2)
						break;

					int side_n = lexical_cast_default<int>(param[0], -1);
					std::string team_color; 
					if (side_n < 1) {
						break;
					} else if (side_n < static_cast<int>(team_colors.size())) {
						team_color = team_colors[side_n - 1];
					} else {
					// this side is not inialized use default "n"
						team_color = lexical_cast<std::string>(side_n);
					}	

					if(game_config::tc_info(param[1]).size()){
						function="RC";
						field = param[1] + ">" + team_color;
					}
				}

				if("RC" == function){	// Re-color function
					std::vector<std::string> recolor=utils::split(field,'>');
					if(recolor.size()>1){
						std::map<Uint32, Uint32> tmp_map;
						try {
							color_range const& new_color = game_config::color_info(recolor[1]);
							std::vector<Uint32> const& old_color = game_config::tc_info(recolor[0]);
							tmp_map = recolor_range(new_color,old_color);
						} catch (config::error& e) {
							ERR_DP << "caught config::error... " << e.message << std::endl;
						}
						for(std::map<Uint32, Uint32>::const_iterator tmp = tmp_map.begin(); tmp!= tmp_map.end(); tmp++){
							recolor_map[tmp->first] = tmp->second;
						}
					}
				}
				if("FL" == function){	// Flip layer
					if(field.empty() || field.find("horiz") != std::string::npos) {
						xflip = !xflip;
					}
					if(field.find("vert") != std::string::npos) {
						yflip = !yflip;
					}
				}
				if("GS" == function){	// Flip layer
					greyscale=true;
				}
			}
		}
		surf = recolor_image(surf,recolor_map);
		if(xflip) {
			surf = flip_surface(surf);
		}
		if(yflip) {
			surf = flop_surface(surf);
		}
		if(greyscale) {
			surf = greyscale_image(surf);
		}
	}
	return surf;
}

surface locator::load_from_disk() const
{
	switch(val_.type_) {
		case FILE:
			return load_image_file();
		case SUB_FILE:
			return load_image_sub_file();
		default:
			return surface(NULL);
	}
}


manager::manager() {}

manager::~manager()
{
	flush_cache();
}

void set_wm_icon()
{
#if !(defined(__APPLE__))
	surface icon(get_image(game_config::game_icon,UNSCALED));
	if(icon != NULL) {
		::SDL_WM_SetIcon(icon,NULL);
	}
#endif
}

SDL_PixelFormat last_pixel_format;

void set_pixel_format(SDL_PixelFormat* format)
{
	assert(format != NULL);
	
	SDL_PixelFormat &f = *format;
	SDL_PixelFormat &l = last_pixel_format;
	// if the pixel format change, we clear the cache,
	// because some images are now optimized for the wrong display format
	// FIXME: 8 bpp use palette, need to compare them. For now assume a change
	if (format->BitsPerPixel == 8 ||
		f.BitsPerPixel != l.BitsPerPixel || f.BytesPerPixel != l.BytesPerPixel ||
		f.Rmask != l.Rmask || f.Gmask != l.Gmask || f.Bmask != l.Bmask ||
		f.Rloss != l.Rloss || f.Gloss != l.Gloss || f.Bloss != l.Bloss ||
		f.Rshift != l.Rshift || f.Gshift != l.Gshift || f.Bshift != l.Bshift ||
		f.colorkey != l.colorkey || f.alpha != l.alpha)
	{
		INFO_DP << "detected a new display format\n";
		flush_cache();
	}
	last_pixel_format = *format;
}

void set_colour_adjustment(int r, int g, int b)
{
	if(r != red_adjust || g != green_adjust || b != blue_adjust) {
		red_adjust = r;
		green_adjust = g;
		blue_adjust = b;
		scaled_to_hex_images_.flush();
		brightened_images_.flush();
		semi_brightened_images_.flush();
		reversed_images_.clear();
	}
}

void set_team_colors(const std::vector<std::string>* colors)
{
	if (colors == NULL)
		team_colors.clear();
	else {
		team_colors = *colors;
	}
}

void set_image_mask(const std::string& /*image*/)
{

	// image_mask are blitted in display.cpp
	// so no need to flush the cache here
/*
	if(image_mask != image) {
		image_mask = image;
		scaled_to_hex_images_.flush();
		scaled_to_zoom_.flush();
		brightened_images_.flush();
		semi_brightened_images_.flush();
		reversed_images_.clear();
	}
*/
}

void set_zoom(int amount)
{
	if(amount != zoom) {
		zoom = amount;
		scaled_to_hex_images_.flush();
		brightened_images_.flush();
		semi_brightened_images_.flush();
		reversed_images_.clear();

		// We keep these caches if:
		// we use default zoom (it doesn't need those)
		// or if they are already at the wanted zoom.
		if (zoom != tile_size && zoom != cached_zoom) {
			scaled_to_zoom_.flush();
			unmasked_images_.flush();
			cached_zoom = zoom;
		}
	}
}

static surface get_hexed(const locator i_locator)
{
	surface image(get_image(i_locator, UNSCALED));
	// Re-cut scaled tiles according to a mask.
	const surface hex(get_image(game_config::terrain_mask_image,
					UNSCALED));
	return mask_surface(image, hex);
}

static surface get_unmasked(const locator i_locator)
{
	// If no scaling needed at this zoom level,
	// we just use the hexed image.
	surface image(get_image(i_locator, HEXED));
	if (zoom != tile_size)
		return scale_surface(image, zoom, zoom);
	else
		return image;
}

static surface get_scaled_to_hex(const locator i_locator)
{
	surface res(get_image(i_locator, UNMASKED));

	// Adjusts colour if necessary.
	if (red_adjust != 0 ||
				green_adjust != 0 || blue_adjust != 0) {
		res = surface(adjust_surface_colour(res,
					red_adjust, green_adjust, blue_adjust));
	}
	/*
	const surface mask(get_image(image_mask,UNMASKED));
	if(mask != NULL) {
		SDL_SetAlpha(mask,SDL_SRCALPHA|SDL_RLEACCEL,SDL_ALPHA_OPAQUE);
		SDL_SetAlpha(res,SDL_SRCALPHA|SDL_RLEACCEL,SDL_ALPHA_OPAQUE);

		//commented out pending reply from SDL team about bug report
		//SDL_BlitSurface(mask,NULL,result,NULL);
	}*/
	return res;
}

static surface get_scaled_to_zoom(const locator i_locator)
{
	assert(zoom != tile_size);
	assert(tile_size != 0);

	surface res(get_image(i_locator, UNSCALED));
	// For some reason haloes seems to have invalid images, protect against crashing
	if(!res.null()) {
		return scale_surface(res, ((res.get()->w * zoom) / tile_size), ((res.get()->h * zoom) / tile_size));
	} else {
		return surface(NULL);
	}
}

static surface get_brightened(const locator i_locator)
{
	surface image(get_image(i_locator, SCALED_TO_HEX));
	return surface(brighten_image(image, ftofxp(1.5)));
}

static surface get_semi_brightened(const locator i_locator)
{
	surface image(get_image(i_locator, SCALED_TO_HEX));
	return surface(brighten_image(image, ftofxp(1.25)));
}

surface get_image(const image::locator& i_locator, TYPE type)
{
	surface res(NULL);
	image_cache *imap;

	if(i_locator.is_void())
		return surface(NULL);

	bool is_unscaled = false;

	switch(type) {
	case UNSCALED:
		is_unscaled = true;
		imap = &images_;
		break;
	case SCALED_TO_HEX:
		imap = &scaled_to_hex_images_;
		break;
	case SCALED_TO_ZOOM:
		// Only use separate cache if scaled
		if(zoom != tile_size) {
			imap = &scaled_to_zoom_;
		} else {
			is_unscaled = true;
			imap = &images_;
		}
		break;
	case HEXED:
		imap = &hexed_images_;
		break;
	case UNMASKED:
		// Only use separate cache if scaled
		if(zoom != tile_size) {
			imap = &unmasked_images_;
		} else {
			imap = &hexed_images_;
		}
		break;
	case BRIGHTENED:
		imap = &brightened_images_;
		break;
	case SEMI_BRIGHTENED:
		imap = &semi_brightened_images_;
		break;
	default:
		return surface(NULL);
	}

	if(i_locator.in_cache(*imap))
		return i_locator.locate_in_cache(*imap);

	// If type is unscaled, directly load the image from the disk.
	// Else, create it from the unscaled image.
	if(is_unscaled) {
		res = i_locator.load_from_disk();

		if(res == NULL) {
			 i_locator.add_to_cache(*imap, surface(NULL));
			return surface(NULL);
		}
	} else {

		// surface base_image(get_image(i_locator, UNSCALED));

		switch(type) {
		case SCALED_TO_HEX:
			res = get_scaled_to_hex(i_locator);
			break;
		case SCALED_TO_ZOOM:
			res = get_scaled_to_zoom(i_locator);
			break;
		case HEXED:
			res = get_hexed(i_locator);
			break;
		case UNMASKED:
			res = get_unmasked(i_locator);
			break;
		case BRIGHTENED:
			res = get_brightened(i_locator);
			break;
		case SEMI_BRIGHTENED:
			res = get_semi_brightened(i_locator);
			break;
		default:
			return surface(NULL);
		}
	}

	// Optimizes surface before storing it
	res = create_optimized_surface(res);
	 i_locator.add_to_cache(*imap, res);
	return res;
}

surface reverse_image(const surface& surf)
{
	if(surf == NULL) {
		return surface(NULL);
	}

	const std::map<surface,surface>::iterator itor = reversed_images_.find(surf);
	if(itor != reversed_images_.end()) {
		// sdl_add_ref(itor->second);
		return itor->second;
	}

	const surface rev(flip_surface(surf));
	if(rev == NULL) {
		return surface(NULL);
	}

	reversed_images_.insert(std::pair<surface,surface>(surf,rev));
	// sdl_add_ref(rev);
	return rev;
}

bool exists(const image::locator& i_locator, bool precached)
{
	typedef image::locator loc;
	loc::type type = i_locator.get_type();
	if (type != loc::FILE && type != loc::SUB_FILE)
		return false;

	if (precached) {
		std::map<image::locator,bool>::const_iterator b =  image_existence_map.find(i_locator);
		if (b != image_existence_map.end())
			return b->second;
		else
			return false;
	}

	// The insertion will fail if there is already an element in the cache
	std::pair< std::map< image::locator, bool >::iterator, bool >
		it = image_existence_map.insert(std::make_pair(i_locator, false));
	bool &cache = it.first->second;
	if (it.second)
		cache = !get_binary_file_location("images", i_locator.get_filename()).empty();
	return cache;
}

static void precache_file_existence_internal(const std::string& dir, const std::string& subdir)
{
	const std::string checked_dir = dir + "/" + subdir;
	if (precached_dirs.find(checked_dir) != precached_dirs.end())
		return;
	precached_dirs.insert(checked_dir);

	std::vector<std::string> files_found;
	std::vector<std::string> dirs_found;
	get_files_in_dir(checked_dir, &files_found, &dirs_found,
			FILE_NAME_ONLY, NO_FILTER, DONT_REORDER);

	for(std::vector<std::string>::const_iterator f = files_found.begin();
			f != files_found.end(); ++f) {
		image_existence_map[subdir + *f] = true;
	}

	for(std::vector<std::string>::const_iterator d = dirs_found.begin();
			d != dirs_found.end(); ++d) {
		precache_file_existence_internal(dir, subdir + *d + "/");
	}
}

void precache_file_existence(const std::string& subdir)
{
	const std::vector<std::string>& paths = get_binary_paths("images");

	for(std::vector<std::string>::const_iterator p = paths.begin();
			 p != paths.end(); ++p) {

		const std::string dir = *p + "/" + subdir;
		precache_file_existence_internal(*p, subdir);
	}
}


template<typename T>
cache_item<T>& cache_type<T>::get_element(int index){
	assert (index != -1);
	while(index >= content.size()) {
		content.push_back(cache_item<T>());
	}
	cache_item<T>& elt = content[index];
	if(elt.loaded) {
		assert(*elt.position == index);
		lru_list.erase(elt.position);
		lru_list.push_front(index);
		elt.position = lru_list.begin();
	}
	return elt;
}
template<typename T>
void cache_type<T>::on_load(int index){
	if(index == -1) return ;
	cache_item<T>& elt = content[index];
	if(!elt.loaded) return ;
	lru_list.push_front(index);
	cache_size++;
	elt.position = lru_list.begin();
	while(cache_size > cache_max_size-100) {
		cache_item<T>& elt = content[lru_list.back()];
		elt.loaded=false;
		elt.item = T();
		lru_list.pop_back();
		cache_size--;
	}
}

} // end namespace image

