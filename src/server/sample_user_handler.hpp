/* $Id$ */
/*
   Copyright (C) 2008 by Thomas Baumhauer <thomas.baumhauer@NOSPAMgmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef SAMPLE_USER_HANDLER_HPP_INCLUDED
#define SAMPLE_USER_HANDLER_HPP_INCLUDED

#include "user_handler.hpp"

/**
 * @class An exmple how to implement user_handler to demonstrate the interface
 * If you use this on anything real you are insane
 */

// Class is currently broken due to the hardcoding of the phpbb hashing algorithms
// @todo fix it

class suh : public user_handler {
	public:
		suh(config c);

		void add_user(const std::string& name, const std::string& mail, const std::string& password);
		void remove_user(const std::string& name);

		void clean_up();

		bool login(const std::string& name, const std::string& password);
		void user_logged_in(const std::string& name);

		void password_reminder(const std::string& name);

		bool user_exists(const std::string& name);

		std::string user_info(const std::string& name);

		struct user {
			user() : registrationdate(time(NULL)) {}
			std::string password;
			std::string realname;
			std::string mail;
			time_t lastlogin;
			time_t registrationdate;
		};

		void set_user_detail(const std::string& user, const std::string& detail, const std::string& value);
		std::string get_valid_details();

	private:
		std::string get_mail(const std::string& user);
		std::string get_password(const std::string& user);
		std::string get_realname(const std::string& user) ;
		time_t get_lastlogin(const std::string& user);
		time_t get_registrationdate(const std::string& user);

		void check_name(const std::string& name);
		void check_mail(const std::string& mail);
		void check_password(const std::string& password);
		void check_realname(const std::string& realname);

		void set_mail(const std::string& user, const std::string& mail);
		void set_password(const std::string& user, const std::string& password);
		void set_realname(const std::string& user, const std::string& realname);

		void set_lastlogin(const std::string& user, const time_t& lastlogin);
		void set_registrationdate(const std::string& user, const time_t& registrationdate);

		int user_expiration_;

		std::map <std::string,user> users_;
		std::vector<std::string> users();
};

#endif //SAMPLE_USER_HANDLER_HPP_INCLUDED
