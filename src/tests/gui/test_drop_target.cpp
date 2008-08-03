/* $Id$ */
/*
   Copyright (C) 2008 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include <boost/test/auto_unit_test.hpp>

#include <algorithm>
#include <vector>

#include <boost/bind.hpp>

#include "sdl_utils.hpp"
#include "widgets/drop_target.hpp"

BOOST_AUTO_TEST_SUITE( test_drop_target )

BOOST_AUTO_TEST_CASE( test_create_group )
{

	gui::drop_group_manager group0;
	gui::drop_group_manager* group1 = new gui::drop_group_manager();

	BOOST_CHECK_EQUAL(group0.get_group_id(), 0);
	BOOST_CHECK_EQUAL(group1->get_group_id(), 1);
	
	delete group1;
	
	gui::drop_group_manager_ptr group2(new gui::drop_group_manager());


	BOOST_CHECK_EQUAL(group2->get_group_id(), 2);
}

typedef std::vector<gui::drop_target_ptr> target_store;

void create_drop_targets(const SDL_Rect& loc, gui::drop_group_manager_ptr group, target_store& targets, int& id_counter)
{
	gui::drop_target_ptr new_target(new gui::drop_target(group, loc));
	BOOST_CHECK_EQUAL(id_counter++, new_target->get_id());
	// Test that drop gives -1 correctly for non overlapping
	// targets
	BOOST_CHECK_EQUAL(new_target->handle_drop(), -1);
	targets.push_back(new_target);
}

BOOST_AUTO_TEST_CASE( test_create_drop_targets )
{
	gui::drop_group_manager_ptr group(new gui::drop_group_manager());
	BOOST_CHECK(group->get_group_id() > 0);

	typedef std::vector<SDL_Rect> location_store;
	location_store locations;

	// Create rectangles for drop targets
	locations.push_back(create_rect(50,50,20,20));
	locations.push_back(create_rect(50,100,20,20));
	locations.push_back(create_rect(50,150,20,20));
	locations.push_back(create_rect(50,200,20,20));
	locations.push_back(create_rect(50,250,20,20));
	locations.push_back(create_rect(50,300,20,20));

	target_store targets;

	int id_counter = 0;

	std::for_each(locations.begin(), locations.end(),
			boost::bind(create_drop_targets,_1, group, boost::ref(targets), boost::ref(id_counter)));

	BOOST_CHECK_EQUAL(targets.size(), locations.size());

	// Modify 3rd rectangle to overlap with 4th
	locations[2].y = 190;

	// Check for correct drop results
	BOOST_CHECK_EQUAL(targets[2]->handle_drop(), 3);
	BOOST_CHECK_EQUAL(targets[3]->handle_drop(), 2);
	BOOST_CHECK_EQUAL(targets[1]->handle_drop(), -1);
	BOOST_CHECK_EQUAL(targets[4]->handle_drop(), -1);
}

BOOST_AUTO_TEST_CASE( check_memory_leaks )
{
	BOOST_CHECK(gui::drop_target::empty());
}

/* vim: set ts=4 sw=4: */
BOOST_AUTO_TEST_SUITE_END()
