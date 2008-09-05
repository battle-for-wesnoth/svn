/* $Id$ */
/*
   Copyright (C) 2008 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of thie Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef TESTS_UTILS_PLAY_SCENARIO_HPP_INCLUDED
#define TESTS_UTILS_PLAY_SCENARIO_HPP_INCLUDED

#include "tests/utils/fake_event_source.hpp"

#include "map.hpp"

#include <string>

#include <boost/scoped_ptr.hpp>

class config;


namespace test_utils {

	class end_position_collector;

	class timing {
		static const size_t step_size = 2;
		size_t time_;
		public:
		timing() : time_(0) {}
		timing(const size_t& t) : time_(t) {}

		timing operator++()
		{ 
			return time_ += step_size;
		}

		timing operator++(int)
		{
			timing ret(*this);
			operator++();
			return ret;
		}

		operator size_t&()
		{ return time_;}

		timing operator+(const size_t& t)
		{ return timing(time_ + t*step_size);}
		
		timing operator+(const int& t)
		{ return *this + size_t(t);}

		timing& operator+=(const size_t& t)
		{ time_ += t*step_size;
		return *this;}
		
		timing& operator+=(const int& t)
		{ return *this+= size_t(t);}
	};

	/**
	 * simple abstaction to settup scenario playing test case
	 **/
	class play_scenario {
		const std::string id_;
		fake_event_source source_;
		const config& game_config_;
		timing current_time_;
		event_node_ptr end_pos_;

		/**
		 * We have to add automaticaly one enter press
		 * for objectives view
		 **/
		void add_initial_signals();
		public:
			play_scenario(const std::string& id);

			void play();

			void add_formula_command(const std::string& command);

			gamemap::location find_unit_loc(const std::string& id);
	};
}

#endif
