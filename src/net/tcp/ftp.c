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

const char *ftp_strings[] = {
	[FTP_CONNECT] = "",
	[FTP_USER] = "USER anonymous\r\n",
	[FTP_PASS] = "PASS etherboot@etherboot.org\r\n",
	[FTP_TYPE] = "TYPE I\r\n",
	[FTP_PASV] = "PASV\r\n",
	[FTP_RETR] = "RETR %s\r\n",
	[FTP_QUIT] = "QUIT\r\n",
	[FTP_DONE] = "",
};

static inline struct ftp_request *
tcp_to_ftp ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp );
}

static inline struct ftp_request *
tcp_to_ftp_data ( struct tcp_connection *conn ) {
	return container_of ( conn, struct ftp_request, tcp_data );
}

static void ftp_complete ( struct ftp_request *ftp, int complete ) {
	ftp->complete = complete;
	tcp_close ( &ftp->tcp_data );
	tcp_close ( &ftp->tcp );
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

static void ftp_connected ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );

	/* Nothing to do */
}

static void ftp_acked ( struct tcp_connection *conn, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	
	ftp->already_sent += len;
}

static int ftp_open_passive ( struct ftp_request *ftp ) {
	char *ptr = ftp->passive_text;
	uint8_t *byte = ( uint8_t * ) ( &ftp->tcp_data.sin );
	int i;

	/* Parse the IP address and port from the PASV repsonse */
	for ( i = 6 ; i ; i-- ) {
		if ( ! *ptr )
			return -EINVAL;
		*(byte++) = strtoul ( ptr, &ptr, 10 );
		if ( *ptr )
			ptr++;
	}
	if ( *ptr )
		return -EINVAL;

	tcp_connect ( &ftp->tcp_data );
	return 0;
}

static void ftp_reply ( struct ftp_request *ftp ) {
	char status_major = ftp->status_text[0];
	int success;

	/* Ignore "intermediate" messages */
	if ( status_major == '1' )
		return;

	/* Check for success */
	success = ( status_major == '2' );

	/* Special-case the "USER"-"PASS" sequence */
	if ( ftp->state == FTP_USER ) {
		if ( success ) {
			/* No password was asked for; pretend we have
			 * already entered it
			 */
			ftp->state = FTP_PASS;
		} else if ( status_major == '3' ) {
			/* Password requested, treat this as success
			 * for our purposes
			 */
			success = 1;
		}
	}
	
	/* Abort on failure */
	if ( ! success )
		goto err;

	/* Open passive connection when we get "PASV" response */
	if ( ftp->state == FTP_PASV ) {
		if ( ftp_open_passive ( ftp ) != 0 )
			goto err;
	}

	/* Move to next state */
	if ( ftp->state < FTP_DONE )
		ftp->state++;
	ftp->already_sent = 0;
	return;

 err:
	ftp->complete = -EPROTO;
	tcp_close ( &ftp->tcp );
}

static void ftp_newdata ( struct tcp_connection *conn,
			  void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	char c;

	for ( ; len ; data++, len-- ) {
		c = * ( ( char * ) data );
		if ( ( c == '\r' ) || ( c == '\n' ) ) {
			if ( ftp->recvsize == 0 )
				ftp_reply ( ftp );
			ftp->recvbuf = ftp->status_text;
			ftp->recvsize = sizeof ( ftp->status_text ) - 1;
		} else if ( c == '(' ) {
			ftp->recvbuf = ftp->passive_text;
			ftp->recvsize = sizeof ( ftp->passive_text ) - 1;
		} else if ( c == ')' ) {
			ftp->recvsize = 0;
		} else if ( ftp->recvsize > 0 ) {
			*(ftp->recvbuf++) = c;
			ftp->recvsize--;
		}
	}
}

static void ftp_senddata ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp ( conn );
	const char *format;
	const char *data;
	size_t len;

	/* Select message format string and data */
	format = ftp_strings[ftp->state];
	switch ( ftp->state ) {
	case FTP_RETR:
		data = ftp->filename;
		break;
	default:
		data = NULL;
		break;
	}
	if ( ! data )
		data = "";
	
	/* Build message */
	len = snprintf ( tcp_buffer, tcp_buflen, format, data );
	tcp_send ( conn, tcp_buffer + ftp->already_sent,
		   len - ftp->already_sent );
}

static struct tcp_operations ftp_tcp_operations = {
	.aborted	= ftp_aborted,
	.timedout	= ftp_timedout,
	.closed		= ftp_closed,
	.connected	= ftp_connected,
	.acked		= ftp_acked,
	.newdata	= ftp_newdata,
	.senddata	= ftp_senddata,
};

static void ftp_data_aborted ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp_complete ( ftp, -ECONNABORTED );
}

static void ftp_data_timedout ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp_complete ( ftp, -ETIMEDOUT );
}

static void ftp_data_closed ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	/* Nothing to do */
}

static void ftp_data_connected ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	/* Nothing to do */
}

static void ftp_data_acked ( struct tcp_connection *conn, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );
	
	/* Nothing to do */
}

static void ftp_data_newdata ( struct tcp_connection *conn,
			       void *data, size_t len ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );

	ftp->callback ( data, len );
}

static void ftp_data_senddata ( struct tcp_connection *conn ) {
	struct ftp_request *ftp = tcp_to_ftp_data ( conn );
	
	/* Nothing to do */
}

static struct tcp_operations ftp_data_tcp_operations = {
	.aborted	= ftp_data_aborted,
	.timedout	= ftp_data_timedout,
	.closed		= ftp_data_closed,
	.connected	= ftp_data_connected,
	.acked		= ftp_data_acked,
	.newdata	= ftp_data_newdata,
	.senddata	= ftp_data_senddata,
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
