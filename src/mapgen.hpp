#ifndef MAP_GEN_HPP_INCLUDED
#define MAP_GEN_HPP_INCLUDED

#include "config.hpp"
#include "display.hpp"

#include <string>

std::string random_generate_map(const std::string& parms, const config* cfg);
config random_generate_scenario(const std::string& parms, const config* cfg);

class map_generator
{
public:

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

map_generator* create_map_generator(const std::string& name, const config* cfg);

std::string default_generate_map(size_t width, size_t height, size_t island_size, size_t island_off_center,
                                 size_t iterations, size_t hill_size,
								 size_t max_lakes, size_t nvillages, size_t nplayers, std::map<gamemap::location,std::string>* labels,
						         const config& cfg);

#endif
