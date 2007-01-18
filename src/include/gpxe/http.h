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
#include <gpxe/linebuf.h>
#include <gpxe/uri.h>

/** HTTP default port */
#define HTTP_PORT 80

/** HTTP receive state */
enum http_rx_state {
	HTTP_RX_RESPONSE = 0,
	HTTP_RX_HEADER,
	HTTP_RX_DATA,
	HTTP_RX_DEAD,
};

/**
 * An HTTP request
 *
 */
struct http_request {
	/** URI being fetched */
	struct uri *uri;
	/** Data buffer to fill */
	struct buffer *buffer;
	/** Asynchronous operation */
	struct async async;

	/** HTTP response code */
	unsigned int response;
	/** HTTP Content-Length */
	size_t content_length;

	/** Server address */
	struct sockaddr server;
	/** TCP application for this request */
	struct tcp_application tcp;
	/** Number of bytes already sent */
	size_t tx_offset;
	/** RX state */
	enum http_rx_state rx_state;
	/** Line buffer for received header lines */
	struct line_buffer linebuf;
};

extern int http_get ( struct uri *uri, struct buffer *buffer,
		      struct async *parent );

#endif /* _GPXE_HTTP_H */
