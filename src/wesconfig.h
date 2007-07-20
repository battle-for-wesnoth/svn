#ifndef WESCONFIG_H_INCLUDED
#define WESCONFIG_H_INCLUDED

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define VERSION "1.3.5+svn"
# define PACKAGE "wesnoth"
# ifndef LOCALEDIR
#  define LOCALEDIR "translations"
# endif
#endif

/**
 * Some older savegames of Wesnoth can't be loaded anymore this
 * variable defines the minimum required version
 */
#define MIN_SAVEGAME_VERSION "1.3.2"

#endif
