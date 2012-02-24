/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
                 2008 - 2012 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "addon/manager_ui.hpp"

#include "addon/info.hpp"
#include "addon/manager.hpp"
#include "dialogs.hpp"
#include "display.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_string_utils.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/addon/description.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "wml_separators.hpp"
#include "addon/client.hpp"

static lg::log_domain log_config("config");
static lg::log_domain log_network("network");
static lg::log_domain log_filesystem("filesystem");
static lg::log_domain log_addons_client("addons-client");

#define ERR_CFG LOG_STREAM(err,   log_config)

#define ERR_NET LOG_STREAM(err,   log_network)

#define ERR_FS  LOG_STREAM(err,   log_filesystem)

#define ERR_AC  LOG_STREAM(err ,  log_addons_client)
#define WRN_AC  LOG_STREAM(warn,  log_addons_client)
#define LOG_AC  LOG_STREAM(info,  log_addons_client)
#define DBG_AC  LOG_STREAM(debug, log_addons_client)

namespace {

/** Defines various add-on installation statuses. */
enum ADDON_STATUS {
	/** Add-on is not installed. */
	ADDON_NONE,
	/** Version in the server matches local installation. */
	ADDON_INSTALLED,
	/** Version in the server is newer than local installation. */
	ADDON_INSTALLED_UPGRADABLE,
	/** Version in the server is older than local installation. */
	ADDON_INSTALLED_OUTDATED,
	/** Dependencies not satisfied.
	 *  @todo This option isn't currently implemented! */
	ADDON_INSTALLED_BROKEN,
	/** No tracking information available. */
	ADDON_NOT_TRACKED
};

/** Stores additional status information about add-ons. */
struct addon_tracking_info
{
	ADDON_STATUS state;
	bool can_publish;
	bool in_version_control;
	version_info installed_version;
};

inline const addon_info& addon_at(const std::string& id, const addons_list& addons)
{
	addons_list::const_iterator it = addons.find(id);
	assert(it != addons.end());
	return it->second;
}

bool get_addons_list(addons_client& client, addons_list& list)
{
	list.clear();

	config cfg;
	client.request_addons_list(cfg);

	if(!cfg) {
		return false;
	}

	/** @todo FIXME: get rid of this legacy "campaign"/"campaigns" silliness */
	const config::const_child_itors &addon_cfgs = cfg.child_range("campaign");

	foreach(const config& addon_cfg, addon_cfgs) {
		const std::string& id = addon_cfg["name"].str();
		if(list.find(id) != list.end()) {
			ERR_AC << "add-ons server list has multiple entries for '" << id << "', not good; ignoring them\n";
			continue;
		}
		list[id].read(addon_cfg);
	}

	return true;
}

/**
 * Return a short string describing an add-on's type.
 *
 * @param type Numerical add-on type.
 *
 * @return     A string, translated to the current locale.
 */
std::string get_translatable_addon_type(ADDON_TYPE type)
{
	switch (type) {
	case ADDON_SP_CAMPAIGN:
		return _("addon_type^Campaign");
	case ADDON_SP_SCENARIO:
		return _("addon_type^Scenario");
	case ADDON_MP_ERA:
		return _("addon_type^MP era");
	case ADDON_MP_FACTION:
		return _("addon_type^MP faction");
	case ADDON_MP_MAPS:
		return _("addon_type^MP map-pack");
	case ADDON_MP_SCENARIO:
		return _("addon_type^MP scenario");
	case ADDON_MP_CAMPAIGN:
		return _("addon_type^MP campaign");
	case ADDON_MEDIA:
		return _("addon_type^Resources");
	case ADDON_OTHER:
		return _("addon_type^Other");
	default:
		return _("addon_type^(unknown)");
	}
}

// FIXME: is it even possible for the add-ons server to provide untitled
// add-ons in its reply anymore? Titles seem to be required at upload time.
inline std::string auto_addon_title(const addon_info& addon)
{
	if(addon.title.empty()) {
		return make_addon_title(addon.id);
	} else {
		return addon.title;
	}
}

/** Warns the user about unresolved dependencies and installs them if they choose to do so. */
bool do_resolve_addon_dependencies(display& disp, addons_client& client, const addons_list& addons, const addon_info& addon, bool& wml_changed)
{
	// TODO: We don't currently check for the need to upgrade. I'll probably
	// work on that when implementing dependency tiers later.

	const std::set<std::string>& deps = addon.resolve_dependencies(addons);

	std::vector<std::string> missing_deps;
	std::vector<std::string> broken_deps;

	foreach(const std::string& dep, deps) {
		if(!is_addon_installed(dep)) {
			if(addons.find(dep) != addons.end()) {
				missing_deps.push_back(dep);
			} else {
				broken_deps.push_back(dep);
			}
		}
	}

	if(!broken_deps.empty()) {
		std::string broken_deps_report;

		broken_deps_report = _n(
			"The selected add-on has the following dependency, which is not currently installed and is not available from the server. Do you wish to continue?",
			"The selected add-on has the following dependencies, which are not currently installed and are not available from the server. Do you wish to continue?",
			broken_deps.size());
		broken_deps_report += "\n";

		foreach(const std::string& broken_dep_id, broken_deps) {
			broken_deps_report += "\n" + make_addon_title(broken_dep_id);
		}

		if(gui2::show_message(disp.video(), _("Broken Dependencies"), broken_deps_report, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return false; // canceled by user
		}
	}

	if(missing_deps.empty()) {
		// No dependencies to install, carry on.
		return true;
	}

	//
	// Prepare UI report on available dependencies.
	//

	const std::string sep(1, COLUMN_SEPARATOR);
	const std::string& header = (formatter() << HEADING_PREFIX << sep <<
		_("Name") << sep << _("Version") << sep << _("Author") << sep <<
		_("Type") << sep << _("Size")).str();

	std::vector<std::string> options(1, header);
	std::vector<int> sort_sizes;
	
	foreach(const std::string& dep, missing_deps) {
		const addon_info& addon = addon_at(dep, addons);

		const std::string& display_size = size_display_string(addon.size);
		const std::string& display_type = get_translatable_addon_type(addon.type);
		const std::string& display_icon = addon.display_icon();
		const std::string& display_version = addon.version.str();

		std::string display_title = auto_addon_title(addon);
		utils::truncate_as_wstring(display_title, 20);
		std::string display_author = addon.author;
		utils::truncate_as_wstring(display_author, 16);

		// Add negative sizes to reverse the sort order.
		sort_sizes.push_back(-addon.size);

		// NOTE: NULL_MARKUP used to escape abuse of formatting chars in add-on titles
		options.push_back(IMAGE_PREFIX + display_icon + sep +
			font::NULL_MARKUP + display_title + sep + display_version + sep +
			display_author + sep + display_type + sep +
			display_size);
	}

	/* do */ {
		gui::dialog dlg(disp, _("Install Dependencies"),
			_n("The selected add-on has the following dependency, which is not currently installed. Do you wish to install it before continuing?",
			   "The selected add-on has the following dependencies, which are not currently installed. Do you wish to install them before continuing?",
			   missing_deps.size()),
			gui::YES_NO);
		gui::menu::imgsel_style addon_style(gui::menu::bluebg_style);
		addon_style.scale_images(font::relative_size(72), font::relative_size(72));
		gui::menu* addon_menu = new gui::menu(
			disp.video(), options, false, -1,
			gui::dialog::max_menu_width, NULL, &addon_style, false);
		dlg.set_menu(addon_menu);

		if(dlg.show() < 0) {
			return true;
		}
	}

	//
	// Install dependencies now.
	//

	std::vector<std::string> failed_titles;

	foreach(const std::string& dep, missing_deps) {
		const addon_info& addon = addon_at(dep, addons);

		config archive;

		if(!(
			client.download_addon(archive, addon.id, addon.title) &&
			client.install_addon(archive, addon)
		)) {
			const std::string& server_error = client.get_last_server_error();
			if(!server_error.empty()) {
				gui2::show_error_message(disp.video(),
					std::string(_("The server responded with an error:")) + "\n" + server_error);
			}

			failed_titles.push_back(addon.title);
		} else {
			wml_changed = true;
		}
	}

	if(!failed_titles.empty()) {
		const std::string& failed_deps_report = _n(
			"The following dependency could not be installed. Do you still wish to continue?",
			"The following dependencies could not be installed. Do you still wish to continue?",
			failed_titles.size()) + std::string("\n\n") + utils::join(failed_titles, "\n");

		return gui2::show_message(disp.video(), _("Dependencies Installation Failed"), failed_deps_report, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK;
	}

	return true;
}

/** Checks whether the given add-on has local .pbl or VCS information and asks before overwriting it. */
bool do_check_before_overwriting_addon(CVideo& video, const std::string& addon_id)
{
	if(!have_addon_pbl_info(addon_id)) {
		return true;
	}

	utils::string_map symbols;
	symbols["addon"] = make_addon_title(addon_id); // FIXME: need the real title!
	const std::string& text = utils::interpolate_variables_into_string(
		_("You seem to be the author of '$addon|'. Downloading it again from the server will overwrite any changes you have made since the last upload, and it may also delete your pbl file. Do you really wish to continue?"), &symbols);

	return gui2::show_message(video, _("Confirm"), text, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK;
}

/** Performs all backend and UI actions for taking down the specified add-on. */
void do_remote_addon_delete(CVideo& video, addons_client& client, const std::string& addon_id)
{
	utils::string_map symbols;
	symbols["addon"] = make_addon_title(addon_id); // FIXME: need the real title!
	const std::string& text = utils::interpolate_variables_into_string(
		_("Deleting '$addon|' will permanently erase its download and upload counts on the add-ons server. Do you really wish to continue?"), &symbols);

	const int res = gui2::show_message(
		video, _("Confirm"), text, gui2::tmessage::yes_no_buttons);

	if(res != gui2::twindow::OK) {
		return;
	}

	std::string server_msg;
	if(!client.delete_remote_addon(addon_id, server_msg)) {
		gui2::show_error_message(video,
			std::string(_("The server responded with an error:")) + "\n" +
			client.get_last_server_error());
	} else {
		// FIXME: translation needed!
		gui2::show_transient_message(video, _("Response"), server_msg);
	}
}

/** Performs all backend and UI actions for publishing the specified add-on. */
void do_remote_addon_publish(CVideo& video, addons_client& client, const std::string& addon_id)
{
	std::string server_msg;

	if(!client.request_distribution_terms(server_msg)) {
		gui2::show_error_message(video,
			std::string(_("The server responded with an error:")) + "\n" +
			client.get_last_server_error());
	} else if(gui2::show_message(video, _("Terms"), server_msg, gui2::tmessage::ok_cancel_buttons) == gui2::twindow::OK) {
		if(!client.upload_addon(addon_id, server_msg)) {
			gui2::show_error_message(video,
				std::string(_("The server responded with an error:")) + "\n" +
				client.get_last_server_error());
		} else {
			gui2::show_transient_message(video, _("Response"), server_msg);
		}
	}
}

/**
 * Get information about an (optionally installed) add-on compared against the add-ons server.
 *
 * @param addon The add-ons server entry information.
 * @param t     The local tracking status information.
 */
void get_addon_tracking_info(const addon_info& addon, addon_tracking_info& t)
{
	const std::string& id = addon.id;

	t.can_publish = have_addon_pbl_info(id);
	t.in_version_control = have_addon_in_vcs_tree(id);
	t.installed_version = version_info();

	if(is_addon_installed(id)) {
		try {
			t.installed_version = get_addon_version_info(id);
			const version_info& remote_version = addon.version;

			if(remote_version == t.installed_version) {
				t.state = ADDON_INSTALLED;
			} else if(remote_version > t.installed_version) {
				t.state = ADDON_INSTALLED_UPGRADABLE;
			} else /* if(remote_version < t.installed_version) */ {
				t.state = ADDON_INSTALLED_OUTDATED;
			}
		} catch(version_info::not_sane_exception const&) {
			LOG_AC << "local add-on " << id << " has invalid or missing version info, skipping from updates check...\n";
			t.state = ADDON_NOT_TRACKED;
		}
	} else {
		t.state = ADDON_NONE;
	}
}

/** GUI1 support class handling the button used to display add-on descriptions. */
class description_display_action : public gui::dialog_button_action
{
	display& disp_;
	std::vector<std::string> display_ids_;
	addons_list addons_;
	gui::filter_textbox* filter_;

public:
	description_display_action(display& disp, const std::vector<std::string>& display_ids, const addons_list& addons, gui::filter_textbox* filter)
		: disp_(disp) , display_ids_(display_ids), addons_(addons), filter_(filter)
	{}

	virtual gui::dialog_button_action::RESULT button_pressed(int filter_choice)
	{
		assert(filter_ != NULL);

		const int menu_selection = filter_->get_index(filter_choice);
		if(menu_selection < 0) { return gui::CONTINUE_DIALOG; }

		const size_t choice = static_cast<size_t>(menu_selection);
		if(choice < display_ids_.size()) {
			gui2::taddon_description::display(addons_[display_ids_[choice]], disp_.video());
		}

		return gui::CONTINUE_DIALOG;
	}
};

void show_addons_manager_dialog(display& disp, addons_client& client, addons_list& addons, bool updates_only, std::string& last_addon_id, bool& stay_in_ui, bool& wml_changed)
{
	stay_in_ui = false;

	// Currently installed add-ons, which we'll need to check when updating.
	// const std::vector<std::string>& installed_addons_list = installed_addons();

	// Add-ons available for publishing in the remote
	// (i.e. we have .pbl files for them).
	const std::vector<std::string>& can_publish_ids = available_addons();

	// Add-ons available for deleting in the remote
	// (i.e. already published, and we have .pbl files for them).
	std::vector<std::string> can_delete_ids;

	// Status tracking information about add-ons.
	std::map<std::string, addon_tracking_info> tracking;

	// UI markup.
	const std::string sep(1, COLUMN_SEPARATOR);
	
	// List and list filter control contents.
	std::vector<std::string> options, filter_options;
	std::string header;

	// The add-on ids actually available for the user to pick from in the UI.
	std::vector<std::string> option_ids;

	// UI sorting detail.
	std::vector<int> sort_sizes;

	header = HEADING_PREFIX + sep + _("Name") + sep;
	if(updates_only) {
		header += _("Old Version") + sep + _("New Version") + sep;
	} else {
		header += _("Version") + sep;
	}
	header += _("Author") + sep;
	// The Type and Downloads columns aren't displayed when updating because of
	// display space constraints. Presumably, the user doesn't care about that
	// information since the add-on is already installed.
	if(!updates_only) {
		header += _("Type") + sep + _("Downloads") + sep;
	}
	header += _("Size"); // end of list header

	options.push_back(header);
	filter_options.push_back(header);

	//
	// Prepare the add-ons list for display and get status
	// information.
	//

	foreach(const addons_list::value_type& entry, addons) {
		const addon_info& addon = entry.second;
		get_addon_tracking_info(addon, tracking[addon.id]);

		if(updates_only && tracking[addon.id].state != ADDON_INSTALLED_UPGRADABLE) {
			continue;
		}

		option_ids.push_back(addon.id);

		if(tracking[addon.id].can_publish) {
			can_delete_ids.push_back(addon.id);
		}

		const std::string& display_size = size_display_string(addon.size);
		const std::string& display_type = get_translatable_addon_type(addon.type);
		const std::string& display_down = str_cast(addon.downloads);
		const std::string& display_icon = addon.display_icon();

		std::string display_version = addon.version.str();
		std::string display_old_version = tracking[addon.id].installed_version;
		std::string display_title = auto_addon_title(addon);
		std::string display_author = addon.author;

		// Add negative sizes to reverse the sort order.
		sort_sizes.push_back(-addon.size);

		std::string row;

		// First we enter information that's used only for filtering.
		// This includes the description, which we cannot display
		// as a normal list row due to space constraints.

		row = display_title + sep;
		if(updates_only) {
			row += display_old_version + sep;
		}
		row += display_version + sep + display_author + sep +
			display_type + sep + display_down + sep +
			display_size + sep + addon.description;

		filter_options.push_back(row);

		// Now we enter information for list row display.
		// Three fields are truncated to accomodate for GUI1's limitations.

		utils::truncate_as_wstring(display_title, 20);
		utils::truncate_as_wstring(display_author, 16);
		// Versions are too important in upgrades mode, so don't
		// truncate them then.
		if(!updates_only) {
			utils::truncate_as_wstring(display_version, 12);
		}

		// NOTE: NULL_MARKUP used to escape abuse of formatting chars in add-on titles
		row = IMAGE_PREFIX + display_icon + sep + font::NULL_MARKUP + display_title + sep;
		if(updates_only) {
			row += display_old_version + sep;
		}
		row += display_version + sep + display_author + sep;
		if(!updates_only) {
			row += display_type + sep + display_down + sep;
		}
		row += display_size;

		options.push_back(row);
	}

	if(!updates_only) {
		// Enter publish and remote deletion options
		foreach(const std::string& pub_id, can_publish_ids) {
			static const std::string publish_icon = "icons/icon-addon-publish.png";
			const std::string text = _("Publish add-on: ") + make_addon_title(pub_id);
			options.push_back(IMAGE_PREFIX + publish_icon + COLUMN_SEPARATOR + font::GOOD_TEXT + text);
			filter_options.push_back(text);
		}
		foreach(const std::string& del_id, can_delete_ids) {
			static const std::string delete_icon = "icons/icon-addon-delete.png";
			const std::string text = _("Delete add-on: ") + make_addon_title(del_id);
			options.push_back(IMAGE_PREFIX + delete_icon + COLUMN_SEPARATOR + font::BAD_TEXT + text);
			filter_options.push_back(text);
		}
	}

	if(option_ids.empty()) {
		if(!updates_only && can_publish_ids.empty() && can_delete_ids.empty()) {
			gui2::show_error_message(disp.video(), _("There are no add-ons available for download from this server."));
		} else if(updates_only) {
			gui2::show_transient_message(disp.video(), _("Add-ons Manager"), _("All add-ons are up to date."));
		}

		return;
	}

	int result;
	// Magic constant assigned to the Update All button as its return value.
	static const int update_all_value = -255;

	/* do */ {
		//
		// Set-up the actual GUI1 dialog and its children.
		//

		const std::string dlg_title(!updates_only ? _("Get Add-ons") : _("Update Add-ons"));

		gui::dialog dlg(disp, dlg_title, "", gui::OK_CANCEL);

		gui::menu::basic_sorter sorter;
		sorter.set_alpha_sort(1).set_alpha_sort(2).set_alpha_sort(3).set_alpha_sort(4);
		if(!updates_only) {
			sorter.set_numeric_sort(5).set_position_sort(6, sort_sizes);
		} else {
			sorter.set_position_sort(5, sort_sizes);
		}

		gui::menu::imgsel_style addon_style(gui::menu::bluebg_style);
		addon_style.scale_images(font::relative_size(72), font::relative_size(72));

		gui::menu* addons_list_menu = new gui::menu(disp.video(), options, false, -1,
			gui::dialog::max_menu_width, &sorter, &addon_style, false);
		dlg.set_menu(addons_list_menu);

		gui::filter_textbox* filter_box = new gui::filter_textbox(disp.video(),
			_("Filter: "), options, filter_options, 1, dlg, 300);
		dlg.set_textbox(filter_box);

		description_display_action description_helper(disp, option_ids, addons, filter_box);
		gui::dialog_button* description_button = new gui::dialog_button(disp.video(),
			_("Description"), gui::button::TYPE_PRESS, gui::CONTINUE_DIALOG, &description_helper);
		dlg.add_button(description_button, gui::dialog::BUTTON_EXTRA);

		gui::dialog_button* update_all_button = NULL;
		if(updates_only) {
			update_all_button = new gui::dialog_button(disp.video(), _("Update All"),
				gui::button::TYPE_PRESS, update_all_value);
			dlg.add_button(update_all_button, gui::dialog::BUTTON_EXTRA);
		}

		// Focus the menu on the previous selection.
		std::vector<std::string>::iterator it = !last_addon_id.empty() ?
			std::find(option_ids.begin(), option_ids.end(), last_addon_id) :
			option_ids.end();

		if(it != option_ids.end()) {
			addons_list_menu->move_selection(std::distance(option_ids.begin(), it));
		}

		//
		// Execute the dialog.
		//
		result = filter_box->get_index(dlg.show());
	}

	const bool update_everything = updates_only && result == update_all_value;
	if(result < 0 && !update_everything) {
		// User canceled the dialog.
		return;
	}

	stay_in_ui = true;

	if(!updates_only) {
		if(result >= int(option_ids.size() + can_publish_ids.size())) {
			// Handle remote deletion.
			const std::string& id = can_delete_ids[result - int(option_ids.size() + can_publish_ids.size())];
			do_remote_addon_delete(disp.video(), client, id);
			return;
		} else if(result >= int(option_ids.size())) {
			// Handle remote publishing.
			const std::string& id = can_publish_ids[result - int(option_ids.size())];
			do_remote_addon_publish(disp.video(), client, id);
			return;
		}
	}

	std::vector<std::string> ids_to_install;
	std::vector<std::string> failed_titles;

	if(update_everything) {
		ids_to_install = option_ids;
	} else {
		assert(result >= 0 && size_t(result) < option_ids.size());
		last_addon_id = option_ids[result];
		ids_to_install.push_back(option_ids[result]);
	}

	foreach(const std::string& id, ids_to_install) {
		const addon_info& addon = addon_at(id, addons);

		if(!(do_check_before_overwriting_addon(disp.video(), id) && do_resolve_addon_dependencies(disp, client, addons, addon, wml_changed))) {
			// Just do nothing and leave.
			return;
		}

		config archive;

		if(!(
			client.download_addon(archive, addon.id, addon.title) &&
			client.install_addon(archive, addon)
		)) {
			failed_titles.push_back(addon.title);
			const std::string& server_error = client.get_last_server_error();
			if(!server_error.empty()) {
				gui2::show_error_message(disp.video(),
					std::string(_("The server responded with an error:")) + "\n" + server_error);
			}
		} else {
			wml_changed = true;
		}
	}

	const char* msg_title = NULL;
	const char* msg_text = NULL;

	if(ids_to_install.size() == 1 && failed_titles.empty()) {
		utils::string_map syms;
		syms["addon_title"] = addons[ids_to_install[0]].title;

		msg_title = !updates_only ? _("Add-on Installed") : _("Add-on Updated");
		msg_text = !updates_only ? _("The add-on '$addon_title|' has been successfully installed.") : _("The add-on '$addon_title|' has been successfully updated.");

		gui2::show_transient_message(disp.video(),
			msg_title, utils::interpolate_variables_into_string(msg_text, &syms));
	} else if(failed_titles.empty()) {
		msg_title = !updates_only ? _("Add-ons Installed") : _("Add-ons Updated");
		msg_text = !updates_only ? _("All add-ons installed successfully.") : _("All add-ons updated successfully.");

		gui2::show_transient_message(disp.video(), msg_title, msg_text);
	} else {
		msg_title = !updates_only ? _("Installation Failed") : _("Update Failed");
		msg_text = _n(
			"The following add-on could not be downloaded or installed successfully:",
			"The following add-ons could not be downloaded or installed successfully:",
			failed_titles.size());

		gui2::show_message(disp.video(), msg_title, msg_text + std::string("\n\n") + utils::join(failed_titles, "\n"), gui2::tmessage::ok_button);
	}
}
} // end anonymous namespace

bool addons_manager_ui(display& disp, const std::string& remote_address, bool show_updates_only)
{
	bool stay_in_manager_ui = false;
	bool need_wml_cache_refresh = false;
	std::string last_addon_id;

	preferences::set_campaign_server(remote_address);

	try {
		do {
			// TODO: don't create a new client instance each time we return to the UI,
			// but for that we need to make sure any pending network operations are canceled
			// whenever addons_client throws user_exit even before it gets destroyed
			addons_client client(disp, remote_address);

			addons_list addons;

			if(!get_addons_list(client, addons)) {
				gui2::show_error_message(disp.video(), _("An error occurred while downloading the add-ons list from the server."));
				return need_wml_cache_refresh;
			}

			try {
				show_addons_manager_dialog(disp, client, addons, show_updates_only, last_addon_id, stay_in_manager_ui, need_wml_cache_refresh);
			} catch(const addons_client::user_exit&) {
				// Don't do anything; just go back to the addons manager UI
				// if the user cancels a download or other network operation
				// after fetching the add-ons list above.
				LOG_AC << "operation canceled by user; returning to add-ons manager\n";
			}
		} while(stay_in_manager_ui);
	} catch(const config::error& e) {
		ERR_CFG << "config::error thrown during transaction with add-on server; \""<< e.message << "\"\n";
		gui2::show_error_message(disp.video(), _("Network communication error."));
	} catch(const network::error& e) {
		ERR_NET << "network::error thrown during transaction with add-on server; \""<< e.message << "\"\n";
		gui2::show_error_message(disp.video(), _("Remote host disconnected."));
	} catch(const network_asio::error& e) {
		ERR_NET << "network_asio::error thrown during transaction with add-on server; \""<< e.what() << "\"\n";
		gui2::show_error_message(disp.video(), _("Remote host disconnected."));
	} catch(const io_exception& e) {
		ERR_FS << "io_exception thrown while installing an addon; \"" << e.what() << "\"\n";
		gui2::show_error_message(disp.video(), _("A problem occurred when trying to create the files necessary to install this add-on."));
	} catch(twml_exception& e) {
		e.show(disp);
	} catch(const addons_client::user_exit&) {
		LOG_AC << "initial connection canceled by user\n";
	} catch(const addons_client::invalid_server_address&) {
		gui2::show_error_message(disp.video(), _("The add-ons server address specified is not valid."));
	}

	return need_wml_cache_refresh;
}
