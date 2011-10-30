/* $Id$ */
/*
   Copyright (C) 2009 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

// In this domain since it compares a shared string from this domain.
#define GETTEXT_DOMAIN "wesnoth-lib"

#include <boost/test/unit_test.hpp>

#include "config_cache.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formula_debugger.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "game_display.hpp"
#include "gui/auxiliary/layout_exception.hpp"
#include "gui/dialogs/addon_connect.hpp"
#include "gui/dialogs/addon_list.hpp"
#include "gui/dialogs/campaign_selection.hpp"
#include "gui/dialogs/data_manage.hpp"
#include "gui/dialogs/debug_clock.hpp"
#include "gui/dialogs/edit_label.hpp"
#include "gui/dialogs/editor_generate_map.hpp"
#include "gui/dialogs/editor_new_map.hpp"
#include "gui/dialogs/editor_resize_map.hpp"
#include "gui/dialogs/editor_settings.hpp"
#include "gui/dialogs/folder_create.hpp"
#include "gui/dialogs/formula_debugger.hpp"
#include "gui/dialogs/game_delete.hpp"
#include "gui/dialogs/game_load.hpp"
#include "gui/dialogs/game_save.hpp"
#include "gui/dialogs/gamestate_inspector.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_change_control.hpp"
#include "gui/dialogs/mp_cmd_wrapper.hpp"
#include "gui/dialogs/mp_connect.hpp"
#include "gui/dialogs/mp_create_game.hpp"
#include "gui/dialogs/mp_login.hpp"
#include "gui/dialogs/mp_method_selection.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/title_screen.hpp"
#include "gui/dialogs/tip.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/unit_attack.hpp"
#include "gui/dialogs/unit_create.hpp"
#include "gui/dialogs/wml_message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "language.hpp"
#include "map_create.hpp"
#include "tests/utils/fake_display.hpp"
#include "video.hpp"
#include "wml_exception.hpp"

#include <boost/bind.hpp>

#include <memory>

namespace gui2 {

std::vector<std::string>& unit_test_registered_window_list()
{
	static std::vector<std::string> result =
			tunit_test_access_only::get_registered_window_list();

	return result;
}

std::string unit_test_mark_as_tested(const tdialog& dialog)
{
	std::vector<std::string>& list = unit_test_registered_window_list();
	list.erase(
			std::remove(list.begin(), list.end(), dialog.window_id())
			, list.end());
	return dialog.window_id();
}

std::string unit_test_mark_popup_as_tested(const tpopup& dialog)
{
	std::vector<std::string>& list = unit_test_registered_window_list();
	list.erase(
			std::remove(list.begin(), list.end(), dialog.window_id())
			, list.end());
	return dialog.window_id();
}

twindow* unit_test_window(const tpopup& dialog)
{
	return dialog.window_;
}

class tmp_server_list;

tdialog* unit_test_mp_server_list()
{
	return tmp_connect::mp_server_list_for_unit_test();
}

} // namespace gui2

namespace {

	/** The main config, which contains the entire WML tree. */
	config main_config;

	/**
	 * Helper class to generate a dialog.
	 *
	 * This class makes sure the dialog is properly created and initialized.
	 * The specialized versions are at the end of this file.
	 */
	template<class T>
	struct twrapper
	{
		static T* create() { return new T(); }
	};

	typedef std::pair<unsigned, unsigned> tresolution;
	typedef std::vector<std::pair<unsigned, unsigned> > tresolution_list;

CVideo & video() {
	static CVideo * v_ = new CVideo(CVideo::FAKE_TEST);
	return *v_;
}

	template<class T>
	void test_resolutions(const tresolution_list& resolutions)
	{
		foreach(const tresolution& resolution, resolutions) {
			video().make_test_fake(resolution.first, resolution.second);

			boost::scoped_ptr<gui2::tdialog> dlg(twrapper<T>::create());
			BOOST_REQUIRE_MESSAGE(dlg.get(), "Failed to create a dialog.");

			const std::string id = gui2::unit_test_mark_as_tested(*(dlg.get()));

			std::string exception;
			try {
				dlg->show(video(), 1);
			} catch(gui2::tlayout_exception_width_modified&) {
				exception = "gui2::tlayout_exception_width_modified";
			} catch(gui2::tlayout_exception_width_resize_failed&) {
				exception = "gui2::tlayout_exception_width_resize_failed";
			} catch(gui2::tlayout_exception_height_resize_failed&) {
				exception = "gui2::tlayout_exception_height_resize_failed";
			} catch(twml_exception& e) {
				exception = e.dev_message;
			} catch(std::exception& e) {
				exception = e.what();
			} catch(...) {
				exception = "unknown";
			}
			BOOST_CHECK_MESSAGE(exception.empty(),
					"Test for '" << id
					<< "' Failed\nnew widgets = " << gui2::new_widgets
					<< " small gui = " << game_config::small_gui
					<< " resolution = " << resolution.first
					<< 'x' << resolution.second
					<< "\nException caught: " << exception << '.');
		}
	}

	template<class T>
	void test_popup_resolutions(const tresolution_list& resolutions)
	{
		bool interact = false;
		for(int i = 0; i < 2; ++i) {
			foreach(const tresolution& resolution, resolutions) {
				video().make_test_fake(resolution.first, resolution.second);

				boost::scoped_ptr<gui2::tpopup> dlg(twrapper<T>::create());
				BOOST_REQUIRE_MESSAGE(dlg.get(), "Failed to create a dialog.");

				const std::string id = gui2::unit_test_mark_popup_as_tested(*(dlg.get()));

				std::string exception;
				try {
					dlg->show(video(), interact);
					gui2::twindow* window = gui2::unit_test_window((*dlg.get()));
					BOOST_REQUIRE_NE(window, static_cast<void*>(NULL));
					window->draw();
				} catch(gui2::tlayout_exception_width_modified&) {
					exception = "gui2::tlayout_exception_width_modified";
				} catch(gui2::tlayout_exception_width_resize_failed&) {
					exception = "gui2::tlayout_exception_width_resize_failed";
				} catch(gui2::tlayout_exception_height_resize_failed&) {
					exception = "gui2::tlayout_exception_height_resize_failed";
				} catch(twml_exception& e) {
					exception = e.dev_message;
				} catch(std::exception& e) {
					exception = e.what();
				} catch(...) {
					exception = "unknown";
				}
				BOOST_CHECK_MESSAGE(exception.empty(),
						"Test for '" << id
						<< "' Failed\nnew widgets = " << gui2::new_widgets
						<< " small gui = " << game_config::small_gui
						<< " resolution = " << resolution.first
						<< 'x' << resolution.second
						<< "\nException caught: " << exception << '.');
			}

			interact = true;
		}
	}

	void test_tip_resolutions(const tresolution_list& resolutions
			, const std::string& id)
	{
		foreach(const tresolution& resolution, resolutions) {
			video().make_test_fake(resolution.first, resolution.second);

			std::vector<std::string>& list =
					gui2::unit_test_registered_window_list();
			list.erase(std::remove(list.begin(), list.end(), id), list.end());

			std::string exception;
			try {
/**
 * @todo The code crashes for some unknown reason when this code is disabled.
 * The backtrace however doesn't show this path, in fact the crash occurs
 * before this code is used. So not entirely sure whether it's a compiler bug
 * or a part of the static initialization fiasco. Need to test with different
 * compilers and try to find the cause.
 */
#if 0
				gui2::tip::show(video()
						, id
						, "Test messsage for a tooltip."
						, gui2::tpoint(0, 0));
#endif
			} catch(gui2::tlayout_exception_width_modified&) {
				exception = "gui2::tlayout_exception_width_modified";
			} catch(gui2::tlayout_exception_width_resize_failed&) {
				exception = "gui2::tlayout_exception_width_resize_failed";
			} catch(gui2::tlayout_exception_height_resize_failed&) {
				exception = "gui2::tlayout_exception_height_resize_failed";
			} catch(twml_exception& e) {
				exception = e.dev_message;
			} catch(std::exception& e) {
				exception = e.what();
			} catch(...) {
				exception = "unknown";
			}
			BOOST_CHECK_MESSAGE(exception.empty(),
					"Test for tip '" << id
					<< "' Failed\nnew widgets = " << gui2::new_widgets
					<< " small gui = " << game_config::small_gui
					<< " resolution = " << resolution.first
					<< 'x' << resolution.second
					<< "\nException caught: " << exception << '.');
		}
	}

const tresolution_list& get_small_gui_resolutions()
{
	static tresolution_list result;
	if(result.empty()) {
		result.push_back(std::make_pair(800, 480));
	}
	return result;
}

const tresolution_list& get_gui_resolutions()
{
	static tresolution_list result;
	if(result.empty()) {
		result.push_back(std::make_pair(800, 600));
		result.push_back(std::make_pair(1024, 768));
		result.push_back(std::make_pair(1280, 1024));
		result.push_back(std::make_pair(1680, 1050));
	}
	return result;
}

template<class T>
void test()
{
	gui2::new_widgets = false;

	for(size_t i = 0; i < 2; ++i) {

		game_config::small_gui = true;
		test_resolutions<T>(get_small_gui_resolutions());

		game_config::small_gui = false;
		test_resolutions<T>(get_gui_resolutions());

		gui2::new_widgets = true;
	}
}

template<class T>
void test_popup()
{
	gui2::new_widgets = false;

	for(size_t i = 0; i < 2; ++i) {

		game_config::small_gui = true;
		test_popup_resolutions<T>(get_small_gui_resolutions());

		game_config::small_gui = false;
		test_popup_resolutions<T>(get_gui_resolutions());

		gui2::new_widgets = true;
	}
}

void test_tip(const std::string& id)
{
	gui2::new_widgets = false;

	for(size_t i = 0; i < 2; ++i) {

		game_config::small_gui = true;
		test_tip_resolutions(get_small_gui_resolutions(), id);

		game_config::small_gui = false;
		test_tip_resolutions(get_gui_resolutions(), id);

		gui2::new_widgets = true;
	}
}

} // namespace

BOOST_AUTO_TEST_CASE(test_gui2)
{
	/**** Initialize the environment. *****/
	game_config::config_cache& cache = game_config::config_cache::instance();

	cache.clear_defines();
	cache.add_define("EDITOR");
	cache.add_define("MULTIPLAYER");
	cache.get_config(game_config::path +"/data", main_config);

	const binary_paths_manager bin_paths_manager(main_config);

	load_language_list();
	game_config::load_config(main_config.child("game_config"));

	/**** Run the tests. *****/

	/* The tdialog classes. */
	test<gui2::taddon_connect>();
	test<gui2::taddon_list>();
	test<gui2::tcampaign_selection>();
	test<gui2::tdata_manage>();
	test<gui2::tedit_label>();
	test<gui2::teditor_generate_map>();
	test<gui2::teditor_new_map>();
	test<gui2::teditor_resize_map>();
	test<gui2::teditor_settings>();
	test<gui2::tfolder_create>();
	test<gui2::tformula_debugger>();
	test<gui2::tgame_delete>();
	test<gui2::tgame_load>();
	test<gui2::tgame_save>();
	test<gui2::tgame_save_message>();
	test<gui2::tgame_save_oos>();
	test<gui2::tgamestate_inspector>();
	test<gui2::tlanguage_selection>();
	test<gui2::tmessage>();
	test<gui2::tsimple_item_selector>();
	test<gui2::tmp_change_control>();
	test<gui2::tmp_cmd_wrapper>();
	test<gui2::tmp_connect>();
	test<gui2::tmp_create_game>();
	test<gui2::tmp_login>();
	test<gui2::tmp_method_selection>();
	test<gui2::tmp_server_list>();
	test<gui2::ttitle_screen>();
	test<gui2::ttransient_message>();
//	test<gui2::tunit_attack>(); /** @todo ENABLE */
	test<gui2::tunit_create>();
	test<gui2::twml_message_left>();
	test<gui2::twml_message_right>();

	/* The tpopup classes. */
	test_popup<gui2::tdebug_clock>();

	/* The tooltip classes. */
	test_tip("tooltip_large");

	std::vector<std::string>& list = gui2::unit_test_registered_window_list();

	/*
	 * The unit attack unit test are disabled for now, they calling parameters
	 * don't allow 'NULL's needs to be fixed.
	 */
	list.erase(
			std::remove(list.begin(), list.end(), "unit_attack")
			, list.end());

	// Test size() instead of empty() to get the number of offenders
	BOOST_CHECK_EQUAL(list.size(), 0);
	foreach(const std::string& id, list) {
		std::cerr << "Window '" << id << "' registered but not tested.\n";
	}
}

BOOST_AUTO_TEST_CASE(test_make_test_fake)
{
	video().make_test_fake(10, 10);

	try {
		gui2::tmessage dlg("title", "message", true);
		dlg.show(video(), 1);
	} catch(twml_exception& e) {
		BOOST_CHECK(e.user_message == _("Failed to show a dialog, "
					"which doesn't fit on the screen."));
		return;
	} catch(...) {
	}
	BOOST_ERROR("Didn't catch the wanted exception.");
}

namespace {

template<>
struct twrapper<gui2::taddon_connect>
{
	static gui2::taddon_connect* create()
	{
		static std::string host_name = "host_name";
		return new gui2::taddon_connect(host_name, true, true);
	}
};

template<>
struct twrapper<gui2::taddon_list>
{
	static gui2::taddon_list* create()
	{
		/** @todo Would nice to add one or more dummy addons in the list. */
		static config cfg;
		return new gui2::taddon_list(cfg);
	}
};

template<>
struct twrapper<gui2::tcampaign_selection>
{
	static gui2::tcampaign_selection* create()
	{
		const config::const_child_itors &ci =
				main_config.child_range("campaign");
		static std::vector<config> campaigns(ci.first, ci.second);

		return new gui2::tcampaign_selection(campaigns);
	}
};

template<>
struct twrapper<gui2::tdata_manage>
{
	static gui2::tdata_manage* create()
	{
		/** @todo Would be nice to add real data to the config. */
		return new gui2::tdata_manage(config());
	}
};

template<>
struct twrapper<gui2::tedit_label>
{
	static gui2::tedit_label* create()
	{
		static std::string label = "Label text to modify";
		static bool team_only = false;
		return new gui2::tedit_label(label, team_only);
	}
};

template<>
struct twrapper<gui2::tformula_debugger>
{
	static gui2::tformula_debugger* create()
	{
		static game_logic::formula_debugger debugger;
		return new gui2::tformula_debugger(debugger);
	}
};

template<>
struct twrapper<gui2::tgame_load>
{
	static gui2::tgame_load* create()
	{
		/** @todo Would be nice to add real data to the config. */
		static config cfg;
		return new gui2::tgame_load(cfg);
	}

};

template<>
struct twrapper<gui2::tgame_save>
{
	static gui2::tgame_save* create()
	{
		static std::string title = "Title";
		static std::string filename = "filename";
		return new gui2::tgame_save(title, filename);
	}

};

template<>
struct twrapper<gui2::tgame_save_message>
{
	static gui2::tgame_save_message* create()
	{
		static std::string title = "Title";
		static std::string filename = "filename";
		static std::string message = "message";
		return new gui2::tgame_save_message(title, filename, message);
	}

};

template<>
struct twrapper<gui2::tgame_save_oos>
{
	static gui2::tgame_save_oos* create()
	{
		static bool ignore_all = false;
		static std::string title = "Title";
		static std::string filename = "filename";
		static std::string message = "message";
		return new gui2::tgame_save_oos(ignore_all, title, filename, message);
	}

};

template<>
struct twrapper<gui2::tgamestate_inspector>
{
	static gui2::tgamestate_inspector* create()
	{
		/**
		 * @todo Would be nice to add real data to the vconfig.
		 * It would also involve adding real data to the resources.
		 */
		static config cfg;
		static vconfig vcfg(cfg);
		return new gui2::tgamestate_inspector(vcfg);
	}

};

template<>
struct twrapper<gui2::tmessage>
{
	static gui2::tmessage* create()
	{
		return new gui2::tmessage("Title", "Message", false);
	}
};

template<>
struct twrapper<gui2::tmp_change_control>
{
	static gui2::tmp_change_control* create()
	{
		return new gui2::tmp_change_control(NULL);
	}
};

template<>
struct twrapper<gui2::tmp_cmd_wrapper>
{
	static gui2::tmp_cmd_wrapper* create()
	{
		return new gui2::tmp_cmd_wrapper("foo");
	}
};

template<>
struct twrapper<gui2::tmp_create_game>
{
	static gui2::tmp_create_game* create()
	{
		return new gui2::tmp_create_game(main_config);
	}
};

template<>
struct twrapper<gui2::tmp_login>
{
	static gui2::tmp_login* create()
	{
		return new gui2::tmp_login("label", true);
	}
};

template<>
struct twrapper<gui2::tsimple_item_selector>
{
	static gui2::tsimple_item_selector* create()
	{
		return new gui2::tsimple_item_selector("title"
				, "message"
				, std::vector<std::string>()
				, false
				, false);
	}
};

template<>
struct twrapper<gui2::teditor_generate_map>
{
	static gui2::teditor_generate_map* create()
	{
		gui2::teditor_generate_map* result = new gui2::teditor_generate_map();
		BOOST_REQUIRE_MESSAGE(result, "Failed to create a dialog.");

		std::vector<map_generator*> map_generators;
		foreach (const config &i, main_config.child_range("multiplayer")) {
			if(i["map_generation"] == "default") {
				const config &generator_cfg = i.child("generator");
				if (generator_cfg) {
					map_generators.push_back(
							create_map_generator("", generator_cfg));
				}
			}
		}
		result->set_map_generators(map_generators);

		result->set_gui(
				static_cast<display*>(&test_utils::get_fake_display(-1, -1)));

		return result;
	}
};

template<>
struct twrapper<gui2::teditor_new_map>
{
	static gui2::teditor_new_map* create()
	{
		static int width;
		static int height;
		return new gui2::teditor_new_map(width, height);
	}
};

template<>
struct twrapper<gui2::teditor_resize_map>
{
	static gui2::teditor_resize_map* create()
	{
		static int width = 0;
		static int height = 0;
		static gui2::teditor_resize_map::EXPAND_DIRECTION expand_direction =
				gui2::teditor_resize_map::EXPAND_TOP;
		static bool copy = false;
		return new gui2::teditor_resize_map(
				  width
				, height
				, expand_direction
				, copy);
	}
};

template<>
struct twrapper<gui2::teditor_settings>
{
	static gui2::teditor_settings* create()
	{
		const config &cfg = main_config.child("editor_times");
		BOOST_REQUIRE_MESSAGE(cfg, "No editor time-of-day defined");

		std::vector<time_of_day> tods;
		foreach (const config &i, cfg.child_range("time")) {
			tods.push_back(time_of_day(i));
		}
		return new gui2::teditor_settings(NULL, tods);
	}
};

template<>
struct twrapper<gui2::tfolder_create>
{
	static gui2::tfolder_create* create()
	{
		static std::string folder_name;
		return new gui2::tfolder_create(folder_name);
	}
};

template<>
struct twrapper<gui2::tmp_server_list>
{
	static gui2::tdialog* create()
	{
		return gui2::unit_test_mp_server_list();
	}
};

template<>
struct twrapper<gui2::ttransient_message>
{
	static gui2::ttransient_message* create()
	{
		return new gui2::ttransient_message("Title", false, "Message", false, "");
	}
};

template<>
struct twrapper<gui2::twml_message_left>
{
	static gui2::twml_message_left* create()
	{
		return new gui2::twml_message_left("Title", "Message", "", false);
	}
};

template<>
struct twrapper<gui2::twml_message_right>
{
	static gui2::twml_message_right* create()
	{
		return new gui2::twml_message_right("Title", "Message", "", false);
	}
};

} // namespace

