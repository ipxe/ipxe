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
 * Hyper Text Transfer Protocol (HTTP) core functionality
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
#include <ipxe/base64.h>
#include <ipxe/blockdev.h>
#include <ipxe/acpi.h>
#include <ipxe/http.h>

/* Disambiguate the various error causes */
#define EACCES_401 __einfo_error ( EINFO_EACCES_401 )
#define EINFO_EACCES_401 \
	__einfo_uniqify ( EINFO_EACCES, 0x01, "HTTP 401 Unauthorized" )
#define EIO_OTHER __einfo_error ( EINFO_EIO_OTHER )
#define EINFO_EIO_OTHER \
	__einfo_uniqify ( EINFO_EIO, 0x01, "Unrecognised HTTP response code" )
#define EIO_CONTENT_LENGTH __einfo_error ( EINFO_EIO_CONTENT_LENGTH )
#define EINFO_EIO_CONTENT_LENGTH \
	__einfo_uniqify ( EINFO_EIO, 0x02, "Content length mismatch" )
#define EINVAL_RESPONSE __einfo_error ( EINFO_EINVAL_RESPONSE )
#define EINFO_EINVAL_RESPONSE \
	__einfo_uniqify ( EINFO_EINVAL, 0x01, "Invalid content length" )
#define EINVAL_HEADER __einfo_error ( EINFO_EINVAL_HEADER )
#define EINFO_EINVAL_HEADER \
	__einfo_uniqify ( EINFO_EINVAL, 0x02, "Invalid header" )
#define EINVAL_CONTENT_LENGTH __einfo_error ( EINFO_EINVAL_CONTENT_LENGTH )
#define EINFO_EINVAL_CONTENT_LENGTH \
	__einfo_uniqify ( EINFO_EINVAL, 0x03, "Invalid content length" )
#define EINVAL_CHUNK_LENGTH __einfo_error ( EINFO_EINVAL_CHUNK_LENGTH )
#define EINFO_EINVAL_CHUNK_LENGTH \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "Invalid chunk length" )
#define ENOENT_404 __einfo_error ( EINFO_ENOENT_404 )
#define EINFO_ENOENT_404 \
	__einfo_uniqify ( EINFO_ENOENT, 0x01, "HTTP 404 Not Found" )
#define EPERM_403 __einfo_error ( EINFO_EPERM_403 )
#define EINFO_EPERM_403 \
	__einfo_uniqify ( EINFO_EPERM, 0x01, "HTTP 403 Forbidden" )
#define EPROTO_UNSOLICITED __einfo_error ( EINFO_EPROTO_UNSOLICITED )
#define EINFO_EPROTO_UNSOLICITED \
	__einfo_uniqify ( EINFO_EPROTO, 0x01, "Unsolicited data" )

/** Block size used for HTTP block device request */
#define HTTP_BLKSIZE 512

/** HTTP flags */
enum http_flags {
	/** Request is waiting to be transmitted */
	HTTP_TX_PENDING = 0x0001,
	/** Fetch header only */
	HTTP_HEAD_ONLY = 0x0002,
	/** Keep connection alive */
	HTTP_KEEPALIVE = 0x0004,
};

/** HTTP receive state */
enum http_rx_state {
	HTTP_RX_RESPONSE = 0,
	HTTP_RX_HEADER,
	HTTP_RX_CHUNK_LEN,
	HTTP_RX_DATA,
	HTTP_RX_TRAILER,
	HTTP_RX_IDLE,
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
	/** Partial transfer interface */
	struct interface partial;

	/** URI being fetched */
	struct uri *uri;
	/** Transport layer interface */
	struct interface socket;

	/** Flags */
	unsigned int flags;
	/** Starting offset of partial transfer (if applicable) */
	size_t partial_start;
	/** Length of partial transfer (if applicable) */
	size_t partial_len;

	/** TX process */
	struct process process;

	/** RX state */
	enum http_rx_state rx_state;
	/** Received length */
	size_t rx_len;
	/** Length remaining (or 0 if unknown) */
	size_t remaining;
	/** HTTP is using Transfer-Encoding: chunked */
	int chunked;
	/** Current chunk length remaining (if applicable) */
	size_t chunk_remaining;
	/** Line buffer for received header lines */
	struct line_buffer linebuf;
	/** Receive data buffer (if applicable) */
	userptr_t rx_buffer;
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
 * Close HTTP request
 *
 * @v http		HTTP request
 * @v rc		Return status code
 */
static void http_close ( struct http_request *http, int rc ) {

	/* Prevent further processing of any current packet */
	http->rx_state = HTTP_RX_DEAD;

	/* If we had a Content-Length, and the received content length
	 * isn't correct, flag an error
	 */
	if ( http->remaining != 0 ) {
		DBGC ( http, "HTTP %p incorrect length %zd, should be %zd\n",
		       http, http->rx_len, ( http->rx_len + http->remaining ) );
		if ( rc == 0 )
			rc = -EIO_CONTENT_LENGTH;
	}

	/* Remove process */
	process_del ( &http->process );

	/* Close all data transfer interfaces */
	intf_shutdown ( &http->socket, rc );
	intf_shutdown ( &http->partial, rc );
	intf_shutdown ( &http->xfer, rc );
}

/**
 * Mark HTTP request as completed successfully
 *
 * @v http		HTTP request
 */
static void http_done ( struct http_request *http ) {

	/* If we had a Content-Length, and the received content length
	 * isn't correct, force an error
	 */
	if ( http->remaining != 0 ) {
		http_close ( http, -EIO_CONTENT_LENGTH );
		return;
	}

	/* Enter idle state */
	http->rx_state = HTTP_RX_IDLE;
	http->rx_len = 0;
	assert ( http->remaining == 0 );
	assert ( http->chunked == 0 );
	assert ( http->chunk_remaining == 0 );

	/* Close partial transfer interface */
	intf_restart ( &http->partial, 0 );

	/* Close everything unless we are keeping the connection alive */
	if ( ! ( http->flags & HTTP_KEEPALIVE ) )
		http_close ( http, 0 );
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
	case 206:
	case 301:
	case 302:
	case 303:
		return 0;
	case 404:
		return -ENOENT_404;
	case 403:
		return -EPERM_403;
	case 401:
		return -EACCES_401;
	default:
		return -EIO_OTHER;
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
	unsigned int code;
	int rc;

	DBGC ( http, "HTTP %p response \"%s\"\n", http, response );

	/* Check response starts with "HTTP/" */
	if ( strncmp ( response, "HTTP/", 5 ) != 0 )
		return -EINVAL_RESPONSE;

	/* Locate and check response code */
	spc = strchr ( response, ' ' );
	if ( ! spc )
		return -EINVAL_RESPONSE;
	code = strtoul ( spc, NULL, 10 );
	if ( ( rc = http_response_to_rc ( code ) ) != 0 )
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
	struct block_device_capacity capacity;
	size_t content_len;
	char *endp;

	/* Parse content length */
	content_len = strtoul ( value, &endp, 10 );
	if ( *endp != '\0' ) {
		DBGC ( http, "HTTP %p invalid Content-Length \"%s\"\n",
		       http, value );
		return -EINVAL_CONTENT_LENGTH;
	}

	/* If we already have an expected content length, and this
	 * isn't it, then complain
	 */
	if ( http->remaining && ( http->remaining != content_len ) ) {
		DBGC ( http, "HTTP %p incorrect Content-Length %zd (expected "
		       "%zd)\n", http, content_len, http->remaining );
		return -EIO_CONTENT_LENGTH;
	}
	if ( ! ( http->flags & HTTP_HEAD_ONLY ) )
		http->remaining = content_len;

	/* Use seek() to notify recipient of filesize */
	xfer_seek ( &http->xfer, http->remaining );
	xfer_seek ( &http->xfer, 0 );

	/* Report block device capacity if applicable */
	if ( http->flags & HTTP_HEAD_ONLY ) {
		capacity.blocks = ( content_len / HTTP_BLKSIZE );
		capacity.blksize = HTTP_BLKSIZE;
		capacity.max_count = -1U;
		block_capacity ( &http->partial, &capacity );
	}
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
		if ( ( http->rx_state == HTTP_RX_HEADER ) &&
		     ( ! ( http->flags & HTTP_HEAD_ONLY ) ) ) {
			DBGC ( http, "HTTP %p start of data\n", http );
			http->rx_state = ( http->chunked ?
					   HTTP_RX_CHUNK_LEN : HTTP_RX_DATA );
			return 0;
		} else {
			DBGC ( http, "HTTP %p end of trailer\n", http );
			http_done ( http );
			return 0;
		}
	}

	DBGC ( http, "HTTP %p header \"%s\"\n", http, header );

	/* Split header at the ": " */
	separator = strstr ( header, ": " );
	if ( ! separator ) {
		DBGC ( http, "HTTP %p malformed header\n", http );
		return -EINVAL_HEADER;
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
	http->chunk_remaining = strtoul ( length, &endp, 16 );
	if ( *endp != '\0' ) {
		DBGC ( http, "HTTP %p invalid chunk length \"%s\"\n",
		       http, length );
		return -EINVAL_CHUNK_LENGTH;
	}

	/* Terminate chunked encoding if applicable */
	if ( http->chunk_remaining == 0 ) {
		DBGC ( http, "HTTP %p end of chunks\n", http );
		http->chunked = 0;
		http->rx_state = HTTP_RX_TRAILER;
		return 0;
	}

	/* Use seek() to notify recipient of new filesize */
	DBGC ( http, "HTTP %p start of chunk of length %zd\n",
	       http, http->chunk_remaining );
	xfer_seek ( &http->xfer, ( http->rx_len + http->chunk_remaining ) );
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
		case HTTP_RX_IDLE:
			/* Receiving any data in this state is an error */
			DBGC ( http, "HTTP %p received %zd bytes while %s\n",
			       http, iob_len ( iobuf ),
			       ( ( http->rx_state == HTTP_RX_IDLE ) ?
				 "idle" : "dead" ) );
			rc = -EPROTO_UNSOLICITED;
			goto done;
		case HTTP_RX_DEAD:
			/* Do no further processing */
			goto done;
		case HTTP_RX_DATA:
			/* Pass received data to caller */
			data_len = iob_len ( iobuf );
			if ( http->chunk_remaining &&
			     ( http->chunk_remaining < data_len ) ) {
				data_len = http->chunk_remaining;
			}
			if ( http->remaining &&
			     ( http->remaining < data_len ) ) {
				data_len = http->remaining;
			}
			if ( http->rx_buffer != UNULL ) {
				/* Copy to partial transfer buffer */
				copy_to_user ( http->rx_buffer, http->rx_len,
					       iobuf->data, data_len );
				iob_pull ( iobuf, data_len );
			} else if ( data_len < iob_len ( iobuf ) ) {
				/* Deliver partial buffer as raw data */
				rc = xfer_deliver_raw ( &http->xfer,
							iobuf->data, data_len );
				iob_pull ( iobuf, data_len );
				if ( rc != 0 )
					goto done;
			} else {
				/* Deliver whole I/O buffer */
				if ( ( rc = xfer_deliver_iob ( &http->xfer,
						 iob_disown ( iobuf ) ) ) != 0 )
					goto done;
			}
			http->rx_len += data_len;
			if ( http->chunk_remaining ) {
				http->chunk_remaining -= data_len;
				if ( http->chunk_remaining == 0 )
					http->rx_state = HTTP_RX_CHUNK_LEN;
			}
			if ( http->remaining ) {
				http->remaining -= data_len;
				if ( ( http->remaining == 0 ) &&
				     ( http->rx_state == HTTP_RX_DATA ) ) {
					http_done ( http );
				}
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
		http_close ( http, rc );
	free_iob ( iobuf );
	return rc;
}

/**
 * Check HTTP socket flow control window
 *
 * @v http		HTTP request
 * @ret len		Length of window
 */
static size_t http_socket_window ( struct http_request *http __unused ) {

	/* Window is always open.  This is to prevent TCP from
	 * stalling if our parent window is not currently open.
	 */
	return ( ~( ( size_t ) 0 ) );
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
	int request_len = unparse_uri ( NULL, 0, http->uri,
					URI_PATH_BIT | URI_QUERY_BIT );
	struct {
		uint8_t user_pw[ user_pw_len + 1 /* NUL */ ];
		char user_pw_base64[ user_pw_base64_len + 1 /* NUL */ ];
		char request[ request_len + 1 /* NUL */ ];
		char range[48]; /* Enough for two 64-bit integers in decimal */
	} *dynamic;
	int partial;
	int rc;

	/* Do nothing if we have already transmitted the request */
	if ( ! ( http->flags & HTTP_TX_PENDING ) )
		return;

	/* Do nothing until socket is ready */
	if ( ! xfer_window ( &http->socket ) )
		return;

	/* Allocate dynamic storage */
	dynamic = malloc ( sizeof ( *dynamic ) );
	if ( ! dynamic ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Construct path?query request */
	unparse_uri ( dynamic->request, sizeof ( dynamic->request ), http->uri,
		      URI_PATH_BIT | URI_QUERY_BIT );

	/* Construct authorisation, if applicable */
	if ( user ) {
		/* Make "user:password" string from decoded fields */
		snprintf ( ( ( char * ) dynamic->user_pw ),
			   sizeof ( dynamic->user_pw ), "%s:%s",
			   user, password );

		/* Base64-encode the "user:password" string */
		base64_encode ( dynamic->user_pw, user_pw_len,
				dynamic->user_pw_base64 );
	}

	/* Force a HEAD request if we have nowhere to send any received data */
	if ( ( xfer_window ( &http->xfer ) == 0 ) &&
	     ( http->rx_buffer == UNULL ) ) {
		http->flags |= ( HTTP_HEAD_ONLY | HTTP_KEEPALIVE );
	}

	/* Determine type of request */
	partial = ( http->partial_len != 0 );
	snprintf ( dynamic->range, sizeof ( dynamic->range ),
		   "%zd-%zd", http->partial_start,
		   ( http->partial_start + http->partial_len - 1 ) );

	/* Mark request as transmitted */
	http->flags &= ~HTTP_TX_PENDING;

	/* Send GET request */
	if ( ( rc = xfer_printf ( &http->socket,
				  "%s %s%s HTTP/1.1\r\n"
				  "User-Agent: iPXE/" VERSION "\r\n"
				  "Host: %s%s%s\r\n"
				  "%s%s%s%s%s%s%s"
				  "\r\n",
				  ( ( http->flags & HTTP_HEAD_ONLY ) ?
				    "HEAD" : "GET" ),
				  ( http->uri->path ? "" : "/" ),
				  dynamic->request, host,
				  ( http->uri->port ?
				    ":" : "" ),
				  ( http->uri->port ?
				    http->uri->port : "" ),
				  ( ( http->flags & HTTP_KEEPALIVE ) ?
				    "Connection: Keep-Alive\r\n" : "" ),
				  ( partial ? "Range: bytes=" : "" ),
				  ( partial ? dynamic->range : "" ),
				  ( partial ? "\r\n" : "" ),
				  ( user ?
				    "Authorization: Basic " : "" ),
				  ( user ? dynamic->user_pw_base64 : "" ),
				  ( user ? "\r\n" : "" ) ) ) != 0 ) {
		goto err_xfer;
	}

 err_xfer:
	free ( dynamic );
 err_alloc:
	if ( rc != 0 )
		http_close ( http, rc );
}

/**
 * Check HTTP data transfer flow control window
 *
 * @v http		HTTP request
 * @ret len		Length of window
 */
static size_t http_xfer_window ( struct http_request *http ) {

	/* New block commands may be issued only when we are idle */
	return ( ( http->rx_state == HTTP_RX_IDLE ) ? 1 : 0 );
}

/**
 * Initiate HTTP partial read
 *
 * @v http		HTTP request
 * @v partial		Partial transfer interface
 * @v offset		Starting offset
 * @v buffer		Data buffer
 * @v len		Length
 * @ret rc		Return status code
 */
static int http_partial_read ( struct http_request *http,
			       struct interface *partial,
			       size_t offset, userptr_t buffer, size_t len ) {

	/* Sanity check */
	if ( http_xfer_window ( http ) == 0 )
		return -EBUSY;

	/* Initialise partial transfer parameters */
	http->rx_buffer = buffer;
	http->partial_start = offset;
	http->partial_len = len;
	http->remaining = len;

	/* Schedule request */
	http->rx_state = HTTP_RX_RESPONSE;
	http->flags = ( HTTP_TX_PENDING | HTTP_KEEPALIVE );
	if ( ! len )
		http->flags |= HTTP_HEAD_ONLY;
	process_add ( &http->process );

	/* Attach to parent interface and return */
	intf_plug_plug ( &http->partial, partial );

	return 0;
}

/**
 * Issue HTTP block device read
 *
 * @v http		HTTP request
 * @v block		Block data interface
 * @v lba		Starting logical block address
 * @v count		Number of blocks to transfer
 * @v buffer		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int http_block_read ( struct http_request *http,
			     struct interface *block,
			     uint64_t lba, unsigned int count,
			     userptr_t buffer, size_t len __unused ) {

	return http_partial_read ( http, block, ( lba * HTTP_BLKSIZE ),
				   buffer, ( count * HTTP_BLKSIZE ) );
}

/**
 * Read HTTP block device capacity
 *
 * @v http		HTTP request
 * @v block		Block data interface
 * @ret rc		Return status code
 */
static int http_block_read_capacity ( struct http_request *http,
				      struct interface *block ) {

	return http_partial_read ( http, block, 0, 0, 0 );
}

/**
 * Describe HTTP device in an ACPI table
 *
 * @v http		HTTP request
 * @v acpi		ACPI table
 * @v len		Length of ACPI table
 * @ret rc		Return status code
 */
static int http_acpi_describe ( struct http_request *http,
				struct acpi_description_header *acpi,
				size_t len ) {

	DBGC ( http, "HTTP %p cannot yet describe device in an ACPI table\n",
	       http );
	( void ) acpi;
	( void ) len;
	return 0;
}

/** HTTP socket interface operations */
static struct interface_operation http_socket_operations[] = {
	INTF_OP ( xfer_window, struct http_request *, http_socket_window ),
	INTF_OP ( xfer_deliver, struct http_request *, http_socket_deliver ),
	INTF_OP ( xfer_window_changed, struct http_request *, http_step ),
	INTF_OP ( intf_close, struct http_request *, http_close ),
};

/** HTTP socket interface descriptor */
static struct interface_descriptor http_socket_desc =
	INTF_DESC_PASSTHRU ( struct http_request, socket,
			     http_socket_operations, xfer );

/** HTTP partial transfer interface operations */
static struct interface_operation http_partial_operations[] = {
	INTF_OP ( intf_close, struct http_request *, http_close ),
};

/** HTTP partial transfer interface descriptor */
static struct interface_descriptor http_partial_desc =
	INTF_DESC ( struct http_request, partial, http_partial_operations );

/** HTTP data transfer interface operations */
static struct interface_operation http_xfer_operations[] = {
	INTF_OP ( xfer_window, struct http_request *, http_xfer_window ),
	INTF_OP ( block_read, struct http_request *, http_block_read ),
	INTF_OP ( block_read_capacity, struct http_request *,
		  http_block_read_capacity ),
	INTF_OP ( intf_close, struct http_request *, http_close ),
	INTF_OP ( acpi_describe, struct http_request *, http_acpi_describe ),
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
					  const char *name,
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
	intf_init ( &http->partial, &http_partial_desc, &http->refcnt );
	http->uri = uri_get ( uri );
	intf_init ( &http->socket, &http_socket_desc, &http->refcnt );
	process_init ( &http->process, &http_process_desc, &http->refcnt );
	http->flags = HTTP_TX_PENDING;

	/* Open socket */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( http->uri, default_port ) );
	socket = &http->socket;
	if ( filter ) {
		if ( ( rc = filter ( socket, uri->host, &socket ) ) != 0 )
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
	http_close ( http, rc );
	ref_put ( &http->refcnt );
	return rc;
}
