/* $Id: ai_manager.hpp   $ */
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
 * Managing the AIs lifecycle - headers
 * @file ai_manager.hpp
 * */

#ifndef AI_MANAGER_HPP_INCLUDED
#define AI_MANAGER_HPP_INCLUDED

#include "global.hpp"

#include "ai_interface.hpp"
#include <map>
#include <stack>
#include <vector>
#include <deque>


/**
 * Base class to hold the AI and current AI parameters. It is an implementation detail.
 */
class ai_holder{
public:
	ai_holder(int team, const std::string& ai_algorithm_type);

	void init(ai_interface::info& i);

	virtual ~ai_holder();

	ai_interface& get_ai_ref(ai_interface::info& i);

	const std::string& get_ai_algorithm_type() const;
	void set_ai_algorithm_type(const std::string& ai_algorithm_type);

	const config& get_ai_memory() const;
	config& get_ai_memory();
	void set_ai_memory(const config& ai_memory);

	const std::vector<config>& get_ai_parameters() const;
	std::vector<config>& get_ai_parameters();
	void set_ai_parameters(const std::vector<config>& ai_parameters);

	const config& get_ai_effective_parameters() const;
	config& get_ai_effective_parameters();
	void set_ai_effective_parameters(const config& ai_effective_parameters);

	const config& get_ai_global_parameters() const;
	config& get_ai_global_parameters();
	void set_ai_global_parameters(const config& ai_global_parameters);

	const std::string describe_ai();

	//not used in the moment
	bool is_mandate_ok(ai_interface::info &i);

private:
	ai_interface* ai_;
	std::string ai_algorithm_type_;
	config ai_effective_parameters_;
	config ai_global_parameters_;
	config ai_memory_;
	std::vector<config> ai_parameters_;
	int team_;

	ai_interface* create_ai(ai_interface::info& info);
};

/**
 * AI Command History Item. It is an implementation detail
 */
class ai_command_history_item{
public:
	ai_command_history_item();

	ai_command_history_item(int number, const std::string& command);

	virtual ~ai_command_history_item();

	int get_number() const;
	void set_number(int number);

	const std::string& get_command() const;
	void set_command(const std::string& command);

private:
	int number_;
	std::string command_;

};

/**
* AI Manager - class that manages AI's for all teams and manages AI redeployment
* This class is responsible for managing the AI lifecycle
* It can be accessed like this:   ai_manager:foo(...);
*/
class ai_manager
{
public:

	// =======================================================================
	// CONSTANTS
	// =======================================================================

	static const size_t MAX_HISTORY_SIZE = 200;

	static const int AI_TEAM_COMMAND_AI = 0;
	static const int AI_TEAM_FALLBACK_AI = -1;


	static const std::string AI_TYPE_SAMPLE_AI;
	static const std::string AI_TYPE_IDLE_AI;
	static const std::string AI_TYPE_FORMULA_AI;
	static const std::string AI_TYPE_DFOOL_AI;
	static const std::string AI_TYPE_AI2;
	static const std::string AI_TYPE_DEFAULT;


	// =======================================================================
	// LIFECYCLE
	// =======================================================================

protected:

	ai_manager();


public:

	virtual ~ai_manager();

	// =======================================================================
	// EVALUATION
	// =======================================================================

	/**
	 * Evaluate a string command using command AI
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * Will intercept those commands which start with '!' and '?',
	 * and will try to evaluate them as internal commands
	 * @return string result of evaluation
	 * @param i game information for the AI
	 * @param str string to evaluate
	 */
	static const std::string evaluate_command( ai_interface::info& i, const std::string& str );


	// =======================================================================
	// ADD, CREATE AIs, OR LIST AI TYPES
	// =======================================================================

	/**
	 * Add active AI for specified team from 'file'.
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * AI is not initialized at this point.
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param file file name, follows the usual WML convention
	 * @return success?
	 * @retval true all ok
	 * @retval false not all ok
	 */
	static bool add_ai_for_team_from_file( int team, const std::string& file );


	/**
	 * Add active AI for specified team from parameters.
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * AI is not initialized at this point.
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_algorithm_type type of AI algorithm to create
	 * @param ai_params AI parameters
	 * @return success?
	 * @retval true all ok
	 * @retval false not all ok
	 */
	static bool add_ai_for_team( int team, const std::string& ai_algorithm_type);


	/**
	 * Return a pointer to a new AI. it is the sole responsibility of the caller to manage its lifecycle
	 * @param ai_algorithm_type type of AI algorithm to create
	 * @param i game information for the AI.
	 * @param master should this AI be a master AI ? (only master AIs are allowed to fallback. if you are not sure, pick 'false' here)
	 * @return returns the reference to the created AI
	 */
	static ai_interface* create_transient_ai( const std::string& ai_algorithm_type, ai_interface::info& i, bool master = false);


	/**
	 * Return a list of available AIs
	 * @return the list of available AIs
	 * @todo tet this list from a list of files in specified directory
	 */
	static std::vector<std::string> get_available_ais();


	// =======================================================================
	// REMOVE
	// =======================================================================

	/**
	 * Remove top-level AI from team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static void remove_ai_for_team( int team );


	/**
	 * Remove all AIs from team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static void remove_all_ais_for_team( int team );


	/**
	 * Clear all the AIs.
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * for example, this is called from the destructor of playsingle_controller
	 * it is necessary to do this if any of the info structures used by the AI go out of scope.
	 */
	static void clear_ais();


	// =======================================================================
	// GET active AI parameters
	// =======================================================================

	/**
	 * Get active AI for specified team.
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to the active ai
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param i game information for the AI. is used only if AI is to be created.
	 */
	static ai_interface& get_active_ai_for_team( int team, ai_interface::info& i );


	/**
	 * Get AI parameters for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to active ai parameters.
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static const std::vector<config>& get_active_ai_parameters_for_team( int team );


	/**
	 * Get effective AI parameters for active AI of the specific team
	 * @note; Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to active ai effective parameters.
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static const config& get_active_ai_effective_parameters_for_team( int team );


	/**
	 * Get global AI parameters for active AI of the specific team
	 * @note; Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to global ai global parameters.
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static const config& get_active_ai_global_parameters_for_team( int team );


	/**
	 * Get AI memory for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to active ai memory.
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static const config& get_active_ai_memory_for_team( int team );


	/**
	 * Get AI algorithm type for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @return a reference to active ai algorithm_type.
	 * @note this reference may become invalid after specific ai_manager operations.
	 * @param team side number (1-based, as in ai_interface::info)
	 */
	static const std::string& get_active_ai_algorithm_type_for_team( int team );


	// =======================================================================
	// SET active AI parameters
	// =======================================================================

	/**
	 * Set AI parameters for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_parameters ai parameters to be set
	 */
	static void set_active_ai_parameters_for_team( int team, const std::vector<config>& ai_parameters );


	/**
	 * Set effective AI parameters for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_effective_parameters ai effective parameters to be set
	 * @deprecated added only for bug-for-bug compatibility with team.cpp. Will be refactored away
	 */
	static void set_active_ai_effective_parameters_for_team( int team, const config& ai_effective_parameters );


	/**
	 * Set global AI parameters for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_global_parameters ai global parameters to be set
	 * @deprecated added only for bug-for-bug compatibility with team.cpp. Will be refactored away
	 */
	static void set_active_ai_global_parameters_for_team( int team, const config& ai_global_parameters );


	/**
	 * Set AI memory for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_memory ai memory to be set
	 * @deprecated added only for bug-for-bug compatibility with team.cpp. Will be refactored away
	 */
	static void set_active_ai_memory_for_team( int team, const config& ai_memory );


	/**
	 * Set AI algorithm type for active AI of the specific team
	 * @note: Running this command may invalidate references previously returned by AI Manager
	 * @param team side number (1-based, as in ai_interface::info)
	 * @param ai_algorithm_type ai algorithm type to be set
	 */
	static void set_active_ai_algorithm_type_for_team( int team, const std::string& ai_algorithm_type );


private:

	typedef std::map< int, std::stack< ai_holder > > AI_map_of_stacks;
	static AI_map_of_stacks ai_map_;
	static std::deque< ai_command_history_item > history_;
	static long history_item_counter;

	// =======================================================================
	// EVALUATION
	// =======================================================================

	/**
	 * Evaluate an internal AI manager command
	 * @return string result of evaluation
	 * @param i game information for the AI
	 * @param str string to evaluate
	 * @todo: rewrite this function to use a fai or lua parser
	 */
	static const std::string internal_evaluate_command( ai_interface::info& i, const std::string& str );

	/**
	 * Determine if the command should be intercepted and evaluated as internal command
	 * @param str command string to check
	 * @return should the command be intercepted ?
	 * @retval true the command should be evaluated as internal command
	 * @retval false the command should be evaluated as normal command
	 */
	static bool should_intercept( const std::string& str );

	// =======================================================================
	// AI STACKS
	// =======================================================================


	/**
	 * Get the AI stack for the specified team, create it if it doesn't exist
	 */
	static std::stack< ai_holder >& get_or_create_ai_stack_for_team(int team);

	// =======================================================================
	// AI HOLDERS
	// =======================================================================


	/**
	 * Get active AI holder for specified team
	 */
	static ai_holder& get_active_ai_holder_for_team( int team );

	/**
	 * Get command AI holder for specified team
	 */
	static ai_holder& get_command_ai_holder( int team );

	/**
	 * Get fallback AI holder for specified team
	 */
	static ai_holder& get_fallback_ai_holder( int team );

	/**
	 * Get or create active AI holder for specified team without fallback
	 */
	static ai_holder& get_or_create_active_ai_holder_for_team_without_fallback(int team, const std::string& ai_algorithm_type);

	// =======================================================================
	// AI POINTERS
	// =======================================================================

	/**
	 * Get the command AI for the specified team
	 */
	static ai_interface& get_command_ai(int team, ai_interface::info& i);


	/**
	 * Get the fallback AI for the specified team
	 */
	static ai_interface& get_fallback_ai(int team, ai_interface::info& i);

	/**
	 * Get or create active ai for specified team without fallback
	 */
	static ai_interface& get_or_create_active_ai_for_team_without_fallback(int team, ai_interface::info& i, const std::string& ai_algorithm_type);


	// =======================================================================
	// MISC
	// =======================================================================

	/**
	 * Correct team information inside the 'info' struct
	 */
	static void correct_team_in_info(int team, ai_interface::info& i);

};

//@todo: Refactor history handling and internal commands
//@todo: Refactor all the mess with those AI parameters
//@todo: AI Interface command to clear the history

#endif
