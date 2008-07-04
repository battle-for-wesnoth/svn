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

#include <iostream>
#include "tests/utils/fake_display.hpp"

#include "game_display.hpp"
#include "video.hpp"
#include "config.hpp"
#include "unit_types.hpp"
#include "gamestatus.hpp"

namespace test_utils {

	class fake_display_manager {
		static fake_display_manager* manager_;

		CVideo video_;
		unit_map dummy_umap_;
		config dummy_cfg_;
		gamemap dummy_map_;
		gamestatus dummy_status_;
		std::vector<team> dummy_teams_;
		const events::event_context main_event_context_;


		game_display disp_;

		public:
		static fake_display_manager* get_manager();
		game_display& get_display();

		fake_display_manager();
//		~fake_display_manager();
	};

	fake_display_manager* fake_display_manager::manager_ = 0;

	fake_display_manager* fake_display_manager::get_manager()
	{
		if (!manager_)
		{
			manager_ = new fake_display_manager();
		}
		return manager_;
	}

	fake_display_manager::fake_display_manager() :
	   	video_(CVideo::FAKE_TEST), 
		dummy_umap_(),  
		dummy_cfg_(), 
		dummy_map_(dummy_cfg_,""),
		dummy_status_(dummy_cfg_, 0),
		dummy_teams_(),
		main_event_context_(),
		disp_(dummy_umap_, video_, dummy_map_, dummy_status_,
				dummy_teams_, dummy_cfg_, dummy_cfg_, dummy_cfg_)
	{
	}

	game_display& fake_display_manager::get_display()
	{
		return disp_;
	}

	game_display& get_fake_display()
	{
		return fake_display_manager::get_manager()->get_display();
	}


}
