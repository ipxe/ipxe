/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <unistd.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/list.h>
#include <gpxe/if_arp.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/ipoib.h>
#include <gpxe/infiniband.h>

/** @file
 *
 * Infiniband protocol
 *
 */

/**
 * Create completion queue
 *
 * @v ibdev		Infiniband device
 * @v num_cqes		Number of completion queue entries
 * @ret cq		New completion queue
 */
struct ib_completion_queue * ib_create_cq ( struct ib_device *ibdev,
					    unsigned int num_cqes ) {
	struct ib_completion_queue *cq;
	int rc;

	DBGC ( ibdev, "IBDEV %p creating completion queue\n", ibdev );

	/* Allocate and initialise data structure */
	cq = zalloc ( sizeof ( *cq ) );
	if ( ! cq )
		return NULL;
	cq->num_cqes = num_cqes;
	INIT_LIST_HEAD ( &cq->work_queues );

	/* Perform device-specific initialisation and get CQN */
	if ( ( rc = ibdev->op->create_cq ( ibdev, cq ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not initialise completion "
		       "queue: %s\n", ibdev, strerror ( rc ) );
		free ( cq );
		return NULL;
	}

	DBGC ( ibdev, "IBDEV %p created %d-entry completion queue %p (%p) "
	       "with CQN %#lx\n", ibdev, num_cqes, cq,
	       ib_cq_get_drvdata ( cq ), cq->cqn );
	return cq;
}

/**
 * Destroy completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
void ib_destroy_cq ( struct ib_device *ibdev,
		     struct ib_completion_queue *cq ) {
	DBGC ( ibdev, "IBDEV %p destroying completion queue %#lx\n",
	       ibdev, cq->cqn );
	assert ( list_empty ( &cq->work_queues ) );
	ibdev->op->destroy_cq ( ibdev, cq );
	free ( cq );
}

/**
 * Create queue pair
 *
 * @v ibdev		Infiniband device
 * @v num_send_wqes	Number of send work queue entries
 * @v send_cq		Send completion queue
 * @v num_recv_wqes	Number of receive work queue entries
 * @v recv_cq		Receive completion queue
 * @v qkey		Queue key
 * @ret qp		Queue pair
 */
struct ib_queue_pair * ib_create_qp ( struct ib_device *ibdev,
				      unsigned int num_send_wqes,
				      struct ib_completion_queue *send_cq,
				      unsigned int num_recv_wqes,
				      struct ib_completion_queue *recv_cq,
				      unsigned long qkey ) {
	struct ib_queue_pair *qp;
	size_t total_size;
	int rc;

	DBGC ( ibdev, "IBDEV %p creating queue pair\n", ibdev );

	/* Allocate and initialise data structure */
	total_size = ( sizeof ( *qp ) +
		       ( num_send_wqes * sizeof ( qp->send.iobufs[0] ) ) +
		       ( num_recv_wqes * sizeof ( qp->recv.iobufs[0] ) ) );
	qp = zalloc ( total_size );
	if ( ! qp )
		return NULL;
	qp->qkey = qkey;
	qp->send.qp = qp;
	qp->send.is_send = 1;
	qp->send.cq = send_cq;
	list_add ( &qp->send.list, &send_cq->work_queues );
	qp->send.num_wqes = num_send_wqes;
	qp->send.iobufs = ( ( ( void * ) qp ) + sizeof ( *qp ) );
	qp->recv.qp = qp;
	qp->recv.cq = recv_cq;
	list_add ( &qp->recv.list, &recv_cq->work_queues );
	qp->recv.num_wqes = num_recv_wqes;
	qp->recv.iobufs = ( ( ( void * ) qp ) + sizeof ( *qp ) +
			    ( num_send_wqes * sizeof ( qp->send.iobufs[0] ) ));

	/* Perform device-specific initialisation and get QPN */
	if ( ( rc = ibdev->op->create_qp ( ibdev, qp ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not initialise queue pair: "
		       "%s\n", ibdev, strerror ( rc ) );
		list_del ( &qp->send.list );
		list_del ( &qp->recv.list );
		free ( qp );
		return NULL;
	}

	DBGC ( ibdev, "IBDEV %p created queue pair %p (%p) with QPN %#lx\n",
	       ibdev, qp, ib_qp_get_drvdata ( qp ), qp->qpn );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d send entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_send_wqes, qp->send.iobufs,
	       qp->recv.iobufs );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d receive entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_recv_wqes, qp->recv.iobufs,
	       ( ( ( void * ) qp ) + total_size ) );
	return qp;
}

/**
 * Modify queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v mod_list		Modification list
 * @v qkey		New queue key, if applicable
 * @ret rc		Return status code
 */
int ib_modify_qp ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		   unsigned long mod_list, unsigned long qkey ) {
	int rc;

	DBGC ( ibdev, "IBDEV %p modifying QPN %#lx\n", ibdev, qp->qpn );

	if ( mod_list & IB_MODIFY_QKEY )
		qp->qkey = qkey;

	if ( ( rc = ibdev->op->modify_qp ( ibdev, qp, mod_list ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not modify QPN %#lx: %s\n",
		       ibdev, qp->qpn, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Destroy queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 */
void ib_destroy_qp ( struct ib_device *ibdev, struct ib_queue_pair *qp ) {
	DBGC ( ibdev, "IBDEV %p destroying QPN %#lx\n",
	       ibdev, qp->qpn );
	ibdev->op->destroy_qp ( ibdev, qp );
	list_del ( &qp->send.list );
	list_del ( &qp->recv.list );
	free ( qp );
}

/**
 * Find work queue belonging to completion queue
 *
 * @v cq		Completion queue
 * @v qpn		Queue pair number
 * @v is_send		Find send work queue (rather than receive)
 * @ret wq		Work queue, or NULL if not found
 */
struct ib_work_queue * ib_find_wq ( struct ib_completion_queue *cq,
				    unsigned long qpn, int is_send ) {
	struct ib_work_queue *wq;

	list_for_each_entry ( wq, &cq->work_queues, list ) {
		if ( ( wq->qp->qpn == qpn ) && ( wq->is_send == is_send ) )
			return wq;
	}
	return NULL;
}

/***************************************************************************
 *
 * Management datagram operations
 *
 ***************************************************************************
 */

/**
 * Get port information
 *
 * @v ibdev		Infiniband device
 * @v port_info		Port information datagram to fill in
 * @ret rc		Return status code
 */
static int ib_get_port_info ( struct ib_device *ibdev,
			      struct ib_mad_port_info *port_info ) {
	struct ib_mad_hdr *hdr = &port_info->mad_hdr;
	int rc;

	/* Construct MAD */
	memset ( port_info, 0, sizeof ( *port_info ) );
	hdr->base_version = IB_MGMT_BASE_VERSION;
	hdr->mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	hdr->class_version = 1;
	hdr->method = IB_MGMT_METHOD_GET;
	hdr->attr_id = htons ( IB_SMP_ATTR_PORT_INFO );
	hdr->attr_mod = htonl ( ibdev->port );

	if ( ( rc = ib_mad ( ibdev, hdr, sizeof ( *port_info ) ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get port info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get GUID information
 *
 * @v ibdev		Infiniband device
 * @v guid_info		GUID information datagram to fill in
 * @ret rc		Return status code
 */
static int ib_get_guid_info ( struct ib_device *ibdev,
			      struct ib_mad_guid_info *guid_info ) {
	struct ib_mad_hdr *hdr = &guid_info->mad_hdr;
	int rc;

	/* Construct MAD */
	memset ( guid_info, 0, sizeof ( *guid_info ) );
	hdr->base_version = IB_MGMT_BASE_VERSION;
	hdr->mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	hdr->class_version = 1;
	hdr->method = IB_MGMT_METHOD_GET;
	hdr->attr_id = htons ( IB_SMP_ATTR_GUID_INFO );

	if ( ( rc = ib_mad ( ibdev, hdr, sizeof ( *guid_info ) ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get GUID info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get partition key table
 *
 * @v ibdev		Infiniband device
 * @v guid_info		Partition key table datagram to fill in
 * @ret rc		Return status code
 */
static int ib_get_pkey_table ( struct ib_device *ibdev,
			       struct ib_mad_pkey_table *pkey_table ) {
	struct ib_mad_hdr *hdr = &pkey_table->mad_hdr;
	int rc;

	/* Construct MAD */
	memset ( pkey_table, 0, sizeof ( *pkey_table ) );
	hdr->base_version = IB_MGMT_BASE_VERSION;
	hdr->mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	hdr->class_version = 1;
	hdr->method = IB_MGMT_METHOD_GET;
	hdr->attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE );

	if ( ( rc = ib_mad ( ibdev, hdr, sizeof ( *pkey_table ) ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get pkey table: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get MAD parameters
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
static int ib_get_mad_params ( struct ib_device *ibdev ) {
	union {
		/* This union exists just to save stack space */
		struct ib_mad_port_info port_info;
		struct ib_mad_guid_info guid_info;
		struct ib_mad_pkey_table pkey_table;
	} u;
	int rc;

	/* Port info gives us the link state, the first half of the
	 * port GID and the SM LID.
	 */
	if ( ( rc = ib_get_port_info ( ibdev, &u.port_info ) ) != 0 )
		return rc;
	ibdev->link_up = ( ( u.port_info.port_state__link_speed_supported
			     & 0xf ) == 4 );
	memcpy ( &ibdev->port_gid.u.bytes[0], u.port_info.gid_prefix, 8 );
	ibdev->sm_lid = ntohs ( u.port_info.mastersm_lid );

	/* GUID info gives us the second half of the port GID */
	if ( ( rc = ib_get_guid_info ( ibdev, &u.guid_info ) ) != 0 )
		return rc;
	memcpy ( &ibdev->port_gid.u.bytes[8], u.guid_info.gid_local, 8 );

	/* Get partition key */
	if ( ( rc = ib_get_pkey_table ( ibdev, &u.pkey_table ) ) != 0 )
		return rc;
	ibdev->pkey = ntohs ( u.pkey_table.pkey[0][0] );

	DBGC ( ibdev, "IBDEV %p port GID is %08lx:%08lx:%08lx:%08lx\n",
	       ibdev, htonl ( ibdev->port_gid.u.dwords[0] ),
	       htonl ( ibdev->port_gid.u.dwords[1] ),
	       htonl ( ibdev->port_gid.u.dwords[2] ),
	       htonl ( ibdev->port_gid.u.dwords[3] ) );

	return 0;
}

/***************************************************************************
 *
 * Infiniband device creation/destruction
 *
 ***************************************************************************
 */

/**
 * Allocate Infiniband device
 *
 * @v priv_size		Size of driver private data area
 * @ret ibdev		Infiniband device, or NULL
 */
struct ib_device * alloc_ibdev ( size_t priv_size ) {
	struct ib_device *ibdev;
	void *drv_priv;
	size_t total_len;

	total_len = ( sizeof ( *ibdev ) + priv_size );
	ibdev = zalloc ( total_len );
	if ( ibdev ) {
		drv_priv = ( ( ( void * ) ibdev ) + sizeof ( *ibdev ) );
		ib_set_drvdata ( ibdev, drv_priv );
	}
	return ibdev;
}

/**
 * Register Infiniband device
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
int register_ibdev ( struct ib_device *ibdev ) {
	int rc;

	/* Open link */
	if ( ( rc = ib_open ( ibdev ) ) != 0 )
		goto err_open;

	/* Get MAD parameters */
	if ( ( rc = ib_get_mad_params ( ibdev ) ) != 0 )
		goto err_get_mad_params;

	/* Add IPoIB device */
	if ( ( rc = ipoib_probe ( ibdev ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not add IPoIB device: %s\n",
		       ibdev, strerror ( rc ) );
		goto err_ipoib_probe;
	}

	return 0;

 err_ipoib_probe:
 err_get_mad_params:
	ib_close ( ibdev );
 err_open:
	return rc;
}

/**
 * Unregister Infiniband device
 *
 * @v ibdev		Infiniband device
 */
void unregister_ibdev ( struct ib_device *ibdev ) {
	ipoib_remove ( ibdev );
	ib_close ( ibdev );
}

/**
 * Free Infiniband device
 *
 * @v ibdev		Infiniband device
 */
void free_ibdev ( struct ib_device *ibdev ) {
	free ( ibdev );
}

/**
 * Handle Infiniband link state change
 *
 * @v ibdev		Infiniband device
 */
void ib_link_state_changed ( struct ib_device *ibdev ) {
	int rc;

	/* Update MAD parameters */
	if ( ( rc = ib_get_mad_params ( ibdev ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not update MAD parameters: %s\n",
		       ibdev, strerror ( rc ) );
		return;
	}

	/* Notify IPoIB of link state change */
	ipoib_link_state_changed ( ibdev );
}
