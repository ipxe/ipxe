/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Hyper Text Transfer Protocol (HTTP)
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/uri.h>
#include <gpxe/buffer.h>
#include <gpxe/http.h>

static inline struct http_request *
tcp_to_http ( struct tcp_application *app ) {
	return container_of ( app, struct http_request, tcp );
}

/**
 * Mark HTTP request as complete
 *
 * @v http		HTTP request
 * @v rc		Return status code
 *
 */
static void http_done ( struct http_request *http, int rc ) {

	/* Close TCP connection */
	tcp_close ( &http->tcp );

	/* Prevent further processing of any current packet */
	http->rx_state = HTTP_RX_DEAD;

	/* Free up any dynamically allocated storage */
	empty_line_buffer ( &http->linebuf );

	/* If we had a Content-Length, and the received content length
	 * isn't correct, flag an error
	 */
	if ( http->content_length &&
	     ( http->content_length != http->buffer->fill ) ) {
		DBGC ( http, "HTTP %p incorrect length %zd, should be %zd\n",
		       http, http->buffer->fill, http->content_length );
		rc = -EIO;
	}

	/* Mark async operation as complete */
	async_done ( &http->async, rc );
}

/**
 * Convert HTTP response code to return status code
 *
 * @v response		HTTP response code
 * @ret rc		Return status code
 */
static int http_response_to_rc ( unsigned int response ) {
	switch ( response ) {
	case 200:
		return 0;
	case 404:
		return -ENOENT;
	case 403:
		return -EPERM;
	default:
		return -EIO;
	}
}

/**
 * Handle HTTP response
 *
 * @v http		HTTP request
 * @v response		HTTP response
 */
static void http_rx_response ( struct http_request *http, char *response ) {
	char *spc;
	int rc = -EIO;

	DBGC ( http, "HTTP %p response \"%s\"\n", http, response );

	/* Check response starts with "HTTP/" */
	if ( strncmp ( response, "HTTP/", 5 ) != 0 )
		goto err;

	/* Locate and check response code */
	spc = strchr ( response, ' ' );
	if ( ! spc )
		goto err;
	http->response = strtoul ( spc, NULL, 10 );
	if ( ( rc = http_response_to_rc ( http->response ) ) != 0 )
		goto err;

	/* Move to received headers */
	http->rx_state = HTTP_RX_HEADER;
	return;

 err:
	DBGC ( http, "HTTP %p bad response\n", http );
	http_done ( http, rc );
	return;
}

/**
 * Handle HTTP Content-Length header
 *
 * @v http		HTTP request
 * @v value		HTTP header value
 * @ret rc		Return status code
 */
static int http_rx_content_length ( struct http_request *http,
				    const char *value ) {
	char *endp;

	http->content_length = strtoul ( value, &endp, 10 );
	if ( *endp != '\0' ) {
		DBGC ( http, "HTTP %p invalid Content-Length \"%s\"\n",
		       http, value );
		return -EIO;
	}

	return 0;
}

/**
 * An HTTP header handler
 *
 */
struct http_header_handler {
	/** Name (e.g. "Content-Length") */
	const char *header;
	/** Handle received header
	 *
	 * @v http	HTTP request
	 * @v value	HTTP header value
	 * @ret rc	Return status code
	 */
	int ( * rx ) ( struct http_request *http, const char *value );
};

/** List of HTTP header handlers */
struct http_header_handler http_header_handlers[] = {
	{
		.header = "Content-Length",
		.rx = http_rx_content_length,
	},
	{ NULL, NULL }
};

/**
 * Handle HTTP header
 *
 * @v http		HTTP request
 * @v header		HTTP header
 */
static void http_rx_header ( struct http_request *http, char *header ) {
	struct http_header_handler *handler;
	char *separator;
	char *value;
	int rc = -EIO;

	/* An empty header line marks the transition to the data phase */
	if ( ! header[0] ) {
		DBGC ( http, "HTTP %p start of data\n", http );
		empty_line_buffer ( &http->linebuf );
		http->rx_state = HTTP_RX_DATA;
		return;
	}

	DBGC ( http, "HTTP %p header \"%s\"\n", http, header );

	/* Split header at the ": " */
	separator = strstr ( header, ": " );
	if ( ! separator )
		goto err;
	*separator = '\0';
	value = ( separator + 2 );

	/* Hand off to header handler, if one exists */
	for ( handler = http_header_handlers ; handler->header ; handler++ ) {
		if ( strcasecmp ( header, handler->header ) == 0 ) {
			if ( ( rc = handler->rx ( http, value ) ) != 0 )
				goto err;
			break;
		}
	}
	return;

 err:
	DBGC ( http, "HTTP %p bad header\n", http );
	http_done ( http, rc );
	return;
}

/**
 * Handle new data arriving via HTTP connection in the data phase
 *
 * @v http		HTTP request
 * @v data		New data
 * @v len		Length of new data
 */
static void http_rx_data ( struct http_request *http,
			   const char *data, size_t len ) {
	int rc;

	/* Fill data buffer */
	if ( ( rc = fill_buffer ( http->buffer, data,
				  http->buffer->fill, len ) ) != 0 ) {
		DBGC ( http, "HTTP %p failed to fill data buffer: %s\n",
		       http, strerror ( rc ) );
		http_done ( http, rc );
		return;
	}

	/* If we have reached the content-length, stop now */
	if ( http->content_length &&
	     ( http->buffer->fill >= http->content_length ) ) {
		http_done ( http, 0 );
	}
}

/**
 * Handle new data arriving via HTTP connection
 *
 * @v http		HTTP request
 * @v data		New data
 * @v len		Length of new data
 */
static void http_newdata ( struct tcp_application *app,
			   void *data, size_t len ) {
	struct http_request *http = tcp_to_http ( app );
	const char *buf = data;
	char *line;
	int rc;

	while ( len ) {
		if ( http->rx_state == HTTP_RX_DEAD ) {
			/* Do no further processing */
			return;
		} else if ( http->rx_state == HTTP_RX_DATA ) {
			/* Once we're into the data phase, just fill
			 * the data buffer
			 */
			http_rx_data ( http, buf, len );
			return;
		} else {
			/* In the other phases, buffer and process a
			 * line at a time
			 */
			if ( ( rc = line_buffer ( &http->linebuf, &buf,
						  &len ) ) != 0 ) {
				DBGC ( http, "HTTP %p could not buffer line: "
				       "%s\n", http, strerror ( rc ) );
				http_done ( http, rc );
				return;
			}
			if ( ( line = buffered_line ( &http->linebuf ) ) ) {
				switch ( http->rx_state ) {
				case HTTP_RX_RESPONSE:
					http_rx_response ( http, line );
					break;
				case HTTP_RX_HEADER:
					http_rx_header ( http, line );
					break;
				default:
					assert ( 0 );
					break;
				}
			}
		}
	}
}

/**
 * Send HTTP data
 *
 * @v app		TCP application
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 */
static void http_senddata ( struct tcp_application *app,
			    void *buf, size_t len ) {
	struct http_request *http = tcp_to_http ( app );
	const char *path = http->uri->path;
	const char *host = http->uri->host;

	if ( ! path )
		path = "/";

	if ( ! host )
		host = "";

	len = snprintf ( buf, len,
			 "GET %s HTTP/1.1\r\n"
			 "User-Agent: gPXE/" VERSION "\r\n"
			 "Host: %s\r\n"
			 "\r\n", path, host );

	tcp_send ( app, ( buf + http->tx_offset ), ( len - http->tx_offset ) );
}

/**
 * HTTP data acknowledged
 *
 * @v app		TCP application
 * @v len		Length of acknowledged data
 */
static void http_acked ( struct tcp_application *app, size_t len ) {
	struct http_request *http = tcp_to_http ( app );

	http->tx_offset += len;
}

/**
 * HTTP connection closed by network stack
 *
 * @v app		TCP application
 */
static void http_closed ( struct tcp_application *app, int rc ) {
	struct http_request *http = tcp_to_http ( app );

	DBGC ( http, "HTTP %p connection closed: %s\n",
	       http, strerror ( rc ) );
	
	http_done ( http, rc );
}

/** HTTP TCP operations */
static struct tcp_operations http_tcp_operations = {
	.closed		= http_closed,
	.acked		= http_acked,
	.newdata	= http_newdata,
	.senddata	= http_senddata,
};

/**
 * Reap asynchronous operation
 *
 * @v async		Asynchronous operation
 */
static void http_reap ( struct async *async ) {
	struct http_request *http =
		container_of ( async, struct http_request, async );

	free_uri ( http->uri );
	free ( http );
}

/** HTTP asynchronous operations */
static struct async_operations http_async_operations = {
	.reap			= http_reap,
};

#warning "Quick name resolution hack"
#include <byteswap.h>

/**
 * Initiate a HTTP connection
 *
 * @v uri		Uniform Resource Identifier
 * @v buffer		Buffer into which to download file
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 *
 * If it returns success, this function takes ownership of the URI.
 */
int http_get ( struct uri *uri, struct buffer *buffer, struct async *parent ) {
	struct http_request *http;
	int rc;

	/* Allocate and populate HTTP structure */
	http = malloc ( sizeof ( *http ) );
	if ( ! http ) {
		rc = -ENOMEM;
		goto err;
	}
	memset ( http, 0, sizeof ( *http ) );
	http->uri = uri;
	http->buffer = buffer;
	http->tcp.tcp_op = &http_tcp_operations;

#warning "Quick name resolution hack"
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;
	server.sin.sin_port = htons ( HTTP_PORT );
	server.sin.sin_family = AF_INET;
	if ( inet_aton ( uri->host, &server.sin.sin_addr ) == 0 ) {
		rc = -EINVAL;
		goto err;
	}

	if ( ( rc = tcp_connect ( &http->tcp, &server.st, 0 ) ) != 0 )
		goto err;

	async_init ( &http->async, &http_async_operations, parent );
	return 0;

 err:
	DBGC ( http, "HTTP %p could not create request: %s\n", 
	       http, strerror ( rc ) );
	return rc;
}
