/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Copyright (C) 2005 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "config.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "language.hpp"
#include "log.hpp"
#include "util.hpp"
#include "wassert.hpp"
#include "wesconfig.h"
#include "serialization/binary_wml.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/tokenizer.hpp"

#include <sstream>
#include <stack>

#define ERR_CF LOG_STREAM(err, config)
#define WRN_CF LOG_STREAM(warn, config)
#define LOG_CF LOG_STREAM(info, config)

static const size_t max_recursion_levels = 100;

line_source get_line_source(std::vector< line_source > const &line_src, int line)
{
	line_source res(line, "", 0);
	std::vector< line_source >::const_iterator it =
		std::upper_bound(line_src.begin(), line_src.end(), res);
	if (it != line_src.begin()) {
		--it;
		res.file = it->file;
		res.fileline = it->fileline + (line - it->linenum);
	}

	return res;
}

namespace {

class parser
{
public:
	parser(config& cfg, std::istream& in, std::vector<line_source> const* line_sources);
	void operator() ();

private:
	void parse_element();
	void parse_variable();
	void parse_directive();
	std::string lineno_string(utils::string_map& map, size_t lineno,
			const std::string& string1, const std::string& string2);
	void error(const std::string& message);

	config& cfg_;
	tokenizer tok_;
	std::vector<line_source> const* line_sources;

	struct element {
		element(config* cfg, const std::string& name, size_t start_line, const std::string& textdomain) :
			cfg(cfg), name(name), textdomain(textdomain), start_line(start_line){};

		config* cfg;
		std::string name;

		std::map<std::string, config*> last_element_map;
		std::string textdomain;
		size_t start_line;
	};

	std::stack<element> elements;

	std::string current_textdomain_location;
};

parser::parser(config &cfg, std::istream &in, std::vector<line_source> const *line_sources) :
	cfg_(cfg),
	tok_(in),
	line_sources(line_sources),
	current_textdomain_location("")
{
}

void parser::operator()()
{
	cfg_.clear();
	elements.push(element(&cfg_, "", 0, PACKAGE));
	tok_.textdomain() = PACKAGE;

	do {
		tok_.next_token();

		switch(tok_.current_token().type) {
		case token::LF:
			continue;
		case '[':
			parse_element();
			break;
		case token::STRING:
			parse_variable();
			break;
		default:
			error(_("Unexpected characters at line start"));
			break;
		case token::END:
			break;
		}
	} while (tok_.current_token().type != token::END);

	// The main element should be there. If it is not, this is a parser error.
	wassert(!elements.empty());

	if(elements.size() != 1) {
		utils::string_map i18n_symbols;
		i18n_symbols["tag"] = elements.top().name;
		error(lineno_string(i18n_symbols, elements.top().start_line,
				N_("Missing closing tag for tag $tag (file $file, line $line)"),
				N_("Missing closing tag for tag $tag (line $line)")));
	}
}

void parser::parse_element() 
{
	tok_.next_token();
	std::string elname;
	config* current_element = NULL;
	std::map<std::string, config*>::const_iterator last_element_itor;

	switch(tok_.current_token().type) {
	case token::STRING: // [element]
		elname = tok_.current_token().value;
		if (tok_.next_token().type != ']')
			error(_("Unterminated [element] tag"));

		// Add the element
		current_element = &(elements.top().cfg->add_child(elname));
		elements.top().last_element_map[elname] = current_element;
		elements.top().textdomain = tok_.textdomain();
		elements.push(element(current_element, elname, tok_.get_line(), elements.top().textdomain));
		break;

	case '+': // [+element]
		if (tok_.next_token().type != token::STRING)
			error(_("Invalid tag name"));
		elname = tok_.current_token().value;
		if (tok_.next_token().type != ']')
			error(_("Unterminated [+element] tag"));

		// Find the last child of the current element whose name is
		// element
		last_element_itor = elements.top().last_element_map.find(elname);
		if(last_element_itor == elements.top().last_element_map.end()) {
			current_element = &elements.top().cfg->add_child(elname);
		} else {
			current_element = last_element_itor->second;
		}
		elements.top().last_element_map[elname] = current_element;
		elements.top().textdomain = tok_.textdomain();
		elements.push(element(current_element, elname, tok_.get_line(), elements.top().textdomain));
		break;

	case '/': // [/element]
		if(tok_.next_token().type != token::STRING)
			error(_("Invalid closing tag name"));
		elname = tok_.current_token().value;
		if(tok_.next_token().type != ']')
			error(_("Unterminated closing tag"));
		if(elements.size() <= 1)
			error(_("Unexpected closing tag"));
		if(elname != elements.top().name) {
			utils::string_map i18n_symbols;
			i18n_symbols["tag"] = elements.top().name;
			error(lineno_string(i18n_symbols, elements.top().start_line,
					N_("Found invalid closing tag for tag $tag (file $file, line $line)"),
					N_("Found invalid closing tag for tag $tag (line $line)")));
		}

		elements.pop();
		tok_.textdomain() = elements.top().textdomain;
		break;
	default:
		error(_("Invalid tag name"));
	}
}

void parser::parse_variable()
{
	config& cfg = *elements.top().cfg;
	std::vector<std::string> variables;
	variables.push_back("");

	while (tok_.current_token().type != '=') {
		switch(tok_.current_token().type) {
		case token::STRING:
			if(!variables.back().empty())
				variables.back() += ' ';
			variables.back() += tok_.current_token().value;
			break;
		case ',':
			if(variables.back().empty()) {
				// FIXME: this error message is not really
				// appropriate, although a proper one should
				// wait after string freeze.
				error(_("Unexpected characters after variable name (expected , or =)"));
			}
			variables.push_back("");
			break;
		default:
			error(_("Unexpected characters after variable name (expected , or =)"));
			break;
		}
		tok_.next_token();
	}

	std::vector<std::string>::const_iterator curvar = variables.begin(); 

	bool ignore_next_newlines = false;
	while(1) {
		tok_.next_token();
		wassert(curvar != variables.end());

		switch (tok_.current_token().type) {
		case ',':
			if ((curvar+1) != variables.end()) {
				curvar++;
				continue;
			} else {
				cfg[*curvar] += ",";
			}
			break;
		case '_':
			tok_.next_token();
			switch (tok_.current_token().type) {
			case token::UNTERMINATED_QSTRING:
				error(_("Unterminated quoted string"));
				break;
			case token::QSTRING:
				cfg[*curvar] += t_string(tok_.current_token().value, tok_.textdomain());
				break;
			default:
				cfg[*curvar] += "_";
				cfg[*curvar] += tok_.current_token().value;
				break;
			case token::END:
			case token::LF:
				return;
			}
			break;
		case '+':
			// Ignore this
			break;
		default:
			cfg[*curvar] += tok_.current_token().leading_spaces + tok_.current_token().value;
			break;
		case token::QSTRING:
			cfg[*curvar] += tok_.current_token().value;
			break;
		case token::UNTERMINATED_QSTRING:
			error(_("Unterminated quoted string"));
			break;
		case token::LF:
			if(!ignore_next_newlines)
				return;
			break;
		case token::END:
			return;
		}

		if (tok_.current_token().type == '+') {
			ignore_next_newlines = true;
		} else if (tok_.current_token().type != token::LF) {
			ignore_next_newlines = false;
		}
	}
}

std::string parser::lineno_string(utils::string_map& i18n_symbols, size_t lineno,
			const std::string& string1, const std::string& string2)
{
	std::string res;

	if(line_sources != NULL) {
		const line_source src = get_line_source(*line_sources, lineno);
		i18n_symbols["file"] = lexical_cast<std::string>(src.file);
		i18n_symbols["line"] = lexical_cast<std::string>(src.fileline);
		i18n_symbols["column"] = lexical_cast<std::string>(tok_.get_column());

		res = vgettext(string1.c_str(), i18n_symbols);
	} else {
		i18n_symbols["line"] = lexical_cast<std::string>(lineno);
		i18n_symbols["column"] = lexical_cast<std::string>(tok_.get_column());

		res = vgettext(string2.c_str(), i18n_symbols);
	}
	return res;
}

void parser::error(const std::string& error_type)
{
	utils::string_map i18n_symbols;
	i18n_symbols["error"] = error_type;

	throw config::error(lineno_string(i18n_symbols, tok_.get_line(), 
				N_("$error in file $file (line $line, column $column)"),
				N_("$error (line $line, column $column)")));
}

} // end anon namespace

void read(config &cfg, std::istream &data_in, std::vector< line_source > const *line_sources)
{
	parser(cfg, data_in, line_sources)();
}

static char const *AttributeEquals = "=";

static char const *TranslatableAttributePrefix = "_ \"";
static char const *AttributePrefix = "\"";
static char const *AttributePostfix = "\"";

static char const* AttributeContPostfix = " + \n";
static char const* AttributeEndPostfix = "\n";

static char const* TextdomainPrefix = "#textdomain ";
static char const* TextdomainPostfix = "\n";

static char const *ElementPrefix = "[";
static char const *ElementPostfix = "]\n";
static char const *EndElementPrefix = "[/";
static char const *EndElementPostfix = "]\n";

static std::string escaped_string(const std::string& value) {
	std::vector<char> res;
	for(std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
		//double interior quotes
		if(*i == '\"') res.push_back(*i);
		res.push_back(*i);
	}
	return std::string(res.begin(), res.end());
}

static void write_internal(config const &cfg, std::ostream &out, std::string textdomain, size_t tab = 0)
{
	if (tab > max_recursion_levels)
		return;

	for(string_map::const_iterator i = cfg.values.begin(), i_end = cfg.values.end(); i != i_end; ++i) {
		if (!i->second.empty()) {
			bool first = true;

			for(t_string::walker w(i->second); !w.eos(); w.next()) {
				std::string part(w.begin(), w.end());

				if(w.translatable()) {
					if(w.textdomain() != textdomain) {
						out << TextdomainPrefix 
							<< w.textdomain() 
							<< TextdomainPostfix;
						textdomain = w.textdomain();
					}

					if(first) {
						out << std::string(tab, '\t') 
							<< i->first 
							<< AttributeEquals;
					}

					out << TranslatableAttributePrefix 
						<< escaped_string(part)
						<< AttributePostfix;

				} else {
					if(first) {
						out << std::string(tab, '\t') 
							<< i->first 
							<< AttributeEquals;
					}

					out << AttributePrefix 
						<< escaped_string(part)
						<< AttributePostfix;
				}

				if(w.last()) {
					out << AttributeEndPostfix;
				} else {
					out << AttributeContPostfix;
					out << std::string(tab+1, '\t');
				}
				
				first = false;
			}
		}
	}

	for(config::all_children_iterator j = cfg.ordered_begin(), j_end = cfg.ordered_end(); j != j_end; ++j) {
		const std::pair<const std::string*,const config*>& item = *j;
		const std::string& name = *item.first;
		const config& cfg = *item.second;

		out << std::string(tab, '\t')
		    << ElementPrefix << name << ElementPostfix;
		write_internal(cfg, out, textdomain, tab + 1);
		out << std::string(tab, '\t')
		    << EndElementPrefix << name << EndElementPostfix;
	}
}

void write(std::ostream &out, config const &cfg)
{
	write_internal(cfg, out, PACKAGE);
}
