#ifndef NETWORK_HPP_INCLUDED
#define NETWORK_HPP_INCLUDED

#include "config.hpp"

#include "SDL_net.h"

#include <string>

//this module wraps the network interface.

namespace network {

//a network manager must be created before networking can be used.
//it must be destroyed only after all networking activity stops.
struct manager {
	manager();
	~manager();

private:
	bool free_;
};

//a server manager causes listening on a given port to occur
//for the duration of its lifetime.
struct server_manager {

	//parameter to pass to the constructor.
	
	enum CREATE_SERVER { MUST_CREATE_SERVER, //will throw exception on failure
	                     TRY_CREATE_SERVER, //will swallow failure
	                     NO_SERVER }; //won't try to create a server at all

	//throws error.
	server_manager(int port, CREATE_SERVER create_server=MUST_CREATE_SERVER);
	~server_manager();

	bool is_running() const;

private:
	bool free_;
};

typedef int connection;

extern const connection null_connection;

//the number of peers we are connected to
size_t nconnections();

//if we are currently accepting connections
bool is_server();

//function to attempt to connect to a remote host. Returns
//the new connection on success, or 0 on failure.
//throws error.
connection connect(const std::string& host, int port=15000);

//function to accept a connection from a remote host. If no
//host is attempting to connect, it will return 0 immediately.
//otherwise returns the new connection.
//throws error.
connection accept_connection();

//function to disconnect from a certain host, or close all
//connections if connection_num is 0
void disconnect(connection connection_num=0);

//function to queue a disconnection. Next time receive_data is
//called, it will generate an error on the given connection.
//(and presumably then the handling of the error will include
// closing the connection)
void queue_disconnect(connection connection_num);

//function to receive data from either a certain connection, or
//all connections if connection_num is 0. Will store the data
//received in cfg. Times out after timeout milliseconds. Returns
//the connection that data was received from, or 0 if timeout
//occurred. Throws error if an error occurred.
connection receive_data(config& cfg, connection connection_num=0, int timeout=0);

//sets the default maximum number of bytes to send to a client at a time
void set_default_send_size(size_t send_size);

enum SEND_TYPE { SEND_DATA, QUEUE_ONLY };

//function to send data down a given connection, or broadcast
//to all peers if connection_num is 0. throws error.
//it will send a maximum of 'max_size' bytes. If cfg when serialized is more than
//'max_size' bytes, then any remaining bytes will be appended to the connection's
//send queue. Also if there is data already in the send queue, then it will be
//appended to the send queue, and max_size bytes in the send queue will be sent.
//if 'max_size' is 0, then the entire contents of the send_queue as well as 'cfg'
//will be sent.
//if 'mode' is set to 'QUEUE_ONLY', then no data will be sent at all: only placed
//in the send queue. This will guarantee that this function never throws
//network::error exceptions
//
//data in the send queue can be sent using process_send_queue
void send_data(const config& cfg, connection connection_num=0, size_t max_size=0, SEND_TYPE mode=SEND_DATA);

//function to queue data to be sent. queue_data(cfg,sock) is equivalent to send_data(cfg,sock,0,QUEUE_ONLY)
void queue_data(const config& cfg, connection connection_num=0);

//function to send any data that is in a connection's send_queue, up to a maximum
//of 'max_size' bytes -- or the entire send queue if 'max_size' bytes is 0
void process_send_queue(connection connection_num=0, size_t max_size=0);

//function to send data to all peers except 'connection_num'
void send_data_all_except(const config& cfg, connection connection_num, size_t max_size=0);

//function to get the remote ip address of a socket
std::string ip_address(connection connection_num);

//function to see the number of bytes being processed on the current socket
std::pair<int,int> current_transfer_stats();

struct connection_stats
{
	connection_stats(int sent, int received, int connected_at);

	int bytes_sent, bytes_received;
	int time_connected;
};

connection_stats get_connection_stats(connection connection_num);

struct error
{
	error(const std::string& msg="", connection sock=0);
	std::string message;
	connection socket;

	void disconnect();
};

bool sends_queued();

}

#endif
