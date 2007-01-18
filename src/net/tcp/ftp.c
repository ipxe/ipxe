#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <vsprintf.h>
#include <assert.h>
#include <errno.h>
#include <gpxe/async.h>
#include <gpxe/buffer.h>
#include <gpxe/uri.h>
#include <gpxe/download.h>
#include <gpxe/ftp.h>

/** @file
 *
 * File transfer protocol
 *
 */

/*****************************************************************************
 *
 * FTP control channel
 *
 */

/** FTP control channel strings
 *
 * These are used as printf() format strings.  Since only one of them
 * (RETR) takes an argument, we always supply that argument to the
 * snprintf() call.
 */
static const char * ftp_strings[] = {
	[FTP_CONNECT]	= "",
	[FTP_USER]	= "USER anonymous\r\n",
	[FTP_PASS]	= "PASS etherboot@etherboot.org\r\n",
	[FTP_TYPE]	= "TYPE I\r\n",
	[FTP_PASV]	= "PASV\r\n",
	[FTP_RETR]	= "RETR %s\r\n", 
	[FTP_QUIT]	= "QUIT\r\n",
	[FTP_DONE]	= "",
};

/**
 * Get FTP request from control TCP application
 *
 * @v app		TCP application
 * @ret ftp		FTP request
 */
static inline struct ftp_request * tcp_to_ftp ( struct tcp_application *app ) {
	return container_of ( app, struct ftp_request, tcp );
}

/**
 * Mark FTP operation as complete
 *
 * @v ftp		FTP request
 * @v rc		Return status code
 */
static void ftp_done ( struct ftp_request *ftp, int rc ) {

	DBGC ( ftp, "FTP %p completed with status %d\n", ftp, rc );

	/* Close both TCP connections */
	tcp_close ( &ftp->tcp );
	tcp_close ( &ftp->tcp_data );

	/* Mark asynchronous operation as complete */
	async_done ( &ftp->async, rc );
}

/**
 * Parse FTP byte sequence value
 *
 * @v text	Text string
 * @v value	Value buffer
 * @v len	Length of value buffer
 *
 * This parses an FTP byte sequence value (e.g. the "aaa,bbb,ccc,ddd"
 * form for IP addresses in PORT commands) into a byte sequence.  @c
 * *text will be updated to point beyond the end of the parsed byte
 * sequence.
 *
 * This function is safe in the presence of malformed data, though the
 * output is undefined.
 */
static void ftp_parse_value ( char **text, uint8_t *value, size_t len ) {
	do {
		*(value++) = strtoul ( *text, text, 10 );
		if ( **text )
			(*text)++;
	} while ( --len );
}

/**
 * Handle an FTP control channel response
 *
 * @v ftp	FTP request
 *
 * This is called once we have received a complete response line.
 */
static void ftp_reply ( struct ftp_request *ftp ) {
	char status_major = ftp->status_text[0];

	DBGC ( ftp, "FTP %p received status %s\n", ftp, ftp->status_text );

	/* Ignore "intermediate" responses (1xx codes) */
	if ( status_major == '1' )
		return;

	/* Anything other than success (2xx) or, in the case of a
	 * repsonse to a "USER" command, a password prompt (3xx), is a
	 * fatal error.
	 */
	if ( ! ( ( status_major == '2' ) ||
		 ( ( status_major == '3' ) && ( ftp->state == FTP_USER ) ) ) ){
		/* Flag protocol error and close connections */
		ftp_done ( ftp, -EPROTO );
	}

	/* Open passive connection when we get "PASV" response */
	if ( ftp->state == FTP_PASV ) {
		char *ptr = ftp->passive_text;
		union {
			struct sockaddr_in sin;
			struct sockaddr_tcpip st;
		} sa;
		int rc;

		sa.sin.sin_family = AF_INET;
		ftp_parse_value ( &ptr, ( uint8_t * ) &sa.sin.sin_addr,
				  sizeof ( sa.sin.sin_addr ) );
		ftp_parse_value ( &ptr, ( uint8_t * ) &sa.sin.sin_port,
				  sizeof ( sa.sin.sin_port ) );
		if ( ( rc = tcp_connect ( &ftp->tcp_data, &sa.st, 0 ) ) != 0 ){
			DBGC ( ftp, "FTP %p could not make data connection\n",
			       ftp );
			ftp_done ( ftp, rc );
			return;
		}
	}

	/* Move to next state */
	if ( ftp->state < FTP_DONE )
		ftp->state++;
	ftp->already_sent = 0;

	if ( ftp->state < FTP_DONE ) {
		DBGC ( ftp, "FTP %p sending ", ftp );
		DBGC ( ftp, ftp_strings[ftp->state], ftp->uri->path );
	}

	return;
}

/**
 * Handle new data arriving on FTP control channel
 *
 * @v app	TCP application
 * @v data	New data
 * @v len	Length of new data
 *
 * Data is collected until a complete line is received, at which point
 * its information is passed to ftp_reply().
 */
static void ftp_newdata ( struct tcp_application *app,
			  void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( app );
	char *recvbuf = ftp->recvbuf;
	size_t recvsize = ftp->recvsize;
	char c;
	
	while ( len-- ) {
		c = * ( ( char * ) data++ );
		switch ( c ) {
		case '\r' :
		case '\n' :
			/* End of line: call ftp_reply() to handle
			 * completed reply.  Avoid calling ftp_reply()
			 * twice if we receive both \r and \n.
			 */
			if ( recvsize == 0 )
				ftp_reply ( ftp );
			/* Start filling up the status code buffer */
			recvbuf = ftp->status_text;
			recvsize = sizeof ( ftp->status_text ) - 1;
			break;
		case '(' :
			/* Start filling up the passive parameter buffer */
			recvbuf = ftp->passive_text;
			recvsize = sizeof ( ftp->passive_text ) - 1;
			break;
		case ')' :
			/* Stop filling the passive parameter buffer */
			recvsize = 0;
			break;
		default :
			/* Fill up buffer if applicable */
			if ( recvsize > 0 ) {
				*(recvbuf++) = c;
				recvsize--;
			}
			break;
		}
	}

	/* Store for next invocation */
	ftp->recvbuf = recvbuf;
	ftp->recvsize = recvsize;
}

/**
 * Handle acknowledgement of data sent on FTP control channel
 *
 * @v app	TCP application
 */
static void ftp_acked ( struct tcp_application *app, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( app );
	
	/* Mark off ACKed portion of the currently-transmitted data */
	ftp->already_sent += len;
}

/**
 * Construct data to send on FTP control channel
 *
 * @v app	TCP application
 * @v buf	Temporary data buffer
 * @v len	Length of temporary data buffer
 */
static void ftp_senddata ( struct tcp_application *app,
			   void *buf, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( app );

	/* Send the as-yet-unACKed portion of the string for the
	 * current state.
	 */
	len = snprintf ( buf, len, ftp_strings[ftp->state], ftp->uri->path );
	tcp_send ( app, buf + ftp->already_sent, len - ftp->already_sent );
}

/**
 * Handle control channel being closed
 *
 * @v app		TCP application
 *
 * When the control channel is closed, the data channel must also be
 * closed, if it is currently open.
 */
static void ftp_closed ( struct tcp_application *app, int rc ) {
	struct ftp_request *ftp = tcp_to_ftp ( app );

	DBGC ( ftp, "FTP %p control connection closed: %s\n",
	       ftp, strerror ( rc ) );

	/* Complete FTP operation */
	ftp_done ( ftp, rc );
}

/** FTP control channel operations */
static struct tcp_operations ftp_tcp_operations = {
	.closed		= ftp_closed,
	.acked		= ftp_acked,
	.newdata	= ftp_newdata,
	.senddata	= ftp_senddata,
};

/*****************************************************************************
 *
 * FTP data channel
 *
 */

/**
 * Get FTP request from data TCP application
 *
 * @v app		TCP application
 * @ret ftp		FTP request
 */
static inline struct ftp_request *
tcp_to_ftp_data ( struct tcp_application *app ) {
	return container_of ( app, struct ftp_request, tcp_data );
}

/**
 * Handle data channel being closed
 *
 * @v app		TCP application
 *
 * When the data channel is closed, the control channel should be left
 * alone; the server will send a completion message via the control
 * channel which we'll pick up.
 *
 * If the data channel is closed due to an error, we abort the request.
 */
static void ftp_data_closed ( struct tcp_application *app, int rc ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( app );

	DBGC ( ftp, "FTP %p data connection closed: %s\n",
	       ftp, strerror ( rc ) );
	
	/* If there was an error, close control channel and record status */
	if ( rc )
		ftp_done ( ftp, rc );
}

/**
 * Handle new data arriving on the FTP data channel
 *
 * @v app	TCP application
 * @v data	New data
 * @v len	Length of new data
 */
static void ftp_data_newdata ( struct tcp_application *app,
			       void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( app );
	int rc;

	/* Fill data buffer */
	if ( ( rc = fill_buffer ( ftp->buffer, data,
				  ftp->buffer->fill, len ) ) != 0 ){
		DBGC ( ftp, "FTP %p failed to fill data buffer: %s\n",
		       ftp, strerror ( rc ) );
		ftp_done ( ftp, rc );
		return;
	}
}

/** FTP data channel operations */
static struct tcp_operations ftp_data_tcp_operations = {
	.closed		= ftp_data_closed,
	.newdata	= ftp_data_newdata,
};

/*****************************************************************************
 *
 * API
 *
 */

/**
 * Reap asynchronous operation
 *
 * @v async		Asynchronous operation
 */
static void ftp_reap ( struct async *async ) {
	struct ftp_request *ftp =
		container_of ( async, struct ftp_request, async );

	free ( ftp );
}

/** FTP asynchronous operations */
static struct async_operations ftp_async_operations = {
	.reap = ftp_reap,
};

#warning "Quick name resolution hack"
#include <byteswap.h>

/**
 * Initiate an FTP connection
 *
 * @v uri		Uniform Resource Identifier
 * @v buffer		Buffer into which to download file
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 */
int ftp_get ( struct uri *uri, struct buffer *buffer, struct async *parent ) {
	struct ftp_request *ftp = NULL;
	int rc;

	/* Sanity checks */
	if ( ! uri->path ) {
		rc = -EINVAL;
		goto err;
	}

	/* Allocate and populate FTP structure */
	ftp = malloc ( sizeof ( *ftp ) );
	if ( ! ftp ) {
		rc = -ENOMEM;
		goto err;
	}
	memset ( ftp, 0, sizeof ( *ftp ) );
	ftp->uri = uri;
	ftp->buffer = buffer;
	ftp->state = FTP_CONNECT;
	ftp->already_sent = 0;
	ftp->recvbuf = ftp->status_text;
	ftp->recvsize = sizeof ( ftp->status_text ) - 1;
	ftp->tcp.tcp_op = &ftp_tcp_operations;
	ftp->tcp_data.tcp_op = &ftp_data_tcp_operations;

#warning "Quick name resolution hack"
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;
	server.sin.sin_port = htons ( FTP_PORT );
	server.sin.sin_family = AF_INET;
	if ( inet_aton ( uri->host, &server.sin.sin_addr ) == 0 ) {
		rc = -EINVAL;
		goto err;
	}

	DBGC ( ftp, "FTP %p fetching %s\n", ftp, ftp->uri->path );

	if ( ( rc = tcp_connect ( &ftp->tcp, &server.st, 0 ) ) != 0 )
		goto err;

	async_init ( &ftp->async, &ftp_async_operations, parent );
	return 0;

 err:
	DBGC ( ftp, "FTP %p could not create request: %s\n", 
	       ftp, strerror ( rc ) );
	free ( ftp );
	return rc;
}

/** HTTP download protocol */
struct download_protocol ftp_download_protocol __download_protocol = {
	.name = "ftp",
	.start_download = ftp_get,
};
