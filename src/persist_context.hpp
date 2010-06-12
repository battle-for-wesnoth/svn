/* $Id$ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef PERSIST_CONTEXT_H_INCLUDED
#define PERSIST_CONTEXT_H_INCLUDED
#include "config.hpp"
#include "log.hpp"
static lg::log_domain log_persist("engine/persistence");

#define LOG_SAVE LOG_STREAM(info, log_persist)
#define ERR_SAVE LOG_STREAM(err, log_persist)
config pack_scalar(const std::string &,const t_string &);
class persist_context {
private:
	struct name_space {
		std::string namespace_;
		std::string root_;
		std::string node_;
		std::string lineage_;
		std::string descendants_;
		bool valid_;
		bool valid() const {
			return valid_;
		}
		void parse() {
			while (namespace_.find_first_of("^") < namespace_.size()) {
				if (namespace_[0] == '^') {
					//TODO: Throw a WML error
					namespace_ = "";
					break;
				}
				std::string infix = namespace_.substr(namespace_.find_first_of("^"));
				size_t end = infix.find_first_not_of("^");
				if (!((end >= infix.length()) || (infix[end] == '.'))) {
					//TODO: Throw a WML error
					namespace_ = "";
					break;
				}
				infix = infix.substr(0,end);
				std::string suffix = namespace_.substr(namespace_.find_first_of("^") + infix.length());
				while (!infix.empty())
				{
					std::string body = namespace_.substr(0,namespace_.find_first_of("^"));
					body = body.substr(0,body.find_last_of("."));
					infix = infix.substr(1);
					namespace_ = body + infix + suffix;
				}
			}
		}
		name_space next() const {
			return name_space(descendants_);
		}
		name_space prev() const {
			return name_space(lineage_);
		}
		operator bool () const { return valid_; }
		name_space()
			: namespace_()
			, root_()
			, node_()
			, lineage_()
			, descendants_()
			, valid_(false)
		{
		}

		name_space(const std::string &ns)
			: namespace_(ns)
			, root_()
			, node_()
			, lineage_()
			, descendants_()
			, valid_(false)
		{
			parse();
			valid_ = ((namespace_.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.") > namespace_.length()) && !namespace_.empty());
			root_ = namespace_.substr(0,namespace_.find_first_of("."));
			node_ = namespace_.substr(namespace_.find_last_of(".") + 1);
			if (namespace_.find_last_of(".") <= namespace_.length())
				lineage_ = namespace_.substr(0,namespace_.find_last_of("."));
			if (namespace_.find_first_of(".") <= namespace_.length())
				descendants_ = namespace_.substr(namespace_.find_first_of(".") + 1);
		}
	};
	// TODO: transaction support (needed for MP)
	name_space namespace_;
	config cfg_;
	struct node {
		typedef std::map<std::string,node*> child_map;
		std::string name_;
		persist_context *root_;
		config &cfg_;
		node *parent_;
		child_map children_;
		node(std::string name, persist_context *root, config & cfg, node *parent = NULL)
			: name_(name)
			, root_(root)
			, cfg_(cfg)
			, parent_(parent)
			, children_()
		{
		}

		~node() {
			for (child_map::iterator i = children_.begin(); i != children_.end(); i++)
				delete (i->second);
		}
		config &cfg() { return cfg_; }
		node &add_child(const std::string &name) {
			children_[name] = new node(name,root_,cfg_.child_or_add(name),this);
			return *(children_[name]);
		}
		node &child(const name_space &name) {
			if (name) {
				if (children_.find(name.root_) == children_.end())
					add_child(name.root_);
				node &chld = *children_[name.root_];
				return chld.child(name.next());
			}
			else return *this;
		}
		void init () {
			for (config::all_children_iterator i = cfg_.ordered_begin(); i != cfg_.ordered_end(); i++) {
				if (i->key != "variables") {
					child(i->key).init();
				}
			}
			if (!cfg_.child("variables"))
				cfg_.add_child("variables");
		}
	};
	bool valid_;
	node root_node_;
	node *active_;
	void load();
	void init();
	bool save_context();
	persist_context()
		: namespace_()
		, cfg_()
		, valid_(false)
		, root_node_("",this,cfg_)
		, active_(&root_node_)
	{};

	static persist_context invalid;
	persist_context &add_child(const std::string &key);
public:
	persist_context(const std::string &name_space);
	~persist_context();
	bool clear_var(std::string &);
	config get_var(const std::string &);
	bool set_var(const std::string &, const config &);
	bool valid() const { return valid_; };
	bool dirty() const {
		return true;
	};
	operator bool() { return valid_; }
};
#endif
