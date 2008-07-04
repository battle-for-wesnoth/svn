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

#include "dialogs.hpp"
#include "key.hpp"
#include "filesystem.hpp"
#include "gamestatus.hpp"
#include "unit_types.hpp"
//

#include "tests/utils/fake_event_source.hpp"
#include "tests/utils/fake_display.hpp"


// Linker workarounds start here
#define LD_WA

#ifdef LD_WA
#include "config.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "sdl_utils.hpp"
#include "game_events.hpp"
#include "network.hpp"
// to load libwesnoth_extras
WML_HANDLER_FUNCTION(test_sources, , , )
{
	// To load libwesnoth_core
	network::get_pending_stats();
	// To load libwesnoth_sdl
	SDL_Color color = int_to_color(255);
	std::cerr << "Fooled you\n";
	{
		config cfg;
		scoped_istream stream = preprocess_file("data/hardwired/language.cfg");
		read(cfg, *stream);
	}
}
#endif
// Linker workarounds end here

namespace test {

	struct save_dialog_fixture {
		test_utils::fake_event_source source;
	};

	BOOST_FIXTURE_TEST_SUITE( save_dialog , save_dialog_fixture)

		BOOST_AUTO_TEST_CASE( test_fake_input )
		{

			BOOST_MESSAGE("Starting endlevel test!");

			test_utils::event_node_ptr new_keypress = source.press_key(2, SDLK_RETURN);
			test_utils::event_node_ptr new_keyrelease = source.release_key(4, SDLK_RETURN);

			CKey key;
			source.start();

			while(true)
			{
				events::pump();

				BOOST_CHECK_EQUAL(key[SDLK_RETURN], new_keypress->is_fired());
				if (key[SDLK_RETURN])
					break;
			}	
			while(true)
			{	
				events::pump();
				BOOST_CHECK_EQUAL(key[SDLK_RETURN], !new_keyrelease->is_fired());
				if (!key[SDLK_RETURN])
					break;
			}
		}

		BOOST_AUTO_TEST_CASE( test_dialog_get_save_name_enter_pressed )
		{
			// fill in events to be used in test
			test_utils::event_node_ptr press_return_before = source.press_key(0, SDLK_RETURN);
			test_utils::event_node_ptr release_return_before = source.release_key(200, SDLK_RETURN);
			test_utils::event_node_ptr press_return_after = source.press_key(240, SDLK_RETURN);
			test_utils::event_node_ptr release_return_after = source.release_key(1000, SDLK_RETURN);

			// Just to make sure no forever loops happening
			source.press_key(1100, SDLK_RETURN);
			source.release_key(1200, SDLK_RETURN);
		
			std::string fname("press_enter");
			write_file(get_saves_dir() + "/" + fname +".gz", "böö");
			// Start test (set ticks start time)
			source.start();
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
