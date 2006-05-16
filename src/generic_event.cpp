/*
   Copyright (C) 2006 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "generic_event.hpp"

#include <algorithm>

namespace events{
	observer::~observer() {
	}
	generic_event::generic_event(std::string name){
		name_ = name;
		change_handler_ = false;
		notify_active_ = false;
	}
	generic_event::~generic_event() {
	}
	bool generic_event::attach_handler(observer* obs){
		bool handler_attached = false;

		//make sure observers are not notified right now
		if (!notify_active_){
			change_handler_ = true;
			try{
				std::vector<observer*>::const_iterator it = std::find(observers_.begin(), observers_.end(), obs);
				if (it != observers_.end()){
					handler_attached = false;
				}
				else{
					observers_.push_back(obs);
					handler_attached = true;
				}
			}
			catch (std::exception&){
				change_handler_ = false;
				throw;
			}
			change_handler_ = false;
		}

		return handler_attached;
	}

	bool generic_event::detach_handler(observer* obs){
		bool handler_detached = false;

		//make sure observers are not notified right now
		if (!notify_active_){
			change_handler_ = true;
			try{
				std::vector<observer*>::iterator it = std::find(observers_.begin(), observers_.end(), obs);
				if (it == observers_.end()){
					handler_detached = false;
				}
				else{
					observers_.erase(it);
					handler_detached = true;
				}
			}
			catch (std::exception&){
				change_handler_ = false;
				throw;
			}
			change_handler_ = false;
		}

		return handler_detached;
	}

	void generic_event::notify_observers(){
		if (!change_handler_){
			notify_active_ = true;
			try{
				for (std::vector<observer*>::const_iterator it = observers_.begin(); 
					it != observers_.end(); it++){
					(*it)->handle_generic_event(name_);
				}
			}
			catch (std::exception&){
				//reset the flag if event handlers throw exceptions and don't catch them
				notify_active_ = false;
				throw;
			}
			notify_active_ = false;
		}
	}

} //namespace events
