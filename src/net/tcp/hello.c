#include <stddef.h>
#include <string.h>
#include <vsprintf.h>
#include <assert.h>
#include <gpxe/hello.h>

/** @file
 *
 * "Hello world" TCP protocol
 *
 * This file implements a trivial TCP-based protocol.  It connects to
 * the server specified in hello_request::tcp and transmits a single
 * message (hello_request::message).  Any data received from the
 * server will be passed to the callback function,
 * hello_request::callback(), and once the connection has been closed,
 * hello_request::complete will be set to 1.
 *
 * To use this code, do something like:
 *
 * @code
 *
 *   static void my_callback ( char *data, size_t len ) {
 *     ... process data ...
 *   }
 *
 *   struct hello_request hello = {
 *     .message = "hello world!",
 *     .callback = my_callback,
 *   };
 *
 *   hello.sin.sin_addr.s_addr = ... server IP address ...
 *   hello.sin.sin_port = ... server port ...
 *
 *   hello_connect ( &hello );
 *   while ( ! hello.completed ) {
 *     run_tcpip();
 *   }
 *
 * @endcode
 *
 * It's worth noting that this trivial protocol would be entirely
 * adequate to implement a TCP-based version of TFTP; just use "RRQ
 * <filename>" as the message.  Now, if only an appropriate server
 * existed...
 */

static inline struct hello_request *
tcp_to_hello ( struct tcp_connection *conn ) {
	return container_of ( conn, struct hello_request, tcp );
}

static void hello_aborted ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	printf ( "Connection aborted\n" );
	hello->complete = 1;
}

static void hello_timedout ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	printf ( "Connection timed out\n" );
	hello->complete = 1;
}

static void hello_closed ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	hello->complete = 1;
}

static void hello_connected ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	hello->remaining = strlen ( hello->message );
	hello->state = HELLO_SENDING_MESSAGE;
}

static void hello_acked ( struct tcp_connection *conn, size_t len ) {
	struct hello_request *hello = tcp_to_hello ( conn );
	
	hello->message += len;
	hello->remaining -= len;
	if ( hello->remaining == 0 ) {
		switch ( hello->state ) {
		case HELLO_SENDING_MESSAGE:
			hello->message = "\r\n";
			hello->remaining = 2;
			hello->state = HELLO_SENDING_ENDL;
			break;
		case HELLO_SENDING_ENDL:
			/* Nothing to do once we've finished sending
			 * the end-of-line indicator.
			 */
			break;
		default:
			assert ( 0 );
		}
	}
}

static void hello_newdata ( struct tcp_connection *conn, void *data,
			    size_t len ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	hello->callback ( data, len );
}

static void hello_senddata ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	tcp_send ( conn, hello->message, hello->remaining );
}

static struct tcp_operations hello_tcp_operations = {
	.aborted	= hello_aborted,
	.timedout	= hello_timedout,
	.closed		= hello_closed,
	.connected	= hello_connected,
	.acked		= hello_acked,
	.newdata	= hello_newdata,
	.senddata	= hello_senddata,
};

/**
 * Initiate a "hello world" connection
 *
 * @v hello	"Hello world" request
 */
void hello_connect ( struct hello_request *hello ) {
	hello->tcp.tcp_op = &hello_tcp_operations;
	tcp_connect ( &hello->tcp );
}
