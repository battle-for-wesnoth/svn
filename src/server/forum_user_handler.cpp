#ifdef HAVE_MYSQLPP

#include "forum_user_handler.hpp"

#include <stdlib.h> 
#include <sstream>

fuh::fuh(config c) {
	db_name_ = c["db_name"];
	db_host_ = c["db_host"];
	db_user_ = c["db_user"];
	db_password_ = c["db_password"];

	// Connect to the database
	if(!(db_interface_.connect(db_name_.c_str(), db_host_.c_str(), db_user_.c_str(), db_password_.c_str()))) {
		std::cerr << "FUH: ERROR: Could not connect to database" << std::endl;
	}
}

void fuh::add_user(const std::string& name, const std::string& mail, const std::string& password) {
	throw error("For now please register at http://forum.wesnoth.org");
}

void fuh::remove_user(const std::string& name) {
	throw error("'Dropping your nick is currently impossible");
}

void fuh::set_user_detail(const std::string& user, const std::string& detail, const std::string& value) {
	throw error("For now this is a 'read-only' user_handler");
}

std::string fuh::get_valid_details() {
	return "For now this is a 'read-only' user_handler";
}

mysqlpp::StoreQueryResult fuh::db_query(const std::string& sql) {

	//Check if we are connected
	if(!(db_interface_.connected())) {
		//Try to reconnect
		if(!(db_interface_.connect(db_name_.c_str(), db_host_.c_str(), db_user_.c_str(), db_password_.c_str()))) {
			//If we still are not connect throw an error
			throw error("Not connected to database");
		}
	}

	mysqlpp::Query query = db_interface_.query(sql);
	return query.store();
}

std::string fuh::db_query_to_string(const std::string& query) {
	return std::string(db_query(query)[0][0]);
}

// The hashing code is basically taken from forum_auth.cpp
bool fuh::login(const std::string& name, const std::string& password, const std::string& seed) {

	// Set an alphabet-like string for use in encrytpion algorithm	 
	std::string itoa64("./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	// Retrive users' password as hash

	std::string hash;

	try {
		hash = get_hash(name);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return false;
	}

	// Check hash prefix, if different than $H$ hash is invalid
	if(hash.substr(0,3) != "$H$") {
		std::cerr << "ERROR: Invalid hash prefix for user '" << name << "'" << std::endl;
		return false;
	}

	std::string valid_hash = hash.substr(12,34) + seed;
	MD5 md5_worker;
	md5_worker.update((unsigned char *)valid_hash.c_str(), valid_hash.size());
	md5_worker.finalize();
	valid_hash = std::string(md5_worker.hex_digest());

	if(password == valid_hash) return true;

	return false;
}

std::string fuh::create_pepper(const std::string& name, int index) {

	// Some doulbe security, this should never be neeeded
	if(!(user_exists(name))) {
		return "";
	}

	// Set an alphabet-like string for use in encrytpion algorithm	 
	std::string itoa64("./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::string hash;

	try {
		hash = get_hash(name);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return "";
	}

	// Check hash prefix, if different than $H$ hash is invalid
	if(hash.substr(0,3) != "$H$")
		return "";

	if(index == 0) {
		// Start of the encryption, get the position of first nonidentifier character in extended alphabet
		int hash_seed = itoa64.find_first_of(hash[3]);

		// If position is lower than 8 or higher than 32 hash is also invalid
		if(hash_seed < 7 || hash_seed > 30)
			return "";

		// Set the number of encryption passes as 2^position
		hash_seed = 1 << hash_seed;

		std::stringstream ss;
		ss << hash_seed;
		return ss.str();

	} else if (index == 1) {
		// Create salt for mixing with the hash
		return hash.substr(4,8);

	} else {
		return "";
	}

}

bool fuh::user_exists(const std::string& name) {

	// Make a test query for this username
	std::string sql("SELECT username FROM phpbb_users WHERE username='");
	sql.append(name);
	sql.append("'");

	try {
		return db_query(sql).num_rows() > 0;
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		// If the database is down just let all usernames log in
		return false;
	}
}

void fuh::user_logged_in(const std::string& name) {
	set_lastlogin(name, time(NULL));
}


void fuh::clean_up() {

}

void fuh::set_lastlogin(const std::string& user, const time_t& lastlogin) {

	// Disabled for now

	/*
	std::stringstream ss;
	ss << lastlogin;

	std::string sql("UPDATE phpbb_users set user_lastvisit='");
	sql.append(ss.str());
	sql.append("' where username='");
	sql.append(user);
	sql.append("'");

	try {
	db_query(sql);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
	}*/
}

std::string fuh::get_hash(const std::string& user) {
	std::string sql("SELECT user_password FROM phpbb_users WHERE username='");
	sql.append(user);
	sql.append("'");

	try {
		return db_query_to_string(sql);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return time_t(0);
	}
}

std::string fuh::get_mail(const std::string& user) {
	std::string sql("SELECT user_email FROM phpbb_users WHERE username='");
	sql.append(user);
	sql.append("'");

	try {
		return db_query_to_string(sql);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return time_t(0);
	}
}

time_t fuh::get_lastlogin(const std::string& user) {
	std::string sql("SELECT user_lastvisit FROM phpbb_users WHERE username='");
	sql.append(user);
	sql.append("'");

	try {
		int time_int = atoi(db_query_to_string(sql).c_str());
		return time_t(time_int);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return time_t(0);
	}
}

time_t fuh::get_registrationdate(const std::string& user) {
	std::string sql("SELECT user_regdate FROM phpbb_users WHERE username='");
	sql.append(user);
	sql.append("'");

	try {
		int time_int = atoi(db_query_to_string(sql).c_str());
		return time_t(time_int);
	} catch (error e) {
		std::cerr << "FUH: ERROR: " << e.message << std::endl;
		return time_t(0);
	}
}

void fuh::password_reminder(const std::string& name) {
	throw error("For now please use the password recovery"
		"function provided at http://forum.wesnoth.org");
}

std::string fuh::user_info(const std::string& name) {
	if(!user_exists(name)) {
		throw error("No user with the name '" + name + "' exists.");
	}

	time_t reg_date = get_registrationdate(name);
	time_t ll_date = get_lastlogin(name);

	std::string reg_string = ctime(&reg_date);
	std::string ll_string = ctime(&ll_date);

	std::stringstream info;
	info << "Name: " << name << "\n"
		 << "Registered: " << reg_string
		 << "Last login: " << ll_string << create_pepper(name, 0) << "\n" << create_pepper(name,1);
	return info.str();
}

#endif //HAVE_MYSQLPP
