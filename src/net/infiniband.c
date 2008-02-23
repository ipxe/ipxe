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
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/list.h>
#include <gpxe/if_arp.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
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
	       "with CQN %#lx\n", ibdev, num_cqes, cq, cq->dev_priv, cq->cqn );
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
	       ibdev, qp, qp->dev_priv, qp->qpn );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d send entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_send_wqes, qp->send.iobufs,
	       qp->recv.iobufs );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d receive entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_send_wqes, qp->recv.iobufs,
	       ( ( ( void * ) qp ) + total_size ) );
	return qp;
}

/**
 * Destroy queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 */
void ib_destroy_qp ( struct ib_device *ibdev,
		     struct ib_queue_pair *qp ) {
	DBGC ( ibdev, "IBDEV %p destroying queue pair %#lx\n",
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

/**
 * Allocate Infiniband device
 *
 * @v priv_size		Size of private data area
 * @ret ibdev		Infiniband device, or NULL
 */
struct ib_device * alloc_ibdev ( size_t priv_size ) {
	struct ib_device *ibdev;
	size_t total_len;

	total_len = ( sizeof ( *ibdev ) + priv_size );
	ibdev = zalloc ( total_len );
	if ( ibdev ) {
		ibdev->dev_priv = ( ( ( void * ) ibdev ) + sizeof ( *ibdev ) );
	}
	return ibdev;
}

/**
 * Free Infiniband device
 *
 * @v ibdev		Infiniband device
 */
void free_ibdev ( struct ib_device *ibdev ) {
	free ( ibdev );
}
