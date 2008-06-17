#include "global.hpp"

#include <cmath>
#include <set>
#include <stdlib.h>
#include <iostream>
#include <string.h>

#include "boost/lexical_cast.hpp"

#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

namespace {
std::string variant_type_to_string(variant::TYPE type) {
	switch(type) {
	case variant::TYPE_NULL: 
		return "null";
	case variant::TYPE_INT: 
		return "int";
	case variant::TYPE_CALLABLE: 
		return "object";
	case variant::TYPE_LIST: 
		return "list";
	case variant::TYPE_STRING: 
		return "string";
	case variant::TYPE_MAP: 
		return "map";
	default:
		assert(false);
		return "invalid";
	}
}

std::vector<const char*> call_stack;
}

void push_call_stack(const char* str)
{
	call_stack.push_back(str);
}

void pop_call_stack()
{
	call_stack.pop_back();
}

std::string get_call_stack()
{
	std::string res;
	for(std::vector<const char*>::const_iterator i = call_stack.begin();
	    i != call_stack.end(); ++i) {
		if(!*i) {
			continue;
		}
		res += "  ";
		res += *i;
		res += "\n";
	}
	return res;
}

type_error::type_error(const std::string& str) : message(str) {
	std::cerr << "ERROR: " << message << "\n" << get_call_stack();
}

struct variant_list {
	variant_list() : refcount(0)
	{}
	std::vector<variant> elements;
	int refcount;
};

struct variant_string {
	variant_string() : refcount(0)
	{}
	std::string str;
	int refcount;
};

struct variant_map {
	variant_map() : refcount(0)
	{}
	std::map<variant,variant> elements;
	int refcount;
};

void variant::increment_refcount()
{
	switch(type_) {
	case TYPE_LIST:
		++list_->refcount;
		break;
	case TYPE_STRING:
		++string_->refcount;
		break;
	case TYPE_MAP:
		++map_->refcount;
		break;
	case TYPE_CALLABLE:
		intrusive_ptr_add_ref(callable_);
		break;

	// These are not used here, add them to silence a compiler warning.
	case TYPE_NULL:
	case TYPE_INT :
		break;
	}
}

void variant::release()
{
	switch(type_) {
	case TYPE_LIST:
		if(--list_->refcount == 0) {
			delete list_;
		}
		break;
	case TYPE_STRING:
		if(--string_->refcount == 0) {
			delete string_;
		}
		break;
	case TYPE_MAP:
		if(--map_->refcount == 0) {
			delete map_;
		}
		break;
	case TYPE_CALLABLE:
		intrusive_ptr_release(callable_);
		break;

	// These are not used here, add them to silence a compiler warning.
	case TYPE_NULL:
	case TYPE_INT :
		break;
	}
}

variant::variant() : type_(TYPE_NULL), int_value_(0)
{}

variant::variant(int n) : type_(TYPE_INT), int_value_(n)
{}

variant::variant(const game_logic::formula_callable* callable)
	: type_(TYPE_CALLABLE), callable_(callable)
{
	assert(callable_);
	increment_refcount();
}

variant::variant(std::vector<variant>* array)
    : type_(TYPE_LIST)
{
	assert(array);
	list_ = new variant_list;
	list_->elements.swap(*array);
	increment_refcount();
}

variant::variant(const std::string& str)
	: type_(TYPE_STRING)
{
	string_ = new variant_string;
	string_->str = str;
	increment_refcount();
}

variant::variant(std::map<variant,variant>* map)
    : type_(TYPE_MAP)
{
	assert(map);
	map_ = new variant_map;
	map_->elements.swap(*map);
	increment_refcount();
}

variant::variant(const variant& v)
{
	memcpy(this, &v, sizeof(v));
	increment_refcount();
}

variant::~variant()
{
	release();
}

const variant& variant::operator=(const variant& v)
{
	if(&v != this) {
		release();
		memcpy(this, &v, sizeof(v));
		increment_refcount();
	}
	return *this;
}

const variant& variant::operator[](size_t n) const
{
	if(type_ == TYPE_CALLABLE) {
		assert(n == 0);
		return *this;
	}

	must_be(TYPE_LIST);
	assert(list_);
	if(n >= list_->elements.size()) {
		throw type_error("invalid index");
	}

	return list_->elements[n];
}

const variant& variant::operator[](const variant v) const
{
	if(type_ == TYPE_CALLABLE) {
		assert(v.as_int() == 0);
		return *this;
	}

	if(type_ == TYPE_MAP) {
		assert(map_);
		std::map<variant,variant>::const_iterator i = map_->elements.find(v);
		if (i == map_->elements.end())
		{
			static variant null_variant;
			return null_variant;
		}
		return i->second;
	} else if(type_ == TYPE_LIST) {
		return operator[](v.as_int());
	} else {
		throw type_error((formatter() << "type error: " 
			<< " expected a list or a map but found " 
			<< variant_type_to_string(type_) 
			<< " (" << to_debug_string() << ")").str());
	}	
}

std::map<variant, variant> variant::get_map() const 
{  
	must_be(TYPE_MAP); 
	return map_->elements; 
}

variant variant::get_keys() const
{
	must_be(TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->first);
	}
	return variant(&tmp);
}

variant variant::get_values() const
{
	must_be(TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->second);
	}
	return variant(&tmp);
}

size_t variant::num_elements() const
{
	if(type_ == TYPE_CALLABLE) {
		return 1;
	}

	if (type_ == TYPE_LIST) {
		assert(list_);
		return list_->elements.size();
	} else if (type_ == TYPE_MAP) {
		assert(map_);
		return map_->elements.size();
	} else {
		throw type_error((formatter() << "type error: " 
			<< " expected a list or a map but found " 
			<< variant_type_to_string(type_) 
			<< " (" << to_debug_string() << ")").str());
	}
}

variant variant::get_member(const std::string& str) const
{
	if(is_callable()) {
		return callable_->query_value(str);
	}

	if(str == "self") {
		return *this;
	} else {
		return variant();
	}
}

bool variant::as_bool() const
{
	switch(type_) {
	case TYPE_NULL:
		return false;
	case TYPE_INT:
		return int_value_ != 0;
	case TYPE_CALLABLE:
		return callable_ != NULL;
	case TYPE_LIST:
		return !list_->elements.empty();
	case TYPE_MAP:
		return !map_->elements.empty();
	case TYPE_STRING:
		return !string_->str.empty();
	default:
		assert(false);
		return false;
	}
}

const std::string& variant::as_string() const
{
	must_be(TYPE_STRING);
	assert(string_);
	return string_->str;
}

variant variant::operator+(const variant& v) const
{
	if(type_ == TYPE_LIST) {
		if(v.type_ == TYPE_LIST) {
			std::vector<variant> res;
			res.reserve(list_->elements.size() + v.list_->elements.size());
			for(size_t i = 0; i<list_->elements.size(); ++i) {
				const variant& var = list_->elements[i];
				res.push_back(var);
			}

			for(size_t j = 0; j<v.list_->elements.size(); ++j) {
				const variant& var = v.list_->elements[j];
				res.push_back(var);
			}

			return variant(&res);
		}
	}
	if(type_ == TYPE_MAP) {
		if(v.type_ == TYPE_MAP) {
			std::map<variant,variant> res(map_->elements);

			for(std::map<variant,variant>::const_iterator i = v.map_->elements.begin(); i != v.map_->elements.end(); ++i) {
				res[i->first] = i->second;
			}

			return variant(&res);
		}
	}

	return variant(as_int() + v.as_int());
}

variant variant::operator-(const variant& v) const
{
	return variant(as_int() - v.as_int());
}

variant variant::operator*(const variant& v) const
{
	return variant(as_int() * v.as_int());
}

variant variant::operator/(const variant& v) const
{
	const int numerator = as_int();
	const int denominator = v.as_int();
	if(denominator == 0) {
		throw type_error((formatter() << "divide by zero error").str());;
	}

	return variant(numerator/denominator);
}

variant variant::operator%(const variant& v) const
{
	const int numerator = as_int();
	const int denominator = v.as_int();
	if(denominator == 0) {
		throw type_error((formatter() << "divide by zero error").str());
	}

	return variant(numerator%denominator);
}

variant variant::operator^(const variant& v) const
{
	return variant(static_cast<int>(pow(as_int(), v.as_int())));
}

variant variant::operator-() const
{
	return variant(-as_int());
}

bool variant::operator==(const variant& v) const
{
	if(type_ != v.type_) {
		return false;
	}

	switch(type_) {
	case TYPE_NULL: {
		return v.is_null();
	}

	case TYPE_STRING: {
		return string_->str == v.string_->str;
	}

	case TYPE_INT: {
		return int_value_ == v.int_value_;
	}

	case TYPE_LIST: {
		if(num_elements() != v.num_elements()) {
			return false;
		}

		for(size_t n = 0; n != num_elements(); ++n) {
			if((*this)[n] != v[n]) {
				return false;
			}
		}

		return true;
	}

	case TYPE_MAP: {
		return map_->elements == v.map_->elements;
	}

	case TYPE_CALLABLE: {
		return callable_->equals(v.callable_);
	}
	}

	assert(false);
	return false;
}

bool variant::operator!=(const variant& v) const
{
	return !operator==(v);
}

bool variant::operator<=(const variant& v) const
{
	if(type_ != v.type_) {
		return type_ < v.type_;
	}

	switch(type_) {
	case TYPE_NULL: {
		return true;
	}

	case TYPE_STRING: {
		return string_->str <= v.string_->str;
	}

	case TYPE_INT: {
		return int_value_ <= v.int_value_;
	}

	case TYPE_LIST: {
		for(size_t n = 0; n != num_elements() && n != v.num_elements(); ++n) {
			if((*this)[n] < v[n]) {
				return true;
			} else if((*this)[n] > v[n]) {
				return false;
			}
		}

		return num_elements() <= v.num_elements();
	}

	case TYPE_MAP: {
		return map_->elements <= v.map_->elements;
	}

	case TYPE_CALLABLE: {
		return !v.callable_->less(callable_);
	}
	}

	assert(false);
	return false;
}

bool variant::operator>=(const variant& v) const
{
	return v <= *this;
}

bool variant::operator<(const variant& v) const
{
	return !(*this >= v);
}

bool variant::operator>(const variant& v) const
{
	return !(*this <= v);
}

void variant::must_be(variant::TYPE t) const
{
	if(type_ != t) {
		throw type_error((formatter() << "type error: " << " expected " 
			<< variant_type_to_string(t) << " but found " 
			<< variant_type_to_string(type_) 
			<< " (" << to_debug_string() << ")").str());
	}
}

void variant::serialize_to_string(std::string& str) const
{
	switch(type_) {
	case TYPE_NULL:
		str += "null()";
	case TYPE_INT:
		str += boost::lexical_cast<std::string>(int_value_);
		break;
	case TYPE_CALLABLE:
		callable_->serialize(str);
		break;
	case TYPE_LIST: {
		str += "[";
		bool first_time = true;
		for(size_t i=0; i<list_->elements.size(); ++i) {
			const variant& var = list_->elements[i];
			if(!first_time) {
				str += ",";
			}
			first_time = false;
			var.serialize_to_string(str);
		}
		str += "]";
		break;
	}
	case TYPE_MAP: {
		str += "{";
		bool first_time = true;
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!first_time) {
				str += ",";
			}
			first_time = false;
			i->first.serialize_to_string(str);
			str += "->";
			i->second.serialize_to_string(str);
		}
		str += "}";
		break;
	}
	case TYPE_STRING:
		str += "'";
		str += string_->str;
		str += "'";
		break;
	default:
		assert(false);
	}
}

void variant::serialize_from_string(const std::string& str)
{
	try {
		*this = game_logic::formula(str).execute();
	} catch(...) {
		*this = variant(str);
	}
}

int variant::refcount() const
{
	switch(type_) {
	case TYPE_LIST:
		return list_->refcount;
		break;
	case TYPE_STRING:
		return string_->refcount;
		break;
	case TYPE_MAP:
		return map_->refcount;
		break;
	case TYPE_CALLABLE:
		return callable_->refcount();
		break;
	default:
		return -1;
	}
}

std::string variant::string_cast() const
{
	switch(type_) {
	case TYPE_NULL:
		return "0";
	case TYPE_INT:
		return boost::lexical_cast<std::string>(int_value_);
	case TYPE_CALLABLE:
		return "(object)";
	case TYPE_LIST: {
		std::string res = "";
		for(size_t i=0; i<list_->elements.size(); ++i) {
			const variant& var = list_->elements[i];
			if(!res.empty()) {
				res += ", ";
			}

			res += var.string_cast();
		}

		return res;
	}
	case TYPE_MAP: {
		std::string res = "";
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!res.empty()) {
				res += ",";
			}
			res += i->first.string_cast();
			res += "->";
			res += i->second.string_cast();
		}
		return res;
	}

	case TYPE_STRING:
		return string_->str;
	default:
		assert(false);
		return "invalid";
	}
}

std::string variant::to_debug_string(std::vector<const game_logic::formula_callable*>* seen) const
{
	std::vector<const game_logic::formula_callable*> seen_stack;
	if(!seen) {
		seen = &seen_stack;
	}

	std::ostringstream s;
	switch(type_) {
	case TYPE_NULL:
		s << "(null)";
	case TYPE_INT:
		s << int_value_;
		break;
	case TYPE_LIST: {
		s << "[";
		for(size_t n = 0; n != num_elements(); ++n) {
			if(n != 0) {
				s << ", ";
			}

			s << operator[](n).to_debug_string(seen);
		}
		s << "]";
		break;
	}
	case TYPE_CALLABLE: {
		s << "{";
		if(std::find(seen->begin(), seen->end(), callable_) == seen->end()) {
			seen->push_back(callable_);
			std::vector<game_logic::formula_input> v = callable_->inputs();
			bool first = true;
			for(size_t i=0; i<v.size(); ++i) {
				const game_logic::formula_input& input = v[i];
				if(!first) {
					s << ", ";
				}
				first = false;
				s << input.name << " ";
				if(input.access == game_logic::FORMULA_READ_WRITE) {
					s << "(read-write) ";
				} else if(input.access == game_logic::FORMULA_WRITE_ONLY) {
					s << "(writeonly) ";
				}

				s << "-> " << callable_->query_value(input.name).to_debug_string(seen);
			}
		} else {
			s << "...";
		}
		s << "}";
		break;
	}
	case TYPE_MAP: {
		s << "{";
		bool first_time = true;
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!first_time) {
				s << ",";
			}
			first_time = false;
			s << i->first.to_debug_string(seen);
			s << "->";
			s << i->second.to_debug_string(seen);
		}
		s << "}";
		break;
	}
	case TYPE_STRING: {
		s << "'" << string_->str << "'";
		break;
	}
	}

	return s.str();
}
