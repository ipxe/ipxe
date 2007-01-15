#ifndef _GPXE_HELLO_H
#define _GPXE_HELLO_H

/** @file
 *
 * "Hello world" TCP protocol
 *
 */

#include <stdint.h>
#include <gpxe/tcp.h>
#include <gpxe/async.h>

enum hello_state {
	HELLO_SENDING_MESSAGE = 1,
	HELLO_SENDING_ENDL,
};

/**
 * A "hello world" request
 *
 */
struct hello_request {
	/** Server to connect to */
	struct sockaddr_tcpip server;
	/** Message to be transmitted */
	const char *message;
	/** Callback function
	 *
	 * @v data	Received data
	 * @v len	Length of received data
	 *
	 * This function is called for all data received from the
	 * remote server.
	 */
	void ( *callback ) ( char *data, size_t len );

	/** Current state */
	enum hello_state state;
	/** Amount of message remaining to be transmitted */
	size_t remaining;

	/** TCP application for this request */
	struct tcp_application tcp;

	/** Asynchronous operation */
	struct async async;
};

extern struct async_operation * say_hello ( struct hello_request *hello );

#endif
