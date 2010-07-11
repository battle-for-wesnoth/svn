/* $Id$ */

/**
 * @file asio_listener.hpp
 * @brief Header file of a listener for the ana project.
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

#ifndef ASIO_LISTENER_HPP
#define ASIO_LISTENER_HPP

#include <boost/asio.hpp>
#include <memory>

#include "ana.hpp"

using boost::asio::ip::tcp;

class asio_listener : public virtual ana::detail::listener
{
    public:
        asio_listener( );

        virtual void set_listener_handler( ana::listener_handler* listener);
        virtual void run_listener();

        virtual ~asio_listener();

    protected:
        virtual tcp::socket& socket() = 0;

    private:
        virtual void disconnect_listener()                   {}

        virtual void set_raw_buffer_max_size( size_t size );

        virtual void wait_raw_object(ana::serializer::bistream& bis, size_t size);

        void log_conditional_receive( const ana::detail::read_buffer& buf );
        void log_conditional_receive( size_t size, bool finished = false);

        void listen_one_message();

        void disconnect( boost::system::error_code error);

        void handle_header(char* header, const boost::system::error_code& );

        void handle_body( ana::detail::read_buffer , const boost::system::error_code& );

        void handle_partial_body( ana::detail::read_buffer,
                                  const boost::system::error_code&,
                                  size_t accumulated,
                                  size_t last_msg_size);

        void handle_raw_buffer( ana::detail::read_buffer, const boost::system::error_code&, size_t);

        /*attr*/
        bool                       disconnected_;
        ana::listener_handler*     listener_;
        char                       header_[ana::HEADER_LENGTH];
        size_t                     raw_mode_buffer_size_;
};

#endif