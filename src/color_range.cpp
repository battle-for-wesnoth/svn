/* $Id: team.hpp 9124 2005-12-12 04:09:50Z darthfool $ */
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
#include "color_range.hpp"
#include "serialization/string_utils.hpp"
#include "wassert.hpp"

#include <string>
#include <vector>
#include <set>

std::map<Uint32, Uint32> recolor_range(const color_range& new_range, const std::vector<Uint32>& old_rgb){
	std::map<Uint32, Uint32> map_rgb;

	Uint16 new_red  = (new_range.mid() & 0x00FF0000)>>16;
    	Uint16 new_green= (new_range.mid() & 0x0000FF00)>>8;
    	Uint16 new_blue = (new_range.mid() & 0x000000FF);
	Uint16 max_red  = (new_range.max() & 0x00FF0000)>>16;
	Uint16 max_green= (new_range.max() & 0x0000FF00)>>8 ;
	Uint16 max_blue = (new_range.max() & 0x000000FF)    ;
	Uint16 min_red  = (new_range.min() & 0x00FF0000)>>16;
	Uint16 min_green= (new_range.min() & 0x0000FF00)>>8 ;
	Uint16 min_blue = (new_range.min() & 0x000000FF)    ;

	//map first color in vector to exact new color
	Uint32 temp_rgb=old_rgb[0];
	int old_r=(temp_rgb & 0X00FF0000)>>16;
	int old_g=(temp_rgb & 0X0000FF00)>>8;
	int old_b=(temp_rgb & 0X000000FF);
	Uint16 reference_avg = (Uint16)(((Uint16) old_r + (Uint16)old_g + (Uint16)old_b)
			   / 3);

	for(std::vector< Uint32 >::const_iterator temp_rgb2 = old_rgb.begin();
	      temp_rgb2 != old_rgb.end(); temp_rgb2++)
	{
		int old_r=((*temp_rgb2) & 0X00FF0000)>>16;
	     int old_g=((*temp_rgb2) & 0X0000FF00)>>8;
	     int old_b=((*temp_rgb2) & 0X000000FF);

	     const Uint16 old_avg = (Uint16)(((Uint16) old_r +
			(Uint16) old_g + (Uint16) old_b) / 3);
	      //calculate new color
	     Uint32 new_r, new_g, new_b;

	     if(reference_avg && old_avg <= reference_avg){
			float old_rat = ((float)old_avg)/reference_avg;
			new_r=Uint32( old_rat * new_red   + (1 - old_rat) * min_red);
			new_g=Uint32( old_rat * new_green + (1 - old_rat) * min_green);
			new_b=Uint32( old_rat * new_blue  + (1 - old_rat) * min_blue);
	     }else if(255 - reference_avg){
			float old_rat = ((float) 255 - old_avg) / (255 - reference_avg);
			new_r=Uint32( old_rat * new_red   + (1 - old_rat) * max_red);
			new_g=Uint32( old_rat * new_green + (1 - old_rat) * max_green);
			new_b=Uint32( old_rat * new_blue  + (1 - old_rat) * max_blue);
	     }else{
		      new_r=0; new_g=0; new_b=0; //supress warning
		      wassert(false);
			//should never get here
			//would imply old_avg > reference_avg = 255
	     }

	     if(new_r>255) new_r=255;
	     if(new_g>255) new_g=255;
	     if(new_b>255) new_b=255;

	     Uint32 newrgb = (new_r << 16) + (new_g << 8) + (new_b );
		map_rgb[*temp_rgb2]=newrgb;
	}

	return map_rgb;
}

std::vector<Uint32> string2rgb(std::string s){
	std::vector<Uint32> out;
	std::vector<std::string> rgb_vec = utils::split(s);

	//reserve two extra slots to prevent iterator invalidation
	for(int alloc_tries = 0; rgb_vec.capacity() < rgb_vec.size() + 2; ++alloc_tries)
	{
		if(alloc_tries > 99)
		{
			throw std::bad_alloc();
		}
		rgb_vec.reserve(rgb_vec.size() + 2);
	}

	std::vector<std::string>::iterator c=rgb_vec.begin();
	while(c!=rgb_vec.end())
	{
		Uint32 rgb_hex;
		if(c->length() != 6)
		{
			//integer triplets, e.g. white="255,255,255"
			while(c + 3 > rgb_vec.end())
			{
				rgb_vec.push_back("0");
			}
			rgb_hex =  (0x00FF0000 & ((lexical_cast<int>(*c++))<<16)); //red
			rgb_hex += (0x0000FF00 & ((lexical_cast<int>(*c++))<<8)); //green
			rgb_hex += (0x000000FF & ((lexical_cast<int>(*c++))<<0)); //blue
		} else {
			//hexadecimal format, e.g. white="FFFFFF"
			char* endptr;
			rgb_hex = (0x00FFFFFF & strtol(c->c_str(), &endptr, 16));
			if (*endptr != '\0') {
				throw bad_lexical_cast();
			}
			c++;
		}
		out.push_back(rgb_hex);
	}
	return(out);
}

std::vector<Uint32> palette(color_range cr){
//generate a color palette from a color range
	std::vector<Uint32> temp,res;
	std::set<Uint32> clist;
	//use blue to make master set of possible colors
	for(int i=255;i!=0;i--){
		int j=255-i;
		Uint32 rgb = i;
		temp.push_back(rgb);
		rgb = (j << 16) + (j << 8) + 255;
		temp.push_back(rgb);
	}

	//use recolor function to generate list of possible colors.
	//could use a special function, would be more efficient, but
	//harder to maintain.
	std::map<Uint32,Uint32> cmap = recolor_range(cr,temp);
	for(std::map<Uint32,Uint32>::const_iterator k=cmap.begin(); k!=cmap.end();k++){
		clist.insert(k->second);
	}
	res.push_back(cmap[255]);
	for(std::set<Uint32>::const_iterator c=clist.begin();c!=clist.end();c++){
		if(*c != res[0] && *c!=0 && *c != 0x00FFFFFF){
			res.push_back(*c);}
	}
	return(res);
}
