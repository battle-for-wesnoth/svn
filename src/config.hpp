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
#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED

//disable the very annoying VC++ warning 4786
#ifdef _WIN32
#pragma warning(disable:4786)
#endif

#include <cassert>
#include <map>
#include <sstream>
#include <string>
#include <vector>

//This module defines the interface to Wesnoth Markup Language (WML).
//WML is a simple hierarchical text-based file format. The format
//is defined in Wiki, under BuildingScenariosWML
//
//All configuration files are stored in this format, and data is
//sent across the network in this format. It is thus used extensively
//throughout the game.

//an object which defines the location an error occurred at when
//parsing WML files
struct line_source
{
	line_source(int ln,const std::string& fname, int line) :
	              linenum(ln), file(fname), fileline(line)
	{}

	int linenum;
	std::string file;
	int fileline;
};

bool operator<(const line_source& a, const line_source& b);

//an exception object used when an IO error occurs
struct io_exception : public std::exception {
	io_exception() {}
	io_exception(const std::string& msg) : message(msg) {}
	virtual ~io_exception() throw() {}

	virtual const char* what() const throw();
private:
	std::string message;
};

//basic disk I/O
std::string read_file(const std::string& fname);
//throws io_exception if an error occurs
void write_file(const std::string& fname, const std::string& data);

struct preproc_define {
	preproc_define() {}
	explicit preproc_define(const std::string& val) : value(val) {}
	preproc_define(const std::string& val, const std::vector<std::string>& args)
		: value(val), arguments(args) {}
	std::string value;
	std::vector<std::string> arguments;
};

typedef std::map<std::string,preproc_define> preproc_map;

//function to use the WML preprocessor on a file, and returns the resulting
//preprocessed file data. defines is a map of symbols defined. src is used
//internally and should be set to NULL
std::string preprocess_file(const std::string& fname,
                            const preproc_map* defines=NULL,
                            std::vector<line_source>* src=NULL);

typedef std::map<std::string,std::string> string_map;

//this object holds the schema by which config objects can be compressed and decompressed.
struct compression_schema
{
	typedef std::map<unsigned char,std::string> char_word_map;
	char_word_map char_to_word;

	typedef std::map<std::string,unsigned char> word_char_map;
	word_char_map word_to_char;
};

//a config object defines a single node in a WML file, with access to
//child nodes.
struct config
{
	//create an empty node.
	config() {}

	//create a node from data
	explicit config(const std::string& data,
	       const std::vector<line_source>* lines=0); //throws config::error
	config(const config& cfg);
	~config();

	config& operator=(const config& cfg);

	//read data in, clobbering existing data.
	void read(const std::string& data,
	          const std::vector<line_source>* lines=0); //throws config::error
	std::string write() const;

	//functions to read and write compressed data. The schema will be created and written
	//with the data. However if you are making successive writes (e.g. a network connection)
	//you can re-use the same schema on the sending end, and the receiver can store the schema,
	//meaning that the entire schema won't have to be transmitted each time.
	std::string write_compressed(compression_schema& schema) const;
	void read_compressed(const std::string& data, compression_schema& schema); //throws config::error

	typedef std::vector<config*> child_list;
	typedef std::map<std::string,child_list> child_map;

	typedef std::vector<config*>::iterator child_iterator;
	typedef std::vector<config*>::const_iterator const_child_iterator;

	typedef std::pair<child_iterator,child_iterator> child_itors;
	typedef std::pair<const_child_iterator,const_child_iterator> const_child_itors;

	child_itors child_range(const std::string& key);
	const_child_itors child_range(const std::string& key) const;

	const child_list& get_children(const std::string& key) const;
	const child_map& all_children() const;

	config* child(const std::string& key);
	const config* child(const std::string& key) const;
	config& add_child(const std::string& key);
	config& add_child(const std::string& key, const config& val);
	std::string& operator[](const std::string& key);
	const std::string& operator[](const std::string& key) const;

	config* find_child(const std::string& key, const std::string& name,
	                   const std::string& value);
	const config* find_child(const std::string& key, const std::string& name,
	                         const std::string& value) const;

	void clear_children(const std::string& key);
	config* remove_child(const std::string& key, size_t index);

	static std::vector<std::string> split(const std::string& val, char c=',', bool remove_empty=true);
	static std::vector<std::string> quoted_split(const std::string& val, char c=',',
												 bool remove_empty=true, char quote='\\');
	static std::pair<int,int> parse_range(const std::string& str);
	static std::string& escape(std::string& str);
	static std::string& unescape(std::string& str);
	static std::string& strip(std::string& str);
	static bool has_value(const std::string& values, const std::string& val);

	static std::string interpolate_variables_into_string(const std::string& str, const string_map& symbols);

	void clear();
	bool empty() const;

	struct error {
		error(const std::string& msg) : message(msg) {}
		std::string message;
	};

	struct child_pos {
		child_pos(child_map::const_iterator p, size_t i) : pos(p), index(i) {}
		child_map::const_iterator pos;
		size_t index;

		bool operator==(const child_pos& o) const { return pos == o.pos && index == o.index; }
		bool operator!=(const child_pos& o) const { return !operator==(o); }
	};

	struct all_children_iterator {
		typedef std::vector<child_pos>::const_iterator Itor;
		explicit all_children_iterator(Itor i);

		all_children_iterator operator++();
		all_children_iterator operator++(int);

		std::pair<const std::string*,const config*> operator*() const;

		bool operator==(all_children_iterator i) const;
		bool operator!=(all_children_iterator i) const;

	private:
		Itor i_;
	};

	//in-order iteration over all children
	all_children_iterator ordered_begin() const;
	all_children_iterator ordered_end() const;

	//a function to get the differences between this object, and 'c', as another config
	//object. i.e. calling cfg2.apply_diff(cfg1.get_diff(cfg2)) will make cfg1 identical
	//to cfg2.
	config get_diff(const config& c) const;

	void apply_diff(const config& diff); //throw error

	//all the attributes of this node.
	string_map values;

private:
	size_t write_size() const;
	std::string::iterator write_internal(std::string::iterator out) const;
	std::string::const_iterator read_compressed_internal(std::string::const_iterator i1, std::string::const_iterator i2, compression_schema& schema);
	void write_compressed_internal(compression_schema& schema, std::vector<char>& res) const;

	//a list of all children of this node.
	child_map children;

	std::vector<child_pos> ordered_children;
};

bool operator==(const config& a, const config& b);
bool operator!=(const config& a, const config& b);

struct config_has_value {
	config_has_value(const std::string& name, const std::string& value)
	              : name_(name), value_(value)
	{}

	bool operator()(const config* cfg) const { return (*cfg)[name_] == value_; }

private:
	const std::string name_, value_;
};

//an object which wraps around a config object and interpolates
//variables into all strings it returns
struct config_interpolater {
	config_interpolater(const config& cfg, const string_map& m) : cfg_(cfg), map_(m)
	{}

	std::string operator[](const std::string& str) const
	{
		return config::interpolate_variables_into_string(cfg_[str],map_);
	}

private:
	const config& cfg_;
	const string_map& map_;
};

#endif
