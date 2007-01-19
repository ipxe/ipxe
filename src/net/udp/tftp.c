/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/async.h>
#include <gpxe/tftp.h>
#include <gpxe/uri.h>

/** @file
 *
 * TFTP protocol
 *
 */

/** A TFTP option */
struct tftp_option {
	/** Option name */
	const char *name;
	/** Option processor
	 *
	 * @v tftp	TFTP connection
	 * @v value	Option value
	 * @ret rc	Return status code
	 */
	int ( * process ) ( struct tftp_session *tftp, const char *value );
};

/**
 * Process TFTP "blksize" option
 *
 * @v tftp		TFTP connection
 * @v value		Option value
 * @ret rc		Return status code
 */
static int tftp_process_blksize ( struct tftp_session *tftp,
				  const char *value ) {
	char *end;

	tftp->blksize = strtoul ( value, &end, 10 );
	if ( *end ) {
		DBGC ( tftp, "TFTP %p got invalid blksize \"%s\"\n",
		       tftp, value );
		return -EINVAL;
	}
	DBGC ( tftp, "TFTP %p blksize=%d\n", tftp, tftp->blksize );

	return 0;
}

/**
 * Process TFTP "tsize" option
 *
 * @v tftp		TFTP connection
 * @v value		Option value
 * @ret rc		Return status code
 */
static int tftp_process_tsize ( struct tftp_session *tftp,
				const char *value ) {
	char *end;

	tftp->tsize = strtoul ( value, &end, 10 );
	if ( *end ) {
		DBGC ( tftp, "TFTP %p got invalid tsize \"%s\"\n",
		       tftp, value );
		return -EINVAL;
	}
	DBGC ( tftp, "TFTP %p tsize=%ld\n", tftp, tftp->tsize );

	return 0;
}

/** Recognised TFTP options */
static struct tftp_option tftp_options[] = {
	{ "blksize", tftp_process_blksize },
	{ "tsize", tftp_process_tsize },
	{ NULL, NULL }
};

/**
 * Process TFTP option
 *
 * @v tftp		TFTP connection
 * @v name		Option name
 * @v value		Option value
 * @ret rc		Return status code
 */
static int tftp_process_option ( struct tftp_session *tftp,
				 const char *name, const char *value ) {
	struct tftp_option *option;

	for ( option = tftp_options ; option->name ; option++ ) {
		if ( strcasecmp ( name, option->name ) == 0 )
			return option->process ( tftp, value );
	}

	DBGC ( tftp, "TFTP %p received unknown option \"%s\" = \"%s\"\n",
	       tftp, name, value );

	return -EINVAL;
}

/** Translation between TFTP errors and internal error numbers */
static const uint8_t tftp_errors[] = {
	[TFTP_ERR_FILE_NOT_FOUND]	= PXENV_STATUS_TFTP_FILE_NOT_FOUND,
	[TFTP_ERR_ACCESS_DENIED]	= PXENV_STATUS_TFTP_ACCESS_VIOLATION,
	[TFTP_ERR_ILLEGAL_OP]		= PXENV_STATUS_TFTP_UNKNOWN_OPCODE,
};

/**
 * Mark TFTP session as complete
 *
 * @v tftp		TFTP connection
 * @v rc		Return status code
 */
static void tftp_done ( struct tftp_session *tftp, int rc ) {

	/* Stop the retry timer */
	stop_timer ( &tftp->timer );

	/* Close UDP connection */
	udp_close ( &tftp->udp );

	/* Mark async operation as complete */
	async_done ( &tftp->async, rc );
}

/**
 * Send next packet in TFTP session
 *
 * @v tftp		TFTP connection
 */
static void tftp_send_packet ( struct tftp_session *tftp ) {
	start_timer ( &tftp->timer );
	udp_senddata ( &tftp->udp );
}

/**
 * Handle TFTP retransmission timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void tftp_timer_expired ( struct retry_timer *timer, int fail ) {
	struct tftp_session *tftp =
		container_of ( timer, struct tftp_session, timer );

	if ( fail ) {
		tftp_done ( tftp, -ETIMEDOUT );
	} else {
		tftp_send_packet ( tftp );
	}
}

/**
 * Mark TFTP block as received
 *
 * @v tftp		TFTP connection
 * @v block		Block number
 */
static void tftp_received ( struct tftp_session *tftp, unsigned int block ) {

	/* Stop the retry timer */
	stop_timer ( &tftp->timer );

	/* Update state to indicate which block we're now waiting for */
	tftp->state = block;

	/* Send next packet */
	tftp_send_packet ( tftp );
}

/**
 * Transmit RRQ
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_send_rrq ( struct tftp_session *tftp, void *buf, size_t len ) {
	struct tftp_rrq *rrq = buf;
	void *data;
	void *end;

	DBGC ( tftp, "TFTP %p requesting \"%s\"\n", tftp, tftp->uri->path );

	data = rrq->data;
	end = ( buf + len );
	if ( data > end )
		goto overflow;
	data += ( snprintf ( data, ( end - data ),
			     "%s%coctet%cblksize%c%d%ctsize%c0",
			     tftp->uri->path, 0, 0, 0,
			     tftp->request_blksize, 0, 0 ) + 1 );
	if ( data > end )
		goto overflow;
	rrq->opcode = htons ( TFTP_RRQ );

	return udp_send ( &tftp->udp, buf, ( data - buf ) );

 overflow:
	DBGC ( tftp, "TFTP %p RRQ out of space\n", tftp );
	return -ENOBUFS;
}

/**
 * Receive OACK
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_rx_oack ( struct tftp_session *tftp, void *buf, size_t len ) {
	struct tftp_oack *oack = buf;
	char *end = buf + len;
	char *name;
	char *value;
	int rc;

	/* Sanity check */
	if ( len < sizeof ( *oack ) ) {
		DBGC ( tftp, "TFTP %p received underlength OACK packet "
		       "length %d\n", tftp, len );
		return -EINVAL;
	}
	if ( end[-1] != '\0' ) {
		DBGC ( tftp, "TFTP %p received OACK missing final NUL\n",
		       tftp );
		return -EINVAL;
	}

	/* Process each option in turn */
	name = oack->data;
	while ( name < end ) {
		value = ( name + strlen ( name ) + 1 );
		if ( value == end ) {
			DBGC ( tftp, "TFTP %p received OACK missing value "
			       "for option \"%s\"\n", tftp, name );
			return -EINVAL;
		}
		if ( ( rc = tftp_process_option ( tftp, name, value ) ) != 0 )
			return rc;
		name = ( value + strlen ( value ) + 1 );
	}

	/* Mark as received block 0 (the OACK) */
	tftp_received ( tftp, 0 );

	return 0;
}

/**
 * Receive DATA
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_rx_data ( struct tftp_session *tftp, void *buf, size_t len ) {
	struct tftp_data *data = buf;
	unsigned int block;
	size_t data_offset;
	size_t data_len;
	int rc;

	/* Sanity check */
	if ( len < sizeof ( *data ) ) {
		DBGC ( tftp, "TFTP %p received underlength DATA packet "
		       "length %d\n", tftp, len );
		return -EINVAL;
	}

	/* Fill data buffer */
	block = ntohs ( data->block );
	data_offset = ( ( block - 1 ) * tftp->blksize );
	data_len = ( len - offsetof ( typeof ( *data ), data ) );
	if ( ( rc = fill_buffer ( tftp->buffer, data->data, data_offset,
				  data_len ) ) != 0 ) {
		DBGC ( tftp, "TFTP %p could not fill data buffer: %s\n",
		       tftp, strerror ( rc ) );
		tftp_done ( tftp, rc );
		return rc;
	}

	/* Mark block as received */
	tftp_received ( tftp, block );

	/* Finish when final block received */
	if ( data_len < tftp->blksize )
		tftp_done ( tftp, 0 );

	return 0;
}

/**
 * Transmit ACK
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_send_ack ( struct tftp_session *tftp ) {
	struct tftp_ack ack;

	ack.opcode = htons ( TFTP_ACK );
	ack.block = htons ( tftp->state );
	return udp_send ( &tftp->udp, &ack, sizeof ( ack ) );
}

/**
 * Receive ERROR
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_rx_error ( struct tftp_session *tftp, void *buf, size_t len ) {
	struct tftp_error *error = buf;
	unsigned int err;
	int rc = 0;

	/* Sanity check */
	if ( len < sizeof ( *error ) ) {
		DBGC ( tftp, "TFTP %p received underlength ERROR packet "
		       "length %d\n", tftp, len );
		return -EINVAL;
	}

	DBGC ( tftp, "TFTP %p received ERROR packet with code %d, message "
	       "\"%s\"\n", tftp, ntohs ( error->errcode ), error->errmsg );
	
	/* Determine final operation result */
	err = ntohs ( error->errcode );
	if ( err < ( sizeof ( tftp_errors ) / sizeof ( tftp_errors[0] ) ) )
		rc = -tftp_errors[err];
	if ( ! rc )
		rc = -PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION;

	/* Close TFTP session */
	tftp_done ( tftp, rc );

	return 0;
}

/**
 * Transmit data
 *
 * @v conn		UDP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_senddata ( struct udp_connection *conn,
			   void *buf, size_t len ) {
	struct tftp_session *tftp = 
		container_of ( conn, struct tftp_session, udp );

	if ( tftp->state < 0 ) {
		return tftp_send_rrq ( tftp, buf, len );
	} else {
		return tftp_send_ack ( tftp );
	}
}

/**
 * Receive new data
 *
 * @v udp		UDP connection
 * @v data		Received data
 * @v len		Length of received data
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 */
static int tftp_newdata ( struct udp_connection *conn, void *data, size_t len,
			  struct sockaddr_tcpip *st_src __unused,
			  struct sockaddr_tcpip *st_dest __unused ) {
	struct tftp_session *tftp = 
		container_of ( conn, struct tftp_session, udp );
	struct tftp_common *common = data;
	
	if ( len < sizeof ( *common ) ) {
		DBGC ( tftp, "TFTP %p received underlength packet length %d\n",
		       tftp, len );
		return -EINVAL;
	}

	/* Filter by TID.  Set TID on first response received */
	if ( tftp->tid ) {
		if ( tftp->tid != st_src->st_port ) {
			DBGC ( tftp, "TFTP %p received packet from wrong port "
			       "(got %d, wanted %d)\n", tftp,
			       ntohs ( st_src->st_port ), ntohs ( tftp->tid ));
			return -EINVAL;
		}
	} else {
		tftp->tid = st_src->st_port;
		DBGC ( tftp, "TFTP %p using remote port %d\n", tftp,
		       ntohs ( tftp->tid ) );
		udp_connect_port ( &tftp->udp, tftp->tid );
	}

	/* Filter by source address */
	if ( memcmp ( st_src, udp_peer ( &tftp->udp ),
		      sizeof ( *st_src ) ) != 0 ) {
		DBGC ( tftp, "TFTP %p received packet from foreign source\n",
		       tftp );
		return -EINVAL;
	}

	switch ( common->opcode ) {
	case htons ( TFTP_OACK ):
		return tftp_rx_oack ( tftp, data, len );
	case htons ( TFTP_DATA ):
		return tftp_rx_data ( tftp, data, len );
	case htons ( TFTP_ERROR ):
		return tftp_rx_error ( tftp, data, len );
	default:
		DBGC ( tftp, "TFTP %p received strange packet type %d\n", tftp,
		       ntohs ( common->opcode ) );
		return -EINVAL;
	};
}

/** TFTP UDP operations */
static struct udp_operations tftp_udp_operations = {
	.senddata = tftp_senddata,
	.newdata = tftp_newdata,
};

/**
 * Reap asynchronous operation
 *
 * @v async		Asynchronous operation
 */
static void tftp_reap ( struct async *async ) {
	struct tftp_session *tftp =
		container_of ( async, struct tftp_session, async );

	free ( tftp );
}

/** TFTP asynchronous operations */
static struct async_operations tftp_async_operations = {
	.reap = tftp_reap,
};

/**
 * Initiate TFTP download
 *
 * @v uri		Uniform Resource Identifier
 * @v buffer		Buffer into which to download file
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 */
int tftp_get ( struct uri *uri, struct buffer *buffer, struct async *parent ) {
	struct tftp_session *tftp = NULL;
	int rc;

	/* Sanity checks */
	if ( ! uri->path ) {
		rc = -EINVAL;
		goto err;
	}

	/* Allocate and populate TFTP structure */
	tftp = malloc ( sizeof ( *tftp ) );
	if ( ! tftp ) {
		rc = -ENOMEM;
		goto err;
	}
	memset ( tftp, 0, sizeof ( *tftp ) );
	tftp->uri = uri;
	tftp->buffer = buffer;
	if ( ! tftp->request_blksize )
		tftp->request_blksize = TFTP_MAX_BLKSIZE;
	tftp->blksize = TFTP_DEFAULT_BLKSIZE;
	tftp->state = -1;
	tftp->udp.udp_op = &tftp_udp_operations;
	tftp->timer.expired = tftp_timer_expired;


#warning "Quick name resolution hack"
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;
	server.sin.sin_port = htons ( TFTP_PORT );
	server.sin.sin_family = AF_INET;
	if ( inet_aton ( uri->host, &server.sin.sin_addr ) == 0 ) {
		rc = -EINVAL;
		goto err;
	}
	udp_connect ( &tftp->udp, &server.st );


	/* Open UDP connection */
	if ( ( rc = udp_open ( &tftp->udp, 0 ) ) != 0 )
		goto err;

	/* Transmit initial RRQ */
	tftp_send_packet ( tftp );

	async_init ( &tftp->async, &tftp_async_operations, parent );
	return 0;

 err:
	DBGC ( tftp, "TFTP %p could not create session: %s\n",
	       tftp, strerror ( rc ) );
	free ( tftp );
	return rc;
}
