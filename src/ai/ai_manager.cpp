/* $Id$ */
/*
   Copyright (C) 2009 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * Managing the AI lifecycle and interface for the rest of Wesnoth
 * @file ai/ai_manager.cpp
 */

#include "ai.hpp"
#include "ai2.hpp"
#include "ai_configuration.hpp"
#include "ai_manager.hpp"
#include "ai_dfool.hpp"
#include "contexts.hpp"
#include "formula_ai.hpp"
#include "../game_events.hpp"
#include "../game_preferences.hpp"
#include "../log.hpp"
#include "../replay.hpp"
#include "../serialization/string_utils.hpp"
#include "../statistics.hpp"

#include <map>
#include <stack>
#include <vector>

const std::string ai_manager::AI_TYPE_SAMPLE_AI = "sample_ai";
const std::string ai_manager::AI_TYPE_IDLE_AI = "idle_ai";
const std::string ai_manager::AI_TYPE_FORMULA_AI = "formula_ai";
const std::string ai_manager::AI_TYPE_DFOOL_AI = "dfool_ai";
const std::string ai_manager::AI_TYPE_AI2 = "ai2";
const std::string ai_manager::AI_TYPE_DEFAULT = "default";


static lg::log_domain log_ai_manager("ai/manager");
#define DBG_AI_MANAGER LOG_STREAM(debug, log_ai_manager)
#define LOG_AI_MANAGER LOG_STREAM(info, log_ai_manager)
#define ERR_AI_MANAGER LOG_STREAM(err, log_ai_manager)

ai_holder::ai_holder( int side, const std::string& ai_algorithm_type )
	: ai_(NULL), side_context_(NULL), readonly_context_(NULL), readwrite_context_(NULL), ai_algorithm_type_(ai_algorithm_type), ai_effective_parameters_(),  ai_global_parameters_(), ai_memory_(), ai_parameters_(), side_(side)
{
	DBG_AI_MANAGER << describe_ai() << "Preparing new AI holder" << std::endl;
}


void ai_holder::init( int side )
{
	LOG_AI_MANAGER << describe_ai() << "Preparing to create new managed master AI" << std::endl;
	if (side_context_ == NULL) {
		side_context_ = new ai::side_context_impl(side);
	} else {
		side_context_->set_side(side);
	}
	if (readonly_context_ == NULL){
		readonly_context_ = new ai::readonly_context_impl(*side_context_);
	}
	if (readwrite_context_ == NULL){
		readwrite_context_ = new ai::readwrite_context_impl(*readonly_context_);
	}
	this->ai_ = create_ai(side);
	if (this->ai_ == NULL) {
		ERR_AI_MANAGER << describe_ai()<<"AI lazy initialization error!" << std::endl;
	}

}


ai_holder::~ai_holder()
{
	if (this->ai_ != NULL) {
		LOG_AI_MANAGER << describe_ai() << "Managed AI will be deleted" << std::endl;
	}
	delete this->ai_;
	delete this->readwrite_context_;
	delete this->readonly_context_;
	delete this->side_context_;
}


ai_interface& ai_holder::get_ai_ref( int side )
{
	if (this->ai_ == NULL) {
		this->init(side);
	}
	assert(this->ai_ != NULL);

	return *this->ai_;
}

ai_interface& ai_holder::get_ai_ref()
{
	return get_ai_ref(this->side_);
}


const std::string& ai_holder::get_ai_algorithm_type() const
{
	return this->ai_algorithm_type_;
}


config& ai_holder::get_ai_memory()
{
	return this->ai_memory_;
}


std::vector<config>& ai_holder::get_ai_parameters()
{
	return this->ai_parameters_;
}


void ai_holder::set_ai_parameters( const std::vector<config>& ai_parameters )
{
	this->ai_parameters_ = ai_parameters;
	DBG_AI_MANAGER << describe_ai() << "AI parameters are set." << std::endl;
}


config& ai_holder::get_ai_effective_parameters()
{
	return this->ai_effective_parameters_;
}


void ai_holder::set_ai_effective_parameters( const config& ai_effective_parameters )
{
	this->ai_effective_parameters_ = ai_effective_parameters;
	DBG_AI_MANAGER << describe_ai() << "AI effective parameters are set." << std::endl;
}


config& ai_holder::get_ai_global_parameters()
{
	return this->ai_global_parameters_;
}


void ai_holder::set_ai_global_parameters( const config& ai_global_parameters )
{
	this->ai_global_parameters_ = ai_global_parameters;
	DBG_AI_MANAGER << describe_ai() << "AI global parameters are set." << std::endl;
}


void ai_holder::set_ai_memory( const config& ai_memory )
{
	this->ai_memory_ = ai_memory;
	DBG_AI_MANAGER << describe_ai() << "AI memory is set." << std::endl;
}


void ai_holder::set_ai_algorithm_type( const std::string& ai_algorithm_type ){
	this->ai_algorithm_type_ = ai_algorithm_type;
	DBG_AI_MANAGER << describe_ai() << "AI algorithm type is set to '"<< ai_algorithm_type_<< "'" << std::endl;
}


const std::string ai_holder::describe_ai()
{
	std::string sidestr;
	//@todo 1.7 extract side naming to separate static function
	if (this->side_ == ai_manager::AI_TEAM_FALLBACK_AI){
		sidestr = "'fallback_side'";
	} else if (this->side_ == ai_manager::AI_TEAM_COMMAND_AI){
		sidestr = "'command_side'";
	} else {
		sidestr = lexical_cast<std::string>(this->side_);
	}
	if (this->ai_!=NULL) {
		return this->ai_->describe_self()+std::string(" for side ")+sidestr+std::string(" : ");
	} else {
		return std::string("[")+this->ai_algorithm_type_+std::string("] (not initialized) for side ")+sidestr+std::string(" : ");
	}
}

bool ai_holder::is_mandate_ok()
{
	DBG_AI_MANAGER << describe_ai() << "AI mandate is ok" << std::endl;
	return true;
}

ai_interface* ai_holder::create_ai( int side )
{
	assert (side > 0);
	assert (readwrite_context_!=NULL);
	//@note: ai_params, contexts, and ai_algorithm_type are supposed to be set before calling init(  );
	return ai_manager::create_transient_ai(ai_algorithm_type_,readwrite_context_);

}

// =======================================================================
// AI COMMAND HISTORY ITEM
// =======================================================================

ai_command_history_item::ai_command_history_item()
	: number_(0), command_()
{

}


ai_command_history_item::ai_command_history_item( int number, const std::string& command )
	: number_(number), command_(command)
{

}


ai_command_history_item::~ai_command_history_item()
{

}


int ai_command_history_item::get_number() const
{
	return this->number_;
}


void ai_command_history_item::set_number( int number )
{
	this->number_ = number;
}

const std::string& ai_command_history_item::get_command() const
{
	return this->command_;
}


void ai_command_history_item::set_command( const std::string& command )
{
	this->command_ = command;
}

// =======================================================================
// LIFECYCLE
// =======================================================================


ai_manager::ai_manager()
{

}


ai_manager::~ai_manager()
{

}


ai_manager::AI_map_of_stacks ai_manager::ai_map_;
ai_game_info *ai_manager::ai_info_;
events::generic_event ai_manager::user_interact_("ai_user_interact");
events::generic_event ai_manager::unit_recruited_("ai_unit_recruited");
events::generic_event ai_manager::unit_moved_("ai_unit_moved");
events::generic_event ai_manager::enemy_attacked_("ai_enemy_attacked");
int ai_manager::last_interact_ = 0;
int ai_manager::num_interact_ = 0;


void ai_manager::set_ai_info(const ai_game_info& i)
{
	if (ai_info_!=NULL){
		clear_ai_info();
	}
	ai_info_ = new ai_game_info(i);
}


void ai_manager::clear_ai_info(){
	if (ai_info_ != NULL){
		delete ai_info_;
		ai_info_ = NULL;
	}
}

void ai_manager::add_observer( events::observer* event_observer){
	user_interact_.attach_handler(event_observer);
	unit_recruited_.attach_handler(event_observer);
	unit_moved_.attach_handler(event_observer);
	enemy_attacked_.attach_handler(event_observer);
}

void ai_manager::remove_observer(events::observer* event_observer){
	user_interact_.detach_handler(event_observer);
	unit_recruited_.detach_handler(event_observer);
	unit_moved_.detach_handler(event_observer);
	enemy_attacked_.detach_handler(event_observer);
}

void ai_manager::raise_user_interact() {
        const int interact_time = 30;
        const int time_since_interact = SDL_GetTicks() - last_interact_;
        if(time_since_interact < interact_time) {
                return;
        }

	++num_interact_;
        user_interact_.notify_observers();

        last_interact_ = SDL_GetTicks();

}

void ai_manager::raise_unit_recruited() {
	unit_recruited_.notify_observers();
}

void ai_manager::raise_unit_moved() {
	unit_moved_.notify_observers();
}

void ai_manager::raise_enemy_attacked() {
	enemy_attacked_.notify_observers();
}


// =======================================================================
// EVALUATION
// =======================================================================

const std::string ai_manager::evaluate_command( int side, const std::string& str )
{
	//insert new command into history
	history_.push_back(ai_command_history_item(history_item_counter_++,str));

	//prune history - erase 1/2 of it if it grows too large
	if (history_.size()>MAX_HISTORY_SIZE){
		history_.erase(history_.begin(),history_.begin()+MAX_HISTORY_SIZE/2);
		LOG_AI_MANAGER << "AI MANAGER: pruned history" << std::endl;
	}

	if (!should_intercept(str)){
		ai_interface& ai = get_command_ai(side);
		return ai.evaluate(str);
	}

	return internal_evaluate_command(side,str);
}


bool ai_manager::should_intercept( const std::string& str )
{
	if (str.length()<1) {
		return false;
	}
	if (str.at(0)=='!'){
		return true;
	}
	if (str.at(0)=='?'){
		return true;
	}
	return false;

}

std::deque< ai_command_history_item > ai_manager::history_;
long ai_manager::history_item_counter_ = 1;

//this is stub code to allow testing of basic 'history', 'repeat-last-command', 'add/remove/replace ai' capabilities.
//yes, it doesn't look nice. but it is usable.
//to be refactored at earliest opportunity
//@todo 1.7 extract to separate class which will use fai or lua parser
const std::string ai_manager::internal_evaluate_command( int side, const std::string& str ){
	const int MAX_HISTORY_VISIBLE = 30;

	//repeat last command
	if (str=="!") {
			//this command should not be recorded in history
			if (!history_.empty()){
				history_.pop_back();
				history_item_counter_--;
			}

			if (history_.empty()){
				return "AI MANAGER: empty history";
			}
			return evaluate_command(side, history_.back().get_command());//no infinite loop since '!' commands are not present in history
	};
	//show last command
	if (str=="?") {
		//this command should not be recorded in history
		if (!history_.empty()){
			history_.pop_back();
			history_item_counter_--;
		}

		if (history_.empty()){
			return "AI MANAGER: History is empty";
		}

		int n = std::min<int>( MAX_HISTORY_VISIBLE, history_.size() );
		std::stringstream strstream;
		strstream << "AI MANAGER: History - last "<< n <<" commands:\n";
		std::deque< ai_command_history_item >::reverse_iterator j = history_.rbegin();

		for (int cmd_id=n; cmd_id>0; --cmd_id){
			strstream << j->get_number() << "    :" << j->get_command() << '\n';
			j++;//this is *reverse* iterator
		}

		return strstream.str();
	};


	std::vector< std::string > cmd = utils::paranthetical_split(str, ' ',"'","'");

	if (cmd.size()==3){
		//!add_ai side file
		if (cmd.at(0)=="!add_ai"){
			int side = lexical_cast<int>(cmd.at(1));
			std::string file = cmd.at(2);
			if (add_ai_for_side_from_file(side,file,false)){
				return std::string("AI MANAGER: added [")+ai_manager::get_active_ai_algorithm_type_for_side(side)+std::string("] AI for side ")+lexical_cast<std::string>(side)+std::string(" from file ")+file;
			} else {
				return std::string("AI MANAGER: failed attempt to add AI for side ")+lexical_cast<std::string>(side)+std::string(" from file ")+file;
			}
		}
		//!replace_ai side file
		if (cmd.at(0)=="!replace_ai"){
			int side = lexical_cast<int>(cmd.at(1));
			std::string file = cmd.at(2);
			if (add_ai_for_side_from_file(side,file,true)){
					return std::string("AI MANAGER: added [")+ai_manager::get_active_ai_algorithm_type_for_side(side)+std::string("] AI for side ")+lexical_cast<std::string>(side)+std::string(" from file ")+file;
			} else {
					return std::string("AI MANAGER: failed attempt to add AI for side ")+lexical_cast<std::string>(side)+std::string(" from file ")+file;
			}
		}

	} else if (cmd.size()==2){
		//!remove_ai side
		if (cmd.at(0)=="!remove_ai"){
			int side = lexical_cast<int>(cmd.at(1));
			remove_ai_for_side(side);
			return std::string("AI MANAGER: made an attempt to remove AI for side ")+lexical_cast<std::string>(side);
		}
		if (cmd.at(0)=="!"){
			//this command should not be recorded in history
			if (!history_.empty()){
				history_.pop_back();
				history_item_counter_--;
			}

			int command = lexical_cast<int>(cmd.at(1));
			std::deque< ai_command_history_item >::reverse_iterator j = history_.rbegin();
			//yes, the iterator could be precisely positioned (since command numbers go 1,2,3,4,..). will do it later.
			while ( (j!=history_.rend()) && (j->get_number()!=command) ){
				j++;// this is *reverse* iterator
			}
			if (j!=history_.rend()){
				return evaluate_command(side,j->get_command());//no infinite loop since '!' commands are not present in history
			}
			return "AI MANAGER: no command with requested number found";
		}
	} else if (cmd.size()==1){
		if (cmd.at(0)=="!help") {
			return
				"known commands:\n"
				"!    - repeat last command (? and ! do not count)\n"
				"! NUMBER    - repeat numbered command\n"
				"?    - show a history list\n"
				"!add_ai TEAM FILE    - add a AI to side (0 - command AI, N - AI for side #N) from file\n"
				"!remove_ai TEAM    - remove AI from side (0 - command AI, N - AI for side #N)\n"
				"!replace_ai TEAM FILE    - replace AI of side (0 - command AI, N - AI for side #N) from file\n"
				"!help    - show this help message";
		}
	}


	return "AI MANAGER: nothing to do";
}

// =======================================================================
// ADD, CREATE AIs, OR LIST AI TYPES
// =======================================================================

//@todo 1.7 add error reporting
bool ai_manager::add_ai_for_side_from_file( int side, const std::string& file, bool replace )
{
	config cfg;
	if (!ai_configuration::get_side_config_from_file(file,cfg)){
		ERR_AI_MANAGER << " unable to read [SIDE] config for side "<< side << "from file [" << file <<"]"<< std::endl;
		return false;
	}
	return add_ai_for_side_from_config(side,cfg,replace);
}


bool ai_manager::add_ai_for_side_from_config( int side, const config& cfg, bool replace ){
	config ai_memory;//AI memory
	std::vector<config> ai_parameters;//AI parameters inside [ai] tags. May contain filters
	config global_ai_parameters ;//AI parameters which do not have a filter applied
	const config& default_ai_parameters = ai_configuration::get_default_ai_parameters();//default AI parameters
	std::string ai_algorithm_type;//AI algorithm type
	config effective_ai_parameters;//legacy effective ai parameters

	ai_configuration::parse_side_config(cfg, ai_algorithm_type, global_ai_parameters, ai_parameters, default_ai_parameters, ai_memory, effective_ai_parameters);
	if (replace) {
		remove_ai_for_side (side);
	}
	ai_holder new_ai_holder(side,ai_algorithm_type);
	new_ai_holder.set_ai_effective_parameters(effective_ai_parameters);
	new_ai_holder.set_ai_global_parameters(global_ai_parameters);
	new_ai_holder.set_ai_memory(ai_memory);
	new_ai_holder.set_ai_parameters(ai_parameters);
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);
	ai_stack_for_specific_side.push(new_ai_holder);
	return true;
}


//@todo 1.7 add error reporting
bool ai_manager::add_ai_for_side( int side, const std::string& ai_algorithm_type, bool replace )
{
	if (replace) {
		remove_ai_for_side (side);
	}
	ai_holder new_ai_holder(side,ai_algorithm_type);
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);
	ai_stack_for_specific_side.push(new_ai_holder);
	return true;
}


ai_interface* ai_manager::create_transient_ai( const std::string &ai_algorithm_type, ai::readwrite_context *rw_context )
{
	assert(rw_context!=NULL);
	//@todo 1.7 modify this code to use a 'factory lookup' pattern -
	//a singleton which holds a map<string,ai_factory_ptr> of all functors which can create AIs.
	//this will allow individual AI implementations to 'register' themselves.



	//: To add an AI of your own, put
	//	if(ai_algorithm_type == "my_ai") {
	//		LOG_AI_MANAGER << "Creating new AI of type [" << "my_ai" << "]"<< std::endl;
	//		ai_interface *a = new my_ai(*rw_context);
	//		a->on_create();
	//		return a;
	//	}
	// at the top of this function

	//if(ai_algorithm_type == ai_manager::AI_TYPE_SAMPLE_AI) {
	//  LOG_AI_MANAGER << "Creating new AI of type [" << ai_manager::AI_TYPE_IDLE_AI << "]"<< std::endl;
	//	ai_interface *a = new sample_ai(*rw_context);
	//	a->on_create();
	//	return a;
	//}

	if(ai_algorithm_type == ai_manager::AI_TYPE_IDLE_AI) {
		LOG_AI_MANAGER << "Creating new AI of type [" << ai_manager::AI_TYPE_IDLE_AI << "]"<< std::endl;
		ai_interface *a = new idle_ai(*rw_context);
		a->on_create();
		return a;
	}

	if(ai_algorithm_type == ai_manager::AI_TYPE_FORMULA_AI) {
		LOG_AI_MANAGER << "Creating new AI of type [" << ai_manager::AI_TYPE_FORMULA_AI << "]"<< std::endl;
		ai_interface *a = new formula_ai(*rw_context);
		a->on_create();
		return a;
	}

	if(ai_algorithm_type == ai_manager::AI_TYPE_DFOOL_AI) {
		LOG_AI_MANAGER << "Creating new AI of type [" << ai_manager::AI_TYPE_DFOOL_AI << "]"<< std::endl;
		ai_interface *a = new dfool::dfool_ai(*rw_context);
		a->on_create();
		return a;
	}

	if(ai_algorithm_type == ai_manager::AI_TYPE_AI2) {
		LOG_AI_MANAGER << "Creating new AI of type [" << ai_manager::AI_TYPE_AI2 << "]"<< std::endl;
		ai_interface *a = new ai2(*rw_context);
		a->on_create();
		return a;
	}

	if (!ai_algorithm_type.empty() && ai_algorithm_type != ai_manager::AI_TYPE_DEFAULT) {
		ERR_AI_MANAGER << "AI not found: [" << ai_algorithm_type << "]. Using default instead.\n";
	}

	LOG_AI_MANAGER  << "Creating new AI of type [" << ai_manager::AI_TYPE_DEFAULT << "]"<< std::endl;
	ai_interface *a = new ai_default(*rw_context);
	a->on_create();
	return a;
}

std::vector<std::string> ai_manager::get_available_ais()
{
	std::vector<std::string> ais;
	ais.push_back("default");
	return ais;
}




// =======================================================================
// REMOVE
// =======================================================================

void ai_manager::remove_ai_for_side( int side )
{
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);
	if (!ai_stack_for_specific_side.empty()){
		ai_stack_for_specific_side.pop();
	}
}


void ai_manager::remove_all_ais_for_side( int side )
{
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);

	//clear the stack. std::stack doesn't have a '.clear()' method to do it
	while (!ai_stack_for_specific_side.empty()){
			ai_stack_for_specific_side.pop();
	}
}


void ai_manager::clear_ais()
{
	ai_map_.clear();
}

// =======================================================================
// GET active AI parameters
// =======================================================================

const std::vector<config>& ai_manager::get_active_ai_parameters_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_parameters();
}


const config& ai_manager::get_active_ai_effective_parameters_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_effective_parameters();
}


const config& ai_manager::get_active_ai_global_parameters_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_global_parameters();
}


const config& ai_manager::get_active_ai_memory_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_memory();
}


const std::string& ai_manager::get_active_ai_algorithm_type_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_algorithm_type();
}


ai_game_info& ai_manager::get_active_ai_info_for_side( int /*side*/ )
{
	return *ai_info_;
}


ai_game_info& ai_manager::get_ai_info()
{
	return *ai_info_;
}

// =======================================================================
// SET active AI parameters
// =======================================================================

void ai_manager::set_active_ai_parameters_for_side( int side, const std::vector<config>& ai_parameters )
{
	get_active_ai_holder_for_side(side).set_ai_parameters(ai_parameters);
}


void ai_manager::set_active_ai_effective_parameters_for_side( int side, const config& ai_parameters )
{
	get_active_ai_holder_for_side(side).set_ai_effective_parameters(ai_parameters);
}


void ai_manager::set_active_ai_global_parameters_for_side( int side, const config& ai_global_parameters )
{
	get_active_ai_holder_for_side(side).set_ai_global_parameters(ai_global_parameters);
}


void ai_manager::set_active_ai_memory_for_side( int side, const config& ai_memory )
{
	get_active_ai_holder_for_side(side).set_ai_memory(ai_memory);
}


void ai_manager::set_active_ai_algorithm_type_for_side( int side, const std::string& ai_algorithm_type )
{
	get_active_ai_holder_for_side(side).set_ai_algorithm_type(ai_algorithm_type);
}


// =======================================================================
// PROXY
// =======================================================================

void ai_manager::play_turn( int side, events::observer* /*event_observer*/ ){
	last_interact_ = 0;
	num_interact_ = 0;
	ai_interface& ai_obj = get_active_ai_for_side(side);
	ai_obj.play_turn();
	DBG_AI_MANAGER << "side " << side << ": number of user interactions: "<<num_interact_<<std::endl;
}


// =======================================================================
// PRIVATE
// =======================================================================
// =======================================================================
// AI STACKS
// =======================================================================
std::stack<ai_holder>& ai_manager::get_or_create_ai_stack_for_side( int side )
{
	AI_map_of_stacks::iterator iter = ai_map_.find(side);
	if (iter!=ai_map_.end()){
		return iter->second;
	}
	return ai_map_.insert(std::pair<int, std::stack<ai_holder> >(side, std::stack<ai_holder>())).first->second;
}

// =======================================================================
// AI HOLDERS
// =======================================================================
ai_holder& ai_manager::get_active_ai_holder_for_side( int side )
{
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);

	if (!ai_stack_for_specific_side.empty()){
		return ai_stack_for_specific_side.top();
	} else if (side==ai_manager::AI_TEAM_COMMAND_AI){
		return get_command_ai_holder( side );
	} else {
		return get_fallback_ai_holder( side );
	}

}

ai_holder& ai_manager::get_command_ai_holder( int /*side*/ )
{
	ai_holder& ai_holder = get_or_create_active_ai_holder_for_side_without_fallback(ai_manager::AI_TEAM_COMMAND_AI,AI_TYPE_FORMULA_AI);
	return ai_holder;
}

ai_holder& ai_manager::get_fallback_ai_holder( int /*side*/ )
{
	ai_holder& ai_holder = get_or_create_active_ai_holder_for_side_without_fallback(ai_manager::AI_TEAM_FALLBACK_AI,AI_TYPE_IDLE_AI);
	return ai_holder;
}

ai_holder& ai_manager::get_or_create_active_ai_holder_for_side_without_fallback(int side, const std::string& ai_algorithm_type)
{
	std::stack<ai_holder>& ai_stack_for_specific_side = get_or_create_ai_stack_for_side(side);

	if (!ai_stack_for_specific_side.empty()){
		return ai_stack_for_specific_side.top();
	} else {
		ai_holder new_ai_holder(side, ai_algorithm_type);
		ai_stack_for_specific_side.push(new_ai_holder);
		return ai_stack_for_specific_side.top();
	}

}


// =======================================================================
// AI POINTERS
// =======================================================================

ai_interface& ai_manager::get_active_ai_for_side( int side )
{
	return get_active_ai_holder_for_side(side).get_ai_ref();
}


ai_interface& ai_manager::get_or_create_active_ai_for_side_without_fallback( int side, const std::string& ai_algorithm_type )
{
	ai_holder& ai_holder = get_or_create_active_ai_holder_for_side_without_fallback(side,ai_algorithm_type);
	return ai_holder.get_ai_ref();
}

ai_interface& ai_manager::get_command_ai( int side )
{
	ai_holder& ai_holder = get_command_ai_holder(side);
	ai_interface& ai = ai_holder.get_ai_ref(side);
	return ai;
}

ai_interface& ai_manager::get_fallback_ai( int side )
{
	ai_holder& ai_holder = get_fallback_ai_holder(side);
	ai_interface& ai = ai_holder.get_ai_ref(side);
	return ai;
}

// =======================================================================
// MISC
// =======================================================================

