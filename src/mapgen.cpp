#include "global.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>

#include "cavegen.hpp"
#include "language.hpp"
#include "log.hpp"
#include "mapgen.hpp"
#include "mapgen_dialog.hpp"
#include "pathfind.hpp"
#include "race.hpp"
#include "scoped_resource.hpp"
#include "util.hpp"

//function to generate a random map, from a string which describes
//the generator to use and its arguments
std::string random_generate_map(const std::string& parms, const config* cfg)
{
	//the first token is the name of the generator, tokens after
	//that are arguments to the generator
	std::vector<std::string> parameters = config::split(parms,' ');
	util::scoped_ptr<map_generator> generator(create_map_generator(parameters.front(),cfg));
	if(generator == NULL) {
		std::cerr << "could not find map generator '" << parameters.front() << "'\n";
		return "";
	}

	parameters.erase(parameters.begin());
	return generator.get()->create_map(parameters);
}

config random_generate_scenario(const std::string& parms, const config* cfg)
{
	//the first token is the name of the generator, tokens after
	//that are arguments to the generator
	std::vector<std::string> parameters = config::split(parms,' ');
	util::scoped_ptr<map_generator> generator(create_map_generator(parameters.front(),cfg));
	if(generator == NULL) {
		std::cerr << "could not find map generator '" << parameters.front() << "'\n";
		return config();
	}

	parameters.erase(parameters.begin());
	return generator->create_scenario(parameters);
}

config map_generator::create_scenario(const std::vector<std::string>& args)
{
	config res;
	res["map_data"] = create_map(args);
	return res;
}

namespace {

typedef std::vector<std::vector<int> > height_map;
typedef std::vector<std::vector<char> > terrain_map;

typedef gamemap::location location;

//basically we generate alot of hills, each hill being centered at a certain point, with a certain radius - being a half sphere.
//Hills are combined additively to form a bumpy surface
//The size of each hill varies randomly from 1-hill_size.
//we generate 'iterations' hills in total.
//the range of heights is normalized to 0-1000
//'island_size' controls whether or not the map should tend toward an island shape, and if
//so, how large the island should be. Hills with centers that are more than 'island_size'
//away from the center of the map will be inverted (i.e. be valleys).
//'island_size' as 0 indicates no island
height_map generate_height_map(size_t width, size_t height,
                               size_t iterations, size_t hill_size,
							   size_t island_size, size_t island_off_center)
{
	height_map res(width,std::vector<int>(height,0));

	size_t center_x = width/2;
	size_t center_y = height/2;

	std::cerr << "off-centering...\n";

	if(island_off_center != 0) {
		switch(rand()%4) {
		case 0:
			center_x += island_off_center;
			break;
		case 1:
			center_y += island_off_center;
			break;
		case 2:
			if(center_x < island_off_center)
				center_x = 0;
			else
				center_x -= island_off_center;
			break;
		case 3:
			if(center_y < island_off_center)
				center_y = 0;
			else
				center_y -= island_off_center;
			break;
		}
	}

	for(size_t i = 0; i != iterations; ++i) {

		//(x1,y1) is the location of the hill, and 'radius' is the radius of the hill.
		//we iterate over all points, (x2,y2). The formula for the amount the height
		//is increased by is radius - sqrt((x2-x1)^2 + (y2-y1)^2) with negative values
		//ignored.
		//
		//rather than iterate over every single point, we can reduce the points to
		//a rectangle that contains all the positive values for this formula --
		//the rectangle is given by min_x,max_x,min_y,max_y

		//is this a negative hill? (i.e. a valley)
		bool is_valley = false;

		int x1 = island_size > 0 ? center_x - island_size + (rand()%(island_size*2)) :
			                                 int(rand()%width);
		int y1 = island_size > 0 ? center_y - island_size + (rand()%(island_size*2)) :
			                                 int(rand()%height);

		//we have to check whether this is actually a valley
		if(island_size != 0) {
			const size_t diffx = abs(x1 - int(center_x));
			const size_t diffy = abs(y1 - int(center_y));
			const size_t dist = size_t(sqrt(double(diffx*diffx + diffy*diffy)));
			is_valley = dist > island_size;
		}

		const int radius = rand()%hill_size + 1;

		const int min_x = x1 - radius > 0 ? x1 - radius : 0;
		const int max_x = x1 + radius < res.size() ? x1 + radius : res.size();
		const int min_y = y1 - radius > 0 ? y1 - radius : 0;
		const int max_y = y1 + radius < res.front().size() ? y1 + radius : res.front().size();

		for(int x2 = min_x; x2 < max_x; ++x2) {
			for(int y2 = min_y; y2 < max_y; ++y2) {
				const int xdiff = (x2-x1);
				const int ydiff = (y2-y1);

				const int height = radius - int(sqrt(double(xdiff*xdiff + ydiff*ydiff)));

				if(height > 0) {
					if(is_valley) {
						if(height > res[x2][y2]) {
							res[x2][y2] = 0;
						} else {
							res[x2][y2] -= height;
						}
					} else {
						res[x2][y2] += height;
					}
				}
			}
		}
	}

	//find the heighest and lowest points on the map for normalization
	int heighest = 0, lowest = 100000, x;
	for(x = 0; size_t(x) != res.size(); ++x) {
		for(int y = 0; size_t(y) != res[x].size(); ++y) {
			if(res[x][y] > heighest)
				heighest = res[x][y];

			if(res[x][y] < lowest)
				lowest = res[x][y];
		}
	}

	//normalize the heights to the range 0-1000
	heighest -= lowest;
	for(x = 0; size_t(x) != res.size(); ++x) {
		for(int y = 0; size_t(y) != res[x].size(); ++y) {
			res[x][y] -= lowest;
			res[x][y] *= 1000;
			if(heighest != 0)
				res[x][y] /= heighest;
		}
	}

	return res;
}

//function to generate a lake. It will create water at (x,y), and then have
//'lake_fall_off' % chance to make another water tile in each of the directions n,s,e,w.
//In each of the directions it does make another water tile, it will have 'lake_fall_off'/2 %
//chance to make another water tile in each of the directions. This will continue recursively.
bool generate_lake(terrain_map& terrain, int x, int y, int lake_fall_off, std::set<location>& locs_touched)
{
	if(x < 0 || y < 0 || size_t(x) >= terrain.size() || size_t(y) >= terrain.front().size()) {
		return false;
	}

	terrain[x][y] = 'c';
	locs_touched.insert(location(x,y));

	if((rand()%100) < lake_fall_off) {
		generate_lake(terrain,x+1,y,lake_fall_off/2,locs_touched);
	}

	if((rand()%100) < lake_fall_off) {
		generate_lake(terrain,x-1,y,lake_fall_off/2,locs_touched);
	}

	if((rand()%100) < lake_fall_off) {
		generate_lake(terrain,x,y+1,lake_fall_off/2,locs_touched);
	}

	if((rand()%100) < lake_fall_off) {
		generate_lake(terrain,x,y-1,lake_fall_off/2,locs_touched);
	}

	return true;
}

typedef gamemap::location location;

//river generation:
//rivers have a source, and then keep on flowing until they meet another body of water,
//which they flow into, or until they reach the edge of the map. Rivers will always flow
//downhill, except that they can flow a maximum of 'river_uphill' uphill - this is to
//represent the water eroding the higher ground lower.
//
//Every possible path for a river will be attempted, in random order, and the first river
//path that can be found that makes the river flow into another body of water or off the map
//will be used. If no path can be found, then the river's generation will be aborted, and
//false will be returned. true is returned if the river is generated successfully.
bool generate_river_internal(const height_map& heights, terrain_map& terrain, int x, int y, std::vector<location>& river, std::set<location>& seen_locations, int river_uphill)
{
	const bool on_map = x >= 0 && y >= 0 && x < heights.size() && y < heights.back().size();

	if(on_map && !river.empty() && heights[x][y] > heights[river.back().x][river.back().y] + river_uphill) {
		return false;
	}
	
	//if we're at the end of the river
	if(!on_map || terrain[x][y] == 'c' || terrain[x][y] == 's') {
		std::cerr << "generating river...\n";

		//generate the river
		for(std::vector<location>::const_iterator i = river.begin();
		    i != river.end(); ++i) {
			terrain[i->x][i->y] = 'c';
		}

			std::cerr << "done generating river\n";

		return true;
	}
	
	location current_loc(x,y);
	location adj[6];
	get_adjacent_tiles(current_loc,adj);
	static int items[6] = {0,1,2,3,4,5};
	std::random_shuffle(items,items+4);

	//mark that we have attempted from this location
	seen_locations.insert(current_loc);
	river.push_back(current_loc);
	for(int a = 0; a != 6; ++a) {
		const location& loc = adj[items[a]];
		if(seen_locations.count(loc) == 0) {
			const bool res = generate_river_internal(heights,terrain,loc.x,loc.y,river,seen_locations,river_uphill);
			if(res) {
				return true;
			}

		}
	}

	river.pop_back();

	return false;
}

std::vector<location> generate_river(const height_map& heights, terrain_map& terrain, int x, int y, int river_uphill)
{
	std::vector<location> river;
	std::set<location> seen_locations;
	const bool res = generate_river_internal(heights,terrain,x,y,river,seen_locations,river_uphill);
	if(!res) {
		river.clear();
	}

	return river;
}

//function to return a random tile at one of the borders of a map that is
//of the given dimensions.
location random_point_at_side(size_t width, size_t height)
{
	const int side = rand()%4;
	if(side < 2) {
		const int x = rand()%width;
		const int y = side == 0 ? 0 : height-1;
		return location(x,y);
	} else {
		const int y = rand()%height;
		const int x = side == 2 ? 0 : width-1;
		return location(x,y);
	}
}

//function which, given the map will output it in a valid format.
std::string output_map(const terrain_map& terrain)
{
	std::stringstream res;

	//remember that we only want the middle 1/9th of the map. All other
	//segments of the map are there only to give the important middle part
	//some context.
	const size_t begin_x = terrain.size()/3;
	const size_t end_x = begin_x*2;
	const size_t begin_y = terrain.front().size()/3;
	const size_t end_y = begin_y*2;

	for(size_t y = begin_y; y != end_y; ++y) {
		for(size_t x = begin_x; x != end_x; ++x) {
			res << terrain[x][y];
		}

		res << "\n";
	}

	return res.str();
}

//an object that will calculate the cost of building a road over terrain
//for use in the a_star_search algorithm.
struct road_path_calculator : cost_calculator
{
	road_path_calculator(const terrain_map& terrain, const config& cfg)
		        : calls(0), map_(terrain), cfg_(cfg),

				  //find out how windy roads should be.
				  windiness_(maximum<int>(1,atoi(cfg["road_windiness"].c_str()))) {}
	virtual double cost(const location& loc, const double so_far, const bool isDst) const;

	void terrain_changed(const location& loc) { loc_cache_.erase(loc); }
	mutable int calls;
private:
	const terrain_map& map_;
	const config& cfg_;
	int windiness_;
	mutable std::map<location,double> loc_cache_;
	mutable std::map<char,double> cache_;
};

double road_path_calculator::cost(const location& loc, const double so_far, const bool isDst) const
{
	++calls;
	if (loc.x < 0 || loc.y < 0 || loc.x >= map_.size() || loc.y >= map_.front().size())
		return (getNoPathValue());

	const std::map<location,double>::const_iterator val = loc_cache_.find(loc);
	if(val != loc_cache_.end()) {
		return val->second;
	}

	//we multiply the cost by a random amount depending upon how 'windy' the road should
	//be. If windiness is 1, that will mean that the cost is always genuine, and so
	//the road always takes the shortest path. If windiness is greater than 1, we sometimes
	//over-report costs for some segments, to make the road wind a little.
	const double windiness = windiness_ > 0 ? (double(rand()%windiness_) + 1.0) : 1.0;

	const char c = map_[loc.x][loc.y];
	const std::map<char,double>::const_iterator itor = cache_.find(c);
	if(itor != cache_.end()) {
		return itor->second*windiness;
	}

	static std::string terrain(1,'x');
	terrain[0] = c;

	const config* const child = cfg_.find_child("road_cost","terrain",terrain);
	double res = getNoPathValue();
	if(child != NULL) {
		res = double(atof((*child)["cost"].c_str()));
	}

	cache_.insert(std::pair<char,double>(c,res));
	loc_cache_.insert(std::pair<location,double>(loc,windiness*res));
	return windiness*res;
}

struct is_valid_terrain
{
	is_valid_terrain(const std::vector<std::vector<gamemap::TERRAIN> >& map, const std::string& terrain_list);
	bool operator()(int x, int y) const;
private:
	std::vector<std::vector<gamemap::TERRAIN> > map_;
	const std::string& terrain_;
};

is_valid_terrain::is_valid_terrain(const std::vector<std::vector<gamemap::TERRAIN> >& map, const std::string& terrain_list)
: map_(map), terrain_(terrain_list)
{}

bool is_valid_terrain::operator()(int x, int y) const
{
	if(x < 0 || x >= map_.size() || y < 0 || y >= map_[x].size()) {
		return false;
	}

	return std::find(terrain_.begin(),terrain_.end(),map_[x][y]) != terrain_.end();
}

int rank_castle_location(int x, int y, const is_valid_terrain& valid_terrain, int min_x, int max_x, int min_y, int max_y,
						 size_t min_distance, const std::vector<gamemap::location>& other_castles, int highest_ranking)
{
	const gamemap::location loc(x,y);

	size_t avg_distance = 0, lowest_distance = 1000;

	for(std::vector<gamemap::location>::const_iterator c = other_castles.begin(); c != other_castles.end(); ++c) {
		const size_t distance = distance_between(loc,*c);
		if(distance < 6) {
			return 0;
		}

		if(distance < lowest_distance) {
			lowest_distance = distance;
		}

		if(distance < min_distance) {
			avg_distance = 0;
			break;
		}

		avg_distance += distance;
	}

	if(other_castles.empty() == false) {
		avg_distance /= other_castles.size();
	}

	for(int i = x-1; i <= x+1; ++i) {
		for(int j = y-1; j <= y+1; ++j) {
			if(!valid_terrain(i,j)) {
				return 0;
			}
		}
	}

	const int x_from_border = minimum<int>(x - min_x,max_x - x);
	const int y_from_border = minimum<int>(y = min_y,max_y - y);

	const int border_ranking = min_distance - minimum<int>(x_from_border,y_from_border) +
	                           min_distance - x_from_border - y_from_border;

	int current_ranking = border_ranking*2 + avg_distance*10 + lowest_distance*10;
	static const int num_nearby_locations = 11*11;

	const int max_possible_ranking = current_ranking + num_nearby_locations;

	if(max_possible_ranking < highest_ranking) {
		return current_ranking;
	}

	int surrounding_ranking = 0;
	
	for(int xpos = x-5; xpos <= x+5; ++xpos) {
		for(int ypos = y-5; ypos <= y+5; ++ypos) {
			if(valid_terrain(xpos,ypos)) {
				++surrounding_ranking;
			}
		}
	}

	return surrounding_ranking + current_ranking;
}

gamemap::location place_village(const std::vector<std::vector<gamemap::TERRAIN> >& map,
								size_t x, size_t y, size_t radius, const config& cfg)
{
	const gamemap::location loc(x,y);
	std::set<gamemap::location> locs;
	get_tiles_radius(loc,radius,locs);
	gamemap::location best_loc;
	size_t best_rating = 0;
	for(std::set<gamemap::location>::const_iterator i = locs.begin(); i != locs.end(); ++i) {
		if(i->x < 0 || i->y < 0 || i->x >= map.size() || i->y >= map[i->x].size()) {
			continue;
		}

		const std::string str(1,map[i->x][i->y]);
		const config* const child = cfg.find_child("village","terrain",str);
		if(child != NULL) {
			size_t rating = atoi((*child)["rating"].c_str());
			gamemap::location adj[6];
			get_adjacent_tiles(gamemap::location(i->x,i->y),adj);
			for(size_t n = 0; n != 6; ++n) {
				if(adj[n].x < 0 || adj[n].y < 0 || adj[n].x >= map.size() || adj[n].y >= map[adj[n].x].size()) {
					continue;
				}

				const gamemap::TERRAIN t = map[adj[n].x][adj[n].y];
				const std::string& adjacent_liked = (*child)["adjacent_liked"];
				rating += std::count(adjacent_liked.begin(),adjacent_liked.end(),t);
			}

			if(rating > best_rating) {
				best_loc = gamemap::location(i->x,i->y);
				best_rating = rating;
			}
		}
	}

	return best_loc;
}

std::string generate_name(const unit_race& name_generator, const std::string& id, std::string* base_name=NULL,
						  std::map<std::string,std::string>* additional_symbols=NULL)
{
	const std::vector<std::string>& options = config::split(string_table[id]);
	if(options.empty() == false) {
		const size_t choice = rand()%options.size();
		std::cerr << "calling name generator...\n";
		const std::string& name = name_generator.generate_name(unit_race::MALE);
		std::cerr << "name generator returned '" << name << "'\n";
		if(base_name != NULL) {
			*base_name = name;
		}

		std::cerr << "assigned base name..\n";
		std::map<std::string,std::string> table;
		if(additional_symbols == NULL) {
			additional_symbols = &table;
		}

		std::cerr << "got additional symbols\n";

		(*additional_symbols)["name"] = name;
		std::cerr << "interpolation variables into '" << options[choice] << "'\n";
		return config::interpolate_variables_into_string(options[choice],additional_symbols);
	}

	return "";
}

//the configuration file should contain a number of [height] tags:
//[height]
//height=n
//terrain=x
//[/height]
//these should be in descending order of n. They are checked sequentially, and if
//height is greater than n for that tile, then the tile is set to terrain type x.
class terrain_height_mapper
{
public:
	explicit terrain_height_mapper(const config& cfg);

	bool convert_terrain(int height) const;
	gamemap::TERRAIN convert_to() const;

private:
	int terrain_height;
	gamemap::TERRAIN to;
};

terrain_height_mapper::terrain_height_mapper(const config& cfg) : terrain_height(lexical_cast_default<int>(cfg["height"],0)), to('g')
{
	const std::string& terrain = cfg["terrain"];
	if(terrain != "") {
		to = terrain[0];
	}
}

bool terrain_height_mapper::convert_terrain(int height) const
{
	return height >= terrain_height;
}

gamemap::TERRAIN terrain_height_mapper::convert_to() const
{
	return to;
}

class terrain_converter
{
public:
	explicit terrain_converter(const config& cfg);

	bool convert_terrain(gamemap::TERRAIN terrain, int height, int temperature) const;
	gamemap::TERRAIN convert_to() const;

private:
	int min_temp, max_temp, min_height, max_height;
	std::string from;
	gamemap::TERRAIN to;
};

terrain_converter::terrain_converter(const config& cfg) : min_temp(-1), max_temp(-1), min_height(-1), max_height(-1), from(cfg["from"]), to(0)
{
	min_temp = lexical_cast_default<int>(cfg["min_temperature"],-100000);
	max_temp = lexical_cast_default<int>(cfg["max_temperature"],100000);
	min_height = lexical_cast_default<int>(cfg["min_height"],-100000);
	max_height = lexical_cast_default<int>(cfg["max_height"],100000);

	const std::string& to_str = cfg["to"];
	if(to_str != "") {
		to = to_str[0];
	}
}

bool terrain_converter::convert_terrain(gamemap::TERRAIN terrain, int height, int temperature) const
{
	return std::find(from.begin(),from.end(),terrain) != from.end() && height >= min_height && height <= max_height &&
	       temperature >= min_temp && temperature <= max_temp && to != 0;
}

gamemap::TERRAIN terrain_converter::convert_to() const
{
	return to;
}

}

//function to generate the map.
std::string default_generate_map(size_t width, size_t height, size_t island_size, size_t island_off_center,
                                 size_t iterations, size_t hill_size,
						         size_t max_lakes, size_t nvillages, size_t nplayers, bool roads_between_castles,
								 std::map<gamemap::location,std::string>* labels, const config& cfg)
{
	log_scope("map generation");

	//odd widths are nasty, so make them even
	if(is_odd(width)) {
		++width;
	}

	int ticks = SDL_GetTicks();

	//find out what the 'flatland' on this map is. i.e. grassland.
	std::string flatland = cfg["default_flatland"];
	if(flatland == "") {
		flatland = "g";
	}

	const char grassland = flatland[0];

	//we want to generate a map that is 9 times bigger than the
	//actual size desired. Only the middle part of the map will be
	//used, but the rest is so that the map we end up using can
	//have a context (e.g. rivers flowing from out of the map into
	//the map, same for roads, etc etc)
	width *= 3;
	height *= 3;
	
	std::cerr << "generating height map...\n";
	//generate the height of everything.
	const height_map heights = generate_height_map(width,height,iterations,hill_size,island_size,island_off_center);
	std::cerr << "done generating height map...\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	const config* const names_info = cfg.child("naming");
	config naming;
	if(names_info != NULL) {
		naming = *names_info;
	}

	std::cerr << "random map config: '" << cfg.write() << "'\n";
	std::cerr << "making dummy race for naming: '" << naming.write() << "'\n";
	//make a dummy race for generating names
	unit_race name_generator(naming);

	std::vector<terrain_height_mapper> height_conversion;

	const config::child_list& height_cfg = cfg.get_children("height");
	for(config::child_list::const_iterator h = height_cfg.begin(); h != height_cfg.end(); ++h) {
		height_conversion.push_back(terrain_height_mapper(**h));
	}

	terrain_map terrain(width,std::vector<char>(height,grassland));
	size_t x, y;
	for(x = 0; x != heights.size(); ++x) {
		for(y = 0; y != heights[x].size(); ++y) {
			for(std::vector<terrain_height_mapper>::const_iterator i = height_conversion.begin();
			    i != height_conversion.end(); ++i) {
				if(i->convert_terrain(heights[x][y])) {
					terrain[x][y] = i->convert_to();
					break;
				}
			}
		}
	}

	std::cerr << "placed land forms\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	//now that we have our basic set of flatland/hills/mountains/water, we can place lakes
	//and rivers on the map. All rivers are sourced at a lake. Lakes must be in high land -
	//at least 'min_lake_height'. (Note that terrain below a certain altitude may be made
	//into bodies of water in the code above - i.e. 'sea', but these are not considered 'lakes' because
	//they are not sources of rivers).
	//
	//we attempt to place 'max_lakes' lakes. Each lake will be placed at a random location,
	//if that random location meets the minimum terrain requirements for a lake.
	//We will also attempt to source a river from each lake.
	std::set<location> lake_locs;

	std::map<location,std::string> river_names, lake_names;

	const int nlakes = max_lakes > 0 ? (rand()%max_lakes) : 0;
	for(size_t lake = 0; lake != nlakes; ++lake) {
		for(int tries = 0; tries != 100; ++tries) {
			const int x = rand()%width;
			const int y = rand()%height;
			if(heights[x][y] > atoi(cfg["min_lake_height"].c_str())) {
				const std::vector<location> river = generate_river(heights,terrain,x,y,atoi(cfg["river_frequency"].c_str()));
				
				if(river.empty() == false && labels != NULL) {
					std::string base_name;
					std::cerr << "generating name for river...\n";
					const std::string& name = generate_name(name_generator,"river_name",&base_name);
					std::cerr << "named river '" << name << "'\n";
					size_t name_frequency = 20;
					for(std::vector<location>::const_iterator r = river.begin(); r != river.end(); ++r) {

						const gamemap::location loc(r->x-width/3,r->y-height/3);

						if(((r - river.begin())%name_frequency) == name_frequency/2) {
							labels->insert(std::pair<gamemap::location,std::string>(loc,name));
						}

						river_names.insert(std::pair<location,std::string>(loc,base_name));
					}

					std::cerr << "put down river name...\n";
				}

				std::cerr << "generating lake...\n";
				std::set<location> locs;
				const bool res = generate_lake(terrain,x,y,atoi(cfg["lake_size"].c_str()),locs);
				if(res && labels != NULL) {
					bool touches_other_lake = false;

					std::string base_name;
					const std::string& name = generate_name(name_generator,"lake_name",&base_name);

					std::set<location>::const_iterator i;

					//only generate a name if the lake hasn't touched any other lakes, so that we
					//don't end up with one big lake with multiple names
					for(i = locs.begin(); i != locs.end(); ++i) {
						if(lake_locs.count(*i) != 0) {
							touches_other_lake = true;

							//reassign the name of this lake to be the same as the other lake
							const location loc(i->x-width/3,i->y-height/3);
							const std::map<location,std::string>::const_iterator other_name = lake_names.find(loc);
							if(other_name != lake_names.end()) {
								base_name = other_name->second;
							}
						}

						lake_locs.insert(*i);
					}

					if(!touches_other_lake) {
						const gamemap::location loc(x-width/3,y-height/3);
						labels->erase(loc);
						labels->insert(std::pair<gamemap::location,std::string>(loc,name));
					}

					for(i = locs.begin(); i != locs.end(); ++i) {
						const location loc(i->x-width/3,i->y-height/3);
						lake_names.insert(std::pair<location,std::string>(*i,base_name));
					}
				}

				break;
			}
		}
	}

	std::cerr << "done generating rivers...\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	const size_t default_dimensions = 40*40*9;

	//convert grassland terrain to other types of flat terrain.
	//we generate a 'temperature map' which uses the height generation algorithm to
	//generate the temperature levels all over the map. Then we can use a combination
	//of height and terrain to divide terrain up into more interesting types than the default
	const height_map temperature_map = generate_height_map(width,height,
	                                                       (atoi(cfg["temperature_iterations"].c_str())*width*height)/default_dimensions,
														   atoi(cfg["temperature_size"].c_str()),0,0);

	std::cerr << "generated temperature map...\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	std::vector<terrain_converter> converters;
	const config::child_list& converter_items = cfg.get_children("convert");
	for(config::child_list::const_iterator cv = converter_items.begin(); cv != converter_items.end(); ++cv) {
		converters.push_back(terrain_converter(**cv));
	}

	std::cerr << "created terrain converters\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();


	//iterate over every flatland tile, and determine what type of flatland it is,
	//based on our [convert] tags.
	for(x = 0; x != width; ++x) {
		for(y = 0; y != height; ++y) {
			for(std::vector<terrain_converter>::const_iterator i = converters.begin(); i != converters.end(); ++i) {
				if(i->convert_terrain(terrain[x][y],heights[x][y],temperature_map[x][y])) {
					terrain[x][y] = i->convert_to();
					break;
				}
			}
		}
	}

	std::cerr << "placing villages...\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	//place villages in a 'grid', to make placing fair, but with villages
	//displaced from their position according to terrain and randomness, to
	//add some variety.

	std::set<location> villages;

	std::cerr << "placing castles...\n";

	//try to find configuration for castles.
	const config* const castle_config = cfg.child("castle");
	if(castle_config == NULL) {
		std::cerr << "Could not find castle configuration\n";
		return "";
	}

	//castle configuration tag contains a 'valid_terrain' attribute which is a list of
	//terrains that the castle may appear on.
	const is_valid_terrain terrain_tester(terrain,(*castle_config)["valid_terrain"]);

	//attempt to place castles at random. Once we have placed castles, we run a sanity
	//check to make sure that the castles are well-placed. If the castles are not well-placed,
	//we try again. Definition of 'well-placed' is if no two castles are closer than
	//'min_distance' hexes from each other, and the castles appear on a terrain listed
	//in 'valid_terrain'.
	std::vector<location> castles;
	std::set<location> failed_locs;

	for(size_t player = 0; player != nplayers; ++player) {
		std::cerr << "placing castle for " << player << "\n";
		log_scope("placing castle");
		const int min_x = width/3 + 2;
		const int min_y = height/3 + 2;
		const int max_x = (width/3)*2 - 3;
		const int max_y = (height/3)*2 - 3;
		const size_t min_distance = atoi((*castle_config)["min_distance"].c_str());

		location best_loc;
		int best_ranking = -1;
		for(int x = min_x; x != max_x; ++x) {
			for(int y = min_y; y != max_y; ++y) {
				const location loc(x,y);
				if(failed_locs.count(loc)) {
					continue;
				}

				const int ranking = rank_castle_location(x,y,terrain_tester,min_x,max_x,min_y,max_y,min_distance,castles,best_ranking);
				if(ranking <= 0) {
					failed_locs.insert(loc);
				}

				if(ranking > best_ranking) {
					best_ranking = ranking;
					best_loc = loc;
				}
			}
		}

		castles.push_back(best_loc);
	}

	std::cerr << "placing roads...\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	//place roads. We select two tiles at random locations on the borders of the map,
	//and try to build roads between them.
	size_t nroads = atoi(cfg["roads"].c_str());
	if(roads_between_castles) {
		nroads += castles.size()*castles.size();
	}

	std::set<location> bridges;

	road_path_calculator calc(terrain,cfg);
	for(size_t road = 0; road != nroads; ++road) {
		log_scope("creating road");

		//we want the locations to be on the portion of the map we're actually going
		//to use, since roads on other parts of the map won't have any influence,
		//and doing it like this will be quicker.
		location src = random_point_at_side(width/3 + 2,height/3 + 2);
		location dst = random_point_at_side(width/3 + 2,height/3 + 2);

		src.x += width/3 - 1;
		src.y += height/3 - 1;
		dst.x += width/3 - 1;
		dst.y += height/3 - 1;

		if(roads_between_castles && road < castles.size()*castles.size()) {
			const size_t src_castle = road/castles.size();
			const size_t dst_castle = road%castles.size();
			if(src_castle == dst_castle) {
				continue;
			}

			src = castles[src_castle];
			dst = castles[dst_castle];
		}

		//if the road isn't very interesting (on the same border), don't draw it
		else if(src.x == dst.x || src.y == dst.y) {
			continue;
		}

		if (calc.cost(src, 0.0, false) >= 1000.0 || calc.cost(dst, 0.0, true) >= 1000.0) {
			continue;
		}

		//search a path out for the road
		const paths::route rt = a_star_search(src, dst, 10000.0, &calc, width, height);

		const std::string& name = generate_name(name_generator,"road_name");
		const int name_frequency = 20;
		int name_count = 0;

		bool on_bridge = false;

		//draw the road. If the search failed, rt.steps will simply be empty
		for(std::vector<location>::const_iterator step = rt.steps.begin(); step != rt.steps.end(); ++step) {
			const int x = step->x;
			const int y = step->y;

			if(x < 0 || y < 0 || x >= width || y >= height)
				continue;

			calc.terrain_changed(*step);

			//find the configuration which tells us what to convert this tile to
			//to make it into a road.
			const std::string str(1,terrain[x][y]);
			const config* const child = cfg.find_child("road_cost","terrain",str);
			if(child != NULL) {
				//convert to bridge means that we want to convert depending
				//upon the direction the road is going.
				//typically it will be in a format like,
				//convert_to_bridge=|,/,\ 
				// '|' will be used if the road is going north-south
				// '/' will be used if the road is going south west-north east
				// '\' will be used if the road is going south east-north west
				//the terrain will be left unchanged otherwise (if there is no clear
				//direction)
				const std::string& convert_to_bridge = (*child)["convert_to_bridge"];
				if(convert_to_bridge.empty() == false) {
					if(step == rt.steps.begin() || step+1 == rt.steps.end())
						continue;

					const location& last = *(step-1);
					const location& next = *(step+1);

					location adj[6];
					get_adjacent_tiles(*step,adj);

					int direction = -1;

					//if we are going north-south
					if(last == adj[0] && next == adj[3] || last == adj[3] && next == adj[0]) {
						direction = 0;
					}
					
					//if we are going south west-north east
					else if(last == adj[1] && next == adj[4] || last == adj[4] && next == adj[1]) {
						direction = 1;
					}
					
					//if we are going south east-north west
					else if(last == adj[2] && next == adj[5] || last == adj[5] && next == adj[2]) {
						direction = 2;
					}

					if(labels != NULL && on_bridge == false) {
						on_bridge = true;
						const std::string& name = generate_name(name_generator,"bridge_name");
						const location loc(x-width/3,y-height/3);
						labels->insert(std::pair<gamemap::location,std::string>(loc,name));
						bridges.insert(loc);
					}

					if(direction != -1) {
						const std::vector<std::string> items = config::split(convert_to_bridge);
						if(size_t(direction) < items.size() && items[direction].empty() == false) {
							terrain[x][y] = items[direction][0];
						}

						continue;
					}
				} else {
					on_bridge = false;
				}

				//just a plain terrain substitution for a road
				const std::string& convert_to = (*child)["convert_to"];
				if(convert_to.empty() == false) {
					if(labels != NULL && terrain[x][y] != convert_to[0] && name_count++ == name_frequency && on_bridge == false) {
						labels->insert(std::pair<gamemap::location,std::string>(gamemap::location(x-width/3,y-height/3),name));
						name_count = 0;
					}

					terrain[x][y] = convert_to[0];
				}
			}
		}

		std::cerr << "looked at " << calc.calls << " locations\n";
	}


	//now that road drawing is done, we can plonk down the castles.
	for(std::vector<location>::const_iterator c = castles.begin(); c != castles.end(); ++c) {
		if(c->valid() == false) {
			continue;
		}

		const int x = c->x;
		const int y = c->y;
		const int player = c - castles.begin();
		terrain[x][y] = '1' + player;
		terrain[x-1][y] = 'C';
		terrain[x+1][y] = 'C';
		terrain[x][y-1] = 'C';
		terrain[x][y+1] = 'C';
		terrain[x-1][y-1] = 'C';
		terrain[x-1][y+1] = 'C';
		terrain[x+1][y-1] = 'C';
		terrain[x+1][y+1] = 'C';

		//remove all labels under the castle tiles
		if(labels != NULL) {
			labels->erase(location(x-width/3,y-height/3));
			labels->erase(location(x-1-width/3,y-height/3));
			labels->erase(location(x+1-width/3,y-height/3));
			labels->erase(location(x-width/3,y-1-height/3));
			labels->erase(location(x-width/3,y+1-height/3));
			labels->erase(location(x-1-width/3,y-1-height/3));
			labels->erase(location(x-1-width/3,y+1-height/3));
			labels->erase(location(x+1-width/3,y-1-height/3));
			labels->erase(location(x+1-width/3,y+1-height/3));
		}
	}

	std::cerr << "placed castles\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();

	if(nvillages > 0) {
		const config* const naming = cfg.child("village_naming");
		config naming_cfg;
		if(naming != NULL) {
			naming_cfg = *naming;
		}

		const unit_race village_names_generator(naming_cfg);

		//first we work out the size of the x and y distance between villages
		const size_t tiles_per_village = ((width*height)/9)/nvillages;
		size_t village_x = 1, village_y = 1;
	
		//alternate between incrementing the x and y value. When they are high enough
		//to equal or exceed the tiles_per_village, then we have them to the value
		//we want them at.
		while(village_x*village_y < tiles_per_village) {
			if(village_x < village_y) {
				++village_x;
			} else {
				++village_y;
			}
		}

		std::set<std::string> used_names;
	
		for(size_t vx = 0; vx < width; vx += village_x) {
			std::cerr << "village at " << vx << "\n";
			for(size_t vy = rand()%village_y; vy < height; vy += village_y) {
				
				const size_t add_x = rand()%3;
				const size_t add_y = rand()%3;
				const size_t x = (vx + add_x) - 1;
				const size_t y = (vy + add_y) - 1;
	
				const gamemap::location res = place_village(terrain,x,y,2,cfg);
	
				if(res.x >= width/3 && res.x < (width*2)/3 && res.y >= height/3 && res.y < (height*2)/3) {
					const std::string str(1,terrain[res.x][res.y]);
					const config* const child = cfg.find_child("village","terrain",str);
					if(child != NULL) {
						const std::string& convert_to = (*child)["convert_to"];
						if(convert_to != "") {
							terrain[res.x][res.y] = convert_to[0];
							villages.insert(res);

							if(labels != NULL && naming_cfg.empty() == false) {
								const gamemap::location loc(res.x-width/3,res.y-height/3);

								gamemap::location adj[6];
								get_adjacent_tiles(loc,adj);

								std::string name_type = "village_name";

								const std::string field = "g", forest = "f", mountain = "m", hill = "h";

								size_t field_count = 0, forest_count = 0, mountain_count = 0, hill_count = 0;

								std::map<std::string,std::string> symbols;

								size_t n;
								for(n = 0; n != 6; ++n) {
									const std::map<location,std::string>::const_iterator river_name = river_names.find(adj[n]);
									if(river_name != river_names.end()) {
										symbols["river"] = river_name->second;
										name_type = "village_name_river";

										if(bridges.count(loc)) {
											name_type = "village_name_river_bridge";
										}

										break;
									}

									const std::map<location,std::string>::const_iterator lake_name = lake_names.find(adj[n]);
									if(lake_name != lake_names.end()) {
										symbols["lake"] = lake_name->second;
										name_type = "village_name_lake";
										break;
									}
									
									const gamemap::TERRAIN terr = terrain[adj[n].x+width/3][adj[n].y+height/3];

									if(std::count(field.begin(),field.end(),terr) > 0) {
										++field_count;
									} else if(std::count(forest.begin(),forest.end(),terr) > 0) {
										++forest_count;
									} else if(std::count(hill.begin(),hill.end(),terr) > 0) {
										++hill_count;
									} else if(std::count(mountain.begin(),mountain.end(),terr) > 0) {
										++mountain_count;
									}
								}

								if(n == 6) {
									if(field_count == 6) {
										name_type = "village_name_grassland";
									} else if(forest_count >= 2) {
										name_type = "village_name_forest";
									} else if(mountain_count >= 1) {
										name_type = "village_name_mountain";
									} else if(hill_count >= 2) {
										name_type = "village_name_hill";
									}
								}

								std::string name;
								for(size_t ntry = 0; ntry != 30 && (ntry == 0 || used_names.count(name) > 0); ++ntry) {
									name = generate_name(village_names_generator,name_type,NULL,&symbols);
								}

								used_names.insert(name);
								labels->insert(std::pair<gamemap::location,std::string>(loc,name));
							}
						}
					}
				}
			}
		}
	}

	std::cerr << "placed villages\n";
	std::cerr << (SDL_GetTicks() - ticks) << "\n"; ticks = SDL_GetTicks();


	return output_map(terrain);
}

namespace {

typedef std::map<std::string,map_generator*> generator_map;
generator_map generators;

}

map_generator* create_map_generator(const std::string& name, const config* cfg)
{
	if(name == "default" || name == "") {
		return new default_map_generator(cfg);
	} else if(name == "cave") {
		return new cave_map_generator(cfg);
	} else {
		return NULL;
	}
}

#ifdef TEST_MAPGEN

int main(int argc, char** argv)
{
	int x = 50, y = 50, iterations = 50, hill_size = 50, lakes=3,
	    nvillages = 25, nplayers = 2;
	if(argc >= 2) {
		x = atoi(argv[1]);
	}

	if(argc >= 3) {
		y = atoi(argv[2]);
	}

	if(argc >= 4) {
		iterations = atoi(argv[3]);
	}

	if(argc >= 5) {
		hill_size = atoi(argv[4]);
	}

	if(argc >= 6) {
		lakes = atoi(argv[5]);
	}

	if(argc >= 7) {
		nvillages = atoi(argv[6]);
	}

	if(argc >= 8) {
		nplayers = atoi(argv[7]);
	}

	srand(time(NULL));
	std::cout << generate_map(x,y,iterations,hill_size,lakes,nvillages,nplayers) << "\n";
}

#endif
