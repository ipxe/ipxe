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
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>
#include <gpxe/infiniband.h>
#include <gpxe/iobuf.h>
#include <gpxe/ib_gma.h>

/**
 * @file
 *
 * Infiniband General Management Agent
 *
 */

/** A MAD request */
struct ib_mad_request {
	/** Associated GMA */
	struct ib_gma *gma;
	/** List of outstanding MAD requests */
	struct list_head list;
	/** Retry timer */
	struct retry_timer timer;
	/** Destination address */
	struct ib_address_vector av;
	/** MAD request */
	union ib_mad mad;
};

/** GMA number of send WQEs
 *
 * This is a policy decision.
 */
#define IB_GMA_NUM_SEND_WQES 4

/** GMA number of receive WQEs
 *
 * This is a policy decision.
 */
#define IB_GMA_NUM_RECV_WQES 2

/** GMA number of completion queue entries
 *
 * This is a policy decision
 */
#define IB_GMA_NUM_CQES 8

/** TID magic signature */
#define IB_GMA_TID_MAGIC ( ( 'g' << 24 ) | ( 'P' << 16 ) | ( 'X' << 8 ) | 'E' )

/** TID to use for next MAD request */
static unsigned int next_request_tid;

/*****************************************************************************
 *
 * Subnet management MAD handlers
 *
 *****************************************************************************
 */

/**
 * Construct directed route response, if necessary
 *
 * @v gma		General management agent
 * @v mad		MAD response without DR fields filled in
 * @ret mad		MAD response with DR fields filled in
 */
static union ib_mad * ib_sma_dr_response ( struct ib_gma *gma,
					   union ib_mad *mad ) {
	struct ib_mad_hdr *hdr = &mad->hdr;
	struct ib_mad_smp *smp = &mad->smp;
	unsigned int hop_pointer;
	unsigned int hop_count;

	/* Set response fields for directed route SMPs */
	if ( hdr->mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE ) {
		hdr->status |= htons ( IB_SMP_STATUS_D_INBOUND );
		hop_pointer = smp->mad_hdr.class_specific.smp.hop_pointer;
		hop_count = smp->mad_hdr.class_specific.smp.hop_count;
		assert ( hop_count == hop_pointer );
		if ( hop_pointer < ( sizeof ( smp->return_path.hops ) /
				     sizeof ( smp->return_path.hops[0] ) ) ) {
			smp->return_path.hops[hop_pointer] = gma->ibdev->port;
		} else {
			DBGC ( gma, "GMA %p invalid hop pointer %d\n",
			       gma, hop_pointer );
			return NULL;
		}
	}

	return mad;
}

/**
 * Get node information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_get_node_info ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_node_info *node_info = &mad->smp.smp_data.node_info;

	memset ( node_info, 0, sizeof ( *node_info ) );
	node_info->base_version = IB_MGMT_BASE_VERSION;
	node_info->class_version = IB_SMP_CLASS_VERSION;
	node_info->node_type = IB_NODE_TYPE_HCA;
	node_info->num_ports = ib_get_hca_info ( ibdev, &node_info->sys_guid );
	memcpy ( &node_info->node_guid, &node_info->sys_guid,
		 sizeof ( node_info->node_guid ) );
	memcpy ( &node_info->port_guid, &ibdev->gid.u.half[1],
		 sizeof ( node_info->port_guid ) );
	node_info->partition_cap = htons ( 1 );
	node_info->local_port_num = ibdev->port;

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	return ib_sma_dr_response ( gma, mad );
}

/**
 * Get node description
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_get_node_desc ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_node_desc *node_desc = &mad->smp.smp_data.node_desc;
	struct ib_gid_half *guid = &ibdev->gid.u.half[1];

	memset ( node_desc, 0, sizeof ( *node_desc ) );
	snprintf ( node_desc->node_string, sizeof ( node_desc->node_string ),
		   "gPXE %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (%s)",
		   guid->bytes[0], guid->bytes[1], guid->bytes[2],
		   guid->bytes[3], guid->bytes[4], guid->bytes[5],
		   guid->bytes[6], guid->bytes[7], ibdev->dev->name );

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	return ib_sma_dr_response ( gma, mad );
}

/**
 * Get GUID information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_get_guid_info ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_guid_info *guid_info = &mad->smp.smp_data.guid_info;

	memset ( guid_info, 0, sizeof ( *guid_info ) );
	memcpy ( guid_info->guid[0], &ibdev->gid.u.half[1],
		 sizeof ( guid_info->guid[0] ) );

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	return ib_sma_dr_response ( gma, mad );
}

/**
 * Get port information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_get_port_info ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_port_info *port_info = &mad->smp.smp_data.port_info;

	memset ( port_info, 0, sizeof ( *port_info ) );
	memcpy ( port_info->gid_prefix, &ibdev->gid.u.half[0],
		 sizeof ( port_info->gid_prefix ) );
	port_info->lid = ntohs ( ibdev->lid );
	port_info->mastersm_lid = ntohs ( ibdev->sm_lid );
	port_info->local_port_num = ibdev->port;
	port_info->link_width_enabled = ibdev->link_width;
	port_info->link_width_supported = ibdev->link_width;
	port_info->link_width_active = ibdev->link_width;
	port_info->link_speed_supported__port_state =
		( ( ibdev->link_speed << 4 ) | ibdev->port_state );
	port_info->port_phys_state__link_down_def_state =
		( ( IB_PORT_PHYS_STATE_POLLING << 4 ) |
		  IB_PORT_PHYS_STATE_POLLING );
	port_info->link_speed_active__link_speed_enabled =
		( ( ibdev->link_speed << 4 ) | ibdev->link_speed );
	port_info->neighbour_mtu__mastersm_sl =
		( ( IB_MTU_2048 << 4 ) | ibdev->sm_sl );
	port_info->vl_cap__init_type = ( IB_VL_0 << 4 );
	port_info->init_type_reply__mtu_cap = IB_MTU_2048;
	port_info->operational_vls__enforcement = ( IB_VL_0 << 4 );
	port_info->guid_cap = 1;

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	return ib_sma_dr_response ( gma, mad );
}

/**
 * Set port information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_set_port_info ( struct ib_gma *gma,
					     union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	const struct ib_port_info *port_info = &mad->smp.smp_data.port_info;
	int rc;

	memcpy ( &ibdev->gid.u.half[0], port_info->gid_prefix,
		 sizeof ( ibdev->gid.u.half[0] ) );
	ibdev->lid = ntohs ( port_info->lid );
	ibdev->sm_lid = ntohs ( port_info->mastersm_lid );
	ibdev->sm_sl = ( port_info->neighbour_mtu__mastersm_sl & 0xf );

	if ( ( rc = ib_set_port_info ( ibdev, port_info ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not set port information: %s\n",
		       ibdev, strerror ( rc ) );
		mad->hdr.status =
			htons ( IB_MGMT_STATUS_UNSUPPORTED_METHOD_ATTR );
	}

	return ib_sma_get_port_info ( gma, mad );
}

/**
 * Get partition key table
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_get_pkey_table ( struct ib_gma *gma,
					      union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_pkey_table *pkey_table = &mad->smp.smp_data.pkey_table;

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	memset ( pkey_table, 0, sizeof ( *pkey_table ) );
	pkey_table->pkey[0] = htons ( ibdev->pkey );

	mad->hdr.method = IB_MGMT_METHOD_GET_RESP;
	return ib_sma_dr_response ( gma, mad );
}

/**
 * Set partition key table
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret response	MAD response
 */
static union ib_mad * ib_sma_set_pkey_table ( struct ib_gma *gma,
					      union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_pkey_table *pkey_table = &mad->smp.smp_data.pkey_table;

	ibdev->pkey = ntohs ( pkey_table->pkey[0] );

	return ib_sma_get_pkey_table ( gma, mad );
}

/** List of attribute handlers */
struct ib_gma_handler ib_sma_handlers[] __ib_gma_handler = {
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.attr_id = htons ( IB_SMP_ATTR_NODE_INFO ),
		.handle = ib_sma_get_node_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.attr_id = htons ( IB_SMP_ATTR_NODE_DESC ),
		.handle = ib_sma_get_node_desc,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.attr_id = htons ( IB_SMP_ATTR_GUID_INFO ),
		.handle = ib_sma_get_guid_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.attr_id = htons ( IB_SMP_ATTR_PORT_INFO ),
		.handle = ib_sma_get_port_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SET,
		.attr_id = htons ( IB_SMP_ATTR_PORT_INFO ),
		.handle = ib_sma_set_port_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE ),
		.handle = ib_sma_get_pkey_table,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SET,
		.attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE ),
		.handle = ib_sma_set_pkey_table,
	},
};

/*****************************************************************************
 *
 * General management agent
 *
 *****************************************************************************
 */

/**
 * Call attribute handler
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret mad		MAD response
 */
static union ib_mad * ib_handle_mad ( struct ib_gma *gma, union ib_mad *mad ) {
	struct ib_mad_hdr *hdr = &mad->hdr;
	struct ib_gma_handler *handler;

	for_each_table_entry ( handler, IB_GMA_HANDLERS ) {
		if ( ( ( handler->mgmt_class & ~handler->mgmt_class_ignore ) ==
		       ( hdr->mgmt_class & ~handler->mgmt_class_ignore ) ) &&
		     ( handler->class_version == hdr->class_version ) &&
		     ( handler->method == hdr->method ) &&
		     ( handler->attr_id == hdr->attr_id ) ) {
			return handler->handle ( gma, mad );
		}
	}

	hdr->method = IB_MGMT_METHOD_TRAP;
	hdr->status = htons ( IB_MGMT_STATUS_UNSUPPORTED_METHOD_ATTR );
	return mad;
}

/**
 * Complete GMA receive
 *
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ib_gma_complete_recv ( struct ib_device *ibdev,
				   struct ib_queue_pair *qp,
				   struct ib_address_vector *av,
				   struct io_buffer *iobuf, int rc ) {
	struct ib_gma *gma = ib_qp_get_ownerdata ( qp );
	struct ib_mad_request *request;
	union ib_mad *mad;
	struct ib_mad_hdr *hdr;
	union ib_mad *response;

	/* Ignore errors */
	if ( rc != 0 ) {
		DBGC ( gma, "GMA %p RX error: %s\n", gma, strerror ( rc ) );
		goto out;
	}

	/* Sanity checks */
	if ( iob_len ( iobuf ) != sizeof ( *mad ) ) {
		DBGC ( gma, "GMA %p RX bad size (%zd bytes)\n",
		       gma, iob_len ( iobuf ) );
		DBGC_HDA ( gma, 0, iobuf->data, iob_len ( iobuf ) );
		goto out;
	}
	mad = iobuf->data;
	hdr = &mad->hdr;
	if ( hdr->base_version != IB_MGMT_BASE_VERSION ) {
		DBGC ( gma, "GMA %p unsupported base version %x\n",
		       gma, hdr->base_version );
		DBGC_HDA ( gma, 0, mad, sizeof ( *mad ) );
		goto out;
	}
	DBGC ( gma, "GMA %p RX TID %08x%08x (%02x,%02x,%02x,%04x) status "
	       "%04x\n", gma, ntohl ( hdr->tid[0] ), ntohl ( hdr->tid[1] ),
	       hdr->mgmt_class, hdr->class_version, hdr->method,
	       ntohs ( hdr->attr_id ), ntohs ( hdr->status ) );
	DBGC2_HDA ( gma, 0, mad, sizeof ( *mad ) );

	/* Dequeue request if applicable */
	list_for_each_entry ( request, &gma->requests, list ) {
		if ( memcmp ( &request->mad.hdr.tid, &hdr->tid,
			      sizeof ( request->mad.hdr.tid ) ) == 0 ) {
			stop_timer ( &request->timer );
			list_del ( &request->list );
			free ( request );
			break;
		}
	}

	/* Handle MAD */
	if ( ( response = ib_handle_mad ( gma, mad ) ) == NULL )
		goto out;

	/* Re-use I/O buffer for response */
	memcpy ( mad, response, sizeof ( *mad ) );
	DBGC ( gma, "GMA %p TX TID %08x%08x (%02x,%02x,%02x,%04x) status "
	       "%04x\n", gma, ntohl ( hdr->tid[0] ), ntohl ( hdr->tid[1] ),
	       hdr->mgmt_class, hdr->class_version, hdr->method,
	       ntohs ( hdr->attr_id ), ntohs ( hdr->status ) );
	DBGC2_HDA ( gma, 0, mad, sizeof ( *mad ) );

	/* Send MAD response, if applicable */
	if ( ( rc = ib_post_send ( ibdev, qp, av,
				   iob_disown ( iobuf ) ) ) != 0 ) {
		DBGC ( gma, "GMA %p could not send MAD response: %s\n",
		       gma, strerror ( rc ) );
		goto out;
	}

 out:
	free_iob ( iobuf );
}

/** GMA completion operations */
static struct ib_completion_queue_operations ib_gma_completion_ops = {
	.complete_recv = ib_gma_complete_recv,
};

/**
 * Transmit MAD request
 *
 * @v gma		General management agent
 * @v request		MAD request
 * @ret rc		Return status code
 */
static int ib_gma_send ( struct ib_gma *gma, struct ib_mad_request *request ) {
	struct io_buffer *iobuf;
	int rc;

	DBGC ( gma, "GMA %p TX TID %08x%08x (%02x,%02x,%02x,%04x)\n",
	       gma, ntohl ( request->mad.hdr.tid[0] ),
	       ntohl ( request->mad.hdr.tid[1] ), request->mad.hdr.mgmt_class,
	       request->mad.hdr.class_version, request->mad.hdr.method,
	       ntohs ( request->mad.hdr.attr_id ) );
	DBGC2_HDA ( gma, 0, &request->mad, sizeof ( request->mad ) );

	/* Construct I/O buffer */
	iobuf = alloc_iob ( sizeof ( request->mad ) );
	if ( ! iobuf ) {
		DBGC ( gma, "GMA %p could not allocate buffer for TID "
		       "%08x%08x\n", gma, ntohl ( request->mad.hdr.tid[0] ),
		       ntohl ( request->mad.hdr.tid[1] ) );
		return -ENOMEM;
	}
	memcpy ( iob_put ( iobuf, sizeof ( request->mad ) ), &request->mad,
		 sizeof ( request->mad ) );

	/* Send I/O buffer */
	if ( ( rc = ib_post_send ( gma->ibdev, gma->qp, &request->av,
				   iobuf ) ) != 0 ) {
		DBGC ( gma, "GMA %p could not send TID %08x%08x: %s\n",
		       gma,  ntohl ( request->mad.hdr.tid[0] ),
		       ntohl ( request->mad.hdr.tid[1] ), strerror ( rc ) );
		free_iob ( iobuf );
		return rc;
	}

	return 0;
}

/**
 * Handle MAD request timer expiry
 *
 * @v timer		Retry timer
 * @v expired		Failure indicator
 */
static void ib_gma_timer_expired ( struct retry_timer *timer, int expired ) {
	struct ib_mad_request *request =
		container_of ( timer, struct ib_mad_request, timer );
	struct ib_gma *gma = request->gma;

	/* Abandon TID if we have tried too many times */
	if ( expired ) {
		DBGC ( gma, "GMA %p abandoning TID %08x%08x\n",
		       gma, ntohl ( request->mad.hdr.tid[0] ),
		       ntohl ( request->mad.hdr.tid[1] ) );
		list_del ( &request->list );
		free ( request );
		return;
	}

	/* Restart retransmission timer */
	start_timer ( timer );

	/* Resend request */
	ib_gma_send ( gma, request );
}

/**
 * Issue MAD request
 *
 * @v gma		General management agent
 * @v mad		MAD request
 * @v av		Destination address, or NULL for SM
 * @v retry		Request should be retried until a response arrives
 * @ret rc		Return status code
 */
int ib_gma_request ( struct ib_gma *gma, union ib_mad *mad,
		     struct ib_address_vector *av, int retry ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_mad_request *request;

	/* Allocate and initialise structure */
	request = zalloc ( sizeof ( *request ) );
	if ( ! request ) {
		DBGC ( gma, "GMA %p could not allocate MAD request\n", gma );
		return -ENOMEM;
	}
	request->gma = gma;
	request->timer.expired = ib_gma_timer_expired;

	/* Determine address vector */
	if ( av ) {
		memcpy ( &request->av, av, sizeof ( request->av ) );
	} else {
		request->av.lid = ibdev->sm_lid;
		request->av.sl = ibdev->sm_sl;
		request->av.qpn = IB_QPN_GMA;
		request->av.qkey = IB_QKEY_GMA;
	}

	/* Copy MAD body */
	memcpy ( &request->mad, mad, sizeof ( request->mad ) );

	/* Allocate TID */
	request->mad.hdr.tid[0] = htonl ( IB_GMA_TID_MAGIC );
	request->mad.hdr.tid[1] = htonl ( ++next_request_tid );

	/* Send initial request.  Ignore errors; the retry timer will
	 * take care of those we care about.
	 */
	ib_gma_send ( gma, request );

	/* Add to list and start timer if applicable */
	if ( retry ) {
		list_add ( &request->list, &gma->requests );
		start_timer ( &request->timer );
	} else {
		free ( request );
	}

	return 0;
}

/**
 * Create GMA
 *
 * @v ibdev		Infiniband device
 * @v type		Queue pair type
 * @ret gma		General management agent, or NULL
 */
struct ib_gma * ib_create_gma ( struct ib_device *ibdev,
				enum ib_queue_pair_type type ) {
	struct ib_gma *gma;
	unsigned long qkey;

	/* Allocate and initialise fields */
	gma = zalloc ( sizeof ( *gma ) );
	if ( ! gma )
		goto err_alloc;
	gma->ibdev = ibdev;
	INIT_LIST_HEAD ( &gma->requests );

	/* Create completion queue */
	gma->cq = ib_create_cq ( ibdev, IB_GMA_NUM_CQES,
				 &ib_gma_completion_ops );
	if ( ! gma->cq ) {
		DBGC ( gma, "GMA %p could not allocate completion queue\n",
		       gma );
		goto err_create_cq;
	}

	/* Create queue pair */
	qkey = ( ( type == IB_QPT_SMA ) ? IB_QKEY_SMA : IB_QKEY_GMA );
	gma->qp = ib_create_qp ( ibdev, type, IB_GMA_NUM_SEND_WQES, gma->cq,
				 IB_GMA_NUM_RECV_WQES, gma->cq, qkey );
	if ( ! gma->qp ) {
		DBGC ( gma, "GMA %p could not allocate queue pair\n", gma );
		goto err_create_qp;
	}
	ib_qp_set_ownerdata ( gma->qp, gma );

	DBGC ( gma, "GMA %p running on QPN %#lx\n", gma, gma->qp->qpn );

	/* Fill receive ring */
	ib_refill_recv ( ibdev, gma->qp );
	return gma;

	ib_destroy_qp ( ibdev, gma->qp );
 err_create_qp:
	ib_destroy_cq ( ibdev, gma->cq );
 err_create_cq:
	free ( gma );
 err_alloc:
	return NULL;
}

/**
 * Destroy GMA
 *
 * @v gma		General management agent
 */
void ib_destroy_gma ( struct ib_gma *gma ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_mad_request *request;
	struct ib_mad_request *tmp;

	/* Flush any outstanding requests */
	list_for_each_entry_safe ( request, tmp, &gma->requests, list ) {
		stop_timer ( &request->timer );
		list_del ( &request->list );
		free ( request );
	}

	ib_destroy_qp ( ibdev, gma->qp );
	ib_destroy_cq ( ibdev, gma->cq );
	free ( gma );
}
