#include "etherboot.h"
#include "proto.h"
#include "errno.h"
#include "tftp.h"
#include "tftpcore.h"

/** @file
 *
 * TFTM protocol
 *
 * TFTM is a protocol defined in RFC2090 as a multicast extension to
 * TFTP.
 */

static inline int tftm_process_opts ( struct tftp_state *state,
				      struct tftp_oack *oack ) {
	struct in_addr old_mcast_addr = state->multicast.sin_addr;

	if ( ! tftp_process_opts ( state, oack ) )
		return 0;

	if ( old_mcast_addr.s_addr != state->multicast.sin_addr.s_addr ) {
		if ( old_mcast_addr.s_addr ) {
			DBG ( "TFTM: Leaving multicast group %@\n",
			      old_mcast_addr.s_addr );
			leave_group ( IGMP_SERVER );
		}
		DBG ( "TFTM: Joining multicast group %@\n",
		      state->multicast.sin_addr.s_addr );
		join_group ( IGMP_SERVER, state->multicast.sin_addr.s_addr );
	}

	DBG ( "TFTM: I am a %s client\n",
	      ( state->master ? "master" : "slave" ) );

	return 1;
}


static inline int tftm_process_data ( struct tftp_state *state,
				      struct tftp_data *data,
				      struct buffer *buffer ) {
	unsigned int blksize;
	off_t offset;

	/* Calculate block size and offset within file */
	blksize = ( ntohs ( data->udp.len )
		    + offsetof ( typeof ( *data ), udp )
		    - offsetof ( typeof ( *data ), data ) );
	offset = ( ntohs ( data->block ) - 1 ) * state->blksize;

	/* Check for oversized block */
	if ( blksize > state->blksize ) {
		DBG ( "TFTM: oversized block size %d (max %d)\n",
		      blksize, state->blksize );
		errno = PXENV_STATUS_TFTP_INVALID_PACKET_SIZE;
		return 0;
	}

	/* Place block in the buffer */
	if ( ! fill_buffer ( buffer, data->data, offset, blksize ) ) {
		DBG ( "TFTM: could not place data in buffer: %m\n" );
		return 0;
	}

	/* If this is the last block, record the filesize (in case the
	 * server didn't supply a tsize option.
	 */
	if ( blksize < state->blksize ) {
		state->tsize = offset + blksize;
	}

	/* Record the last received block */
	state->block = ntohs ( data->block );

	return 1;
}


static inline int tftm_next ( struct tftp_state *state,
			      union tftp_any **reply,
			      struct buffer *buffer ) {
	long listen_timeout;

	listen_timeout = rfc2131_sleep_interval ( TIMEOUT, MAX_TFTP_RETRIES );

	/* If we are not the master client, just listen for the next
	 * packet
	 */
	if ( ! state->master ) {
		if ( tftp_get ( state, listen_timeout, reply ) ) {
			/* Heard a non-error packet */
			return 1;
		}
		if ( *reply ) {
			/* Received an error packet */
			return 0;
		}
		/* Didn't hear anything; try prodding the server */
		state->master = 1;
	}
	/* We are the master client; trigger the next packet
	 * that we want
	 */
	state->block = buffer->fill / state->blksize;
	return tftp_ack ( state, reply );
}

/**
 * Download a file via TFTM
 *
 * @v server				TFTP server
 * @v file				File name
 * @v buffer				Buffer into which to load file
 * @ret True				File was downloaded successfully
 * @ret False				File was not downloaded successfully
 * @err #PXENV_STATUS_TFTP_UNKNOWN_OPCODE Unknown type of TFTP block received
 * @err other				As returned by tftp_open()
 * @err other				As returned by tftp_process_opts()
 * @err other				As returned by tftp_ack()
 * @err other				As returned by tftp_process_data()
 *
 * Download a file from a TFTP server into the specified buffer using
 * the TFTM protocol.
 */
static int tftm ( char *url __unused, struct sockaddr_in *server, char *file,
		  struct buffer *buffer ) {
	struct tftp_state state;
	union tftp_any *reply;
	int rc = 0;

	/* Initialise TFTP state */
	memset ( &state, 0, sizeof ( state ) );
	state.server = *server;

	/* Start as the master.  This means that if the TFTP server
	 * doesn't actually support multicast, we'll still ACK the
	 * packets and it should all proceed as for a normal TFTP
	 * connection.
	 */
	state.master = 1;
	
	/* Open the file */
	if ( ! tftp_open ( &state, file, &reply, 1 ) ) {
		DBG ( "TFTM: could not open %@:%d/%s : %m\n",
		      server->sin_addr.s_addr, server->sin_port, file );
		return 0;
	}

	/* Fetch file, a block at a time */
	while ( 1 ) {
		twiddle();
		/* Process the current packet */
		switch ( ntohs ( reply->common.opcode ) ) {
		case TFTP_OACK:
			/* Options can be received at any time */
			if ( ! tftm_process_opts ( &state, &reply->oack ) ) {
				DBG ( "TFTM: failed to process OACK: %m\n" );
				tftp_error ( &state, TFTP_ERR_BAD_OPTS, NULL );
				goto out;
			}
			break;
		case TFTP_DATA:
			if ( ! tftm_process_data ( &state, &reply->data,
						   buffer ) ) {
				DBG ( "TFTM: failed to process DATA: %m\n" );
				tftp_error ( &state, TFTP_ERR_ILLEGAL_OP,
					     NULL );
				goto out;
			}
			break;
		default:
			DBG ( "TFTM: unexpected packet type %d\n",
			      ntohs ( reply->common.opcode ) );
			errno = PXENV_STATUS_TFTP_UNKNOWN_OPCODE;
			tftp_error ( &state, TFTP_ERR_ILLEGAL_OP, NULL );
			goto out;
		}
		/* If we know the filesize, and we have all the data, stop */
		if ( state.tsize && ( buffer->fill == state.tsize ) )
			break;
		/* Fetch the next packet */
		if ( ! tftm_next ( &state, &reply, buffer ) ) {
			DBG ( "TFTM: could not get next block: %m\n" );
			if ( ! reply ) {
				tftp_error ( &state, TFTP_ERR_ILLEGAL_OP,
					     NULL );
			}
			goto out;
		}
	}

	/* ACK the final packet, as a courtesy to the server */
	tftp_ack_nowait ( &state );

	rc = 1;
 out:
	if ( state.multicast.sin_addr.s_addr ) {
		leave_group ( IGMP_SERVER );
	}
	return rc;
}

static struct protocol tftm_protocol __protocol = {
	.name = "x-tftm",
	.default_port = TFTP_PORT,
	.load = tftm,
};
