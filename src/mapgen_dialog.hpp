#ifndef MAPGEN_DIALOG_HPP_INCLUDED
#define MAPGEN_DIALOG_HPP_INCLUDED

#include "mapgen.hpp"

class default_map_generator : public map_generator
{
public:
	default_map_generator(const config& game_config);

	bool allow_user_config() const;
	void user_config(display& disp);

	std::string name() const;

	std::string create_map(const std::vector<std::string>& args) const;

private:
	size_t width_, height_, iterations_, hill_size_, max_lakes_, nvillages_, nplayers_;
	const config* cfg_;
};

#endif
