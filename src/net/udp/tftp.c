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
#include <gpxe/refcnt.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/uri.h>
#include <gpxe/tcpip.h>
#include <gpxe/retry.h>
#include <gpxe/tftp.h>

/** @file
 *
 * TFTP protocol
 *
 */

/**
 * A TFTP request
 *
 * This data structure holds the state for an ongoing TFTP transfer.
 */
struct tftp_request {
	/** Reference count */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct xfer_interface xfer;

	/** URI being fetched */
	struct uri *uri;
	/** Transport layer interface */
	struct xfer_interface socket;

	/** Data block size
	 *
	 * This is the "blksize" option negotiated with the TFTP
	 * server.  (If the TFTP server does not support TFTP options,
	 * this will default to 512).
	 */
	unsigned int blksize;
	/** File size
	 *
	 * This is the value returned in the "tsize" option from the
	 * TFTP server.  If the TFTP server does not support the
	 * "tsize" option, this value will be zero.
	 */
	unsigned long tsize;

	/** Request state
	 *
	 * This is the block number to be used in the next ACK sent
	 * back to the server, i.e. the number of the last received
	 * data block.  The value zero indicates that the last
	 * received block was an OACK (i.e. that the next ACK will
	 * contain a block number of zero), and any value less than
	 * zero indicates that the connection has not yet been opened
	 * (i.e. that no blocks have yet been received).
	 */
	int state;
	/** Peer address
	 *
	 * The peer address is determined by the first response
	 * received to the TFTP RRQ.
	 */
	struct sockaddr_tcpip peer;
	/** Retransmission timer */
	struct retry_timer timer;
};

/**
 * Free TFTP request
 *
 * @v refcnt		Reference counter
 */
static void tftp_free ( struct refcnt *refcnt ) {
	struct tftp_request *tftp =
		container_of ( refcnt, struct tftp_request, refcnt );

	uri_put ( tftp->uri );
	free ( tftp );
}

/**
 * Mark TFTP request as complete
 *
 * @v tftp		TFTP connection
 * @v rc		Return status code
 */
static void tftp_done ( struct tftp_request *tftp, int rc ) {

	/* Stop the retry timer */
	stop_timer ( &tftp->timer );

	/* Close all data transfer interfaces */
	xfer_nullify ( &tftp->socket );
	xfer_close ( &tftp->socket, rc );
	xfer_nullify ( &tftp->xfer );
	xfer_close ( &tftp->xfer, rc );
}

/**
 * TFTP requested blocksize
 *
 * This is treated as a global configuration parameter.
 */
static unsigned int tftp_request_blksize = TFTP_MAX_BLKSIZE;

/**
 * Set TFTP request blocksize
 *
 * @v blksize		Requested block size
 */
void tftp_set_request_blksize ( unsigned int blksize ) {
	if ( blksize < TFTP_DEFAULT_BLKSIZE )
		blksize = TFTP_DEFAULT_BLKSIZE;
	tftp_request_blksize = blksize;
}

/**
 * Transmit RRQ
 *
 * @v tftp		TFTP connection
 * @ret rc		Return status code
 */
static int tftp_send_rrq ( struct tftp_request *tftp ) {
	struct tftp_rrq *rrq;
	const char *path = tftp->uri->path;
	size_t len = ( sizeof ( *rrq ) + strlen ( path ) + 1 /* NUL */
		       + 5 + 1 /* "octet" + NUL */
		       + 7 + 1 + 5 + 1 /* "blksize" + NUL + ddddd + NUL */
		       + 5 + 1 + 1 + 1 /* "tsize" + NUL + "0" + NUL */ );
	struct io_buffer *iobuf;

	DBGC ( tftp, "TFTP %p requesting \"%s\"\n", tftp, path );

	/* Allocate buffer */
	iobuf = xfer_alloc_iob ( &tftp->socket, len );
	if ( ! iobuf )
		return -ENOMEM;

	/* Build request */
	rrq = iob_put ( iobuf, sizeof ( *rrq ) );
	rrq->opcode = htons ( TFTP_RRQ );
	iob_put ( iobuf,
		  snprintf ( iobuf->data, iob_tailroom ( iobuf ),
			     "%s%coctet%cblksize%c%d%ctsize%c0", path, 0,
			     0, 0, tftp_request_blksize, 0, 0 ) + 1 );

	/* RRQ always goes to the address specified in the initial
	 * xfer_open() call
	 */
	return xfer_deliver_iob ( &tftp->socket, iobuf );
}

/**
 * Transmit ACK
 *
 * @v tftp		TFTP connection
 * @ret rc		Return status code
 */
static int tftp_send_ack ( struct tftp_request *tftp ) {
	struct tftp_ack *ack;
	struct io_buffer *iobuf;
	struct xfer_metadata meta = {
		.dest = ( struct sockaddr * ) &tftp->peer,
	};

	/* Allocate buffer */
	iobuf = xfer_alloc_iob ( &tftp->socket, sizeof ( *ack ) );
	if ( ! iobuf )
		return -ENOMEM;

	/* Build ACK */
	ack = iob_put ( iobuf, sizeof ( *ack ) );
	ack->opcode = htons ( TFTP_ACK );
	ack->block = htons ( tftp->state );

	/* ACK always goes to the peer recorded from the RRQ response */
	return xfer_deliver_iob_meta ( &tftp->socket, iobuf, &meta );
}

/**
 * Transmit data
 *
 * @v tftp		TFTP connection
 * @ret rc		Return status code
 */
static int tftp_send_packet ( struct tftp_request *tftp ) {

	/* Start retransmission timer */
	start_timer ( &tftp->timer );

	/* Send RRQ or ACK as appropriate */
	if ( tftp->state < 0 ) {
		return tftp_send_rrq ( tftp );
	} else {
		return tftp_send_ack ( tftp );
	}
}

/**
 * Handle TFTP retransmission timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void tftp_timer_expired ( struct retry_timer *timer, int fail ) {
	struct tftp_request *tftp =
		container_of ( timer, struct tftp_request, timer );

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
static void tftp_received ( struct tftp_request *tftp, unsigned int block ) {

	/* Stop the retry timer */
	stop_timer ( &tftp->timer );

	/* Update state to indicate which block we're now waiting for */
	tftp->state = block;

	/* Send next packet */
	tftp_send_packet ( tftp );
}

/**
 * Process TFTP "blksize" option
 *
 * @v tftp		TFTP connection
 * @v value		Option value
 * @ret rc		Return status code
 */
static int tftp_process_blksize ( struct tftp_request *tftp,
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
static int tftp_process_tsize ( struct tftp_request *tftp,
				const char *value ) {
	char *end;

	tftp->tsize = strtoul ( value, &end, 10 );
	if ( *end ) {
		DBGC ( tftp, "TFTP %p got invalid tsize \"%s\"\n",
		       tftp, value );
		return -EINVAL;
	}
	DBGC ( tftp, "TFTP %p tsize=%ld\n", tftp, tftp->tsize );

	/* Notify recipient of file size */
	xfer_seek ( &tftp->xfer, tftp->tsize, SEEK_SET );
	xfer_seek ( &tftp->xfer, 0, SEEK_SET );

	return 0;
}

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
	int ( * process ) ( struct tftp_request *tftp, const char *value );
};

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
static int tftp_process_option ( struct tftp_request *tftp,
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

/**
 * Receive OACK
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_rx_oack ( struct tftp_request *tftp, void *buf, size_t len ) {
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
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 *
 * Takes ownership of I/O buffer.
 */
static int tftp_rx_data ( struct tftp_request *tftp,
			  struct io_buffer *iobuf ) {
	struct tftp_data *data = iobuf->data;
	unsigned int block;
	size_t data_len;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *data ) ) {
		DBGC ( tftp, "TFTP %p received underlength DATA packet "
		       "length %d\n", tftp, iob_len ( iobuf ) );
		free_iob ( iobuf );
		return -EINVAL;
	}

	/* Extract data */
	block = ntohs ( data->block );
	iob_pull ( iobuf, sizeof ( *data ) );
	data_len = iob_len ( iobuf );

	/* Deliver data */
	if ( ( rc = xfer_deliver_iob ( &tftp->xfer, iobuf ) ) != 0 ) {
		DBGC ( tftp, "TFTP %p could not deliver data: %s\n",
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

/** Translation between TFTP errors and internal error numbers */
static const uint8_t tftp_errors[] = {
	[TFTP_ERR_FILE_NOT_FOUND]	= PXENV_STATUS_TFTP_FILE_NOT_FOUND,
	[TFTP_ERR_ACCESS_DENIED]	= PXENV_STATUS_TFTP_ACCESS_VIOLATION,
	[TFTP_ERR_ILLEGAL_OP]		= PXENV_STATUS_TFTP_UNKNOWN_OPCODE,
};

/**
 * Receive ERROR
 *
 * @v tftp		TFTP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * @ret rc		Return status code
 */
static int tftp_rx_error ( struct tftp_request *tftp, void *buf, size_t len ) {
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

	/* Close TFTP request */
	tftp_done ( tftp, rc );

	return 0;
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
static int tftp_socket_deliver_iob ( struct xfer_interface *socket,
				     struct io_buffer *iobuf,
				     struct xfer_metadata *meta ) {
	struct tftp_request *tftp =
		container_of ( socket, struct tftp_request, socket );
	struct sockaddr_tcpip *st_src;
	struct tftp_common *common = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc = -EINVAL;
	
	/* Sanity checks */
	if ( len < sizeof ( *common ) ) {
		DBGC ( tftp, "TFTP %p received underlength packet length %d\n",
		       tftp, len );
		goto done;
	}
	if ( ! meta ) {
		DBGC ( tftp, "TFTP %p received packet without metadata\n",
		       tftp );
		goto done;
	}
	if ( ! meta->src ) {
		DBGC ( tftp, "TFTP %p received packet without source port\n",
		       tftp );
		goto done;
	}

	/* Filter by TID.  Set TID on first response received */
	st_src = ( struct sockaddr_tcpip * ) meta->src;
	if ( tftp->state < 0 ) {
		memcpy ( &tftp->peer, st_src, sizeof ( tftp->peer ) );
		DBGC ( tftp, "TFTP %p using remote port %d\n", tftp,
		       ntohs ( tftp->peer.st_port ) );
	} else if ( memcmp ( &tftp->peer, st_src,
			     sizeof ( tftp->peer ) ) != 0 ) {
		DBGC ( tftp, "TFTP %p received packet from wrong source (got "
		       "%d, wanted %d)\n", tftp, ntohs ( st_src->st_port ),
		       ntohs ( tftp->peer.st_port ) );
		goto done;
	}

	switch ( common->opcode ) {
	case htons ( TFTP_OACK ):
		rc = tftp_rx_oack ( tftp, iobuf->data, len );
		break;
	case htons ( TFTP_DATA ):
		rc = tftp_rx_data ( tftp, iobuf );
		iobuf = NULL;
		break;
	case htons ( TFTP_ERROR ):
		rc = tftp_rx_error ( tftp, iobuf->data, len );
		break;
	default:
		DBGC ( tftp, "TFTP %p received strange packet type %d\n",
		       tftp, ntohs ( common->opcode ) );
		break;
	};

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * TFTP connection closed by network stack
 *
 * @v socket		Transport layer interface
 * @v rc		Reason for close
 */
static void tftp_socket_close ( struct xfer_interface *socket, int rc ) {
	struct tftp_request *tftp =
		container_of ( socket, struct tftp_request, socket );

	DBGC ( tftp, "TFTP %p socket closed: %s\n",
	       tftp, strerror ( rc ) );

	tftp_done ( tftp, rc );
}

/** TFTP socket operations */
static struct xfer_interface_operations tftp_socket_operations = {
	.close		= tftp_socket_close,
	.vredirect	= xfer_vopen,
	.request	= ignore_xfer_request,
	.seek		= ignore_xfer_seek,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= tftp_socket_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};
 
/**
 * Close TFTP data transfer interface
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
static void tftp_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct tftp_request *tftp =
		container_of ( xfer, struct tftp_request, xfer );

	DBGC ( tftp, "TFTP %p interface closed: %s\n",
	       tftp, strerror ( rc ) );

	tftp_done ( tftp, rc );
}

/** TFTP data transfer interface operations */
static struct xfer_interface_operations tftp_xfer_operations = {
	.close		= tftp_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.request	= ignore_xfer_request,
	.seek		= ignore_xfer_seek,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= ignore_xfer_deliver_raw,
};

/**
 * Initiate TFTP download
 *
 * @v xfer		Data transfer interface
 * @v uri		Uniform Resource Identifier
 * @ret rc		Return status code
 */
int tftp_open ( struct xfer_interface *xfer, struct uri *uri ) {
	struct tftp_request *tftp;
	struct sockaddr_tcpip server;
	int rc;

	/* Sanity checks */
	if ( ! uri->host )
		return -EINVAL;
	if ( ! uri->path )
		return -EINVAL;

	/* Allocate and populate TFTP structure */
	tftp = malloc ( sizeof ( *tftp ) );
	if ( ! tftp )
		return -ENOMEM;
	memset ( tftp, 0, sizeof ( *tftp ) );
	tftp->refcnt.free = tftp_free;
	xfer_init ( &tftp->xfer, &tftp_xfer_operations, &tftp->refcnt );
	tftp->uri = uri_get ( uri );
	xfer_init ( &tftp->socket, &tftp_socket_operations, &tftp->refcnt );
	tftp->state = -1;
	tftp->timer.expired = tftp_timer_expired;

	/* Open socket */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( tftp->uri, TFTP_PORT ) );
	if ( ( rc = xfer_open_named_socket ( &tftp->socket, SOCK_DGRAM,
					     ( struct sockaddr * ) &server,
					     uri->host, NULL ) ) != 0 )
		goto err;

	/* Start timer to initiate RRQ */
	start_timer ( &tftp->timer );

	/* Attach to parent interface, mortalise self, and return */
	xfer_plug_plug ( &tftp->xfer, xfer );
	ref_put ( &tftp->refcnt );
	return 0;

 err:
	DBGC ( tftp, "TFTP %p could not create request: %s\n",
	       tftp, strerror ( rc ) );
	tftp_done ( tftp, rc );
	ref_put ( &tftp->refcnt );
	return rc;
}

/** TFTP URI opener */
struct uri_opener tftp_uri_opener __uri_opener = {
	.scheme	= "tftp",
	.open	= tftp_open,
};
