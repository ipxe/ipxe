#ifndef _GPXE_HTTP_H
#define _GPXE_HTTP_H

/** @file
 *
 * Hyper Text Transport Protocol
 *
 */

#include <stdint.h>
#include <gpxe/tcp.h>
#include <gpxe/async.h>

/** HTTP default port */
#define HTTP_PORT 80

enum http_state {
	HTTP_INIT_CONN = 0,
	HTTP_REQUEST_FILE,
	HTTP_PARSE_HEADER,
	HTTP_RECV_FILE,
	HTTP_DONE,
};

/**
 * A HTTP request
 *
 */
struct http_request;

struct http_request {
	/** TCP connection for this request */
	struct tcp_connection tcp;
	/** Current state */
	enum http_state state;
        /** File to download */
        const char *filename;
        /** Size of file downloading */
        size_t file_size;
	/** Number of bytes recieved so far */
	size_t file_recv;
	/** Callback function
	 *
	 * @v http	HTTP request struct
	 * @v data	Received data
	 * @v len	Length of received data
	 *
	 * This function is called for all data received from the
	 * remote server.
	 */
	void ( *callback ) ( struct http_request *http, char *data, size_t len );
	/** Asynchronous operation */
	struct async_operation aop;
};

extern struct async_operation * get_http ( struct http_request *http );

#endif /* _GPXE_HTTP_H */
