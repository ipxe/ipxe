/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/list.h>
#include <gpxe/process.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_gma.h>
#include <gpxe/ib_pathrec.h>
#include <gpxe/ib_cm.h>

/**
 * @file
 *
 * Infiniband communication management
 *
 */

/** An outstanding connection request */
struct ib_cm_request {
	/** List of all outstanding requests */
	struct list_head list;
	/** Local communication ID */
	uint32_t local_id;
	/** Remote communication ID */
	uint32_t remote_id;
	/** Queue pair */
	struct ib_queue_pair *qp;
	/** Target service ID */
	struct ib_gid_half service_id;
	/** Connection process */
	struct process process;
	/** Notification handler
	 *
	 * @v qp		Queue pair
	 * @v rc		Connection status code
	 * @v private_data	Private data
	 * @v private_data_len	Length of private data
	 */
	void ( * notify ) ( struct ib_queue_pair *qp, int rc,
			    void *private_data, size_t private_data_len );
	/** Private data length */
	size_t private_data_len;
	/** Private data */
	uint8_t private_data[0];
};

/** List of all outstanding connection requests */
static LIST_HEAD ( ib_cm_requests );

/**
 * Send connection request
 *
 * @v request		Connection request
 * @ret rc		Return status code
 */
static int ib_cm_send_request ( struct ib_cm_request *request ) {
	struct ib_queue_pair *qp = request->qp;
	struct ib_device *ibdev = qp->ibdev;
	struct ib_gma *gma = ibdev->gma;
	union ib_mad mad;
	struct ib_mad_cm *cm = &mad.cm;
	struct ib_cm_connect_request *connect_req =
		&cm->cm_data.connect_request;
	size_t private_data_len;
	int rc;

	/* Construct connection request */
	memset ( cm, 0, sizeof ( *cm ) );
	cm->mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	cm->mad_hdr.mgmt_class = IB_MGMT_CLASS_CM;
	cm->mad_hdr.class_version = IB_CM_CLASS_VERSION;
	cm->mad_hdr.method = IB_MGMT_METHOD_SEND;
	cm->mad_hdr.attr_id = htons ( IB_CM_ATTR_CONNECT_REQUEST );
	connect_req->local_id = htonl ( request->local_id );
	memcpy ( &connect_req->service_id, &request->service_id,
		 sizeof ( connect_req->service_id ) );
	ib_get_hca_info ( ibdev, &connect_req->local_ca );
	connect_req->local_qpn__responder_resources =
		htonl ( ( qp->qpn << 8 ) | 1 );
	connect_req->local_eecn__initiator_depth = htonl ( ( 0 << 8 ) | 1 );
	connect_req->remote_eecn__remote_timeout__service_type__ee_flow_ctrl =
		htonl ( ( 0x14 << 3 ) | ( IB_CM_TRANSPORT_RC << 1 ) |
			( 0 << 0 ) );
	connect_req->starting_psn__local_timeout__retry_count =
		htonl ( ( qp->recv.psn << 8 ) | ( 0x14 << 3 ) |
			( 0x07 << 0 ) );
	connect_req->pkey = htons ( ibdev->pkey );
	connect_req->payload_mtu__rdc_exists__rnr_retry =
		( ( IB_MTU_2048 << 4 ) | ( 1 << 3 ) | ( 0x07 << 0 ) );
	connect_req->max_cm_retries__srq =
		( ( 0x0f << 4 ) | ( 0 << 3 ) );
	connect_req->primary.local_lid = htons ( ibdev->lid );
	connect_req->primary.remote_lid = htons ( request->qp->av.lid );
	memcpy ( &connect_req->primary.local_gid, &ibdev->gid,
		 sizeof ( connect_req->primary.local_gid ) );
	memcpy ( &connect_req->primary.remote_gid, &request->qp->av.gid,
		 sizeof ( connect_req->primary.remote_gid ) );
	connect_req->primary.flow_label__rate =
		htonl ( ( 0 << 12 ) | ( request->qp->av.rate << 0 ) );
	connect_req->primary.hop_limit = 0;
	connect_req->primary.sl__subnet_local =
		( ( request->qp->av.sl << 4 ) | ( 1 << 3 ) );
	connect_req->primary.local_ack_timeout = ( 0x13 << 3 );
	private_data_len = request->private_data_len;
	if ( private_data_len > sizeof ( connect_req->private_data ) )
		private_data_len = sizeof ( connect_req->private_data );
	memcpy ( &connect_req->private_data, &request->private_data,
		 private_data_len );

	/* Send request */
	if ( ( rc = ib_gma_request ( gma, &mad, NULL, 1 ) ) != 0 ) {
		DBGC ( gma, "GMA %p could not send connection request: %s\n",
		       gma, strerror ( rc ) );
		return rc;
	}

	return 0;

}

/**
 * Connection request process step
 *
 * @v process		Connection request process
 */
static void ib_cm_step ( struct process *process ) {
	struct ib_cm_request *request =
		container_of ( process, struct ib_cm_request, process );
	struct ib_queue_pair *qp = request->qp;
	struct ib_device *ibdev = qp->ibdev;
	int rc;

	/* Wait until path can be resolved */
	if ( ( rc = ib_resolve_path ( ibdev, &request->qp->av ) ) != 0 )
		return;

	/* Wait until request can be sent */
	if ( ( rc = ib_cm_send_request ( request ) ) != 0 )
		return;

	/* Stop process */
	process_del ( process );
}

/**
 * Identify connection request by communication ID
 *
 * @v local_id		Local communication ID
 * @v remote_id		Remote communication ID
 * @ret request		Connection request, or NULL
 */
static struct ib_cm_request * ib_cm_find_request ( uint32_t local_id,
						   uint32_t remote_id ) {
	struct ib_cm_request *request;

	list_for_each_entry ( request, &ib_cm_requests, list ) {
		if ( request->local_id == local_id ) {
			request->remote_id = remote_id;
			return request;
		}
	}
	return NULL;
}

/**
 * Handle connection reply
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_cm_connect_reply ( struct ib_gma *gma,
					    union ib_mad *mad ) {
	struct ib_cm_connect_reply *connect_rep =
		&mad->cm.cm_data.connect_reply;
	struct ib_cm_ready_to_use *ready =
		&mad->cm.cm_data.ready_to_use;
	struct ib_cm_request *request;
	int rc;

	/* Identify request */
	request = ib_cm_find_request ( ntohl ( connect_rep->remote_id ),
				       ntohl ( connect_rep->local_id ) );
	if ( ! request ) {
		DBGC ( gma, "GMA %p received connection reply with unknown "
		       "ID %08x\n", gma, ntohl ( connect_rep->remote_id ) );
		return NULL;
	}

	/* Extract fields */
	request->qp->av.qpn = ( ntohl ( connect_rep->local_qpn ) >> 8 );
	request->qp->send.psn = ( ntohl ( connect_rep->starting_psn ) >> 8 );
	DBGC ( gma, "GMA %p QPN %lx connected to QPN %lx PSN %x\n", gma,
	       request->qp->qpn, request->qp->av.qpn, request->qp->send.psn );

	/* Modify queue pair */
	if ( ( rc = ib_modify_qp ( request->qp->ibdev, request->qp ) ) != 0 ) {
		DBGC ( gma, "GMA %p QPN %lx could not modify queue pair: %s\n",
		       gma, request->qp->qpn, strerror ( rc ) );
		return NULL;
	}

	/* Inform recipient that we are now connected */
	request->notify ( request->qp, 0, &connect_rep->private_data,
			  sizeof ( connect_rep->private_data ) );

	/* Construct ready to use reply */
	mad->hdr.attr_id = htons ( IB_CM_ATTR_READY_TO_USE );
	memset ( ready, 0, sizeof ( *ready ) );
	ready->local_id = htonl ( request->local_id );
	ready->remote_id = htonl ( request->remote_id );

	return mad;
}

/**
 * Handle connection rejection
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_cm_connect_reject ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_cm_connect_reject *connect_rej =
		&mad->cm.cm_data.connect_reject;
	struct ib_cm_request *request;
	uint16_t reason;

	/* Identify request */
	request = ib_cm_find_request ( ntohl ( connect_rej->remote_id ),
				       ntohl ( connect_rej->local_id ) );
	if ( ! request ) {
		DBGC ( gma, "GMA %p received connection rejection with "
		       "unknown ID %08x\n", gma,
		       ntohl ( connect_rej->remote_id ) );
		return NULL;
	}

	/* Extract fields */
	reason = ntohs ( connect_rej->reason );
	DBGC ( gma, "GMA %p QPN %lx connection rejected (reason %d)\n",
	       gma, request->qp->qpn, reason );

	/* Inform recipient that we are now disconnected */
	request->notify ( request->qp, -ENOTCONN, &connect_rej->private_data,
			  sizeof ( connect_rej->private_data ) );

	return NULL;
}

/** Communication management MAD handlers */
struct ib_gma_handler ib_cm_handlers[] __ib_gma_handler = {
	{
		.mgmt_class = IB_MGMT_CLASS_CM,
		.class_version = IB_CM_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SEND,
		.attr_id = htons ( IB_CM_ATTR_CONNECT_REPLY ),
		.handle = ib_cm_connect_reply,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_CM,
		.class_version = IB_CM_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SEND,
		.attr_id = htons ( IB_CM_ATTR_CONNECT_REJECT ),
		.handle = ib_cm_connect_reject,
	},
};

/**
 * Connect to remote QP
 *
 * @v qp		Queue pair
 * @v dgid		Target GID
 * @v service_id	Target service ID
 * @v private_data	Private data
 * @v private_data_len	Length of private data
 * @ret rc		Return status code
 */
int ib_cm_connect ( struct ib_queue_pair *qp, struct ib_gid *dgid,
		    struct ib_gid_half *service_id,
		    void *private_data, size_t private_data_len,
		    void ( * notify ) ( struct ib_queue_pair *qp, int rc,
					void *private_data,
					size_t private_data_len ) ) {
	struct ib_cm_request *request;

	/* Allocate and initialise request */
	request = zalloc ( sizeof ( *request ) + private_data_len );
	if ( ! request )
		return -ENOMEM;
	list_add ( &request->list, &ib_cm_requests );
	request->local_id = random();
	request->qp = qp;
	memset ( &qp->av, 0, sizeof ( qp->av ) );
	qp->av.gid_present = 1;
	memcpy ( &qp->av.gid, dgid, sizeof ( qp->av.gid ) );
	memcpy ( &request->service_id, service_id,
		 sizeof ( request->service_id ) );
	request->notify = notify;
	request->private_data_len = private_data_len;
	memcpy ( &request->private_data, private_data, private_data_len );
	process_init ( &request->process, ib_cm_step, NULL );

	return 0;
}
