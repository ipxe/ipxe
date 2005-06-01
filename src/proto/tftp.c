#include "etherboot.h"
#include "proto.h"
#include "errno.h"
#include "tftp.h"
#include "tftpcore.h"

/** @file
 *
 * TFTP protocol
 */

/**
 * Process a TFTP block
 *
 */
static inline int process_tftp_data ( struct tftp_state *state,
				      struct tftp_data *data,
				      struct buffer *buffer,
				      int *eof ) {
	unsigned int blksize;

	/* Check it's the correct DATA block */
	if ( ntohs ( data->block ) != ( state->block + 1 ) ) {
		DBG ( "TFTP: got block %d, wanted block %d\n",
		      ntohs ( data->block ), state->block + 1 );
		return 1;
	}
	/* Check it's an acceptable size */
	blksize = ( ntohs ( data->udp.len )
		    + offsetof ( typeof ( *data ), udp )
		    - offsetof ( typeof ( *data ), data ) );
	if ( blksize > state->blksize ) {
		DBG ( "TFTP: oversized block size %d (max %d)\n",
		      blksize, state->blksize );
		return 1;
	}
	/* Place block in the buffer */
	if ( ! fill_buffer ( buffer, data->data, state->block * state->blksize,
			     blksize ) ) {
		DBG ( "TFTP: could not place data in buffer: %m\n" );
		return 0;
	}
	/* Increment block counter */
	state->block++;
	/* Set EOF marker */
	*eof = ( blksize < state->blksize );
	return 1;
}

/**
 * Download a file via TFTP
 *
 */
int tftp ( char *url __unused, struct sockaddr_in *server, char *file,
	   struct buffer *buffer ) {
	struct tftp_state state;
	union tftp_any *reply;
	int eof = 0;

	/* Initialise TFTP state */
	memset ( &state, 0, sizeof ( state ) );
	state.server = *server;
	
	/* Open the file */
	if ( ! tftp_open ( &state, file, &reply ) ) {
		DBG ( "TFTP: could not open %@:%d/%s : %m\n",
		      server->sin_addr.s_addr, server->sin_port, file );
		return 0;
	}
	
	/* Process OACK, if any */
	if ( ntohs ( reply->common.opcode ) == TFTP_OACK ) {
		if ( ! tftp_process_opts ( &state, &reply->oack ) ) {
			DBG ( "TFTP: option processing failed : %m\n" );
			return 0;
		}
		reply = NULL;
	}

	/* Fetch file, a block at a time */
	do {
		/* Get next block to process.  (On the first time
		 * through, we may already have a block from
		 * tftp_open()).
		 */
		if ( ! reply ) {
			if ( ! tftp_ack ( &state, &reply ) ) {
				DBG ( "TFTP: could not get next block: %m\n" );
				return 0;
			}
		}
		twiddle();
		/* Check it's a DATA block */
		if ( ntohs ( reply->common.opcode ) != TFTP_DATA ) {
			DBG ( "TFTP: unexpected opcode %d\n",
			      ntohs ( reply->common.opcode ) );
			errno = PXENV_STATUS_TFTP_UNKNOWN_OPCODE;
			return 0;
		}
		/* Process the DATA block */
		if ( ! process_tftp_data ( &state, &reply->data, buffer,
					   &eof ) )
			return 0;
		reply = NULL;
	} while ( ! eof );

	/* ACK the final packet, as a courtesy to the server */
	tftp_ack_nowait ( &state );

	return 1;
}

struct protocol tftp_protocol __default_protocol = {
	.name = "tftp",
	.default_port = TFTP_PORT,
	.load = tftp,
};
