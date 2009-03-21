/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"


//#include "foreach.hpp"
#include "callable_objects.hpp"
#include "formula_function.hpp"
#include "log.hpp"


#define DBG_NG LOG_STREAM(debug, engine)
#define LOG_AI LOG_STREAM(info, formula_ai)
#define WRN_AI LOG_STREAM(warn, formula_ai)
#define ERR_AI LOG_STREAM(err, formula_ai)

namespace game_logic {

namespace {

class dir_function : public function_expression {
public:
	explicit dir_function(const args_list& args)
	     : function_expression("dir", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		variant var = args()[0]->evaluate(variables);
		const formula_callable* callable = var.as_callable();
		callable->add_ref();
		std::vector<formula_input> inputs = callable->inputs();
		std::vector<variant> res;
		for(size_t i=0; i<inputs.size(); ++i) {
			const formula_input& input = inputs[i];
			res.push_back(variant(input.name));
		}

		return variant(&res);
	}
};

class if_function : public function_expression {
public:
	explicit if_function(const args_list& args)
	     : function_expression("if", args, 3, 3)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const int i = args()[0]->evaluate(variables).as_bool() ? 1 : 2;
		return args()[i]->evaluate(variables);
	}
};

class switch_function : public function_expression {
public:
	explicit switch_function(const args_list& args)
	    : function_expression("switch", args, 3, -1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		variant var = args()[0]->evaluate(variables);
		for(size_t n = 1; n < args().size()-1; n += 2) {
			variant val = args()[n]->evaluate(variables);
			if(val == var) {
				return args()[n+1]->evaluate(variables);
			}
		}

		if((args().size()%2) == 0) {
			return args().back()->evaluate(variables);
		} else {
			return variant();
		}
	}
};

class abs_function : public function_expression {
public:
	explicit abs_function(const args_list& args)
	     : function_expression("abs", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const int n = args()[0]->evaluate(variables).as_int();
		return variant(n >= 0 ? n : -n);
	}
};

class min_function : public function_expression {
public:
	explicit min_function(const args_list& args)
	     : function_expression("min", args, 1, -1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		bool found = false;
		int res = 0;
		for(size_t n = 0; n != args().size(); ++n) {
			const variant v = args()[n]->evaluate(variables);
			if(v.is_list()) {
				for(size_t m = 0; m != v.num_elements(); ++m) {
					if(!found || v[m].as_int() < res) {
						res = v[m].as_int();
						found = true;
					}
				}
			} else if(v.is_int()) {
				if(!found || v.as_int() < res) {
					res = v.as_int();
					found = true;
				}
			}
		}

		return variant(res);
	}
};

class max_function : public function_expression {
public:
	explicit max_function(const args_list& args)
	     : function_expression("max", args, 1, -1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		bool found = false;
		int res = 0;
		for(size_t n = 0; n != args().size(); ++n) {
			const variant v = args()[n]->evaluate(variables);
			if(v.is_list()) {
				for(size_t m = 0; m != v.num_elements(); ++m) {
					if(!found || v[m].as_int() > res) {
						res = v[m].as_int();
						found = true;
					}
				}
			} else if(v.is_int()) {
				if(!found || v.as_int() > res) {
					res = v.as_int();
					found = true;
				}
			}
		}

		return variant(res);
	}
};

class debug_float_function : public function_expression {
public:
        explicit debug_float_function(const args_list& args)
          : function_expression("debug_float", args, 2, 3)
        {}

private:
        variant execute(const formula_callable& variables) const {
                const args_list& arguments = args();
                const variant var0 = arguments[0]->evaluate(variables);
                const variant var1 = arguments[1]->evaluate(variables);

                const map_location location = convert_variant<location_callable>(var0)->loc();
                std::string text;
                
                if(arguments.size() == 2) {
                        text = var1.to_debug_string();
                        display_float(location,text);
                        return var1;
                } else {
                        const variant var2 = arguments[2]->evaluate(variables);
                        text = var1.string_cast() + var2.to_debug_string();
                        display_float(location,text);
                        return var2;
                }

        } 

        void display_float(const map_location& location, const std::string& text) const{
                game_display::get_singleton()->float_label(location,text,255,0,0);
        }
};


class debug_print_function : public function_expression {
public:
	explicit debug_print_function(const args_list& args)
	     : function_expression("debug_print", args, 1, 2)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant var1 = args()[0]->evaluate(variables);

		std::string str1,str2;

		if( args().size() == 1)
		{
			str1 = var1.to_debug_string();
			LOG_AI << str1 << std::endl;
			return var1;
		} else {
			str1 = var1.string_cast();
			const variant var2 = args()[1]->evaluate(variables);
			str2 = var2.to_debug_string();
			LOG_AI << str1 << str2 << std::endl;
			return var2;
		}
	}
};

class keys_function : public function_expression {
public:
	explicit keys_function(const args_list& args)
	     : function_expression("keys", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant map = args()[0]->evaluate(variables);
		return map.get_keys();
	}
};

class values_function : public function_expression {
public:
	explicit values_function(const args_list& args)
	     : function_expression("values", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant map = args()[0]->evaluate(variables);
		return map.get_values();
	}
};

class tolist_function : public function_expression {
public:
	explicit tolist_function(const args_list& args)
	     : function_expression("values", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant var = args()[0]->evaluate(variables);

		std::vector<variant> tmp;

		variant_iterator it = var.get_iterator();
		for(it = var.begin(); it != var.end(); ++it) {
			tmp.push_back( *it );
		}

		return variant( &tmp );
	}
};

class tomap_function : public function_expression {
public:
	explicit tomap_function(const args_list& args)
	     : function_expression("values", args, 1, 2)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant var_1 = args()[0]->evaluate(variables);

		std::map<variant, variant> tmp;

		if (args().size() == 2)
		{
			const variant var_2 = args()[1]->evaluate(variables);
			if ( var_1.num_elements() != var_2.num_elements() )
				return variant();
			for(size_t i = 0; i < var_1.num_elements(); i++ )
				tmp[ var_1[i] ] = var_2[i];
		} else
		{
			variant_iterator it = var_1.get_iterator();
			for(it = var_1.begin(); it != var_1.end(); ++it) {
				std::map<variant, variant>::iterator map_it = tmp.find( *it );
				if (map_it == tmp.end())
					tmp[ *it ] = variant( 1 );
				else
					map_it->second = variant(map_it->second.as_int() + 1);
			}
		}

		return variant( &tmp );
	}
};

class index_of_function : public function_expression {
public:
	explicit index_of_function(const args_list& args)
	     : function_expression("index_of", args, 2, 2)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant value = args()[0]->evaluate(variables);
		const variant list = args()[1]->evaluate(variables);

		for(size_t i = 0; i < list.num_elements(); i++ ) {
			if( list[i] == value) {
				return variant(i);
			}
		}

		return variant( -1 );
	}
};


class choose_function : public function_expression {
public:
	explicit choose_function(const args_list& args)
	     : function_expression("choose", args, 2, 3)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant items = args()[0]->evaluate(variables);
		variant max_value;
		variant_iterator it = items.get_iterator();
		variant_iterator max = variant_iterator();

		if(args().size() == 2) {
			for(it = items.begin(); it != items.end(); ++it) {
				const variant val = args().back()->evaluate(formula_variant_callable_with_backup(*it, variables));
				if(max == variant_iterator() || val > max_value) {
					max = it;
					max_value = val;
				}
			}
		} else {
			map_formula_callable self_callable;
			self_callable.add_ref();
			const std::string self = args()[1]->evaluate(variables).as_string();
			for(it = items.begin(); it != items.end(); ++it) {
				self_callable.add(self, *it);
				const variant val = args().back()->evaluate(formula_callable_with_backup(self_callable, formula_variant_callable_with_backup(*it, variables)));
				if(max == variant_iterator() || val > max_value) {
					max = it;
					max_value = val;
				}
			}
		}

		if(max == variant_iterator() ) {
			return variant();
		} else {
			return *max;
		}
	}
};

class wave_function : public function_expression {
public:
	explicit wave_function(const args_list& args)
	     : function_expression("wave", args, 1, 1)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const int value = args()[0]->evaluate(variables).as_int()%1000;
		const double angle = 2.0*3.141592653589*(static_cast<double>(value)/1000.0);
		return variant(static_cast<int>(sin(angle)*1000.0));
	}
};

namespace {
class variant_comparator : public formula_callable {
	expression_ptr expr_;
	const formula_callable* fallback_;
	mutable variant a_, b_;
	variant get_value(const std::string& key) const {
		if(key == "a") {
			return a_;
		} else if(key == "b") {
			return b_;
		} else {
			return fallback_->query_value(key);
		}
	}

	void get_inputs(std::vector<formula_input>* inputs) const {
		fallback_->get_inputs(inputs);
	}
public:
	variant_comparator(const expression_ptr& expr, const formula_callable& fallback) :
		expr_(expr),
		fallback_(&fallback),
		a_(),
		b_()
	{}

	bool operator()(const variant& a, const variant& b) const {
		a_ = a;
		b_ = b;
		return expr_->evaluate(*this).as_bool();
	}
};
}

class sort_function : public function_expression {
public:
	explicit sort_function(const args_list& args)
	     : function_expression("sort", args, 1, 2)
	{}

private:
	variant execute(const formula_callable& variables) const {
		variant list = args()[0]->evaluate(variables);
		std::vector<variant> vars;
		vars.reserve(list.num_elements());
		for(size_t n = 0; n != list.num_elements(); ++n) {
			vars.push_back(list[n]);
		}

		if(args().size() == 1) {
			std::sort(vars.begin(), vars.end());
		} else {
			std::sort(vars.begin(), vars.end(), variant_comparator(args()[1], variables));
		}

		return variant(&vars);
	}
};

class contains_string_function : public function_expression {
public:
	explicit contains_string_function(const args_list& args)
	     : function_expression("contains_string", args, 2, 2)
	{}

private:
	variant execute(const formula_callable& variables) const {
		std::string str = args()[0]->evaluate(variables).as_string();
		std::string key = args()[1]->evaluate(variables).as_string();

		if (key.size() > str.size())
			return variant(0);

		std::string::iterator str_it, key_it, tmp_it;

		for(str_it = str.begin(); str_it != str.end() - (key.size()-1); ++str_it)
		{
			key_it = key.begin();
                        if((key_it) == key.end()) {
                                return variant(1);
                        }
			tmp_it = str_it;

			while( *tmp_it == *key_it)
			{
				if( ++key_it == key.end())
					return variant(1);
				if( ++tmp_it == str.end())
					return variant(0);
			}
		}

		return variant(0);
	}
};

class filter_function : public function_expression {
public:
	explicit filter_function(const args_list& args)
	    : function_expression("filter", args, 2, 3)
	{}
private:
	variant execute(const formula_callable& variables) const {
		std::vector<variant> list_vars;
		std::map<variant,variant> map_vars;

		const variant items = args()[0]->evaluate(variables);

		variant_iterator it = items.get_iterator();

		if(args().size() == 2) {
			for(it = items.begin(); it != items.end(); ++it)  {
				const variant val = args()[1]->evaluate(formula_variant_callable_with_backup(*it, variables));
				if(val.as_bool()) {
					if (items.is_map() )
						map_vars[ (*it).get_member("key") ] = (*it).get_member("value");
					else
						list_vars.push_back(*it);
				}
			}
		} else {
			map_formula_callable self_callable;
			self_callable.add_ref();
			const std::string self = args()[1]->evaluate(variables).as_string();
			for(it = items.begin(); it != items.end(); ++it) {
				self_callable.add(self, *it);
				const variant val = args()[2]->evaluate(formula_callable_with_backup(self_callable, formula_variant_callable_with_backup(*it, variables)));
				if(val.as_bool()) {
					if (items.is_map() )
						map_vars[ (*it).get_member("key") ] = (*it).get_member("value");
					else
						list_vars.push_back(*it);
				}
			}
		}
		if (items.is_map() )
			return variant(&map_vars);
		return variant(&list_vars);
	}
};

class find_function : public function_expression {
public:
	explicit find_function(const args_list& args)
	    : function_expression("find", args, 2, 3)
	{}

private:
	variant execute(const formula_callable& variables) const {
		const variant items = args()[0]->evaluate(variables);

		variant_iterator it = items.get_iterator();
		if(args().size() == 2) {
			for(it = items.begin(); it != items.end(); ++it) {
				const variant val = args()[1]->evaluate(formula_variant_callable_with_backup(*it, variables));
				if(val.as_bool()) {
					return *it;
				}
			}
		} else {
			map_formula_callable self_callable;
			self_callable.add_ref();
			const std::string self = args()[1]->evaluate(variables).as_string();
			for(it = items.begin(); it != items.end(); ++it){
				self_callable.add(self, *it);
				const variant val = args().back()->evaluate(formula_callable_with_backup(self_callable, formula_variant_callable_with_backup(*it, variables)));
				if(val.as_bool()) {
					return *it;
				}
			}
		}

		return variant();
	}
};

class map_function : public function_expression {
public:
	explicit map_function(const args_list& args)
	    : function_expression("map", args, 2, 3)
	{}
private:
	variant execute(const formula_callable& variables) const {
		std::vector<variant> list_vars;
		std::map<variant,variant> map_vars;
		const variant items = args()[0]->evaluate(variables);

		variant_iterator it = items.get_iterator();
		if(args().size() == 2) {
			for(it = items.begin(); it != items.end(); ++it) {
				const variant val = args().back()->evaluate(formula_variant_callable_with_backup(*it, variables));
				if (items.is_map() )
					map_vars[ (*it).get_member("key") ] = val;
				else
					list_vars.push_back(val);
			}
		} else {
			map_formula_callable self_callable;
			self_callable.add_ref();
			const std::string self = args()[1]->evaluate(variables).as_string();
			for(it = items.begin(); it != items.end(); ++it) {
				self_callable.add(self, *it);
				const variant val = args().back()->evaluate(formula_callable_with_backup(self_callable, formula_variant_callable_with_backup(*it, variables)));
				if (items.is_map() )
					map_vars[ (*it).get_member("key") ] = val;
				else
					list_vars.push_back(val);
			}
		}
		if (items.is_map() )
			return variant(&map_vars);
		return variant(&list_vars);
	}
};

class sum_function : public function_expression {
public:
	explicit sum_function(const args_list& args)
	    : function_expression("sum", args, 1, 2)
	{}
private:
	variant execute(const formula_callable& variables) const {
		variant res(0);
		const variant items = args()[0]->evaluate(variables);
		if (items.num_elements() > 0)
		{
			if (items[0].is_list() )
			{
				std::vector<variant> tmp;
				res = variant(&tmp);
				if(args().size() >= 2) {
					res = args()[1]->evaluate(variables);
					if(!res.is_list())
						return variant();
				}
			} else if( items[0].is_map() )
			{
				std::map<variant,variant> tmp;
				res = variant(&tmp);
				if(args().size() >= 2) {
					res = args()[1]->evaluate(variables);
					if(!res.is_map())
						return variant();
				}
			} else
			{
				if(args().size() >= 2) {
					res = args()[1]->evaluate(variables);
				}
			}
		}

		for(size_t n = 0; n != items.num_elements(); ++n) {
			res = res + items[n];
		}

		return res;
	}
};

class head_function : public function_expression {
public:
	explicit head_function(const args_list& args)
	    : function_expression("head", args, 1, 1)
	{}
private:
	variant execute(const formula_callable& variables) const {
		const variant items = args()[0]->evaluate(variables);
		variant_iterator it = items.get_iterator();
		it = items.begin();
                if(it == items.end()) {
                        return variant();
                }
		return *it;
	}
};

class size_function : public function_expression {
public:
	explicit size_function(const args_list& args)
	    : function_expression("size", args, 1, 1)
	{}
private:
	variant execute(const formula_callable& variables) const {
		const variant items = args()[0]->evaluate(variables);
		return variant(static_cast<int>(items.num_elements()));
	}
};

class null_function : public function_expression {
public:
	explicit null_function(const args_list& args)
	    : function_expression("null", args, 0, 0)
	{}
private:
	variant execute(const formula_callable& /*variables*/) const {
		return variant();
	}
};

class refcount_function : public function_expression {
public:
	explicit refcount_function(const args_list& args)
	    : function_expression("refcount", args, 1, 1)
	{}
private:
	variant execute(const formula_callable& variables) const {
		return variant(args()[0]->evaluate(variables).refcount());
	}
};

class loc_function : public function_expression {
public:
	explicit loc_function(const args_list& args)
	  : function_expression("loc", args, 2, 2)
	{}
private:
	variant execute(const formula_callable& variables) const {
		return variant(new location_callable(map_location(args()[0]->evaluate(variables).as_int()-1, args()[1]->evaluate(variables).as_int()-1)));
	}
};

}

variant key_value_pair::get_value(const std::string& key) const
{
	if(key == "key")
	{
		return key_;
	} else
		if(key == "value")
		{
			return value_;
		} else
			return variant();
}

void key_value_pair::get_inputs(std::vector<game_logic::formula_input>* inputs) const {
		inputs->push_back(game_logic::formula_input("key", game_logic::FORMULA_READ_ONLY));
		inputs->push_back(game_logic::formula_input("value", game_logic::FORMULA_READ_ONLY));
}

formula_function_expression::formula_function_expression(const std::string& name, const args_list& args, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& arg_names)
  : function_expression(name, args, arg_names.size(), arg_names.size()),
    formula_(formula), precondition_(precondition), arg_names_(arg_names), star_arg_(-1)
{
	for(size_t n = 0; n != arg_names_.size(); ++n) {
		if(arg_names_.empty() == false && arg_names_[n][arg_names_[n].size()-1] == '*') {
			arg_names_[n].resize(arg_names_[n].size()-1);
			star_arg_ = n;
			break;
		}
	}
}

variant formula_function_expression::execute(const formula_callable& variables) const
{
	static std::string indent;
	indent += "  ";
	DBG_NG << indent << "executing '" << formula_->str() << "'\n";
	const int begin_time = SDL_GetTicks();
	map_formula_callable callable;
	for(size_t n = 0; n != arg_names_.size(); ++n) {
		variant var = args()[n]->evaluate(variables);
		callable.add(arg_names_[n], var);
		if(static_cast<int>(n) == star_arg_) {
			callable.set_fallback(var.as_callable());
		}
	}

	if(precondition_) {
		if(!precondition_->execute(callable).as_bool()) {
			DBG_NG << "FAILED function precondition for function '" << formula_->str() << "' with arguments: ";
			for(size_t n = 0; n != arg_names_.size(); ++n) {
				DBG_NG << "  arg " << (n+1) << ": " << args()[n]->evaluate(variables).to_debug_string() << "\n";
			}
		}
	}

	variant res = formula_->execute(callable);
	const int taken = SDL_GetTicks() - begin_time;
	DBG_NG << indent << "returning: " << taken << "\n";
	indent.resize(indent.size() - 2);

	return res;
}

function_expression_ptr formula_function::generate_function_expression(const std::vector<expression_ptr>& args) const
{
	return function_expression_ptr(new formula_function_expression(name_, args, formula_, precondition_, args_));
}

void function_symbol_table::add_formula_function(const std::string& name, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& args)
{
	custom_formulas_[name] = formula_function(name, formula, precondition, args);
}

expression_ptr function_symbol_table::create_function(const std::string& fn, const std::vector<expression_ptr>& args) const
{
	const std::map<std::string, formula_function>::const_iterator i = custom_formulas_.find(fn);
	if(i != custom_formulas_.end()) {
		return i->second.generate_function_expression(args);
	}

	return expression_ptr();
}

std::vector<std::string> function_symbol_table::get_function_names() const
{
	std::vector<std::string> res;
	for(std::map<std::string, formula_function>::const_iterator iter = custom_formulas_.begin(); iter != custom_formulas_.end(); iter++ ) {
		res.push_back((*iter).first);
	}
	return res;
}

namespace {

class base_function_creator {
public:
	virtual expression_ptr create_function(const std::vector<expression_ptr>& args) const = 0;
	virtual ~base_function_creator() {}
};

template<typename T>
class function_creator : public base_function_creator {
public:
	virtual expression_ptr create_function(const std::vector<expression_ptr>& args) const {
		return expression_ptr(new T(args));
	}
	virtual ~function_creator() {}
};

typedef std::map<std::string, base_function_creator*> functions_map;

functions_map& get_functions_map() {

	static functions_map functions_table;

	if(functions_table.empty()) {
#define FUNCTION(name) functions_table[#name] = new function_creator<name##_function>();
		FUNCTION(dir);
		FUNCTION(if);
		FUNCTION(switch);
		FUNCTION(abs);
		FUNCTION(min);
		FUNCTION(max);
		FUNCTION(choose);
                FUNCTION(debug_float);
		FUNCTION(debug_print);
		FUNCTION(wave);
		FUNCTION(sort);
		FUNCTION(contains_string);
		FUNCTION(filter);
		FUNCTION(find);
		FUNCTION(map);
		FUNCTION(sum);
		FUNCTION(head);
		FUNCTION(size);
		FUNCTION(null);
		FUNCTION(refcount);
		FUNCTION(loc);
		FUNCTION(index_of);
		FUNCTION(keys);
		FUNCTION(values);
		FUNCTION(tolist);
		FUNCTION(tomap);
#undef FUNCTION
	}

	return functions_table;
}

}

expression_ptr create_function(const std::string& fn,
                               const std::vector<expression_ptr>& args,
							   const function_symbol_table* symbols)
{
	if(symbols) {
		expression_ptr res(symbols->create_function(fn, args));
		if(res) {
			return res;
		}
	}

	DBG_NG << "FN: '" << fn << "' " << fn.size() << "\n";

	functions_map::const_iterator i = get_functions_map().find(fn);
	if(i == get_functions_map().end()) {
		throw formula_error("Unknow function: " + fn, "", "", 0);
	}

	return i->second->create_function(args);
}

std::vector<std::string> builtin_function_names()
{
	std::vector<std::string> res;
	const functions_map& m = get_functions_map();
	for(functions_map::const_iterator i = m.begin(); i != m.end(); ++i) {
		res.push_back(i->first);
	}

	return res;
}

}
