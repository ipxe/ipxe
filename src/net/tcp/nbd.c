/*
 * Copyright (C) 2021 Petr Borsodi, S.ICZ a.s.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/blockdev.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/socket.h>
#include <ipxe/iobuf.h>
#include <ipxe/uri.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/process.h>
#include <ipxe/tcpip.h>
#include <ipxe/nbd.h>
#include <ipxe/features.h>

#define NBD_BLOCK_SIZE 512

/** Maximum number of blocks per request
 *
 * This is a policy decision.
 */
#define NBD_MAX_BLOCK_COUNT ( 16 * 1024 / NBD_BLOCK_SIZE )

FEATURE ( FEATURE_PROTOCOL, "NBD", DHCP_EB_FEATURE_NBD, 1 );

enum nbd_command_type {
	NBD_BLOCK_READ = 0,
	NBD_BLOCK_WRITE,
	NBD_BLOCK_READ_CAPACITY,
};

/** A NBD block command */
struct nbd_command {
	/** Reference count */
	struct refcnt refcnt;
	/** NBD session */
	struct nbd_session *nbd;

	/** Block data interface */
	struct interface block;

	/** Request handle (tag), in network order */
	uint64_t handle;

	/** Command type */
	enum nbd_command_type type;
	/** Starting logical block address */
	uint64_t lba;
	/** Number of blocks */
	unsigned int count;

	/** Data buffer */
	userptr_t data_buffer;
	/** Data buffer length */
	size_t data_len;
	/** Offset within data buffer */
	size_t data_offset;
};

static void nbd_start_tx ( struct nbd_session *nbd,
			   enum nbd_tx_state tx_state );


/**
 * Free NBD command
 *
 * @v refcnt		Reference count
 */
static void nbdcmd_free ( struct refcnt *refcnt ) {
	struct nbd_command *nbdcmd =
		container_of ( refcnt, struct nbd_command, refcnt );
	struct nbd_session *nbd = nbdcmd->nbd;

	/* Drop reference to NBD session */
	ref_put ( &nbd->refcnt );

	/* Free command */
	free ( nbdcmd );
}

/**
 * Close NBD command
 *
 * @v nbdcmd		NBD command
 * @v rc		Reason for close
 */
static void nbdcmd_close ( struct nbd_command *nbdcmd, int rc ) {
	struct nbd_session *nbd = nbdcmd->nbd;

	if ( rc != 0 ) {
		DBGC ( nbd, "NBD %p cmd %p closed: %s\n",
		       nbd, nbdcmd, strerror ( rc ) );
	}

	/* Sanity check */
	assert ( nbd->command == nbdcmd );

	/* Shut down interface */
	intf_shutdown ( &nbdcmd->block, rc );

	/* Drop session's reference */
	ref_put ( &nbdcmd->refcnt );
	nbd->command = NULL;
}

/**
 * Assign new NBD request handle
 *
 * @v nbdcmd		NBD command
 */
static void nbdcmd_new_handle ( struct nbd_command *nbdcmd ) {
	static uint32_t handle_idx;

	nbdcmd->handle = htonl ( ++handle_idx );
}

/** NBD command block interface operations */
static struct interface_operation nbdcmd_block_op[] = {
	INTF_OP ( intf_close, struct nbd_command *, nbdcmd_close ),
};

/** NBD command block interface descriptor */
static struct interface_descriptor nbdcmd_block_desc =
	INTF_DESC ( struct nbd_command, block, nbdcmd_block_op );


/**
 * Create NBD command
 *
 * @v nbd		NBD session
 * @v block		Block data interface
 * @v type		NBD command type
 * @v lba		Starting logical block address
 * @v count		Number of blocks to transfer
 * @v buffer		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int nbd_command ( struct nbd_session *nbd, struct interface *block,
			 enum nbd_command_type type,
			 uint64_t lba, unsigned int count,
			 userptr_t buffer, size_t len ) {
	struct nbd_command *nbdcmd;

	/* Sanity check */
	assert ( nbd->command == NULL && nbd->rx_state == NBD_RX_TRANS_REP_CMD );

	/* Allocate and initialise structure */
	nbdcmd = zalloc ( sizeof ( *nbdcmd ) );
	if ( ! nbdcmd )
		return -ENOMEM;

	ref_init ( &nbdcmd->refcnt, nbdcmd_free );
	intf_init ( &nbdcmd->block, &nbdcmd_block_desc, &nbdcmd->refcnt );

	nbdcmd->nbd = nbd;
	nbd->command = nbdcmd;
	ref_get ( &nbdcmd->nbd->refcnt );

	nbdcmd_new_handle ( nbdcmd );

	nbdcmd->type = type;
	nbdcmd->lba = lba;
	nbdcmd->count = count;

	nbdcmd->data_buffer = buffer;
	nbdcmd->data_len = len;

	/* Attach to parent interface, transfer reference to session,
	 * and return
	 */
	intf_plug_plug ( &nbdcmd->block, block );
	return 0;
}


/**
 * Free NBD session
 *
 * @v refcnt		Reference counter
 */
static void nbd_free ( struct refcnt *refcnt ) {
	struct nbd_session *nbd =
		container_of ( refcnt, struct nbd_session, refcnt );

	uri_put ( nbd->uri );
	free ( nbd );
}

/**
 * Shut down NBD interface
 *
 * @v nbd		NBD session
 * @v rc		Reason for close
 */
static void nbd_close ( struct nbd_session *nbd, int rc ) {
	/* A TCP graceful close is still an error from our point of view */
	if ( rc == 0 )
		rc = -ECONNRESET;

	DBGC ( nbd, "NBD %p closed: %s\n", nbd, strerror ( rc ) );

	/* Stop transmission process */
	process_del ( &nbd->process );

	if ( nbd->command )
		nbdcmd_close ( nbd->command, rc );

	/* Shut down interfaces */
	intfs_shutdown ( rc, &nbd->block, &nbd->socket, NULL );
}


/****************************************************************************
 *
 * Block to NBD interface
 *
 */

/**
 * Check NBD flow-control window
 *
 * @v nbd		NBD session
 * @ret len		Length of window
 */
static size_t nbd_block_window ( struct nbd_session *nbd ) {

	if ( nbd->rx_state >= NBD_RX_TRANS_REP_CMD &&
	     nbd->command == NULL ) {
		/* We cannot handle concurrent commands */
		return 1;
	} else {
		return 0;
	}
}

/**
 * Issue NBD block read
 *
 * @v nbd		NBD session
 * @v block		Block data interface
 * @v lba		Starting logical block address
 * @v count		Number of blocks to transfer
 * @v buffer		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int nbd_block_read ( struct nbd_session *nbd,
			    struct interface *block,
			    uint64_t lba, unsigned int count,
			    userptr_t buffer, size_t len ) {
	int rc;

	DBGC2 ( nbd, "NBD %p block %p read LBA 0x%08llx count 0x%04x\n",
		nbd, block, lba, count );

	if ( ( rc = nbd_command ( nbd, block, NBD_BLOCK_READ,
				  lba, count, buffer, len ) ) != 0 )
		return rc;

	/* Start command processing */
	nbd_start_tx ( nbd, NBD_TX_CMD_HEADER );
	return 0;
}

/**
 * Issue NBD block write
 *
 * @v nbd		NBD session
 * @v block		Block data interface
 * @v lba		Starting logical block address
 * @v count		Number of blocks to transfer
 * @v buffer		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int nbd_block_write ( struct nbd_session *nbd,
			     struct interface *block,
			     uint64_t lba, unsigned int count,
			     userptr_t buffer, size_t len ) {
	int rc;

	DBGC2 ( nbd, "NBD %p block %p write LBA 0x%08llx count 0x%04x\n",
		nbd, block, lba, count );

	if ( nbd->trans_flags & NBD_FLAG_READ_ONLY ) {
		DBGC ( nbd, "NBD %p read only media\n", nbd );
		return -EROFS;
	}

	if ( ( rc = nbd_command ( nbd, block, NBD_BLOCK_WRITE,
				  lba, count, buffer, len ) ) != 0 )
		return rc;

	/* Start command processing */
	nbd_start_tx ( nbd, NBD_TX_CMD_HEADER );
	return 0;
}

/**
 * Read NBD device capacity
 *
 * @v nbd		NBD session
 * @v block		Block data interface
 * @ret rc		Return status code
 */
static int nbd_block_read_capacity ( struct nbd_session *nbd,
				     struct interface *block ) {
	int rc;

	DBGC2 ( nbd, "NBD %p block read capacity %p\n", nbd, block );

	if ( ( rc = nbd_command ( nbd, block, NBD_BLOCK_READ_CAPACITY,
				  0, 0, UNULL, 0 ) ) != 0 )
		return rc;

	/* Start command processing */
	nbd_start_tx ( nbd, NBD_TX_CMD_BRC );
	return 0;
}

/**
 * Describe as an EFI device path
 *
 * @v nbd		NBD session
 * @ret path		EFI device path, or NULL on error
 */
static EFI_DEVICE_PATH_PROTOCOL * nbd_efi_describe ( struct nbd_session *nbd ) {
	DBGC2 ( nbd, "NBD %p EFI describe\n", nbd );
	return efi_uri_path ( nbd->uri );
}


/** NBD block interface operations */
static struct interface_operation nbd_block_op[] = {
	INTF_OP ( xfer_window, struct nbd_session *, nbd_block_window ),
	INTF_OP ( block_read, struct nbd_session *, nbd_block_read ),
	INTF_OP ( block_write, struct nbd_session *, nbd_block_write ),
	INTF_OP ( block_read_capacity, struct nbd_session *,
		  nbd_block_read_capacity ),
	INTF_OP ( intf_close, struct nbd_session *, nbd_close ),
	EFI_INTF_OP ( efi_describe, struct nbd_session *, nbd_efi_describe ),
};

/** NBD block interface descriptor */
static struct interface_descriptor nbd_block_desc =
	INTF_DESC ( struct nbd_session, block, nbd_block_op );


/****************************************************************************
 *
 * NBD to socket interface
 *
 */

/**
 * Pause TX engine
 *
 * @v nbd		NBD session
 */
static void nbd_tx_pause ( struct nbd_session *nbd ) {
	process_del ( &nbd->process );
}

/**
 * Resume TX engine
 *
 * @v nbd		NBD session
 */
static void nbd_tx_resume ( struct nbd_session *nbd ) {
	process_add ( &nbd->process );
}

/**
 * Start up a new transmit
 *
 * @v nbd		NBD session
 * @v tx_state		New transmit state
 *
 * This initiates the process of sending a new data.
 */
static void nbd_start_tx ( struct nbd_session *nbd,
			   enum nbd_tx_state tx_state ) {
	assert ( nbd->tx_state == NBD_TX_IDLE );

	/* Set new TX state */
	nbd->tx_state = tx_state;

	/* Start transmission process */
	nbd_tx_resume ( nbd );
}

/**
 * Transmit an option (NBD_OPT_EXPORT_NAME or NBD_OPT_GO)
 *
 * @v nbd		NBD session
 * @ret rc		Return status code
 */
int nbd_tx_neg_opt ( struct nbd_session *nbd ) {
	struct io_buffer *iobuf;
	struct {
		uint32_t client_flags;
		union {
			struct nbd_proto_opt_export_name opt_export_name;
			struct nbd_proto_opt_go opt_go;
		};
	} __attribute__ (( packed )) *opt;
	size_t name_len, opt_len;
	int rc;

	name_len = strlen ( nbd->export_name );
	if ( nbd->use_opt_go ) {
		opt_len = offsetof ( typeof ( *opt ),
				     opt_go.export_name ) +
				     name_len +
				     sizeof ( uint16_t );
	} else {
		opt_len = offsetof ( typeof ( *opt ),
				     opt_export_name.export_name ) +
				     name_len;
	}

	iobuf = xfer_alloc_iob ( &nbd->socket, opt_len );
	if ( ! iobuf )
		return -ENOMEM;

	opt = iob_put ( iobuf, opt_len );
	memset ( opt, 0, opt_len );

	opt->client_flags =
		htonl ( NBD_FLAG_C_FIXED_NEWSTYLE |
			( nbd->handshake_flags & NBD_FLAG_NO_ZEROES ?
			NBD_FLAG_C_NO_ZEROES : 0 ) );

	if ( nbd->use_opt_go ) {
		/* Construct NBD_OPT_GO option with empty NBD_INFO list */
		opt->opt_go.request.request_magic =
			htonll ( NBD_OPT_REQ_MAGIC );
		opt->opt_go.request.option = htonl ( NBD_OPT_GO );
		opt->opt_go.request.length = htonl ( opt_len -
			offsetof ( typeof ( *opt ), opt_go.name_length ) );
		opt->opt_go.name_length = htonl ( name_len );
		memcpy ( opt->opt_go.export_name, nbd->export_name, name_len );
	} else {
		/* Construct NBD_OPT_EXPORT_NAME option */
		opt->opt_export_name.request.request_magic =
			htonll ( NBD_OPT_REQ_MAGIC );
		opt->opt_export_name.request.option =
			htonl ( NBD_OPT_EXPORT_NAME );
		opt->opt_export_name.request.length = htonl ( name_len );
		memcpy ( opt->opt_export_name.export_name, nbd->export_name, name_len );
	}

	DBGCP ( nbd, "transmit:\n" );
	DBGCP_HD ( nbd, opt, opt_len );

	/* Deliver packet */
	if ( ( rc = xfer_deliver_iob ( &nbd->socket,
				       iob_disown ( iobuf ) ) ) != 0 ) {
		DBGC ( nbd, "NBD %p cannot transmit: %s\n",
		       nbd, strerror ( rc ) );
		return rc;
	}

	nbd->rx_state = nbd->use_opt_go ?
		NBD_RX_NEG_OPT_INFO : NBD_RX_NEG_EXP_NAME;
	nbd->tx_state = NBD_TX_IDLE;
	return 0;
}

/**
 * Process the block_read_capacity command
 *
 * @v nbd		NBD session
 */
void nbd_tx_cmd_brc ( struct nbd_session *nbd ) {
	struct nbd_command *nbdcmd = nbd->command;
	struct block_device_capacity capacity;

	/* Sanity check */
	assert ( nbdcmd != NULL && nbdcmd->type == NBD_BLOCK_READ_CAPACITY );

	capacity.blocks = nbd->export_size / NBD_BLOCK_SIZE;
	capacity.blksize = NBD_BLOCK_SIZE;
	/* Use reasonable amount of data */
	capacity.max_count = NBD_MAX_BLOCK_COUNT;

	/* Report block device capacity */
	block_capacity ( &nbdcmd->block, &capacity );

	nbdcmd_close (nbdcmd, 0);
	nbd->tx_state = NBD_TX_IDLE;
}

/**
 * Transmit a command header
 *
 * @v nbd		NBD session
 * @ret rc		Return status code
 */
int nbd_tx_cmd_header ( struct nbd_session *nbd ) {
	struct nbd_command *nbdcmd = nbd->command;
	struct nbd_proto_trans_request request;
	int rc;

	/* Sanity check */
	assert ( nbdcmd != NULL &&
		 ( nbdcmd->type == NBD_BLOCK_READ ||
		   nbdcmd->type == NBD_BLOCK_WRITE ) );

	request.request_magic = htonl ( NBD_REQUEST_MAGIC );
	request.flags = 0;
	if ( nbdcmd->type == NBD_BLOCK_READ ) {
		request.type = htons ( NBD_CMD_READ );
	} else {
		request.type = htons ( NBD_CMD_WRITE );
	}
	request.handle = nbdcmd->handle;
	request.offset = htonll ( nbdcmd->lba * NBD_BLOCK_SIZE );
	request.length = htonl ( nbdcmd->count * NBD_BLOCK_SIZE );

	/* Deliver request */
	if ( ( rc = xfer_deliver_raw ( &nbd->socket, &request,
				       sizeof ( request ) ) ) != 0 ) {
		DBGC ( nbd, "NBD %p cannot transmit: %s\n",
		       nbd, strerror ( rc ) );
		return rc;
	}

	if ( nbdcmd->type == NBD_BLOCK_READ ) {
		nbd->tx_state = NBD_TX_IDLE;
	} else {
		nbd->tx_state = NBD_TX_CMD_DATA;
	}

	return 0;
}

/**
 * Transmit a command data
 *
 * @v nbd		NBD session
 * @ret rc		Return status code
 */
int nbd_tx_cmd_data ( struct nbd_session *nbd ) {
	struct nbd_command *nbdcmd = nbd->command;
	size_t len;
	struct io_buffer *iobuf;
	int rc;

	/* Sanity check */
	assert ( nbdcmd != NULL && nbdcmd->type == NBD_BLOCK_WRITE );

	len = nbdcmd->data_len - nbdcmd->data_offset;
	/* Always send 512-byte data chunks */
	if ( len > NBD_BLOCK_SIZE )
		len = NBD_BLOCK_SIZE;

	/* Deliver data */
	iobuf = xfer_alloc_iob ( &nbd->socket, len );
	if ( ! iobuf )
		return -ENOMEM;

	copy_from_user ( iob_put ( iobuf, len ),
			 nbdcmd->data_buffer, nbdcmd->data_offset, len );
	if ( ( rc = xfer_deliver_iob ( &nbd->socket,
				       iob_disown ( iobuf ) ) ) != 0 ) {
		DBGC ( nbd, "NBD %p cannot transmit: %s\n",
		       nbd, strerror ( rc ) );
		return rc;
	}

	nbdcmd->data_offset += len;
	if ( nbdcmd->data_offset == nbdcmd->data_len )
		nbd->tx_state = NBD_TX_IDLE;

	return 0;
}

/**
 * NBD transmit process
 *
 * @v nbd		NBD session
 */
static void nbd_tx_step ( struct nbd_session *nbd ) {
	int ( * tx ) ( struct nbd_session *nbd );
	int rc;

	while ( 1 ) {
		DBGCP ( nbd, "NBD %p try to transmit from state %d\n",
			nbd, nbd->tx_state );

		switch ( nbd->tx_state ) {
		case NBD_TX_IDLE:
			/* Nothing to do; pause processing */
			nbd_tx_pause ( nbd );
			return;

		case NBD_TX_NEG_OPT:
			tx = nbd_tx_neg_opt;
			break;

		case NBD_TX_CMD_BRC:
			/* block_read_capacity command don't transmit really */
			nbd_tx_cmd_brc ( nbd );
			continue;

		case NBD_TX_CMD_HEADER:
			tx = nbd_tx_cmd_header;
			break;

		case NBD_TX_CMD_DATA:
			tx = nbd_tx_cmd_data;
			break;

		default:
			assert ( 0 );
			return;
		}

		/* Check for window availability */
		if ( xfer_window ( &nbd->socket ) == 0 ) {
			/* Cannot transmit at this point; pause
			 * processing and wait for window to reopen
			 */
			nbd_tx_pause ( nbd );
			return;
		}

		/* Transmit data */
		if ( ( rc = tx ( nbd ) ) != 0 ) {
			DBGC ( nbd, "NBD %p could not transmit: %s\n",
			       nbd, strerror ( rc ) );
			/* Transmission errors are fatal */
			nbd_close ( nbd, rc );
			return;
		}
	}
}


/** NBD command reply process descriptor */
static struct process_descriptor nbd_process_desc =
	PROC_DESC ( struct nbd_session, process, nbd_tx_step );


/**
 * Negotiation is done, enter transmission phase
 *
 * @v nbd		NBD session
 */
static inline void nbd_neg_done ( struct nbd_session *nbd ) {
	DBGC2 ( nbd, "NBD %p negotiation done, enter transmission\n", nbd );
	DBGC2 ( nbd, "NBD %p export size: %lld MiB, flags: 0x%04x\n", nbd,
		nbd->export_size / 1048576, nbd->trans_flags );

	nbd->rx_state = NBD_RX_TRANS_REP_CMD;
	xfer_window_changed ( &nbd->block );
}

/**
 * Process initial negotiation
 *
 * @v nbd		NBD session
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int nbd_rx_neg_init ( struct nbd_session *nbd ) {
	DBGC2 ( nbd, "NBD %p initial handshake:\n", nbd );
	DBGC2_HD ( nbd, &nbd->rx_neg_init, sizeof ( nbd->rx_neg_init ) );

	if ( nbd->rx_neg_init.init_magic != htonll ( NBD_INIT_PASSWD ) ||
	     nbd->rx_neg_init.opt_magic != htonll ( NBD_OPT_REQ_MAGIC ) ) {
		DBGC ( nbd, "NBD %p initial handshake failed (1)\n", nbd );
		return -EPROTO;
	}

	nbd->handshake_flags = ntohs ( nbd->rx_neg_init.handshake_flags );
	if ( ! ( nbd->handshake_flags & NBD_FLAG_FIXED_NEWSTYLE ) ) {
		DBGC ( nbd, "NBD %p initial handshake failed (2)\n", nbd );
		return -EPROTO;
	}

	/* Start transmit an option */
	nbd_start_tx ( nbd, NBD_TX_NEG_OPT );
	return 0;
}

/**
 * Process reply for NBD_OPT_EXPORT_NAME
 *
 * @v nbd		NBD session
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int nbd_rx_neg_exp_name ( struct nbd_session *nbd ) {
	DBGC2 ( nbd, "NBD %p export name reply:\n", nbd );
	DBGC2_HD ( nbd, &nbd->rx_exp_name_reply,
		   sizeof ( nbd->rx_exp_name_reply ) );

	nbd->export_size = ntohll ( nbd->rx_exp_name_reply.export_size );
	nbd->trans_flags = ntohs ( nbd->rx_exp_name_reply.trans_flags );

	/* Discard NBD_ZERO_PAD_LEN bytes if applicable */
	if ( ! ( nbd->handshake_flags & NBD_FLAG_NO_ZEROES ) )
		nbd->discard_len = NBD_ZERO_PAD_LEN;

	nbd_neg_done ( nbd );
	return 0;
}

/**
 * Process reply for NBD_OPT_GO
 *
 * @v nbd		NBD session
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int nbd_rx_neg_opt_info ( struct nbd_session *nbd ) {
	uint32_t opt_type, opt_len;

	DBGC2 ( nbd, "NBD %p option reply:\n", nbd );
	DBGC2_HD ( nbd, &nbd->rx_opt_reply, sizeof ( nbd->rx_opt_reply ) );

	if ( nbd->rx_opt_reply.reply_magic != htonll ( NBD_OPT_REPLY_MAGIC ) ||
	     nbd->rx_opt_reply.option != htonl ( NBD_OPT_GO ) ) {
		DBGC ( nbd, "NBD %p option info failed (1)\n", nbd );
		return -EPROTO;
	}

	opt_type = ntohl ( nbd->rx_opt_reply.type );
	opt_len = ntohl ( nbd->rx_opt_reply.length );

	if ( opt_type & NBD_REP_FLAG_ERROR ) {
		switch ( opt_type ) {
		case NBD_REP_ERR_UNSUP:
			DBGC ( nbd, "NBD %p NBD_OPT_GO option not supported\n",
			       nbd );
			return -ENOTSUP;

		case NBD_REP_ERR_UNKNOWN:
			DBGC ( nbd, "NBD %p requested export is not "
			       "available\n", nbd );
			return -ENOENT;

		default:
			DBGC ( nbd, "NBD %p option info failed (2),"
			       " type = 0x%08x\n", nbd, opt_type );
			return -EPROTO;
		}
	}

	switch ( opt_type ) {
	case NBD_REP_ACK:
		if ( nbd->export_size == 0 || opt_len != 0 ) {
			DBGC ( nbd, "NBD %p option info failed (3)\n", nbd );
			return -EPROTO;
		}

		nbd_neg_done ( nbd );
		break;

	case NBD_REP_INFO:
		if ( opt_len <= sizeof ( nbd-> rx_rep_info_export ) ) {
			nbd->reply_info_length = opt_len;
			nbd->rx_state = NBD_RX_NEG_REP_INFO;
			break;
		}
		/* Fall through */
	default:
		DBGC2 ( nbd, "NBD %p option info ignored, type = 0x%08x\n",
			nbd, opt_type );
		nbd->discard_len = opt_len;
		break;
	}

	return 0;
}

/**
 * Process NBD_REP_INFO
 *
 * @v nbd		NBD session
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int nbd_rx_neg_rep_info ( struct nbd_session *nbd ) {
	DBGC2 ( nbd, "NBD %p rep info:\n", nbd );
	DBGC2_HD ( nbd, &nbd->rx_rep_info_export, nbd->reply_info_length );

	if ( nbd->rx_rep_info_export.type == htons ( NBD_INFO_EXPORT ) ) {
		nbd->export_size =
			ntohll ( nbd->rx_rep_info_export.export_size );
		nbd->trans_flags =
			ntohs ( nbd->rx_rep_info_export.trans_flags );
	}

	nbd->rx_state = NBD_RX_NEG_OPT_INFO;
	return 0;
}

/**
 * Process reply to NBD_CMD_?
 *
 * @v nbd		NBD session
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int nbd_rx_trans_rep_cmd ( struct nbd_session *nbd ) {
	struct nbd_command *nbdcmd = nbd->command;
	uint32_t errno;
	DBGCP ( nbd, "NBD %p cmd %p reply:\n", nbd, nbdcmd );
	DBGCP_HD ( nbd, &nbd->rx_trans_reply, sizeof ( nbd->rx_trans_reply ) );

	if ( nbdcmd == NULL ) {
		DBGC ( nbd, "NBD %p no cmd to process\n", nbd );
		return -EPROTO;
	}

	if ( nbd->rx_trans_reply.reply_magic != htonl ( NBD_REPLY_MAGIC ) ||
	     nbd->rx_trans_reply.handle != nbdcmd->handle ) {
		DBGC ( nbd, "NBD %p cmd reply invalid\n", nbd );
		return -EPROTO;
	}

	errno = ntohl ( nbd->rx_trans_reply.error );
	if ( errno != 0 ) {
		DBGC ( nbd, "NBD %p cmd errno: %d\n", nbd, errno );
		return -EIO;
	}

	if ( nbdcmd->type == NBD_BLOCK_READ ) {
		nbd->rx_state = NBD_RX_TRANS_DATA;
	} else {
		DBGCP ( nbd, "NBD %p cmd complete\n", nbd );
		nbdcmd_close ( nbdcmd, 0 );
	}

	return 0;
}

/**
 * Process data of NBD_CMD_READ
 *
 * @v nbd		NBD session
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int nbd_rx_trans_data ( struct nbd_session *nbd,
			       struct io_buffer *iobuf ) {
	struct nbd_command *nbdcmd = nbd->command;
	size_t len = iob_len ( iobuf );
	DBGCP ( nbd, "NBD %p cmd %p data len: %zd\n", nbd, nbdcmd, len );

	if ( nbdcmd == NULL ) {
		DBGC ( nbd, "NBD %p no cmd to process\n", nbd );
		return -EPROTO;
	}

	/* Sanity checks */
	if ( nbdcmd->data_offset + len > nbdcmd->data_len ) {
		DBGC ( nbd, "NBD %p data overrun\n", nbd );
		return -ERANGE;
	}

	DBGCP ( nbd, "NBD %p copy %zd of %zd offset %zd\n",
		nbd, len, nbdcmd->data_len, nbdcmd->data_offset);
	copy_to_user ( nbdcmd->data_buffer, nbdcmd->data_offset,
		       iobuf->data, len );
	nbdcmd->data_offset += len;

	if ( nbdcmd->data_offset == nbdcmd->data_len ) {
		DBGCP ( nbd, "NBD %p cmd complete\n", nbd );
		nbd->rx_state = NBD_RX_TRANS_REP_CMD;
		nbdcmd_close ( nbdcmd, 0 );
	}

	return 0;
}

/**
 * Handle received NBD data
 *
 * @v nbd		NBD session
 * @v iobuf		I/O buffer
 * @v meta		Transfer metadata
 * @ret rc		Return status code
 *
 * This function takes ownership of the I/O buffer.
 */
static int nbd_socket_deliver ( struct nbd_session *nbd,
				struct io_buffer *iobuf,
				struct xfer_metadata *meta __unused ) {
	int ( * rx ) ( struct nbd_session *nbd );
	size_t req_len, part_len;
	int rc;

	DBGCP ( nbd, "NBD %p deliver %zd bytes\n", nbd, iob_len ( iobuf ) );
	while ( 1 ) {
		/* Discard some data, if applicable */
		if ( nbd->discard_len ) {
			DBGCP ( nbd, "NBD %p discard:\n", nbd );
			if ( nbd->discard_len >= iob_len ( iobuf ) ) {
				DBGCP_HD ( nbd, iobuf->data,
					   iob_len ( iobuf ) );
				nbd->discard_len -= iob_len ( iobuf );
				rc = 0;
				goto done;
			} else {
				DBGCP_HD ( nbd, iobuf->data,
					   nbd->discard_len );
				iob_pull ( iobuf, nbd->discard_len );
				nbd->discard_len = 0;
			}
		}

		switch ( nbd->rx_state ) {
		case NBD_RX_NEG_INIT:
			rx = nbd_rx_neg_init;
			req_len = sizeof ( nbd->rx_neg_init );
			break;

		case NBD_RX_NEG_EXP_NAME:
			rx = nbd_rx_neg_exp_name;
			req_len = sizeof ( nbd->rx_exp_name_reply );
			break;

		case NBD_RX_NEG_OPT_INFO:
			rx = nbd_rx_neg_opt_info;
			req_len = sizeof ( nbd->rx_opt_reply );
			break;

		case NBD_RX_NEG_REP_INFO:
			rx = nbd_rx_neg_rep_info;
			req_len = nbd->reply_info_length;
			break;

		case NBD_RX_TRANS_REP_CMD:
			rx = nbd_rx_trans_rep_cmd;
			req_len = sizeof ( nbd->rx_trans_reply );
			break;

		case NBD_RX_TRANS_DATA:
			rc = nbd_rx_trans_data ( nbd, iobuf );
			goto done;

		default:
			assert ( 0 );
			rc = -EINVAL;
			goto done;
		}

		DBGCP ( nbd, "NBD %p state %d req %zd, off %zd, \n",
			nbd, nbd->rx_state, req_len, nbd->rx_offset );

		part_len = req_len - nbd->rx_offset;
		if ( part_len > iob_len ( iobuf ) )
			part_len = iob_len ( iobuf );
		memcpy ( nbd->rx_buffer + nbd->rx_offset,
			 iobuf->data, part_len );
		nbd->rx_offset += part_len;

		iob_pull ( iobuf, part_len );

		/* If all the data for this state has not yet been
		 * received, stay in this state for now.
		 */
		if ( nbd->rx_offset != req_len ) {
			rc = 0;
			goto done;
		}

		rc = rx ( nbd );
		if ( rc ) {
			DBGC ( nbd, "NBD %p could not process received "
			       "data: %s\n", nbd, strerror ( rc ) );
			goto done;
		}

		nbd->rx_offset = 0;
	}

 done:
	/* Free I/O buffer */
	free_iob ( iobuf );

	/* Destroy session on error */
	if ( rc != 0 )
		nbd_close ( nbd, rc );

	return rc;
}

/**
 * Handle data transfer window change
 *
 * @v nbd		NBD session
 */
static void nbd_socket_window_changed ( struct nbd_session *nbd ) {
	DBGCP ( nbd, "NBD %p socket window changed\n", nbd );
	nbd_tx_resume ( nbd );
}

/** NBD socket interface operations */
static struct interface_operation nbd_socket_op[] = {
	INTF_OP ( xfer_deliver, struct nbd_session *, nbd_socket_deliver ),
	INTF_OP ( xfer_window_changed, struct nbd_session *,
		  nbd_socket_window_changed ),
	INTF_OP ( intf_close, struct nbd_session *, nbd_close ),
};

/** NBD socket interface descriptor */
static struct interface_descriptor nbd_socket_desc =
	INTF_DESC ( struct nbd_session, socket, nbd_socket_op );


/**
 * Open NBD URI
 *
 * @v parent		Parent interface
 * @v uri		URI
 * @ret rc		Return status code
 */
static int nbd_open ( struct interface *parent, struct uri *uri ) {
	struct nbd_session *nbd;
	struct sockaddr_tcpip server;
	int rc;

	/* Sanity check */
	if ( ! uri->host )
		return -EINVAL;

	/* Allocate and initialise structure */
	nbd = zalloc ( sizeof ( *nbd ) );
	if ( ! nbd )
		return -ENOMEM;

	ref_init ( &nbd->refcnt, nbd_free );
	intf_init ( &nbd->block, &nbd_block_desc, &nbd->refcnt );
	intf_init ( &nbd->socket, &nbd_socket_desc, &nbd->refcnt );

	nbd->uri = uri_get ( uri );
	if ( uri->path ) {
		/* Remove first '/' from the path */
		nbd->export_name = uri->path + 1;
	} else {
		/* Use empty name */
		nbd->export_name = "";
	}

	DBGC ( nbd, "NBD %p open %s (%s)\n", nbd, uri->host, nbd->export_name );

	/* Use NBD_OPT_EXPORT_NAME option by default */
	nbd->use_opt_go = 0;

	if ( strstr ( uri->query, "use-opt-go" ) )
		nbd->use_opt_go = 1;

	/* Open connection to server */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( uri, DEFAULT_NBD_PORT ) );
	if ( ( rc = xfer_open_named_socket ( &nbd->socket, SOCK_STREAM,
					     ( struct sockaddr * ) &server,
					     uri->host, NULL ) ) != 0 )
		goto err_connect;

	nbd->rx_state = NBD_RX_NEG_INIT;
	nbd->tx_state = NBD_TX_IDLE;

	process_init_stopped ( &nbd->process, &nbd_process_desc, &nbd->refcnt );

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &nbd->block, parent );
	ref_put ( &nbd->refcnt );
	return 0;

err_connect:
	nbd_close ( nbd, rc );
	ref_put ( &nbd->refcnt );
	return rc;
}

/** NBD URI opener */
struct uri_opener nbd_uri_opener __uri_opener = {
	.scheme = "nbd",
	.open = nbd_open,
};
