#include <stddef.h>
#include <string.h>
#include <vsprintf.h>
#include <assert.h>
#include <gpxe/async.h>
#include <gpxe/hello.h>

/** @file
 *
 * "Hello world" TCP protocol
 *
 * This file implements a trivial TCP-based protocol.  It connects to
 * the server specified in hello_request::server and transmits a
 * single message (hello_request::message).  Any data received from
 * the server will be passed to the callback function,
 * hello_request::callback(), and once the connection has been closed,
 * the asynchronous operation associated with the request will be
 * marked as complete.
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
 *     .server = {
 *       ...
 *     },
 *     .message = "hello world!",
 *     .callback = my_callback,
 *   };
 *
 *   rc = async_wait ( say_hello ( &hello ) );
 *
 * @endcode
 *
 * It's worth noting that this trivial protocol would be entirely
 * adequate to implement a TCP-based version of TFTP; just use "RRQ
 * <filename>" as the message.  Now, if only an appropriate server
 * existed...
 */

static inline struct hello_request *
tcp_to_hello ( struct tcp_application *app ) {
	return container_of ( app, struct hello_request, tcp );
}

static void hello_closed ( struct tcp_application *app, int status ) {
	struct hello_request *hello = tcp_to_hello ( app );

	async_done ( &hello->aop, status );
}

static void hello_connected ( struct tcp_application *app ) {
	struct hello_request *hello = tcp_to_hello ( app );

	hello->remaining = strlen ( hello->message );
	hello->state = HELLO_SENDING_MESSAGE;
}

static void hello_acked ( struct tcp_application *app, size_t len ) {
	struct hello_request *hello = tcp_to_hello ( app );
	
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

static void hello_newdata ( struct tcp_application *app, void *data,
			    size_t len ) {
	struct hello_request *hello = tcp_to_hello ( app );

	hello->callback ( data, len );
}

static void hello_senddata ( struct tcp_application *app,
			     void *buf __unused, size_t len __unused ) {
	struct hello_request *hello = tcp_to_hello ( app );

	tcp_send ( app, hello->message, hello->remaining );
}

static struct tcp_operations hello_tcp_operations = {
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
struct async_operation * say_hello ( struct hello_request *hello ) {
	int rc;

	hello->tcp.tcp_op = &hello_tcp_operations;
	if ( ( rc = tcp_connect ( &hello->tcp, &hello->server, 0 ) ) != 0 )
		async_done ( &hello->aop, rc );

	return &hello->aop;
}
