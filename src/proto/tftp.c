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
 * @v state			TFTP transfer state
 * @v tftp_state::block		Last received data block
 * @v tftp_state::blksize	Transfer block size
 * @v data			The data block to process
 * @v buffer			The buffer to fill with the data
 * @ret True			Block processed successfully
 * @ret False			Block not processed successfully
 * @ret tftp_state::block	Incremented if applicable
 * @ret *eof			End-of-file marker
 * @err #PXENV_STATUS_TFTP_INVALID_PACKET_SIZE Packet is too large
 * @err other			As returned by fill_buffer()
 *
 * Process a TFTP DATA packet that has been received.  If the data
 * packet is the next data packet in the stream, its contents will be
 * placed in the #buffer and tftp_state::block will be incremented.
 * If the packet is the final packet, end-of-file will be indicated
 * via #eof.
 *
 * If the data packet is a duplicate, then process_tftp_data() will
 * still return True, though nothing will be done with the packet.  A
 * False return value always indicates an error that should abort the
 * transfer.
 */
static inline int tftp_process_data ( struct tftp_state *state,
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
		errno = PXENV_STATUS_TFTP_INVALID_PACKET_SIZE;
		return 0;
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
 * Download a file from a TFTP server into the specified buffer.
 */
static int tftp ( char *url __unused, struct sockaddr_in *server, char *file,
		  struct buffer *buffer ) {
	struct tftp_state state;
	union tftp_any *reply;
	int eof = 0;

	/* Initialise TFTP state */
	memset ( &state, 0, sizeof ( state ) );
	state.server = *server;
	
	/* Open the file */
	if ( ! tftp_open ( &state, file, &reply, 0 ) ) {
		DBG ( "TFTP: could not open %@:%d/%s : %m\n",
		      server->sin_addr.s_addr, server->sin_port, file );
		return 0;
	}
	
	/* Fetch file, a block at a time */
	while ( 1 ) {
		twiddle();
		switch ( ntohs ( reply->common.opcode ) ) {
		case TFTP_DATA:
			if ( ! tftp_process_data ( &state, &reply->data,
						   buffer, &eof ) ) {
				tftp_error ( &state, TFTP_ERR_ILLEGAL_OP,
					     NULL );
				return 0;
			}
			break;
		case TFTP_OACK:
			if ( state.block ) {
				/* OACK must be first block, if present */
				DBG ( "TFTP: OACK after block %d\n",
				      state.block );
				errno = PXENV_STATUS_TFTP_UNKNOWN_OPCODE;
				tftp_error ( &state, TFTP_ERR_ILLEGAL_OP,
					     NULL ); 
				return 0;
			}
			if ( ! tftp_process_opts ( &state, &reply->oack ) ) {
				DBG ( "TFTP: option processing failed: %m\n" );
				tftp_error ( &state, TFTP_ERR_BAD_OPTS, NULL );
				return 0;
			}
			break;
		default:
			DBG ( "TFTP: unexpected opcode %d\n",
			      ntohs ( reply->common.opcode ) );
			errno = PXENV_STATUS_TFTP_UNKNOWN_OPCODE;
			tftp_error ( &state, TFTP_ERR_ILLEGAL_OP, NULL );
			return 0;
		}
		/* If we have reached EOF, stop here */
		if ( eof )
			break;
		/* Fetch the next data block */
		if ( ! tftp_ack ( &state, &reply ) ) {
			DBG ( "TFTP: could not get next block: %m\n" );
			if ( ! reply ) {
				tftp_error ( &state, TFTP_ERR_ILLEGAL_OP,
					     NULL );
			}
			return 0;
		}
	}

	/* ACK the final packet, as a courtesy to the server */
	tftp_ack_nowait ( &state );

	return 1;
}

struct protocol tftp_protocol __default_protocol = {
	.name = "tftp",
	.default_port = TFTP_PORT,
	.load = tftp,
};
