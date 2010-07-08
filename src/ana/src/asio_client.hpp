/* $Id$ */

/**
 * @file asio_client.hpp
 * @brief Header file of the client side of the ana project.
 *
 * ana: Asynchronous Network API.
 * Copyright (C) 2010 Guillermo Biset.
 *
 * This file is part of the ana project.
 *
 * System:         ana
 * Language:       C++
 *
 * Author:         Guillermo Biset
 * E-Mail:         billybiset AT gmail DOT com
 *
 * ana is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * ana is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ana.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ASIO_CLIENT_HPP
#define ASIO_CLIENT_HPP

#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::tcp;

#include "ana.hpp"

#include "asio_proxy_connection.hpp"
#include "asio_listener.hpp"
#include "asio_sender.hpp"


class asio_client : public ana::client,
                    public asio_listener,
                    private proxy_connection_manager,
                    private asio_sender
{
    public:
        /**
         * Standard constructor.
         *
         * @param address : Address to try to connect to. The server should be there.
         * @param port : port to use for the connection. The server should have opened it.
         */
        asio_client(std::string address, ana::port port);

    private:
        virtual ~asio_client();

        virtual void connect( ana::connection_handler* );

        virtual void connect_through_proxy(std::string                     proxy_address,
                                           std::string                     proxy_port,
                                           ana::connection_handler*        handler,
                                           std::string                     user_name = "",
                                           std::string                     password = "");

        virtual void run();

        virtual void send( boost::asio::const_buffer, ana::send_handler*, ana::send_type );

        virtual void disconnect() { disconnect_listener(); }

        virtual void disconnect_listener();

        virtual void handle_proxy_connection(const boost::system::error_code&, ana::connection_handler*);

        virtual tcp::socket& socket();

        virtual void log_receive( ana::detail::read_buffer buffer );

        virtual void start_logging();
        virtual void stop_logging();

        virtual const ana::stats* get_stats( ana::stat_type type ) const;

        virtual ana::stats_collector* stats_collector() { return stats_collector_; }

        virtual ana::timer* create_timer() { return new ana::timer( io_service_); }

        void handle_connect(const boost::system::error_code& ec,
                            tcp::resolver::iterator endpoint_iterator,
                            ana::connection_handler* );

        /* Override, as per -Weffc++ */
        asio_client(const asio_client& other);
        asio_client& operator= (const asio_client& other);

        /*attr*/
        boost::asio::io_service       io_service_;
        boost::asio::io_service::work work_;

        tcp::socket               socket_;

        std::string               address_;
        ana::port                 port_;

        proxy_connection*         proxy_;
        bool                      use_proxy_;

        ana::stats_collector*     stats_collector_;
};

#endif