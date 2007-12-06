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
#include <gpxe/features.h>
#include <gpxe/bitmap.h>
#include <gpxe/tftp.h>

/** @file
 *
 * TFTP protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "TFTP", DHCP_EB_FEATURE_TFTP, 1 );

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
	/** Multicast transport layer interface */
	struct xfer_interface mc_socket;

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
	/** Multicast address
	 *
	 * This is the destination address for multicast data
	 * transmissions.
	 */
	struct sockaddr_tcpip multicast;
	/** Master client
	 *
	 * True if this is the client responsible for sending ACKs.
	 */
	int master;
	
	/** Peer address
	 *
	 * The peer address is determined by the first response
	 * received to the TFTP RRQ.
	 */
	struct sockaddr_tcpip peer;
	/** Block bitmap */
	struct bitmap bitmap;
	/** Maximum known length
	 *
	 * We don't always know the file length in advance.  In
	 * particular, if the TFTP server doesn't support the tsize
	 * option, or we are using MTFTP, then we don't know the file
	 * length until we see the end-of-file block (which, in the
	 * case of MTFTP, may not be the last block we see).
	 *
	 * This value is updated whenever we obtain information about
	 * the file length.
	 */
	size_t filesize;
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
	bitmap_free ( &tftp->bitmap );
	free ( tftp );
}

/**
 * Mark TFTP request as complete
 *
 * @v tftp		TFTP connection
 * @v rc		Return status code
 */
static void tftp_done ( struct tftp_request *tftp, int rc ) {

	DBGC ( tftp, "TFTP %p finished with status %d (%s)\n",
	       tftp, rc, strerror ( rc ) );

	/* Stop the retry timer */
	stop_timer ( &tftp->timer );

	/* Close all data transfer interfaces */
	xfer_nullify ( &tftp->socket );
	xfer_close ( &tftp->socket, rc );
	xfer_nullify ( &tftp->mc_socket );
	xfer_close ( &tftp->mc_socket, rc );
	xfer_nullify ( &tftp->xfer );
	xfer_close ( &tftp->xfer, rc );
}

/**
 * Presize TFTP receive buffers and block bitmap
 *
 * @v tftp		TFTP connection
 * @v filesize		Known minimum file size
 * @ret rc		Return status code
 */
static int tftp_presize ( struct tftp_request *tftp, size_t filesize ) {
	unsigned int num_blocks;
	int rc;

	/* Do nothing if we are already large enough */
	if ( filesize <= tftp->filesize )
		return 0;

	/* Record filesize */
	tftp->filesize = filesize;

	/* Notify recipient of file size */
	xfer_seek ( &tftp->xfer, filesize, SEEK_SET );
	xfer_seek ( &tftp->xfer, 0, SEEK_SET );

	/* Calculate expected number of blocks.  Note that files whose
	 * length is an exact multiple of the blocksize will have a
	 * trailing zero-length block, which must be included.
	 */
	num_blocks = ( ( filesize / tftp->blksize ) + 1 );
	if ( ( rc = bitmap_resize ( &tftp->bitmap, num_blocks ) ) != 0 ) {
		DBGC ( tftp, "TFTP %p could not resize bitmap to %d blocks: "
		       "%s\n", tftp, num_blocks, strerror ( rc ) );
		return rc;
	}

	return 0;
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
		       + 5 + 1 + 1 + 1 /* "tsize" + NUL + "0" + NUL */ 
		       + 9 + 1 + 1 /* "multicast" + NUL + NUL */ );
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
		  snprintf ( rrq->data, iob_tailroom ( iobuf ),
			     "%s%coctet%cblksize%c%d%ctsize%c0%cmulticast%c",
			     path, 0, 0, 0, tftp_request_blksize, 0,
			     0, 0, 0 ) + 1 );

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
	unsigned int block;

	/* Determine next required block number */
	block = bitmap_first_gap ( &tftp->bitmap );
	DBGC2 ( tftp, "TFTP %p sending ACK for block %d\n", tftp, block );

	/* Allocate buffer */
	iobuf = xfer_alloc_iob ( &tftp->socket, sizeof ( *ack ) );
	if ( ! iobuf )
		return -ENOMEM;

	/* Build ACK */
	ack = iob_put ( iobuf, sizeof ( *ack ) );
	ack->opcode = htons ( TFTP_ACK );
	ack->block = htons ( block );

	/* ACK always goes to the peer recorded from the RRQ response */
	return xfer_deliver_iob_meta ( &tftp->socket, iobuf, &meta );
}

/**
 * Transmit next relevant packet
 *
 * @v tftp		TFTP connection
 * @ret rc		Return status code
 */
static int tftp_send_packet ( struct tftp_request *tftp ) {

	/* Update retransmission timer */
	stop_timer ( &tftp->timer );
	start_timer ( &tftp->timer );

	/* If we are the master client, send RRQ or ACK as appropriate */
	if ( tftp->master ) {
		if ( ! tftp->peer.st_family ) {
			return tftp_send_rrq ( tftp );
		} else {
			return tftp_send_ack ( tftp );
		}
	} else {
		/* Do nothing when not the master client */
		return 0;
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

	return 0;
}

/**
 * Process TFTP "multicast" option
 *
 * @v tftp		TFTP connection
 * @v value		Option value
 * @ret rc		Return status code
 */
static int tftp_process_multicast ( struct tftp_request *tftp,
				    const char *value ) {
	struct sockaddr_in *sin = ( struct sockaddr_in * ) &tftp->multicast;
	char buf[ strlen ( value ) + 1 ];
	char *addr;
	char *port;
	char *port_end;
	char *mc;
	char *mc_end;
	struct sockaddr *mc_peer;
	struct sockaddr *mc_local;
	int rc;

	/* Split value into "addr,port,mc" fields */
	memcpy ( buf, value, sizeof ( buf ) );
	addr = buf;
	port = strchr ( addr, ',' );
	if ( ! port ) {
		DBGC ( tftp, "TFTP %p multicast missing port,mc\n", tftp );
		return -EINVAL;
	}
	*(port++) = '\0';
	mc = strchr ( port, ',' );
	if ( ! mc ) {
		DBGC ( tftp, "TFTP %p multicast missing mc\n", tftp );
		return -EINVAL;
	}
	*(mc++) = '\0';

	/* Parse parameters */
	if ( *addr ) {
		if ( inet_aton ( addr, &sin->sin_addr ) == 0 ) {
			DBGC ( tftp, "TFTP %p multicast invalid IP address "
			       "%s\n", tftp, addr );
			return -EINVAL;
		}
		DBGC ( tftp, "TFTP %p multicast IP address %s\n",
		       tftp, inet_ntoa ( sin->sin_addr ) );
	}
	if ( *port ) {
		sin->sin_port = htons ( strtoul ( port, &port_end, 0 ) );
		if ( *port_end ) {
			DBGC ( tftp, "TFTP %p multicast invalid port %s\n",
			       tftp, port );
			return -EINVAL;
		}
		DBGC ( tftp, "TFTP %p multicast port %d\n",
		       tftp, ntohs ( sin->sin_port ) );
	}
	tftp->master = strtoul ( mc, &mc_end, 0 );
	if ( *mc_end ) {
		DBGC ( tftp, "TFTP %p multicast invalid mc %s\n", tftp, mc );
		return -EINVAL;
	}
	DBGC ( tftp, "TFTP %p is%s the master client\n",
	       tftp, ( tftp->master ? "" : " not" ) );

	/* Open multicast socket, if new address specified */
	if ( *addr || *port ) {
		xfer_close ( &tftp->mc_socket, 0 );
		mc_peer = ( ( struct sockaddr * ) &tftp->peer );
		mc_local = ( ( struct sockaddr * ) &tftp->multicast );
		mc_local->sa_family = mc_peer->sa_family;
		if ( ( rc = xfer_open_socket ( &tftp->mc_socket, SOCK_DGRAM,
					       mc_peer, mc_local ) ) != 0 ) {
			DBGC ( tftp, "TFTP %p could not open multicast "
			       "socket: %s\n", tftp, strerror ( rc ) );
			return rc;
		}
	}

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
	{ "multicast", tftp_process_multicast },
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

	/* Unknown options should be silently ignored */
	return 0;
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
	int rc = 0;

	/* Sanity check */
	if ( len < sizeof ( *oack ) ) {
		DBGC ( tftp, "TFTP %p received underlength OACK packet "
		       "length %zd\n", tftp, len );
		rc = -EINVAL;
		goto done;
	}
	if ( end[-1] != '\0' ) {
		DBGC ( tftp, "TFTP %p received OACK missing final NUL\n",
		       tftp );
		rc = -EINVAL;
		goto done;
	}

	/* Process each option in turn */
	name = oack->data;
	while ( name < end ) {
		value = ( name + strlen ( name ) + 1 );
		if ( value == end ) {
			DBGC ( tftp, "TFTP %p received OACK missing value "
			       "for option \"%s\"\n", tftp, name );
			rc = -EINVAL;
			goto done;
		}
		if ( ( rc = tftp_process_option ( tftp, name, value ) ) != 0 )
			goto done;
		name = ( value + strlen ( value ) + 1 );
	}

	/* Process tsize information, if available */
	if ( tftp->tsize ) {
		if ( ( rc = tftp_presize ( tftp, tftp->tsize ) ) != 0 )
			goto done;
	}

	/* Request next data block */
	tftp_send_packet ( tftp );

 done:
	if ( rc )
		tftp_done ( tftp, rc );
	return rc;
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
	int block;
	off_t offset;
	size_t data_len;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *data ) ) {
		DBGC ( tftp, "TFTP %p received underlength DATA packet "
		       "length %zd\n", tftp, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto done;
	}

	/* Extract data */
	block = ( ntohs ( data->block ) - 1 );
	offset = ( block * tftp->blksize );
	iob_pull ( iobuf, sizeof ( *data ) );
	data_len = iob_len ( iobuf );
	if ( data_len > tftp->blksize ) {
		DBGC ( tftp, "TFTP %p received overlength DATA packet "
		       "length %zd\n", tftp, data_len );
		rc = -EINVAL;
		goto done;
	}

	/* Deliver data */
	xfer_seek ( &tftp->xfer, offset, SEEK_SET );
	rc = xfer_deliver_iob ( &tftp->xfer, iobuf );
	iobuf = NULL;
	if ( rc != 0 ) {
		DBGC ( tftp, "TFTP %p could not deliver data: %s\n",
		       tftp, strerror ( rc ) );
		goto done;
	}

	/* Ensure block bitmap is ready */
	if ( ( rc = tftp_presize ( tftp, ( offset + data_len ) ) ) != 0 )
		goto done;

	/* Mark block as received */
	bitmap_set ( &tftp->bitmap, block );

	/* Acknowledge block */
	tftp_send_packet ( tftp );

	/* If all blocks have been received, finish. */
	if ( bitmap_full ( &tftp->bitmap ) )
		tftp_done ( tftp, 0 );

 done:
	free_iob ( iobuf );
	if ( rc )
		tftp_done ( tftp, rc );
	return rc;
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
		       "length %zd\n", tftp, len );
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
 * @v tftp		TFTP connection
 * @v iobuf		I/O buffer
 * @v meta		Transfer metadata, or NULL
 * @ret rc		Return status code
 */
static int tftp_rx ( struct tftp_request *tftp,
		     struct io_buffer *iobuf,
		     struct xfer_metadata *meta ) {
	struct sockaddr_tcpip *st_src;
	struct tftp_common *common = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc = -EINVAL;
	
	/* Sanity checks */
	if ( len < sizeof ( *common ) ) {
		DBGC ( tftp, "TFTP %p received underlength packet length "
		       "%zd\n", tftp, len );
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
	if ( ! tftp->peer.st_family ) {
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
 * Receive new data via socket
 *
 * @v socket		Transport layer interface
 * @v iobuf		I/O buffer
 * @v meta		Transfer metadata, or NULL
 * @ret rc		Return status code
 */
static int tftp_socket_deliver_iob ( struct xfer_interface *socket,
				     struct io_buffer *iobuf,
				     struct xfer_metadata *meta ) {
	struct tftp_request *tftp =
		container_of ( socket, struct tftp_request, socket );

	return tftp_rx ( tftp, iobuf, meta );
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

	/* Any close counts as an error */
	if ( ! rc )
		rc = -ECONNRESET;

	tftp_done ( tftp, rc );
}

/** TFTP socket operations */
static struct xfer_interface_operations tftp_socket_operations = {
	.close		= tftp_socket_close,
	.vredirect	= xfer_vopen,
	.seek		= ignore_xfer_seek,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= tftp_socket_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/**
 * Receive new data via multicast socket
 *
 * @v mc_socket		Multicast transport layer interface
 * @v iobuf		I/O buffer
 * @v meta		Transfer metadata, or NULL
 * @ret rc		Return status code
 */
static int tftp_mc_socket_deliver_iob ( struct xfer_interface *mc_socket,
					struct io_buffer *iobuf,
					struct xfer_metadata *meta ) {
	struct tftp_request *tftp =
		container_of ( mc_socket, struct tftp_request, mc_socket );

	return tftp_rx ( tftp, iobuf, meta );
}

/**
 * TFTP multicast connection closed by network stack
 *
 * @v socket		Multicast transport layer interface
 * @v rc		Reason for close
 */
static void tftp_mc_socket_close ( struct xfer_interface *mc_socket,
				   int rc ) {
	struct tftp_request *tftp =
		container_of ( mc_socket, struct tftp_request, mc_socket );

	DBGC ( tftp, "TFTP %p multicast socket closed: %s\n",
	       tftp, strerror ( rc ) );

	/* The multicast socket may be closed when we receive a new
	 * OACK and open/reopen the socket; we should not call
	 * tftp_done() at this point.
	 */
}
 
/** TFTP multicast socket operations */
static struct xfer_interface_operations tftp_mc_socket_operations = {
	.close		= tftp_mc_socket_close,
	.vredirect	= xfer_vopen,
	.seek		= ignore_xfer_seek,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= tftp_mc_socket_deliver_iob,
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
	.seek		= ignore_xfer_seek,
	.window		= unlimited_xfer_window,
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
	tftp = zalloc ( sizeof ( *tftp ) );
	if ( ! tftp )
		return -ENOMEM;
	tftp->refcnt.free = tftp_free;
	xfer_init ( &tftp->xfer, &tftp_xfer_operations, &tftp->refcnt );
	tftp->uri = uri_get ( uri );
	xfer_init ( &tftp->socket, &tftp_socket_operations, &tftp->refcnt );
	xfer_init ( &tftp->mc_socket, &tftp_mc_socket_operations,
		    &tftp->refcnt );
	tftp->blksize = TFTP_DEFAULT_BLKSIZE;
	tftp->master = 1;
	tftp->timer.expired = tftp_timer_expired;

	/* Open socket */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( tftp->uri, TFTP_PORT ) );
	if ( ( rc = xfer_open_named_socket ( &tftp->socket, SOCK_DGRAM,
					     ( struct sockaddr * ) &server,
					     uri->host, NULL ) ) != 0 )
		goto err;

	/* Start timer to initiate RRQ */
	start_timer_nodelay ( &tftp->timer );

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
