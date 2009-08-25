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
 * Composite AI component
 * @file ai/composite/component.cpp
 */

#include "component.hpp"
#include "engine.hpp"
#include "../formula/ai.hpp"
#include "../../log.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

namespace ai {

static lg::log_domain log_ai_composite_component("ai/composite/component");
#define DBG_AI_COMPOSITE LOG_STREAM(debug, log_ai_composite_component)
#define LOG_AI_COMPOSITE LOG_STREAM(info, log_ai_composite_component)
#define ERR_AI_COMPOSITE LOG_STREAM(err, log_ai_composite_component)


component* component::get_child(const path_element &/*child*/)
{
	return NULL;
}


bool component::add_child(const path_element &/*child*/, const config &/*cfg*/)
{
	return false;
}


bool component::change_child(const path_element &/*child*/, const config &/*cfg*/)
{
	return false;
}


bool component::delete_child(const path_element &/*child*/)
{
	return false;
}




/*
[modify_ai]
    path = "stage['fallback']
    action = "change"
    [cfg]...[/cfg]
[/modify_ai]

[modify_ai]
    component = "aspect['avoid'].facet['zzz']"
    action = "change"
    [cfg]...[/cfg]
[/modify_ai]

[modify_ai]
    path = "aspect['aggression'].facet['zzzz']
    action = "delete"
[/modify_ai]

[modify_ai]
    component = "aspect['aggression'].facet"
    action = "add"
    [cfg]...[/cfg]
[/modify_ai]
*/


static component *find_component(component *root, const std::string &path, path_element &tail)
{
	if (root==NULL) {
		return NULL;
	}

	//match path elements in [modify_ai] tag
	boost::regex re("([^\\.^\\[]+)(\\['([^\\]]+)'\\]|\\[(\\d*)\\]|())");
	int const sub_matches[] = {1,3,4};
	boost::sregex_token_iterator i(path.begin(), path.end(), re, sub_matches);
	boost::sregex_token_iterator j;

	component *c  = root;

	std::vector< path_element > elements;
	while(i != j)
	{
		path_element pe;
		pe.property = *i++;
		pe.id = *i++;
		std::string position = *i++;
		if (position.empty()) {
			pe.position = -2;
		} else {
			try {
				pe.position = boost::lexical_cast<int>(position);
			} catch (boost::bad_lexical_cast){
				pe.position = -2;
			}
		}	
		DBG_AI_COMPOSITE << "adding path element: "<< pe << std::endl;
		elements.push_back(pe);
	}
	if (elements.size()<1) {
		return NULL;
	}

	std::vector< path_element >::iterator k_max = elements.end()-1;
	for (std::vector< path_element >::iterator k = elements.begin(); k!=k_max; ++k) {
		//not last
		c = c->get_child(*k);
		if (c==NULL) {
			return NULL;
		}
	}	

	tail = *k_max;	
	return c;

}


bool component_manager::add_component(component *root, const std::string &path, const config &cfg)
{
	path_element tail;
	component *c = find_component(root,path,tail);
	if (c==NULL) {
		return false;
	}
	return c->add_child(tail, cfg);
	
}

bool component_manager::change_component(component *root, const std::string &path, const config &cfg)
{
	path_element tail;
	component *c = find_component(root,path,tail);
	if (c==NULL) {
		return false;
	}
	return c->change_child(tail,cfg);
}

bool component_manager::delete_component(component *root, const std::string &path)
{
	path_element tail;
	component *c = find_component(root,path,tail);
	if (c==NULL) {
		return false;
	}

	return c->delete_child(tail);
}


} //end of namespace ai

std::ostream &operator<<(std::ostream &o, const ai::path_element &e)
{
	o << "property["<<e.property<<"] id["<<e.id <<"] position["<<e.position<<"]"<<std::endl;
	return o;
}
