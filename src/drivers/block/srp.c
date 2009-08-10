/*
 * Copyright (C) 2009 Fen Systems Ltd <mbrown@fensystems.co.uk>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

FILE_LICENCE ( BSD2 );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gpxe/scsi.h>
#include <gpxe/xfer.h>
#include <gpxe/features.h>
#include <gpxe/ib_srp.h>
#include <gpxe/srp.h>

/**
 * @file
 *
 * SCSI RDMA Protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "SRP", DHCP_EB_FEATURE_SRP, 1 );

/** Tag to be used for next SRP IU */
static unsigned int srp_tag = 0;

static void srp_login ( struct srp_device *srp );
static void srp_cmd ( struct srp_device *srp );

/**
 * Mark SRP SCSI command as complete
 *
 * @v srp		SRP device
 * @v rc		Status code
 */
static void srp_scsi_done ( struct srp_device *srp, int rc ) {
	if ( srp->command )
		srp->command->rc = rc;
	srp->command = NULL;
}

/**
 * Handle SRP session failure
 *
 * @v srp		SRP device
 * @v rc 		Reason for failure
 */
static void srp_fail ( struct srp_device *srp, int rc ) {

	/* Close underlying socket */
	xfer_close ( &srp->socket, rc );

	/* Clear session state */
	srp->state = 0;

	/* If we have reached the retry limit, report the failure */
	if ( srp->retry_count >= SRP_MAX_RETRIES ) {
		srp_scsi_done ( srp, rc );
		return;
	}

	/* Otherwise, increment the retry count and try to reopen the
	 * connection
	 */
	srp->retry_count++;
	srp_login ( srp );
}

/**
 * Initiate SRP login
 *
 * @v srp		SRP device
 */
static void srp_login ( struct srp_device *srp ) {
	struct io_buffer *iobuf;
	struct srp_login_req *login_req;
	int rc;

	assert ( ! ( srp->state & SRP_STATE_SOCKET_OPEN ) );

	/* Open underlying socket */
	if ( ( rc = srp->transport->connect ( srp ) ) != 0 ) {
		DBGC ( srp, "SRP %p could not open socket: %s\n",
		       srp, strerror ( rc ) );
		goto err;
	}
	srp->state |= SRP_STATE_SOCKET_OPEN;

	/* Allocate I/O buffer */
	iobuf = xfer_alloc_iob ( &srp->socket, sizeof ( *login_req ) );
	if ( ! iobuf ) {
		rc = -ENOMEM;
		goto err;
	}

	/* Construct login request IU */
	login_req = iob_put ( iobuf, sizeof ( *login_req ) );
	memset ( login_req, 0, sizeof ( *login_req ) );
	login_req->type = SRP_LOGIN_REQ;
	login_req->tag.dwords[1] = htonl ( ++srp_tag );
	login_req->max_i_t_iu_len = htonl ( SRP_MAX_I_T_IU_LEN );
	login_req->required_buffer_formats = SRP_LOGIN_REQ_FMT_DDBD;
	memcpy ( &login_req->port_ids, &srp->port_ids,
		 sizeof ( login_req->port_ids ) );

	DBGC2 ( srp, "SRP %p TX login request tag %08x%08x\n",
		srp, ntohl ( login_req->tag.dwords[0] ),
		ntohl ( login_req->tag.dwords[1] ) );
	DBGC2_HDA ( srp, 0, iobuf->data, iob_len ( iobuf ) );

	/* Send login request IU */
	if ( ( rc = xfer_deliver_iob ( &srp->socket, iobuf ) ) != 0 ) {
		DBGC ( srp, "SRP %p could not send login request: %s\n",
		       srp, strerror ( rc ) );
		goto err;
	}

	return;

 err:
	srp_fail ( srp, rc );
}

/**
 * Handle SRP login response
 *
 * @v srp		SRP device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int srp_login_rsp ( struct srp_device *srp, struct io_buffer *iobuf ) {
	struct srp_login_rsp *login_rsp = iobuf->data;
	int rc;

	DBGC2 ( srp, "SRP %p RX login response tag %08x%08x\n",
		srp, ntohl ( login_rsp->tag.dwords[0] ),
		ntohl ( login_rsp->tag.dwords[1] ) );

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *login_rsp ) ) {
		DBGC ( srp, "SRP %p RX login response too short (%zd bytes)\n",
		       srp, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto out;
	}

	DBGC ( srp, "SRP %p logged in\n", srp );

	/* Mark as logged in */
	srp->state |= SRP_STATE_LOGGED_IN;

	/* Reset error counter */
	srp->retry_count = 0;

	/* Issue pending command */
	srp_cmd ( srp );

	rc = 0;
 out:
	free_iob ( iobuf );
	return rc;
}

/**
 * Handle SRP login rejection
 *
 * @v srp		SRP device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int srp_login_rej ( struct srp_device *srp, struct io_buffer *iobuf ) {
	struct srp_login_rej *login_rej = iobuf->data;
	int rc;

	DBGC2 ( srp, "SRP %p RX login rejection tag %08x%08x\n",
		srp, ntohl ( login_rej->tag.dwords[0] ),
		ntohl ( login_rej->tag.dwords[1] ) );

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *login_rej ) ) {
		DBGC ( srp, "SRP %p RX login rejection too short (%zd "
		       "bytes)\n", srp, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto out;
	}

	/* Login rejection always indicates an error */
	DBGC ( srp, "SRP %p login rejected (reason %08x)\n",
	       srp, ntohl ( login_rej->reason ) );
	rc = -EPERM;

 out:
	free_iob ( iobuf );
	return rc;
}

/**
 * Transmit SRP SCSI command
 *
 * @v srp		SRP device
 */
static void srp_cmd ( struct srp_device *srp ) {
	struct io_buffer *iobuf;
	struct srp_cmd *cmd;
	struct srp_memory_descriptor *data_out;
	struct srp_memory_descriptor *data_in;
	int rc;

	assert ( srp->state & SRP_STATE_LOGGED_IN );

	/* Allocate I/O buffer */
	iobuf = xfer_alloc_iob ( &srp->socket, SRP_MAX_I_T_IU_LEN );
	if ( ! iobuf ) {
		rc = -ENOMEM;
		goto err;
	}

	/* Construct base portion */
	cmd = iob_put ( iobuf, sizeof ( *cmd ) );
	memset ( cmd, 0, sizeof ( *cmd ) );
	cmd->type = SRP_CMD;
	cmd->tag.dwords[1] = htonl ( ++srp_tag );
	cmd->lun = srp->lun;
	memcpy ( &cmd->cdb, &srp->command->cdb, sizeof ( cmd->cdb ) );

	/* Construct data-out descriptor, if present */
	if ( srp->command->data_out ) {
		cmd->data_buffer_formats |= SRP_CMD_DO_FMT_DIRECT;
		data_out = iob_put ( iobuf, sizeof ( *data_out ) );
		data_out->address =
		    cpu_to_be64 ( user_to_phys ( srp->command->data_out, 0 ) );
		data_out->handle = ntohl ( srp->memory_handle );
		data_out->len = ntohl ( srp->command->data_out_len );
	}

	/* Construct data-in descriptor, if present */
	if ( srp->command->data_in ) {
		cmd->data_buffer_formats |= SRP_CMD_DI_FMT_DIRECT;
		data_in = iob_put ( iobuf, sizeof ( *data_in ) );
		data_in->address =
		     cpu_to_be64 ( user_to_phys ( srp->command->data_in, 0 ) );
		data_in->handle = ntohl ( srp->memory_handle );
		data_in->len = ntohl ( srp->command->data_in_len );
	}

	DBGC2 ( srp, "SRP %p TX SCSI command tag %08x%08x\n", srp,
		ntohl ( cmd->tag.dwords[0] ), ntohl ( cmd->tag.dwords[1] ) );
	DBGC2_HDA ( srp, 0, iobuf->data, iob_len ( iobuf ) );

	/* Send IU */
	if ( ( rc = xfer_deliver_iob ( &srp->socket, iobuf ) ) != 0 ) {
		DBGC ( srp, "SRP %p could not send command: %s\n",
		       srp, strerror ( rc ) );
		goto err;
	}

	return;

 err:
	srp_fail ( srp, rc );
}

/**
 * Handle SRP SCSI response
 *
 * @v srp		SRP device
 * @v iobuf		I/O buffer
 * @ret rc		Returns status code
 */
static int srp_rsp ( struct srp_device *srp, struct io_buffer *iobuf ) {
	struct srp_rsp *rsp = iobuf->data;
	int rc;

	DBGC2 ( srp, "SRP %p RX SCSI response tag %08x%08x\n", srp,
		ntohl ( rsp->tag.dwords[0] ), ntohl ( rsp->tag.dwords[1] ) );

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *rsp ) ) {
		DBGC ( srp, "SRP %p RX SCSI response too short (%zd bytes)\n",
		       srp, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto out;
	}

	/* Report SCSI errors */
	if ( rsp->status != 0 ) {
		DBGC ( srp, "SRP %p response status %02x\n",
		       srp, rsp->status );
		if ( srp_rsp_sense_data ( rsp ) ) {
			DBGC ( srp, "SRP %p sense data:\n", srp );
			DBGC_HDA ( srp, 0, srp_rsp_sense_data ( rsp ),
				   srp_rsp_sense_data_len ( rsp ) );
		}
	}
	if ( rsp->valid & ( SRP_RSP_VALID_DOUNDER | SRP_RSP_VALID_DOOVER ) ) {
		DBGC ( srp, "SRP %p response data-out %srun by %#x bytes\n",
		       srp, ( ( rsp->valid & SRP_RSP_VALID_DOUNDER )
			      ? "under" : "over" ),
		       ntohl ( rsp->data_out_residual_count ) );
	}
	if ( rsp->valid & ( SRP_RSP_VALID_DIUNDER | SRP_RSP_VALID_DIOVER ) ) {
		DBGC ( srp, "SRP %p response data-in %srun by %#x bytes\n",
		       srp, ( ( rsp->valid & SRP_RSP_VALID_DIUNDER )
			      ? "under" : "over" ),
		       ntohl ( rsp->data_in_residual_count ) );
	}
	srp->command->status = rsp->status;

	/* Mark SCSI command as complete */
	srp_scsi_done ( srp, 0 );

	rc = 0;
 out:
	free_iob ( iobuf );
	return rc;
}

/**
 * Handle SRP unrecognised response
 *
 * @v srp		SRP device
 * @v iobuf		I/O buffer
 * @ret rc		Returns status code
 */
static int srp_unrecognised ( struct srp_device *srp,
			      struct io_buffer *iobuf ) {
	struct srp_common *common = iobuf->data;

	DBGC ( srp, "SRP %p RX unrecognised IU tag %08x%08x type %02x\n",
	       srp, ntohl ( common->tag.dwords[0] ),
	       ntohl ( common->tag.dwords[1] ), common->type );

	free_iob ( iobuf );
	return -ENOTSUP;
}

/**
 * Receive data from underlying socket
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int srp_xfer_deliver_iob ( struct xfer_interface *xfer,
				  struct io_buffer *iobuf,
				  struct xfer_metadata *meta __unused ) {
	struct srp_device *srp =
		container_of ( xfer, struct srp_device, socket );
	struct srp_common *common = iobuf->data;
	int ( * type ) ( struct srp_device *srp, struct io_buffer *iobuf );
	int rc;

	/* Determine IU type */
	switch ( common->type ) {
	case SRP_LOGIN_RSP:
		type = srp_login_rsp;
		break;
	case SRP_LOGIN_REJ:
		type = srp_login_rej;
		break;
	case SRP_RSP:
		type = srp_rsp;
		break;
	default:
		type = srp_unrecognised;
		break;
	}

	/* Handle IU */
	if ( ( rc = type ( srp, iobuf ) ) != 0 )
		goto err;

	return 0;

 err:
	srp_fail ( srp, rc );
	return rc;
}

/**
 * Underlying socket closed
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
static void srp_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct srp_device *srp =
		container_of ( xfer, struct srp_device, socket );

	DBGC ( srp, "SRP %p socket closed: %s\n", srp, strerror ( rc ) );

	srp_fail ( srp, rc );
}

/** SRP data transfer interface operations */
static struct xfer_interface_operations srp_xfer_operations = {
	.close		= srp_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= srp_xfer_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/**
 * Issue SCSI command via SRP
 *
 * @v scsi		SCSI device
 * @v command		SCSI command
 * @ret rc		Return status code
 */
static int srp_command ( struct scsi_device *scsi,
			 struct scsi_command *command ) {
	struct srp_device *srp =
		container_of ( scsi->backend, struct srp_device, refcnt );

	/* Store SCSI command */
	if ( srp->command ) {
		DBGC ( srp, "SRP %p cannot handle concurrent SCSI commands\n",
		       srp );
		return -EBUSY;
	}
	srp->command = command;

	/* Log in or issue command as appropriate */
	if ( ! ( srp->state & SRP_STATE_SOCKET_OPEN ) ) {
		srp_login ( srp );
	} else if ( srp->state & SRP_STATE_LOGGED_IN ) {
		srp_cmd ( srp );
	} else {
		/* Still waiting for login; do nothing */
	}

	return 0;
}

/**
 * Attach SRP device
 *
 * @v scsi		SCSI device
 * @v root_path		Root path
 */
int srp_attach ( struct scsi_device *scsi, const char *root_path ) {
	struct srp_transport_type *transport;
	struct srp_device *srp;
	int rc;

	/* Hard-code an IB SRP back-end for now */
	transport = &ib_srp_transport;

	/* Allocate and initialise structure */
	srp = zalloc ( sizeof ( *srp ) + transport->priv_len );
	if ( ! srp ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	xfer_init ( &srp->socket, &srp_xfer_operations, &srp->refcnt );
	srp->transport = transport;
	DBGC ( srp, "SRP %p using %s\n", srp, root_path );

	/* Parse root path */
	if ( ( rc = transport->parse_root_path ( srp, root_path ) ) != 0 ) {
		DBGC ( srp, "SRP %p could not parse root path: %s\n",
		       srp, strerror ( rc ) );
		goto err_parse_root_path;
	}

	/* Attach parent interface, mortalise self, and return */
	scsi->backend = ref_get ( &srp->refcnt );
	scsi->command = srp_command;
	ref_put ( &srp->refcnt );
	return 0;

 err_parse_root_path:
	ref_put ( &srp->refcnt );
 err_alloc:
	return rc;
}

/**
 * Detach SRP device
 *
 * @v scsi		SCSI device
 */
void srp_detach ( struct scsi_device *scsi ) {
	struct srp_device *srp =
		container_of ( scsi->backend, struct srp_device, refcnt );

	/* Close socket */
	xfer_nullify ( &srp->socket );
	xfer_close ( &srp->socket, 0 );
	scsi->command = scsi_detached_command;
	ref_put ( scsi->backend );
	scsi->backend = NULL;
}
