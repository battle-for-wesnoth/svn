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

/**
 * this file implements game config caching
 * to speed up startup 
 ***/

#ifndef CONFIG_CACHE_HPP_INCLUDED
#define CONFIG_CACHE_HPP_INCLUDED

#include <map>
#include <list>
#include <boost/utility.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "config.hpp"
#include "serialization/preprocessor.hpp"

namespace game_config {


	/**
	 * Used to set and unset scoped defines to preproc_map
	 * This is prefered form to set defines that aren't global
	 **/
	template <class T>
		class scoped_preproc_define_internal : private boost::noncopyable {
			// Protected to let test code access
			protected:
				std::string name_;
				bool add_;
				std::string path_;
			public:
				/**
				 * adds normal preproc define
				 * @params name of preproc define to add
				 * @aprams true if we should add this
				 **/
				scoped_preproc_define_internal(const std::string& name, bool add = true) : name_(name), add_(add), path_()
				{
					if (add_)
						T::instance().add_define(name_);
				}

				/**
				 * adds path specific preproc define
				 **/
				scoped_preproc_define_internal(const std::string& path, const std::string& name, bool add = true) : name_(name), add_(add), path_(path)
				{
					if (add_)
						T::instance().add_path_define(path_, name_);
				}

				/**
				 * This removes preproc define from cacher
				 **/
				~scoped_preproc_define_internal()
				{
					if(add_) 
					{
						if (path_.empty())
							T::instance().remove_define(name_);
						else
							T::instance().remove_path_define(path_, name_);
					}
				}
		};

	class config_cache;

	typedef scoped_preproc_define_internal<config_cache> scoped_preproc_define;
	typedef boost::shared_ptr<scoped_preproc_define> scoped_preproc_define_ptr;
	typedef std::list<scoped_preproc_define_ptr> scoped_preproc_define_list;
	/**
	 * Singleton class to manage game config file caching.
	 * It uses paths to config files as key to find correct cache
	 * @TODO: Make smarter filetree checksum caching so only required parts
	 * 		  of tree are checked at startup. Trees are overlapping so have
	 * 		  to split trees to subtrees to only do check once per file.
	 * @TODO: Make cache system easily allow validation of in memory cache objects
	 * 		  using hash checksum of preproc_map.
	 **/
	class config_cache : private boost::noncopyable {
		public:
		typedef std::multimap<std::string,std::string> path_define_map;
		private:

		static config_cache cache_;

		bool force_valid_cache_, use_cache_, not_valid_cache_;
		preproc_map defines_map_;
		path_define_map path_defines_;

		void read_file(const std::string& file, config& cfg);
		void write_file(std::string file, const config& cfg);
		void write_file(std::string file, const preproc_map&);

		void read_cache(const std::string& path, config& cfg);

		void read_configs(const std::string& path, config& cfg, preproc_map& defines);
		void load_configs(const std::string& path, config& cfg);
		void read_defines_queue();
		void read_defines_file(const std::string& path);

		preproc_map& make_copy_map();
		void add_defines_map_diff(preproc_map&);

		scoped_preproc_define_ptr create_path_preproc_define(const path_define_map::value_type&);

		// Protected to let test code access
		protected:
		config_cache();

		void set_force_not_valid_cache(bool);



		public:
		/**
		 * Get reference to the singleton object
		 **/
		static config_cache& instance();

		const preproc_map& get_preproc_map() const;
		/**
		 * get config object from given path
		 * @param path which to load. Should be _main.cfg.
		 * @param config object that is writen to, It should be empty
		 * 	      because there is no quarentee how filled in config is handled
		 **/
		void get_config(const std::string& path, config& cfg);
		/**
		 * get config_ptr from given path
		 * @return shread_ptr config object
		 * @param config object that is writen to
		 **/
		config_ptr get_config(const std::string& path);

		/**
		 * Clear stored defines map to default values
		 **/
		void clear_defines();
		/**
		 * Add a entry to preproc defines map
		 **/
		void add_define(const std::string& define);
		/**
		 * Remove a entry to preproc defines map
		 **/
		void remove_define(const std::string& define);
		/**
		 * Add a path specific entry to preproc defines map
		 **/
		void add_path_define(const std::string& path, const std::string& define);
		/**
		 * Remove a path specific entry to preproc defines map
		 **/
		void remove_path_define(const std::string& path, const std::string& define);

		/**
		 * Enable/disable caching
		 **/
		void set_use_cache(bool use);
		/**
		 * Enable/disable cache validation
		 **/
		void set_force_valid_cache(bool force);
		/**
		 * Force cache checksum validation.
		 **/
		void recheck_filetree_checksum();
		
	};

	struct add_define_from_file;
	class fake_transaction;
	/**
	 * Used to share macros between cache objects
	 * You have to create transaction object to load all
	 * macros to memory and share them subsequent cache loads.
	 * If transaction is locked all stored macros are still
	 * available but new macros aren't added.
	 **/
	class config_cache_transaction  : private boost::noncopyable {
		public:
		config_cache_transaction();
		~config_cache_transaction();
		/**
		 * Lock the transaction so no more macros are added
		 **/
		void lock();

		enum state { FREE,
			NEW,
			ACTIVE,
			LOCKED
		};
		typedef std::vector<std::string> filenames;

		/**
		 * Used to let std::for_each insert new defines to active_map
		 * map to active 
		 **/
		void insert_to_active(const preproc_map::value_type& def);
		
		private:
		static state state_;
		static config_cache_transaction* active_;
		filenames define_filenames_;
		preproc_map active_map_;

		static state get_state()
		{return state_; }
		static bool is_active()
		{return active_ != 0;}
		static config_cache_transaction& instance()
		{ assert(active_); return *active_; }
		friend class config_cache;
		friend class add_define_from_file;
		friend class fake_transaction;
		const filenames& get_define_files() const;
		void add_define_file(const std::string&);
		preproc_map& get_active_map(const preproc_map&);
		void add_defines_map_diff(preproc_map& defines_map);
	};

	struct add_define_from_file {
		void operator()(const config::all_children_iterator::value_type& value) const
		{
			config_cache_transaction::instance().insert_to_active(
					preproc_define::read_pair(value.second));
		}
	};


	/**
	 * Holds a fake cache transaction if no real one is used
	 **/
	class fake_transaction : private boost::noncopyable {
		typedef boost::scoped_ptr<config_cache_transaction> value_type;
		value_type trans_;
		fake_transaction() : trans_()
		{
			if (!config_cache_transaction::is_active())
			{
				trans_.reset(new config_cache_transaction());
			}
		}
		friend class config_cache;
	};
}
#endif
