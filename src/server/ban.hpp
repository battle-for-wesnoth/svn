/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef SERVER_GAME_HPP_INCLUDED
#define SERVER_GAME_HPP_INCLUDED
#include "game_errors.hpp"

#include <set>
#include <map>
#include <list>
#include <queue>
#include <ctime>

#include <boost/shared_ptr.hpp>

class config;

namespace wesnothd {

	class banned;

	std::ostream& operator<<(std::ostream& o, const banned& n);

	typedef boost::shared_ptr<banned> banned_ptr;

	/** We want to move the lowest value to the top. */
	struct banned_compare {
		bool operator()(const banned_ptr& a, const banned_ptr& b) const;
	};

	class subnet_compare_setter;
	struct banned_compare_subnet {
		bool operator()(const banned_ptr& a, const banned_ptr& b) const;
		private:
		static void set_use_subnet_mask(bool);
		bool less(const banned_ptr& a, const banned_ptr& b) const;
		bool less_with_subnet(const banned_ptr& a, const banned_ptr& b) const;
		typedef bool (banned_compare_subnet::*compare_fn)(const banned_ptr& a, const banned_ptr& b) const;
		static compare_fn active_;
		friend class subnet_compare_setter;
	};

	struct subnet_compare_setter {
		subnet_compare_setter();
		~subnet_compare_setter();
	};

	typedef std::set<banned_ptr,banned_compare_subnet > ban_set;
	typedef std::list<banned_ptr> deleted_ban_list;
	typedef std::priority_queue<banned_ptr,std::vector<banned_ptr>, banned_compare> ban_time_queue;
	typedef std::map<std::string, size_t> default_ban_times;


	class banned {
		unsigned int ip_;
		unsigned int mask_;
		std::string ip_text_;
		time_t end_time_;
		time_t start_time_;
		std::string reason_;
		std::string who_banned_;
		std::string group_;
		std::string nick_;
		static const std::string who_banned_default_;
		typedef std::pair<unsigned int, unsigned int> ip_mask;
		
		ip_mask parse_ip(const std::string&) const;
		
		banned(const std::string& ip);

	public:
		banned(const std::string& ip, const time_t end_time, const std::string& reason, const std::string& who_banned=who_banned_default_, const std::string& group="", const std::string& nick="");
		banned(const config&);

		void read(const config&);
		void write(config&) const;

		time_t get_end_time() const
		{ return end_time_;	}

		std::string get_human_end_time() const;
		std::string get_human_start_time() const;
		static std::string get_human_time(const time_t&);

		std::string get_reason() const
		{ return reason_; }

		std::string get_ip() const
		{ return ip_text_; }
		std::string get_group() const
		{ return group_; }

		std::string get_who_banned() const
		{ return who_banned_; }

		std::string get_nick() const
		{ return nick_; }

		bool match_group(const std::string& group) const
		{ return group_ == group; }

		unsigned int get_mask_ip(unsigned int) const;
		unsigned int get_int_ip() const
		{ return ip_; }

		unsigned int mask() const 
		{ return mask_; }

		static banned_ptr create_dummy(const std::string& ip);

		bool operator>(const banned& b) const;

		struct error : public ::game::error {
			error(const std::string& message) : ::game::error(message) {}
		};
	};

	class ban_manager
	{

		ban_set bans_;
		deleted_ban_list deleted_bans_;
		ban_time_queue time_queue_;
		default_ban_times ban_times_;
		std::string ban_help_;
		std::string filename_;
		bool dirty_;

		bool is_digit(const char& c) const
		{ return c >= '0' && c <= '9'; }
		size_t to_digit(const char& c) const
		{ return c - '0'; }

		void init_ban_help();
	public:
		ban_manager();
		~ban_manager();
		
		void read();
		void write();

		time_t parse_time(std::string time_in) const;

		std::string ban(const std::string&, const time_t&, const std::string&, const std::string&, const std::string&, const std::string& = "");
		void unban(std::ostringstream& os, const std::string& ip);
		void unban_group(std::ostringstream& os, const std::string& group);


		void check_ban_times(time_t time_now);

		void list_deleted_bans(std::ostringstream& out) const;
		void list_bans(std::ostringstream& out) const;

		bool is_ip_banned(std::string ip) const;
		
		const std::string& get_ban_help() const
		{ return ban_help_; }	

		void load_config(const config&);

	};
}

#endif
