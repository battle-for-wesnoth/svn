#ifndef RACE_HPP_INCLUDED
#define RACE_HPP_INCLUDED

#include <map>
#include <string>
#include <vector>

#include "serialization/string_utils.hpp"
#include "config.hpp"

typedef std::map<wide_string, std::vector<wchar_t> > markov_prefix_map;

class unit_race
{
public:
	enum GENDER { MALE, FEMALE, NUM_GENDERS };

	unit_race();
	unit_race(const config& cfg);

	const t_string& name() const;

	std::string generate_name(GENDER gender) const;

	bool uses_global_traits() const;

	const config::child_list& additional_traits() const;
	int num_traits() const;

	bool not_living() const;

private:
	t_string name_;
	int ntraits_;
	std::vector<std::string> names_[NUM_GENDERS];
	markov_prefix_map next_[NUM_GENDERS];
	int chain_size_;

	bool not_living_;

	const config::child_list* traits_;
	bool global_traits_;
};

typedef std::map<std::string,unit_race> race_map;

#endif
