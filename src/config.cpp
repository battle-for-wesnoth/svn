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
#include <cctype>
#include <fstream>
#include <iostream>
#include <stack>
#include <sstream>
#include <vector>

#include "config.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "game_events.hpp"
#include "log.hpp"
#include "scoped_resource.hpp"

bool operator<(const line_source& a, const line_source& b)
{
	return a.linenum < b.linenum;
}

namespace {

bool isnewline(char c)
{
	return c == '\r' || c == '\n';
}

//make sure that we can use Mac, DOS, or Unix style text files on any system
//and they will work, by making sure the definition of whitespace is consistent
bool portable_isspace(char c)
{
	return isnewline(c) || isspace(c);
}


line_source get_line_source(const std::vector<line_source>& line_src, int line)
{
	line_source res(line,"",0);
	std::vector<line_source>::const_iterator it =
	           std::upper_bound(line_src.begin(),line_src.end(),res);
	if(it != line_src.begin()) {
		--it;
		res.file = it->file;
		res.fileline = it->fileline + (line - it->linenum);
	}

	return res;
}

struct close_FILE
{
	void operator()(FILE* f) const { if(f != NULL) { fclose(f); } }
};

void read_file_internal(const std::string& fname, std::string& res)
{
	res.resize(0);

	const util::scoped_resource<FILE*,close_FILE> file(fopen(fname.c_str(),"rb"));
	if(file == NULL) {
		return;
	}

	const int block_size = 4096;
	char buf[block_size];

	for(;;) {
		size_t nbytes = fread(buf,1,block_size,file);
		res.resize(res.size()+nbytes);
		std::copy(buf,buf+nbytes,res.end()-nbytes);

		if(nbytes < block_size) {
			return;
		}
	}
}

} //end anon namespace

const char* io_exception::what() const throw() {
	return message.c_str();
}

std::string read_file(const std::string& fname)
{
	//if we have a path to the data,
	//convert any filepath which is relative
	if(!fname.empty() && fname[0] != '/' && !game_config::path.empty()) {
		std::string res;
		read_file_internal(game_config::path + "/" + fname,res);
		if(!res.empty()) {
			return res;
		}
	}

	std::string res;
	read_file_internal(fname,res);
	return res;
}

//throws io_exception if an error occurs
void write_file(const std::string& fname, const std::string& data)
{
	std::ofstream file(fname.c_str());
	if(!file.good()) {
		std::cerr << "error writing to file: '" << fname << "'\n";
		throw io_exception("Error writing to file: " + fname);
	}
	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		file << *i;
	}
}

namespace {

void internal_preprocess_file(const std::string& fname,
                              preproc_map& defines_map,
                              int depth, std::vector<char>& res,
                              std::vector<line_source>* lines_src, int& line);

void internal_preprocess_data(const std::string& data,
                              preproc_map& defines_map,
                              int depth, std::vector<char>& res,
                              std::vector<line_source>* lines_src, int& line,
							  const std::string& fname, int srcline)
{
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

			const std::string newfilename = newfile.str();
			std::vector<std::string> items = config::split(newfilename,' ');
			const std::string symbol = items.front();

			//if this is a known pre-processing symbol, then we insert
			//it, otherwise we assume it's a file name to load
			if(defines_map.count(symbol) != 0) {
				items.erase(items.begin());

				const preproc_define& val = defines_map[symbol];
				if(val.arguments.size() != items.size()) {
					std::cerr << "error: preprocessor symbol '" << symbol << "' has "
					          << items.size() << " arguments, "
							  << val.arguments.size() << " expected\n";
				}

				std::string str = val.value;

				//substitute in given arguments
				for(size_t n = 0; n != val.arguments.size(); ++n) {
					const std::string& replace_with = (n < items.size()) ? items[n] : "";

					int subs = 0;

					const std::string item = "{" + val.arguments[n] + "}";
					std::string::size_type pos = str.find(item);
					while(pos != std::string::npos) {
						++subs;
						str.replace(pos,item.size(),replace_with);
						const std::string::size_type new_pos = str.find(item);
						if(new_pos < pos+replace_with.size()) {
							std::cerr << "macro substitution in symbol '" << symbol
								      << "' could lead to infinite recursion. Aborting.\n";
							break;
						}

						pos = new_pos;
					}
				}

				internal_preprocess_data(str,defines_map,depth,res,NULL,line,fname,srcline);
			} else if(depth < 20) {
				internal_preprocess_file("data/" + newfilename,
				                       defines_map, depth+1,res,
				                       lines_src,line);
			} else {
				const std::string& str = read_file(newfilename);
				res.insert(res.end(),str.begin(),str.end());
				line += std::count(str.begin(),str.end(),'\n');
			}

			if(lines_src != NULL) {
				lines_src->push_back(line_source(line,fname,srcline));
			}
		} else if(c == '#' && !in_quotes) {
			//we are about to skip some things, so keep track of
			//the start of where we're skipping, so we can count
			//the number of newlines, so we can track the line number
			//in the source file
			const std::string::const_iterator begin = i;

			//if this is the beginning of a pre-processing definition
			static const std::string hash_define("#define");
			if(size_t(data.end() - i) > hash_define.size() &&
			   std::equal(hash_define.begin(),hash_define.end(),i)) {

				i += hash_define.size();

				i = std::find_if(i,data.end(),isgraph);

				const std::string::const_iterator end = std::find_if(i,data.end(),isnewline);

				if(end == data.end())
					break;

				const std::string items(i,end);
				std::vector<std::string> args = config::split(items,' ');
				const std::string symbol = args.front();
				args.erase(args.begin());

				std::stringstream value;
				static const std::string hash_enddef("#enddef");
				for(i = end+1; i <= data.end() - hash_enddef.size(); ++i) {
					if(std::equal(hash_enddef.begin(),hash_enddef.end(),i)) {
						break;
					}

					value << *i;
				}

				if(i > data.end() - hash_enddef.size()) {
					throw config::error("pre-processing condition unterminated in '" + fname + "': '" + items + "'");
				}

				i += hash_enddef.size();

				defines_map.insert(std::pair<std::string,preproc_define>(
				                    symbol,preproc_define(value.str(),args)));
			}

			//if this is a pre-processing conditional
			static const std::string hash_ifdef("#ifdef");
			static const std::string hash_else("#else");
			static const std::string hash_endif("#endif");

			if(size_t(data.end() - i) > hash_ifdef.size() &&
			   std::equal(hash_ifdef.begin(),hash_ifdef.end(),i)) {
				i += hash_ifdef.size();
				while(i != data.end() && portable_isspace(*i))
					++i;

				const std::string::const_iterator end = std::find_if(i,data.end(),portable_isspace);

				if(end == data.end())
					break;

				//if the symbol is not defined, then we want to skip
				//to the #endif or #else . Otherwise, continue processing
				//as normal. The #endif will just be treated as a comment
				//anyway.
				const std::string symbol(i,end);
				if(defines_map.count(symbol) == 0) {
					while(size_t(data.end() - i) > hash_endif.size() &&
					      !std::equal(hash_endif.begin(),hash_endif.end(),i) &&
					      !std::equal(hash_else.begin(),hash_else.end(),i)) {
						++i;
					}

					i = std::find_if(i,data.end(),isnewline);
					if(i == data.end())
						break;
				} else {
					i = end;
				}
			}

			//if we come across a #else, it must mean that we found a #ifdef
			//earlier, and we should ignore until #endif
			if(size_t(data.end() - i) > hash_else.size() &&
			   std::equal(hash_else.begin(),hash_else.end(),i)) {
				while(size_t(data.end() - i) > hash_endif.size() &&
				      !std::equal(hash_endif.begin(),hash_endif.end(),i)) {
					++i;
				}

				i = std::find_if(i,data.end(),isnewline);
				if(i == data.end())
					break;
			}

			i = std::find_if(i,data.end(),isnewline);

			if(i == data.end())
				break;

			srcline += std::count(begin,i,'\n');
			++line;

			res.push_back('\n');
		} else {
			if(c == '\n') {
				++line;
				++srcline;
			}

			res.push_back(c);
		}
	}
}

void internal_preprocess_file(const std::string& fname,
                              preproc_map& defines_map,
                              int depth, std::vector<char>& res,
                              std::vector<line_source>* lines_src, int& line)
{
	//if it's a directory, we process all files in the directory
	//that end in .cfg
	if(is_directory(fname)) {

		std::vector<std::string> files;
		get_files_in_dir(fname,&files,NULL,ENTIRE_FILE_PATH);

		for(std::vector<std::string>::const_iterator f = files.begin();
		    f != files.end(); ++f) {
			if(is_directory(*f) || (f->size() > 4 && std::equal(f->end()-4,f->end(),".cfg"))) {
				internal_preprocess_file(*f,defines_map,depth,res,
				                         lines_src,line);
			}
		}

		return;
	}

	if(lines_src != NULL) {
		lines_src->push_back(line_source(line,fname,1));
	}

	internal_preprocess_data(read_file(fname),defines_map,depth,res,lines_src,line,fname,1);
}

} //end anonymous namespace

std::string preprocess_file(const std::string& fname,
                            const preproc_map* defines,
                            std::vector<line_source>* line_sources)
{
	log_scope("preprocessing file...");
	preproc_map defines_copy;
	if(defines != NULL)
		defines_copy = *defines;

	std::vector<char> res;
	int linenum = 0;
	internal_preprocess_file(fname,defines_copy,0,res,line_sources,linenum);
	return std::string(res.begin(),res.end());
}

config::config(const std::string& data,
               const std::vector<line_source>* line_sources)
{
	log_scope("parsing config...");
	read(data,line_sources);
}

config::config(const config& cfg) : values(cfg.values)
{
	for(all_children_iterator j = cfg.ordered_begin(); j != cfg.ordered_end(); ++j) {
		add_child(*(*j).first,*(*j).second);
	}
}

config::~config()
{
	clear();
}

config& config::operator=(const config& cfg)
{
	if(this == &cfg) {
		return *this;
	}

	clear();

	values = cfg.values;

	for(all_children_iterator j = cfg.ordered_begin(); j != cfg.ordered_end(); ++j) {
		add_child(*(*j).first,*(*j).second);
	}

	return *this;
}

void config::read(const std::string& data,
                  const std::vector<line_source>* line_sources)
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

	bool in_quotes = false, has_quotes = false, in_comment = false;

	int line = 0;

	for(std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
		const char c = *i;
		if(c == '\r') //ignore any DOS-style newlines
			continue;

		if(c == '\n') {
			in_comment = false;
			++line;
		}

		if(*i == '#' && !in_quotes) {
			in_comment = true;
		}

		if(in_comment) {
			continue;
		}

		switch(state) {
			case ELEMENT_NAME:
				if(c == ']') {
					if(value == "end" || !value.empty() && value[0] == '/') {
						assert(!elements.empty());

						if(value[0] == '/' &&
						   std::string("/" + element_names.top()) != value) {
							std::stringstream err;

							if(line_sources != NULL) {
								const line_source src = get_line_source(*line_sources,line);

								err << src.file << " " << src.fileline << ": ";
							} else {
								err << "line " << line << ": ";
							}

							err << "Found illegal end tag: '" << value
							    << "', at end of '"
							    << element_names.top() << "'";

							throw error(err.str());
						}

						elements.pop();
						element_names.pop();

						if(elements.empty()) {
							std::stringstream err;

							if(line_sources != NULL) {
								const line_source src =
								        get_line_source(*line_sources,line);

								err << src.file << " " << src.fileline << ": ";
							}

							err << "Unexpected terminating tag\n";
							throw error(err.str());
							return;
						}


						state = IN_ELEMENT;

						break;
					}

					elements.push(&elements.top()->add_child(value));
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
				} else if(!portable_isspace(c)) {
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
				if(c == '[') {
					if(line_sources != NULL) {
						const line_source src = get_line_source(*line_sources,line);
						std::cerr << src.file << " " << src.fileline << ": ";
					}

					std::cerr << "WARNING: square bracket found in string. Is this a run-away string?\n";
				}
				
				if(c == '"') {
					in_quotes = !in_quotes;
					has_quotes = true;
				} else if(c == '\n' && !in_quotes) {
					state = IN_ELEMENT;
					elements.top()->values[var] = has_quotes ? value : strip(value);
					var = "";
					value = "";
					has_quotes = false;
				} else if(in_quotes || !has_quotes) {
					value.resize(value.size()+1);
					value[value.size()-1] = c;
				}

				break;
		}
	}

	assert(!element_names.empty());
	const std::string top = element_names.top();
	element_names.pop();
	if(!element_names.empty()) {
		throw error("Configuration not terminated: no closing tag to '" +
		            top + "'");
	}
}

namespace {
	const std::string AttributeEquals = "=\"";
	const std::string AttributePostfix = "\"\n";
	const std::string ElementPrefix = "[";
	const std::string ElementPostfix = "]\n";
	const std::string EndElementPrefix = "[/";
	const std::string EndElementPostfix = "]\n";
	const std::string ConfigPostfix = "\n";
}

size_t config::write_size() const
{
	size_t res = 0;
	for(string_map::const_iterator i = values.begin(); i != values.end(); ++i) {
		if(i->second.empty() == false) {
			res += i->first.size() + AttributeEquals.size() +
			       i->second.size() + AttributePostfix.size();
		}
	}

	for(all_children_iterator j = ordered_begin(); j != ordered_end(); ++j) {
		const std::pair<const std::string*,const config*>& item = *j;
		const std::string& name = *item.first;
		const config& cfg = *item.second;
		res += ElementPrefix.size() + name.size() + ElementPostfix.size() +
		       cfg.write_size() + EndElementPrefix.size() + name.size() + EndElementPostfix.size();
		
	}

	res += ConfigPostfix.size();

	return res;
}

std::string::iterator config::write_internal(std::string::iterator out) const
{
	for(std::map<std::string,std::string>::const_iterator i = values.begin();
					i != values.end(); ++i) {
		if(i->second.empty() == false) {
			out = std::copy(i->first.begin(),i->first.end(),out);
			out = std::copy(AttributeEquals.begin(),AttributeEquals.end(),out);
			out = std::copy(i->second.begin(),i->second.end(),out);
			out = std::copy(AttributePostfix.begin(),AttributePostfix.end(),out);
		}
	}

	for(all_children_iterator j = ordered_begin(); j != ordered_end(); ++j) {
		const std::pair<const std::string*,const config*>& item = *j;
		const std::string& name = *item.first;
		const config& cfg = *item.second;

		out = std::copy(ElementPrefix.begin(),ElementPrefix.end(),out);
		out = std::copy(name.begin(),name.end(),out);
		out = std::copy(ElementPostfix.begin(),ElementPostfix.end(),out);
		out = cfg.write_internal(out);
		out = std::copy(EndElementPrefix.begin(),EndElementPrefix.end(),out);
		out = std::copy(name.begin(),name.end(),out);
		out = std::copy(EndElementPostfix.begin(),EndElementPostfix.end(),out);
	}

	out = std::copy(ConfigPostfix.begin(),ConfigPostfix.end(),out);
	return out;
}

std::string config::write() const
{
	log_scope("config::write");

	std::string res;

	res.resize(write_size());

	const std::string::iterator i = write_internal(res.begin());
	assert(i == res.end());
	if(i != res.end()) {
		std::cerr << "ERROR in size of config buffer: " << (i - res.begin()) << "/" << res.size() << "\n";
	}

	return res;
}

//data compression. Compression is designed for network traffic.
//assumptions compression is based on:
// - most space is taken up by element names and attribute names
// - there are relatively few element names and attribute names that are repeated many times
//
//how it works: there are some control characters:
// 'compress_open_element': signals that the next word found is an element.
// any words found that are not after this are assumed to be attributes
// 'compress_close_element': signals to close the current element
// 'compress_schema_item': signals that following is a nul-delimited string, which should
//                         be added as a word in the schema
// 'compress_literal_word': signals that following is a word stored as a nul-delimited string
//    (an attribute name, unless it was preceeded by 'compress_open_element')
//
// all other characters are mapped to words. When an item is inserted into the schema,
// it is mapped to the first available character. Any attribute found is always followed
// by a nul-delimited string which is the value for the attribute.
//
// the schema objects are designed to be persisted. That is, in a network game, both peers
// can store their schema objects, and so rather than sending schema data each time, the peers
// use and build their schemas as the game progresses, adding a new word to the schema anytime
// it is required.
namespace {
	const unsigned char compress_open_element = 0, compress_close_element = 1,
	                    compress_schema_item = 2, compress_literal_word = 3,
						compress_first_word = 4, compress_last_word = 255;
	const size_t compress_max_words = compress_last_word - compress_first_word + 1;

	void compress_output_literal_word(const std::string& word, std::vector<char>& output)
	{
		output.resize(output.size() + word.size());
		std::copy(word.begin(),word.end(),output.end()-word.size());
		output.push_back(char(0));
	}

	compression_schema::word_char_map::const_iterator add_word_to_schema(const std::string& word, compression_schema& schema)
	{
		const unsigned char c = compress_first_word + schema.word_to_char.size();
			
		std::cerr << "inserting in schema: " << int(c) << " -> '" << word << "'\n";

		schema.char_to_word.insert(std::pair<unsigned char,std::string>(c,word));
		return schema.word_to_char.insert(std::pair<std::string,unsigned char>(word,c)).first;
	}

	compression_schema::word_char_map::const_iterator get_word_in_schema(const std::string& word, compression_schema& schema, std::vector<char>& output)
	{
		//see if this word is already in the schema
		const compression_schema::word_char_map::const_iterator w = schema.word_to_char.find(word);
		if(w != schema.word_to_char.end()) {
			//in the schema. Return it
			return w;
		} else if(schema.word_to_char.size() < compress_max_words) {
			//we can add the word to the schema

			//we insert the code to add a schema item, followed by the zero-delimited word
			output.push_back(char(compress_schema_item));
			compress_output_literal_word(word,output);

			return add_word_to_schema(word,schema);
		} else {
			//it's not there, and there's no room to add it
			std::cerr << "no room for word '" << word << "' in schema\n";
			return schema.word_to_char.end();
		}
	}

	void compress_emit_word(const std::string& word, compression_schema& schema, std::vector<char>& res)
	{
		//get the word in the schema
		const compression_schema::word_char_map::const_iterator w = get_word_in_schema(word,schema,res);
		if(w != schema.word_to_char.end()) {
			//the word is in the schema, all we have to do is output the compression code for it.
			res.push_back(w->second);
		} else {
			//the word is not in the schema. Output it as a literal word
			res.push_back(char(compress_literal_word));
			compress_output_literal_word(word,res);
		}
	}

	std::string::const_iterator compress_read_literal_word(std::string::const_iterator i1, std::string::const_iterator i2, std::string& res)
	{
		const std::string::const_iterator end_word = std::find(i1,i2,0);
		if(end_word == i2) {
			throw config::error("Unexpected end of data in compressed config read\n");
		}

		res = std::string(i1,end_word);
		return end_word;
	}
}

void config::write_compressed_internal(compression_schema& schema, std::vector<char>& res) const
{
	for(std::map<std::string,std::string>::const_iterator i = values.begin();
					i != values.end(); ++i) {
		if(i->second.empty() == false) {
			
			//output the name, using compression
			compress_emit_word(i->first,schema,res);

			//output the value, with no compression
			compress_output_literal_word(i->second,res);
		}
	}

	for(all_children_iterator j = ordered_begin(); j != ordered_end(); ++j) {
		const std::pair<const std::string*,const config*>& item = *j;
		const std::string& name = *item.first;
		const config& cfg = *item.second;

		res.push_back(compress_open_element);
		compress_emit_word(name,schema,res);
		cfg.write_compressed_internal(schema,res);
		res.push_back(compress_close_element);
	}
}

std::string config::write_compressed(compression_schema& schema) const
{
	std::vector<char> res;
	write_compressed_internal(schema,res);
	std::string s;
	s.resize(res.size());
	std::copy(res.begin(),res.end(),s.begin());
	return s;
}

std::string::const_iterator config::read_compressed_internal(std::string::const_iterator i1, std::string::const_iterator i2, compression_schema& schema)
{
	bool in_open_element = false;
	while(i1 != i2) {
		switch(*i1) {
		case compress_open_element:
			in_open_element = true;
			break;
		case compress_close_element:
			return i1;
		case compress_schema_item: {
			std::string word;
			i1 = compress_read_literal_word(i1+1,i2,word);

			add_word_to_schema(word,schema);

			break;
		}

		default: {
			std::string word;
			if(*i1 == compress_literal_word) {
				i1 = compress_read_literal_word(i1+1,i2,word);				
			} else {
				const compression_schema::char_word_map::const_iterator itor = schema.char_to_word.find(*i1);
				if(itor == schema.char_to_word.end()) {
					std::cerr << "illegal char: " << int(*i1) << "\n";
					throw error("Illegal character in compression input\n");
				}

				word = itor->second;
			}

			if(in_open_element) {
				in_open_element = false;
				config& cfg = add_child(word);
				i1 = cfg.read_compressed_internal(i1+1,i2,schema);
			} else {
				//we have a name/value pair, the value is always a literal string
				std::string value;
				i1 = compress_read_literal_word(i1+1,i2,value);
				values.insert(std::pair<std::string,std::string>(word,value));
			}
		}

		}

		++i1;
	}

	return i1;
}

void config::read_compressed(const std::string& data, compression_schema& schema)
{
	clear();
	read_compressed_internal(data.begin(),data.end(),schema);
}

config::child_itors config::child_range(const std::string& key)
{
	child_map::iterator i = children.find(key);
	if(i != children.end()) {
		return child_itors(i->second.begin(),i->second.end());
	} else {
		static std::vector<config*> dummy;
		return child_itors(dummy.begin(),dummy.end());
	}
}

config::const_child_itors config::child_range(const std::string& key) const
{
	child_map::const_iterator i = children.find(key);
	if(i != children.end()) {
		return const_child_itors(i->second.begin(),i->second.end());
	} else {
		static const std::vector<config*> dummy;
		return const_child_itors(dummy.begin(),dummy.end());
	}
}

const config::child_list& config::get_children(const std::string& key) const
{
	const child_map::const_iterator i = children.find(key);
	if(i != children.end()) {
		return i->second;
	} else {
		static const child_list dummy;
		return dummy;
	}
}

const config::child_map& config::all_children() const { return children; }

config* config::child(const std::string& key)
{
	const child_map::const_iterator i = children.find(key);
	if(i != children.end() && i->second.empty() == false) {
		return i->second.front();
	} else {
		return NULL;
	}
}

const config* config::child(const std::string& key) const
{
	return const_cast<config*>(this)->child(key);
}

config& config::add_child(const std::string& key)
{
	std::vector<config*>& v = children[key];
	v.push_back(new config());
	ordered_children.push_back(child_pos(children.find(key),v.size()-1));
	return *v.back();
}

config& config::add_child(const std::string& key, const config& val)
{
	std::vector<config*>& v = children[key];
	v.push_back(new config(val));
	ordered_children.push_back(child_pos(children.find(key),v.size()-1));
	return *v.back();
}

struct remove_ordered {
	remove_ordered(const std::string& key) : key_(key) {}

	bool operator()(const config::child_pos& pos) const { return pos.pos->first == key_; }
private:
	std::string key_;
};

void config::clear_children(const std::string& key)
{
	ordered_children.erase(std::remove_if(ordered_children.begin(),ordered_children.end(),remove_ordered(key)),ordered_children.end());
	children.erase(key);
}

config* config::remove_child(const std::string& key, size_t index)
{
	//remove from the ordering
	const child_pos pos(children.find(key),index);
	ordered_children.erase(std::remove(ordered_children.begin(),ordered_children.end(),pos),ordered_children.end());

	//decrement all indices in the ordering that are above this index, since everything
	//is getting shifted back by 1.
	for(std::vector<child_pos>::iterator i = ordered_children.begin(); i != ordered_children.end(); ++i) {
		if(i->pos->first == key && i->index > index) {
			i->index--;
		}
	}

	//remove from the child map
	child_list& v = children[key];
	assert(index < v.size());
	config* const res = v[index];
	v.erase(v.begin()+index);
	return res;
}

std::string& config::operator[](const std::string& key)
{
	return values[key];
}

const std::string& config::operator[](const std::string& key) const
{
	const string_map::const_iterator i = values.find(key);
	if(i != values.end()) {
		//see if the value is a variable
		if(i->second[0] == '$') {
			return game_events::get_variable(std::string(i->second.begin()+1,i->second.end()));
		}

		return i->second;
	} else {
		static const std::string empty_string;
		return empty_string;
	}
}

config* config::find_child(const std::string& key,
                           const std::string& name,
                           const std::string& value)
{
	const child_map::iterator i = children.find(key);
	if(i == children.end())
		return NULL;

	const child_list::iterator j = std::find_if(i->second.begin(),
	                                            i->second.end(),
	                                            config_has_value(name,value));
	if(j != i->second.end())
		return *j;
	else
		return NULL;
}

const config* config::find_child(const std::string& key,
                                 const std::string& name,
                                 const std::string& value) const
{
	const child_map::const_iterator i = children.find(key);
	if(i == children.end())
		return NULL;

	const child_list::const_iterator j = std::find_if(
	                                            i->second.begin(),
	                                            i->second.end(),
	                                            config_has_value(name,value));
	if(j != i->second.end())
		return *j;
	else
		return NULL;
}

std::vector<std::string> config::split(const std::string& val, char c, bool remove_empty)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while(i2 != val.end()) {
		if(*i2 == c) {
			std::string new_val(i1,i2);
			strip(new_val);
			if(!remove_empty || !new_val.empty())
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
	strip(new_val);
	if(!remove_empty || !new_val.empty())
		res.push_back(new_val);

	return res;
}
//identical to split(), except it does not split when it otherwise
//would if the previous character was identical to the parameter 'quote'.
//i.e. it does not split quoted commas.
//this method was added to make it possible to quote user input,
//particularly so commas in user input will not cause visual problems in menus.
//why not change split()? that would change the methods post condition.
std::vector<std::string> config::quoted_split(const std::string& val, char c, bool remove_empty, char quote)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while(i2 != val.end()) {
		if(*i2 == quote) {
			// ignore quoted character
			++i2;
			if(i2 != val.end()) ++i2;
		} else if(*i2 == c) {
			std::string new_val(i1,i2);
			strip(new_val);
			if(!remove_empty || !new_val.empty())
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
	strip(new_val);
	if(!remove_empty || !new_val.empty())
		res.push_back(new_val);

	return res;
}

namespace {
//make sure we regard '\r' and '\n' as a space, since Mac, Unix, and DOS
//all consider these differently.
bool notspace(char c) { return !portable_isspace(c); }
}

//prepend all special characters with a backslash
//special characters are:
//#@{}+-,\*
std::string& config::escape(std::string& str)
{
	if(!str.empty()) {
		std::string::size_type pos = 0;

		do {
			pos = str.find_first_of("#@{}+-,\\*",pos);
			if(pos != std::string::npos) {
				str.insert(pos,1,'\\');
				pos += 2;
			}
		} while(pos < str.size() && pos != std::string::npos);
	}
	return str;
}
// remove all escape characters (backslash)
std::string& config::unescape(std::string& str)
{
	std::string::size_type pos = 0;

	do {
		pos = str.find('\\',pos);
		if(pos != std::string::npos) {
			str.erase(pos,1);
			++pos;
		}
	} while(pos < str.size() && pos != std::string::npos);
	return str;
}
std::string& config::strip(std::string& str)
{
	//if all the string contains is whitespace, then the whitespace may
	//have meaning, so don't strip it
	const std::string::iterator it=std::find_if(str.begin(),str.end(),notspace);
	if(it == str.end())
		return str;

	str.erase(str.begin(),it);
	str.erase(std::find_if(str.rbegin(),str.rend(),notspace).base(),str.end());

	return str;
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
	ordered_children.clear();
}

bool config::empty() const
{
	return children.empty() && values.empty();
}

config::all_children_iterator::all_children_iterator(config::all_children_iterator::Itor i) : i_(i)
{}

config::all_children_iterator config::all_children_iterator::operator++()
{
	++i_;
	return *this;
}

config::all_children_iterator config::all_children_iterator::operator++(int)
{
	config::all_children_iterator i = *this;
	++i_;
	return i;
}

std::pair<const std::string*,const config*> config::all_children_iterator::operator*() const
{
	return std::pair<const std::string*,const config*>(&(i_->pos->first),i_->pos->second[i_->index]);
}

bool config::all_children_iterator::operator==(all_children_iterator i) const
{
	return i_ == i.i_;
}

bool config::all_children_iterator::operator!=(all_children_iterator i) const
{
	return i_ != i.i_;
}

config::all_children_iterator config::ordered_begin() const
{
	return all_children_iterator(ordered_children.begin());
}

config::all_children_iterator config::ordered_end() const
{
	return all_children_iterator(ordered_children.end());
}

config config::get_diff(const config& c) const
{
	config res;

	config* inserts = NULL;

	string_map::const_iterator i;
	for(i = values.begin(); i != values.end(); ++i) {
		const string_map::const_iterator j = c.values.find(i->first);
		if(j == c.values.end() || i->second != j->second && i->second != "") {
			if(inserts == NULL) {
				inserts = &res.add_child("insert");
			}

			(*inserts)[i->first] = i->second;
		}
	}

	config* deletes = NULL;

	for(i = c.values.begin(); i != c.values.end(); ++i) {
		const string_map::const_iterator itor = values.find(i->first);
		if(itor == values.end() || itor->second == "") {
			if(deletes == NULL) {
				deletes = &res.add_child("delete");
			}

			(*deletes)[i->first] = "x";
		}
	}

	std::vector<std::string> entities;

	child_map::const_iterator ci;
	for(ci = children.begin(); ci != children.end(); ++ci) {
		entities.push_back(ci->first);
	}

	for(ci = c.children.begin(); ci != c.children.end(); ++ci) {
		if(children.count(ci->first) == 0) {
			entities.push_back(ci->first);
		}
	}

	for(std::vector<std::string>::const_iterator itor = entities.begin(); itor != entities.end(); ++itor) {

		const child_map::const_iterator itor_a = children.find(*itor);
		const child_map::const_iterator itor_b = c.children.find(*itor);

		static const child_list dummy;

		//get the two child lists. 'b' has to be modified to look like 'a'
		const child_list& a = itor_a != children.end() ? itor_a->second : dummy;
		const child_list& b = itor_b != c.children.end() ? itor_b->second : dummy;
		
		size_t ndeletes = 0;
		size_t ai = 0, bi = 0;
		while(ai != a.size() || bi != b.size()) {
			//if the two elements are the same, nothing needs to be done
			if(ai < a.size() && bi < b.size() && *a[ai] == *b[bi]) {
				++ai;
				++bi;
			} else {
				//we have to work out what the most appropriate operation --
				//delete, insert, or change is the best to get b[bi] looking like a[ai]
				//if b has more elements than a, then we assume this element is an
				//element that needs deleting
				if(b.size() - bi > a.size() - ai) {
					config& new_delete = res.add_child("delete_child");
					char buf[50];
					sprintf(buf,"%d",bi-ndeletes);
					new_delete.values["index"] = buf;
					new_delete.add_child(*itor);

					++ndeletes;
					++bi;
				} 

				//if b has less elements than a, then we assume this element is an
				//element that needs inserting
				else if(b.size() - bi < a.size() - ai) {
					config& new_insert = res.add_child("insert_child");
					char buf[50];
					sprintf(buf,"%d",ai);
					new_insert.values["index"] = buf;
					new_insert.add_child(*itor,*a[ai]);

					++ai;
				}

				//otherwise, they have the same number of elements, so try just
				//changing this element to match
				else {
					config& new_change = res.add_child("change_child");
					char buf[50];
					sprintf(buf,"%d",bi);
					new_change.values["index"] = buf;
					new_change.add_child(*itor,a[ai]->get_diff(*b[bi]));

					++ai;
					++bi;
				}
			}
		}
	}


	return res;
}

void config::apply_diff(const config& diff)
{
	const config* const inserts = diff.child("insert");
	if(inserts != NULL) {
		for(string_map::const_iterator i = inserts->values.begin(); i != inserts->values.end(); ++i) {
			values[i->first] = i->second;
		}
	}

	const config* const deletes = diff.child("delete");
	if(deletes != NULL) {
		for(string_map::const_iterator i = deletes->values.begin(); i != deletes->values.end(); ++i) {
			values.erase(i->first);
		}
	}

	const child_list& child_changes = diff.get_children("change_child");
	child_list::const_iterator i;
	for(i = child_changes.begin(); i != child_changes.end(); ++i) {
		const size_t index = atoi((**i)["index"].c_str());
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const std::pair<const std::string*,const config*> item = *j;

			if(item.first->empty()) {
				continue;
			}
			
			const child_map::iterator itor = children.find(*item.first);
			if(itor == children.end() || index >= itor->second.size()) {
				throw error("error in diff: could not find element '" + *item.first + "'");
			}

			itor->second[index]->apply_diff(*item.second);
		}
	}

	const child_list& child_inserts = diff.get_children("insert_child");
	for(i = child_inserts.begin(); i != child_inserts.end(); ++i) {
		const size_t index = atoi((**i)["index"].c_str());
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const std::pair<const std::string*,const config*> item = *j;

			if(item.first->empty()) {
				continue;
			}

			child_list& v = children[*item.first];
			if(index > v.size()) {
				throw error("error in diff: could not find element '" + *item.first + "'");
			}

			v.insert(v.begin()+index,new config(*item.second));
		}
	}

	const child_list& child_deletes = diff.get_children("delete_child");
	for(i = child_deletes.begin(); i != child_deletes.end(); ++i) {
		const size_t index = atoi((**i)["index"].c_str());
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const std::pair<const std::string*,const config*> item = *j;

			if(item.first->empty()) {
				continue;
			}

			const child_map::iterator itor = children.find(*item.first);
			if(itor == children.end() || index > itor->second.size()) {
				throw error("error in diff: could not find element '" + *item.first + "'");
			}

			delete *(itor->second.begin()+index);
			itor->second.erase(itor->second.begin()+index);
		}
	}
}

bool operator==(const config& a, const config& b)
{
	if(a.values.size() != b.values.size()) {
		return false;
	}

	for(string_map::const_iterator i = a.values.begin(); i != a.values.end(); ++i) {
		const string_map::const_iterator j = b.values.find(i->first);
		if(j == b.values.end() || i->second != j->second) {
			return false;
		}
	}

	config::all_children_iterator x = a.ordered_begin(), y = b.ordered_begin();
	while(x != a.ordered_end() && y != b.ordered_end()) {
		const std::pair<const std::string*,const config*> val1 = *x;
		const std::pair<const std::string*,const config*> val2 = *y;

		if(*val1.first != *val2.first || *val1.second != *val2.second) {
			return false;
		}

		++x;
		++y;
	}

	return x == a.ordered_end() && y == b.ordered_end();
}

bool operator!=(const config& a, const config& b)
{
	return !operator==(a,b);
}

//#define TEST_CONFIG

#ifdef TEST_CONFIG

int main()
{
	config cfg(read_file("testconfig"));
	std::cout << cfg.write() << std::endl;
}

#endif
