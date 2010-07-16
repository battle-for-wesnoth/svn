/* $Id$ */

/**
 * @file asio_sender.hpp
 * @brief Implementation file providing send capability to ana.
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

#include "asio_sender.hpp"

void asio_sender::send(ana::detail::shared_buffer buffer,
                       tcp::socket&               socket,
                       ana::send_handler*         handler,
                       ana::detail::sender*       sender)
{
    ana::timer* running_timer( NULL );
    try
    {
        if ( sender->timeouts_enabled() )
        {
            running_timer = sender->create_timer();

            sender->start_timer( running_timer, buffer,
                                boost::bind(&asio_sender::handle_send, this,
                                            boost::asio::placeholders::error, handler, running_timer, true ) );
        }

        if ( raw_mode() )
        {
            socket.async_write_some( boost::asio::buffer(buffer->base(), buffer->size() ),
                                     boost::bind(&asio_sender::handle_partial_send,this,
                                                 buffer, boost::asio::placeholders::error,
                                                 &socket, handler, running_timer, 0, _2 ));
        }
        else
        {
            if ( stats_collector() != NULL )
                stats_collector()->start_send_packet( buffer->size() + ana::HEADER_LENGTH);

            ana::ana_uint32 size( buffer->size() );
            ana::host_to_network_long( size );

            ana::serializer::bostream* output_stream = new ana::serializer::bostream();
            (*output_stream) << size;

            //write the header first in a separate operation, then send the full buffer
            socket.async_write_some( boost::asio::buffer( output_stream->str() ),
                                     boost::bind(&asio_sender::handle_sent_header,this,
                                                 boost::asio::placeholders::error, output_stream,
                                                 &socket, buffer, handler, running_timer, _2 ));
        }
    }
    catch(std::exception& e)
    {
        disconnect();
        delete running_timer;
    }
}


void asio_sender::handle_sent_header(const ana::error_code&      ec,
                                     ana::serializer::bostream*  bos,
                                     tcp::socket*                socket,
                                     ana::detail::shared_buffer  buffer,
                                     ana::send_handler*          handler,
                                     ana::timer*                 running_timer,
                                     size_t                      bytes_sent)
{
    delete bos;

    if (bytes_sent != sizeof(uint32_t) )
        throw std::runtime_error("Couldn't send header.");

    if ( ec )
        handle_send(ec, handler, running_timer);
    else
    {
        socket->async_write_some( boost::asio::buffer(buffer->base(), buffer->size() ),
                                  boost::bind(&asio_sender::handle_partial_send,this,
                                              buffer, boost::asio::placeholders::error,
                                              socket, handler, running_timer, 0, _2 ));
    }
}

void asio_sender::handle_partial_send( ana::detail::shared_buffer  buffer,
                                       const ana::error_code&      ec,
                                       tcp::socket*                socket,
                                       ana::send_handler*          handler,
                                       ana::timer*                 timer,
                                       size_t                      accumulated,
                                       size_t                      last_msg_size)
{
    try
    {
        if (ec)
            handle_send(ec, handler, timer);
        else
        {
            accumulated += last_msg_size;

            log_conditional_send( last_msg_size, accumulated == buffer->size() );

            if ( accumulated > buffer->size() )
                throw std::runtime_error("The send operation was too large.");

            if ( accumulated == buffer->size() )
                handle_send( ec, handler, timer );
            else
                socket->async_write_some(boost::asio::buffer(buffer->base_char() + accumulated,
                                                             buffer->size()      - accumulated),
                                         boost::bind(&asio_sender::handle_partial_send, this,
                                                     buffer, boost::asio::placeholders::error,
                                                     socket, handler, timer, accumulated, _2 ));
        }
    }
    catch(const std::exception& e)
    {
        disconnect( );
    }
}

void asio_sender::handle_send(const ana::error_code& ec,
                              ana::send_handler*     handler,
                              ana::timer*            running_timer,
                              bool                   from_timeout)
{
    if ( ec != boost::asio::error::operation_aborted ) // equals only after cancellation
    {
        delete running_timer;

        if ( ec && from_timeout )
            handler->handle_send( ana::timeout_error , id() );
        else
            handler->handle_send( ec, id() );

        if ( ec )
            disconnect();
    }
}

void asio_sender::log_conditional_send( size_t size, bool finished )
{
    if (stats_collector() != NULL )
        stats_collector()->log_send( size, finished );
}