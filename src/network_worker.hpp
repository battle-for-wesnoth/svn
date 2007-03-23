/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef NETWORK_WORKER_HPP_INCLUDED
#define NETWORK_WORKER_HPP_INCLUDED

#include <map>
#include <vector>
#include "config.hpp"

#include "SDL_net.h"

namespace network
{
	struct statistics;
}

namespace network_worker_pool
{

struct manager
{
	explicit manager(size_t min_threads,size_t max_threads);
	~manager();

private:
	manager(const manager&);
	void operator=(const manager&);

	bool active_;
};

//function to asynchronously received data to the given socket
void receive_data(TCPsocket sock);

TCPsocket get_received_data(TCPsocket sock, config& cfg);

void queue_data(TCPsocket sock, const config& buf);
bool socket_locked(TCPsocket sock);
bool close_socket(TCPsocket sock);
std::pair<unsigned int,size_t> thread_state();
TCPsocket detect_error();

std::pair<network::statistics,network::statistics> get_current_transfer_stats(TCPsocket sock);

}

#endif
