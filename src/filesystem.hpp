/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file filesystem.hpp
 * Declarations for File-IO.
 */

#ifndef FILESYSTEM_HPP_INCLUDED
#define FILESYSTEM_HPP_INCLUDED

#include <time.h>

#include <iosfwd>
#include <string>
#include <vector>

/** An exception object used when an IO error occurs */
struct io_exception : public std::exception {
	io_exception() : message("") {}
	io_exception(const std::string& msg) : message(msg) {}
	virtual ~io_exception() throw() {}

	virtual const char* what() const throw() { return message.c_str(); }
private:
	std::string message;
};

struct file_tree_checksum;

enum file_name_option { ENTIRE_FILE_PATH, FILE_NAME_ONLY };
enum file_filter_option { NO_FILTER, SKIP_MEDIA_DIR };
enum file_reorder_option { DONT_REORDER, DO_REORDER };

/**
 * Populates 'files' with all the files and
 * 'dirs' with all the directories in dir.
 * If files or dirs are NULL they will not be used.
 *
 * mode: determines whether the entire path or just the filename is retrieved.
 * filter: determines if we skip images and sounds directories
 * reorder: triggers the special handling of _main.cfg and _final.cfg
 * checksum: can be used to store checksum info
 */
void get_files_in_dir(const std::string dir,
                      std::vector<std::string>* files,
                      std::vector<std::string>* dirs=NULL,
                      file_name_option mode = FILE_NAME_ONLY,
                      file_filter_option filter = NO_FILTER,
                      file_reorder_option reorder = DONT_REORDER,
                      file_tree_checksum* checksum = NULL);

std::string get_dir(const std::string &dir);

// The location of various important files:
std::string get_prefs_file();
std::string get_save_index_file();
std::string get_saves_dir();
std::string get_cache_dir();
std::string get_intl_dir();
std::string get_screenshot_dir();
std::string get_addon_campaigns_dir();

/**
 * Wrapper around get_mp_addons_dir() and get_addon_campaigns_dir() intended
 * for massive operations with add-ons, e.g. enumerating for publishing.
 */
std::vector< std::string > get_addon_dirs();

/**
 * Get the next free filename using "name + number (3 digits) + extension"
 * maximum 1000 files then start always giving 999
 */
std::string get_next_filename(const std::string& name, const std::string& extension);
std::string get_upload_dir();
void set_preferences_dir(std::string path);
const std::string& get_user_data_dir();

std::string get_cwd();
std::string get_exe_dir();

bool make_directory(const std::string& dirname);
bool delete_directory(const std::string& dirname);

// Basic disk I/O:

/** Basic disk I/O - read file.
 * The bool relative_from_game_path determines whether relative paths should be treated as relative
 * to the game path (true) or to the current directory from which Wesnoth was run (false).
 * The non-const version will update the string if the game path is prepended.
 */
std::string read_file(const std::string &fname, bool relative_from_game_path = true);
std::string read_file(std::string &fname, bool relative_from_game_path = true);
std::istream *istream_file(const std::string& fname, bool relative_from_game_path = true);
std::istream *istream_file(std::string& fname, bool relative_from_game_path = true);
std::ostream *ostream_file(std::string const &fname);
/** Throws io_exception if an error occurs. */
void write_file(const std::string& fname, const std::string& data);

std::string read_map(const std::string& name);

/**
 * Creates a directory if it does not exist already.
 *
 * @param dirname                 Path to directory. All parents should exist.
 * @returns                       True if the directory exists or could be
 *                                successfully created; false otherwise.
 */
bool create_directory_if_missing(const std::string& dirname);

/** Returns true if the given file is a directory. */
bool is_directory(const std::string& fname);

/** Returns true if a file or directory with such name already exists. */
bool file_exists(const std::string& name);

/** Get the creation time of a file. */
time_t file_create_time(const std::string& fname);

/** Return the next ordered full filename within this directory. */
std::string next_filename(const std::string &dirname, unsigned int max = 0);

/** Returns true if the file ends with '.gz'. */
bool is_gzip_file(const std::string& filename);

struct file_tree_checksum
{
	file_tree_checksum();
	explicit file_tree_checksum(const class config& cfg);
	void write(class config& cfg) const;
	void reset() {nfiles = 0;modified = 0;sum_size=0;};
	// @todo: make variables private!
	size_t nfiles, sum_size;
	time_t modified;
};

bool operator==(const file_tree_checksum& lhs, const file_tree_checksum& rhs);
bool operator!=(const file_tree_checksum& lhs, const file_tree_checksum& rhs);

/** Get the time at which the data/ tree was last modified at. */
const file_tree_checksum& data_tree_checksum(bool reset = false);

/** Returns the size of a file, or -1 if the file doesn't exist. */
int file_size(const std::string& fname);

bool ends_with(const std::string& str, const std::string& suffix);

/**
 * Returns the base filename of a file, with directory name stripped.
 * Equivalent to a portable basename() function.
 */
std::string file_name(const std::string& file);

/**
 * Returns the directory name of a file, with filename stripped.
 * Equivalent to a portable dirname()
 */
std::string directory_name(const std::string& file);

/**
 *  The paths manager is responsible for recording the various paths
 *  that binary files may be located at.
 *  It should be passed a config object which holds binary path information.
 *  This is in the format
 *@verbatim
 *    [binary_path]
 *      path=<path>
 *    [/binary_path]
 *  Binaries will be searched for in [wesnoth-path]/data/<path>/images/
 *@endverbatim
 */
struct binary_paths_manager
{
	binary_paths_manager();
	binary_paths_manager(const class config& cfg);
	~binary_paths_manager();

	void set_paths(const class config& cfg);

private:
	binary_paths_manager(const binary_paths_manager& o);
	binary_paths_manager& operator=(const binary_paths_manager& o);

	void cleanup();

	std::vector<std::string> paths_;
};

void clear_binary_paths_cache();

/**
 * Returns a vector with all possible paths to a given type of binary,
 * e.g. 'images', 'sounds', etc,
 */
const std::vector<std::string>& get_binary_paths(const std::string& type);

/**
 * Returns a complete path to the actual file of a given @a type
 * or an empty string if the file isn't present.
 */
std::string get_binary_file_location(const std::string& type, const std::string& filename);

class scoped_istream {
	std::istream *stream;
public:
	scoped_istream(std::istream *s): stream(s) {}
	scoped_istream& operator=(std::istream *);
	std::istream &operator*() { return *stream; }
	std::istream *operator->() { return stream; }
	~scoped_istream();
};

class scoped_ostream {
	std::ostream *stream;
public:
	scoped_ostream(std::ostream *s): stream(s) {}
	scoped_ostream& operator=(std::ostream *);
	std::ostream &operator*() { return *stream; }
	std::ostream *operator->() { return stream; }
	~scoped_ostream();
};

#endif
