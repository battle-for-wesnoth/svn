/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
//include files for opendir(3), readdir(3), etc. These files may vary
//from platform to platform, since these functions are NOT ANSI-conforming
//functions. They may have to be altered to port to new platforms
#include <sys/types.h>
#include <dirent.h>

//for mkdir
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include <direct.h>

#include <io.h>

#define mkdir(a,b) (_mkdir(a))

#define mode_t int

#endif

//for getenv
#include <cstdlib>

#include <algorithm>
#include <iostream>

#include "filesystem.hpp"

namespace {
	const mode_t AccessMode = 00770;
}

#ifdef _WIN32
#define DIR_INVALID(d) (d == -1)
#else
#define DIR_INVALID(d) (d == NULL)
#endif

void get_files_in_dir(const std::string& directory,
                      std::vector<std::string>* files,
                      std::vector<std::string>* dirs,
                      FILE_NAME_MODE mode)
{

	//if we have a path to find directories in, then convert relative
	//pathnames to be rooted on the wesnoth path
#ifdef WESNOTH_PATH
	if(!directory.empty() && directory[0] != '/' && WESNOTH_PATH[0] == '/') {
		const std::string& dir = WESNOTH_PATH + std::string("/") + directory;
		if(is_directory(dir)) {
			get_files_in_dir(dir,files,dirs,mode);
			return;
		}
	}
#endif

#ifdef _WIN32
	_finddata_t fileinfo;
	long dir = _findfirst((directory + "/*.*").c_str(),&fileinfo);
#else

	DIR* dir = opendir(directory.c_str());
#endif

	if(DIR_INVALID(dir)) {
		//try to make the directory
		const int res = mkdir(directory.c_str(),AccessMode);
		if(res == 0) {
#ifdef _WIN32
			dir = _findfirst((directory + "/*.*").c_str(),&fileinfo);
#else
			dir = opendir(directory.c_str());
#endif
		}

		if(DIR_INVALID(dir))
			return;
	}

#ifdef _WIN32

	int res = dir;
	do {
		if(fileinfo.name[0] != '.') {
			const std::string path = (mode == ENTIRE_FILE_PATH ?
				directory + "/" : std::string("")) + fileinfo.name;

			if(fileinfo.attrib&_A_SUBDIR) {
				if(dirs != NULL)
					dirs->push_back(path);
			} else {
				if(files != NULL)
					files->push_back(path);
			}
		}

		res = _findnext(dir,&fileinfo);
	} while(!DIR_INVALID(res));

	_findclose(dir);

#else
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL) {
		if(entry->d_name[0] == '.')
			continue;

		const std::string name((directory + "/") + entry->d_name);

		//try to open it as a directory to test if it is a directory
		DIR* const temp_dir = opendir(name.c_str());
		if(temp_dir != NULL) {
			closedir(temp_dir);
			if(dirs != NULL) {
				if(mode == ENTIRE_FILE_PATH)
					dirs->push_back(name);
				else
					dirs->push_back(entry->d_name);
			}
		} else if(files != NULL) {
			if(mode == ENTIRE_FILE_PATH)
				files->push_back(name);
			else
				files->push_back(entry->d_name);
		}
	}

	closedir(dir);
#endif

	if(files != NULL)
		std::sort(files->begin(),files->end());

	if(dirs != NULL)
		std::sort(dirs->begin(),dirs->end());
}

std::string get_prefs_file()
{
	return get_user_data_dir() + "/preferences";
}

std::string get_saves_dir()
{
	const std::string dir_path = get_user_data_dir() + "/saves";

#ifdef _WIN32
	_mkdir(dir_path.c_str());
#else

	DIR* dir = opendir(dir_path.c_str());
	if(dir == NULL) {
		const int res = mkdir(dir_path.c_str(),AccessMode);
		if(res == 0) {
			dir = opendir(dir_path.c_str());
		} else {
			std::cerr << "Could not open or create directory: '" << dir_path
			          << "'\n";
		}
	}

	if(dir == NULL)
		return "";
#endif

	return dir_path;
}

std::string get_user_data_dir()
{
	static const char* const current_dir = ".";

#ifdef _WIN32
	_mkdir("userdata");
	return "userdata";
#else

	const char* home_str = getenv("HOME");
	if(home_str == NULL)
		home_str = current_dir;

	const std::string home(home_str);

	const std::string dir_path = home + "/.wesnoth";
	DIR* dir = opendir(dir_path.c_str());
	if(dir == NULL) {
		const int res = mkdir(dir_path.c_str(),AccessMode);
		if(res == 0) {
			dir = opendir(dir_path.c_str());
		} else {
			std::cerr << "Could not open or create directory: '" << dir_path
			          << "'\n";
		}
	}

	if(dir == NULL)
		return "";

	closedir(dir);

	return dir_path;
#endif
}

bool is_directory(const std::string& fname)
{

#ifdef WESNOTH_PATH
	if(!fname.empty() && fname[0] != '/' && WESNOTH_PATH[0] == '/') {
		if(is_directory(WESNOTH_PATH + std::string("/") + fname))
			return true;
	}
#endif

#ifdef _WIN32
	_finddata_t info;
	const long handle = _findfirst((fname + "/*").c_str(),&info);
	if(handle >= 0) {
		_findclose(handle);
		return true;
	} else {
		return false;
	}

#else
	DIR* const dir = opendir(fname.c_str());
	if(dir != NULL) {
		closedir(dir);
		return true;
	} else {
		return false;
	}
#endif
}
