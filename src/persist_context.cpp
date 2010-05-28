/* $Id$ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "filesystem.hpp"
#include "util.hpp"
#include "log.hpp"
#include "persist_context.hpp"
#include "resources.hpp"
#include "serialization/binary_or_text.hpp"

static bool is_valid_namespace(const std::string &name_space)
{
	return (name_space.find_first_not_of("!'(),-0123456789;ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_") > name_space.length());
}

static std::string get_persist_cfg_name(const std::string &name_space)
{
	// TODO: get namespace base name
	if (is_valid_namespace(name_space))
		return (get_dir(get_user_data_dir() + "/persist/") + name_space + ".cfg");
	else
		return "";
}

static bool load_persist_data(const std::string &name_space, config &cfg, const bool create_if_missing = true)
{
	bool success = false;
	std::string cfg_dir = get_dir(get_user_data_dir() + "/persist");
	create_directory_if_missing(cfg_dir);

	// TODO: Support nested namespaces
	std::string cfg_name = get_persist_cfg_name(name_space);
	if (!cfg_name.empty()) {
		scoped_istream file_stream = istream_file(cfg_name);
		if (file_stream->fail()) {
			if (create_if_missing) {
				// TODO: create proper file structure.
				success = true;
			}
		} else {
			try {
				detect_format_and_read(cfg,*file_stream);
				success = true;
			} catch (config::error &err) {
				LOG_SAVE << err.message;
				success = false;
			}
		}
	}
	return success;
}

static bool save_persist_data(std::string &name_space, config &cfg)
{
	bool success = false;

	// TODO: Support nested namespaces
	std::string cfg_name = get_persist_cfg_name(name_space);
	if (!cfg_name.empty()) {
		if (cfg.empty()) {
			success = delete_directory(cfg_name);
		} else {
			scoped_ostream out = ostream_file(cfg_name);
			if (!out->fail())
			{
				config_writer writer(*out,false);
				try {
					writer.write(cfg);
					success = true;
				} catch(config::error &err) {
					LOG_SAVE << err.message;
					success = false;
				}
			}
		}
	}
	return success;
}

config pack_scalar(const std::string &name, const t_string &val)
{
	config cfg;
	cfg[name] = val;
	return cfg;
}

// TODO: support nested namespaces
persist_context::persist_context(const std::string &name_space) : namespace_(name_space), cfg_(), valid_(is_valid_namespace(namespace_.substr(0,name_space.find_first_of(".")))) {}

bool persist_context::clear_var(std::string &global)
{
	if (cfg_.empty()) {
		load_persist_data(namespace_,cfg_);
		if (!cfg_.child("variables"))
			cfg_.add_child("variables");
	}

	// TODO: get config's variables.
	bool exists = cfg_.child("variables").has_attribute(global);
	bool ret;
	if (!exists) {
		if (cfg_.child("variables").child(global)) {
			exists = true;
			std::string::iterator index_start = std::find(global.begin(),global.end(),'[');
			if (index_start != global.end())
			{
				const std::string::iterator index_end = std::find(global.begin(),global.end(),']');
				const std::string index_str(index_start+1,index_end);
				size_t index = static_cast<size_t>(lexical_cast_default<int>(index_str));
				cfg_.child("variables").remove_child(global,index);
			} else {
				cfg_.child("variables").clear_children(global);
			}
		}
	}
	if (exists) {
		cfg_.child("variables").remove_attribute(global);
		if (cfg_.child("variables").empty()) {
			cfg_.clear_children("variables");
			cfg_.remove_attribute("variables");
		}
		ret = save_persist_data(namespace_,cfg_);
	} else {
		ret = exists;
	}
	return ret;
}

config persist_context::get_var(const std::string &global)
{
	config ret;
	if (cfg_.empty()) {
		load_persist_data(namespace_,cfg_,false);
		if (!cfg_.child("variables"))
			cfg_.add_child("variables");
	}
	//TODO: get config's [variables]
	if (cfg_.child("variables").child(global)) {
		ret.add_child(global,cfg_.child("variables").child(global));
	} else {
		ret = pack_scalar(global,cfg_.child("variables")[global]);
	}
	return ret;
}

bool persist_context::set_var(const std::string &global,const config &val)
{
	if (cfg_.empty()) {
		load_persist_data(namespace_,cfg_);
		if (!cfg_.child("variables"))
			cfg_.add_child("variables");
	}

	//TODO: get config's [variables]
	if (val.has_attribute(global)) {
		cfg_.child("variables")[global] = val[global];
	} else {
		cfg_.child("variables").add_child(global,val.child(global));
	}
	return save_persist_data(namespace_,cfg_);
}
