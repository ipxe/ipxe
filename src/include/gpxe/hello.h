#ifndef _HELLO_H
#define _HELLO_H

/** @file
 *
 * "Hello world" TCP protocol
 *
 */

#include <stdint.h>
#include <gpxe/tcp.h>

enum hello_state {
	HELLO_SENDING_MESSAGE = 1,
	HELLO_SENDING_ENDL,
};

/**
 * A "hello world" request
 *
 */
struct hello_request {
	/** TCP connection for this request */
	struct tcp_connection tcp;
	/** Current state */
	enum hello_state state;
	/** Message to be transmitted */
	const char *message;
	/** Amount of message remaining to be transmitted */
	size_t remaining;
	/** Callback function
	 *
	 * @v data	Received data
	 * @v len	Length of received data
	 *
	 * This function is called for all data received from the
	 * remote server.
	 */
	void ( *callback ) ( char *data, size_t len );
	/** Connection complete indicator */
	int complete;
};

extern int hello_connect ( struct hello_request *hello );

#endif
