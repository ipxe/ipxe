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

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * @file
 *
 * Hyper Text Transfer Protocol (HTTP)
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/uri.h>
#include <ipxe/refcnt.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/socket.h>
#include <ipxe/tcpip.h>
#include <ipxe/process.h>
#include <ipxe/linebuf.h>
#include <ipxe/features.h>
#include <ipxe/base64.h>
#include <ipxe/http.h>

FEATURE ( FEATURE_PROTOCOL, "HTTP", DHCP_EB_FEATURE_HTTP, 1 );

/** HTTP transmission state */
enum http_tx_state {
	HTTP_TX_REQUEST = 0,
	HTTP_TX_DONE,
};

/** HTTP receive state */
enum http_rx_state {
	HTTP_RX_RESPONSE = 0,
	HTTP_RX_HEADER,
	HTTP_RX_CHUNK_LEN,
	HTTP_RX_DATA,
	HTTP_RX_TRAILER,
	HTTP_RX_DEAD,
};

/**
 * An HTTP request
 *
 */
struct http_request {
	/** Reference count */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct interface xfer;

	/** URI being fetched */
	struct uri *uri;
	/** Transport layer interface */
	struct interface socket;

	/** TX process */
	struct process process;
	/** TX state */
	enum http_tx_state tx_state;

	/** HTTP response code */
	unsigned int response;
	/** HTTP Content-Length */
	size_t content_length;
	/** HTTP is using Transfer-Encoding: chunked */
	int chunked;
	/** Current chunk length */
	size_t chunk_len;
	/** Received length */
	size_t rx_len;
	/** RX state */
	enum http_rx_state rx_state;
	/** Line buffer for received header lines */
	struct line_buffer linebuf;
};

/**
 * Free HTTP request
 *
 * @v refcnt		Reference counter
 */
static void http_free ( struct refcnt *refcnt ) {
	struct http_request *http =
		container_of ( refcnt, struct http_request, refcnt );

	uri_put ( http->uri );
	empty_line_buffer ( &http->linebuf );
	free ( http );
};

/**
 * Mark HTTP request as complete
 *
 * @v http		HTTP request
 * @v rc		Return status code
 */
static void http_done ( struct http_request *http, int rc ) {

	/* Prevent further processing of any current packet */
	http->rx_state = HTTP_RX_DEAD;

	/* If we had a Content-Length, and the received content length
	 * isn't correct, flag an error
	 */
	if ( http->content_length &&
	     ( http->content_length != http->rx_len ) ) {
		DBGC ( http, "HTTP %p incorrect length %zd, should be %zd\n",
		       http, http->rx_len, http->content_length );
		rc = -EIO;
	}

	/* Remove process */
	process_del ( &http->process );

	/* Close all data transfer interfaces */
	intf_shutdown ( &http->socket, rc );
	intf_shutdown ( &http->xfer, rc );
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
	case 301:
	case 302:
		return 0;
	case 404:
		return -ENOENT;
	case 403:
		return -EPERM;
	case 401:
		return -EACCES;
	default:
		return -EIO;
	}
}

/**
 * Handle HTTP response
 *
 * @v http		HTTP request
 * @v response		HTTP response
 * @ret rc		Return status code
 */
static int http_rx_response ( struct http_request *http, char *response ) {
	char *spc;
	int rc;

	DBGC ( http, "HTTP %p response \"%s\"\n", http, response );

	/* Check response starts with "HTTP/" */
	if ( strncmp ( response, "HTTP/", 5 ) != 0 )
		return -EIO;

	/* Locate and check response code */
	spc = strchr ( response, ' ' );
	if ( ! spc )
		return -EIO;
	http->response = strtoul ( spc, NULL, 10 );
	if ( ( rc = http_response_to_rc ( http->response ) ) != 0 )
		return rc;

	/* Move to received headers */
	http->rx_state = HTTP_RX_HEADER;
	return 0;
}

/**
 * Handle HTTP Location header
 *
 * @v http		HTTP request
 * @v value		HTTP header value
 * @ret rc		Return status code
 */
static int http_rx_location ( struct http_request *http, const char *value ) {
	int rc;

	/* Redirect to new location */
	DBGC ( http, "HTTP %p redirecting to %s\n", http, value );
	if ( ( rc = xfer_redirect ( &http->xfer, LOCATION_URI_STRING,
				    value ) ) != 0 ) {
		DBGC ( http, "HTTP %p could not redirect: %s\n",
		       http, strerror ( rc ) );
		return rc;
	}

	return 0;
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

	/* Use seek() to notify recipient of filesize */
	xfer_seek ( &http->xfer, http->content_length );
	xfer_seek ( &http->xfer, 0 );

	return 0;
}

/**
 * Handle HTTP Transfer-Encoding header
 *
 * @v http		HTTP request
 * @v value		HTTP header value
 * @ret rc		Return status code
 */
static int http_rx_transfer_encoding ( struct http_request *http,
				       const char *value ) {

	if ( strcmp ( value, "chunked" ) == 0 ) {
		/* Mark connection as using chunked transfer encoding */
		http->chunked = 1;
	}

	return 0;
}

/** An HTTP header handler */
struct http_header_handler {
	/** Name (e.g. "Content-Length") */
	const char *header;
	/** Handle received header
	 *
	 * @v http	HTTP request
	 * @v value	HTTP header value
	 * @ret rc	Return status code
	 *
	 * If an error is returned, the download will be aborted.
	 */
	int ( * rx ) ( struct http_request *http, const char *value );
};

/** List of HTTP header handlers */
static struct http_header_handler http_header_handlers[] = {
	{
		.header = "Location",
		.rx = http_rx_location,
	},
	{
		.header = "Content-Length",
		.rx = http_rx_content_length,
	},
	{
		.header = "Transfer-Encoding",
		.rx = http_rx_transfer_encoding,
	},
	{ NULL, NULL }
};

/**
 * Handle HTTP header
 *
 * @v http		HTTP request
 * @v header		HTTP header
 * @ret rc		Return status code
 */
static int http_rx_header ( struct http_request *http, char *header ) {
	struct http_header_handler *handler;
	char *separator;
	char *value;
	int rc;

	/* An empty header line marks the end of this phase */
	if ( ! header[0] ) {
		empty_line_buffer ( &http->linebuf );
		if ( http->rx_state == HTTP_RX_HEADER ) {
			DBGC ( http, "HTTP %p start of data\n", http );
			http->rx_state = ( http->chunked ?
					   HTTP_RX_CHUNK_LEN : HTTP_RX_DATA );
			return 0;
		} else {
			DBGC ( http, "HTTP %p end of trailer\n", http );
			http_done ( http, 0 );
			return 0;
		}
	}

	DBGC ( http, "HTTP %p header \"%s\"\n", http, header );

	/* Split header at the ": " */
	separator = strstr ( header, ": " );
	if ( ! separator ) {
		DBGC ( http, "HTTP %p malformed header\n", http );
		return -EIO;
	}
	*separator = '\0';
	value = ( separator + 2 );

	/* Hand off to header handler, if one exists */
	for ( handler = http_header_handlers ; handler->header ; handler++ ) {
		if ( strcasecmp ( header, handler->header ) == 0 ) {
			if ( ( rc = handler->rx ( http, value ) ) != 0 )
				return rc;
			break;
		}
	}
	return 0;
}

/**
 * Handle HTTP chunk length
 *
 * @v http		HTTP request
 * @v length		HTTP chunk length
 * @ret rc		Return status code
 */
static int http_rx_chunk_len ( struct http_request *http, char *length ) {
	char *endp;

	/* Skip blank lines between chunks */
	if ( length[0] == '\0' )
		return 0;

	/* Parse chunk length */
	http->chunk_len = strtoul ( length, &endp, 16 );
	if ( *endp != '\0' ) {
		DBGC ( http, "HTTP %p invalid chunk length \"%s\"\n",
		       http, length );
		return -EIO;
	}

	/* Terminate chunked encoding if applicable */
	if ( http->chunk_len == 0 ) {
		DBGC ( http, "HTTP %p end of chunks\n", http );
		http->chunked = 0;
		http->rx_state = HTTP_RX_TRAILER;
		return 0;
	}

	/* Use seek() to notify recipient of new filesize */
	DBGC ( http, "HTTP %p start of chunk of length %zd\n",
	       http, http->chunk_len );
	xfer_seek ( &http->xfer, ( http->rx_len + http->chunk_len ) );
	xfer_seek ( &http->xfer, http->rx_len );

	/* Start receiving data */
	http->rx_state = HTTP_RX_DATA;

	return 0;
}

/** An HTTP line-based data handler */
struct http_line_handler {
	/** Handle line
	 *
	 * @v http	HTTP request
	 * @v line	Line to handle
	 * @ret rc	Return status code
	 */
	int ( * rx ) ( struct http_request *http, char *line );
};

/** List of HTTP line-based data handlers */
static struct http_line_handler http_line_handlers[] = {
	[HTTP_RX_RESPONSE]	= { .rx = http_rx_response },
	[HTTP_RX_HEADER]	= { .rx = http_rx_header },
	[HTTP_RX_CHUNK_LEN]	= { .rx = http_rx_chunk_len },
	[HTTP_RX_TRAILER]	= { .rx = http_rx_header },
};

/**
 * Handle new data arriving via HTTP connection
 *
 * @v http		HTTP request
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int http_socket_deliver ( struct http_request *http,
				 struct io_buffer *iobuf,
				 struct xfer_metadata *meta __unused ) {
	struct http_line_handler *lh;
	char *line;
	size_t data_len;
	ssize_t line_len;
	int rc = 0;

	while ( iobuf && iob_len ( iobuf ) ) {
		switch ( http->rx_state ) {
		case HTTP_RX_DEAD:
			/* Do no further processing */
			goto done;
		case HTTP_RX_DATA:
			/* Pass received data to caller */
			data_len = iob_len ( iobuf );
			if ( http->chunk_len && ( http->chunk_len < data_len )){
				data_len = http->chunk_len;
				rc = xfer_deliver_raw ( &http->xfer,
							iobuf->data, data_len );
				iob_pull ( iobuf, data_len );
			} else {
				rc = xfer_deliver_iob ( &http->xfer,
							iob_disown ( iobuf ) );
			}
			if ( rc != 0 )
				goto done;
			if ( http->chunk_len ) {
				http->chunk_len -= data_len;
				if ( http->chunk_len == 0 )
					http->rx_state = HTTP_RX_CHUNK_LEN;
			}
			http->rx_len += data_len;
			if ( http->content_length &&
			     ( http->rx_len >= http->content_length ) ) {
				http_done ( http, 0 );
				goto done;
			}
			break;
		case HTTP_RX_RESPONSE:
		case HTTP_RX_HEADER:
		case HTTP_RX_CHUNK_LEN:
		case HTTP_RX_TRAILER:
			/* In the other phases, buffer and process a
			 * line at a time
			 */
			line_len = line_buffer ( &http->linebuf, iobuf->data,
						 iob_len ( iobuf ) );
			if ( line_len < 0 ) {
				rc = line_len;
				DBGC ( http, "HTTP %p could not buffer line: "
				       "%s\n", http, strerror ( rc ) );
				goto done;
			}
			iob_pull ( iobuf, line_len );
			line = buffered_line ( &http->linebuf );
			if ( line ) {
				lh = &http_line_handlers[http->rx_state];
				if ( ( rc = lh->rx ( http, line ) ) != 0 )
					goto done;
			}
			break;
		default:
			assert ( 0 );
			break;
		}
	}

 done:
	if ( rc )
		http_done ( http, rc );
	free_iob ( iobuf );
	return rc;
}

/**
 * HTTP process
 *
 * @v http		HTTP request
 */
static void http_step ( struct http_request *http ) {
	const char *host = http->uri->host;
	const char *user = http->uri->user;
	const char *password =
		( http->uri->password ? http->uri->password : "" );
	size_t user_pw_len = ( user ? ( strlen ( user ) + 1 /* ":" */ +
					strlen ( password ) ) : 0 );
	size_t user_pw_base64_len = base64_encoded_len ( user_pw_len );
	uint8_t user_pw[ user_pw_len + 1 /* NUL */ ];
	char user_pw_base64[ user_pw_base64_len + 1 /* NUL */ ];
	int rc;
	int request_len = unparse_uri ( NULL, 0, http->uri,
					URI_PATH_BIT | URI_QUERY_BIT );
	char request[ request_len + 1 /* NUL */ ];

	/* Do nothing if we have already transmitted the request */
	if ( http->tx_state != HTTP_TX_REQUEST )
		return;

	/* Do nothing until socket is ready */
	if ( ! xfer_window ( &http->socket ) )
		return;

	/* Construct path?query request */
	unparse_uri ( request, sizeof ( request ), http->uri,
		      URI_PATH_BIT | URI_QUERY_BIT );

	/* Construct authorisation, if applicable */
	if ( user ) {
		/* Make "user:password" string from decoded fields */
		snprintf ( ( ( char * ) user_pw ), sizeof ( user_pw ),
			   "%s:%s", user, password );

		/* Base64-encode the "user:password" string */
		base64_encode ( user_pw, user_pw_len, user_pw_base64 );
	}

	/* Mark request as transmitted */
	http->tx_state = HTTP_TX_DONE;

	/* Send GET request */
	if ( ( rc = xfer_printf ( &http->socket,
				  "GET %s%s HTTP/1.1\r\n"
				  "User-Agent: iPXE/" VERSION "\r\n"
				  "%s%s%s"
				  "Host: %s\r\n"
				  "\r\n",
				  http->uri->path ? "" : "/",
				  request,
				  ( user ?
				    "Authorization: Basic " : "" ),
				  ( user ? user_pw_base64 : "" ),
				  ( user ? "\r\n" : "" ),
				  host ) ) != 0 ) {
		http_done ( http, rc );
	}
}

/** HTTP socket interface operations */
static struct interface_operation http_socket_operations[] = {
	INTF_OP ( xfer_deliver, struct http_request *, http_socket_deliver ),
	INTF_OP ( xfer_window_changed, struct http_request *, http_step ),
	INTF_OP ( intf_close, struct http_request *, http_done ),
};

/** HTTP socket interface descriptor */
static struct interface_descriptor http_socket_desc =
	INTF_DESC_PASSTHRU ( struct http_request, socket,
			     http_socket_operations, xfer );

/** HTTP data transfer interface operations */
static struct interface_operation http_xfer_operations[] = {
	INTF_OP ( intf_close, struct http_request *, http_done ),
};

/** HTTP data transfer interface descriptor */
static struct interface_descriptor http_xfer_desc =
	INTF_DESC_PASSTHRU ( struct http_request, xfer,
			     http_xfer_operations, socket );

/** HTTP process descriptor */
static struct process_descriptor http_process_desc =
	PROC_DESC_ONCE ( struct http_request, process, http_step );

/**
 * Initiate an HTTP connection, with optional filter
 *
 * @v xfer		Data transfer interface
 * @v uri		Uniform Resource Identifier
 * @v default_port	Default port number
 * @v filter		Filter to apply to socket, or NULL
 * @ret rc		Return status code
 */
int http_open_filter ( struct interface *xfer, struct uri *uri,
		       unsigned int default_port,
		       int ( * filter ) ( struct interface *xfer,
					  struct interface **next ) ) {
	struct http_request *http;
	struct sockaddr_tcpip server;
	struct interface *socket;
	int rc;

	/* Sanity checks */
	if ( ! uri->host )
		return -EINVAL;

	/* Allocate and populate HTTP structure */
	http = zalloc ( sizeof ( *http ) );
	if ( ! http )
		return -ENOMEM;
	ref_init ( &http->refcnt, http_free );
	intf_init ( &http->xfer, &http_xfer_desc, &http->refcnt );
       	http->uri = uri_get ( uri );
	intf_init ( &http->socket, &http_socket_desc, &http->refcnt );
	process_init ( &http->process, &http_process_desc, &http->refcnt );

	/* Open socket */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( http->uri, default_port ) );
	socket = &http->socket;
	if ( filter ) {
		if ( ( rc = filter ( socket, &socket ) ) != 0 )
			goto err;
	}
	if ( ( rc = xfer_open_named_socket ( socket, SOCK_STREAM,
					     ( struct sockaddr * ) &server,
					     uri->host, NULL ) ) != 0 )
		goto err;

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &http->xfer, xfer );
	ref_put ( &http->refcnt );
	return 0;

 err:
	DBGC ( http, "HTTP %p could not create request: %s\n", 
	       http, strerror ( rc ) );
	http_done ( http, rc );
	ref_put ( &http->refcnt );
	return rc;
}

/**
 * Initiate an HTTP connection
 *
 * @v xfer		Data transfer interface
 * @v uri		Uniform Resource Identifier
 * @ret rc		Return status code
 */
static int http_open ( struct interface *xfer, struct uri *uri ) {
	return http_open_filter ( xfer, uri, HTTP_PORT, NULL );
}

/** HTTP URI opener */
struct uri_opener http_uri_opener __uri_opener = {
	.scheme	= "http",
	.open	= http_open,
};
