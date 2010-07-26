/* $Id$ */

/*
Hello reader, welcome:

This document provides a proposal (in C++ header-file format) for
a new network API for Wesnoth.

The idea is to act as an intermediary to the ANA API & Library, which
is already under use in the game, but to implement the old API (thought
to work with SDLnet.)

The point is that the new API will use ANA's asynchronous capabilities,
thus reducing CPU usage by the network implementation and simplifying
the code.

An important point is that this new API would shift control in some cases:
e.g. instead of calling every now and then to see if a new client has
connected, the object implementing the corresponding interfaces will get a
call through the corresponding method.

References:

 - ANA : You can look at the ANA source code in src/ana. Some documentation
         about ANA can be found in doc/ana (LaTeX files).
 - asio : ANA is implemented using the boost::asio library, you can find
          documentation for it in boost.org.
 - Code : File network.hpp in trunk holds the current Wesnoth network API.
          This API has many deprecated methods and the implementation
          currently calls for many blocking calls (which is not desired.)

          Files network.cpp and network_worker.cpp hold the current SDLnet
          implementation of this API.

          Files network_ana.cpp and network_manager_ana.Xpp hold the current
          ANA implementation of the API.


Please submit all feature requests you deem relevant in the following section.

Feature Requests:

*/

/** Namespace of the network API. */
namespace network
{
    /**
     * Main interface for handlers of Wesnoth network events.
     *
     * Code that needs to create network servers or clients (components) will implement this
     * interface. Each method then corresponds to a particular network event.
     */
    struct handler
    {
        /**
         * Signal the successful connection of a component after a handshake.
         *
         * If the associated component is a server, invocation of this method means a new client
         * has connected.
         *
         * If the associated component is a client, invocation of this method means that the client
         * has logged on and handshaked to the server.
         *
         * @param error_code : The ana::error_code of the operation.
         * @param net_id : The ana::net_id of the new client in the server case and the ID of the
         *                 client that connected in the client case.
         *
         * @sa ana::error_code
         */
        virtual void handle_connect(ana::error_code, ana::net_id) {}

        /**
         * A message has been received in the form of a WML document.
         *
         * @param error_code : The corresponding error_code of the operation.
         * @param net_id : The ID of the client that sent the message in the server case or the ID
         *                 of the server in the client case.
         * @param config : The received WML document.
         *
         * @sa ana::error_code
         */
        virtual void handle_receive(ana::error_code, ana::net_id, const config&) {}

        /**
         * A component has disconnected.
         *
         * @param error_code : The corresponding error_code. May shed some light as to why
         *                     the component disconnected in the first place.
         * @param net_id : The ID of the client that sent disconnected in the server case and of the
         *                 server in the client case.
         *
         * @sa ana::error_code
         */
        virtual void handle_disconnect(ana::error_code, ana::net_id) {}

        /**
         * An asynchronous send operation has completed.
         *
         * @param error_code : The corresponding error_code of the operation, it will evaluate to
         *                     false if the operation completed successfully.
         * @param net_id : The ID of the client that the message was sent to in the server case and
         *                 of the server in the client case.
         * @param operation_id : The ID of the send operation. This may be important if there are
         *                       many send operations pending completion.
         *
         * @sa ana::error_code
         * @sa ana::operation_id
         */
        virtual void handle_send(ana::error_code, ana::net_id, operation_id) {}
    };

    //Code that needs to create network components will USE these objects

    /**
     * A network client. Can connect to at most one network server at a time. It will handshake to
     * servers using a procedure described in:
     *                 http://wiki.wesnoth.org/MultiplayerServerWML#The_handshake
     *
     * After a successful connection it can send and receive WML documents to and from the server.
     */
    class client
    {
        public:
            /**
             * Create a client, and associate a handler object to it.
             *
             * Note: The parameter is a reference because it can't be a NULL pointer.
             */
            client( handler& handler );
            // Idea: Add a set_handler method to change handlers during execution.

            // Example, set_timeout( network::SEND_OPERATIONS, ana::time::seconds( 10 ) );
            //          set_timeout( network::SEND_OPERATIONS, ana::KILOBYTES(1),
            //                                                 ana::time::seconds( 1 ) ); (?)
            //          set_timeout( network::CONNECT_OPERATIONS, ana::time::seconds( 30 ) );
            /**
             *
             */
            void set_timeout( ... );

            // Possibilities: Either use set_timeout to set general timeouts for groups of
            // operations or have a timeout parameter for each time you call a method.

            /**
             * Attempt an asynchronous connection to a server, it will call the corresponding
             * handle_connect with the results eventually.
             *
             * @param address : The address of the server you are trying to connect to. Defaults
             *                  to Wesnoth's official server.
             * @param port : The port you are trying to connect to. Defaults to Wesnoth's default.
             */
            void async_connect( const std::string& address = "server.wesnoth.org",
                                const std::string& port    = "15000"  );

            /**
             * Attempt an asynchronous connection through proxy, will call handle_connect
             * with results eventually.
             *
             * @param address : The address of the server you are trying to connect to. Defaults
             *                  to Wesnoth's official server.
             * @param port : The port you are trying to connect to. Defaults to Wesnoth's default.
             * @param user_name : The proxy's user name used for credentials.
             * @param password : The proxy's password used for credentials.
             */
            void async_connect_through_proxy( const std::string& proxy_addr  = "server.wesnoth.org",
                                              const std::string& proxy_port  = "15000",
                                              const std::string& user_name   = "",
                                              const std::string& password    = "");

            /**
             * Attempt to send a WML document to the server.
             *
             * @param config : The WML document to send.
             * @param send_type : The type of send operation, use ana::DONT_COPY to avoid
             *                    unnecessary memory duplication. However, you must ensure that
             *                    the config object used will be alive until the call of
             *                    the handle_send with the returned operation_id. The default
             *                    value will be ana::COPY_BUFFER.
             *
             * @returns the ana::operation_id corresponding to this operation.
             *
             * @sa ana::operation_id
             * @sa ana::send_type
             */
            operation_id async_send( const config&, ana::send_type = ana::COPY_BUFFER);

            /**
             * Signal the client that you are waiting for a message from the server
             * The time parameter indicates how long you are willing to wait
             * If a message is received before this time period the appropiate call to
             * handle_receive will be made with ana::timeout_error.
             */
            void waiting_for_message( time );

            /**
             * Get network stats for the client, the parameter indicates the time period
             * See the stats interface in ana/api/stats.hpp:53
             */
            ana::stats* get_stats( ana::stat_type = ana::ACCUMULATED );

            /** Cancel all pending asynchronous operations. */
            void cancel_pending( );

            /** Force disconnect. */
            void disconnect();

            /**
             * Returns the string representing the IP address of the remote endpoint (server).
             */
            std::string ip_address_of_server() const;

        private:
            //Attributes

            ana::client*  client_;
    };

    /**
     * A Wesnoth network server component. Supports the connection of many clients.
     *
     */
    class server
    {
        public:
            /**
             * Create a server, and associate a handler object to it.
             *
             * @param port : The port to associate the server to.
             * @param handler : A reference to the object handling network events.
             *
             * @throws runtime_error if can't open port.
             */
            server( port, handler& );

            // Set timeouts for a given client
            // Example, set_timeout( 1, network::SEND_OPERATIONS, ana::time::seconds( 10 ) );
            //          set_timeout( 2, network::SEND_OPERATIONS, ana::KILOBYTES(1),
            //                                                    ana::time::seconds( 1 ) ); (?)
            /**
             *
             */
            void set_timeout( ana::net_id, ... );

            /**
             * Attempt to send a WML document to a client.
             */
            operation_id async_send( ana::net_id, const cfg&, ana::SEND_TYPE );

            /**
             * Attempt to send a WML document to every connected client.
             */
            operation_id async_send( const cfg&, ana::SEND_TYPE );

            /**
             * Attempt to send a WML document to every connected client in the container.
             */
            operation_id async_send( container_of_ids,  const cfg&, ana::SEND_TYPE );

            /**
             * Attempt to send a WML document to every connected client except one.
             */
            operation_id async_send_except( ana::net_id,  const cfg&, ana::SEND_TYPE );

            /**
             * Attempt to send a WML document to every connected client except those in the
             * container.
             */
            operation_id async_send_except( container_of_ids,  const cfg&, ana::SEND_TYPE );

            /**
             * Signal the server that you are waiting for a message from a given client
             * The time parameter indicates how long you are willing to wait
             * If a message is received before this time period the appropiate call to
             * handle_receive will be made with ana::timeout_error.
             */
            void waiting_for_message( ana::net_id, time );

            /**
             * Get network stats for the server, the parameter indicates the time period
             * See the stats interface in ana/api/stats.hpp:53
             */
            ana::stats* get_stats( ana::stat_type = ana::ACCUMULATED );

            // Get network stats for a given client, the parameter indicates the time period
            // See the stats interface in ana/api/stats.hpp:53
            /**
             * Get network stats for a connected client, the parameter indicates the time period.
             * See the stats interface in ana/api/stats.hpp:53
             */
            ana::stats* get_stats( ana::net_id, ana::stat_type = ana::ACCUMULATED );

            /** Cancels all pending asynchronous operations. */
            void cancel_pending( );

            /**
             * Cancel all pending asynchronous operations for a given connected client.
             * Will do nothing if the ID doesn't belong to a connected client.
             */
            void cancel_pending( ana::net_id  client_id );

            /**
             * Force disconnect on the server.
             * Will disconnect every connected client.
             */
            void disconnect();

            /**
             * Force the disconnection of a client.
             */
            void disconnect(ana::net_id);

            /** Returns the string representing the IP address of a connected client. */
            std::string ip_address( ana::net_id ) const;

        private:
            //Attributes:

            ana::server*  server_;
    };
}
