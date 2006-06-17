#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <vsprintf.h>
#include <assert.h>
#include <errno.h>
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

/** An FTP control channel string */
struct ftp_string {
	/** String format */
	const char *format;
	/** Offset to string data
	 *
	 * This is the offset within the struct ftp_request to the
	 * pointer to the string data.  Use ftp_string_data() to get a
	 * pointer to the actual data.
	 */
	off_t data_offset;
};

/** FTP control channel strings */
static const struct ftp_string ftp_strings[] = {
	[FTP_CONNECT]	= { "", 0 },
	[FTP_USER]	= { "USER anonymous\r\n", 0 },
	[FTP_PASS]	= { "PASS etherboot@etherboot.org\r\n", 0 },
	[FTP_TYPE]	= { "TYPE I\r\n", 0 },
	[FTP_PASV]	= { "PASV\r\n", 0 },
	[FTP_RETR]	= { "RETR %s\r\n", 
			    offsetof ( struct ftp_request, filename ) },
	[FTP_QUIT]	= { "QUIT\r\n", 0 },
	[FTP_DONE]	= { "", 0 },
};

/**
 * Get data associated with an FTP control channel string
 *
 * @v ftp		FTP request
 * @v data_offset	Data offset field from ftp_string structure
 * @ret data		Pointer to data
 */
static inline const void * ftp_string_data ( struct ftp_request *ftp,
					     off_t data_offset ) {
	return * ( ( void ** ) ( ( ( void * ) ftp ) + data_offset ) );
}

/**
 * Get FTP request from control TCP connection
 *
 * @v conn		TCP connection
 * @ret ftp		FTP request
 */
static inline struct ftp_request * tcp_to_ftp ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp );
}

/**
 * Mark FTP request as complete
 *
 * @v ftp		FTP request
 * @v complete		Completion indicator
 *
 */
static void ftp_complete ( struct ftp_request *ftp, int complete ) {
	ftp->complete = complete;
	tcp_close ( &ftp->tcp_data );
	tcp_close ( &ftp->tcp );
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
 * This is called once we have received a complete repsonse line.
 */
static void ftp_reply ( struct ftp_request *ftp ) {
	char status_major = ftp->status_text[0];

	/* Ignore "intermediate" responses (1xx codes) */
	if ( status_major == '1' )
		return;

	/* Anything other than success (2xx) or, in the case of a
	 * repsonse to a "USER" command, a password prompt (3xx), is a
	 * fatal error.
	 */
	if ( ! ( ( status_major == '2' ) ||
		 ( ( status_major == '3' ) && ( ftp->state == FTP_USER ) ) ) )
		goto err;

	/* Open passive connection when we get "PASV" response */
	if ( ftp->state == FTP_PASV ) {
		char *ptr = ftp->passive_text;

		ftp_parse_value ( &ptr,
				  ( uint8_t * ) &ftp->tcp_data.sin.sin_addr,
				  sizeof ( ftp->tcp_data.sin.sin_addr ) );
		ftp_parse_value ( &ptr,
				  ( uint8_t * ) &ftp->tcp_data.sin.sin_port,
				  sizeof ( ftp->tcp_data.sin.sin_port ) );
		tcp_connect ( &ftp->tcp_data );
	}

	/* Move to next state */
	if ( ftp->state < FTP_DONE )
		ftp->state++;
	ftp->already_sent = 0;
	return;

 err:
	/* Flag protocol error and close connections */
	ftp_complete ( ftp, -EPROTO );
}

/**
 * Handle new data arriving on FTP control channel
 *
 * @v conn	TCP connection
 * @v data	New data
 * @v len	Length of new data
 *
 * Data is collected until a complete line is received, at which point
 * its information is passed to ftp_reply().
 */
static void ftp_newdata ( struct tcp_connection *conn,
			  void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
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
 * @v conn	TCP connection
 */
static void ftp_acked ( struct tcp_connection *conn, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	
	/* Mark off ACKed portion of the currently-transmitted data */
	ftp->already_sent += len;
}

/**
 * Construct data to send on FTP control channel
 *
 * @v conn	TCP connection
 * @v buf	Temporary data buffer
 * @v len	Length of temporary data buffer
 */
static void ftp_senddata ( struct tcp_connection *conn,
			   void *buf, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	const struct ftp_string *string;

	/* Send the as-yet-unACKed portion of the string for the
	 * current state.
	 */
	string = &ftp_strings[ftp->state];
	len = snprintf ( buf, len, string->format,
			 ftp_string_data ( ftp, string->data_offset ) );
	tcp_send ( conn, buf + ftp->already_sent, len - ftp->already_sent );
}

/**
 * Handle control channel being closed
 *
 * @v conn		TCP connection
 *
 * When the control channel is closed, the data channel must also be
 * closed, if it is currently open.
 */
static void ftp_closed ( struct tcp_connection *conn, int status ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );

	ftp_complete ( ftp, status ? status : 1 );
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
 * Get FTP request from data TCP connection
 *
 * @v conn		TCP connection
 * @ret ftp		FTP request
 */
static inline struct ftp_request *
tcp_to_ftp_data ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp_data );
}

/**
 * Handle data channel being closed
 *
 * @v conn		TCP connection
 *
 * When the data channel is closed, the control channel should be left
 * alone; the server will send a completion message via the control
 * channel which we'll pick up.
 *
 * If the data channel is closed due to an error, we abort the request.
 */
static void ftp_data_closed ( struct tcp_connection *conn, int status ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	if ( status )
		ftp_complete ( ftp, status );
}

/**
 * Handle new data arriving on the FTP data channel
 *
 * @v conn	TCP connection
 * @v data	New data
 * @v len	Length of new data
 *
 * Data is handed off to the callback registered in the FTP request.
 */
static void ftp_data_newdata ( struct tcp_connection *conn,
			       void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp->callback ( data, len );
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
 * Initiate an FTP connection
 *
 * @v ftp	FTP request
 */
void ftp_connect ( struct ftp_request *ftp ) {
	ftp->tcp.tcp_op = &ftp_tcp_operations;
	ftp->tcp_data.tcp_op = &ftp_data_tcp_operations;
	ftp->recvbuf = ftp->status_text;
	ftp->recvsize = sizeof ( ftp->status_text ) - 1;
	tcp_connect ( &ftp->tcp );
}
