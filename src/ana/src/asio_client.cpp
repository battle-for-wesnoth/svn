/* $Id$ */

/**
 * @file
 * @brief Implementation of the client side of the ana project.
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

#include <iostream>

#include <memory>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "asio_client.hpp"

using boost::asio::ip::tcp;

asio_client::asio_client(ana::address address, ana::port pt) :
    asio_listener(),
    io_service_(),
    io_thread_(),
    work_( io_service_ ),
    socket_(io_service_),
    address_(address),
    port_(pt),
    proxy_( NULL ),
    use_proxy_( false ),
    stats_collector_( NULL ),
    last_valid_operation_id_( ana::no_operation )
{
}

asio_client::~asio_client()
{
    stop_logging();
    disconnect_listener();
    io_thread_.join();
}

ana::client* ana::client::create(ana::address address, ana::port pt)
{
    return new asio_client(address, pt);
}

void asio_client::run()
{
    io_thread_ = boost::thread( boost::bind( &boost::asio::io_service::run, &io_service_) );
}

void asio_client::handle_proxy_connection(const boost::system::error_code& ec,
                                          ana::connection_handler* handler)
{
    handler->handle_connect( ec, 0 );

    if ( ! ec )
        run_listener();

    delete proxy_;
}

tcp::socket& asio_client::socket()
{
    return socket_;
}

void asio_client::handle_connect(const boost::system::error_code& ec,
                                 tcp::resolver::iterator          endpoint_iterator,
                                 ana::connection_handler*         handler )
{
    if ( ! ec )
    {
        handler->handle_connect( ec, 0 );

        if ( ana::client::header_mode() )
            run_listener();
    }
    else
    {
        if ( endpoint_iterator == tcp::resolver::iterator() ) // finished iterating, not connected
            handler->handle_connect( ec, 0 );
        else
        {
            //retry
            socket_.close();

            tcp::endpoint endpoint = *endpoint_iterator;
            socket_.async_connect(endpoint,
                                  boost::bind(&asio_client::handle_connect, this,
                                              boost::asio::placeholders::error, ++endpoint_iterator,
                                              handler));
        }
    }
}

ana::operation_id asio_client::connect( ana::connection_handler* handler )
{
    try
    {
        tcp::resolver resolver(io_service_);
        tcp::resolver::query query(address_.c_str(), port_.c_str() );
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        tcp::endpoint endpoint = *endpoint_iterator;
        socket_.async_connect(endpoint,
                              boost::bind(&asio_client::handle_connect, this,
                                          boost::asio::placeholders::error, ++endpoint_iterator,
                                          handler));
    }
    catch (const std::exception& e)
    {
        handler->handle_connect( boost::system::error_code(1,boost::system::system_category ), 0 );
    }

    return ++last_valid_operation_id_;
}

ana::operation_id asio_client::connect_through_proxy(std::string              proxy_address,
                                                     std::string              proxy_port,
                                                     ana::connection_handler* handler,
                                                     std::string              user_name,
                                                     std::string              password)
{
    use_proxy_ = true;

    proxy_information proxy_info;

    proxy_info.proxy_address = proxy_address;
    proxy_info.proxy_port    = proxy_port;
    proxy_info.user_name     = user_name;
    proxy_info.password      = password;

    proxy_ = new proxy_connection( socket_, proxy_info, address_, port_);

    proxy_->connect( this, handler );

    return ++last_valid_operation_id_;
}

ana::operation_id asio_client::send(boost::asio::const_buffer buffer,
                               ana::send_handler*        handler,
                               ana::send_type            copy_buffer )
{
    ana::detail::shared_buffer s_buf(new ana::detail::copying_buffer(buffer, copy_buffer ) );

    asio_sender::send(s_buf,
                      socket_,
                      handler,
                      static_cast<ana::client*>(this),
                      ++last_valid_operation_id_     );

    return last_valid_operation_id_;
}

void asio_client::log_receive( ana::detail::read_buffer buffer )
{
    if (stats_collector_ != NULL )
        stats_collector_->log_receive( buffer );
}

void asio_client::start_logging()
{
    stop_logging();
    stats_collector_ = new ana::stats_collector();
}

void asio_client::stop_logging()
{
    delete stats_collector_;
    stats_collector_ = NULL;
}

const ana::stats* asio_client::get_stats( ana::stat_type type ) const
{
    if (stats_collector_ != NULL )
        return stats_collector_->get_stats( type );
    else
        throw std::runtime_error("Logging is disabled. Use start_logging first.");
}

void asio_client::cancel_pending()
{
    socket_.cancel();
}


void asio_client::disconnect_listener()
{
    io_service_.stop();
}

