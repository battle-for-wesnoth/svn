#include "config.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "image.hpp"
#include "display.hpp"
#include "font.hpp"
#include "sdl_utils.hpp"
#include "team.hpp"
#include "util.hpp"

#include "SDL_image.h"

#include <iostream>
#include <map>
#include <string>

namespace {

typedef std::map<gamemap::TERRAIN,SDL_Surface*> mini_terrain_cache_map;
mini_terrain_cache_map mini_terrain_cache;

typedef std::map<image::locator,SDL_Surface*> image_map;
image_map images_,scaledImages_,unmaskedImages_,greyedImages_;
image_map brightenedImages_,semiBrightenedImages_;

std::map<image::locator,bool> image_existance_map;

std::map<SDL_Surface*,SDL_Surface*> reversedImages_;

int red_adjust = 0, green_adjust = 0, blue_adjust = 0;

std::string image_mask;

SDL_PixelFormat* pixel_format = NULL;

const int tile_size = 72;
int zoom = tile_size;

//we have to go through all this trickery on clear_surfaces because
//some compilers don't support 'typename type::iterator'
template<typename Map,typename FwIt>
void clear_surfaces_internal(Map& surfaces, FwIt beg, FwIt end)
{
	for(; beg != end; ++beg) {
		SDL_FreeSurface(beg->second);
	}

	surfaces.clear();
}

template<typename Map>
void clear_surfaces(Map& surfaces)
{
	clear_surfaces_internal(surfaces,surfaces.begin(),surfaces.end());
}

enum TINT { GREY_IMAGE, BRIGHTEN_IMAGE, SEMI_BRIGHTEN_IMAGE };

SDL_Surface* get_tinted(const image::locator& i_locator, TINT tint)
{
	image_map* images;
	if (tint == GREY_IMAGE) {
		images = &greyedImages_;
	} else if (tint == BRIGHTEN_IMAGE) {
		images = &brightenedImages_;
	} else {
		images = &semiBrightenedImages_;
	}

	const image_map::iterator itor = images->find(i_locator);

	if(itor != images->end()) {
		return itor->second;
	}

	scoped_sdl_surface base(image::get_image(i_locator,image::SCALED));
	SDL_Surface* surface;
	if (tint == GREY_IMAGE) {
		surface = greyscale_image(base);
	} else if (tint == BRIGHTEN_IMAGE) {
		surface = brighten_image(base,1.5);
	} else {
		surface = brighten_image(base, 1.25);
	}
	images->insert(std::pair<image::locator,SDL_Surface*>(i_locator,surface));

	return surface;
}

void flush_cache()
{
	clear_surfaces(images_);
	clear_surfaces(scaledImages_);
	clear_surfaces(unmaskedImages_);
	clear_surfaces(greyedImages_);
	clear_surfaces(brightenedImages_);
	clear_surfaces(semiBrightenedImages_);
	clear_surfaces(mini_terrain_cache);
	clear_surfaces(reversedImages_);
}

SDL_Surface* load_image_file(image::locator i_locator)
{
	const std::string& location = get_binary_file_location("images",i_locator.filename);
	if(location.empty()) {
		return NULL;
	} else {
		SDL_Surface* const res = IMG_Load(location.c_str());
		if(res == NULL) {
			std::cerr << "Error: could not open image '" << location << "'\n";
		}

		return res;
	}
}

SDL_Surface * load_image_sub_file(image::locator i_locator)
{
	SDL_Surface *surf = NULL;
	SDL_Surface *tmp = NULL;
	
	scoped_sdl_surface mother_surface(image::get_image(i_locator.filename, image::UNSCALED, image::NO_ADJUST_COLOUR));
	scoped_sdl_surface mask(image::get_image(game_config::terrain_mask_image, image::UNSCALED, image::NO_ADJUST_COLOUR));
	
	if(mother_surface == NULL)
		return NULL;
	if(mask == NULL)
		return NULL;
	
	SDL_Rect srcrect = { 
		((tile_size*3) / 4) * i_locator.loc.x,
	       	tile_size * i_locator.loc.y + (tile_size/2) * (i_locator.loc.x % 2),
	       	tile_size, tile_size
	};
       
	tmp = cut_surface(mother_surface, srcrect);
	surf = mask_surface(tmp, mask);

	SDL_FreeSurface(tmp);
	
	return surf;
}

}

namespace image {

bool locator::operator==(const locator& a) const 
{
	if(a.type != type) {
		return false;
	} else if(type == FILE) {
		return filename == a.filename;
	} else if(type == SUB_FILE) {
		return filename == a.filename && loc == a.loc;
	} else {
		return false;
	}
}

bool locator::operator<(const locator &a) const
{
	if(type != a.type) {
		return type < a.type;
	} else if(type == FILE) {
		return filename < a.filename;
	} else if(type == SUB_FILE) {
		if(filename != a.filename)
			return filename < a.filename;

		return loc < a.loc;
	} else {
		return false;
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
	scoped_sdl_surface icon(get_image(game_config::game_icon,UNSCALED));
	if(icon != NULL) {
		::SDL_WM_SetIcon(icon,NULL);
	}
#endif
}

void set_pixel_format(SDL_PixelFormat* format)
{
	pixel_format = format;
	flush_cache();
}

void set_colour_adjustment(int r, int g, int b)
{
	if(r != red_adjust || g != green_adjust || b != blue_adjust) {
		red_adjust = r;
		green_adjust = g;
		blue_adjust = b;
		clear_surfaces(scaledImages_);
		clear_surfaces(greyedImages_);
		clear_surfaces(brightenedImages_);
		clear_surfaces(semiBrightenedImages_);
		clear_surfaces(reversedImages_);
	}
}

void get_colour_adjustment(int *r, int *g, int *b)
{
	if(r != NULL) {
		*r = red_adjust;
	}

	if(g != NULL) {
		*g = green_adjust;
	}

	if(b != NULL) {
		*b = blue_adjust;
	}
}

void set_image_mask(const std::string& image)
{
	if(image_mask != image) {
		image_mask = image;
		clear_surfaces(scaledImages_);
		clear_surfaces(greyedImages_);
		clear_surfaces(brightenedImages_);
		clear_surfaces(semiBrightenedImages_);
		clear_surfaces(reversedImages_);
	}

}

void set_zoom(int amount)
{
	if(amount != zoom) {
		zoom = amount;
		clear_surfaces(scaledImages_);
		clear_surfaces(greyedImages_);
		clear_surfaces(brightenedImages_);
		clear_surfaces(semiBrightenedImages_);
		clear_surfaces(unmaskedImages_);
		clear_surfaces(reversedImages_);
	}
}

SDL_Surface* get_image(const image::locator& i_locator, TYPE type, COLOUR_ADJUSTMENT adjust_colour)
{
	SDL_Surface* result = NULL;
	if(type == GREYED) {
		result = get_tinted(i_locator,GREY_IMAGE);
	} else if(type == BRIGHTENED) {
		result = get_tinted(i_locator,BRIGHTEN_IMAGE);
	} else if(type == SEMI_BRIGHTENED) {
		result = get_tinted(i_locator,SEMI_BRIGHTEN_IMAGE);
	} else {

		image_map::iterator i;

		if(type == SCALED) {
			i = scaledImages_.find(i_locator);
			if(i != scaledImages_.end()) {
				result = i->second;
				sdl_add_ref(result);
				return result;
			}
		}

		if(type == UNMASKED) {
			i = unmaskedImages_.find(i_locator);
			if(i != unmaskedImages_.end()) {
				result = i->second;
				sdl_add_ref(result);
				return result;
			}
		}

		i = images_.find(i_locator);

		if(i == images_.end()) {
			SDL_Surface* surf = NULL;

			switch(i_locator.type)
			{
			case locator::FILE:
				surf = load_image_file(i_locator);
				break;
			case locator::SUB_FILE:
				surf = load_image_sub_file(i_locator);
				break;
			default:
				surf = NULL;
			}
			

			if(surf == NULL) {
				images_.insert(std::pair<image::locator,SDL_Surface*>(i_locator,NULL));
				return NULL;
			}

			if(pixel_format != NULL) {
				SDL_Surface* const conv = display_format_alpha(surf);
				SDL_FreeSurface(surf);
				surf = conv;
			}

			i = images_.insert(std::pair<image::locator,SDL_Surface*>(i_locator,surf)).first;
		}

		if(i->second == NULL)
			return NULL;

		if(type == UNSCALED) {
			result = i->second;
		} else {

			const scoped_sdl_surface scaled_surf(scale_surface(i->second,zoom,zoom));

			if(scaled_surf == NULL) {
				return NULL;
			}

			if(adjust_colour == ADJUST_COLOUR && (red_adjust != 0 || green_adjust != 0 || blue_adjust != 0)) {
				const scoped_sdl_surface scoped_surface(result);
				result = adjust_surface_colour(scaled_surf,red_adjust,green_adjust,blue_adjust);
			} else {
				result = clone_surface(scaled_surf);
			}

			if(result == NULL) {
				return NULL;
			}

			if(type == SCALED && adjust_colour == ADJUST_COLOUR && image_mask != "") {
				const scoped_sdl_surface mask(get_image(image_mask,UNMASKED,NO_ADJUST_COLOUR));
				if(mask != NULL) {
					SDL_SetAlpha(mask,SDL_SRCALPHA|SDL_RLEACCEL,SDL_ALPHA_OPAQUE);
					SDL_SetAlpha(result,SDL_SRCALPHA|SDL_RLEACCEL,SDL_ALPHA_OPAQUE);

					//commented out pending reply from SDL team about bug report
					//SDL_BlitSurface(mask,NULL,result,NULL);
				}
			}

			if(type == UNMASKED) {
				unmaskedImages_.insert(std::pair<image::locator,SDL_Surface*>(i_locator,result));
			} else {
				scaledImages_.insert(std::pair<image::locator,SDL_Surface*>(i_locator,result));
			}
		}
	}

	if(result != NULL) {
		SDL_SetAlpha(result,SDL_SRCALPHA|SDL_RLEACCEL,SDL_ALPHA_OPAQUE);
	}

	sdl_add_ref(result);
	return result;
}

SDL_Surface* get_image_dim(const image::locator& i_locator, size_t x, size_t y)
{
	SDL_Surface* const surf = get_image(i_locator,UNSCALED);
	if(surf != NULL && (size_t(surf->w) != x || size_t(surf->h) != y)) {
		SDL_Surface* const new_image = scale_surface(surf,x,y);
		images_.erase(i_locator);

		//free surf twice: once because calling get_image adds a reference.
		//again because we also want to remove the cache reference, since
		//we're removing it from the cache
		SDL_FreeSurface(surf);
		SDL_FreeSurface(surf);

		images_[i_locator] = new_image;
		sdl_add_ref(new_image);
		return new_image;
	}

	return surf;
}

SDL_Surface* reverse_image(SDL_Surface* surf)
{
	if(surf == NULL) {
		return NULL;
	}

	const std::map<SDL_Surface*,SDL_Surface*>::iterator itor = reversedImages_.find(surf);
	if(itor != reversedImages_.end()) {
		sdl_add_ref(itor->second);
		return itor->second;
	}

	SDL_Surface* const rev = flip_surface(surf);
	if(rev == NULL) {
		return NULL;
	}

	reversedImages_.insert(std::pair<SDL_Surface*,SDL_Surface*>(surf,rev));
	sdl_add_ref(rev);
	return rev;
}

void register_image(const image::locator& id, SDL_Surface* surf)
{
	if(surf == NULL) {
		return;
	}

	image_map::iterator i = images_.find(id);
	if(i != images_.end()) {
		SDL_FreeSurface(i->second);
	}

	images_[id] = surf;
}

void register_image(const std::string &id, SDL_Surface* surf)
{
	register_image(locator(id), surf);
}

bool exists(const image::locator& i_locator)
{
	bool ret=false;

	if(i_locator.type != image::locator::FILE && 
			i_locator.type != image::locator::SUB_FILE) 
		return false;

	if(image_existance_map.find(i_locator) != image_existance_map.end())
		return image_existance_map[i_locator];

	if(get_binary_file_location("images",i_locator.filename).empty() == false) {
		ret = true;
	}

	image_existance_map[i_locator] = ret;

	return ret;
}

SDL_Surface* getMinimap(int w, int h, const gamemap& map, 
		int lawful_bonus, const team* tm)
{
	std::cerr << "generating minimap\n";
	const int scale = 8;

	std::cerr << "creating minimap " << int(map.x()*scale*0.75) << "," << int(map.y()*scale) << "\n";

	const size_t map_width = map.x()*scale*3/4;
	const size_t map_height = map.y()*scale;
	if(map_width == 0 || map_height == 0) {
		return NULL;
	}

	SDL_Surface* minimap = SDL_CreateRGBSurface(SDL_SWSURFACE,
	                               map_width,map_height,
	                               pixel_format->BitsPerPixel,
	                               pixel_format->Rmask,
	                               pixel_format->Gmask,
	                               pixel_format->Bmask,
	                               pixel_format->Amask);
	if(minimap == NULL)
		return NULL;

	std::cerr << "created minimap: " << int(minimap) << "," << int(minimap->pixels) << "\n";

	typedef mini_terrain_cache_map cache_map;
	cache_map& cache = mini_terrain_cache;

	for(int y = 0; y != map.y(); ++y) {
		for(int x = 0; x != map.x(); ++x) {

			SDL_Surface* surf = NULL;
			scoped_sdl_surface scoped_surface(NULL);
			
			const gamemap::location loc(x,y);
			if(map.on_board(loc)) {
				const bool shrouded = tm != NULL && tm->shrouded(x,y);
				const bool fogged = tm != NULL && tm->fogged(x,y) && !shrouded;
				const gamemap::TERRAIN terrain = shrouded ? gamemap::VOID_TERRAIN : map[x][y];
				cache_map::iterator i = cache.find(terrain);

				if(i == cache.end()) {
					scoped_sdl_surface tile(get_image("terrain/" + map.get_terrain_info(terrain).default_image() + ".png",image::UNSCALED));

					if(tile == NULL) {
						std::cerr << "Could not get image for terrrain '"
						          << terrain << "'\n";
						continue;
					}

					surf = scale_surface_blended(tile,scale,scale);

					if(surf == NULL) {
						continue;
					}

					i = cache.insert(cache_map::value_type(terrain,surf)).first;
				} else {
					surf = i->second;
				}

				if(fogged) {
					scoped_surface.assign(adjust_surface_colour(surf,-50,-50,-50));
					surf = scoped_surface;
				}

				assert(surf != NULL);
				
				SDL_Rect maprect = {x*scale*3/4,y*scale + (is_odd(x) ? scale/2 : 0),0,0};
				sdl_safe_blit(surf, NULL, minimap, &maprect);
			}
		}
	}

	std::cerr << "scaling minimap..." << int(minimap) << "." << int(minimap->pixels) << "\n";

	if((minimap->w != w || minimap->h != h) && w != 0) {
		const scoped_sdl_surface surf(minimap);
		minimap = scale_surface(surf,w,h);
	}

	std::cerr << "done generating minimap\n";

	return minimap;
}

}
