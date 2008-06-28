/* $Id$ */
/*
   Copyright (C) 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef FORMULA_FUNCTION_HPP_INCLUDED
#define FORMULA_FUNCTION_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include <map>

#include "formula.hpp"
#include "variant.hpp"

namespace game_logic {

class formula_expression {
public:
	formula_expression() : name_(NULL) {}
	virtual ~formula_expression() {}
	variant evaluate(const formula_callable& variables) const {
		call_stack_manager manager(name_);
		return execute(variables);
	}
	void set_name(const char* name) { name_ = name; }
private:
	virtual variant execute(const formula_callable& variables) const = 0;
	const char* name_;
};

typedef boost::shared_ptr<formula_expression> expression_ptr;

class function_expression : public formula_expression {
public:
	typedef std::vector<expression_ptr> args_list;
	explicit function_expression(
	                    const std::string& name,
	                    const args_list& args,
	                    int min_args=-1, int max_args=-1)
	    : name_(name), args_(args)
	{
		set_name(name.c_str());
		if(min_args >= 0 && args_.size() < static_cast<size_t>(min_args)) {
			std::cerr << "too few arguments\n";
			throw formula_error();
		}

		if(max_args >= 0 && args_.size() > static_cast<size_t>(max_args)) {
			std::cerr << "too many arguments\n";
			throw formula_error();
		}
	}

protected:
	const args_list& args() const { return args_; }
private:
	std::string name_;
	args_list args_;
};

class key_value_pair : public formula_callable {
	variant key_;
	variant value_;

	variant get_value(const std::string& key) const;
	
	void get_inputs(std::vector<game_logic::formula_input>* inputs) const;	
public:	
	explicit key_value_pair(const variant& key, const variant& value) : key_(key), value_(value) {}  
};

class formula_function_expression : public function_expression {
public:
	explicit formula_function_expression(const std::string& name, const args_list& args, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& arg_names);
private:
	variant execute(const formula_callable& variables) const;
	const_formula_ptr formula_;
	const_formula_ptr precondition_;
	std::vector<std::string> arg_names_;
	int star_arg_;
};

typedef boost::shared_ptr<function_expression> function_expression_ptr;

class formula_function {
	std::string name_;
	const_formula_ptr formula_;
	const_formula_ptr precondition_;
	std::vector<std::string> args_;
public:
	formula_function() :
		name_(),
		formula_(),
		precondition_(),
		args_()
	{
	}

	formula_function(const std::string& name, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& args) : name_(name), formula_(formula), precondition_(precondition), args_(args)
	{}

	function_expression_ptr generate_function_expression(const std::vector<expression_ptr>& args) const;
};	

class function_symbol_table {
	std::map<std::string, formula_function> custom_formulas_;
public:
	function_symbol_table() :
		custom_formulas_()
	{
	}

	virtual ~function_symbol_table() {}
	virtual void add_formula_function(const std::string& name, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& args);
	virtual expression_ptr create_function(const std::string& fn,
					                       const std::vector<expression_ptr>& args) const;
	std::vector<std::string> get_function_names() const;
};

expression_ptr create_function(const std::string& fn,
                               const std::vector<expression_ptr>& args,
							   const function_symbol_table* symbols);
std::vector<std::string> builtin_function_names();

}

#endif
