#ifndef MAP_GEN_HPP_INCLUDED
#define MAP_GEN_HPP_INCLUDED

class config;
class display;

#include "map.hpp"

#include <map>
#include <string>
#include <vector>

class map_generator
{
public:
	virtual ~map_generator() {}

	//returns true iff the map generator has an interactive screen
	//which allows the user to modify how the generator behaves
	virtual bool allow_user_config() const = 0;

	//display the interactive screen which allows the user to
	//modify how the generator behaves. (This function will not
	//be called if allow_user_config() returns false)
	virtual void user_config(display& disp) = 0;

	//returns a string identifying the generator by name. The name should
	//not contain spaces.
	virtual std::string name() const = 0;

	//creates a new map and returns it. args may contain arguments to
	//the map generator
	virtual std::string create_map(const std::vector<std::string>& args) = 0;

	virtual config create_scenario(const std::vector<std::string>& args);
};

std::string default_generate_map(size_t width, size_t height, size_t island_size, size_t island_off_center,
                                 size_t iterations, size_t hill_size,
								 size_t max_lakes, size_t nvillages, size_t nplayers,
								 bool roads_between_castles, std::map<gamemap::location,std::string>* labels,
						         const config& cfg);

#endif
