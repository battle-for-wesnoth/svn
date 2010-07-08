/* $Id$ */

/**
 * @file asio_listener.cpp
 * @brief Implementation of a listener for the ana project.
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

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "asio_listener.hpp"

using boost::asio::ip::tcp;

asio_listener::asio_listener( ) :
    disconnected_( false ),
    listener_( NULL )
{
}

asio_listener::~asio_listener()
{
}

void asio_listener::disconnect( boost::system::error_code error)
{
    if ( ! disconnected_ )
    {
        listener_->handle_disconnect( error, id() );
        disconnected_ = true;
        disconnect_listener();
    }
}


void asio_listener::handle_body( ana::detail::read_buffer buf, const boost::system::error_code& ec)
{
    try
    {
        if (ec)
            disconnect( ec );
        else
        {
            log_receive( buf );

            listener_->handle_message( ec, id(), buf );

            listen_one_message();
        }
    }
    catch(const std::exception& e)
    {
        disconnect( ec);
    }
}

void asio_listener::handle_header(char* header, const boost::system::error_code& ec)
{
    try
    {
        if (ec)
            disconnect( ec);
        else
        {
            ana::serializer::bistream input( std::string(header, ana::HEADER_LENGTH) );

            ana::ana_uint32 size;
            input >> size;
            ana::network_to_host_long( size );

            if (size != 0)
            {
                ana::detail::read_buffer read_buf( new ana::detail::read_buffer_implementation( size ) );

                boost::asio::async_read(socket(), boost::asio::buffer( read_buf->base(), read_buf->size() ),
                                        boost::bind(&asio_listener::handle_body,
                                                    this, read_buf,
                                                    boost::asio::placeholders::error ));
            }
            else
            {   // copy the header to a read_buffer
                ana::detail::read_buffer read_buf ( new ana::detail::read_buffer_implementation( ana::HEADER_LENGTH ) );
                for (size_t i(0); i< ana::HEADER_LENGTH; ++i)
                    static_cast<char*>(read_buf->base())[i] = header[i];

                listener_->handle_message( ec, id(), read_buf );
            }
        }
    }
    catch(const std::exception& e)
    {
        disconnect(ec);
    }
}

void asio_listener::wait_raw_object(ana::serializer::bistream& bis, size_t size)
{
    tcp::socket& sock = socket();

    char buf[ size ];

    size_t received;

    received = sock.receive( boost::asio::buffer( buf, size ) );

    if ( received != size )
        throw std::runtime_error("Read a different amount of bytes than what was expected.");

    bis.str( std::string( buf, size ) );
}


void asio_listener::set_listener_handler( ana::listener_handler* listener )
{
    listener_ = listener;
}

void asio_listener::run_listener( )
{
    listen_one_message();
}

void asio_listener::listen_one_message()
{
    try
    {
        boost::asio::async_read(socket(), boost::asio::buffer(header_, ana::HEADER_LENGTH),
                                boost::bind(&asio_listener::handle_header, this,
                                            header_, boost::asio::placeholders::error));
    }
    catch(const std::exception& e)
    {
        disconnect( boost::system::error_code(1,boost::system::system_category) );
    }
}