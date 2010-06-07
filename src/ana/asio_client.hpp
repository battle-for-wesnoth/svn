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
 * the Free Software Foundation, either version 3 of the License, or
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

#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::tcp;

#include "ana.hpp"

#include "asio_proxy_connection.hpp"
#include "asio_listener.hpp"
#include "mili/mili.h"

#ifndef ASIO_CLIENT_HPP
#define ASIO_CLIENT_HPP

class asio_client : public ana::client,
                    public asio_listener,
                    private proxy_connection_manager
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

        virtual void connect_through_proxy(ana::proxy::authentication_type auth_type,
                                           std::string                     proxy_address,
                                           std::string                     proxy_port,
                                           ana::connection_handler*        handler,
                                           std::string                     user_name = "",
                                           std::string                     password = "");

        virtual void run();

        virtual ana::client_id id() const;

        virtual void send( boost::asio::const_buffer, ana::send_handler*, ana::send_type );

        virtual void disconnect_listener();

        virtual void handle_proxy_connection(const boost::system::error_code&, ana::connection_handler*);

        void handle_sent_header(const boost::system::error_code& ec,
                                mili::bostream*, ana::detail::shared_buffer,
                                ana::send_handler*);

                                void handle_send(const boost::system::error_code& ec,
                                                 ana::detail::shared_buffer,
                                                 ana::send_handler*);

        void handle_connect(const boost::system::error_code& ec,
                            tcp::resolver::iterator endpoint_iterator,
                            ana::connection_handler* );

        /* Override, as per -Weffc++ */
        asio_client(const asio_client& other);
        asio_client& operator= (const asio_client& other);

        /*attr*/
        boost::asio::io_service   io_service_;
        tcp::socket               socket_;

        std::string               address_;
        ana::port                 port_;

        proxy_connection*         proxy_;
        bool                      use_proxy_;
};

#endif