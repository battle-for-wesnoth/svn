/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include "filesystem.hpp"
#include "log.hpp"
#include "wassert.hpp"
#include "wesconfig.h"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"

#define ERR_CF LOG_STREAM(err, config)
#define LOG_CF LOG_STREAM(info, config)

bool preproc_define::operator==(preproc_define const &v) const {
	return value == v.value && arguments == v.arguments;
}

// FIXME
struct config {
	struct error {
		error(const std::string& msg) : message(msg) {}
		std::string message;
	};
};

class preprocessor;
class preprocessor_file;
class preprocessor_data;
class preprocessor_streambuf;
struct preprocessor_deleter;

class preprocessor
{
	preprocessor *const old_preprocessor_;
	std::string old_textdomain_;
	std::string old_location_;
	int old_linenum_;
protected:
	preprocessor_streambuf &target_;
	preprocessor(preprocessor_streambuf &);
public:
	virtual bool get_chunk() = 0;
	virtual ~preprocessor();
};

class preprocessor_streambuf: public std::streambuf
{
	std::string out_buffer_;
	virtual int underflow();
	std::ostringstream buffer_;
	preprocessor *current_;
	preproc_map *defines_;
	preproc_map default_defines_;
	std::string textdomain_;
	std::string location_;
	int linenum_;
	int depth_;
	bool quoted_;
	friend class preprocessor;
	friend class preprocessor_file;
	friend class preprocessor_data;
	friend struct preprocessor_deleter;
	preprocessor_streambuf(preprocessor_streambuf const &);
public:
	preprocessor_streambuf(preproc_map *);
};

preprocessor_streambuf::preprocessor_streambuf(preproc_map *def)
	: current_(NULL), defines_(def), textdomain_(PACKAGE),
	  depth_(0), quoted_(false)
{
}

preprocessor_streambuf::preprocessor_streambuf(preprocessor_streambuf const &t)
	: current_(NULL), defines_(t.defines_),
	  textdomain_(PACKAGE), depth_(t.depth_), quoted_(t.quoted_)
{
}

int preprocessor_streambuf::underflow()
{
	unsigned sz = 0;
	if (char *gp = gptr()) {
		if (gp < egptr())
			return *gp;
		// the buffer has been completely read; fill it again
		// keep part of the previous buffer, to ensure putback capabilities
		sz = out_buffer_.size();
		if (sz > 3) {
			out_buffer_ = out_buffer_.substr(sz - 3);
			sz = 3;
		}
		buffer_.str(std::string());
		buffer_ << out_buffer_;
	}
	while (current_) {
		if (current_->get_chunk()) {
			if (buffer_.str().size() >= 2000)
				break;
		} else {
			 // automatically restore the previous preprocessor
			delete current_;
		}
	}
	out_buffer_ = buffer_.str();
	char *begin = &*out_buffer_.begin();
	unsigned bs = out_buffer_.size();
	setg(begin, begin + sz, begin + bs);
	if (sz >= bs)
		return EOF;
	return (unsigned char)*(begin + sz);
}

preprocessor::preprocessor(preprocessor_streambuf &t)
	: old_preprocessor_(t.current_), target_(t)
{
	old_location_ = target_.location_;
	old_linenum_ = target_.linenum_;
	old_textdomain_ = target_.textdomain_;
	++target_.depth_;
	target_.current_ = this;
}

preprocessor::~preprocessor()
{
	wassert(target_.current_ == this);
	target_.current_ = old_preprocessor_;
	target_.location_ = old_location_;
	target_.linenum_ = old_linenum_;
	target_.textdomain_ = old_textdomain_;
	if (!old_location_.empty())
		target_.buffer_ << "\376line " << old_linenum_
		                << ' ' << old_location_ << '\n';
	if (!old_textdomain_.empty())
		target_.buffer_ << "\376textdomain " << old_textdomain_ << '\n';
	--target_.depth_;
}

class preprocessor_file: preprocessor
{
	std::vector< std::string > files_;
	std::vector< std::string >::const_iterator pos_, end_;
public:
	preprocessor_file(preprocessor_streambuf &, std::string const &);
	virtual bool get_chunk();
};

class preprocessor_data: preprocessor
{
	struct token_desc
	{
		char type;
		int stack_pos;
		int linenum;
	};
	scoped_istream in_;
	std::string directory_;
	std::vector< std::string > strings_;
	std::vector< token_desc > tokens_;
	int slowpath_, skipping_, linenum_;

	std::string read_word();
	std::string read_line();
	void skip_spaces();
	void skip_eol();
	void push_token(char);
	void pop_token();
	void put(char);
	void put(std::string const &);
public:
	preprocessor_data(preprocessor_streambuf &, std::istream *,
	                  std::string const &history,
	                  std::string const &name, int line,
	                  std::string const &dir, std::string const &domain);
	virtual bool get_chunk();
};

preprocessor_file::preprocessor_file(preprocessor_streambuf &t, std::string const &name)
	: preprocessor(t)
{
	if (is_directory(name))
		get_files_in_dir(name, &files_, NULL, ENTIRE_FILE_PATH);
	else
		new preprocessor_data(t, istream_file(name), "", name, 1, directory_name(name), t.textdomain_);
	pos_ = files_.begin();
	end_ = files_.end();
}

bool preprocessor_file::get_chunk()
{
	while (pos_ != end_) {
		std::string const &name = *(pos_++);
		unsigned sz = name.size();
		if (sz < 5 || !std::equal(name.begin() + sz - 4, name.end(), ".cfg"))
			continue;
		new preprocessor_file(target_, name);
		return true;
	}
	return false;
}

preprocessor_data::preprocessor_data(preprocessor_streambuf &t, std::istream *i,
                                     std::string const &history,
                                     std::string const &name, int linenum,
                                     std::string const &directory, std::string const &domain)
	: preprocessor(t), in_(i), directory_(directory), slowpath_(0), skipping_(0), linenum_(linenum)
{
	std::ostringstream s;

	s << history;
	if (!name.empty()) {
		std::string ename(name);
		if (!history.empty())
			s << ' ';
		s << utils::escape(ename, " \\");
	}
	if (!t.location_.empty())
		s << ' ' << t.linenum_ << ' ' << t.location_;
	t.location_ = s.str();
	t.linenum_ = linenum;
	t.textdomain_ = domain;
	t.buffer_ << "\376line " << linenum << ' ' << t.location_
	          << "\n\376textdomain " << domain << '\n';
	push_token('*');
}

void preprocessor_data::push_token(char t)
{
	token_desc token = { t, strings_.size(), linenum_ };
	tokens_.push_back(token);
	std::ostringstream s;
	if (!skipping_ && slowpath_) {
		s << "\376line " << linenum_ << ' ' << target_.location_
		  << "\n\376textdomain " << target_.textdomain_ << '\n';
	}
	strings_.push_back(s.str());
}

void preprocessor_data::pop_token()
{
	strings_.erase(strings_.begin() + tokens_.back().stack_pos, strings_.end());
	tokens_.pop_back();
}

void preprocessor_data::skip_spaces()
{
	for(;;) {
		int c = in_->peek();
		if (!in_->good() || (c != ' ' && c != '\t'))
			return;
		in_->get();
	}
}

void preprocessor_data::skip_eol()
{
	for(;;) {
		int c = in_->get();
		if (c == '\n') {
			++linenum_;
			return;
		}
		if (!in_->good())
			return;
	}
}

std::string preprocessor_data::read_word()
{
	std::string res;
	for(;;) {
		int c = in_->peek();
		if (c == preprocessor_streambuf::traits_type::eof() || utils::portable_isspace(c)) {
			// LOG_CF << "(" << res << ")\n";
			return res;
		}
		in_->get();
		res += (char)c;
	}
}

std::string preprocessor_data::read_line()
{
	std::string res;
	for(;;) {
		int c = in_->get();
		if (c == '\n') {
			++linenum_;
			return res;
		}
		if (!in_->good())
			return res;
		if (c != '\r')
			res += (char)c;
	}
}

void preprocessor_data::put(char c)
{
	if (skipping_)
		return;
	if (slowpath_) {
		strings_.back() += c;
		return;
	}
	if (linenum_ != target_.linenum_ && c != '\n' ||
	    linenum_ != target_.linenum_ + 1 && c == '\n') {
		target_.linenum_ = linenum_;
		if (c == '\n')
			--target_.linenum_;
		target_.buffer_ << "\376line " << target_.linenum_
		                << ' ' << target_.location_ << '\n';
	}
	if (c == '\n')
		++target_.linenum_;
	target_.buffer_ << c;
}

void preprocessor_data::put(std::string const &s)
{
	if (skipping_)
		return;
	if (slowpath_) {
		strings_.back() += s;
		return;
	}
	target_.buffer_ << s;
	target_.linenum_ += std::count(s.begin(), s.end(), '\n');
	target_.linenum_ -= std::count(s.begin(), s.end(), '\376');
}

bool preprocessor_data::get_chunk()
{
	char c = in_->get();
	if (c == '\n')
		++linenum_;
	token_desc &token = tokens_.back();
	if (!in_->good()) {
		char const *s;
		switch (token.type) {
		case '*': return false; // everything is fine
		case 'i':
		case 'I':
		case 'j':
		case 'J': s = "#ifdef"; break;
		case '"': s = "quoted string"; break;
		case '[':
		case '{': s = "macro substitution"; break;
		case '(': s = "macro argument"; break;
		default: s = "???";
		}
		std::ostringstream error;
		error << s << " not terminated, started at "
		      << token.linenum << ' ' << target_.location_;
		ERR_CF << error.str() << '\n';
		pop_token();
		throw config::error(error.str());
	}

	if (c == '\376') {
		std::string buffer(1, c);
		for(;;) {
			char d = in_->get();
			if (!in_->good() || d == '\n')
				break;
			buffer += d;
		}
		buffer += '\n';
		put(buffer);
	} else if (c == '"') {
		if (token.type == '"') {
			target_.quoted_ = false;
			put(c);
			if (!skipping_ && slowpath_) {
				std::string tmp = strings_.back();
				pop_token();
				strings_.back() += tmp;
			} else
				pop_token();
		} else if (!target_.quoted_) {
			target_.quoted_ = true;
			if (token.type == '{')
				token.type = '[';
			push_token('"');
			put(c);
		} else {
			std::ostringstream error;
			error << "nested quoted string started at "
			      << linenum_ << ' ' << target_.location_;
			ERR_CF << error.str() << '\n';
			throw config::error(error.str());
		}
	} else if (c == '{') {
		if (token.type == '{')
			token.type = '[';
		push_token('{');
		++slowpath_;
	} else if (c == ')' && token.type == '(') {
		tokens_.pop_back();
		strings_.push_back(std::string());
	} else if (c == '#' && !target_.quoted_) {
		std::string command = read_word();
		bool comment = false;
		if (command == "define") {
			skip_spaces();
			int linenum = linenum_;
			std::string s = read_line();
			std::vector< std::string > items = utils::split(s, ' ');
			if (items.empty()) {
				std::ostringstream error;
				error << "no macro name found after #define directive at "
				      << linenum_ << ' ' << target_.location_;
				ERR_CF << error.str() << '\n';
				throw config::error(error.str());
			}
			std::string symbol = items.front();
			items.erase(items.begin());
			int found_enddef = 0;
			std::string buffer;
			for(;;) {
				char d = in_->get();
				if (!in_->good())
					break;
				if (d == '\n')
					++linenum_;
				buffer += d;
				if (d == '#')
					found_enddef = 1;
				else if (found_enddef > 0)
					if (++found_enddef == 7)
						if (std::equal(buffer.end() - 6, buffer.end(), "enddef"))
							break;
						else
							found_enddef = 0;
			}
			if (found_enddef != 7) {
				std::ostringstream error;
				error << "unterminated preprocessor definition at "
				      << linenum << ' ' << target_.location_;
				ERR_CF << error.str() << '\n';
				throw config::error(error.str());
			}
			if (!skipping_) {
				buffer.erase(buffer.end() - 7, buffer.end());
				target_.defines_->insert(std::make_pair(
					symbol, preproc_define(buffer, items, target_.textdomain_,
					                       linenum + 1, target_.location_)));
				LOG_CF << "defining macro " << symbol << " (location " << target_.location_ << ")\n";
			}
		} else if (command == "ifdef") {
			skip_spaces();
			std::string const &symbol = read_word();
			bool skip = target_.defines_->count(symbol) == 0;
			LOG_CF << "testing for macro " << symbol << ": " << (skip ? "not defined" : "defined") << '\n';
			if (skip)
				++skipping_;
			push_token(skip ? 'J' : 'i');
		} else if (command == "else") {
			if (token.type == 'J') {
				pop_token();
				--skipping_;
				push_token('j');
			} else if (token.type == 'i') {
				pop_token();
				++skipping_;
				push_token('I');
			} else {
				std::ostringstream error;
				error << "unexpected #else at "
				      << linenum_ << ' ' << target_.location_;
				ERR_CF << error.str() << '\n';
				throw config::error(error.str());
			}
		} else if (command == "endif") {
			switch (token.type) {
			case 'I':
			case 'J': --skipping_;
			case 'i':
			case 'j': break;
			default:
				std::ostringstream error;
				error << "unexpected #endif at "
				      << linenum_ << ' ' << target_.location_;
				ERR_CF << error.str() << '\n';
				throw config::error(error.str());
			}
			pop_token();
		} else if (command == "textdomain") {
			skip_spaces();
			std::string const &s = read_word();
			put("#textdomain ");
			put(s);
			target_.textdomain_ = s;
			comment = true;
		} else if (command == "enddef") {
			std::ostringstream error;
			error << "unexpected #enddef at "
			      << linenum_ << ' ' << target_.location_;
			ERR_CF << error.str() << '\n';
			throw config::error(error.str());
		} else
			comment = true;
		skip_eol();
		if (comment)
			put('\n');
	} else if (token.type == '{' || token.type == '[') {
		if (c == '(') {
			if (token.type == '[')
				token.type = '{';
			else
				strings_.pop_back();
			push_token('(');
		} else if (utils::portable_isspace(c)) {
			if (token.type == '[') {
				strings_.push_back(std::string());
				token.type = '{';
			}
		} else if (c == '}') {
			--slowpath_;
			if (skipping_) {
				pop_token();
				return true;
			}
			if (token.type == '{') {
				wassert(strings_.back().empty());
				strings_.pop_back();
			}

			std::string symbol = strings_[token.stack_pos];
			std::string::size_type pos;
			while ((pos = symbol.find('\376')) != std::string::npos) {
				std::string::iterator b = symbol.begin(); // invalidated at each iteration
				symbol.erase(b + pos, b + symbol.find('\n', pos + 1) + 1);
			}
			//if this is a known pre-processing symbol, then we insert
			//it, otherwise we assume it's a file name to load
			preproc_map::const_iterator macro = target_.defines_->find(symbol),
			                            unknown_macro = target_.defines_->end();
			if (macro != unknown_macro) {
				preproc_define const &val = macro->second;
				size_t nb_arg = strings_.size() - token.stack_pos - 1;
				if (nb_arg != val.arguments.size()) {
					std::ostringstream error;
					error << "preprocessor symbol '" << symbol << "' expects "
					      << val.arguments.size() << " arguments, but has "
					      << nb_arg << " arguments at " << linenum_
					      << ' ' << target_.location_;
					ERR_CF << error.str() << '\n';
					throw config::error(error.str());
				}

				std::stringstream *buffer = new std::stringstream;
				std::string::const_iterator i_bra = val.value.end();
				int macro_num = val.linenum;
				std::string macro_textdomain = val.textdomain;
				for(std::string::const_iterator i = val.value.begin(),
				    i_end = val.value.end(); i != i_end; ++i) {
					char c = *i;
					if (c == '\n')
						++macro_num;
					if (c == '{') {
						if (i_bra != i_end)
							buffer->write(&*i_bra - 1, i - i_bra + 1);
						i_bra = i + 1;
					} else if (i_bra == i_end) {
						if (c == '#') {
							// keep track of textdomain changes in the body of the
							// macro so they can be restored after each substitution
							// of a macro argument
							std::string::const_iterator i_beg = i + 1;
							if (i_end - i_beg >= 13 &&
							    std::equal(i_beg, i_beg + 10, "textdomain")) {
								i_beg += 10;
								i = std::find(i_beg, i_end, '\n');
								if (i_beg != i)
									++i_beg;
								macro_textdomain = std::string(i_beg, i);
								*buffer << "#textdomain " << macro_textdomain;
								++macro_num;
								c = '\n';
							}
						}
						buffer->put(c);
					} else if (c == '}') {
						size_t sz = i - i_bra;
						for(size_t n = 0; n < nb_arg; ++n) {
							std::string const &arg = val.arguments[n];
							if (arg.size() != sz ||
							    !std::equal(i_bra, i, arg.begin()))
								continue;
							*buffer << strings_[token.stack_pos + n + 1]
							        << "\376line " << macro_num
							        << ' ' << val.location << "\n\376textdomain "
							        << macro_textdomain << '\n';
							i_bra = i_end;
							break;
						}
						if (i_bra != i_end) {
							// the bracketed text was no macro argument
							buffer->write(&*i_bra - 1, sz + 2);
							i_bra = i_end;
						}
					}
				}

				pop_token();
				std::string const &dir = directory_name(val.location.substr(0, val.location.find(' ')));
				if (!slowpath_) {
					LOG_CF << "substituting macro " << symbol << '\n';
					new preprocessor_data(target_, buffer, val.location, "",
					                      val.linenum, dir, val.textdomain);
				} else {
					LOG_CF << "substituting (slow) macro " << symbol << '\n';
					std::ostringstream res;
					preprocessor_streambuf *buf =
						new preprocessor_streambuf(target_);
					{	std::istream in(buf);
						new preprocessor_data(*buf, buffer, val.location, "",
						                      val.linenum, dir, val.textdomain);
						res << in.rdbuf(); }
					delete buf;
					strings_.back() += res.str();
				}
			} else if (target_.depth_ < 40) {
				pop_token();
				std::string prefix;
				std::string nfname;
				std::string const &newfilename = symbol;
#ifdef USE_ZIPIOS
				if(newfilename != "" && newfilename[0] == '~') {
					// I do not know of any valid use of {~xxx} when {xxx} is
					// not used, and zipios takes care of both
					LOG_CF << "ignoring reference to '" << newfilename << "'\n";
				} else
#endif
				{
#ifndef USE_ZIPIOS
					//if the filename begins with a '~', then look
					//in the user's data directory. If the filename begins with
					//a '@' then we look in the user's data directory,
					//but default to the standard data directory if it's not found
					//there.
					if(newfilename != "" && (newfilename[0] == '~' || newfilename[0] == '@')) {
						nfname = newfilename;
						nfname.erase(nfname.begin(),nfname.begin()+1);
						nfname = get_user_data_dir() + "/data/" + nfname;

						LOG_CF << "got relative name '" << newfilename << "' -> '" << nfname << "'\n";

						if(newfilename[0] == '@' && file_exists(nfname) == false && is_directory(nfname) == false) {
							nfname = "data/" + newfilename.substr(1);
						}
					} else
#endif
					if(newfilename.size() >= 2 && newfilename[0] == '.' &&
						newfilename[1] == '/' ) {
						//if the filename begins with a "./", then look
						//in the same directory as the file currrently
						//being preprocessed
						nfname = newfilename;
						nfname.erase(nfname.begin(),nfname.begin()+2);
						nfname = directory_ + nfname;

					} else {
#ifdef USE_ZIPIOS
						if(newfilename != "" && newfilename[0] == '@') {
							nfname = newfilename;
							nfname.erase(nfname.begin(),nfname.begin()+1);
							nfname = "data/" + nfname;
						} else
#endif

							nfname = "data/" + newfilename;
					}

					if (!slowpath_)
						new preprocessor_file(target_, nfname);
					else {
						std::ostringstream res;
						preprocessor_streambuf *buf =
							new preprocessor_streambuf(target_);
						{	std::istream in(buf);
							new preprocessor_file(*buf, nfname);
							res << in.rdbuf(); }
						delete buf;
						strings_.back() += res.str();
					}
				}
			} else {
				ERR_CF << "too much nested preprocessing inclusions at "
				       << linenum_ << ' ' << target_.location_
				       << ". Aborting.\n";
				pop_token();
			}
		} else {
			strings_.back() += c;
			token.type = '[';
		}
	} else
		put(c);
	return true;
}

struct preprocessor_deleter: std::basic_istream<char>
{
	preprocessor_streambuf *buf_;
	preproc_map *defines_;
	preprocessor_deleter(preprocessor_streambuf *buf, preproc_map *defines);
	~preprocessor_deleter();
};

preprocessor_deleter::preprocessor_deleter(preprocessor_streambuf *buf, preproc_map *defines)
	: std::basic_istream<char>(buf), buf_(buf), defines_(defines)
{
}

preprocessor_deleter::~preprocessor_deleter()
{
	rdbuf(NULL);
	delete buf_;
	delete defines_;
}


std::istream *preprocess_file(std::string const &fname,
                              preproc_map *defines)
{
	log_scope("preprocessing file...");
	preproc_map *owned_defines = NULL;
	if (!defines) {
		// if no preproc_map has been given, create a new one, and ensure
		// it is destroyed when the stream is by giving it to the deleter
		owned_defines = new preproc_map;
		defines = owned_defines;
	}
	preprocessor_streambuf *buf = new preprocessor_streambuf(defines);
	new preprocessor_file(*buf, fname);
	return new preprocessor_deleter(buf, owned_defines);
}
