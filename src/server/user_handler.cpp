#include "user_handler.hpp"
#include <cstdlib>
#include <ctime>
#include <sstream> 


bool user_handler::send_mail(const std::string& to_user,
		const std::string& subject, const std::string& message) {

	//If this user is registerd at all
	if(!user_exists(to_user)) {
		throw error("Could not send email. No user with the name '" + to_user + "' exists.");
	}

	// If this user did not provide an email
	if(get_mail(to_user) == "") {
		throw error("Could not send email. The email address of the user '" + to_user + "' is empty.");
	}

	if(!mailer_) {
		throw user_handler::error("This server is configured not to send email.");
	}

	if(!(mailer_->send_mail(get_mail(to_user), subject, message))) {
		throw user_handler::error("There was an error sending the email.");
	}

	return true;
}

void user_handler::init_mailer(config* c) {
	if(c) {
		mailer_ = new mailer(*c);
	} else {
		mailer_ = NULL;
	}
}

std::string user_handler::create_salt(int length) {
	srand((unsigned)time(NULL));

	std::stringstream ss;

	for(int i = 0; i < length; i++) {
		ss << (rand() % 10);
	}

	return  ss.str();
}
