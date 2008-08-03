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

#include <boost/test/unit_test_suite.hpp>
#include <boost/test/unit_test.hpp>


#include "dialogs.hpp"
#include "key.hpp"
#include "filesystem.hpp"
#include "gamestatus.hpp"
#include "unit_types.hpp"
//

#include "SDL.h"

#include "tests/utils/fake_event_source.hpp"
#include "tests/utils/fake_display.hpp"
#include "tests/utils/auto_parameterized.hpp"


// Linker workarounds end here

namespace test {

	struct save_dialog_fixture {
		save_dialog_fixture()
		{
		}
		test_utils::fake_event_source source;
	};



	BOOST_FIXTURE_TEST_SUITE( save_dialog , save_dialog_fixture)

		SDLKey fake_input_keys[] =  {SDLK_KP_ENTER, SDLK_RETURN, SDLK_ESCAPE, SDLK_a};
		
		WESNOTH_PARAMETERIZED_TEST_CASE( test_fake_input, SDLKey,fake_input_keys, keyid)
		{
			test_utils::event_node_ptr new_keypress = source.press_key(2, keyid);
			test_utils::event_node_ptr new_keyrelease = source.release_key(4,keyid);

			// Protection against forever loop
			source.press_key(6, keyid);
			source.release_key(8,keyid);
			CKey key;

			while(true)
			{
				events::pump();

				BOOST_CHECK_EQUAL(key[keyid], new_keypress->is_fired());
				if (key[keyid])
					break;
			}	
			while(true)
			{	
				events::pump();
				BOOST_CHECK_EQUAL(key[keyid], !new_keyrelease->is_fired());
				if (!key[keyid])
					break;
			}
		}
		
		SDLKey dialog_get_save_name_enter_pressed[] =  {SDLK_KP_ENTER, SDLK_RETURN};

		WESNOTH_PARAMETERIZED_TEST_CASE( test_dialog_get_save_name_enter_pressed, SDLKey, dialog_get_save_name_enter_pressed, keyid )
		{
			// fill in events to be used in test
			test_utils::event_node_ptr press_return_before = source.press_key(0, keyid);
			test_utils::event_node_ptr release_return_before = source.release_key(3, keyid);
			test_utils::event_node_ptr press_return_after = source.press_key(5, keyid);
			test_utils::event_node_ptr release_return_after = source.release_key(7, keyid);

			// Protection agains for ever loop
			source.press_key(10, keyid);
			source.release_key(13, keyid);
		
			std::string fname("press_enter");
			write_file(get_saves_dir() + "/" + fname +".gz", "böö");
			// Start test (set ticks start time)
			
			// Activated enter press
			events::pump();

			BOOST_CHECK_MESSAGE(press_return_before->is_fired(), "Enter wasn't activated");
			BOOST_CHECK_MESSAGE(!release_return_before->is_fired(), "Enter was released before test");

			BOOST_CHECK_EQUAL(dialogs::get_save_name(test_utils::get_fake_display(), "Save game?", "file", &fname,gui::OK_CANCEL, "Save game", false, false), 0);

			BOOST_CHECK_MESSAGE(release_return_before->is_fired(), "get_save_name returned before releasing first enter.");
			BOOST_CHECK_MESSAGE(press_return_after->is_fired(), "get_save_name returned before 2nd enter event was sent");
			BOOST_CHECK_MESSAGE(!release_return_after->is_fired(), "get_save_name returned after 2nd release event was sent");

		}

	BOOST_AUTO_TEST_SUITE_END()
}
