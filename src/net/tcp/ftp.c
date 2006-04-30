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

#define ftp_string_offset( fieldname ) \
	offsetof ( struct ftp_request, fieldname )

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

/** FTP control channel strings */
const struct ftp_string ftp_strings[] = {
	[FTP_CONNECT]	= { "", 0 },
	[FTP_USER]	= { "USER anonymous\r\n", 0 },
	[FTP_PASS]	= { "PASS etherboot@etherboot.org\r\n", 0 },
	[FTP_TYPE]	= { "TYPE I\r\n", 0 },
	[FTP_PASV]	= { "PASV\r\n", 0 },
	[FTP_RETR]	= { "RETR %s\r\n", ftp_string_offset ( filename ) },
	[FTP_QUIT]	= { "QUIT\r\n", 0 },
	[FTP_DONE]	= { "", 0 },
};

static inline struct ftp_request *
tcp_to_ftp ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp );
}

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
 * Handle a response from an FTP server
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

static void ftp_acked ( struct tcp_connection *conn, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	
	/* Mark off ACKed portion of the currently-transmitted data */
	ftp->already_sent += len;
}

static void ftp_senddata ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	const struct ftp_string *string;
	size_t len;

	/* Send the as-yet-unACKed portion of the string for the
	 * current state.
	 */
	string = &ftp_strings[ftp->state];
	len = snprintf ( tcp_buffer, tcp_buflen, string->format,
			 ftp_string_data ( ftp, string->data_offset ) );
	tcp_send ( conn, tcp_buffer + ftp->already_sent,
		   len - ftp->already_sent );
}

static void ftp_aborted ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );

	ftp_complete ( ftp, -ECONNABORTED );
}

static void ftp_timedout ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );

	ftp_complete ( ftp, -ETIMEDOUT );
}

static void ftp_closed ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );

	ftp_complete ( ftp, 1 );
}

static struct tcp_operations ftp_tcp_operations = {
	.aborted	= ftp_aborted,
	.timedout	= ftp_timedout,
	.closed		= ftp_closed,
	.acked		= ftp_acked,
	.newdata	= ftp_newdata,
	.senddata	= ftp_senddata,
};

static inline struct ftp_request *
tcp_to_ftp_data ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp_data );
}

static void ftp_data_aborted ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp_complete ( ftp, -ECONNABORTED );
}

static void ftp_data_timedout ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp_complete ( ftp, -ETIMEDOUT );
}

static void ftp_data_newdata ( struct tcp_connection *conn,
			       void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp->callback ( data, len );
}

static struct tcp_operations ftp_data_tcp_operations = {
	.aborted	= ftp_data_aborted,
	.timedout	= ftp_data_timedout,
	.newdata	= ftp_data_newdata,
};

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
