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
    listener_( NULL ),
    raw_mode_buffer_size_( ana::INITIAL_RAW_MODE_BUFFER_SIZE )
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

void asio_listener::set_raw_buffer_max_size( size_t size )
{
    if ( size == 0 )
        throw std::runtime_error("Can't set raw buffer size to 0.");

    raw_mode_buffer_size_ = size;
}

void asio_listener::handle_body( ana::detail::read_buffer buf, const boost::system::error_code& ec)
{
    try
    {
        if (ec)
            disconnect( ec );
        else
        {
            log_conditional_receive( buf );
            listener_->handle_message( ec, id(), buf );
            listen_one_message();
        }
    }
    catch(const std::exception& e)
    {
        disconnect( ec);
    }
}

void asio_listener::log_conditional_receive( const ana::detail::read_buffer& buf )
{
    if ( stats_collector() != NULL )
        stats_collector()->log_receive( buf );
}

void asio_listener::log_conditional_receive( size_t size, bool finished)
{
    if ( stats_collector() != NULL )
        stats_collector()->log_receive( size, finished );
}


void asio_listener::handle_header(char* header, const boost::system::error_code& ec)
{
    try
    {
        if (ec)
            disconnect( ec);
        else
        {
            log_conditional_receive( ana::HEADER_LENGTH );
            ana::serializer::bistream input( std::string(header, ana::HEADER_LENGTH) );

            ana::ana_uint32 size;
            input >> size;
            ana::network_to_host_long( size );

            if ( stats_collector() != NULL )
                stats_collector()->start_receive_packet( size );

            if (size != 0)
            {
                ana::detail::read_buffer read_buf( new ana::detail::read_buffer_implementation( size ) );

                socket().async_read_some(boost::asio::buffer(read_buf->base(), read_buf->size() ),
                                        boost::bind(&asio_listener::handle_partial_body,
                                                    this, read_buf,
                                                    boost::asio::placeholders::error, 0, _2 ));
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

void asio_listener::handle_partial_body( ana::detail::read_buffer         buffer,
                                         const boost::system::error_code& ec,
                                         size_t                           accumulated,
                                         size_t                           last_msg_size)
{
    try
    {
        if (ec)
            disconnect( ec );
        else
        {
            accumulated += last_msg_size;

            //2nd param to add a completed packet
            log_conditional_receive( last_msg_size, accumulated == buffer->size() );


            if ( accumulated > buffer->size() )
                throw std::runtime_error("The read operation was too large.");

            if ( accumulated == buffer->size() )
            {
                listener_->handle_message( ec, id(), buffer );
                listen_one_message();
            }
            else
                socket().async_read_some(boost::asio::buffer(buffer->base_char() + accumulated,
                                                             buffer->size()      - accumulated),
                                         boost::bind(&asio_listener::handle_partial_body,
                                                     this, buffer,
                                                     boost::asio::placeholders::error, accumulated, _2 ));
        }
    }
    catch(const std::exception& e)
    {
        disconnect( ec);
    }
}


void asio_listener::handle_raw_buffer( ana::detail::read_buffer buf, const boost::system::error_code& ec, size_t read_size)
{
    try
    {
        if (ec)
            disconnect( ec );
        else
        {
            buf->resize( read_size );
            log_conditional_receive( buf );
            listener_->handle_message( ec, id(), buf );
            listen_one_message();
        }
    }
    catch(const std::exception& e)
    {
        disconnect( ec);
    }
}

void asio_listener::wait_raw_object(ana::serializer::bistream& bis, size_t size)
{
    tcp::socket& sock = socket();

    std::vector<char> buf(size);

    size_t received;

    received = sock.receive( boost::asio::buffer( &buf[0], size ) );

    if ( received != size )
        throw std::runtime_error("Read a different amount of bytes than what was expected.");

    bis.str( std::string( &buf[0], size ) );
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
        if ( header_mode() )
            boost::asio::async_read(socket(), boost::asio::buffer(header_, ana::HEADER_LENGTH),
                                    boost::bind(&asio_listener::handle_header, this,
                                                header_, boost::asio::placeholders::error));
        else
        {
            ana::detail::read_buffer raw_buffer( new ana::detail::read_buffer_implementation( raw_mode_buffer_size_ ) );

            socket().async_read_some(boost::asio::buffer(raw_buffer->base(), raw_mode_buffer_size_ ),
                                    boost::bind(&asio_listener::handle_raw_buffer, this,
                                                raw_buffer, boost::asio::placeholders::error, _2));
        }
    }
    catch(const std::exception& e)
    {
        disconnect( boost::system::error_code(1,boost::system::system_category) );
    }
}