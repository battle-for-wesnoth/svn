/**
 * \file  asio_server.hpp
 * \brief Definition of asio_server class.
 *
 * Copyright (C) 2010 Guillermo Biset
 *
 * Contents:       Header file providing class asio_server.
 *
 * Language:       C++
 *
 * Author:         Guillermo Biset
 * E-Mail:         billybiset AT gmail.com
 *
 */

#ifndef ASIO_SERVER_HPP
#define ASIO_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/function.hpp>

#include "ana.hpp"
#include "asio_listener.hpp"
#include "mili/mili.h"

using boost::asio::ip::tcp;

struct asio_proxy_manager
{
    virtual void deregister_client(ana::server::client_proxy* client) = 0;
};

class asio_server : public  ana::server,
                    private asio_proxy_manager
{
    private:
        class asio_client_proxy : public ana::server::client_proxy,
                                  public asio_listener
        {
            public:
                asio_client_proxy(boost::asio::io_service& io_service, asio_proxy_manager* mgr);

                tcp::socket& socket();

                virtual ~asio_client_proxy();
            private:

                virtual void disconnect_listener();

                virtual void send(ana::detail::shared_buffer, ana::send_handler*, ana::detail::timed_sender* );

                void handle_sent_header(const boost::system::error_code& ec,
                                        mili::bostream*, ana::detail::shared_buffer,
                                        ana::send_handler*, ana::timer*);

                                        void handle_send(const boost::system::error_code& ec,
                                                         ana::detail::shared_buffer, ana::send_handler*, ana::timer*);

                void handle_timeout(const boost::system::error_code& ec, ana::send_handler*);

                tcp::socket         socket_;
                asio_proxy_manager* manager_;
        };

    public:
        asio_server();

        virtual ~asio_server();

    private:
        virtual void set_connection_handler( ana::connection_handler* );

        virtual void send_all(boost::asio::const_buffer, ana::send_handler*, ana::send_type );
        virtual void send_if (boost::asio::const_buffer, ana::send_handler*, const ana::client_predicate&, ana::send_type );
        virtual void send_one(ana::client_id, boost::asio::const_buffer, ana::send_handler*, ana::send_type );
        virtual void send_all_except(ana::client_id, boost::asio::const_buffer, ana::send_handler*, ana::send_type );

        virtual void set_listener_handler( ana::listener_handler* );
        virtual void run_listener();
        virtual void run(ana::port pt);

        virtual ana::client_id id() const;

        virtual void deregister_client(client_proxy* client);

        void handle_accept (const boost::system::error_code& ec,asio_client_proxy* client, ana::connection_handler* );

        void register_client(client_proxy* client);

        void run_acceptor_thread(asio_server* obj, ana::connection_handler* );

        void async_accept( ana::connection_handler* );

        boost::asio::io_service      io_service_;
        boost::thread                io_thread_;
        std::auto_ptr<tcp::acceptor> acceptor_;
        std::list<client_proxy*>     client_proxies_;
        bool                         listening_;
        ana::listener_handler*       listener_;
        ana::connection_handler*     connection_handler_;
        asio_client_proxy*           last_client_proxy_;
};

#endif
