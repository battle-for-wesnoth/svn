/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>
#include <sstream>
#include <vector>

#include "config.hpp"
#include "filesystem.hpp"
#include "log.hpp"

std::string read_file(const std::string& fname)
{
	//if we have a path to the data
#ifdef WESNOTH_PATH

	//convert any filepath which is relative
	if(!fname.empty() && fname[0] != '/' && WESNOTH_PATH[0] == '/') {
		std::cerr << "trying to read file: '" <<
		           (WESNOTH_PATH + std::string("/") + fname) << "'\n";
		const std::string& res =
		           read_file(WESNOTH_PATH + std::string("/") + fname);
		if(!res.empty()) {
			std::cerr << "success\n";
			return res;
		}
	}
#endif

	std::ifstream file(fname.c_str());
	std::string res;
	char c;
	while(file.get(c)) {
		res.resize(res.size()+1);
		res[res.size()-1] = c;
	}

	return res;
}

void write_file(const std::string& fname, const std::string& data)
{
	std::ofstream file(fname.c_str());
	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		file << *i;
	}
}

namespace {

void internal_preprocess_file(const std::string& fname,
                              std::map<std::string,std::string> defines_map,
                              int depth, std::vector<char>& res)
{
	//if it's a directory, we process all files in the directory
	//that end in .cfg
	if(is_directory(fname)) {

		std::vector<std::string> files;
		get_files_in_dir(fname,&files,NULL,ENTIRE_FILE_PATH);

		for(std::vector<std::string>::const_iterator f = files.begin();
		    f != files.end(); ++f) {
			if(f->size() > 4 && std::equal(f->end()-4,f->end(),".cfg")) {
				internal_preprocess_file(*f,defines_map,depth,res);
			}
		}

		return;
	}

	const std::string data = read_file(fname);

	bool in_quotes = false;

	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		const char c = *i;
		if(c == '"') {
			in_quotes = !in_quotes;
		}

		if(c == '{') {
			std::stringstream newfile;
			for(++i; i != data.end() && *i != '}'; ++i) {
				newfile << *i;
			}

			if(i == data.end())
				break;

			const std::string fname = newfile.str();

			//if this is a known pre-processing symbol, then we insert
			//it, otherwise we assume it's a file name to load
			if(defines_map.count(fname) != 0) {
				const std::string& val = defines_map[fname];
				res.insert(res.end(),val.begin(),val.end());
			} else if(depth < 20) {
				internal_preprocess_file("data/" + newfile.str(),
				                       defines_map, depth+1,res);
			} else {
				const std::string& str = read_file(newfile.str());
				res.insert(res.end(),str.begin(),str.end());
			}
		} else if(c == '#' && !in_quotes) {
			//if this is the beginning of a pre-processing definition
			static const std::string hash_define("#define");
			if(data.end() - i > hash_define.size() &&
			   std::equal(hash_define.begin(),hash_define.end(),i)) {
				i += hash_define.size();
				while(i != data.end() && isspace(*i))
					++i;

				const std::string::const_iterator end =
				                    std::find_if(i,data.end(),isspace);

				if(end == data.end())
					break;

				const std::string symbol(i,end);
				std::stringstream value;
				for(i = end+1; i != data.end(); ++i) {
					static const std::string hash_enddef("#enddef");
					if(data.end() - i > hash_enddef.size() &&
					   std::equal(hash_enddef.begin(),hash_enddef.end(),i)) {
						i += hash_enddef.size();
						break;
					}

					value << *i;
				}

				defines_map.insert(std::pair<std::string,std::string>(
				                               symbol,value.str()));
			}

			//if this is a pre-processing conditional
			static const std::string hash_ifdef("#ifdef");
			static const std::string hash_else("#else");
			static const std::string hash_endif("#endif");

			if(data.end() - i > hash_ifdef.size() &&
			   std::equal(hash_ifdef.begin(),hash_ifdef.end(),i)) {
				i += hash_ifdef.size();
				while(i != data.end() && isspace(*i))
					++i;

				const std::string::const_iterator end =
				                    std::find_if(i,data.end(),isspace);

				if(end == data.end())
					break;

				//if the symbol is not defined, then we want to skip
				//to the #endif or #else . Otherwise, continue processing
				//as normal. The #endif will just be treated as a comment
				//anyway.
				const std::string symbol(i,end);
				if(defines_map.count(symbol) == 0) {
					while(data.end() - i > hash_endif.size() &&
					      !std::equal(hash_endif.begin(),hash_endif.end(),i) &&
					      !std::equal(hash_else.begin(),hash_else.end(),i)) {
						++i;
					}

					i = std::find(i,data.end(),'\n');
					if(i == data.end())
						break;
				} else {
					i = end;
				}
			}

			//if we come across a #else, it must mean that we found a #ifdef
			//earlier, and we should ignore until #endif
			if(data.end() - i > hash_else.size() &&
			   std::equal(hash_else.begin(),hash_else.end(),i)) {
				while(data.end() - i > hash_endif.size() &&
				      !std::equal(hash_endif.begin(),hash_endif.end(),i)) {
					++i;
				}

				i = std::find(i,data.end(),'\n');
				if(i == data.end())
					break;
			}

			for(; i != data.end() && *i != '\n'; ++i) {
			}

			if(i == data.end())
				break;

			res.push_back('\n');
		} else {
			res.push_back(c);
		}
	}

	return;
}

} //end anonymous namespace

std::string preprocess_file(const std::string& fname,
                            const std::map<std::string,std::string>* defines)
{
	log_scope("preprocessing file...");
	static const std::map<std::string,std::string> default_defines;
	if(defines == NULL)
		defines = &default_defines;

	std::vector<char> res;
	internal_preprocess_file(fname,*defines,0,res);
	return std::string(res.begin(),res.end());
}

config::config(const std::string& data)
{
	log_scope("parsing config...");
	read(data);
}

config::config(const config& cfg) : values(cfg.values)
{
	for(std::map<std::string,std::vector<config*> >::const_iterator i =
	    cfg.children.begin(); i != cfg.children.end(); ++i) {
		std::vector<config*> v;
		for(std::vector<config*>::const_iterator j = i->second.begin();
		    j != i->second.end(); ++j) {
			v.push_back(new config(**j));
		}

		children[i->first].swap(v);
	}
}

config::~config()
{
	clear();
}

config& config::operator=(const config& cfg)
{
	clear();

	values = cfg.values;

	for(std::map<std::string,std::vector<config*> >::const_iterator i =
	    cfg.children.begin(); i != cfg.children.end(); ++i) {
		std::vector<config*> v;
		for(std::vector<config*>::const_iterator j = i->second.begin();
		    j != i->second.end(); ++j) {
			v.push_back(new config(**j));
		}

		children[i->first].swap(v);
	}

	return *this;
}

void config::read(const std::string& data)
{
	clear();

	std::stack<std::string> element_names;
	std::stack<config*> elements;
	elements.push(this);
	element_names.push("");

	enum { ELEMENT_NAME, IN_ELEMENT, VARIABLE_NAME, VALUE }
	state = IN_ELEMENT;
	std::string var;
	std::string value;

	bool in_quotes = false;

	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		const char c = *i;
		switch(state) {
			case ELEMENT_NAME:
				if(c == ']') {
					if(value == "end" || !value.empty() && value[0] == '/') {
						assert(!elements.empty());

						if(value[0] == '/' &&
						   std::string("/" + element_names.top()) != value) {
							throw error("Found illegal end tag: '" +
							             value + "', at end of '" +
							             element_names.top() + "'");
						}

						elements.pop();
						element_names.pop();

						if(elements.empty()) {
							throw error("Unexpected terminating tag\n");
							return;
						}


						state = IN_ELEMENT;

						break;
					}

					config* const new_config = new config();
					elements.top()->children[value].push_back(new_config);
					elements.push(new_config);
					element_names.push(value);
					state = IN_ELEMENT;
					value = "";
				} else {
					value.resize(value.size()+1);
					value[value.size()-1] = c;
				}

				break;

			case IN_ELEMENT:
				if(c == '[') {
					state = ELEMENT_NAME;
					value = "";
				} else if(!isspace(c)) {
					value.resize(1);
					value[0] = c;
					state = VARIABLE_NAME;
				}

				break;

			case VARIABLE_NAME:
				if(c == '=') {
					state = VALUE;
					var = value;
					value = "";
				} else {
					value.resize(value.size()+1);
					value[value.size()-1] = c;
				}

				break;

			case VALUE:
				if(c == '"') {
					in_quotes = !in_quotes;
				} else if(c == '\n' && !in_quotes) {
					state = IN_ELEMENT;
					elements.top()->values.insert(
							std::pair<std::string,std::string>(var,value));
					var = "";
					value = "";
				} else {
					value.resize(value.size()+1);
					value[value.size()-1] = c;
				}

				break;
		}
	}
}

std::string config::write() const
{
	std::string res;
	for(std::map<std::string,std::string>::const_iterator i = values.begin();
					i != values.end(); ++i) {
		if(i->second.empty() == false) {
			res += i->first + "=" + i->second + "\n";
		}
	}

	for(std::map<std::string,std::vector<config*> >::const_iterator j =
					children.begin(); j != children.end(); ++j) {
		const std::vector<config*>& v = j->second;
		for(std::vector<config*>::const_iterator it = v.begin();
						it != v.end(); ++it) {
			res += "[" + j->first + "]\n";
			res += (*it)->write();
			res += "[/" + j->first + "]\n";
		}
	}

	res += "\n";

	return res;
}

std::vector<std::string> config::split(const std::string& val)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while(i2 != val.end()) {
		if(*i2 == ',') {
			std::string new_val(i1,i2);
			if(!new_val.empty())
				res.push_back(new_val);
			++i2;
			while(i2 != val.end() && *i2 == ' ')
				++i2;

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1,i2);
	if(!new_val.empty())
		res.push_back(new_val);

	return res;
}

bool config::has_value(const std::string& values, const std::string& val)
{
	const std::vector<std::string>& vals = split(values);
	return std::count(vals.begin(),vals.end(),val) > 0;
}

void config::clear()
{
	for(std::map<std::string,std::vector<config*> >::iterator i =
					children.begin(); i != children.end(); ++i) {
		std::vector<config*>& v = i->second;
		for(std::vector<config*>::iterator j = v.begin(); j != v.end(); ++j)
			delete *j;
	}

	children.clear();
	values.clear();
}

//#define TEST_CONFIG

#ifdef TEST_CONFIG

int main()
{
	config cfg(read_file("testconfig"));
	std::cout << cfg.write() << std::endl;
}

#endif
